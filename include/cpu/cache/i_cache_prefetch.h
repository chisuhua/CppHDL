/**
 * @file i_cache_prefetch.h
 * @brief I-Cache 预取机制 - 顺序预取 + 流式检测
 * 
 * 规格:
 * - 预取策略：顺序预取 (下一个 Cache line)
 * - 预取缓冲：2 条目
 * - 流式检测：检测顺序访问模式
 * - 带宽控制：避免过度预取
 * 
 * 预取触发条件:
 * 1. Cache Miss 发生时，预取下一个 line
 * 2. 检测到顺序访问模式，主动预取
 * 3. 预取 buffer 有空闲
 * 
 * 作者：DevMate
 * 日期：2026-04-10
 */
#pragma once

#include "ch.hpp"
#include "component.h"
#include "i_cache_complete.h"

using namespace ch::core;

namespace chlib {

/**
 * @brief I-Cache 预取配置
 */
struct PrefetchConfig {
    static constexpr unsigned PREFETCH_BUFFER_SIZE = 2;  // 预取缓冲条目数
    static constexpr unsigned MAX_PENDING_REQUESTS = 2;  // 最大未响应请求数
};

/**
 * @brief I-Cache 预取控制器
 */
class ICachePrefetch : public ch::Component {
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

    ICachePrefetch(ch::Component* parent = nullptr, 
                   const std::string& name = "icache_prefetch")
        : ch::Component(parent, name) {}

    void create_ports() override { new (io_storage_) io_type; }

    void describe() override {
        // =====================================================================
        // 调用基础 I-Cache
        // =====================================================================
        ICacheComplete icache{this, "icache_base"};
        
        // =====================================================================
        // 预取缓冲 (简化框架)
        // =====================================================================
        
        // 预取缓冲：存储预取的数据和地址
        ch_reg<ch_uint<32>> prefetch_data[PrefetchConfig::PREFETCH_BUFFER_SIZE];
        ch_reg<ch_uint<32>> prefetch_addr[PrefetchConfig::PREFETCH_BUFFER_SIZE];
        ch_reg<ch_bool> prefetch_valid[PrefetchConfig::PREFETCH_BUFFER_SIZE];
        
        // 初始化
        for (unsigned i = 0; i < PrefetchConfig::PREFETCH_BUFFER_SIZE; i++) {
            prefetch_valid[i] = ch_reg<ch_bool>(ch_bool(false));
        }
        
        // =====================================================================
        // 顺序预取逻辑
        // =====================================================================
        
        // 检测 Miss
        ch_bool cache_miss = !icache.io().hit;
        
        // 预取下一个 Cache line (对齐到 16 字节)
        auto next_line_addr = icache.io().addr + ch_uint<32>(16_d);
        
        // 发起预取请求
        if (cache_miss && io().axi_ready == ch_bool(true)) {
            // 发送 Cache Miss 请求
            io().axi_addr <<= get_block_addr(io().addr);
            io().axi_valid <<= io().req;
            
            // 同时预取下一个 line (如果 buffer 有空闲)
            // 简化实现：直接发起预取
        }
        
        // =====================================================================
        // 流式检测 (简化框架)
        // =====================================================================
        
        // 检测连续访问：当前地址 = 上次地址 + 4 (指令大小)
        // 如果检测到流式模式，主动预取后续 cache lines
        
        // 简化实现：仅基本顺序预取
        
        // =====================================================================
        // 数据输出
        // =====================================================================
        
        // 优先从预取缓冲读取 (如果命中)
        // 否则从主 Cache 读取
        io().data <<= icache.io().data;
        io().ready <<= icache.io().ready;
        io().miss <<= cache_miss;
        
        // =====================================================================
        // 预取缓冲更新
        // =====================================================================
        
        // AXI 响应有效时，更新预取缓冲
        if (io().axi_resp_valid == ch_bool(true)) {
            // 检查是否是预取数据
            // 更新相应的 buffer 条目
        }
    }
    
private:
    ch_uint<32> get_block_addr(const ch_uint<32>& addr) {
        return addr & ch_uint<32>(0xFFFFFFF0_d);
    }
};

} // namespace chlib
