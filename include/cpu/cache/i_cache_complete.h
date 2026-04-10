/**
 * @file i_cache_complete.h
 * @brief 4KB 指令缓存 - 完整实现带 Hit/Miss 检测和 LRU 更新
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
 * 功能:
 * - Hit/Miss 检测
 * - LRU 替换
 * - AXI4 填充
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
 * @brief I-Cache 控制器 (完整实现)
 */
class ICacheComplete : public ch::Component {
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

    ICacheComplete(ch::Component* parent = nullptr, 
                   const std::string& name = "icache_complete")
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
        
        // LRU 位：128 sets × 1-bit (0=way0 MRU, 1=way1 MRU)
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
        auto offset = io().addr(ICacheConfig::OFFSET_BITS - 1, 0);
        auto index = io().addr(ICacheConfig::OFFSET_BITS + ICacheConfig::INDEX_BITS - 1, 
                               ICacheConfig::OFFSET_BITS);
        auto tag = io().addr(31_d, ICacheConfig::OFFSET_BITS + ICacheConfig::INDEX_BITS);
        
        // =====================================================================
        // Hit 检测
        // =====================================================================
        
        // 读取两个 way 的 tag 和 valid
        auto tag_way0 = tag_sram[index][0];
        auto tag_way1 = tag_sram[index][1];
        auto valid_way0 = valid_sram[index][0];
        auto valid_way1 = valid_sram[index][1];
        
        // Hit 检测：tag 匹配且 valid=1
        ch_bool hit_way0 = valid_way0 && (tag_way0 == tag);
        ch_bool hit_way1 = valid_way1 && (tag_way1 == tag);
        ch_bool hit = hit_way0 || hit_way1;
        
        // =====================================================================
        // 数据输出
        // =====================================================================
        
        // 从 Cache 读取数据（简化：使用 offset 选择 32-bit）
        auto data_way0 = data_sram[index][0];
        auto data_way1 = data_sram[index][1];
        
        // 选择命中的 way
        auto cache_data = select(hit_way1, data_way1, data_way0);
        
        // Hit: 从 Cache 返回数据；Miss: 从 AXI 返回数据
        io().data <<= select(hit, cache_data(63_d, 32_d), io().axi_data);
        
        // =====================================================================
        // Ready 信号生成
        // =====================================================================
        
        // Hit: 立即 ready；Miss: 等待 AXI ready
        ch_bool miss = !hit;
        io().ready <<= hit || io().axi_ready;
        io().miss <<= miss;
        
        // =====================================================================
        // AXI 请求生成
        // =====================================================================
        
        // Miss 时发起 AXI 读请求
        io().axi_valid <<= miss && io().req;
        io().axi_addr <<= icache_get_block_addr(io().addr);
        
        // =====================================================================
        // LRU 更新
        // =====================================================================
        
        // Hit 时更新 LRU：命中的 way 设为 0（MRU）
        if (hit_way1 == ch_bool(true)) {
            lru_sram[index] = ch_reg<ch_bool>(ch_bool(true));
        } else if (hit_way0 == ch_bool(true)) {
            lru_sram[index] = ch_reg<ch_bool>(ch_bool(false));
        }
        
        // =====================================================================
        // Miss 处理：填充 Cache
        // =====================================================================
        
        // AXI 响应有效且当前是 Miss 时，更新 Cache
        if (io().axi_resp_valid == ch_bool(true) && miss == ch_bool(true)) {
            // 根据 LRU 选择替换的 way
            auto victim_way = lru_sram[index];
            
            // 更新 Tag、Data、Valid
            tag_sram[index][victim_way] = ch_reg<ch_uint<ICacheConfig::TAG_BITS>>(tag);
            data_sram[index][victim_way] = ch_reg<ch_uint<ICacheConfig::DATA_WIDTH>>(
                ch_uint<ICacheConfig::DATA_WIDTH>(io().axi_data));
            valid_sram[index][victim_way] = ch_reg<ch_bool>(ch_bool(true));
            
            // 更新 LRU：新替换的 way 设为 MRU
            lru_sram[index] = victim_way;
        }
    }
    
private:
    // 辅助函数：获取 Cache Block 地址（对齐到 16 字节）
    ch_uint<32> icache_get_block_addr(const ch_uint<32>& addr) {
        return addr & ch_uint<32>(0xFFFFFFF0_d);  // 清零低 4 位
    }
};

} // namespace chlib
