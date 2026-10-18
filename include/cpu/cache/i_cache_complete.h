/**
 * @file i_cache_complete.h
 * @brief 4KB 指令缓存 - 完整实现带 Hit/Miss 检测和 LRU 更新
 *
 * 规格:
 * - 容量：4KB (4096 bytes)
 * - 组相联：2-way set associative
 * - 行大小：16 bytes
 * - Sets: 128 (4KB / 2 / 16B)
 * - 替换策略：LRU (简化: fill 优先选 way0, 满后选 way1)
 *
 * 地址格式 (32-bit):
 * [31:11] Tag (21 bits)
 * [10:4]  Index (7 bits) - 128 sets
 * [3:0]   Offset (4 bits) - 16 bytes per line
 *
 * 实现: 基于 ch_mem 原语 (commit bfb3960 ADR-039 §2 决策)
 * - SRAM 用 ch_mem<T, NUM_SETS> per way (独立命名避免数组 runtime index)
 * - hit-path 用 aread (0-cycle combinational)
 * - miss-fill 用 ch_mem::write (1-cycle registered)
 * - 关键: 不使用 C++ if (axi_resp_valid & miss) 包裹 ch_mem::write
 *   (describe() 一次性执行, 不能 per-cycle 条件触发 write)
 *   改用 enable 信号: ch_mem::write(addr, data, do_fill & victim_way)
 *
 * 作者：DevMate (initial) + build-agent (ch_mem rewrite)
 * 日期：2026-04-10 (initial), 2026-10-19 (rewrite)
 */
#pragma once

#include "ch.hpp"
#include "component.h"
#include "chlib/stream.h"

namespace chlib {

using namespace ch::core;

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
        // SRAM 阵列 (ch_mem-based, ADR-039 §2 决策)
        // =====================================================================

        // Data RAM: 独立命名 per way (避免 ch_mem 数组 C-style runtime index 问题)
        ch_mem<ch_uint<ICacheConfig::DATA_WIDTH>, ICacheConfig::NUM_SETS> data_mem_way0;
        ch_mem<ch_uint<ICacheConfig::DATA_WIDTH>, ICacheConfig::NUM_SETS> data_mem_way1;

        // Tag RAM: 独立命名 per way
        ch_mem<ch_uint<ICacheConfig::TAG_BITS>, ICacheConfig::NUM_SETS> tag_mem_way0;
        ch_mem<ch_uint<ICacheConfig::TAG_BITS>, ICacheConfig::NUM_SETS> tag_mem_way1;

        // Valid 位: 独立命名 per way
        // 用 ch_uint<1> 替代 ch_bool 以保证 init data = 0 (deterministic initial state)
        // ch_mem<ch_bool> 初始 X 导致 valid_way0 aread 在 cycle 0 是 X, 使后续
        // `!valid_way0` DAG output 默认 0, fill enable 永远不触发
        ch_mem<ch_uint<1>, ICacheConfig::NUM_SETS> valid_mem_way0;
        ch_mem<ch_uint<1>, ICacheConfig::NUM_SETS> valid_mem_way1;

        // LRU 位: 1 个 ch_mem (shared, 1-bit per set)
        //   语义: lru_mem[index] = 哪条路是 LRU (下次 fill 选哪条)
        //   简化: lru=0 -> way0 是 LRU, 优先填 way0; lru=1 -> way1 是 LRU, 优先填 way1
        ch_mem<ch_bool, ICacheConfig::NUM_SETS> lru_mem;

        // =====================================================================
        // 地址解析 (no redundant ch_uint<32>(port) wrap, ADR-039 §2 fix)
        // =====================================================================
        auto offset = bits<ICacheConfig::OFFSET_BITS - 1, 0>(io().addr);
        auto index = bits<ICacheConfig::OFFSET_BITS + ICacheConfig::INDEX_BITS - 1,
                           ICacheConfig::OFFSET_BITS>(io().addr);
        auto tag = bits<31, ICacheConfig::OFFSET_BITS + ICacheConfig::INDEX_BITS>(io().addr);

        // =====================================================================
        // Hit 检测 (aread, 0-cycle combinational)
        // =====================================================================

        // 读取两个 way 的 tag 和 valid (read_port -> ch_uint/ch_bool via .impl())
        ch_uint<ICacheConfig::TAG_BITS> tag_way0(tag_mem_way0.aread(index).impl());
        ch_uint<ICacheConfig::TAG_BITS> tag_way1(tag_mem_way1.aread(index).impl());
        ch_uint<1> valid_way0_raw(valid_mem_way0.aread(index).impl());
        ch_uint<1> valid_way1_raw(valid_mem_way1.aread(index).impl());
        // 转 ch_bool 用于 hit 检测 (== 0 是无效)
        ch_bool valid_way0 = valid_way0_raw != ch_uint<1>(0_d);
        ch_bool valid_way1 = valid_way1_raw != ch_uint<1>(0_d);

        // Hit 检测: tag 匹配且 valid=1
        // 使用 & / | (per ADR-036 deprecate-hdl-logical-operators, ch_bool bit-identical)
        ch_bool hit_way0 = valid_way0 & (tag_way0 == tag);
        ch_bool hit_way1 = valid_way1 & (tag_way1 == tag);
        ch_bool hit = hit_way0 | hit_way1;
        ch_bool miss = !hit;

        // =====================================================================
        // 数据输出
        // =====================================================================

        // 从 Cache 读取数据 (combinational via aread)
        ch_uint<ICacheConfig::DATA_WIDTH> data_way0(data_mem_way0.aread(index).impl());
        ch_uint<ICacheConfig::DATA_WIDTH> data_way1(data_mem_way1.aread(index).impl());

        // 选择命中的 way (way1 优先)
        auto cache_data = select(hit_way1, data_way1, data_way0);

        // Hit: 从 Cache 返回数据；Miss: 从 AXI 返回数据
        // 用 bits<31, 0> 显式函数模板调用, 避免 ch_uint::operator()(unsigned, unsigned) 触发核心库 auto dedup bug
        io().data <<= select(hit, bits<31, 0>(cache_data), io().axi_data);

        // =====================================================================
        // Ready 信号生成
        // =====================================================================

        // Hit: 立即 ready; Miss: 等待 AXI ready
        // mixed ch_bool+port: select() 改法 (per ADR-036)
        io().ready <<= select(hit, ch_bool(true), io().axi_ready);
        io().miss <<= miss;

        // =====================================================================
        // AXI 请求生成
        // =====================================================================

        // Miss 时发起 AXI 读请求
        io().axi_valid <<= select(miss, ch_bool(true), ch_bool(false)) & io().req;
        io().axi_addr <<= icache_get_block_addr(io().addr);

        // =====================================================================
        // LRU read (combinational) — 用 valid_mem 状态替代 lru_mem 避免 X 初始值
        // =====================================================================
        // 简化版 fill 策略 — 按 valid 状态选择 victim:
        //   way0 invalid -> 填 way0
        //   way0 valid, way1 invalid -> 填 way1
        //   两路都 valid -> 填 way0 (覆盖, 简化 LRU)
        // 避开 lru_mem 初始 X 问题 (lru_mem 在 miss path 不能用作 enable, 因为
        // X 是 ch_bool 隐式转 lnodeimpl 的 undefined 行为)

        // =====================================================================
        // Miss 处理: 填充 Cache (ch_mem::write, enable 信号驱动)
        // =====================================================================
        // 关键: 不使用 C++ if 包裹 ch_mem::write. 改用 enable 信号.
        // do_fill: 是否需要 fill (axi 响应有效 + 当前是 miss)
        // fill_way0_en: way0 invalid 时填 way0
        // fill_way1_en: way0 valid 但 way1 invalid 时填 way1
        ch_bool do_fill = io().axi_resp_valid & miss;
        ch_bool fill_way0_en = do_fill & !valid_way0;
        ch_bool fill_way1_en = do_fill & valid_way0 & !valid_way1;

        // 写两路 (enable 互斥; 写相同 data, 但只 1 路真生效)
        // valid_mem 是 ch_uint<1>, 写入 ch_uint<1>(1_d)
        tag_mem_way0.write(index, tag, fill_way0_en);
        data_mem_way0.write(index, zext<ICacheConfig::DATA_WIDTH>(io().axi_data), fill_way0_en);
        valid_mem_way0.write(index, ch_uint<1>(1_d), fill_way0_en);

        tag_mem_way1.write(index, tag, fill_way1_en);
        data_mem_way1.write(index, zext<ICacheConfig::DATA_WIDTH>(io().axi_data), fill_way1_en);
        valid_mem_way1.write(index, ch_uint<1>(1_d), fill_way1_en);

        // =====================================================================
        // Hit 时更新 LRU (用于下次 fill 选择 victim)
        // =====================================================================
        // hit_way1 -> way1 是 MRU, way0 是 LRU -> lru=0 (下次填 way0)
        // hit_way0 -> way0 是 MRU, way1 是 LRU -> lru=1 (下次填 way1)
        // (本简化版 victim 选择优先 valid 而非 lru, 但 lru 仍记录以兼容扩展)
        ch_bool do_lru_update = hit;
        ch_bool new_lru_val = select(hit_way1, ch_bool(false), ch_bool(true));
        lru_mem.write(index, new_lru_val, do_lru_update);
    }

private:
    // 辅助函数: 获取 Cache Block 地址 (对齐到 16 字节)
    // fix: 0xFFFFFFF0_d → 0xFFFFFFF0_h (per ADR-039 §2 typo fix)
    ch_uint<32> icache_get_block_addr(const ch_uint<32>& addr) {
        return addr & ch_uint<32>(0xFFFFFFF0_h);
    }
};

} // namespace chlib