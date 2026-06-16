// include/core/io_logical.h
// Phase 4: Logical/bitwise/comparison operators for port<,> types
// Split from io.h to keep each file <150 lines per Phase 4 plan.
//
// Includes: operator&, |, ^, ~, ==, !=, &&, ||, !, <, <=, >, >=,
//            bit_select, select, and/or/xor_reduce, sext, zext, bits
//            overloads for port<,> types.
//
// The 4 CHREQUIRE assertions from commit bc0cbdb (ADR-010 Q3 port
// validation) live in operator<<= which stays in io.h aggregator.
#pragma once

#include "core/context.h"
#include "core/direction.h"
#include "core/lnode.h"
#include "core/lnodeimpl.h"
#include "core/node_builder.h"
#include "core/operators.h"
#include "core/traits.h"
#include "core/uint.h"
#include <cstdint>
#include <source_location>
#include <type_traits>

namespace ch::core {

// 为端口添加按位与操作符 (&)
template <typename T1, typename Dir1, typename T2, typename Dir2>
auto operator&(const port<T1, Dir1> &lhs, const port<T2, Dir2> &rhs) {
    return get_lnode(lhs) & get_lnode(rhs);
}

// 为端口添加按位或操作符 (|)
template <typename T1, typename Dir1, typename T2, typename Dir2>
auto operator|(const port<T1, Dir1> &lhs, const port<T2, Dir2> &rhs) {
    return get_lnode(lhs) | get_lnode(rhs);
}

// 为端口添加按位异或操作符 (^)
template <typename T1, typename Dir1, typename T2, typename Dir2>
auto operator^(const port<T1, Dir1> &lhs, const port<T2, Dir2> &rhs) {
    return get_lnode(lhs) ^ get_lnode(rhs);
}

// 为端口添加按位取反操作符 (~)
template <typename T, typename Dir>
auto operator~(const port<T, Dir> &operand) {
    return ~get_lnode(operand);
}

// 为端口添加相等比较操作符 (==)
template <typename T1, typename Dir1, typename T2, typename Dir2>
auto operator==(const port<T1, Dir1> &lhs, const port<T2, Dir2> &rhs) {
    return get_lnode(lhs) == get_lnode(rhs);
}

// 为端口添加不等比较操作符 (!=)
template <typename T1, typename Dir1, typename T2, typename Dir2>
auto operator!=(const port<T1, Dir1> &lhs, const port<T2, Dir2> &rhs) {
    return get_lnode(lhs) != get_lnode(rhs);
}

// 为端口添加逻辑操作符支持
// TODO: 这些操作符应标记为已废弃——在硬件中 &&/|| 和 &/| 对 1-bit 信号效果相同，
// 但 C++ 的短路语义让用户误以为有特殊行为。推荐使用 select() 替代。
template <typename T1, typename Dir1, typename T2, typename Dir2>
auto operator&&(const port<T1, Dir1> &lhs, const port<T2, Dir2> &rhs) {
    static_assert(ch_width_v<T1> == 1 && ch_width_v<T2> == 1,
                  "Logical AND only supported for 1-bit types");
    // 在硬件中，逻辑与和按位与效果相同
    return get_lnode(lhs) & get_lnode(rhs);
}

template <typename T1, typename Dir1, typename T2, typename Dir2>
auto operator||(const port<T1, Dir1> &lhs, const port<T2, Dir2> &rhs) {
    static_assert(ch_width_v<T1> == 1 && ch_width_v<T2> == 1,
                  "Logical OR only supported for 1-bit types");
    // 在硬件中，逻辑或和按位或效果相同
    return get_lnode(lhs) | get_lnode(rhs);
}

template <typename T, typename Dir>
auto operator!(const port<T, Dir> &operand) {
    static_assert(ch_width_v<T> == 1,
                  "Logical NOT only supported for 1-bit types");
    return ~get_lnode(operand);
}

// 比较操作符
template <typename T1, typename Dir1, typename T2, typename Dir2>
auto operator<(const port<T1, Dir1> &lhs, const port<T2, Dir2> &rhs) {
    return get_lnode(lhs) < get_lnode(rhs);
}

template <typename T1, typename Dir1, typename T2, typename Dir2>
auto operator<=(const port<T1, Dir1> &lhs, const port<T2, Dir2> &rhs) {
    return get_lnode(lhs) <= get_lnode(rhs);
}

template <typename T1, typename Dir1, typename T2, typename Dir2>
auto operator>(const port<T1, Dir1> &lhs, const port<T2, Dir2> &rhs) {
    return get_lnode(lhs) > get_lnode(rhs);
}

template <typename T1, typename Dir1, typename T2, typename Dir2>
auto operator>=(const port<T1, Dir1> &lhs, const port<T2, Dir2> &rhs) {
    return get_lnode(lhs) >= get_lnode(rhs);
}

// 一元操作符
template <typename T, typename Dir>
auto operator-(const port<T, Dir> &operand) {
    return -get_lnode(operand);
}

// === 添加对bit_select操作的支持 ===
template <unsigned Index, typename T, typename Dir>
auto bit_select(const port<T, Dir> &operand) {
    auto node = to_operand(operand);
    auto *op_node = node_builder::instance().build_bit_select(
        node, Index, "bit_select", std::source_location::current());

    return make_uint_result<1>(op_node);
}

// === 添加对select操作的支持 ===
template <typename CondType, typename T1, typename Dir1, typename T2,
          typename Dir2>
auto select(const CondType &condition, const port<T1, Dir1> &true_val,
            const port<T2, Dir2> &false_val) {
    return select(condition, get_lnode(true_val), get_lnode(false_val));
}

// === 添加对归约操作的支持 ===
template <typename T, typename Dir>
auto and_reduce(const port<T, Dir> &operand) {
    return and_reduce(get_lnode(operand));
}

template <typename T, typename Dir>
auto or_reduce(const port<T, Dir> &operand) {
    return or_reduce(get_lnode(operand));
}

template <typename T, typename Dir>
auto xor_reduce(const port<T, Dir> &operand) {
    return xor_reduce(get_lnode(operand));
}

// 端口类型的重载版本
template <typename T, unsigned NewWidth, typename Dir>
auto sext(const port<T, Dir> &operand) {
    auto lnode_operand = to_operand(operand);
    return make_uint_result<NewWidth>(node_builder::instance().build_operation(
        ch_op::sext, lnode_operand, NewWidth, true, "sext",
        std::source_location::current()));
}

template <typename T, unsigned NewWidth, typename Dir>
auto zext(const port<T, Dir> &operand) {
    auto lnode_operand = to_operand(operand);
    return make_uint_result<NewWidth>(node_builder::instance().build_operation(
        ch_op::zext, lnode_operand, NewWidth, false, "zext",
        std::source_location::current()));
}

template <typename T, unsigned MSB, unsigned LSB, typename Dir>
auto bits(const port<T, Dir> &operand) {
    auto lnode_operand = to_operand(operand);

    auto *op_node = node_builder::instance().build_bits(
        lnode_operand, MSB, LSB, "bits", std::source_location::current());

    return make_uint_result<MSB - LSB + 1>(op_node);
}

} // namespace ch::core
