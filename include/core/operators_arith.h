// include/core/operators_arith.h
// Phase 3: Arithmetic operators + binary_operation template
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
#include <algorithm>
#include <cstdint>
#include <source_location>
#include <type_traits>

namespace ch::core {

// === 通用二元操作模板 ===
template <typename Op, ValidOperand LHS, ValidOperand RHS>
auto binary_operation(const LHS &lhs, const RHS &rhs,
                      const std::string &name_suffix) {
    // 如果两个操作数都是 ValidWidthOperand，则创建节点
    // 如果其中一个操作数是 ArithmeticLiteral，则尝试直接计算
    if constexpr (ValidWidthOperand<LHS> && ValidWidthOperand<RHS>) {
        // 两个操作数都是 ValidWidthOperand，按原来的方式处理
        constexpr ch_op op_type = Op::op_type;

        auto lhs_operand = to_operand(lhs);
        auto rhs_operand = to_operand(rhs);

        constexpr unsigned result_width =
            get_binary_result_width<Op, LHS, RHS>();

        auto *op_node = node_builder::instance().build_operation(
            op_type, lhs_operand, rhs_operand, result_width, false,
            std::string(Op::name()) + "_" + name_suffix,
            std::source_location::current());

        if constexpr (result_width <= 1) {
            if constexpr (std::is_same_v<std::remove_cvref_t<LHS>, ch_bool> ||
                          std::is_same_v<std::remove_cvref_t<RHS>, ch_bool>) {
                return make_bool_result(op_node);
            } else if constexpr (Op::is_comparison) {
                // 对于比较操作，总是返回ch_bool
                return make_bool_result(op_node);
            } else {
                return make_uint_result<1>(op_node);
            }
        } else {
            return make_uint_result<result_width>(op_node);
        }
    } else if constexpr (ArithmeticLiteral<LHS> && ValidWidthOperand<RHS>) {
        // 左操作数是 ArithmeticLiteral，右操作数是 ValidWidthOperand
        // 在右操作数节点上直接进行计算
        auto rhs_operand = to_operand(rhs);

        // 获取右操作数的值（仅当它是文字节点时）
        if (rhs_operand.impl()->is_const()) {
            auto *lit_node = static_cast<litimpl *>(rhs_operand.impl());
            sdata_type lhs_value(static_cast<uint64_t>(lhs), ch_width_v<LHS>);

            // 根据操作类型进行计算
            sdata_type result_data(0, rhs_operand.impl()->size());
            if constexpr (std::is_same_v<Op, add_op>) {
                result_data = lhs_value + lit_node->value();
            } else if constexpr (std::is_same_v<Op, sub_op>) {
                result_data = lhs_value - lit_node->value();
            } else if constexpr (std::is_same_v<Op, mul_op>) {
                result_data = lhs_value * lit_node->value();
            } else if constexpr (std::is_same_v<Op, div_op>) {
                result_data = lhs_value / lit_node->value();
            } else if constexpr (std::is_same_v<Op, mod_op>) {
                result_data = lhs_value % lit_node->value();
            } else if constexpr (std::is_same_v<Op, and_op>) {
                result_data = lhs_value & lit_node->value();
            } else if constexpr (std::is_same_v<Op, or_op>) {
                result_data = lhs_value | lit_node->value();
            } else if constexpr (std::is_same_v<Op, xor_op>) {
                result_data = lhs_value ^ lit_node->value();
            } else if constexpr (std::is_same_v<Op, shl_op>) {
                result_data =
                    static_cast<uint64_t>(lit_node->value())
                    << static_cast<uint32_t>(static_cast<uint64_t>(rhs));
            } else if constexpr (std::is_same_v<Op, shr_op>) {
                result_data = static_cast<uint64_t>(lit_node->value()) >>
                              static_cast<uint32_t>(static_cast<uint64_t>(rhs));
            } else {
                // 不支持的操作，回退到节点创建方式
                goto fallback_rhs;
            }

            // 创建结果字面量
            auto *result_node = node_builder::instance().build_literal(
                ch_literal_runtime(static_cast<uint64_t>(result_data)),
                std::string(Op::name()) + "_" + name_suffix,
                std::source_location::current());

            constexpr unsigned result_width =
                get_binary_result_width<Op, LHS, RHS>();
            if constexpr (result_width <= 1) {
                return make_bool_result(result_node);
            } else {
                return make_uint_result<result_width>(result_node);
            }
        } else {
        fallback_rhs:
            // 右操作数不是常量，回退到节点创建方式
            constexpr ch_op op_type = Op::op_type;

            auto lhs_operand = to_operand(lhs);

            constexpr unsigned result_width =
                get_binary_result_width<Op, LHS, RHS>();

            auto *op_node = node_builder::instance().build_operation(
                op_type, lhs_operand, rhs_operand, result_width, false,
                std::string(Op::name()) + "_" + name_suffix,
                std::source_location::current());

            if constexpr (result_width <= 1) {
                return make_bool_result(op_node);
            } else {
                return make_uint_result<result_width>(op_node);
            }
        }
    } else if constexpr (ValidWidthOperand<LHS> && ArithmeticLiteral<RHS>) {
        // 左操作数是 ValidWidthOperand，右操作数是 ArithmeticLiteral
        // 在左操作数节点上直接进行计算
        auto lhs_operand = to_operand(lhs);

        // 获取左操作数的值（仅当它是文字节点时）
        if (lhs_operand.impl()->is_const()) {
            auto *lit_node = static_cast<litimpl *>(lhs_operand.impl());
            sdata_type rhs_value(static_cast<uint64_t>(rhs), ch_width_v<RHS>);

            // 根据操作类型进行计算
            sdata_type result_data(0, lhs_operand.impl()->size());
            if constexpr (std::is_same_v<Op, add_op>) {
                result_data = lit_node->value() + rhs_value;
            } else if constexpr (std::is_same_v<Op, sub_op>) {
                result_data = lit_node->value() - rhs_value;
            } else if constexpr (std::is_same_v<Op, mul_op>) {
                result_data = lit_node->value() * rhs_value;
            } else if constexpr (std::is_same_v<Op, div_op>) {
                result_data = lit_node->value() / rhs_value;
            } else if constexpr (std::is_same_v<Op, mod_op>) {
                result_data = lit_node->value() % rhs_value;
            } else if constexpr (std::is_same_v<Op, and_op>) {
                result_data = lit_node->value() & rhs_value;
            } else if constexpr (std::is_same_v<Op, or_op>) {
                result_data = lit_node->value() | rhs_value;
            } else if constexpr (std::is_same_v<Op, xor_op>) {
                result_data = lit_node->value() ^ rhs_value;
            } else if constexpr (std::is_same_v<Op, shl_op>) {
                result_data =
                    lit_node->value()
                    << static_cast<uint32_t>(static_cast<uint64_t>(rhs));
            } else if constexpr (std::is_same_v<Op, shr_op>) {
                result_data = lit_node->value() >>
                              static_cast<uint32_t>(static_cast<uint64_t>(rhs));
            } else {
                // 不支持的操作，回退到节点创建方式
            }

            // 创建结果字面量
            auto *result_node = node_builder::instance().build_literal(
                ch_literal_runtime(static_cast<uint64_t>(result_data)),
                std::string(Op::name()) + "_" + name_suffix,
                std::source_location::current());

            constexpr unsigned result_width =
                get_binary_result_width<Op, LHS, RHS>();
            if constexpr (result_width <= 1) {
                return make_bool_result(result_node);
            } else {
                return make_uint_result<result_width>(result_node);
            }
        } else {
            // 左操作数不是常量，回退到节点创建方式
            constexpr ch_op op_type = Op::op_type;

            auto rhs_operand = to_operand(rhs);

            constexpr unsigned result_width =
                get_binary_result_width<Op, LHS, RHS>();

            auto *op_node = node_builder::instance().build_operation(
                op_type, lhs_operand, rhs_operand, result_width, false,
                std::string(Op::name()) + "_" + name_suffix,
                std::source_location::current());

            if constexpr (result_width <= 1) {
                return make_bool_result(op_node);
            } else {
                return make_uint_result<result_width>(op_node);
            }
        }
    } else if constexpr (ArithmeticLiteral<LHS> && ArithmeticLiteral<RHS>) {
        // 两个都是 ArithmeticLiteral，可以完全在编译期计算
        if constexpr (std::is_same_v<Op, add_op>) {
            return lhs + rhs;
        } else if constexpr (std::is_same_v<Op, sub_op>) {
            return lhs - rhs;
        } else if constexpr (std::is_same_v<Op, mul_op>) {
            return lhs * rhs;
        } else if constexpr (std::is_same_v<Op, div_op>) {
            return lhs / rhs;
        } else if constexpr (std::is_same_v<Op, mod_op>) {
            return lhs % rhs;
        } else if constexpr (std::is_same_v<Op, and_op>) {
            return lhs & rhs;
        } else if constexpr (std::is_same_v<Op, or_op>) {
            return lhs | rhs;
        } else if constexpr (std::is_same_v<Op, xor_op>) {
            return lhs ^ rhs;
        } else if constexpr (std::is_same_v<Op, eq_op>) {
            return ch_bool(lhs == rhs);
        } else if constexpr (std::is_same_v<Op, ne_op>) {
            return ch_bool(lhs != rhs);
        } else if constexpr (std::is_same_v<Op, lt_op>) {
            return ch_bool(lhs < rhs);
        } else if constexpr (std::is_same_v<Op, le_op>) {
            return ch_bool(lhs <= rhs);
        } else if constexpr (std::is_same_v<Op, gt_op>) {
            return ch_bool(lhs > rhs);
        } else if constexpr (std::is_same_v<Op, ge_op>) {
            return ch_bool(lhs >= rhs);
        } else {
            // 对于不支持直接计算的操作，回退到原来的实现
            constexpr ch_op op_type = Op::op_type;

            auto lhs_operand = to_operand(lhs);
            auto rhs_operand = to_operand(rhs);

            constexpr unsigned result_width =
                get_binary_result_width<Op, LHS, RHS>();

            auto *op_node = node_builder::instance().build_operation(
                op_type, lhs_operand, rhs_operand, result_width, false,
                std::string(Op::name()) + "_" + name_suffix,
                std::source_location::current());

            if constexpr (result_width <= 1) {
                if constexpr (Op::is_comparison) {
                    return make_bool_result(op_node);
                } else {
                    return make_uint_result<1>(op_node);
                }
            } else {
                return make_uint_result<result_width>(op_node);
            }
        }
    } else {
        // 其他情况，回退到原来的实现
        constexpr ch_op op_type = Op::op_type;

        auto lhs_operand = to_operand(lhs);
        auto rhs_operand = to_operand(rhs);

        constexpr unsigned result_width =
            get_binary_result_width<Op, LHS, RHS>();

        auto *op_node = node_builder::instance().build_operation(
            op_type, lhs_operand, rhs_operand, result_width, false,
            std::string(Op::name()) + "_" + name_suffix,
            std::source_location::current());

        if constexpr (result_width <= 1) {
            if constexpr (std::is_same_v<std::remove_cvref_t<LHS>, ch_bool> ||
                          std::is_same_v<std::remove_cvref_t<RHS>, ch_bool>) {
                return make_bool_result(op_node);
            } else if constexpr (Op::is_comparison) {
                return make_bool_result(op_node);
            } else {
                return make_uint_result<1>(op_node);
            }
        } else {
            return make_uint_result<result_width>(op_node);
        }
    }
}

// === 通用一元操作模板 ===
template <typename Op, ValidOperand T>
auto unary_operation(const T &operand, const std::string &name_suffix) {
    constexpr ch_op op_type = Op::op_type;

    auto operand_node = to_operand(operand);

    constexpr unsigned result_width = get_unary_result_width<Op, T>();

    auto *op_node = node_builder::instance().build_unary_operation(
        op_type, operand_node, result_width,
        std::string(Op::name()) + "_" + name_suffix,
        std::source_location::current());

    // 特殊处理：如果结果宽度为1且操作数是ch_bool，则返回ch_bool
    if constexpr (result_width <= 1 &&
                  std::is_same_v<std::remove_cvref_t<T>, ch_bool>) {
        return make_bool_result(op_node);
    } else if constexpr (result_width <= 1) {
        return make_uint_result<1>(op_node);
    } else {
        return make_uint_result<result_width>(op_node);
    }
}

// === 算术二元操作符重载 ===
template <ValidOperand LHS, ValidOperand RHS>
auto operator+(const LHS &lhs, const RHS &rhs) {
    return binary_operation<add_op>(lhs, rhs, "add");
}

template <ValidOperand LHS, ValidOperand RHS>
auto operator-(const LHS &lhs, const RHS &rhs) {
    return binary_operation<sub_op>(lhs, rhs, "sub");
}

template <ValidOperand LHS, ValidOperand RHS>
auto operator*(const LHS &lhs, const RHS &rhs) {
    return binary_operation<mul_op>(lhs, rhs, "mul");
}

// === 一元负号 ===
template <ValidOperand T> auto operator-(const T &operand) {
    return unary_operation<neg_op>(operand, "neg");
}

// === 除法和取模操作符重载 ===
template <ValidOperand LHS, ValidOperand RHS>
auto operator/(const LHS &lhs, const RHS &rhs) {
    return binary_operation<div_op>(lhs, rhs, "div");
}

template <ValidOperand LHS, ValidOperand RHS>
auto operator%(const LHS &lhs, const RHS &rhs) {
    return binary_operation<mod_op>(lhs, rhs, "mod");
}

} // namespace ch::core
