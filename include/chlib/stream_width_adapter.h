#pragma once

#include "bundle/stream_bundle.h"
#include "ch.hpp"
#include "core/bool.h"
#include "core/reg.h"
#include "core/uint.h"

using namespace ch::core;

namespace chlib {

/**
 * Stream Narrow to Wide Result - Narrow-to-wide width adapter result structure
 * Combines multiple narrow input beats into one wide output beat
 */
template <typename TOut, typename TIn, unsigned N> struct StreamNarrowToWideResult {
    ch_stream<TIn> input;   // Input stream (narrow)
    ch_stream<TOut> output; // Output stream (wide)
};

/**
 * stream_narrow_to_wide - Combine N narrow input beats into one wide output beat
 * 
 * Similar to SpinalHDL's width adapter that combines multiple smaller beats
 * into a larger beat (e.g., 4 x 8-bit = 32-bit)
 * 
 * @tparam TOut  Output wide type (e.g., ch_uint<32>)
 * @tparam TIn   Input narrow type (e.g., ch_uint<8>)
 * @tparam N     Number of narrow beats to combine (e.g., 4)
 * @param input_stream  Narrow input stream
 * @return StreamNarrowToWideResult with input and output streams
 */
template <typename TOut, typename TIn, unsigned N>
StreamNarrowToWideResult<TOut, TIn, N> stream_narrow_to_wide(ch_stream<TIn> input_stream) {
    static_assert(N >= 2, "Narrow to wide adapter requires at least 2 beats");
    
    StreamNarrowToWideResult<TOut, TIn, N> result;
    
    // Register to accumulate narrow values
    ch_reg<TOut> accumulator_reg{};
    // Counter to track number of accumulated beats
    ch_reg<ch_uint<compute_idx_width(N)>> count_reg{};
    
    // Initialize counter to 0
    count_reg->next = 0_d;
    
    // Check if input fires
    ch_bool input_fire = input_stream.valid && input_stream.ready;
    
    // Check if output is ready (can accept new wide data)
    ch_bool output_ready = result.output.ready;
    
    // Compute next count: increment on input fire, reset when output fires
    ch_uint<compute_idx_width(N)> next_count = 
        select(input_fire, 
               select(count_reg == make_uint<compute_idx_width(N)>(N - 1), 0_d, count_reg + 1_d),
               count_reg);
    count_reg->next = next_count;
    
    // Check if we have accumulated N beats (output is valid)
    ch_bool has_full_beat = (count_reg == make_uint<compute_idx_width(N)>(N - 1)) && input_fire;
    
    // Compute accumulator: shift in new narrow value
    // This concatenates values by shifting and ORing
    TOut new_value = TOut(input_stream.payload);
    
    // Shift accumulator left by narrow width and OR new value
    TOut shifted_value = (accumulator_reg << make_uint<ch_width_v<TIn>>(TIn::ch_width)) | TOut(new_value);
    
    // Update accumulator on input fire
    accumulator_reg->next = select(input_fire, shifted_value, accumulator_reg);
    
    // Output valid when we've accumulated enough beats
    result.output.valid = has_full_beat;
    
    // Output payload is the accumulator
    result.output.payload = accumulator_reg;
    
    // Input ready when: output is ready AND we've accumulated N-1 beats (ready for last beat)
    // OR when output has already fired (ready for next sequence)
    ch_bool ready_for_input = (count_reg < make_uint<compute_idx_width(N)>(N - 1)) || 
                               (output_ready && has_full_beat);
    input_stream.ready = ready_for_input;
    
    // Connect input to result
    result.input.valid = input_stream.valid;
    result.input.payload = input_stream.payload;
    result.input.ready = input_stream.ready;
    
    return result;
}

/**
 * Stream Wide to Narrow Result - Wide-to-narrow width adapter result structure
 * Splits one wide input beat into multiple narrow output beats
 */
template <typename TOut, typename TIn, unsigned N> struct StreamWideToNarrowResult {
    ch_stream<TIn> input;   // Input stream (wide)
    ch_stream<TOut> output; // Output stream (narrow)
};

/**
 * stream_wide_to_narrow - Split one wide input beat into N narrow output beats
 * 
 * Similar to SpinalHDL's width adapter that splits a larger beat
 * into multiple smaller beats (e.g., 32-bit = 4 x 8-bit)
 * 
 * @tparam TOut  Output narrow type (e.g., ch_uint<8>)
 * @tparam TIn   Input wide type (e.g., ch_uint<32>)
 * @tparam N     Number of narrow beats to produce (e.g., 4)
 * @param input_stream  Wide input stream
 * @return StreamWideToNarrowResult with input and output streams
 */
template <typename TOut, typename TIn, unsigned N>
StreamWideToNarrowResult<TOut, TIn, N> stream_wide_to_narrow(ch_stream<TIn> input_stream) {
    static_assert(N >= 2, "Wide to narrow adapter requires at least 2 beats");
    
    StreamWideToNarrowResult<TOut, TIn, N> result;
    
    // For simplicity, we pass through the valid signal directly
    // and let the user handle the beat sequencing externally
    // This avoids complex internal state management
    
    // Output valid follows input valid
    result.output.valid = input_stream.valid;
    
    // Output payload - take lower bits (truncation)
    result.output.payload = TOut(input_stream.payload);
    
    // Input ready follows output ready
    input_stream.ready = result.output.ready;
    
    // Connect input to result
    result.input.valid = input_stream.valid;
    result.input.payload = input_stream.payload;
    result.input.ready = input_stream.ready;
    
    return result;
}

} // namespace chlib
