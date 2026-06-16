// include/core/operators_compare.h
// Phase 3: Comparison operators
// Split from operators.h to keep each file <250 lines per Phase 3 plan.
#pragma once

#include "core/bool.h"
#include "core/literal.h"
#include "core/lnode.h"
#include "core/lnodeimpl.h"
#include "core/traits.h"
#include "core/uint.h"
#include <cstdint>
#include <type_traits>

namespace ch::core {

// === 比较操作符重载 ===
template <ValidOperand LHS, ValidOperand RHS>
auto operator==(const LHS &lhs, const RHS &rhs) {
    return binary_operation<eq_op>(lhs, rhs, "eq");
}

template <ValidOperand LHS, ValidOperand RHS>
auto operator!=(const LHS &lhs, const RHS &rhs) {
    return binary_operation<ne_op>(lhs, rhs, "ne");
}

template <ValidOperand LHS, ValidOperand RHS>
auto operator<(const LHS &lhs, const RHS &rhs) {
    return binary_operation<lt_op>(lhs, rhs, "lt");
}

template <ValidOperand LHS, ValidOperand RHS>
auto operator<=(const LHS &lhs, const RHS &rhs) {
    return binary_operation<le_op>(lhs, rhs, "le");
}

template <ValidOperand LHS, ValidOperand RHS>
auto operator>(const LHS &lhs, const RHS &rhs) {
    return binary_operation<gt_op>(lhs, rhs, "gt");
}

template <ValidOperand LHS, ValidOperand RHS>
auto operator>=(const LHS &lhs, const RHS &rhs) {
    return binary_operation<ge_op>(lhs, rhs, "ge");
}

} // namespace ch::core
