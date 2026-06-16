// include/core/operators_shift.h
// Phase 3: Shift operators + popcount + rotate
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
#include <cstdint>
#include <source_location>
#include <type_traits>

namespace ch::core {

// === 算术右移操作（已注释保留位置） ===
/*
template<ValidOperand LHS, ValidOperand RHS>
auto operator>>>(const LHS& lhs, const RHS& rhs) {
    return binary_operation<sshr_op>(lhs, rhs, "sshr");
}
*/

// === 带结果位宽模板参数的左移操作 ===
template <unsigned ResultWidth, ValidOperand LHS, ValidOperand RHS>
auto shl(const LHS &lhs, const RHS &rhs) {
    auto lhs_node = to_operand(lhs);
    auto rhs_node = to_operand(rhs);

    auto *op_node = node_builder::instance().build_operation(
        ch_op::shl, lhs_node, rhs_node, ResultWidth, false, "shl",
        std::source_location::current());

    return make_uint_result<ResultWidth>(op_node);
}

template <ValidOperand LHS, ValidOperand RHS>
auto operator<<(const LHS &lhs, const RHS &rhs) {
    constexpr unsigned lhs_width = ch_width_v<LHS>;

    // Check if RHS is ch_literal_runtime vs ch_literal_impl
    // - ch_literal_runtime: use width-based calculation (value unknown at compile time)
    // - ch_literal_impl: use actual_value directly (compile-time known)
    constexpr unsigned rhs_extra_width = [] {
        using DecayedRHS = std::remove_cvref_t<RHS>;
        if constexpr (std::is_same_v<DecayedRHS, ch_literal_runtime>) {
            // Runtime literal: fall back to width-based calculation
            return (static_cast<uint64_t>(1) << ch_width_v<RHS>) - 1;
        } else if constexpr (is_ch_literal_v<DecayedRHS>) {
            // Compile-time literal: use static constexpr actual_value
            return DecayedRHS::actual_value;
        } else {
            // Non-literal operand: use width-based calculation
            return (static_cast<uint64_t>(1) << ch_width_v<RHS>) - 1;
        }
    }();

    constexpr unsigned result_width = lhs_width + rhs_extra_width;

    auto lhs_node = to_operand(lhs);
    auto rhs_node = to_operand(rhs);

    auto *op_node = node_builder::instance().build_operation(
        ch_op::shl, lhs_node, rhs_node, result_width, false, "shl",
        std::source_location::current());

    return make_uint_result<result_width>(op_node);
}

template <ValidOperand LHS, ValidOperand RHS>
auto operator>>(const LHS &lhs, const RHS &rhs) {
    return binary_operation<shr_op>(lhs, rhs, "shr");
}

// === 添加循环移位函数 ===
template <ValidOperand LHS, ValidOperand RHS>
auto rotate_left(const LHS &lhs, const RHS &rhs) {
    return binary_operation<rotate_l_op>(lhs, rhs, "rotate_l");
}

template <ValidOperand LHS, ValidOperand RHS>
auto rotate_right(const LHS &lhs, const RHS &rhs) {
    return binary_operation<rotate_r_op>(lhs, rhs, "rotate_r");
}

// === 添加popcount函数 ===
template <ValidOperand T> auto popcount(const T &operand) {
    return unary_operation<popcount_op>(operand, "popcount");
}

} // namespace ch::core
