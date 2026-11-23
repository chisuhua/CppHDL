// examples/riscv-soc/src/soc_trace_counter.h
//
// SoC 级**观测 IP**（ADR-077 D1/D2 · change `riscv-soc-skeleton-2026-11`）。
//
// 职责：把核复合体（`Rv32iCoreTop`）的两个**事件**各自累计成 16 位计数，
// 供 SoC 根作为**根自有**输出上抬（ADR-072 D1：后代 `ch_out` 不得直接成为顶层端口）。
//
// ⚠️ **为什么需要两根计数**（签批 spec `rtl-soc-output` 的硬要求）：
//   SoC 交付物的观测集 MUST 含 **≥ 2 个**随激励变化的量 ⇒ ⇒ 单计数会不合规。
//
// ⚠️ **两个事件源必须是单拍脉冲**（否则计数无法手推）：
//   · `store_event`  ← `Rv32iCoreTop::obs_data_write_en`（= `exmem_is_store`）
//   · `retire_event` ← `Rv32iCoreTop::dbg_wb_write_en`（= `wb.write_en`
//     = `needs_writeback & rd_not_zero & mem_valid` —— `mem_valid` 限定已在内
//     ⇒ 气泡拍不会重复计，见 `include/cpu/pipeline/stages/wb_stage.h:108-110`）
//
// ⚠️ **手推值因程序而异**（本 change 的 7 字程序 ⇒ `store_count == 1` · `writeback_count == 3`）：
//   两值**必须不同**，否则事件源接反时观测值不变 ⇒ 判据恒绿。

#pragma once

#include "ch.hpp"
#include "component.h"

#include <string>

namespace riscv_soc {

using namespace ch::core;

/// 两个事件的饱和计数器（16 位 ⇒ 60 拍窗口内实际不会饱和；饱和只为消除回绕 UB）。
class SocTraceCounter : public ch::Component {
public:
    __io(
        ch_in<ch_bool>      store_event{"store_event"};
        ch_in<ch_bool>      retire_event{"retire_event"};
        ch_out<ch_uint<16>> store_count{"store_count"};
        ch_out<ch_uint<16>> writeback_count{"writeback_count"};
    )

    explicit SocTraceCounter(ch::Component *parent = nullptr,
                             const std::string &name = "soc_trace_counter")
        : ch::Component(parent, name) {}

    void create_ports() override { new (io_storage_) io_type; }

    void describe() override {
        ch_reg<ch_uint<16>> store_cnt(0_d);
        ch_reg<ch_uint<16>> retire_cnt(0_d);

        const auto kMax = ch_uint<16>(0xFFFF_h);
        const auto kOne = ch_uint<16>(1_d);

        store_cnt->next = select(io().store_event,
                                 select(store_cnt == kMax, store_cnt, store_cnt + kOne),
                                 store_cnt);
        retire_cnt->next = select(io().retire_event,
                                  select(retire_cnt == kMax, retire_cnt, retire_cnt + kOne),
                                  retire_cnt);

        io().store_count = store_cnt;
        io().writeback_count = retire_cnt;
    }
};

} // namespace riscv_soc
