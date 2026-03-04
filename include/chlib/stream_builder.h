#pragma once

/**
 * @file stream_builder.h
 * @brief Fluent API builder for ch_stream operations
 * 
 * Provides a chainable interface for stream operations like:
 * - .queue<depth>()    - Add a FIFO buffer (compile-time depth)
 * - .haltWhen(cond)    - Pause stream when condition is true
 * - .map(func)         - Transform payload
 * - .throwWhen(cond)  - Drop data when condition is true
 * - .takeWhen(cond)   - Only pass data when condition is true
 * - .continueWhen(cond) - Continue stream when condition is true
 * 
 * Usage:
 *   auto result = StreamBuilder<ch_uint<8>>(source)
 *       .queue<4>()
 *       .haltWhen(busy)
 *       .map([](const ch_uint<8>& v) { return v + 1; })
 *       .build();
 */

#include "bundle/stream_bundle.h"
#include "chlib/stream.h"
#include <functional>

namespace chlib {

/**
 * @brief Fluent API builder for stream operations
 * 
 * @tparam T The payload type of the stream
 */
template <typename T>
class StreamBuilder {
public:
    /**
     * @brief Construct a builder from a source stream
     * @param source The input stream to build upon
     */
    explicit StreamBuilder(ch_stream<T> source) 
        : current_stream_(source) {}

    /**
     * @brief Add a FIFO queue with specified depth
     * @tparam DEPTH The compile-time depth of the FIFO
     * @return Reference to this builder for chaining
     * 
     * Creates a FIFO buffer that provides backpressure when full.
     * Similar to SpinalHDL's .queue() method.
     * 
     * Note: DEPTH must be a compile-time constant.
     */
    template <unsigned DEPTH>
    StreamBuilder<T>& queue() {
        auto result = stream_fifo<T, DEPTH>(current_stream_);
        current_stream_ = result.pop_stream;
        return *this;
    }

    /**
     * @brief Halt the stream when condition is true
     * @param halt The condition that causes the stream to pause
     * @return Reference to this builder for chaining
     * 
     * When halt is true, valid signal is de-asserted but data is preserved.
     * Similar to SpinalHDL's .haltWhen() method.
     */
    StreamBuilder<T>& haltWhen(ch_bool halt) {
        current_stream_ = stream_halt_when(current_stream_, halt);
        return *this;
    }

    /**
     * @brief Continue stream flow when condition is true
     * @param continue_cond The condition that allows stream to flow
     * @return Reference to this builder for chaining
     * 
     * Convenience method - equivalent to haltWhen(!continue_cond).
     * Similar to SpinalHDL's .continueWhen() method.
     */
    StreamBuilder<T>& continueWhen(ch_bool continue_cond) {
        current_stream_ = stream_continue_when(current_stream_, continue_cond);
        return *this;
    }

    /**
     * @brief Map/transform the stream payload
     * @tparam Func A callable type that transforms T to T
     * @param transform A callable that transforms T to T
     * @return Reference to this builder for chaining
     * 
     * Applies a transformation function to each payload.
     * Similar to SpinalHDL's .map() method.
     * 
     * Note: The transform must return the same type T.
     */
    template <typename Func>
    StreamBuilder<T>& map(Func transform) {
        current_stream_ = stream_translate_with<T, T>(current_stream_, transform);
        return *this;
    }

    /**
     * @brief Throw (drop) data when condition is true
     * @param condition The condition that causes data to be dropped
     * @return Reference to this builder for chaining
     * 
     * When condition is true, data is consumed but not passed through.
     * Similar to SpinalHDL's .throwWhen() method.
     */
    StreamBuilder<T>& throwWhen(ch_bool condition) {
        current_stream_ = stream_throw_when(current_stream_, condition);
        return *this;
    }

    /**
     * @brief Take (pass) data only when condition is true
     * @param condition The condition that allows data to pass
     * @return Reference to this builder for chaining
     * 
     * Only passes data when condition is true, otherwise holds back.
     * Similar to SpinalHDL's .takeWhen() method.
     */
    StreamBuilder<T>& takeWhen(ch_bool condition) {
        current_stream_ = stream_take_when(current_stream_, condition);
        return *this;
    }

    /**
     * @brief Build the final stream from the builder chain
     * @return The final ch_stream<T> after all operations
     */
    ch_stream<T> build() {
        return current_stream_;
    }

private:
    ch_stream<T> current_stream_;
};

/**
 * @brief Helper function to create a StreamBuilder
 * @tparam T The payload type
 * @param source The source stream
 * @return StreamBuilder<T> instance
 * 
 * Usage:
 *   auto result = make_stream_builder(source).queue<4>().map(func).build();
 */
template <typename T>
StreamBuilder<T> make_stream_builder(ch_stream<T> source) {
    return StreamBuilder<T>(source);
}

} // namespace chlib
