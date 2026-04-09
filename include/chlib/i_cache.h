/**
 * @file i_cache.h
 * @brief 指令缓存 (I-Cache) 简化实现
 * 
 * 规格:
 * - 容量：4KB
 * - 组相联：2 路
 * - 行大小：16 字节
 * - 替换策略：LRU
 * 
 * 作者：DevMate
 * 日期：2026-04-09
 */
#pragma once

#include "ch.hpp"
#include "component.h"

using namespace ch::core;

namespace chlib {

/**
 * @brief I-Cache 配置参数
 */
struct ICacheConfig {
    static constexpr unsigned CAPACITY = 4096;      // 4KB
    static constexpr unsigned ASSOCIATIVITY = 2;    // 2 路组相联
    static constexpr unsigned LINE_SIZE = 16;       // 16 字节行大小
    static constexpr unsigned NUM_SETS = CAPACITY / (ASSOCIATIVITY * LINE_SIZE); // 128 sets
};

/**
 * @brief I-Cache 控制器 (简化版)
 * 
 * 当前实现：编译验证版本，功能待完善
 */
class ICache : public ch::Component {
public:
    __io(
        // 请求接口
        ch_in<ch_uint<32>> addr;          // 请求地址
        ch_in<ch_bool> req;               // 请求有效
        ch_out<ch_uint<32>> data;         // 读出数据
        ch_out<ch_bool> ready;            // 就绪信号
        ch_out<ch_bool> miss;             // 未命中
        
        // AXI 接口 (简化)
        ch_out<ch_uint<32>> axi_addr;     // AXI 读地址
        ch_in<ch_bool> axi_ready;         // AXI 就绪
        ch_in<ch_uint<32>> axi_data;      // AXI 读数据
    )
    
    ICache(ch::Component* parent = nullptr, const std::string& name = "icache")
        : ch::Component(parent, name) {}
    
    void create_ports() override {
        new (io_storage_) io_type;
    }
    
    void describe() override {
        // 简化实现：直连模式
        io().ready = io().req;
        io().miss = ch_bool(false);
        
        // 数据直接传递（实际应该从 Cache 读取）
        io().data <<= io().axi_data;
        io().axi_addr <<= io().addr;
    }
};

} // namespace chlib
