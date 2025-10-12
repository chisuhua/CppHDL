// include/bitbase.h
#ifndef BITBASE_H
#define BITBASE_H

#include "logic.h"        // For lnode, get_lnode, lnodeimpl, ch_width_v, ch_op, ch_literal
#include "lnodeimpl.h"    // For opimpl, lnodeimpl, ch_op
#include "core/context.h" // For ctx_curr_
#include "uint.h"         // For ch_uint
#include "traits.h"       // For ch_width_v
#include <source_location> // C++20
#include <type_traits>    // For std::is_arithmetic_v, std::remove_cvref_t
#include <algorithm>      // For std::max
#include <cstdint>
#include <iostream>       // For std::cerr

namespace ch { namespace core {

// --- Forward declaration for createOpNodeImpl (from logic.h) ---
template<typename T, typename U>
lnodeimpl* createOpNodeImpl(ch_op op, uint32_t size, bool is_signed,
                            const lnode<T>& lhs, const lnode<U>& rhs,
                            const std::string& name, const std::source_location& sloc) {
    auto ctx = ctx_curr_;
    if (!ctx) {
        std::cerr << "[createOpNodeImpl] Error: No active context!" << std::endl;
        return nullptr;
    }

    lnodeimpl* lhs_node_impl = lhs.impl();
    lnodeimpl* rhs_node_impl = rhs.impl();

    if (!lhs_node_impl || !rhs_node_impl) {
         std::cerr << "[createOpNodeImpl] Error: One or both operand lnodes are invalid!" << std::endl;
         return nullptr;
    }

    opimpl* op_node = ctx->create_node<opimpl>(
        size, op, is_signed, lhs_node_impl, rhs_node_impl,
        name, sloc
    );
    std::cout << "  [createOpNodeImpl] Created opimpl node '" << name << "' for operation " << static_cast<int>(op) << std::endl;

    proxyimpl* result_proxy = ctx->create_node<proxyimpl>(op_node, name, sloc);
    std::cout << "  [createOpNodeImpl] Created result proxyimpl node '" << name << "'" << std::endl;

    return result_proxy;
}

// Concept for HDL types that are NOT literals (i.e., have lnode<T>)
template<typename U>
concept CppHDLCapable = requires(U u) {
    { get_lnode(u) } -> std::convertible_to<lnode<U>>;
};

// ===================================================================
// === BINARY OPERATORS: ch_uint<M> OP ch_uint<N> (generic version) ===
// ===================================================================

template<unsigned M, unsigned N>
auto operator+(const ch_uint<M>& lhs, const ch_uint<N>& rhs) {
    constexpr unsigned result_width = std::max(M, N) + 1;
    lnodeimpl* op_node = createOpNodeImpl(ch_op::add, result_width, false,
                                          get_lnode(lhs), get_lnode(rhs),
                                          "add_op", std::source_location::current());
    return ch_uint<result_width>(op_node);
}

template<unsigned M, unsigned N>
auto operator-(const ch_uint<M>& lhs, const ch_uint<N>& rhs) {
    constexpr unsigned result_width = std::max(M, N);
    lnodeimpl* op_node = createOpNodeImpl(ch_op::sub, result_width, false,
                                          get_lnode(lhs), get_lnode(rhs),
                                          "sub_op", std::source_location::current());
    return ch_uint<result_width>(op_node);
}

template<unsigned M, unsigned N>
auto operator*(const ch_uint<M>& lhs, const ch_uint<N>& rhs) {
    constexpr unsigned result_width = M + N;
    lnodeimpl* op_node = createOpNodeImpl(ch_op::mul, result_width, false,
                                          get_lnode(lhs), get_lnode(rhs),
                                          "mul_op", std::source_location::current());
    return ch_uint<result_width>(op_node);
}

template<unsigned M, unsigned N>
auto operator&(const ch_uint<M>& lhs, const ch_uint<N>& rhs) {
    constexpr unsigned result_width = std::max(M, N);
    lnodeimpl* op_node = createOpNodeImpl(ch_op::and_, result_width, false,
                                          get_lnode(lhs), get_lnode(rhs),
                                          "and_op", std::source_location::current());
    return ch_uint<result_width>(op_node);
}

template<unsigned M, unsigned N>
auto operator|(const ch_uint<M>& lhs, const ch_uint<N>& rhs) {
    constexpr unsigned result_width = std::max(M, N);
    lnodeimpl* op_node = createOpNodeImpl(ch_op::or_, result_width, false,
                                          get_lnode(lhs), get_lnode(rhs),
                                          "or_op", std::source_location::current());
    return ch_uint<result_width>(op_node);
}

template<unsigned M, unsigned N>
auto operator^(const ch_uint<M>& lhs, const ch_uint<N>& rhs) {
    constexpr unsigned result_width = std::max(M, N);
    lnodeimpl* op_node = createOpNodeImpl(ch_op::xor_, result_width, false,
                                          get_lnode(lhs), get_lnode(rhs),
                                          "xor_op", std::source_location::current());
    return ch_uint<result_width>(op_node);
}

template<unsigned M, unsigned N>
auto operator==(const ch_uint<M>& lhs, const ch_uint<N>& rhs) {
    lnodeimpl* op_node = createOpNodeImpl(ch_op::eq, 1, false,
                                          get_lnode(lhs), get_lnode(rhs),
                                          "eq_op", std::source_location::current());
    return ch_uint<1>(op_node);
}

template<unsigned M, unsigned N>
auto operator!=(const ch_uint<M>& lhs, const ch_uint<N>& rhs) {
    lnodeimpl* op_node = createOpNodeImpl(ch_op::ne, 1, false,
                                          get_lnode(lhs), get_lnode(rhs),
                                          "ne_op", std::source_location::current());
    return ch_uint<1>(op_node);
}

template<unsigned M, unsigned N>
auto operator<(const ch_uint<M>& lhs, const ch_uint<N>& rhs) {
    lnodeimpl* op_node = createOpNodeImpl(ch_op::lt, 1, false,
                                          get_lnode(lhs), get_lnode(rhs),
                                          "lt_op", std::source_location::current());
    return ch_uint<1>(op_node);
}

template<unsigned M, unsigned N>
auto operator<=(const ch_uint<M>& lhs, const ch_uint<N>& rhs) {
    lnodeimpl* op_node = createOpNodeImpl(ch_op::le, 1, false,
                                          get_lnode(lhs), get_lnode(rhs),
                                          "le_op", std::source_location::current());
    return ch_uint<1>(op_node);
}

template<unsigned M, unsigned N>
auto operator>(const ch_uint<M>& lhs, const ch_uint<N>& rhs) {
    lnodeimpl* op_node = createOpNodeImpl(ch_op::gt, 1, false,
                                          get_lnode(lhs), get_lnode(rhs),
                                          "gt_op", std::source_location::current());
    return ch_uint<1>(op_node);
}

template<unsigned M, unsigned N>
auto operator>=(const ch_uint<M>& lhs, const ch_uint<N>& rhs) {
    lnodeimpl* op_node = createOpNodeImpl(ch_op::ge, 1, false,
                                          get_lnode(lhs), get_lnode(rhs),
                                          "ge_op", std::source_location::current());
    return ch_uint<1>(op_node);
}

// ===================================================================
// === BINARY OPERATORS: ch_uint<N> OP literal (arithmetic type) ======
// ===================================================================

// Helper: create a ch_uint<N> from a literal value in context
template<unsigned N, typename Lit>
requires std::is_arithmetic_v<Lit>
lnode<ch_uint<N>> make_literal_lnode(Lit value) {
    auto ctx = ctx_curr_;
    if (!ctx) {
        std::cerr << "[make_literal_lnode] No active context!" << std::endl;
        std::abort();
    }
    // Use width N (same as the ch_uint operand)
    sdata_type sval(static_cast<uint64_t>(value), N);
    auto* lit_node = ctx->create_literal(sval);
    return lnode<ch_uint<N>>(lit_node);
}

// --- Addition with literal ---
template<unsigned N, typename Lit>
requires std::is_arithmetic_v<Lit>
auto operator+(const ch_uint<N>& lhs, Lit rhs_val) {
    auto rhs_lnode = make_literal_lnode<N>(rhs_val);
    constexpr unsigned result_width = N + 1;
    auto* op_node = createOpNodeImpl(ch_op::add, result_width, false,
                                     get_lnode(lhs), rhs_lnode,
                                     "add_lit", std::source_location::current());
    return ch_uint<result_width>(op_node);
}

template<typename Lit, unsigned N>
requires std::is_arithmetic_v<Lit>
auto operator+(Lit lhs_val, const ch_uint<N>& rhs) {
    return rhs + lhs_val;
}

// --- Subtraction with literal ---
template<unsigned N, typename Lit>
requires std::is_arithmetic_v<Lit>
auto operator-(const ch_uint<N>& lhs, Lit rhs_val) {
    auto rhs_lnode = make_literal_lnode<N>(rhs_val);
    constexpr unsigned result_width = N;
    auto* op_node = createOpNodeImpl(ch_op::sub, result_width, false,
                                     get_lnode(lhs), rhs_lnode,
                                     "sub_lit", std::source_location::current());
    return ch_uint<result_width>(op_node);
}

template<typename Lit, unsigned N>
requires std::is_arithmetic_v<Lit>
auto operator-(Lit lhs_val, const ch_uint<N>& rhs) {
    auto lhs_lnode = make_literal_lnode<N>(lhs_val);
    constexpr unsigned result_width = N;
    auto* op_node = createOpNodeImpl(ch_op::sub, result_width, false,
                                     lhs_lnode, get_lnode(rhs),
                                     "sub_lit_rev", std::source_location::current());
    return ch_uint<result_width>(op_node);
}

// --- Multiplication with literal ---
template<unsigned N, typename Lit>
requires std::is_arithmetic_v<Lit>
auto operator*(const ch_uint<N>& lhs, Lit rhs_val) {
    auto rhs_lnode = make_literal_lnode<N>(rhs_val);
    constexpr unsigned result_width = N + N; // Could optimize if literal small, but keep simple
    auto* op_node = createOpNodeImpl(ch_op::mul, result_width, false,
                                     get_lnode(lhs), rhs_lnode,
                                     "mul_lit", std::source_location::current());
    return ch_uint<result_width>(op_node);
}

template<typename Lit, unsigned N>
requires std::is_arithmetic_v<Lit>
auto operator*(Lit lhs_val, const ch_uint<N>& rhs) {
    return rhs * lhs_val;
}

// --- Bitwise AND with literal ---
template<unsigned N, typename Lit>
requires std::is_arithmetic_v<Lit>
auto operator&(const ch_uint<N>& lhs, Lit rhs_val) {
    auto rhs_lnode = make_literal_lnode<N>(rhs_val);
    constexpr unsigned result_width = N;
    auto* op_node = createOpNodeImpl(ch_op::and_, result_width, false,
                                     get_lnode(lhs), rhs_lnode,
                                     "and_lit", std::source_location::current());
    return ch_uint<result_width>(op_node);
}

template<typename Lit, unsigned N>
requires std::is_arithmetic_v<Lit>
auto operator&(Lit lhs_val, const ch_uint<N>& rhs) {
    return rhs & lhs_val;
}

// --- Bitwise OR with literal ---
template<unsigned N, typename Lit>
requires std::is_arithmetic_v<Lit>
auto operator|(const ch_uint<N>& lhs, Lit rhs_val) {
    auto rhs_lnode = make_literal_lnode<N>(rhs_val);
    constexpr unsigned result_width = N;
    auto* op_node = createOpNodeImpl(ch_op::or_, result_width, false,
                                     get_lnode(lhs), rhs_lnode,
                                     "or_lit", std::source_location::current());
    return ch_uint<result_width>(op_node);
}

template<typename Lit, unsigned N>
requires std::is_arithmetic_v<Lit>
auto operator|(Lit lhs_val, const ch_uint<N>& rhs) {
    return rhs | lhs_val;
}

// --- Bitwise XOR with literal ---
template<unsigned N, typename Lit>
requires std::is_arithmetic_v<Lit>
auto operator^(const ch_uint<N>& lhs, Lit rhs_val) {
    auto rhs_lnode = make_literal_lnode<N>(rhs_val);
    constexpr unsigned result_width = N;
    auto* op_node = createOpNodeImpl(ch_op::xor_, result_width, false,
                                     get_lnode(lhs), rhs_lnode,
                                     "xor_lit", std::source_location::current());
    return ch_uint<result_width>(op_node);
}

template<typename Lit, unsigned N>
requires std::is_arithmetic_v<Lit>
auto operator^(Lit lhs_val, const ch_uint<N>& rhs) {
    return rhs ^ lhs_val;
}

// --- Equality with literal ---
template<unsigned N, typename Lit>
requires std::is_arithmetic_v<Lit>
auto operator==(const ch_uint<N>& lhs, Lit rhs_val) {
    auto rhs_lnode = make_literal_lnode<N>(rhs_val);
    auto* op_node = createOpNodeImpl(ch_op::eq, 1, false,
                                     get_lnode(lhs), rhs_lnode,
                                     "eq_lit", std::source_location::current());
    return ch_uint<1>(op_node);
}

template<typename Lit, unsigned N>
requires std::is_arithmetic_v<Lit>
auto operator==(Lit lhs_val, const ch_uint<N>& rhs) {
    return rhs == lhs_val;
}

// --- Inequality with literal ---
template<unsigned N, typename Lit>
requires std::is_arithmetic_v<Lit>
auto operator!=(const ch_uint<N>& lhs, Lit rhs_val) {
    auto rhs_lnode = make_literal_lnode<N>(rhs_val);
    auto* op_node = createOpNodeImpl(ch_op::ne, 1, false,
                                     get_lnode(lhs), rhs_lnode,
                                     "ne_lit", std::source_location::current());
    return ch_uint<1>(op_node);
}

template<typename Lit, unsigned N>
requires std::is_arithmetic_v<Lit>
auto operator!=(Lit lhs_val, const ch_uint<N>& rhs) {
    return rhs != lhs_val;
}

// --- Less than with literal ---
template<unsigned N, typename Lit>
requires std::is_arithmetic_v<Lit>
auto operator<(const ch_uint<N>& lhs, Lit rhs_val) {
    auto rhs_lnode = make_literal_lnode<N>(rhs_val);
    auto* op_node = createOpNodeImpl(ch_op::lt, 1, false,
                                     get_lnode(lhs), rhs_lnode,
                                     "lt_lit", std::source_location::current());
    return ch_uint<1>(op_node);
}

template<typename Lit, unsigned N>
requires std::is_arithmetic_v<Lit>
auto operator<(Lit lhs_val, const ch_uint<N>& rhs) {
    auto lhs_lnode = make_literal_lnode<N>(lhs_val);
    auto* op_node = createOpNodeImpl(ch_op::lt, 1, false,
                                     lhs_lnode, get_lnode(rhs),
                                     "lt_lit_rev", std::source_location::current());
    return ch_uint<1>(op_node);
}

// --- Less equal, greater, greater equal: similar pattern ---
template<unsigned N, typename Lit>
requires std::is_arithmetic_v<Lit>
auto operator<=(const ch_uint<N>& lhs, Lit rhs_val) {
    auto rhs_lnode = make_literal_lnode<N>(rhs_val);
    auto* op_node = createOpNodeImpl(ch_op::le, 1, false,
                                     get_lnode(lhs), rhs_lnode,
                                     "le_lit", std::source_location::current());
    return ch_uint<1>(op_node);
}

template<typename Lit, unsigned N>
requires std::is_arithmetic_v<Lit>
auto operator<=(Lit lhs_val, const ch_uint<N>& rhs) {
    auto lhs_lnode = make_literal_lnode<N>(lhs_val);
    auto* op_node = createOpNodeImpl(ch_op::le, 1, false,
                                     lhs_lnode, get_lnode(rhs),
                                     "le_lit_rev", std::source_location::current());
    return ch_uint<1>(op_node);
}

template<unsigned N, typename Lit>
requires std::is_arithmetic_v<Lit>
auto operator>(const ch_uint<N>& lhs, Lit rhs_val) {
    auto rhs_lnode = make_literal_lnode<N>(rhs_val);
    auto* op_node = createOpNodeImpl(ch_op::gt, 1, false,
                                     get_lnode(lhs), rhs_lnode,
                                     "gt_lit", std::source_location::current());
    return ch_uint<1>(op_node);
}

template<typename Lit, unsigned N>
requires std::is_arithmetic_v<Lit>
auto operator>(Lit lhs_val, const ch_uint<N>& rhs) {
    auto lhs_lnode = make_literal_lnode<N>(lhs_val);
    auto* op_node = createOpNodeImpl(ch_op::gt, 1, false,
                                     lhs_lnode, get_lnode(rhs),
                                     "gt_lit_rev", std::source_location::current());
    return ch_uint<1>(op_node);
}

template<unsigned N, typename Lit>
requires std::is_arithmetic_v<Lit>
auto operator>=(const ch_uint<N>& lhs, Lit rhs_val) {
    auto rhs_lnode = make_literal_lnode<N>(rhs_val);
    auto* op_node = createOpNodeImpl(ch_op::ge, 1, false,
                                     get_lnode(lhs), rhs_lnode,
                                     "ge_lit", std::source_location::current());
    return ch_uint<1>(op_node);
}

template<typename Lit, unsigned N>
requires std::is_arithmetic_v<Lit>
auto operator>=(Lit lhs_val, const ch_uint<N>& rhs) {
    auto lhs_lnode = make_literal_lnode<N>(lhs_val);
    auto* op_node = createOpNodeImpl(ch_op::ge, 1, false,
                                     lhs_lnode, get_lnode(rhs),
                                     "ge_lit_rev", std::source_location::current());
    return ch_uint<1>(op_node);
}

// 左移操作
template<unsigned N, typename Lit>
requires std::is_arithmetic_v<Lit>
auto operator<<(const ch_uint<N>& lhs, Lit rhs_val) {
    auto rhs_lnode = make_literal_lnode<N>(rhs_val);
    constexpr unsigned result_width = N;
    auto* op_node = createOpNodeImpl(ch_op::shl, result_width, false,
                                     get_lnode(lhs), rhs_lnode,
                                     "shl_lit", std::source_location::current());
    return ch_uint<result_width>(op_node);
}

// 右移操作
template<unsigned N, typename Lit>
requires std::is_arithmetic_v<Lit>
auto operator>>(const ch_uint<N>& lhs, Lit rhs_val) {
    auto rhs_lnode = make_literal_lnode<N>(rhs_val);
    constexpr unsigned result_width = N;
    auto* op_node = createOpNodeImpl(ch_op::shr, result_width, false,
                                     get_lnode(lhs), rhs_lnode,
                                     "shr_lit", std::source_location::current());
    return ch_uint<result_width>(op_node);
}

// 负号操作
template<unsigned N>
auto operator-(const ch_uint<N>& operand) {
    lnode<ch_uint<N>> op_lnode = get_lnode(operand);
    lnode<ch_uint<N>> null_lnode(nullptr);
    auto* op_node = createOpNodeImpl(ch_op::neg, N, false,
                                     op_lnode, null_lnode,
                                     "neg_op", std::source_location::current());
    return ch_uint<N>(op_node);
}
// ===================================================================
// === UNARY OPERATORS ===
// ===================================================================

template<unsigned N>
auto operator~(const ch_uint<N>& operand) {
    lnode<ch_uint<N>> op_lnode = get_lnode(operand);
    if (!op_lnode.impl()) {
        std::cerr << "[operator~] Error: Operand is not a valid lnode type!" << std::endl;
        std::abort();
    }
    lnodeimpl* op_node = createOpNodeImpl(ch_op::not_, N, false,
                                          op_lnode, op_lnode,
                                          "not_op", std::source_location::current());
    return ch_uint<N>(op_node);
}

}} // namespace ch::core

#endif // BITBASE_H
