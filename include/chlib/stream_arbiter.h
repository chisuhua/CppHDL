// include/chlib/stream_arbiter.h
// Additional stream arbiter strategies: locking and sequential

#ifndef CHLIB_STREAM_ARBITER_H
#define CHLIB_STREAM_ARBITER_H

#include "bundle/stream_bundle.h"
#include "ch.hpp"
#include "chlib/combinational.h"
#include "chlib/selector_arbiter.h"
#include "component.h"
#include "core/bool.h"
#include "core/literal.h"
#include "core/operators.h"
#include "core/uint.h"

using namespace ch::core;

namespace chlib {

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
        current_locked_valid = current_locked_valid || (is_locked && input_streams[i].valid);
    }

    // Determine if we should keep the lock or grab a new one
    ch_bool transaction_complete = result.output_stream.ready;
    ch_bool should_keep_lock = current_locked_valid && !transaction_complete;

    // Select the channel: keep current if locked, otherwise use priority encoder
    ch_uint<compute_idx_width(N_INPUTS)> selected_idx;
    if (should_keep_lock) {
        selected_idx = locked_channel;
    } else {
        selected_idx = priority_encoder<N_INPUTS>(valid_vector);
        ch_bool has_valid = valid_vector != 0_d;
        locked_channel = select(has_valid, selected_idx, locked_channel);
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
    for (unsigned i = 0; i < N_INPUTS; i++) {
        ch_bool is_selected = (selected_idx == make_uint<compute_idx_width(N_INPUTS)>(i));
        result.input_streams[i].ready = is_selected && result.output_stream.ready;
    }

    result.selected = selected_idx;
    result.output_stream.ready = true;

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
    for (unsigned i = 0; i < N_INPUTS; i++) {
        ch_bool is_selected = (selected_idx == make_uint<compute_idx_width(N_INPUTS)>(i));
        result.input_streams[i].ready = is_selected && result.output_stream.ready && has_valid;
    }

    result.selected = selected_idx;
    result.output_stream.ready = true;

    return result;
}

}  // namespace chlib

#endif  // CHLIB_STREAM_ARBITER_H
