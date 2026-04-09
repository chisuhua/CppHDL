/**
 * @file d_cache.h
 * @brief 数据缓存 (D-Cache) 简化实现
 * 
 * 规格:
 * - 容量：4KB
 * - 组相联：2 路
 * - 行大小：16 字节
 * - 写策略：Write-Through
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
 * @brief D-Cache 配置参数
 */
struct DCacheConfig {
    static constexpr unsigned CAPACITY = 4096;      // 4KB
    static constexpr unsigned ASSOCIATIVITY = 2;    // 2 路组相联
    static constexpr unsigned LINE_SIZE = 16;       // 16 字节行大小
    static constexpr unsigned NUM_SETS = CAPACITY / (ASSOCIATIVITY * LINE_SIZE); // 128 sets
};

/**
 * @brief D-Cache 控制器 (简化版)
 * 
 * 当前实现：编译验证版本，功能待完善
 */
class DCache : public ch::Component {
public:
    __io(
        // 请求接口
        ch_in<ch_uint<32>> addr;          // 请求地址
        ch_in<ch_uint<32>> wdata;         // 写入数据
        ch_in<ch_bool> req;               // 请求有效
        ch_in<ch_bool> we;                // 写使能
        ch_out<ch_uint<32>> rdata;        // 读出数据
        ch_out<ch_bool> ready;            // 就绪信号
        ch_out<ch_bool> miss;             // 未命中
        
        // AXI 接口 (简化)
        ch_out<ch_uint<32>> axi_addr;     // AXI 地址
        ch_out<ch_uint<32>> axi_wdata;    // AXI 写数据
        ch_out<ch_bool> axi_we;           // AXI 写使能
        ch_in<ch_bool> axi_ready;         // AXI 就绪
        ch_in<ch_uint<32>> axi_rdata;     // AXI 读数据
    )
    
    DCache(ch::Component* parent = nullptr, const std::string& name = "dcache")
        : ch::Component(parent, name) {}
    
    void create_ports() override {
        new (io_storage_) io_type;
    }
    
    void describe() override {
        // 简化实现：Write-Through 模式
        io().ready = io().req;
        io().miss = ch_bool(false);
        
        // 读数据直接传递
        io().rdata <<= io().axi_rdata;
        io().axi_addr <<= io().addr;
        io().axi_wdata <<= io().wdata;
        io().axi_we <<= io().we;
    }
};

} // namespace chlib
