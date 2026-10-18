/**
 * @file i_cache_async.h
 * @brief 4KB 指令缓存 (0-cycle aread) — 软核 ITCM-style 实现
 *
 * 规格:
 * - 容量: 4KB (4096 bytes)
 * - 组相联: 2-way set associative
 * - 行大小: 16 bytes
 * - Sets: 128 (4KB / 2 / 16B)
 * - 替换策略: LRU (简化: fill 优先选 invalid way, 满后选 way0)
 * - 读延迟: **0 cycle (aread, combinational)** — TCM 风格, 低频设计
 *
 * 地址格式 (32-bit):
 * [31:11] Tag (21 bits)
 * [10:4]  Index (7 bits) - 128 sets
 * [3:0]   Offset (4 bits) - 16 bytes per line
 *
 * 实现: 基于 ch_mem 原语 (commit bfb3960 ADR-039 §2 决策)
 * - SRAM: ch_mem<T, NUM_SETS> per way
 * - hit-path 用 aread (0-cycle combinational) — TCM 风格, addr 输入当拍可拿 data
 * - miss-fill 用 ch_mem::write (1-cycle registered)
 * - 警告: RV32I imem 集成应该用 ICacheSync (1-cycle), 0-cycle aread 只适用于 TCM 风格
 *
 * 与 i_cache_sync.h 的区别: hit latency 0 cycle (本类) vs 1 cycle (sync)
 *
 * 作者: build-agent (cache-complete-rewrite Session B, ADR-041)
 * 日期: 2026-10-19
 */
#pragma once

#include "ch.hpp"
#include "component.h"
#include "chlib/stream.h"

namespace chlib {

using namespace ch::core;

/**
 * @brief I-Cache 异步版 (0-cycle aread) — TCM-style
 *
 * hit 延迟: 0 cycle (aread 组合读)
 * miss-fill: cycle N 触发 → cycle N+1 数据回填 (ch_mem::write 1-cycle)
 *
 * 注: 0-cycle aread + RV32I 5-stage pipeline 在 imem 集成时需要 IF/ID
 *   pipeline reg 吸收 1 cycle pc_lag. 商用 L1 cache 应该用 ICacheSync (1-cycle).
 */
class ICacheAsync : public ch::Component {
public:
    __io(
        ch_in<ch_uint<32>> addr;
        ch_in<ch_bool> req;

        ch_out<ch_uint<32>> data;
        ch_out<ch_bool> ready;
        ch_out<ch_bool> miss;

        ch_out<ch_uint<32>> axi_addr;
        ch_out<ch_bool> axi_valid;
        ch_in<ch_bool> axi_ready;
        ch_in<ch_uint<32>> axi_data;
        ch_in<ch_bool> axi_resp_valid;
    )

    ICacheAsync(ch::Component* parent = nullptr,
                const std::string& name = "icache_async")
        : ch::Component(parent, name) {}

    void create_ports() override { new (io_storage_) io_type; }

    void describe() override {
        ch_mem<ch_uint<ICacheConfig::DATA_WIDTH>, ICacheConfig::NUM_SETS> data_mem_way0;
        ch_mem<ch_uint<ICacheConfig::DATA_WIDTH>, ICacheConfig::NUM_SETS> data_mem_way1;

        ch_mem<ch_uint<ICacheConfig::TAG_BITS>, ICacheConfig::NUM_SETS> tag_mem_way0;
        ch_mem<ch_uint<ICacheConfig::TAG_BITS>, ICacheConfig::NUM_SETS> tag_mem_way1;

        ch_mem<ch_uint<1>, ICacheConfig::NUM_SETS> valid_mem_way0;
        ch_mem<ch_uint<1>, ICacheConfig::NUM_SETS> valid_mem_way1;

        ch_mem<ch_bool, ICacheConfig::NUM_SETS> lru_mem;

        auto offset = bits<ICacheConfig::OFFSET_BITS - 1, 0>(io().addr);
        auto index = bits<ICacheConfig::OFFSET_BITS + ICacheConfig::INDEX_BITS - 1,
                           ICacheConfig::OFFSET_BITS>(io().addr);
        auto tag = bits<31, ICacheConfig::OFFSET_BITS + ICacheConfig::INDEX_BITS>(io().addr);

        // =====================================================================
        // Hit 检测 (aread, 0-cycle combinational)
        // =====================================================================

        ch_uint<ICacheConfig::TAG_BITS> tag_way0(tag_mem_way0.aread(index).impl());
        ch_uint<ICacheConfig::TAG_BITS> tag_way1(tag_mem_way1.aread(index).impl());
        ch_uint<1> valid_way0_raw(valid_mem_way0.aread(index).impl());
        ch_uint<1> valid_way1_raw(valid_mem_way1.aread(index).impl());
        ch_bool valid_way0 = valid_way0_raw != ch_uint<1>(0_d);
        ch_bool valid_way1 = valid_way1_raw != ch_uint<1>(0_d);

        ch_bool hit_way0 = valid_way0 & (tag_way0 == tag);
        ch_bool hit_way1 = valid_way1 & (tag_way1 == tag);
        ch_bool hit = hit_way0 | hit_way1;
        ch_bool miss = !hit;

        // =====================================================================
        // 数据输出 (aread 0-cycle)
        // =====================================================================

        ch_uint<ICacheConfig::DATA_WIDTH> data_way0(data_mem_way0.aread(index).impl());
        ch_uint<ICacheConfig::DATA_WIDTH> data_way1(data_mem_way1.aread(index).impl());

        auto cache_data = select(hit_way1, data_way1, data_way0);

        io().data <<= select(hit, bits<31, 0>(cache_data), io().axi_data);

        // =====================================================================
        // Ready / Miss 信号
        // =====================================================================

        io().ready <<= select(hit, ch_bool(true), io().axi_ready);
        io().miss <<= miss;

        // =====================================================================
        // AXI 请求生成
        // =====================================================================

        io().axi_valid <<= select(miss, ch_bool(true), ch_bool(false)) & io().req;
        io().axi_addr <<= icache_get_block_addr(io().addr);

        // =====================================================================
        // Miss 处理: 填充 Cache (enable 信号驱动)
        // =====================================================================
        ch_bool do_fill = io().axi_resp_valid & miss;
        ch_bool fill_way0_en = do_fill & !valid_way0;
        ch_bool fill_way1_en = do_fill & valid_way0 & !valid_way1;

        tag_mem_way0.write(index, tag, fill_way0_en);
        data_mem_way0.write(index, zext<ICacheConfig::DATA_WIDTH>(io().axi_data), fill_way0_en);
        valid_mem_way0.write(index, ch_uint<1>(1_d), fill_way0_en);

        tag_mem_way1.write(index, tag, fill_way1_en);
        data_mem_way1.write(index, zext<ICacheConfig::DATA_WIDTH>(io().axi_data), fill_way1_en);
        valid_mem_way1.write(index, ch_uint<1>(1_d), fill_way1_en);

        // =====================================================================
        // Hit 时更新 LRU
        // =====================================================================
        ch_bool do_lru_update = hit;
        ch_bool new_lru_val = select(hit_way1, ch_bool(false), ch_bool(true));
        lru_mem.write(index, new_lru_val, do_lru_update);
    }

private:
    ch_uint<32> icache_get_block_addr(const ch_uint<32>& addr) {
        return addr & ch_uint<32>(0xFFFFFFF0_h);
    }
};

} // namespace chlib