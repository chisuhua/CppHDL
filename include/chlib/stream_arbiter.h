// include/chlib/stream_arbiter.h
// Additional stream arbiter strategy: locking

#pragma once

#include "bundle/stream_bundle.h"
#include "ch.hpp"
#include "chlib/combinational.h"
#include "chlib/selector_arbiter.h"
#include "component.h"
#include "core/bool.h"
#include "core/literal.h"
#include "core/operators.h"
#include "core/uint.h"

namespace chlib {

using namespace ch::core;

/**
 * Stream Arbiter Lock Result - 锁定仲裁器结果
 * Once selected, stays locked until transaction completes
 */
template <typename T, unsigned N_INPUTS> struct StreamArbiterLockResult {
    std::array<ch_stream<T>, N_INPUTS> input_streams;
    ch_stream<T> output_stream;
    ch_uint<compute_idx_width(N_INPUTS)> selected; // 选择的输入索引
};

/**
 * Stream Arbiter Lock - 锁定仲裁器
 * Once an input is selected and valid, it stays selected until the transaction completes
 * (i.e., output.ready is asserted). This prevents starvation of the locked channel.
 *
 * ⚠️⚠️ **ADR-065 D3 / D4 修复（2026-11-03）**
 *
 * 修复前选择逻辑写成**普通 C++ `if / else if / else`**：
 *
 *     if (current_locked_valid) { selected_idx = locked_channel; }
 *     else if (has_valid)       { selected_idx = priority_encoder(...);
 *                                 locked_channel = selected_idx; }   // ← C++ 赋值
 *     else                      { selected_idx = locked_channel; }
 *
 * `ch_bool` 的 `operator uint64_t()` 只对 **const 节点**返回真值
 * （`src/lnode/bool.cpp:43-49`）⇒ ⇒ 两个条件在**建图期恒假** ⇒ ⇒ **恒走 `else`**
 * ⇒ ⇒ `selected_idx` 恒为 `locked_channel` 的初值 **0** ⇒ ⇒ **恒选通道 0**。
 * 更糟：`locked_channel = selected_idx;` 是对 `ch_reg` 的 **C++ 赋值**（既非
 * `->next`、也非数据流边）⇒ ⇒ **锁存永不生效** ⇒ ⇒ 头注释承诺的「stays selected
 * until the transaction completes」**完全没有实现**。
 * 实测（唯一有效 = 通道 1，payload `0x22`）：`selected=0` 且 `out.payload=0x11`
 * ⇒ ⇒ **不只选错通道，还输出了另一个通道的数据**（静默数据损坏）。
 *
 * ⇒ 现在：选择用 `select` 链（硬件原语）、锁存走 `locked_channel->next`，
 *   并新增 `locked` 寄存器把「锁到事务完成」真正实现（见 §D4 的状态表）。
 */
template <typename T, unsigned N_INPUTS>
StreamArbiterLockResult<T, N_INPUTS>
stream_arbiter_lock(std::array<ch_stream<T>, N_INPUTS> input_streams) {
    static_assert(N_INPUTS >= 2, "Lock arbiter requires at least 2 inputs");
    using IdxT = ch_uint<compute_idx_width(N_INPUTS)>;

    StreamArbiterLockResult<T, N_INPUTS> result;

    // ── 状态 ────────────────────────────────────────────────────────────
    // `locked`：是否已锁定（授予时置位，`output.fire` 时释放）
    // `locked_channel`：被锁定的通道索引
    ch_reg<ch_bool> locked(ch_bool(false));
    ch_reg<IdxT> locked_channel(IdxT(0_d));

    // Initialize input streams
    for (unsigned i = 0; i < N_INPUTS; i++) {
        result.input_streams[i] = input_streams[i];
    }

    // Create valid vector
    ch_uint<N_INPUTS> valid_vector = 0_d;
    for (unsigned i = 0; i < N_INPUTS; i++) {
        ch_bool is_valid = input_streams[i].valid;
        ch_uint<N_INPUTS> mask = ch_uint<N_INPUTS>(1_d) << make_uint<compute_idx_width(N_INPUTS)>(i);
        valid_vector = select(is_valid, valid_vector | mask, valid_vector);
    }

    ch_bool has_valid = valid_vector != 0_d;
    IdxT prio_sel = priority_encoder<N_INPUTS>(valid_vector);

    // ⚠️ 读 `ch_reg` MUST 用**隐式转换赋值**（与 `round_robin_arbiter` 的
    //    `ch_uint<N> priority_ptr = ptr_reg;` 同形）⇒ MUST NOT 写
    //    `ch_bool(locked)` / `IdxT(locked_channel)`（实测：那会走**另一条**
    //    构造路径，锁存恒读 0 ⇒ 表现为「锁永不生效」）。
    ch_bool locked_now = locked;
    IdxT locked_idx = locked_channel;

    // ── 选择（ADR-065 D3：硬件 `select` 链，MUST NOT 用 C++ `if`）─────────
    // 已锁定 ⇒ 保持 `locked_channel`（**不管**其它通道是否更优先）
    // 未锁定 & 有请求 ⇒ 取仲裁结果
    // 未锁定 & 无请求 ⇒ 保持 `locked_channel`（输出 valid=0，不影响下游）
    IdxT selected_idx = select(locked_now, locked_idx,
                               select(has_valid, prio_sel, locked_idx));

    // Select payload using mux
    std::array<T, N_INPUTS> payloads;
    for (unsigned i = 0; i < N_INPUTS; i++) {
        payloads[i] = input_streams[i].payload;
    }
    T selected_payload = mux<N_INPUTS>(payloads, selected_idx);

    // ⚠️ `valid` MUST 是**选中通道的** valid，MUST NOT 是「任一通道 valid」
    //    （ADR-065 §1.3 的第三个缺陷）：否则锁定通道撤销 valid 而其它通道有效时
    //    `valid=1` 却输出别的通道的数据 ⇒ 数据/valid 不匹配。
    ch_bool selected_valid = bit_select(valid_vector, selected_idx);

    // Output
    result.output_stream.payload = selected_payload;
    result.output_stream.valid = selected_valid;

    // Only the selected input is ready
    //
    // ⚠️⚠️ **ADR-054 形态 E（默认值 + 可覆盖）· P1-a**
    //
    // ⚠️ **核心修复**：删掉了 `result.output_stream.ready = true;`（**硬编码**
    //   「输出恒就绪」⇒ ⇒ ⇒ **无条件忽略消费端的 ready** ⇒ ⇒ ⇒ ⇒ **违反**
    //   主 spec `chlib-stream-ready-ownership` 的 R1 ⇒ ⇒ ⇒ ⇒ 且无护栏检出）
    //   ⇒ ⇒ ⇒ ⇒ ⇒ **现状 = 「上游恒不 ready = 死路」** ⇒ ⇒ ⇒ ⇒ ⇒
    //   ⇒ **修复不是行为变更 ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ 零破坏**成立。
    //
    // ⚠️ **同时修掉「先读后写」**：旧代码读 `result.output_stream.ready`（**空
    //   proxy**）⇒ ⇒ 与之做 `&` ⇒ ⇒ 报 "Invalid operands"（**静默**）
    //   ⇒ ⇒ ⇒ 现改为「**先**建默认驱动（使该字段有节点）**再**读它」。
    ch_bool default_ready(true);                  // 局部 literal 源（可被覆盖）
    result.output_stream.ready <<= default_ready; // ② 默认恒就绪（= 既有旧语义）

    // ── 锁存更新（ADR-065 D4：锁到「事务完成」= `output.fire`）────────────
    // ⚠️ 时序：`fire` 用的是**本拍**的 `valid` × 消费端 `ready`
    //    （`ready` 的最终 driver 由调用方在返回后 `<<=` 覆盖 ⇒ 语义正确）。
    ch_bool out_fire = selected_valid & result.output_stream.ready;
    ch_bool grant_lock = !locked_now & has_valid; // 未锁定 & 有请求 ⇒ 锁新通道

    locked_channel->next = select(grant_lock, prio_sel, locked_idx);
    locked->next = select(out_fire, ch_bool(false),
                          select(grant_lock, ch_bool(true), locked_now));

    for (unsigned i = 0; i < N_INPUTS; i++) {
        ch_bool is_selected = (selected_idx == make_uint<compute_idx_width(N_INPUTS)>(i));
        // ⚠️ **MUST 用 `<<=`**（MUST NOT `operator=`）⇒ ⇒ ⇒ **调用方真收到
        // 反压**（`ch_stream` 的**拷贝与原对象共享同一 ready 节点** ⇒ ⇒ ⇒
        // `<<=` 能**穿透副本**驱动到**调用方**）
        result.input_streams[i].ready <<=
            (is_selected & result.output_stream.ready);
    }

    result.selected = selected_idx;

    return result;
}

}  // namespace chlib

