/**
 * @file d_cache_complete.h
 * @brief 4KB 数据缓存 - 完整实现带 Hit/Miss 检测、LRU 和 AXI 握手
 * 
 * 规格:
 * - 容量：4KB (4096 bytes)
 * - 组相联：2-way set associative
 * - 行大小：16 bytes
 * - Sets: 128 (4KB / 2 / 16B)
 * - 写策略：Write-Through
 * 
 * 地址格式 (32-bit):
 * [31:11] Tag (21 bits)
 * [10:4]  Index (7 bits) - 128 sets
 * [3:0]   Offset (4 bits) - 16 bytes per line
 * 
 * 功能:
 * - Hit/Miss 检测
 * - LRU 替换
 * - Write-Through 策略
 * - AXI4 握手协议
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
 * @brief D-Cache 控制器 (完整实现)
 */
class DCacheComplete : public ch::Component {
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

    DCacheComplete(ch::Component* parent = nullptr, 
                   const std::string& name = "dcache_complete")
        : ch::Component(parent, name) {}

    void create_ports() override { new (io_storage_) io_type; }

    void describe() override {
        // =====================================================================
        // SRAM 阵列
        // =====================================================================
        
        // Data RAM: 128 sets × 2 ways × 128-bit
        ch_reg<ch_uint<ICacheConfig::DATA_WIDTH>> 
            data_sram[ICacheConfig::NUM_SETS][ICacheConfig::ASSOCIATIVITY];
        
        // Tag RAM: 128 sets × 2 ways × 21-bit
        ch_reg<ch_uint<ICacheConfig::TAG_BITS>> 
            tag_sram[ICacheConfig::NUM_SETS][ICacheConfig::ASSOCIATIVITY];
        
        // Valid 位：128 sets × 2 ways
        ch_reg<ch_bool> 
            valid_sram[ICacheConfig::NUM_SETS][ICacheConfig::ASSOCIATIVITY];
        
        // LRU 位：128 sets × 1-bit
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
                data_sram[s][w] = ch_reg<ch_uint<ICacheConfig::DATA_WIDTH>>(
                    ch_uint<ICacheConfig::DATA_WIDTH>(0_d));
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
        // Hit/Miss 检测
        // =====================================================================
        
        auto tag_way0 = tag_sram[index][0];
        auto tag_way1 = tag_sram[index][1];
        auto valid_way0 = valid_sram[index][0];
        auto valid_way1 = valid_sram[index][1];
        
        ch_bool hit_way0 = valid_way0 && (tag_way0 == tag);
        ch_bool hit_way1 = valid_way1 && (tag_way1 == tag);
        ch_bool hit = hit_way0 || hit_way1;
        ch_bool miss = !hit;
        
        io().miss <<= miss;
        
        // =====================================================================
        // 读操作：Hit 时从 Cache 返回
        // =====================================================================
        auto data_way0 = data_sram[index][0];
        auto data_way1 = data_sram[index][1];
        auto cache_data = select(hit_way1, data_way1, data_way0);
        
        // Hit: Cache 返回；Miss: AXI 返回
        io().rdata <<= select(hit, cache_data(63_d, 32_d), io().axi_rdata);
        
        // =====================================================================
        // Write-Through 写操作
        // =====================================================================
        
        // 写操作：同时更新 Cache（如果 Hit）和 AXI 总线
        io().axi_we <<= io().we;
        io().axi_wdata <<= io().wdata;
        io().axi_addr <<= dcache_get_block_addr(io().addr);
        
        // Hit 且写操作时，更新 Cache Data（Write-Through）
        // 实际应用中可能需要字节掩码，这里简化为全写
        if (hit && io().we == ch_bool(true)) {
            // 简化：假设写入整个 Data 块
            if (hit_way0) {
                data_sram[index][0] = ch_reg<ch_uint<ICacheConfig::DATA_WIDTH>>(
                    ch_uint<ICacheConfig::DATA_WIDTH>(io().wdata));
            } else {
                data_sram[index][1] = ch_reg<ch_uint<ICacheConfig::DATA_WIDTH>>(
                    ch_uint<ICacheConfig::DATA_WIDTH>(io().wdata));
            }
        }
        
        // =====================================================================
        // Ready 信号
        // =====================================================================
        
        // Read Hit: 立即 ready
        // Read Miss: 等待 AXI ready
        // Write: AXI ready 后 ready
        ch_bool is_read = !io().we;
        io().ready <<= select(is_read, 
                              select(hit, ch_bool(true), io().axi_ready),  // Read
                              io().axi_ready);                               // Write
        
        // =====================================================================
        // AXI 请求
        // =====================================================================
        
        // Miss 读或任意写都发起 AXI 请求
        io().axi_valid <<= (miss && is_read) || io().we;
        
        // =====================================================================
        // LRU 更新和 Miss 填充
        // =====================================================================
        
        if (miss && io().axi_resp_valid == ch_bool(true) && is_read) {
            // Miss 填充
            auto victim_way = lru_sram[index];
            tag_sram[index][victim_way] = ch_reg<ch_uint<ICacheConfig::TAG_BITS>>(tag);
            data_sram[index][victim_way] = ch_reg<ch_uint<ICacheConfig::DATA_WIDTH>>(
                ch_uint<ICacheConfig::DATA_WIDTH>(io().axi_rdata));
            valid_sram[index][victim_way] = ch_reg<ch_bool>(ch_bool(true));
            // LRU 设为 MRU
            lru_sram[index] = !victim_way;
        }
        
        // Hit 时更新 LRU
        if (hit) {
            lru_sram[index] = select(hit_way1, ch_bool(true), ch_bool(false));
        }
    }
    
private:
    ch_uint<32> dcache_get_block_addr(const ch_uint<32>& addr) {
        return addr & ch_uint<32>(0xFFFFFFF0_d);
    }
};

} // namespace chlib
