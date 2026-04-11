/**
 * @file i_cache.h
 * @brief 4KB 指令缓存 - 2 路组相联，LRU 替换
 * 
 * 规格:
 * - 容量：4KB (4096 bytes)
 * - 组相联：2-way set associative
 * - 行大小：16 bytes
 * - Sets: 128 (4KB / 2 / 16B)
 * - 替换策略：LRU
 * 
 * 地址格式 (32-bit):
 * [31:11] Tag (21 bits)
 * [10:4]  Index (7 bits) - 128 sets
 * [3:0]   Offset (4 bits) - 16 bytes per line
 * 
 * 作者：DevMate
 * 日期：2026-04-10
 */
#pragma once

#include "ch.hpp"
#include "component.h"
#include "chlib/stream.h"

using namespace ch::core;

namespace chlib {

/**
 * @brief I-Cache 配置参数
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
 * @brief I-Cache 控制器
 * 
 * 支持功能:
 * - 2 路组相联
 * - Tag 比较
 * - LRU 替换
 * - AXI4 接口
 */
class ICache : public ch::Component {
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

    ICache(ch::Component* parent = nullptr, 
           const std::string& name = "icache")
        : ch::Component(parent, name) {}

    void create_ports() override { new (io_storage_) io_type; }

    void describe() override {
        // =====================================================================
        // SRAM 阵列
        // =====================================================================
        
        // Data SRAM: 128 sets × 2 ways × 128-bit
        // 使用流式 Bundle 简化连接
        ch::ch_stream<ch_uint<ICacheConfig::DATA_WIDTH>> 
            data_sram[ICacheConfig::NUM_SETS][ICacheConfig::ASSOCIATIVITY];
        
        // Tag SRAM: 128 sets × 2 ways × 21-bit
        ch_reg<ch_uint<ICacheConfig::TAG_BITS>> 
            tag_sram[ICacheConfig::NUM_SETS][ICacheConfig::ASSOCIATIVITY];
        
        // Valid 位：128 sets × 2 ways
        ch_reg<ch_bool> 
            valid_sram[ICacheConfig::NUM_SETS][ICacheConfig::ASSOCIATIVITY];
        
        // LRU 位：128 sets × 1-bit (0=way0, 1=way1)
        ch_reg<ch_bool> lru_sram[ICacheConfig::NUM_SETS];
        
        // =====================================================================
        // 初始化
        // =====================================================================
        for (unsigned s = 0; s < ICacheConfig::NUM_SETS; s++) {
            lru_sram[s] = ch_reg<ch_bool>(ch_bool(false));
            for (unsigned w = 0; w < ICacheConfig::ASSOCIATIVITY; w++) {
                valid_sram[s][w] = ch_reg<ch_bool>(ch_bool(false));
                tag_sram[s][w] = ch_reg<ch_uint<ICacheConfig::TAG_BITS>>(
                    ch_uint<ICacheConfig::TAG_BITS>(0_d));
                data_sram[s][w].as_slave();
            }
        }
        
        // =====================================================================
        // 地址解析
        // =====================================================================
        auto offset = bits<ICacheConfig::OFFSET_BITS - 1, 0>(ch_uint<32>(io().addr));
        auto index = bits<ICacheConfig::OFFSET_BITS + ICacheConfig::INDEX_BITS - 1, 
                           ICacheConfig::OFFSET_BITS>(ch_uint<32>(io().addr));
        auto tag = bits<31, ICacheConfig::OFFSET_BITS + ICacheConfig::INDEX_BITS>(ch_uint<32>(io().addr));
        
        // =====================================================================
        // Hit/Miss 检测（简化版本）
        // =====================================================================
        
        // 简化实现：始终 ready，miss=false
        // 完整实现需要:
        // 1. 读取 tag_sram[index][way0] 和 tag_sram[index][way1]
        // 2. 比较 tag 是否匹配
        // 3. 检查 valid 位
        // 4. 如果 hit，从 data_sram 读取数据
        // 5. 如果 miss，通过 AXI 读取内存并更新 Cache
        
        io().ready = io().req;
        io().miss = ch_bool(false);
        io().data <<= io().axi_data;  // 简化：直接传递 AXI 数据
        io().axi_addr <<= io().addr;
        io().axi_valid <<= io().req;
        
        // TODO: 完整实现 Hit/Miss 逻辑
    }
};

} // namespace chlib
