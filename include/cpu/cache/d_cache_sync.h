/**
 * @file d_cache_sync.h
 * @brief 4KB 数据缓存 (1-cycle sread) — RV32I L1 dmem 默认实现
 *
 * 规格:
 * - 容量: 4KB (4096 bytes) · 2-way set associative · line 16 B (4 word)
 * - Sets: 128 · 写策略: **Write-Through** · 读延迟: **1 cycle (sread)**
 *
 * 地址格式 (32-bit):  [31:11] Tag (21) · [10:4] Index (7) · [3:2] Lane (2) · [1:0] Byte (2)
 *
 * ## ⚠️ 2026-11-23 · S3（change `rv32i-dcache-burst-2026-11`）的**两处结构性变更**
 *
 * 1. **AXI 侧改 AXI4 full 主口**（G-7 裁决 = full 主干）：
 *    - 读 miss ⇒ **4-beat burst** fill（`ARLEN=3` / `ARSIZE=2` / `ARBURST=INCR`）
 *    - 写 ⇒ **write-through 单 beat**（`AWLEN=0`）
 * 2. **修「假 line」**（旧实现的真缺陷）：
 *    旧版 fill 是 `zext<128>(axi_rdata)`、读是 `bits<31,0>(cache_data)`
 *    ⇒ ⇒ **`offset` 完全不参与数据选择** ⇒ 同一 line 的 4 个地址恒读第一个字。
 *    新版：`addr[3:2]` 参与**读选择**与**写更新**；fill 的 4 个 beat **各落一个 lane**。
 *
 * ## 前端契约（**不变**）
 *
 * `addr` / `wdata` / `req` / `we`  →  `rdata` / `ready` / `miss`
 *
 * - `hit` ⇒ `ready` 与 `rdata` **同拍**有效（1 拍，与 `DataTCM` 的 `sread` 对齐）
 * - `miss` ⇒ `ready=0` 直到 fill 完成 ⇒ 流水线用 `data_ready` **反压**等待
 *   （S2 的冻结链 = 这条通路的消费者）
 * - ⚠️ **调用方 MUST 在 `miss` 期间保持 `addr` 不变**（`miss` 是 `sread` 的 1 拍延迟
 *   结果；本组件在 `miss` 有效时**锁存** line 基址，但契约本身不变）
 *
 * 作者: build-agent（cache-complete-rewrite Session B, ADR-041 · S3 重写 2026-11-23）
 */
#pragma once

#include "ch.hpp"
#include "component.h"
#include "chlib/stream.h"

namespace chlib {

using namespace ch::core;

/**
 * @brief D-Cache 配置参数 (与异步版本共享)
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
    static constexpr unsigned LANES = 4;              // 4 × 32-bit word
};

/**
 * @brief D-Cache 同步版 (1-cycle sread · AXI4 full 主口 · 真 line)
 */
class DCacheSync : public ch::Component {
public:
    __io(
        // ---- 前端（与旧版逐字一致）----
        ch_in<ch_uint<32>> addr;
        ch_in<ch_uint<32>> wdata;
        ch_in<ch_bool> req;
        ch_in<ch_bool> we;               // 1=写, 0=读

        ch_out<ch_uint<32>> rdata;
        ch_out<ch_bool> ready;
        ch_out<ch_bool> miss;

        // ---- AXI4 full 主口 · AR ----
        ch_out<ch_uint<32>> ar_addr;
        ch_out<ch_uint<4>>  ar_len;
        ch_out<ch_uint<3>>  ar_size;
        ch_out<ch_uint<2>>  ar_burst;
        ch_out<ch_bool>     ar_valid;
        ch_in<ch_bool>      ar_ready;

        // ---- AXI4 full 主口 · R ----
        ch_in<ch_uint<32>>  r_data;
        ch_in<ch_uint<2>>   r_resp;
        ch_in<ch_bool>      r_last;
        ch_in<ch_bool>      r_valid;
        ch_out<ch_bool>     r_ready;

        // ---- AXI4 full 主口 · AW ----
        ch_out<ch_uint<32>> aw_addr;
        ch_out<ch_uint<4>>  aw_len;
        ch_out<ch_uint<3>>  aw_size;
        ch_out<ch_uint<2>>  aw_burst;
        ch_out<ch_bool>     aw_valid;
        ch_in<ch_bool>      aw_ready;

        // ---- AXI4 full 主口 · W ----
        ch_out<ch_uint<32>> w_data;
        ch_out<ch_uint<4>>  w_strb;
        ch_out<ch_bool>     w_last;
        ch_out<ch_bool>     w_valid;
        ch_in<ch_bool>      w_ready;

        // ---- AXI4 full 主口 · B ----
        ch_in<ch_uint<2>>   b_resp;
        ch_in<ch_bool>      b_valid;
        ch_out<ch_bool>     b_ready;
    )

    DCacheSync(ch::Component* parent = nullptr,
               const std::string& name = "dcache_sync")
        : ch::Component(parent, name) {}

    void create_ports() override { new (io_storage_) io_type; }

    void describe() override {
        // =====================================================================
        // SRAM 阵列（ch_mem · per-way 独立命名）
        // =====================================================================
        ch_mem<ch_uint<DCacheConfig::DATA_WIDTH>, DCacheConfig::NUM_SETS> data_mem_way0;
        ch_mem<ch_uint<DCacheConfig::DATA_WIDTH>, DCacheConfig::NUM_SETS> data_mem_way1;
        ch_mem<ch_uint<DCacheConfig::TAG_BITS>, DCacheConfig::NUM_SETS> tag_mem_way0;
        ch_mem<ch_uint<DCacheConfig::TAG_BITS>, DCacheConfig::NUM_SETS> tag_mem_way1;
        ch_mem<ch_uint<1>, DCacheConfig::NUM_SETS> valid_mem_way0;
        ch_mem<ch_uint<1>, DCacheConfig::NUM_SETS> valid_mem_way1;
        ch_mem<ch_bool, DCacheConfig::NUM_SETS> lru_mem;

        // =====================================================================
        // 地址解析
        // =====================================================================
        auto offset = bits<DCacheConfig::OFFSET_BITS - 1, 0>(io().addr);
        auto index = bits<DCacheConfig::OFFSET_BITS + DCacheConfig::INDEX_BITS - 1,
                           DCacheConfig::OFFSET_BITS>(io().addr);
        auto tag = bits<31, DCacheConfig::OFFSET_BITS + DCacheConfig::INDEX_BITS>(io().addr);
        // ⚠️ **lane 选择 = addr[3:2]**（16 B line 内的 4 个 word 之一）
        auto lane_sel = bits<3, 2>(io().addr);

        // =====================================================================
        // Hit/Miss（sread · 1 拍寄存）
        // =====================================================================
        ch_uint<DCacheConfig::TAG_BITS> tag_way0(tag_mem_way0.sread(index).impl());
        ch_uint<DCacheConfig::TAG_BITS> tag_way1(tag_mem_way1.sread(index).impl());
        ch_uint<1> valid_way0_raw(valid_mem_way0.sread(index).impl());
        ch_uint<1> valid_way1_raw(valid_mem_way1.sread(index).impl());
        ch_bool valid_way0 = valid_way0_raw != ch_uint<1>(0_d);
        ch_bool valid_way1 = valid_way1_raw != ch_uint<1>(0_d);

        ch_bool hit_way0 = valid_way0 & (tag_way0 == tag);
        ch_bool hit_way1 = valid_way1 & (tag_way1 == tag);
        ch_bool hit = hit_way0 | hit_way1;
        ch_bool miss = !hit;
        io().miss <<= miss;

        // =====================================================================
        // 读命中：按 lane_sel 从 line 里选 32-bit word（**修「假 line」**）
        // =====================================================================
        ch_uint<DCacheConfig::DATA_WIDTH> data_way0(data_mem_way0.sread(index).impl());
        ch_uint<DCacheConfig::DATA_WIDTH> data_way1(data_mem_way1.sread(index).impl());
        auto cache_line = select(hit_way1, data_way1, data_way0);
        auto hit_word = select(
            lane_sel == ch_uint<2>(0_d), bits<31, 0>(cache_line),
            select(lane_sel == ch_uint<2>(1_d), bits<63, 32>(cache_line),
                   select(lane_sel == ch_uint<2>(2_d), bits<95, 64>(cache_line),
                          bits<127, 96>(cache_line))));

        // =====================================================================
        // 读 miss 的 burst fill 状态机（4 beat → 4 lane → 一次写整行）
        // =====================================================================
        // ⚠️ 4 个 beat **各自命名**（CppHDL 不支持 ch_reg 数组）
        ch_reg<ch_uint<32>> beat0{0_d, "beat0"};
        ch_reg<ch_uint<32>> beat1{0_d, "beat1"};
        ch_reg<ch_uint<32>> beat2{0_d, "beat2"};
        ch_reg<ch_uint<32>> beat3{0_d, "beat3"};
        ch_reg<ch_uint<2>>  beat_idx{0_d, "beat_idx"};
        ch_reg<ch_bool>     fill_active{false, "fill_active"};
        // 锁存：line 基址 / index / tag / 目标 way / 目标 lane 的数据
        ch_reg<ch_uint<32>> fill_base{0_d, "fill_base"};
        ch_reg<ch_uint<DCacheConfig::INDEX_BITS>> fill_index{0_d, "fill_index"};
        ch_reg<ch_uint<DCacheConfig::TAG_BITS>>   fill_tag{0_d, "fill_tag"};
        ch_reg<ch_bool>     fill_way1{false, "fill_way1"};
        ch_reg<ch_uint<32>> fill_target{0_d, "fill_target"};
        ch_reg<ch_bool>     fill_done{false, "fill_done"};

        // ---- 发起 AR ----
        ch_bool is_read = !io().we;
        ch_bool miss_read = miss & is_read & io().req;
        ch_bool ar_fire = io().ar_valid & io().ar_ready;

        io().ar_valid <<= miss_read & !fill_active;
        io().ar_addr  <<= (io().addr & ch_uint<32>(0xFFFFFFF0_h));
        io().ar_len   <<= ch_uint<4>(3_d);   // 4 beats
        io().ar_size  <<= ch_uint<3>(2_d);   // 4 bytes / beat
        io().ar_burst <<= ch_uint<2>(1_d);   // INCR

        // ---- 收 beat ----
        ch_bool r_fire = io().r_valid & io().r_ready;
        io().r_ready <<= ch_bool(true);      // 本 cache 无 R 缓冲 ⇒ 恒 1

        auto beat0_en = r_fire & fill_active & (beat_idx == ch_uint<2>(0_d));
        auto beat1_en = r_fire & fill_active & (beat_idx == ch_uint<2>(1_d));
        auto beat2_en = r_fire & fill_active & (beat_idx == ch_uint<2>(2_d));
        auto beat3_en = r_fire & fill_active & (beat_idx == ch_uint<2>(3_d));
        beat0->next = select(beat0_en, io().r_data, beat0);
        beat1->next = select(beat1_en, io().r_data, beat1);
        beat2->next = select(beat2_en, io().r_data, beat2);
        beat3->next = select(beat3_en, io().r_data, beat3);

        // 目标 lane 的数据（供 fill 完成那拍直接返回，免二次读 mem）
        auto lane_hit = (lane_sel == beat_idx);
        fill_target->next = select(r_fire & fill_active & lane_hit, io().r_data, fill_target);

        ch_bool fill_complete = r_fire & fill_active & io().r_last;

        beat_idx->next = select(fill_complete, ch_uint<2>(0_d),
                                select(r_fire & fill_active,
                                       beat_idx + ch_uint<2>(1_d), beat_idx));
        fill_active->next = select(fill_complete, ch_bool(false),
                                   select(ar_fire, ch_bool(true), fill_active));
        fill_done->next = fill_complete;

        // 锁存（AR 握手那拍）
        fill_base->next  = select(ar_fire, io().addr & ch_uint<32>(0xFFFFFFF0_h), fill_base);
        fill_index->next = select(ar_fire, index, fill_index);
        fill_tag->next   = select(ar_fire, tag, fill_tag);
        // way 选择 = LRU（优先填 invalid 的 way；都 valid 则按 LRU）
        auto choose_way1 = select(!valid_way0, ch_bool(false),
                                  select(!valid_way1, ch_bool(true),
                                         lru_mem.aread(index) == ch_bool(false)));
        fill_way1->next  = select(ar_fire, choose_way1, fill_way1);

        // ---- 写回 line（4 beat 拼成 128 bit，一次整行写）----
        // ⚠️ **off-by-one 陷阱**（实测踩中）：`fill_complete` 那一拍，`beat3` 的
        //    `->next` 才在**同一沿**上装载 ⇒ 此时读寄存器 `beat3` 仍是**旧值**（1 拍滞后）
        //    ⇒ ⇒ 若直接用 `beat3` 拼整行，**lane 3 恒为 0**。
        //    ⇒ 修法：写回那拍用**当拍的 `r_data`**（它就是第 4 个 beat 的值）。
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

        // =====================================================================
        // 写通路（write-through · 单 beat · 只更新目标 lane）
        // =====================================================================
        // ⚠️ 写命中时 MUST 同步更新 line 的**目标 lane**（其余 lane 保持）
        //    ⇒ read-modify-write（`aread` 0 拍 + 4 路常量 lsb 的 `bits_update`）
        auto w_lane_sel = lane_sel;
        auto cur_line_w0 = data_mem_way0.aread(index);
        auto cur_line_w1 = data_mem_way1.aread(index);
        auto merge_lane = [&](const auto& cur) {
            return select(
                w_lane_sel == ch_uint<2>(0_d), bits_update<32>(cur, io().wdata, 0),
                select(w_lane_sel == ch_uint<2>(1_d), bits_update<32>(cur, io().wdata, 32),
                       select(w_lane_sel == ch_uint<2>(2_d), bits_update<32>(cur, io().wdata, 64),
                              bits_update<32>(cur, io().wdata, 96))));
        };
        ch_bool do_write = hit & io().we & io().req;
        data_mem_way0.write(index, merge_lane(cur_line_w0), do_write & hit_way0);
        data_mem_way1.write(index, merge_lane(cur_line_w1), do_write & hit_way1);

        // AXI 写（单 beat）
        io().aw_addr  <<= (io().addr & ch_uint<32>(0xFFFFFFF0_h));
        io().aw_len   <<= ch_uint<4>(0_d);   // 1 beat
        io().aw_size  <<= ch_uint<3>(2_d);
        io().aw_burst <<= ch_uint<2>(1_d);
        io().aw_valid <<= io().we & io().req;
        io().w_data   <<= io().wdata;
        // byte lane 使能：按 addr[1:0]（`we` 是整字写 ⇒ 4 个 lane 全 1）
        io().w_strb   <<= ch_uint<4>(0xF_h);
        io().w_last   <<= ch_bool(true);
        io().w_valid  <<= io().we & io().req;
        io().b_ready  <<= ch_bool(true);

        // =====================================================================
        // 输出：rdata / ready
        // =====================================================================
        // ⚠️ fill 完成那拍：**直接用锁存的目标 lane 数据**（免再读 mem）
        io().rdata <<= select(hit, hit_word,
                              select(fill_done, fill_target, ch_uint<32>(0_d)));
        // ready：读 ⇒ hit 或 fill_done；写 ⇒ 由 AXI 握手决定（`aw_ready & w_ready`）
        io().ready <<= select(is_read,
                              select(hit, ch_bool(true), fill_done),
                              io().aw_ready & io().w_ready);
    }

private:
};

} // namespace chlib
