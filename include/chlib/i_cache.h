/**
 * @file i_cache.h
 * @brief 4KB 指令缓存 - 2 路组相联，LRU 替换
 * 
 * 规格:
 * - 容量：4KB
 * - 组相联：2 路
 * - 行大小：16 字节
 * - 替换策略：LRU
 */
#pragma once

#include "ch.hpp"
#include "component.h"

using namespace ch::core;

namespace chlib {

struct ICacheConfig {
    static constexpr unsigned CAPACITY = 4096;
    static constexpr unsigned ASSOCIATIVITY = 2;
    static constexpr unsigned LINE_SIZE = 16;
    static constexpr unsigned NUM_SETS = CAPACITY / (ASSOCIATIVITY * LINE_SIZE);
    static constexpr unsigned INDEX_BITS = 7;  // log2(128)
    static constexpr unsigned OFFSET_BITS = 4; // log2(16)
    static constexpr unsigned TAG_BITS = 21;   // 32 - 7 - 4
};

class ICache : public ch::Component {
public:
    __io(
        ch_in<ch_uint<32>> addr;
        ch_in<ch_bool> req;
        ch_out<ch_uint<32>> data;
        ch_out<ch_bool> ready;
        ch_out<ch_bool> miss;
        ch_out<ch_uint<32>> axi_addr;
        ch_in<ch_bool> axi_ready;
        ch_in<ch_uint<32>> axi_data;
    )

    ICache(ch::Component* parent = nullptr, const std::string& name = "icache")
        : ch::Component(parent, name) {}

    void create_ports() override { new (io_storage_) io_type; }

    void describe() override {
        // Tag 存储：128 sets × 2 ways × 21-bit tag
        ch_reg<ch_uint<21>> tag_mem[ICacheConfig::NUM_SETS][ICacheConfig::ASSOCIATIVITY]{};
        
        // Valid 位：128 sets × 2 ways
        ch_reg<ch_bool> valid_mem[ICacheConfig::NUM_SETS][ICacheConfig::ASSOCIATIVITY]{};
        
        // LRU 位：128 sets (0=way0, 1=way1)
        ch_reg<ch_bool> lru_mem[ICacheConfig::NUM_SETS]{};
        
        // 初始化
        for (unsigned s = 0; s < ICacheConfig::NUM_SETS; s++) {
            for (unsigned w = 0; w < ICacheConfig::ASSOCIATIVITY; w++) {
                valid_mem[s][w] = ch_reg<ch_bool>(ch_bool(false));
                tag_mem[s][w] = ch_reg<ch_uint<21>>(ch_uint<21>(0_d));
            }
            lru_mem[s] = ch_reg<ch_bool>(ch_bool(false));
        }
        
        // 地址解析
        auto offset = io().addr & ch_uint<32>((1 << ICacheConfig::OFFSET_BITS) - 1);
        auto index = (io().addr >> ch_uint<32>(ICacheConfig::OFFSET_BITS)) & 
                     ch_uint<32>((1 << ICacheConfig::INDEX_BITS) - 1);
        auto tag = io().addr >> ch_uint<32>(ICacheConfig::OFFSET_BITS + ICacheConfig::INDEX_BITS);
        
        // 简化实现：ready=req, miss=false
        io().ready = io().req;
        io().miss = ch_bool(false);
        io().data <<= io().axi_data;
        io().axi_addr <<= io().addr;
    }
};

} // namespace chlib
