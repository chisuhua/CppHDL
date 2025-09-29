// include/logic.h
#ifndef LOGIC_H
#define LOGIC_H

#include "lnodeimpl.h" // For lnodeimpl
//#include "core/context.h" // For ctx_curr, context::create_literal
#include "traits.h" // For ch_width_v - Include the new traits header
#include <type_traits> // For type traits, std::integral
#include <source_location> // C++20
#include <cstdint> // For uint64_t

namespace ch { namespace core {

// --- lnode wrapper (as seen in cash) ---
// A generic wrapper around lnodeimpl*, holds type info and the pointer.
template<typename T>
struct lnode {
    lnodeimpl* impl() const { return impl_; }
    lnode(lnodeimpl* p) : impl_(p) {}
private:
    lnodeimpl* impl_ = nullptr;
};

// --- get_lnode function template (core mechanism from cash) ---
// Specialization for the lnode wrapper itself
template<typename T>
lnode<T> get_lnode(const lnode<T>& n) {
    return n; // Return the lnode wrapper directly
}

// Default implementation: assumes T has an 'impl()' method returning lnodeimpl*.
template<typename T>
lnode<T> get_lnode(const T& t) requires requires { t.impl(); } {
    return lnode<T>(t.impl()); // Use the impl() method of T to get lnodeimpl*
}

// --- Helper to create literal nodes from arithmetic types (cash style) ---
template<typename LiteralType>
requires std::is_arithmetic_v<LiteralType>
lnode<LiteralType> get_lnode(LiteralType literal_val) {
    auto ctx = ch::core::ctx_curr_;
    if (!ctx) {
        std::cerr << "[get_lnode(literal)] Error: No active context!" << std::endl;
        return lnode<LiteralType>(nullptr);
    }
    // --- 使用 traits.h 中定义的 ch_width_v ---
    constexpr uint32_t size = ch_width_v<LiteralType>; // <<--- Uses ch_width_v from traits.h
    // --- End of ch_width_v usage ---
    sdata_type sval(static_cast<uint64_t>(literal_val), size);
    auto* lit_node = ctx->create_literal(sval); // Assuming create_literal exists
    return lnode<LiteralType>(lit_node);
}

// --- Forward declarations for node creation functions ---
// These functions are implemented in their respective .cpp files (src/ast/...).
// Updated to accept lnode<T> and lnode<U> arguments for consistency and type safety.
template<typename T, typename U>
lnodeimpl* createOpNodeImpl(ch_op op, uint32_t size, bool is_signed,
                            const lnode<T>& lhs, const lnode<U>& rhs,
                            const std::string& name, const std::source_location& sloc);

// Forward declarations for createRegNodeImpl
// Make sure these signatures match the definitions in src/ast/regimpl.cpp
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
