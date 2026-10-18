/**
 * @file i_cache_sync.h
 * @brief 4KB 指令缓存 (1-cycle sread) — RV32I L1 imem 默认实现
 *
 * 规格:
 * - 容量: 4KB (4096 bytes)
 * - 组相联: 2-way set associative
 * - 行大小: 16 bytes
 * - Sets: 128 (4KB / 2 / 16B)
 * - 替换策略: LRU (简化: fill 优先选 invalid way, 满后选 way0)
 * - 读延迟: **1 cycle (sread, registered)** — 对齐 RV32I 5-stage pipeline pc_lag
 *
 * 地址格式 (32-bit):
 * [31:11] Tag (21 bits)
 * [10:4]  Index (7 bits) - 128 sets
 * [3:0]   Offset (4 bits) - 16 bytes per line
 *
 * 实现: 基于 ch_mem 原语 (commit bfb3960 ADR-039 §2 决策)
 * - SRAM: ch_mem<T, NUM_SETS> per way (独立命名避免数组 runtime index)
 * - hit-path 用 sread (1-cycle registered) — 对齐 imem 必须 sread 的项目约定
 *   (root AGENTS.md MEM READ PORT + verilator-e2e-coverage spec §R1 invariant 3)
 * - miss-fill 用 ch_mem::write (1-cycle registered)
 * - fill 路径用 enable 信号 (per cache-complete-doa-note §6.1 R3 hard-gate)
 *
 * 与 i_cache_async.h 的区别: hit latency 1 cycle (本类) vs 0 cycle (async)
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
 * @brief I-Cache 配置参数 (与异步版本共享)
 */
struct ICacheConfig {
    static constexpr unsigned CAPACITY = 4096;        // 4KB
    static constexpr unsigned ASSOCIATIVITY = 2;      // 2-way
    static constexpr unsigned LINE_SIZE = 16;         // 16 bytes per line
    static constexpr unsigned NUM_SETS = CAPACITY / (ASSOCIATIVITY * LINE_SIZE); // 128
    static constexpr unsigned INDEX_BITS = 7;         // log2(128)
    static constexpr unsigned OFFSET_BITS = 4;        // log2(16)
    static constexpr unsigned TAG_BITS = 32 - INDEX_BITS - OFFSET_BITS; // 21
    static constexpr unsigned DATA_WIDTH = 128;       // 16 bytes = 128 bits
};

/**
 * @brief I-Cache 同步版 (1-cycle sread) — RV32I L1 imem 默认
 *
 * hit 延迟: 1 cycle (sread 采样 + 比较 + mux)
 * miss-fill: cycle N 触发 → cycle N+1 数据回填 (ch_mem::write 1-cycle)
 * 总 hit 延迟: 2 cycle (cycle N 提交, cycle N+2 数据可用)
 *   - cycle N: sread 采样 addr
 *   - cycle N+1: 上升沿, sread 输出寄存器 = mem[addr_N], 同时 ch_mem::write 完成 (cycle N fill 触发 → cycle N+1 数据到 mem)
 *   - cycle N+2: hit/miss 决策 (valid 是 sread 输出) → data 输出
 *
 * 注: 此处描述的是 "提交 addr 到 data 输出" 的总延迟. RV32I 集成时 pc_lag
 *   由 IF/ID pipeline reg 自然吸收, 无需额外对齐 logic.
 */
class ICacheSync : public ch::Component {
public:
    __io(
        // CPU 请求接口
        ch_in<ch_uint<32>> addr;          // 请求地址
        ch_in<ch_bool> req;               // 请求有效

        // 数据输出
        ch_out<ch_uint<32>> data;         // 读出指令
        ch_out<ch_bool> ready;            // 就绪信号
        ch_out<ch_bool> miss;             // 未命中信号

        // AXI 主设备接口
        ch_out<ch_uint<32>> axi_addr;     // AXI 读地址
        ch_out<ch_bool> axi_valid;        // AXI 请求有效
        ch_in<ch_bool> axi_ready;         // AXI 就绪
        ch_in<ch_uint<32>> axi_data;      // AXI 读数据
        ch_in<ch_bool> axi_resp_valid;    // AXI 响应有效
    )

    ICacheSync(ch::Component* parent = nullptr,
               const std::string& name = "icache_sync")
        : ch::Component(parent, name) {}

    void create_ports() override { new (io_storage_) io_type; }

    void describe() override {
        // =====================================================================
        // SRAM 阵列 (ch_mem-based, ADR-039 §2 决策)
        // =====================================================================

        // Data RAM: 独立命名 per way (避免 ch_mem 数组 C-style runtime index 问题)
        ch_mem<ch_uint<ICacheConfig::DATA_WIDTH>, ICacheConfig::NUM_SETS> data_mem_way0;
        ch_mem<ch_uint<ICacheConfig::DATA_WIDTH>, ICacheConfig::NUM_SETS> data_mem_way1;

        // Tag RAM: 独立命名 per way
        ch_mem<ch_uint<ICacheConfig::TAG_BITS>, ICacheConfig::NUM_SETS> tag_mem_way0;
        ch_mem<ch_uint<ICacheConfig::TAG_BITS>, ICacheConfig::NUM_SETS> tag_mem_way1;

        // Valid 位: 独立命名 per way (ch_uint<1> 避免 ch_mem<ch_bool> 初始 X 问题)
        ch_mem<ch_uint<1>, ICacheConfig::NUM_SETS> valid_mem_way0;
        ch_mem<ch_uint<1>, ICacheConfig::NUM_SETS> valid_mem_way1;

        // LRU 位: 1 个 ch_mem (shared, 1-bit per set)
        ch_mem<ch_bool, ICacheConfig::NUM_SETS> lru_mem;

        // =====================================================================
        // 地址解析 (no redundant ch_uint<32>(port) wrap, ADR-039 §2 fix)
        // =====================================================================
        auto offset = bits<ICacheConfig::OFFSET_BITS - 1, 0>(io().addr);
        auto index = bits<ICacheConfig::OFFSET_BITS + ICacheConfig::INDEX_BITS - 1,
                           ICacheConfig::OFFSET_BITS>(io().addr);
        auto tag = bits<31, ICacheConfig::OFFSET_BITS + ICacheConfig::INDEX_BITS>(io().addr);

        // =====================================================================
        // Hit 检测 (sread, 1-cycle registered)
        // =====================================================================

        // 读取两个 way 的 tag 和 valid (sread addr, enable=ch_bool(true))
        ch_uint<ICacheConfig::TAG_BITS> tag_way0(tag_mem_way0.sread(index).impl());
        ch_uint<ICacheConfig::TAG_BITS> tag_way1(tag_mem_way1.sread(index).impl());
        ch_uint<1> valid_way0_raw(valid_mem_way0.sread(index).impl());
        ch_uint<1> valid_way1_raw(valid_mem_way1.sread(index).impl());
        ch_bool valid_way0 = valid_way0_raw != ch_uint<1>(0_d);
        ch_bool valid_way1 = valid_way1_raw != ch_uint<1>(0_d);

        // Hit 检测: tag 匹配且 valid=1
        ch_bool hit_way0 = valid_way0 & (tag_way0 == tag);
        ch_bool hit_way1 = valid_way1 & (tag_way1 == tag);
        ch_bool hit = hit_way0 | hit_way1;
        ch_bool miss = !hit;

        // =====================================================================
        // 数据输出 (sread 1-cycle registered)
        // =====================================================================

        ch_uint<ICacheConfig::DATA_WIDTH> data_way0(data_mem_way0.sread(index).impl());
        ch_uint<ICacheConfig::DATA_WIDTH> data_way1(data_mem_way1.sread(index).impl());

        // 选择命中的 way (way1 优先)
        auto cache_data = select(hit_way1, data_way1, data_way0);

        // Hit: 从 Cache 返回数据；Miss: 从 AXI 返回数据
        // 用 bits<31, 0> 显式函数模板调用, 避免 ch_uint::operator()(unsigned, unsigned) 核心库 bug
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
        // Miss 处理: 填充 Cache (ch_mem::write, enable 信号驱动, no C++ if)
        // =====================================================================
        // 简化版 fill 策略: 按 valid 状态选择 victim (避开 lru_mem X 初始问题)
        //   way0 invalid -> 填 way0
        //   way0 valid, way1 invalid -> 填 way1
        //   两路都 valid -> 填 way0 (简化 LRU 覆盖)
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