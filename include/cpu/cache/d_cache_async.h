/**
 * @file d_cache_async.h
 * @brief 4KB 数据缓存 (0-cycle aread) — 软核 DTCM-style 实现
 *
 * 规格:
 * - 容量: 4KB (4096 bytes)
 * - 组相联: 2-way set associative
 * - 行大小: 16 bytes
 * - Sets: 128 (4KB / 2 / 16B)
 * - 写策略: Write-Through
 * - 读延迟: **0 cycle (aread, combinational)** — TCM 风格, 低频设计
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
 * @brief D-Cache 异步版 (0-cycle aread) — DTCM-style
 */
class DCacheAsync : public ch::Component {
public:
    __io(
        ch_in<ch_uint<32>> addr;
        ch_in<ch_uint<32>> wdata;
        ch_in<ch_bool> req;
        ch_in<ch_bool> we;

        ch_out<ch_uint<32>> rdata;
        ch_out<ch_bool> ready;
        ch_out<ch_bool> miss;

        ch_out<ch_uint<32>> axi_addr;
        ch_out<ch_uint<32>> axi_wdata;
        ch_out<ch_bool> axi_we;
        ch_out<ch_bool> axi_valid;
        ch_in<ch_bool> axi_ready;
        ch_in<ch_uint<32>> axi_rdata;
        ch_in<ch_bool> axi_resp_valid;
    )

    DCacheAsync(ch::Component* parent = nullptr,
                const std::string& name = "dcache_async")
        : ch::Component(parent, name) {}

    void create_ports() override { new (io_storage_) io_type; }

    void describe() override {
        ch_mem<ch_uint<DCacheConfig::DATA_WIDTH>, DCacheConfig::NUM_SETS> data_mem_way0;
        ch_mem<ch_uint<DCacheConfig::DATA_WIDTH>, DCacheConfig::NUM_SETS> data_mem_way1;

        ch_mem<ch_uint<DCacheConfig::TAG_BITS>, DCacheConfig::NUM_SETS> tag_mem_way0;
        ch_mem<ch_uint<DCacheConfig::TAG_BITS>, DCacheConfig::NUM_SETS> tag_mem_way1;

        ch_mem<ch_uint<1>, DCacheConfig::NUM_SETS> valid_mem_way0;
        ch_mem<ch_uint<1>, DCacheConfig::NUM_SETS> valid_mem_way1;

        ch_mem<ch_bool, DCacheConfig::NUM_SETS> lru_mem;

        auto offset = bits<DCacheConfig::OFFSET_BITS - 1, 0>(io().addr);
        auto index = bits<DCacheConfig::OFFSET_BITS + DCacheConfig::INDEX_BITS - 1,
                           DCacheConfig::OFFSET_BITS>(io().addr);
        auto tag = bits<31, DCacheConfig::OFFSET_BITS + DCacheConfig::INDEX_BITS>(io().addr);

        // =====================================================================
        // Hit/Miss 检测 (aread, 0-cycle)
        // =====================================================================

        ch_uint<DCacheConfig::TAG_BITS> tag_way0(tag_mem_way0.aread(index).impl());
        ch_uint<DCacheConfig::TAG_BITS> tag_way1(tag_mem_way1.aread(index).impl());
        ch_uint<1> valid_way0_raw(valid_mem_way0.aread(index).impl());
        ch_uint<1> valid_way1_raw(valid_mem_way1.aread(index).impl());
        ch_bool valid_way0 = valid_way0_raw != ch_uint<1>(0_d);
        ch_bool valid_way1 = valid_way1_raw != ch_uint<1>(0_d);

        ch_bool hit_way0 = valid_way0 & (tag_way0 == tag);
        ch_bool hit_way1 = valid_way1 & (tag_way1 == tag);
        ch_bool hit = hit_way0 | hit_way1;
        ch_bool miss = !hit;

        io().miss <<= miss;

        // =====================================================================
        // 读操作 (aread 0-cycle)
        // =====================================================================
        ch_uint<DCacheConfig::DATA_WIDTH> data_way0(data_mem_way0.aread(index).impl());
        ch_uint<DCacheConfig::DATA_WIDTH> data_way1(data_mem_way1.aread(index).impl());
        auto cache_data = select(hit_way1, data_way1, data_way0);

        io().rdata <<= select(hit, bits<31, 0>(cache_data), io().axi_rdata);

        // =====================================================================
        // Write-Through 写操作
        // =====================================================================

        io().axi_we <<= io().we;
        io().axi_wdata <<= io().wdata;
        io().axi_addr <<= dcache_get_block_addr(io().addr);

        ch_bool do_write = hit & io().we;
        data_mem_way0.write(index, zext<DCacheConfig::DATA_WIDTH>(io().wdata), do_write & hit_way0);
        data_mem_way1.write(index, zext<DCacheConfig::DATA_WIDTH>(io().wdata), do_write & hit_way1);

        // =====================================================================
        // Ready 信号
        // =====================================================================

        ch_bool is_read = !io().we;
        io().ready <<= select(is_read,
                              select(hit, ch_bool(true), io().axi_ready),
                              io().axi_ready);

        // =====================================================================
        // AXI 请求
        // =====================================================================

        ch_bool miss_read = miss & is_read;
        io().axi_valid <<= select(miss_read, ch_bool(true), io().we);

        // =====================================================================
        // Miss 填充 (enable 信号)
        // =====================================================================
        ch_bool do_fill = io().axi_resp_valid & miss_read;
        ch_bool fill_way0_en = do_fill & !valid_way0;
        ch_bool fill_way1_en = do_fill & valid_way0 & !valid_way1;

        tag_mem_way0.write(index, tag, fill_way0_en);
        data_mem_way0.write(index, zext<DCacheConfig::DATA_WIDTH>(io().axi_rdata), fill_way0_en);
        valid_mem_way0.write(index, ch_uint<1>(1_d), fill_way0_en);

        tag_mem_way1.write(index, tag, fill_way1_en);
        data_mem_way1.write(index, zext<DCacheConfig::DATA_WIDTH>(io().axi_rdata), fill_way1_en);
        valid_mem_way1.write(index, ch_uint<1>(1_d), fill_way1_en);

        // =====================================================================
        // Hit 时更新 LRU
        // =====================================================================
        ch_bool do_lru_update = hit;
        ch_bool new_lru_val = select(hit_way1, ch_bool(false), ch_bool(true));
        lru_mem.write(index, new_lru_val, do_lru_update);
    }

private:
    ch_uint<32> dcache_get_block_addr(const ch_uint<32>& addr) {
        return addr & ch_uint<32>(0xFFFFFFF0_h);
    }
};

} // namespace chlib