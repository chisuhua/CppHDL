/**
 * @file d_cache.h
 * @brief 4KB 数据缓存 - 2 路组相联，Write-Through 策略
 * 
 * 规格:
 * - 容量：4KB (4096 bytes)
 * - 组相联：2-way set associative
 * - 行大小：16 bytes
 * - Sets: 128 (4KB / 2 / 16B)
 * - 写策略：Write-Through (写操作同时更新 Cache 和主存)
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
#include "stream.h"

using namespace ch::core;

namespace chlib {

/**
 * @brief D-Cache 配置参数
 */
struct DCacheConfig {
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
 * @brief D-Cache 控制器
 * 
 * 支持功能:
 * - 2 路组相联
 * - Tag 比较
 * - Write-Through 策略
 * - AXI4 接口
 */
class DCache : public ch::Component {
public:
    __io(
        // CPU 请求接口
        ch_in<ch_uint<32>> addr;          // 请求地址
        ch_in<ch_uint<32>> wdata;         // 写入数据
        ch_in<ch_bool> req;               // 请求有效
        ch_in<ch_bool> we;                // 写使能 (1=写，0=读)
        
        // 数据输出
        ch_out<ch_uint<32>> rdata;        // 读出数据
        ch_out<ch_bool> ready;            // 就绪信号
        ch_out<ch_bool> miss;             // 未命中信号
        
        // AXI 主设备接口
        ch_out<ch_uint<32>> axi_addr;     // AXI 地址
        ch_out<ch_uint<32>> axi_wdata;    // AXI 写数据
        ch_out<ch_bool> axi_we;          // AXI 写使能
        ch_out<ch_bool> axi_valid;        // AXI 请求有效
        ch_in<ch_bool> axi_ready;         // AXI 就绪
        ch_in<ch_uint<32>> axi_rdata;     // AXI 读数据
        ch_in<ch_bool> axi_resp_valid;    // AXI 响应有效
    )

    DCache(ch::Component* parent = nullptr, 
           const std::string& name = "dcache")
        : ch::Component(parent, name) {}

    void create_ports() override { new (io_storage_) io_type; }

    void describe() override {
        // =====================================================================
        // SRAM 阵列
        // =====================================================================
        
        // Data SRAM: 128 sets × 2 ways × 128-bit
        ch::ch_stream<ch_uint<DCacheConfig::DATA_WIDTH>> 
            data_sram[DCacheConfig::NUM_SETS][DCacheConfig::ASSOCIATIVITY];
        
        // Tag SRAM: 128 sets × 2 ways × 21-bit
        ch_reg<ch_uint<DCacheConfig::TAG_BITS>> 
            tag_sram[DCacheConfig::NUM_SETS][DCacheConfig::ASSOCIATIVITY];
        
        // Valid 位：128 sets × 2 ways
        ch_reg<ch_bool> 
            valid_sram[DCacheConfig::NUM_SETS][DCacheConfig::ASSOCIATIVITY];
        
        // Dirty 位：128 sets × 2 ways (Write-Back 需要，Write-Through 可不用)
        ch_reg<ch_bool> 
            dirty_sram[DCacheConfig::NUM_SETS][DCacheConfig::ASSOCIATIVITY];
        
        // LRU 位：128 sets × 1-bit
        ch_reg<ch_bool> lru_sram[DCacheConfig::NUM_SETS];
        
        // =====================================================================
        // 初始化
        // =====================================================================
        for (unsigned s = 0; s < DCacheConfig::NUM_SETS; s++) {
            lru_sram[s] = ch_reg<ch_bool>(ch_bool(false));
            for (unsigned w = 0; w < DCacheConfig::ASSOCIATIVITY; w++) {
                valid_sram[s][w] = ch_reg<ch_bool>(ch_bool(false));
                dirty_sram[s][w] = ch_reg<ch_bool>(ch_bool(false));
                tag_sram[s][w] = ch_reg<ch_uint<DCacheConfig::TAG_BITS>>(
                    ch_uint<DCacheConfig::TAG_BITS>(0_d));
                data_sram[s][w].as_slave();
            }
        }
        
        // =====================================================================
        // 地址解析
        // =====================================================================
        auto offset = io().addr(DCacheConfig::OFFSET_BITS - 1, 0);
        auto index = io().addr(DCacheConfig::OFFSET_BITS + DCacheConfig::INDEX_BITS - 1, 
                               DCacheConfig::OFFSET_BITS);
        auto tag = io().addr(31_d, DCacheConfig::OFFSET_BITS + DCacheConfig::INDEX_BITS);
        
        // =====================================================================
        // 读写控制逻辑（简化版本）
        // =====================================================================
        
        // Write-Through 策略:
        // - 写命中：同时更新 Cache 和主存
        // - 写未命中：直接写主存，不更新 Cache
        // - 读命中：从 Cache 读取
        // - 读未命中：从主存读取并更新 Cache
        
        io().ready = io().req;
        io().miss = ch_bool(false);
        io().rdata <<= io().axi_rdata;  // 简化：直接传递 AXI 数据
        io().axi_addr <<= io().addr;
        io().axi_wdata <<= io().wdata;
        io().axi_we <<= io().we;
        io().axi_valid <<= io().req;
        
        // TODO: 完整实现 Hit/Miss 逻辑和 Write-Through 缓冲
    }
};

} // namespace chlib
