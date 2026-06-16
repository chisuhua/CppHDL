// include/core/operators_logic.h
// Phase 3: Logic operators + bit_select + bool reduce + bool ops
// Split from operators.h to keep each file <250 lines per Phase 3 plan.
#pragma once

#include "core/bool.h"
#include "core/context.h"
#include "core/literal.h"
#include "core/lnode.h"
#include "core/lnodeimpl.h"
#include "core/node_builder.h"
#include "core/traits.h"
#include "core/uint.h"
#include "utils/logger.h"
#include <cstdint>
#include <source_location>
#include <type_traits>

namespace ch::core {

// === 布尔专用操作模板 ===
template <typename Op>
ch_bool binary_bool_operation(const ch_bool &lhs, const ch_bool &rhs,
                              const std::string &name_suffix) {
    constexpr ch_op op_type = Op::op_type;

    auto lhs_operand = to_operand(lhs);
    auto rhs_operand = to_operand(rhs);

    auto *op_node = node_builder::instance().build_operation(
        op_type, lhs_operand, rhs_operand, 1, false,
        std::string(Op::name()) + "_" + name_suffix,
        std::source_location::current());

    return ch_bool(op_node);
}

template <typename Op>
ch_bool unary_bool_operation(const ch_bool &operand,
                             const std::string &name_suffix) {
    constexpr ch_op op_type = Op::op_type;

    auto operand_node = to_operand(operand);

    auto *op_node = node_builder::instance().build_unary_operation(
        op_type, operand_node, 1, std::string(Op::name()) + "_" + name_suffix,
        std::source_location::current());

    return ch_bool(op_node);
}

// === 位选择操作 ===
template <unsigned Index, typename T> auto bit_select(const T &operand) {
    static_assert(HardwareType<T>, "Operand must be a hardware type");

    auto operand_node = to_operand(operand);

    auto *op_node = node_builder::instance().build_bit_select(
        operand_node, Index, "bit_select", std::source_location::current());

    return make_uint_result<1>(op_node);
}

template <typename T> auto bit_select(const T &operand, unsigned index) {
    static_assert(HardwareType<T>, "Operand must be a hardware type");

    auto operand_node = to_operand(operand);

    auto *op_node = node_builder::instance().build_bit_select(
        operand_node, index, "bit_select", std::source_location::current());

    return make_uint_result<1>(op_node);
}

template <typename T> auto bit_select(const T &operand, int index) {
    return bit_select(operand, static_cast<unsigned>(index));
}

template <typename T> auto bit_select(const T &operand, uint64_t index) {
    return bit_select(operand, static_cast<unsigned>(index));
}

template <typename T> auto bit_select(const T &operand, int64_t index) {
    return bit_select(operand, static_cast<unsigned>(index));
}

template <typename T, typename Index>
auto bit_select(const T &operand, const Index &index) {
    static_assert(HardwareType<T>, "Operand must be a hardware type");
    static_assert(HardwareType<Index> || CHLiteral<Index>, "Index must be a hardware type or literal");

    auto operand_node = to_operand(operand);
    auto index_node = to_operand(index);

    auto *op_node = node_builder::instance().build_bit_select(
        operand_node, index_node, "bit_select",
        std::source_location::current());

    return make_uint_result<1>(op_node);
}

// === 逻辑二元操作符重载 ===
template <ValidOperand LHS, ValidOperand RHS>
auto operator&(const LHS &lhs, const RHS &rhs) {
    return binary_operation<and_op>(lhs, rhs, "and");
}

template <ValidOperand LHS, ValidOperand RHS>
auto operator|(const LHS &lhs, const RHS &rhs) {
    return binary_operation<or_op>(lhs, rhs, "or");
}

template <ValidOperand LHS, ValidOperand RHS>
auto operator^(const LHS &lhs, const RHS &rhs) {
    return binary_operation<xor_op>(lhs, rhs, "xor");
}

// === 一元按位取反 ===
template <ValidOperand T> auto operator~(const T &operand) {
    return unary_operation<not_op>(operand, "not");
}

// === 约简操作 ===
template <typename T> ch_bool and_reduce(const T &operand) {
    static_assert(HardwareType<T>, "Operand must be a hardware type");

    auto operand_node = to_operand(operand);

    auto *op_node = node_builder::instance().build_unary_operation(
        ch_op::and_reduce, // 使用专门的规约操作类型
        operand_node, 1, "and_reduce", std::source_location::current());

    return make_bool_result(op_node);
}

template <typename T> ch_bool or_reduce(const T &operand) {
    static_assert(HardwareType<T>, "Operand must be a hardware type");

    auto operand_node = to_operand(operand);

    auto *op_node = node_builder::instance().build_unary_operation(
        ch_op::or_reduce, // 使用专门的规约操作类型
        operand_node, 1, "or_reduce", std::source_location::current());

    return make_bool_result(op_node);
}

template <typename T> ch_bool xor_reduce(const T &operand) {
    static_assert(HardwareType<T>, "Operand must be a hardware type");

    auto operand_node = to_operand(operand);

    auto *op_node = node_builder::instance().build_unary_operation(
        ch_op::xor_reduce, // 使用专门的规约操作类型
        operand_node, 1, "xor_reduce", std::source_location::current());

    return make_bool_result(op_node);
}

// === 布尔专用操作符重载 ===
inline ch_bool operator&&(const ch_bool &lhs, const ch_bool &rhs) {
    return binary_bool_operation<logical_and_op>(lhs, rhs, "logical_and");
}

inline ch_bool operator||(const ch_bool &lhs, const ch_bool &rhs) {
    return binary_bool_operation<logical_or_op>(lhs, rhs, "logical_or");
}

inline ch_bool operator!(const ch_bool &operand) {
    return unary_bool_operation<logical_not_op>(operand, "logical_not");
}

// === 布尔与其他类型的操作 ===
template <unsigned N>
inline ch_bool operator&&(const ch_bool &lhs, const ch_uint<N> &rhs) {
    return binary_bool_operation<logical_and_op>(lhs, rhs != ch_uint<N>(0),
                                                 "logical_and");
}

template <unsigned N>
inline ch_bool operator&&(const ch_uint<N> &lhs, const ch_bool &rhs) {
    return binary_bool_operation<logical_and_op>(lhs != ch_uint<N>(0), rhs,
                                                 "logical_and");
}

template <unsigned N>
inline ch_bool operator||(const ch_bool &lhs, const ch_uint<N> &rhs) {
    return binary_bool_operation<logical_or_op>(lhs, rhs != ch_uint<N>(0),
                                                "logical_or");
}

template <unsigned N>
inline ch_bool operator||(const ch_uint<N> &lhs, const ch_bool &rhs) {
    return binary_bool_operation<logical_or_op>(lhs != ch_uint<N>(0), rhs,
                                                "logical_or");
}

} // namespace ch::core
