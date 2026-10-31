// include/chlib/stream_arbiter.h
// Additional stream arbiter strategies: locking and sequential

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
 */
template <typename T, unsigned N_INPUTS>
StreamArbiterLockResult<T, N_INPUTS>
stream_arbiter_lock(std::array<ch_stream<T>, N_INPUTS> input_streams) {
    static_assert(N_INPUTS >= 2, "Lock arbiter requires at least 2 inputs");

    StreamArbiterLockResult<T, N_INPUTS> result;

    // Register to track the locked channel
    auto locked_channel = ch_reg<ch_uint<compute_idx_width(N_INPUTS)>>(0_d);

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

    // If currently locked and the locked channel is still valid, keep the lock
    ch_bool current_locked_valid = false;
    for (unsigned i = 0; i < N_INPUTS; i++) {
        ch_bool is_locked = (locked_channel == make_uint<compute_idx_width(N_INPUTS)>(i));
        current_locked_valid = current_locked_valid | (is_locked & input_streams[i].valid);
    }

    // Select the channel: use locked channel if valid, otherwise use priority encoder
    ch_uint<compute_idx_width(N_INPUTS)> selected_idx;
    ch_bool has_valid = valid_vector != 0_d;
    
    if (current_locked_valid) {
        selected_idx = locked_channel;
    } else if (has_valid) {
        selected_idx = priority_encoder<N_INPUTS>(valid_vector);
        locked_channel = selected_idx;
    } else {
        selected_idx = locked_channel;
    }

    // Select payload using mux
    std::array<T, N_INPUTS> payloads;
    for (unsigned i = 0; i < N_INPUTS; i++) {
        payloads[i] = input_streams[i].payload;
    }
    T selected_payload = mux<N_INPUTS>(payloads, selected_idx);

    // Output
    result.output_stream.payload = selected_payload;
    result.output_stream.valid = valid_vector != 0_d;

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

/**
 * Stream Arbiter Sequence Result - 序列仲裁器结果
 * Fixed order arbitration, wraps around
 */
template <typename T, unsigned N_INPUTS> struct StreamArbiterSequenceResult {
    std::array<ch_stream<T>, N_INPUTS> input_streams;
    ch_stream<T> output_stream;
    ch_uint<compute_idx_width(N_INPUTS)> selected; // 选择的输入索引
};

/**
 * Stream Arbiter Sequence - 序列仲裁器
 * Cycles through inputs in fixed order using round-robin selection.
 * This implementation uses priority encoder for simplicity.
 */
template <typename T, unsigned N_INPUTS>
StreamArbiterSequenceResult<T, N_INPUTS>
stream_arbiter_sequence(std::array<ch_stream<T>, N_INPUTS> input_streams) {
    static_assert(N_INPUTS >= 2, "Sequence arbiter requires at least 2 inputs");

    StreamArbiterSequenceResult<T, N_INPUTS> result;

    // Initialize input streams
    for (unsigned i = 0; i < N_INPUTS; i++) {
        result.input_streams[i] = input_streams[i];
    }

    // Create valid vector (one-hot)
    ch_uint<N_INPUTS> valid_vector = 0_d;
    for (unsigned i = 0; i < N_INPUTS; i++) {
        ch_bool is_valid = input_streams[i].valid;
        ch_uint<N_INPUTS> mask = ch_uint<N_INPUTS>(1_d) << make_uint<compute_idx_width(N_INPUTS)>(i);
        valid_vector = select(is_valid, valid_vector | mask, valid_vector);
    }

    // Check if there's any valid request
    ch_bool has_valid = valid_vector != 0_d;

    // Use priority encoder for sequential selection
    constexpr unsigned IDX_WIDTH = compute_idx_width(N_INPUTS);
    ch_uint<IDX_WIDTH> selected_idx = priority_encoder<N_INPUTS>(valid_vector);

    // Select payload using mux
    std::array<T, N_INPUTS> payloads;
    for (unsigned i = 0; i < N_INPUTS; i++) {
        payloads[i] = input_streams[i].payload;
    }
    T selected_payload = mux<N_INPUTS>(payloads, selected_idx);

    // Output
    result.output_stream.payload = selected_payload;
    result.output_stream.valid = has_valid;

    // Only the selected input is ready
    //
    // ⚠️⚠️ **ADR-054 形态 E（默认值 + 可覆盖）· P1-a**（同 `stream_arbiter_lock`
    //   的注释 ⇒ ⇒ 核心修复 = 删硬编码 `= true` + 修「先读后写」）
    ch_bool default_ready(true);
    result.output_stream.ready <<= default_ready;

    for (unsigned i = 0; i < N_INPUTS; i++) {
        ch_bool is_selected = (selected_idx == make_uint<compute_idx_width(N_INPUTS)>(i));
        // ⚠️ **MUST 用 `<<=`**（MUST NOT `operator=`）⇒ ⇒ ⇒ **调用方真收到反压**
        result.input_streams[i].ready <<=
            (is_selected & result.output_stream.ready & has_valid);
    }

    result.selected = selected_idx;
    // ⚠️ `result.output_stream.ready` 已在**上方**建立默认驱动 ⇒ ⇒ 无需再接

    return result;
}

}  // namespace chlib

