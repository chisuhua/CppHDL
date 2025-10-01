// include/literal.h
#ifndef LITERAL_H
#define LITERAL_H

#include <cstdint>

namespace ch { namespace core {

// A lightweight wrapper for compile-time/runtime literals.
// It carries the value and its *actual* bit width (for simulation/debug),
// but does NOT impose a fixed width in type system (width = 0 for trait purposes).
struct ch_literal {
    uint64_t value;
    uint32_t actual_width; // Minimum bits needed to represent `value`

    constexpr ch_literal(uint64_t v, uint32_t w) : value(v), actual_width(w) {
        // Ensure width is at least 1
        if (actual_width == 0) actual_width = 1;
        if (actual_width > 64) actual_width = 64;
    }
};

// Trait to identify ch_literal
template<typename T>
struct is_ch_literal : std::false_type {};

template<>
struct is_ch_literal<ch_literal> : std::true_type {};

template<typename T>
constexpr bool is_ch_literal_v = is_ch_literal<std::remove_cvref_t<T>>::value;

}} // namespace ch::core

#endif // LITERAL_H
