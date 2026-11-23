/**
 * @file d_cache_sync.h
 * @brief 4KB 数据缓存（AXI4 full 主口 · 1-cycle hit）— RV32I L1 dmem 默认实现
 *
 * 规格:
 * - 容量 4KB · **2-way** set associative · line **16 B（4 word）** · 128 sets
 * - 写策略：**Write-Through**（AXI 单 beat 写）
 * - 读 miss：**4-beat burst fill**（`ARLEN=3` / `ARSIZE=2` / `ARBURST=INCR`）
 * - 读延迟：**hit = 1 拍**（`ready` 与请求**同拍**；`rdata` **下一拍**可用并**保持**）
 *
 * 地址格式 (32-bit)：`[31:11]` Tag(21) · `[10:4]` Index(7) · `[3:2]` Lane(2) · `[1:0]` Byte(2)
 *
 * ## 前端契约（**2026-11-23 · S3 定稿**）
 *
 * | 端口 | 语义 |
 * |---|---|
 * | `req` / `we` / `addr` | 请求。⚠️ **`req` MUST 保持到 `ready=1`**（真握手） |
 * | `ready` | **组合**，对**当前 `addr`** 判定 —— 读 = `hit \| fill_complete`；写 = `aw_fire` |
 * | `miss` | **组合** = `!hit`（对**当前 `addr`**） |
 * | `rdata` | **寄存**：在**完成那拍**捕获、**保持**到下一次读完成 |
 *
 * ### ⚠️ 为什么 `rdata` MUST 是「保持到下一次完成」的寄存器
 *
 * 回调约束来自**已签批**的 `specs/rv32i-memory-timing`（MEM 反压 Requirement）：
 * 「**MEM/WB 寄存器 MUST 被冻结**」。于是有一条**必然可达**的时序：
 *
 * 1. 拍 N：load `i` 完成（`ready=1`）⇒ ⇒ 沿后 `M2` 装载 `i`；
 * 2. 拍 N+1：`M2` 持有 `i`，其车道选择**组合**消费 `data_read_data`；
 *    但同拍 `EX/MEM` 里的 `i+1` 若**miss** ⇒ ⇒ `mem_stall=1` ⇒ ⇒ **MEM/WB 被冻结**
 *    ⇒ ⇒ `i` 的 load 结果这一拍**装不进** `MEM/WB`；
 * 3. 拍 G（`i+1` 的 fill 完成）：解冻 ⇒ ⇒ `MEM/WB` 才装载 `i`
 *    ⇒ ⇒ 此时 MUST 仍能读到 `i` 的**数据**。
 *
 * ⇒ ⇒ 数据不能是「随 `addr` 变化的组合量」（拍 G 的 `addr` 是 `i+1` 的）
 * ⇒ ⇒ MUST 是**保持到下一次读完成**的寄存器（本实现）。
 *
 * ### ⚠️ `miss` 从「1 拍寄存」改为「组合」的原因
 *
 * 旧实现的 `miss` 来自 `sread`（**1 拍延迟**）⇒ ⇒ `miss` 与当拍 `addr` **错位一拍**
 * ⇒ ⇒ 一旦接进流水线（`data_ready ← ready`），反压就会**判在错误的请求上**。
 * 现在 tag/valid 走 `aread`（组合）⇒ ⇒ `ready` / `miss` 对**当前 `addr`** 成立
 * ⇒ ⇒ 反压与握手都逐请求对齐（连续两条访存也不会错位）。
 *
 * ### ⚠️ 「假 line」已修（2026-11-23）
 *
 * 旧版 fill 是 `zext<128>(axi_rdata)`、读是 `bits<31,0>(cache_data)`
 * ⇒ ⇒ **`offset` 完全不参与数据选择** ⇒ ⇒ 同一 line 的 4 个地址恒读**第一个字**。
 * 新版：`addr[3:2]` 参与**读选择**与**写更新**；4 个 beat **各落一个 lane**。
 *
 * 作者: build-agent（cache-complete-rewrite Session B · ADR-041；S3 重写 2026-11-23）
 */
#pragma once

#include "ch.hpp"
#include "component.h"
#include "chlib/stream.h"
#include "axi4/byte_lane.h"

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
 * @brief D-Cache 同步版（AXI4 full 主口 · 真 line）
 */
class DCacheSync : public ch::Component {
public:
    __io(
        // ---- 前端 ----
        ch_in<ch_uint<32>> addr;
        ch_in<ch_uint<32>> wdata;
        ch_in<ch_bool> req;
        ch_in<ch_bool> we;               // 1=写, 0=读
        // ⚠️ **字节使能**（2026-11-23 · S3）：RV32I 的 `SB`/`SH` MUST 只写选中车道
        //    ⇒ ⇒ 前端 MUST 有它（否则整字写会覆盖同字的其它字节）。
        //    约定与 `DataTCM` / AXI 侧一致：`wdata` 已**按车道对齐**（流水线负责移位）。
        ch_in<ch_uint<4>> strbe;

        ch_out<ch_uint<32>> rdata;       // **寄存**（完成那拍捕获 · 保持）
        ch_out<ch_bool>     ready;       // **组合**（对当前 addr）
        ch_out<ch_bool>     miss;        // **组合**（对当前 addr）

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
        auto index = bits<DCacheConfig::OFFSET_BITS + DCacheConfig::INDEX_BITS - 1,
                           DCacheConfig::OFFSET_BITS>(io().addr);
        auto tag = bits<31, DCacheConfig::OFFSET_BITS + DCacheConfig::INDEX_BITS>(io().addr);
        // lane 选择 = addr[3:2]（16 B line 内的 4 个 word 之一）
        auto lane_sel = bits<3, 2>(io().addr);

        // =====================================================================
        // Hit/Miss（**组合** · aread ⇒ 与本拍 addr 对齐）
        // =====================================================================
        ch_uint<DCacheConfig::TAG_BITS> tag_way0(tag_mem_way0.aread(index).impl());
        ch_uint<DCacheConfig::TAG_BITS> tag_way1(tag_mem_way1.aread(index).impl());
        ch_uint<1> valid_way0_raw(valid_mem_way0.aread(index).impl());
        ch_uint<1> valid_way1_raw(valid_mem_way1.aread(index).impl());
        ch_bool valid_way0 = valid_way0_raw != ch_uint<1>(0_d);
        ch_bool valid_way1 = valid_way1_raw != ch_uint<1>(0_d);

        ch_bool hit_way0 = valid_way0 & (tag_way0 == tag);
        ch_bool hit_way1 = valid_way1 & (tag_way1 == tag);
        ch_bool hit = hit_way0 | hit_way1;
        io().miss <<= !hit;

        // =====================================================================
        // 读数据：按 lane_sel 从 line 里选 32-bit word（**修「假 line」**）
        // =====================================================================
        ch_uint<DCacheConfig::DATA_WIDTH> line_way0(data_mem_way0.aread(index).impl());
        ch_uint<DCacheConfig::DATA_WIDTH> line_way1(data_mem_way1.aread(index).impl());
        auto cache_line = select(hit_way1, line_way1, line_way0);
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
        // 锁存：line 的 index / tag / 目标 way / 目标 lane 的数据
        ch_reg<ch_uint<DCacheConfig::INDEX_BITS>> fill_index{0_d, "fill_index"};
        ch_reg<ch_uint<DCacheConfig::TAG_BITS>>   fill_tag{0_d, "fill_tag"};
        ch_reg<ch_bool>     fill_way1{false, "fill_way1"};
        ch_reg<ch_uint<32>> fill_target{0_d, "fill_target"};

        ch_bool is_read = !io().we;
        ch_bool miss_read = !hit & is_read & io().req;
        ch_bool ar_fire = io().ar_valid & io().ar_ready;

        // AR：向 line 基址发 4-beat INCR burst
        io().ar_valid <<= miss_read & !fill_active;
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

        // 锁存（AR 握手那拍 —— 调用方 MUST 在 `req` 保持期间不改 `addr`）
        fill_index->next = select(ar_fire, index, fill_index);
        fill_tag->next   = select(ar_fire, tag, fill_tag);
        // way 选择 = LRU（优先填 invalid 的 way；都 valid 则按 LRU）
        auto choose_way1 = select(!valid_way0, ch_bool(false),
                                  select(!valid_way1, ch_bool(true),
                                         lru_mem.aread(index) == ch_bool(false)));
        fill_way1->next  = select(ar_fire, choose_way1, fill_way1);

        // ---- 写回 line（4 beat 拼成 128 bit，一次整行写）----
        // ⚠️ **off-by-one 陷阱**（实测踩中）：`fill_complete` 那一拍，第 4 个 beat 的
        //    `->next` 才在**同一沿**上装载 ⇒ ⇒ 此时读 `beat3` 仍是**旧值**（滞后 1 拍）
        //    ⇒ ⇒ 直接用它拼整行会 **lane 3 恒 0** ⇒ ⇒ 写回那拍 MUST 用**当拍 `r_data`**。
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
        auto lane_mask = axi4::detail::byte_lane_mask<32>(io().strbe);
        auto merge_word = [&](const auto& old_word) {
            return axi4::detail::byte_lane_apply<32, 32>(old_word, io().wdata, lane_mask);
        };
        auto merge_lane = [&](const auto& cur) {
            return select(
                lane_sel == ch_uint<2>(0_d),
                bits_update<32>(cur, merge_word(bits<31, 0>(cur)), 0),
                select(lane_sel == ch_uint<2>(1_d),
                       bits_update<32>(cur, merge_word(bits<63, 32>(cur)), 32),
                       select(lane_sel == ch_uint<2>(2_d),
                              bits_update<32>(cur, merge_word(bits<95, 64>(cur)), 64),
                              bits_update<32>(cur, merge_word(bits<127, 96>(cur)), 96))));
        };

        ch_bool is_write = io().we & io().req;
        io().aw_valid <<= is_write;
        // ⚠️ 单 beat 写 ⇒ 地址是**目标字**（不是 line 基址）⇒ ⇒ AXI 侧按 `w_strb` 落车道
        io().aw_addr  <<= (io().addr & ch_uint<32>(0xFFFFFFFC_h));
        io().aw_len   <<= ch_uint<4>(0_d);   // 1 beat
        io().aw_size  <<= ch_uint<3>(2_d);
        io().aw_burst <<= ch_uint<2>(1_d);   // INCR
        ch_bool aw_fire = io().aw_valid & io().aw_ready;

        // W：AW 握手时锁存数据，保持到 W 握手（AXI 的 AW/W 可不同拍）
        ch_reg<ch_bool>     w_hold{false, "w_hold"};
        ch_reg<ch_uint<32>> w_data_lat{0_d, "w_data_lat"};
        ch_reg<ch_uint<4>>  w_strb_lat{0_d, "w_strb_lat"};
        io().w_valid <<= w_hold;
        io().w_data  <<= w_data_lat;
        io().w_strb  <<= w_strb_lat;
        io().w_last  <<= ch_bool(true);
        ch_bool w_fire = io().w_valid & io().w_ready;
        w_hold->next     = select(aw_fire, ch_bool(true),
                                  select(w_fire, ch_bool(false), w_hold));
        w_data_lat->next = select(aw_fire, io().wdata, w_data_lat);
        w_strb_lat->next = select(aw_fire, io().strbe, w_strb_lat);

        ch_bool do_write = aw_fire & hit;
        data_mem_way0.write(index, merge_lane(line_way0), do_write & hit_way0);
        data_mem_way1.write(index, merge_lane(line_way1), do_write & hit_way1);

        io().b_ready <<= ch_bool(true);

        // =====================================================================
        // 输出：ready / rdata
        // =====================================================================
        // `ready`：**组合**、对**当前 addr** —— 读 = 命中或 fill 完成；写 = AW 握手
        io().ready <<= select(is_read,
                              (hit | fill_complete),
                              aw_fire);

        // `rdata`：**寄存并保持**（理由见文件头）
        // ⚠️ fill 完成那拍用 `target_now`（目标 lane 可能就是最后一个 beat）
        ch_reg<ch_uint<32>> rdata_reg{0_d, "rdata_reg"};
        rdata_reg->next = select(fill_complete, target_now,
                                 select(io().req & is_read & hit, hit_word, rdata_reg));
        io().rdata <<= rdata_reg;
    }
};

} // namespace chlib
