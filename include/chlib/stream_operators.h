#pragma once

#include "bundle/stream_bundle.h"
#include "ch.hpp"
#include "chlib/stream_pipeline.h"

namespace ch {

/**
 * @brief Direct stream connection operator (sink <<= source)
 * 
 * Connects source stream to sink stream with 0-cycle delay.
 * This is equivalent to SpinalHDL's direct stream connection.
 * 
 * Connections made:
 *   - sink.valid = source.valid
 *   - sink.payload = source.payload
 *   - source.ready = sink.ready
 * 
 * @param sink The target stream (slave side)
 * @param source The source stream (master side)
 * @return ch_stream<T>& Reference to sink for chaining
 */
template <typename T>
ch_stream<T>& operator<<=(ch_stream<T>& sink, ch_stream<T>& source) {
    // Direct connection: valid and payload flow from source to sink
    // Ready signal flows backward from sink to source
    sink.valid = source.valid;
    sink.payload = source.payload;
    source.ready = sink.ready;
    return sink;
}

/**
 * @brief Direct stream connection operator (source >>= sink)
 * 
 * Reverse direction of <<=. Equivalent to sink <<= source.
 * 
 * @param source The source stream (master side)
 * @param sink The target stream (slave side)
 * @return ch_stream<T>& Reference to source for chaining
 */
template <typename T>
ch_stream<T>& operator>>=(ch_stream<T>& source, ch_stream<T>& sink) {
    // Reverse direction: delegate to <<=
    return sink <<= source;
}

} // namespace ch

// Also provide aliases in chlib for convenience
namespace chlib {
    using ch::operator<<=;
    using ch::operator>>=;
}
