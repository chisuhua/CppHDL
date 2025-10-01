// include/logic.h
#ifndef LOGIC_H
#define LOGIC_H

#include <bit>           // For std::bit_width (C++20)
#include <type_traits>   // For type traits
#include <source_location> // C++20
#include <cstdint>       // For uint64_t
#include <iostream>      // For std::cerr

#include "lnodeimpl.h"   // For lnodeimpl
#include "traits.h"      // For ch_width_v
#include "uint.h"        // For ctx_curr_, ch_uint
#include "literal.h"     // For ch_literal

namespace ch { namespace core {

// --- Runtime helper function for literal width calculation ---
template <typename T>
requires std::is_integral_v<T> && std::is_unsigned_v<T>
constexpr uint32_t get_literal_width(T value) {
    if (value == 0) return 1;
    return static_cast<uint32_t>(std::bit_width(value));
}

template <typename T>
requires std::is_integral_v<T> && std::is_signed_v<T>
constexpr uint32_t get_literal_width(T value) {
    using U = std::make_unsigned_t<T>;
    if (value == 0) return 1;
    // For negative numbers, bit_width of two's complement representation
    return get_literal_width(static_cast<U>(value));
}

// --- lnode wrapper ---
template<typename T>
struct lnode {
    lnodeimpl* impl() const { return impl_; }
    lnode(lnodeimpl* p) : impl_(p) {}
private:
    lnodeimpl* impl_ = nullptr;
};

// --- get_lnode overloads ---

// Identity: lnode<T> -> lnode<T>
template<typename T>
lnode<T> get_lnode(const lnode<T>& n) {
    return n;
}

// From HDL type (e.g., ch_uint<N>, ch_reg_impl<...>) that has .impl()
template<typename T>
lnode<T> get_lnode(const T& t) requires requires { t.impl(); } {
    return lnode<T>(t.impl());
}

// --- SPECIAL: For arithmetic literals, return ch_literal (NOT ch_uint<N>) ---
template<typename LiteralType>
requires std::is_arithmetic_v<LiteralType>
ch_literal get_lnode(LiteralType literal_val) {
    // Compute actual bit width needed for this literal
    using Decayed = std::decay_t<LiteralType>;
    using UnsignedType = std::make_unsigned_t<Decayed>;
    
    uint32_t actual_width = get_literal_width(static_cast<UnsignedType>(literal_val));
    if (actual_width == 0) actual_width = 1;
    if (actual_width > 64) actual_width = 64;

    return ch_literal{
        static_cast<uint64_t>(literal_val),
        actual_width
    };
}

// --- Forward declarations for node creation ---
template<typename T, typename U>
lnodeimpl* createOpNodeImpl(ch_op op, uint32_t size, bool is_signed,
                            const lnode<T>& lhs, const lnode<U>& rhs,
                            const std::string& name, const std::source_location& sloc);

template<typename T>
lnodeimpl* createRegNodeImpl(unsigned size, const std::string& name, const std::source_location& sloc);

template<typename T>
lnodeimpl* createRegNodeImpl(const lnode<T>& init, const std::string& name, const std::source_location& sloc);

// Forward declaration of the internal implementation
template<typename T>
class ch_reg_impl;

// Public alias for the user-facing register type
template<typename T>
using ch_reg = const ch_reg_impl<T>;

}} // namespace ch::core
#endif // LOGIC_H
