/**
 * @file d_cache.h
 * @brief 4KB 数据缓存 - 2 路组相联，Write-Through 策略
 * 
 * 规格:
 * - 容量：4KB
 * - 组相联：2 路
 * - 行大小：16 字节
 * - 写策略：Write-Through
 */
#pragma once

#include "ch.hpp"
#include "component.h"

using namespace ch::core;

namespace chlib {

struct DCacheConfig {
    static constexpr unsigned CAPACITY = 4096;
    static constexpr unsigned ASSOCIATIVITY = 2;
    static constexpr unsigned LINE_SIZE = 16;
    static constexpr unsigned NUM_SETS = CAPACITY / (ASSOCIATIVITY * LINE_SIZE);
    static constexpr unsigned INDEX_BITS = 7;
    static constexpr unsigned OFFSET_BITS = 4;
};

class DCache : public ch::Component {
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
        ch_in<ch_bool> axi_ready;
        ch_in<ch_uint<32>> axi_rdata;
    )

    DCache(ch::Component* parent = nullptr, const std::string& name = "dcache")
        : ch::Component(parent, name) {}

    void create_ports() override { new (io_storage_) io_type; }

    void describe() override {
        // Data SRAM: 128 sets × 2 ways × 128-bit
        // Tag SRAM: 128 sets × 2 ways × 21-bit
        // Valid/Dirty: 128 sets × 2 ways
        
        // Write-Through: 写操作同时更新 Cache 和主存
        io().ready = io().req;
        io().miss = ch_bool(false);
        io().rdata <<= io().axi_rdata;
        io().axi_addr <<= io().addr;
        io().axi_wdata <<= io().wdata;
        io().axi_we <<= io().we;
        
        // TODO: 完整实现需要:
        // 1. Hit/Miss 检测
        // 2. Write Buffer
        // 3. 替换策略
        // 4. AXI 协议握手
    }
};

} // namespace chlib
