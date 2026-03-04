#pragma once

#include "bundle/stream_bundle.h"
#include "ch.hpp"
#include "core/bool.h"
#include "core/reg.h"

using namespace ch::core;

namespace chlib {

/**
 * Stream M2S Pipe Result - Master-to-Slave pipeline result structure
 * Creates a 1-cycle delay pipeline for valid and payload signals
 */
template <typename T> struct StreamM2SPipeResult {
    ch_stream<T> input;
    ch_stream<T> output;
};

/**
 * stream_m2s_pipe - Create a master-to-slave pipeline with 1-cycle delay
 * 
 * Similar to SpinalHDL's m2sPipe() - adds a register stage between master and slave
 * Both valid and payload are delayed by 1 cycle when input.fire() occurs
 */
template <typename T>
StreamM2SPipeResult<T> stream_m2s_pipe(ch_stream<T> input_stream) {
    StreamM2SPipeResult<T> result;
    
    ch_reg<T> payload_reg{};
    ch_reg<ch_bool> valid_reg{};
    
    ch_bool input_fire = input_stream.valid && input_stream.ready;
    
    payload_reg->next = ch::core::select(input_fire, input_stream.payload, payload_reg);
    valid_reg->next = input_fire;
    
    result.output.valid = valid_reg;
    result.output.payload = payload_reg;
    
    result.input.ready = result.output.ready;
    result.input.valid = input_stream.valid;
    result.input.payload = input_stream.payload;
    
    input_stream.ready = result.input.ready;
    
    return result;
}

/**
 * Stream S2M Pipe Result - Slave-to-Master pipeline result structure
 * Creates a 0-cycle delay pipeline for payload, 1-cycle delay for ready signal
 */
template <typename T> struct StreamS2MPipeResult {
    ch_stream<T> input;
    ch_stream<T> output;
};

/**
 * stream_s2m_pipe - Create a slave-to-master pipeline with 0-cycle payload delay
 * 
 * Similar to SpinalHDL's s2mPipe() - adds a register stage for ready signal only
 * Payload passes through immediately (0-cycle delay), ready is delayed by 1 cycle
 */
template <typename T>
StreamS2MPipeResult<T> stream_s2m_pipe(ch_stream<T> input_stream) {
    StreamS2MPipeResult<T> result;
    
    ch_reg<ch_bool> ready_reg{};
    
    // Ready signal is delayed by 1 cycle
    ready_reg->next = input_stream.ready;
    
    // Payload passes through immediately (0-cycle delay)
    result.output.payload = input_stream.payload;
    result.output.valid = input_stream.valid;
    result.output.ready = ready_reg;
    
    // Input ready comes from registered output ready
    result.input.ready = result.output.ready;
    result.input.valid = input_stream.valid;
    result.input.payload = input_stream.payload;
    
    input_stream.ready = result.input.ready;
    
    return result;
}

/**
 * Stream Half Pipe Result - Half-bandwidth pipeline result structure
 * All signals are registered, bandwidth is halved
 */
template <typename T> struct StreamHalfPipeResult {
    ch_stream<T> input;
    ch_stream<T> output;
};

/**
 * stream_half_pipe - Create a half-bandwidth pipeline with all signals registered
 * 
 * Similar to SpinalHDL's halfPipe() - all signals (valid, payload, ready) are registered
 * Bandwidth is halved by using a toggle mechanism that only accepts data every other cycle
 */
template <typename T>
StreamHalfPipeResult<T> stream_half_pipe(ch_stream<T> input_stream) {
    StreamHalfPipeResult<T> result;
    
    ch_reg<T> payload_reg{};
    ch_reg<ch_bool> valid_reg{};
    ch_reg<ch_bool> ready_reg{};
    ch_reg<ch_bool> toggle_reg{};
    
    // Compute input fire condition
    ch_bool input_fire = input_stream.valid && input_stream.ready;
    
    // Toggle signal - flips every time there's a transfer
    // Only accept data when toggle is high (halves bandwidth)
    toggle_reg->next = select(input_fire, !toggle_reg, toggle_reg);
    
    // Only fire when toggle was low (meaning we accept every other cycle)
    ch_bool accept_data = toggle_reg && input_fire;
    
    // Register payload and valid
    payload_reg->next = ch::core::select(accept_data, input_stream.payload, payload_reg);
    valid_reg->next = accept_data;
    
    // Register ready (delayed by 1 cycle from input)
    ready_reg->next = input_stream.ready;
    
    // Connect output
    result.output.valid = valid_reg;
    result.output.payload = payload_reg;
    result.output.ready = ready_reg;
    
    // Connect input
    result.input.ready = result.output.ready;
    result.input.valid = input_stream.valid;
    result.input.payload = input_stream.payload;
    
    input_stream.ready = result.input.ready;
    
    return result;
}


}
