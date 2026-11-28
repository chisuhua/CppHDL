/**
 * @file i_cache_sync.h
 * @brief 4KB 指令缓存 (1-cycle sread) — RV32I L1 imem 默认实现
 *
 * 规格:
 * - 容量: 4KB (4096 bytes)
 * - 组相联: 2-way set associative
 * - 行大小: 16 bytes
 * - Sets: 128 (4KB / 2 / 16B)
 * - 替换策略: LRU (简化: fill 优先选 invalid way, 两路都 valid 时填 way0)
 * - 读延迟: **1 cycle (sread, registered)**
 *
 * 地址格式 (32-bit):
 * [31:11] Tag (21 bits)
 * [10:4]  Index (7 bits) - 128 sets
 * [3:0]   Offset (4 bits) - 16 bytes per line
 * [3:2]   Lane (2 bits) - 16 B line 内的 4 个 32-bit word 之一
 *
 * 实现: 基于 ch_mem 原语 (commit bfb3960 ADR-039 §2 决策)
 * - SRAM: ch_mem<T, NUM_SETS> per way (独立命名避免数组 runtime index)
 * - hit-path 用 sread (1-cycle registered) — 对齐 imem 必须 sread 的项目约定
 *   (root AGENTS.md MEM READ PORT + verilator-e2e-coverage spec §R1 invariant 3)
 * - miss-fill 用 ch_mem::write (1-cycle registered)
 * - fill 路径用 enable 信号 (per cache-complete-doa-note §6.1 R3 hard-gate)
 *
 * 与 i_cache_async.h 的区别: hit latency 1 cycle (本类) vs 0 cycle (async)
 *
 * ⚠️ **2026-11-28 · S4c-1a（ADR-078 D7）—— AXI 侧 AXI4-full 化 + 修「假 line」**：
 *   ① **AXI 侧**：5 根简化握手 → **AXI4-full 读通道**（`ar_*` 6 根 + `r_*` 5 根）
 *      ⇒ ⇒ **只读 ⇒ 无写通道**（AW/W/B）⇒ ⇒ 与标准 AXI4-full 从设备对接。
 *      形态参照 `d_cache_sync.h`（S3 · `81097dc`）。
 *   ② **fill 路径**：单拍 `zext<128>(axi_data)` → **4-beat burst fill**
 *      （`ARLEN=3` / `ARSIZE=2` / `ARBURST=INCR`）⇒ ⇒ 整行 4 个 lane 各自落位。
 *   ③ **读路径**：恒取 `bits<31,0>` → **按 `addr[3:2]` 选 lane**（**修「假 line」**：
 *      同一 line 的 4 个地址原本恒读到首字 ⇒ 功能错误）。
 *      ⚠️ 与 D 侧同构，但数据源是 **`sread`**（1 拍）⇒ ⇒ 选择在拿到 line 之后做。
 *   ④ **`ready` 语义保持 I 侧既有契约**（**不**照搬 D 侧的组合 ready）：
 *      「**请求已被接受**」⇒ hit ⇒ 数据在**下一拍**到；miss ⇒ 填充完成后
 *      **下一拍** `ready` 与 `data` **同拍**有效 ⇒ ⇒ 两条路径的数据延迟统一为 1 拍。
 *      ⚠️ 用**寄存器** `fill_done_r`（不是组合的 `fill_complete`）—— 后者只活 1 拍，
 *      观测方在 tick **之后**读**永远是 0**。
 *   ⑤ **命中延迟不变**（1 拍 `sread`）⇒ ⇒ `i_cache_async.h` **零 diff**
 *      ⇒ ⇒ ADR-041 的变体族（命中延迟维度）不扩张。
 *
 * ⚠️ **调用方契约（沿用并扩展到 burst fill）**：`req` 有效期间 MUST 保持 `addr` 不变
 *   —— `miss`/`hit` 来自 1 拍 `sread`，而 `ar_addr` 与 `lane_sel` 取**当拍** `addr`
 *   ⇒ ⇒ 填充期间改 `addr` ⇒ 取回**别的块** / 选错 lane。
 *
 * 作者: build-agent (cache-complete-rewrite Session B, ADR-041)
 * 日期: 2026-10-19 · 2026-11-28 S4c-1a
 */
#pragma once

#include "ch.hpp"
#include "component.h"
#include "chlib/stream.h"

namespace chlib {

using namespace ch::core;

/**
 * @brief I-Cache 配置参数 (与异步版本共享)
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
 * @brief I-Cache 同步版 (1-cycle sread) — RV32I L1 imem 默认
 *
 * hit 延迟: 1 cycle (sread 采样 + 比较 + lane 选择)
 * miss: AR 握手 → 4 个 R beat → 整行写回 ⇒ ⇒ 完成后**下一拍** `ready`/`data` 有效
 *
 * 注: 此处描述的是 "提交 addr 到 data 输出" 的总延迟. RV32I 集成时 pc_lag
 *   由 IF/ID pipeline reg 自然吸收, 无需额外对齐 logic.
 */
class ICacheSync : public ch::Component {
public:
    __io(
        // CPU 请求接口
        ch_in<ch_uint<32>> addr;          // 请求地址
        ch_in<ch_bool> req;               // 请求有效

        // 数据输出
        ch_out<ch_uint<32>> data;         // 读出指令
        ch_out<ch_bool> ready;            // 就绪信号（「请求已被接受」）
        ch_out<ch_bool> miss;             // 未命中信号

        // ---- AXI4 full 主口 · AR（**只读** ⇒ 无 AW/W/B 写通道）----
        ch_out<ch_uint<32>> ar_addr{"ar_addr"};
        ch_out<ch_uint<4>>  ar_len{"ar_len"};
        ch_out<ch_uint<3>>  ar_size{"ar_size"};
        ch_out<ch_uint<2>>  ar_burst{"ar_burst"};
        ch_out<ch_bool>     ar_valid{"ar_valid"};
        ch_in<ch_bool>      ar_ready{"ar_ready"};

        // ---- AXI4 full 主口 · R ----
        ch_in<ch_uint<32>>  r_data{"r_data"};
        ch_in<ch_uint<2>>   r_resp{"r_resp"};
        ch_in<ch_bool>      r_last{"r_last"};
        ch_in<ch_bool>      r_valid{"r_valid"};
        ch_out<ch_bool>     r_ready{"r_ready"};
    )

    ICacheSync(ch::Component* parent = nullptr,
               const std::string& name = "icache_sync")
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

        // Valid 位: 独立命名 per way (ch_uint<1> 避免 ch_mem<ch_bool> 初始 X 问题)
        ch_mem<ch_uint<1>, ICacheConfig::NUM_SETS> valid_mem_way0;
        ch_mem<ch_uint<1>, ICacheConfig::NUM_SETS> valid_mem_way1;

        // LRU 位: 1 个 ch_mem (shared, 1-bit per set)
        ch_mem<ch_bool, ICacheConfig::NUM_SETS> lru_mem;

        // =====================================================================
        // 地址解析 (no redundant ch_uint<32>(port) wrap, ADR-039 §2 fix)
        // =====================================================================
        auto index = bits<ICacheConfig::OFFSET_BITS + ICacheConfig::INDEX_BITS - 1,
                           ICacheConfig::OFFSET_BITS>(io().addr);
        auto tag = bits<31, ICacheConfig::OFFSET_BITS + ICacheConfig::INDEX_BITS>(io().addr);
        // lane 选择 = addr[3:2]（16 B line 内的 4 个 word 之一）—— **修「假 line」**
        auto lane_sel = bits<3, 2>(io().addr);

        // =====================================================================
        // Hit 检测 (sread, 1-cycle registered)
        // =====================================================================

        // 读取两个 way 的 tag 和 valid (sread addr, enable=ch_bool(true))
        ch_uint<ICacheConfig::TAG_BITS> tag_way0(tag_mem_way0.sread(index).impl());
        ch_uint<ICacheConfig::TAG_BITS> tag_way1(tag_mem_way1.sread(index).impl());
        ch_uint<1> valid_way0_raw(valid_mem_way0.sread(index).impl());
        ch_uint<1> valid_way1_raw(valid_mem_way1.sread(index).impl());
        ch_bool valid_way0 = valid_way0_raw != ch_uint<1>(0_d);
        ch_bool valid_way1 = valid_way1_raw != ch_uint<1>(0_d);

        // Hit 检测: tag 匹配且 valid=1
        ch_bool hit_way0 = valid_way0 & (tag_way0 == tag);
        ch_bool hit_way1 = valid_way1 & (tag_way1 == tag);
        ch_bool hit = hit_way0 | hit_way1;
        ch_bool miss = !hit;

        // =====================================================================
        // 数据输出 (sread 1-cycle registered)
        // =====================================================================

        ch_uint<ICacheConfig::DATA_WIDTH> data_way0(data_mem_way0.sread(index).impl());
        ch_uint<ICacheConfig::DATA_WIDTH> data_way1(data_mem_way1.sread(index).impl());

        // 选择命中的 way (way1 优先)
        auto cache_data = select(hit_way1, data_way1, data_way0);

        // 读数据：按 lane_sel 从 line 里选 32-bit word（**修「假 line」** —— S4c-1a）
        // ⚠️ 数据源是 **`sread`**（1 拍）⇒ ⇒ 结果在下一拍有效（与既有 hit 延迟一致）
        // ⚠️ 用 `bits<h,l>` 显式函数模板调用, 避免 ch_uint::operator() 核心库 bug
        auto hit_word = select(
            lane_sel == ch_uint<2>(0_d), bits<31, 0>(cache_data),
            select(lane_sel == ch_uint<2>(1_d), bits<63, 32>(cache_data),
                   select(lane_sel == ch_uint<2>(2_d), bits<95, 64>(cache_data),
                          bits<127, 96>(cache_data))));

        // =====================================================================
        // AXI4-full 读通道：AR 请求 + 4-beat burst fill 状态机
        // =====================================================================

        // 4 个 beat **各自命名**（CppHDL 不支持 ch_reg 数组）
        ch_reg<ch_uint<32>> beat0{0_d, "beat0"};
        ch_reg<ch_uint<32>> beat1{0_d, "beat1"};
        ch_reg<ch_uint<32>> beat2{0_d, "beat2"};
        ch_reg<ch_uint<32>> beat3{0_d, "beat3"};
        ch_reg<ch_uint<2>>  beat_idx{0_d, "beat_idx"};
        ch_reg<ch_bool>     fill_active{false, "fill_active"};
        // 锁存：line 的 index / tag / 目标 way / 目标 lane 的数据
        ch_reg<ch_uint<ICacheConfig::INDEX_BITS>> fill_index{0_d, "fill_index"};
        ch_reg<ch_uint<ICacheConfig::TAG_BITS>>   fill_tag{0_d, "fill_tag"};
        ch_reg<ch_bool>     fill_way1{false, "fill_way1"};
        ch_reg<ch_uint<32>> fill_target{0_d, "fill_target"};
        // 填充完成后的数据 / 就绪（⇒ 数据延迟与 hit 路径**统一为 1 拍**）
        ch_reg<ch_uint<32>> miss_data_r{0_d, "miss_data_r"};
        ch_reg<ch_bool>     fill_done_r{false, "fill_done_r"};

        ch_bool miss_read = miss & io().req;
        ch_bool ar_fire = io().ar_valid & io().ar_ready;

        // AR：向 line 基址发 4-beat INCR burst
        // ⚠️ `ar_valid` MUST NOT 依赖 `ar_ready`（AXI：VALID 不得等 READY）
        // ⚠️ `!fill_done_r`：填充完成的那一拍 `miss` 仍来自**上一拍**的 `sread`
        //    ⇒ 若不挡 ⇒ 会在下一拍对同一 line 再发一次 AR（无谓带宽 + 可能覆盖
        //    lane 数据）。`fill_done_r` 恰好覆盖这一拍。
        io().ar_valid <<= miss_read & !fill_active & !fill_done_r;
        io().ar_addr  <<= (io().addr & ch_uint<32>(0xFFFFFFF0_h));
        io().ar_len   <<= ch_uint<4>(3_d);   // 4 beats
        io().ar_size  <<= ch_uint<3>(2_d);   // 4 bytes / beat
        io().ar_burst <<= ch_uint<2>(1_d);   // INCR

        // R：本 cache 无 R 缓冲 ⇒ 恒可收
        ch_bool r_fire = io().r_valid & io().r_ready;
        io().r_ready <<= ch_bool(true);

        auto beat0_en = r_fire & fill_active & (beat_idx == ch_uint<2>(0_d));
        auto beat1_en = r_fire & fill_active & (beat_idx == ch_uint<2>(1_d));
        auto beat2_en = r_fire & fill_active & (beat_idx == ch_uint<2>(2_d));
        auto beat3_en = r_fire & fill_active & (beat_idx == ch_uint<2>(3_d));
        beat0->next = select(beat0_en, io().r_data, beat0);
        beat1->next = select(beat1_en, io().r_data, beat1);
        beat2->next = select(beat2_en, io().r_data, beat2);
        beat3->next = select(beat3_en, io().r_data, beat3);

        // 目标 lane 的数据（**当拍**取值 ⇒ 覆盖「目标 lane 就是最后一个 beat」的情形）
        ch_bool lane_hit = (lane_sel == beat_idx);
        auto target_now = select(lane_hit, io().r_data, fill_target);
        fill_target->next = select(r_fire & fill_active, target_now, fill_target);

        ch_bool fill_complete = r_fire & fill_active & io().r_last;

        beat_idx->next = select(fill_complete, ch_uint<2>(0_d),
                                select(r_fire & fill_active,
                                       beat_idx + ch_uint<2>(1_d), beat_idx));
        fill_active->next = select(fill_complete, ch_bool(false),
                                   select(ar_fire, ch_bool(true), fill_active));

        // 锁存（AR 握手那拍 —— 调用方 MUST 在 `req` 期间不改 `addr`）
        fill_index->next = select(ar_fire, index, fill_index);
        fill_tag->next   = select(ar_fire, tag, fill_tag);
        // way 选择 = 简化 LRU（优先填 invalid 的 way；两路都 valid 则填 way0）
        // ⚠️ 两路都 valid 时**必须**仍选一路写回 —— 否则 fill 完成却不落盘 ⇒
        //    同一 line 永远 miss（活锁）。本选择即文件头声明的「简化 LRU」。
        auto choose_way1 = select(!valid_way0, ch_bool(false),
                                  select(!valid_way1, ch_bool(true), ch_bool(false)));
        fill_way1->next  = select(ar_fire, choose_way1, fill_way1);

        // ---- 写回 line（4 beat 拼成 128 bit，一次整行写）----
        // ⚠️ **off-by-one 陷阱**（S3 实测踩中 · `81097dc`）：`fill_complete` 那一拍，
        //    第 4 个 beat 的 `->next` 才在**同一沿**上装载 ⇒ ⇒ 此时读 `beat3` 仍是
        //    旧值（滞后 1 拍）⇒ ⇒ 直接用它拼整行会 **lane 3 恒 0** ⇒ ⇒ 写回那拍
        //    MUST 用**当拍** `r_data`。
        auto beat3_final = select(fill_complete, io().r_data, beat3);
        auto full_line = concat(concat(beat3_final, beat2), concat(beat1, beat0));
        auto fill_w0 = fill_complete & !fill_way1;
        auto fill_w1 = fill_complete & fill_way1;
        data_mem_way0.write(fill_index, full_line, fill_w0);
        data_mem_way1.write(fill_index, full_line, fill_w1);
        tag_mem_way0.write(fill_index, fill_tag, fill_w0);
        tag_mem_way1.write(fill_index, fill_tag, fill_w1);
        valid_mem_way0.write(fill_index, ch_uint<1>(1_d), fill_w0);
        valid_mem_way1.write(fill_index, ch_uint<1>(1_d), fill_w1);
        // fill 完成后 LRU 指向**另一路**（刚填的那路是最近使用）
        lru_mem.write(fill_index, !fill_way1, fill_complete);

        // miss 路径：填充完成那拍锁存「请求 lane 的字」⇒ 下一拍与 `ready` 同拍可见
        miss_data_r->next = select(fill_complete, fill_target, miss_data_r);
        fill_done_r->next = fill_complete;

        // =====================================================================
        // Ready / Miss / Data
        // =====================================================================

        // ⚠️ **I 侧的 `ready` 语义 = 「请求已被接受」**（**不**照搬 D 侧的组合
        //    「数据此刻可用」）：hit ⇒ 数据下一拍到；miss ⇒ 填充完成后下一拍
        //    `ready` 与 `data` 同拍有效。
        io().ready <<= select(hit, ch_bool(true), fill_done_r);
        io().miss  <<= miss;

        // 数据：hit ⇒ lane 选择结果（sread ⇒ 下一拍）；miss ⇒ 锁存的请求 lane 数据
        io().data <<= select(hit, hit_word, miss_data_r);

        // =====================================================================
        // Hit 时更新 LRU
        // =====================================================================
        ch_bool do_lru_update = hit;
        ch_bool new_lru_val = select(hit_way1, ch_bool(false), ch_bool(true));
        lru_mem.write(index, new_lru_val, do_lru_update);
    }
};

} // namespace chlib
