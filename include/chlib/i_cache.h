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
    static constexpr unsigned INDEX_BITS = 7;
    static constexpr unsigned OFFSET_BITS = 4;
    static constexpr unsigned TAG_BITS = 21;
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
        // Tag SRAM: 128 sets × 2 ways × 21-bit
        // Valid SRAM: 128 sets × 2 ways × 1-bit
        // LRU: 128 sets × 1-bit
        
        // 简化实现：Ready = Req, Miss = 0
        io().ready = io().req;
        io().miss = ch_bool(false);
        io().data <<= io().axi_data;
        io().axi_addr <<= io().addr;
        
        // TODO: 完整实现需要:
        // 1. Tag 比较逻辑
        // 2. Hit/Miss 判断
        // 3. LRU 替换控制
        // 4. AXI 总线接口
    }
};

} // namespace chlib
