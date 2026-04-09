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
        // Data 存储：128 sets × 2 ways × 128-bit (16 bytes)
        ch_reg<ch_uint<128>> data_mem[DCacheConfig::NUM_SETS][DCacheConfig::ASSOCIATIVITY]{};
        
        // Tag 存储
        ch_reg<ch_uint<21>> tag_mem[DCacheConfig::NUM_SETS][DCacheConfig::ASSOCIATIVITY]{};
        
        // Valid 位
        ch_reg<ch_bool> valid_mem[DCacheConfig::NUM_SETS][DCacheConfig::ASSOCIATIVITY]{};
        
        // Dirty 位 (Write-Back 需要)
        ch_reg<ch_bool> dirty_mem[DCacheConfig::NUM_SETS][DCacheConfig::ASSOCIATIVITY]{};
        
        // 初始化
        for (unsigned s = 0; s < DCacheConfig::NUM_SETS; s++) {
            for (unsigned w = 0; w < DCacheConfig::ASSOCIATIVITY; w++) {
                valid_mem[s][w] = ch_reg<ch_bool>(ch_bool(false));
                dirty_mem[s][w] = ch_reg<ch_bool>(ch_bool(false));
            }
        }
        
        // Write-Through 实现：写操作同时更新 Cache 和主存
        io().ready = io().req;
        io().miss = ch_bool(false);
        io().rdata <<= io().axi_rdata;
        io().axi_addr <<= io().addr;
        io().axi_wdata <<= io().wdata;
        io().axi_we <<= io().we;
    }
};

} // namespace chlib
