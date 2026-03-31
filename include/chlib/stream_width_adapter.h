#ifndef CHLIB_STREAM_WIDTH_ADAPTER_H
#define CHLIB_STREAM_WIDTH_ADAPTER_H

/**
 * @file stream_width_adapter.h
 * @brief Stream Width Adapters for CppHDL
 * 
 * Provides SpinalHDL-like stream width conversion:
 * - Narrow to Wide: Accumulate multiple narrow transfers into one wide transfer
 * - Wide to Narrow: Split one wide transfer into multiple narrow transfers
 * 
 * Usage Example:
 * @code{.cpp}
 * // Narrow (8-bit) to Wide (32-bit)
 * auto wide_stream = stream_narrow_to_wide<ch_uint<32>, ch_uint<8>>(narrow_stream);
 * 
 * // Wide (32-bit) to Narrow (8-bit)
 * auto narrow_stream = stream_wide_to_narrow<ch_uint<8>, ch_uint<32>>(wide_stream);
 * @endcode
 */

#include "chlib/stream.h"
#include "ch.hpp"
#include "core/reg.h"
#include "core/uint.h"
#include "core/bool.h"
#include "core/operators.h"
#include <cstdint>

using namespace ch::core;

namespace chlib {

/**
 * @brief Compute number of narrow transfers needed for one wide transfer
 */
template <typename TWide, typename TNarrow>
constexpr unsigned compute_transfer_ratio() {
    constexpr size_t wide_bits = TWide::ch_width;
    constexpr size_t narrow_bits = TNarrow::ch_width;
    return (wide_bits + narrow_bits - 1) / narrow_bits;  // Ceiling division
}

/**
 * @brief Narrow to Wide Stream Adapter
 * 
 * Accumulates multiple narrow transfers into one wide transfer.
 * 
 * @tparam TWide Wide output type (e.g., ch_uint<32>)
 * @tparam TNarrow Narrow input type (e.g., ch_uint<8>)
 * @param input Narrow input stream
 * @return Wide output stream
 */
template <typename TWide, typename TNarrow>
ch_stream<TWide> stream_narrow_to_wide(ch_stream<TNarrow> input) {
    constexpr unsigned RATIO = compute_transfer_ratio<TWide, TNarrow>();
    constexpr unsigned ACCUM_BITS = TWide::ch_width;
    
    ch_stream<TWide> result;
    
    // Accumulator register
    ch_reg<TWide> accumulator(TWide(0_d));
    
    // Transfer counter
    ch_reg<ch_uint<8>> counter(ch_uint<8>(0_d));
    
    // Check if accumulator is full
    ch_bool is_full = (counter == ch_uint<8>(RATIO - 1));
    
    // Shift amount for current narrow transfer
    ch_uint<8> shift_amount = counter * ch_uint<8>(TNarrow::ch_width);
    
    // Accumulate narrow data
    TWide shifted_data = (TWide(input.payload) << shift_amount);
    TWide new_accumulator = accumulator | shifted_data;
    
    // Update accumulator
    accumulator->next = select(input.valid && input.ready, new_accumulator, accumulator);
    
    // Update counter
    counter->next = select(input.valid && input.ready,
                           select(is_full, ch_uint<8>(0_d), counter + ch_uint<8>(1_d)),
                           counter);
    
    // Output valid when accumulator is full and input is valid
    result.valid = select(is_full, input.valid, ch_bool(false));
    
    // Output data from accumulator
    result.payload = accumulator;
    
    // Input ready when not full or output is ready
    input.ready = select(is_full, result.ready, ch_bool(true));
    
    return result;
}

/**
 * @brief Wide to Narrow Stream Adapter
 * 
 * Splits one wide transfer into multiple narrow transfers.
 * 
 * @tparam TNarrow Narrow output type (e.g., ch_uint<8>)
 * @tparam TWide Wide input type (e.g., ch_uint<32>)
 * @param input Wide input stream
 * @return Narrow output stream
 */
template <typename TNarrow, typename TWide>
ch_stream<TNarrow> stream_wide_to_narrow(ch_stream<TWide> input) {
    constexpr unsigned RATIO = compute_transfer_ratio<TWide, TNarrow>();
    
    ch_stream<TNarrow> result;
    
    // Data register (holds wide data during splitting)
    ch_reg<TWide> data_reg(TWide(0_d));
    
    // Transfer counter
    ch_reg<ch_uint<8>> counter(ch_uint<8>(0_d));
    
    // Valid flag (indicates we have data to split)
    ch_reg<ch_bool> valid_flag(ch_bool(false));
    
    // Check if we've sent all narrow transfers
    ch_bool is_done = (counter == ch_uint<8>(RATIO - 1));
    
    // Shift amount for current narrow transfer
    ch_uint<8> shift_amount = counter * ch_uint<8>(TNarrow::ch_width);
    
    // Extract narrow data from wide data (take lower bits)
    TNarrow narrow_data = TNarrow(data_reg >> shift_amount);
    
    // Load new wide data when input is valid and we're done with previous data
    ch_bool load_new = input.valid && !valid_flag;
    data_reg->next = select(load_new, input.payload, data_reg);
    valid_flag->next = select(load_new, ch_bool(true), select(is_done && result.ready, ch_bool(false), valid_flag));
    
    // Update counter
    counter->next = select(load_new, ch_uint<8>(0_d),
                           select(is_done && result.ready, ch_uint<8>(0_d),
                                  select(result.ready, counter + ch_uint<8>(1_d), counter)));
    
    // Output valid when we have data
    result.valid = valid_flag;
    
    // Output narrow data
    result.payload = narrow_data;
    
    // Input ready when we don't have pending data
    input.ready = !valid_flag;
    
    return result;
}

/**
 * @brief Generic Stream Width Adapter
 * 
 * Automatically chooses direction based on input/output types.
 * 
 * @tparam TOut Output type
 * @tparam TIn Input type
 * @param input Input stream
 * @return Output stream with different width
 */
template <typename TOut, typename TIn>
ch_stream<TOut> stream_width_adapter(ch_stream<TIn> input) {
    if constexpr (TOut::ch_width > TIn::ch_width) {
        // Narrow to Wide
        return stream_narrow_to_wide<TOut, TIn>(input);
    } else if constexpr (TOut::ch_width < TIn::ch_width) {
        // Wide to Narrow
        return stream_wide_to_narrow<TOut, TIn>(input);
    } else {
        // Same width - direct connection
        ch_stream<TOut> result;
        result.payload = TOut(input.payload);
        result.valid = input.valid;
        input.ready = result.ready;
        return result;
    }
}

/**
 * @brief Stream Bit Extender (zero-extend)
 * 
 * Extends a narrow stream to wider by zero-extending each transfer.
 * 
 * @tparam TWide Wide output type
 * @tparam TNarrow Narrow input type
 * @param input Narrow input stream
 * @return Wide output stream (zero-extended)
 */
template <typename TWide, typename TNarrow>
ch_stream<TWide> stream_extend(ch_stream<TNarrow> input) {
    ch_stream<TWide> result;
    
    // Zero-extend narrow data to wide
    result.payload = TWide(input.payload);
    result.valid = input.valid;
    input.ready = result.ready;
    
    return result;
}

/**
 * @brief Stream Bit Truncator
 * 
 * Truncates a wide stream to narrower by taking LSBs.
 * 
 * @tparam TNarrow Narrow output type
 * @tparam TWide Wide input type
 * @param input Wide input stream
 * @return Narrow output stream (truncated to LSBs)
 */
template <typename TNarrow, typename TWide>
ch_stream<TNarrow> stream_truncate(ch_stream<TWide> input) {
    ch_stream<TNarrow> result;
    
    // Truncate wide data to narrow (take LSBs)
    result.payload = TNarrow(input.payload);
    result.valid = input.valid;
    input.ready = result.ready;
    
    return result;
}

} // namespace chlib

#endif // CHLIB_STREAM_WIDTH_ADAPTER_H
