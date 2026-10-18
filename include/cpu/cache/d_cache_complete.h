/**
 * @file d_cache_complete.h
 * @brief 4KB 数据缓存 - 完整实现带 Hit/Miss 检测、LRU 和 AXI 握手
 *
 * 规格:
 * - 容量: 4KB (4096 bytes)
 * - 组相联: 2-way set associative
 * - 行大小: 16 bytes
 * - Sets: 128 (4KB / 2 / 16B)
 * - 写策略: Write-Through
 *
 * 地址格式 (32-bit):
 * [31:11] Tag (21 bits)
 * [10:4]  Index (7 bits) - 128 sets
 * [3:0]   Offset (4 bits) - 16 bytes per line
 *
 * 实现: 基于 ch_mem 原语 (commit bfb3960 ADR-039 §2 决策)
 * - SRAM 用 ch_mem<T, NUM_SETS> per way
 * - hit-path 用 aread (0-cycle combinational)
 * - miss-fill 用 ch_mem::write (1-cycle registered)
 * - write-through: hit+we 同时写 cache_data + axi_wdata
 *
 * 作者: DevMate (initial) + build-agent (ch_mem rewrite)
 * 日期: 2026-04-10 (initial), 2026-10-19 (rewrite)
 */
#pragma once

#include "ch.hpp"
#include "component.h"
#include "chlib/stream.h"

namespace chlib {

using namespace ch::core;

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
        ch_in<ch_bool> we;                // 写使能 (1=写, 0=读)

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
        // SRAM 阵列 (ch_mem-based, ADR-039 §2 决策)
        // =====================================================================

        // Data RAM: 独立命名 per way (避免 ch_mem 数组 C-style runtime index 问题)
        ch_mem<ch_uint<DCacheConfig::DATA_WIDTH>, DCacheConfig::NUM_SETS> data_mem_way0;
        ch_mem<ch_uint<DCacheConfig::DATA_WIDTH>, DCacheConfig::NUM_SETS> data_mem_way1;

        // Tag RAM: 独立命名 per way
        ch_mem<ch_uint<DCacheConfig::TAG_BITS>, DCacheConfig::NUM_SETS> tag_mem_way0;
        ch_mem<ch_uint<DCacheConfig::TAG_BITS>, DCacheConfig::NUM_SETS> tag_mem_way1;

        // Valid 位: 独立命名 per way
        ch_mem<ch_bool, DCacheConfig::NUM_SETS> valid_mem_way0;
        ch_mem<ch_bool, DCacheConfig::NUM_SETS> valid_mem_way1;

        // LRU 位: 1 个 ch_mem (shared, 1-bit per set)
        ch_mem<ch_bool, DCacheConfig::NUM_SETS> lru_mem;

        // =====================================================================
        // 地址解析 (no redundant ch_uint<32>(port) wrap, ADR-039 §2 fix)
        // =====================================================================
        auto offset = bits<DCacheConfig::OFFSET_BITS - 1, 0>(io().addr);
        auto index = bits<DCacheConfig::OFFSET_BITS + DCacheConfig::INDEX_BITS - 1,
                           DCacheConfig::OFFSET_BITS>(io().addr);
        auto tag = bits<31, DCacheConfig::OFFSET_BITS + DCacheConfig::INDEX_BITS>(io().addr);

        // =====================================================================
        // Hit/Miss 检测 (aread, 0-cycle combinational)
        // =====================================================================

        ch_uint<DCacheConfig::TAG_BITS> tag_way0(tag_mem_way0.aread(index).impl());
        ch_uint<DCacheConfig::TAG_BITS> tag_way1(tag_mem_way1.aread(index).impl());
        ch_bool valid_way0(valid_mem_way0.aread(index).impl());
        ch_bool valid_way1(valid_mem_way1.aread(index).impl());

        // 使用 & / | (per ADR-036 deprecate-hdl-logical-operators, ch_bool bit-identical)
        ch_bool hit_way0 = valid_way0 & (tag_way0 == tag);
        ch_bool hit_way1 = valid_way1 & (tag_way1 == tag);
        ch_bool hit = hit_way0 | hit_way1;
        ch_bool miss = !hit;

        io().miss <<= miss;

        // =====================================================================
        // 读操作: Hit 时从 Cache 返回
        // =====================================================================
        ch_uint<DCacheConfig::DATA_WIDTH> data_way0(data_mem_way0.aread(index).impl());
        ch_uint<DCacheConfig::DATA_WIDTH> data_way1(data_mem_way1.aread(index).impl());
        auto cache_data = select(hit_way1, data_way1, data_way0);

        // Hit: Cache 返回; Miss: AXI 返回
        // 取 cache line 低 32-bit 作为 rdata 输出 (与 axi_rdata 同宽度)
        // 用 bits<31, 0>(cache_data) 显式函数模板调用, 避免 ch_uint::operator()(unsigned, unsigned) runtime bits 核心库 bug
        io().rdata <<= select(hit, bits<31, 0>(cache_data), io().axi_rdata);

        // =====================================================================
        // Write-Through 写操作
        // =====================================================================

        // 写操作: 同时更新 Cache (如果 Hit) 和 AXI 总线
        io().axi_we <<= io().we;
        io().axi_wdata <<= io().wdata;
        io().axi_addr <<= dcache_get_block_addr(io().addr);

        // Hit 且写操作时, 更新 Cache Data (Write-Through)
        // 实际应用中可能需要字节掩码, 这里简化为全写
        if (hit == ch_bool(true) & io().we == ch_bool(true)) {
            // 简化: 假设写入整个 Data 块
            // mixed ch_bool+port: 先 select 展开成 ch_bool
            ch_bool do_write_way0 = select(hit_way0, ch_bool(true), ch_bool(false));
            ch_bool do_write_way1 = select(hit_way1, ch_bool(true), ch_bool(false));

            data_mem_way0.write(index, zext<DCacheConfig::DATA_WIDTH>(io().wdata), do_write_way0);
            data_mem_way1.write(index, zext<DCacheConfig::DATA_WIDTH>(io().wdata), do_write_way1);
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
        // (miss && is_read) || io().we 改写:
        //   miss & is_read 是 ch_bool & ch_bool (合法)
        //   再 | port 用 select 展开
        ch_bool miss_read = miss & is_read;
        io().axi_valid <<= select(miss_read, ch_bool(true), io().we);

        // =====================================================================
        // LRU 更新和 Miss 填充 (1-cycle registered via ch_mem::write)
        // =====================================================================

        if (miss == ch_bool(true) & io().axi_resp_valid == ch_bool(true) & is_read == ch_bool(true)) {
            // Miss 填充
            // victim_way: lru_mem[index] = 哪条路是 LRU (即要被替换的)
            //   lru=0 (ch_bool(false)) → 替换 way0;  lru=1 (ch_bool(true)) → 替换 way1
            ch_bool victim_way(lru_mem.aread(index).impl());

            // if/else 分发到独立 ch_mem (避免 C-style 数组 runtime index)
            if (victim_way == ch_bool(true)) {
                // 替换 way1
                tag_mem_way1.write(index, tag, ch_bool(true));
                data_mem_way1.write(index, zext<DCacheConfig::DATA_WIDTH>(io().axi_rdata), ch_bool(true));
                valid_mem_way1.write(index, ch_bool(true), ch_bool(true));
                // way1 新替换 → way1 是 MRU → way0 是 LRU → lru = false
                lru_mem.write(index, ch_bool(false), ch_bool(true));
            } else {
                // 替换 way0
                tag_mem_way0.write(index, tag, ch_bool(true));
                data_mem_way0.write(index, zext<DCacheConfig::DATA_WIDTH>(io().axi_rdata), ch_bool(true));
                valid_mem_way0.write(index, ch_bool(true), ch_bool(true));
                // way0 新替换 → way0 是 MRU → way1 是 LRU → lru = true
                lru_mem.write(index, ch_bool(true), ch_bool(true));
            }
        }

        // Hit 时更新 LRU
        if (hit == ch_bool(true)) {
            lru_mem.write(index, select(hit_way1, ch_bool(true), ch_bool(false)), ch_bool(true));
        }
    }

private:
    // 辅助函数: 获取 Cache Block 地址 (对齐到 16 字节)
    // fix: 0xFFFFFFF0_d → 0xFFFFFFF0_h (per ADR-039 §2 typo fix)
    ch_uint<32> dcache_get_block_addr(const ch_uint<32>& addr) {
        return addr & ch_uint<32>(0xFFFFFFF0_h);
    }
};

} // namespace chlib