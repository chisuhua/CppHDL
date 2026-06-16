// include/core/operators.h
// Phase 3: Slim aggregator for HDL core operators.
// After Phase 3 split, this file only declares concepts + to_operand +
// forward decls + select, and #include's the 5 per-category headers:
//   operators_width.h    — get_binary_result_width, get_unary_result_width,
//                          make_uint_result, make_bool_result, zext, sext,
//                          bits, bits_update, concat, to_bits_wrapper
//   operators_arith.h    — binary_operation template + operator+, -, *, /, %,
//                          unary_operation + unary operator-
//   operators_logic.h    — operator&, |, ^, ~, bit_select, and_reduce, or_reduce,
//                          xor_reduce, binary_bool_operation, unary_bool_operation,
//                          operator&&, ||, !
//   operators_compare.h  — operator==, !=, <, <=, >, >=
//   operators_shift.h    — shl, operator<<, >>, >>>, popcount, rotate_*
// All functions remain in namespace ch::core.
#pragma once

#include "core/bool.h"
#include "core/context.h"
#include "core/io.h"
#include "core/literal.h"
#include "core/lnode.h"
#include "core/lnodeimpl.h"
#include "core/node_builder.h"
#include "core/traits.h"
#include "core/uint.h"
#include "traits.h"
#include "utils/logger.h"
#include <algorithm>
#include <cstdint>
#include <source_location>
#include <type_traits>

#include "../lnode/operators_ext.h"

namespace ch::core {

// === 严格的硬件类型概念 ===
template <typename T>
concept HardwareType = requires(const T &t) {
    { t.impl() } -> std::convertible_to<lnodeimpl *>;
} && (!std::is_arithmetic_v<std::remove_cvref_t<T>>);

// === 字面量类型概念 ===
template <typename T>
concept ArithmeticLiteral = std::is_arithmetic_v<std::remove_cvref_t<T>>;

// === ch_literal概念 ===
template <typename T>
concept CHLiteral = is_ch_literal_v<std::remove_cvref_t<T>>;

// === 有效的操作数概念 ===
template <typename T>
concept ValidOperand = HardwareType<T> || ArithmeticLiteral<T> || CHLiteral<T>;

template <typename T>
concept ValidWidthOperand = HardwareType<T> || CHLiteral<T>;

// === 统一的操作数转换 ===
template <typename T> auto to_operand(const T &value) {
    // 特殊处理 port 类型（ch_in/ch_out）：提取 value_type 作为 lnode 的模板参数
    if constexpr (requires { typename T::value_type; typename T::direction; }) {
        return lnode<typename T::value_type>(value.impl());
    } else if constexpr (HardwareType<T>) {
        return ch::core::get_lnode(value);
    } else if constexpr (ArithmeticLiteral<T>) {
        uint64_t val = static_cast<uint64_t>(value);
        constexpr uint32_t width = ch_width_v<T>;
        ch_literal_dynamic lit(val, width);
        auto *lit_impl = node_builder::instance().build_literal(lit, "lit");
        return lnode<ch_uint<width>>(lit_impl);
    } else if constexpr (CHLiteral<T>) {
        auto *lit_impl =
            node_builder::instance().build_literal(value, "ch_lit");
        constexpr uint32_t width = ch_width_v<T>;
        return lnode<ch_uint<width>>(lit_impl);
    } else {
        static_assert(sizeof(T) == 0,
                      "Unsupported operand type - this should never happen due "
                      "to concept constraints");
    }
}

// Forward declarations so the per-category headers can declare operator
// overloads (which CALL binary_operation/unary_operation/ternary_operation)
// without seeing the full template body. The full definitions are provided
// below by the per-category headers; templates are instantiated at the use
// site, at which point all definitions are visible.
template <typename Op, ValidOperand LHS, ValidOperand RHS>
auto binary_operation(const LHS &lhs, const RHS &rhs,
                      const std::string &name_suffix);

template <typename Op, ValidOperand T>
auto unary_operation(const T &operand, const std::string &name_suffix);

template <typename Op, ValidOperand Cond, ValidOperand TrueVal,
          ValidOperand FalseVal>
auto ternary_operation(const Cond &cond, const TrueVal &true_val,
                       const FalseVal &false_val,
                       const std::string &name_suffix);

} // namespace ch::core

// === Per-category operator headers (in dependency order: width first, then
// arith (defines binary_operation/unary_operation), then logic/compare/shift
// which call into the templates) ===
#include "core/operators_width.h"
#include "core/operators_arith.h"
#include "core/operators_logic.h"
#include "core/operators_compare.h"
#include "core/operators_shift.h"

namespace ch::core {

// === 通用三元操作模板 (mux only) ===
template <typename Op, ValidOperand Cond, ValidOperand TrueVal,
          ValidOperand FalseVal>
auto ternary_operation(const Cond &cond, const TrueVal &true_val,
                       const FalseVal &false_val,
                       const std::string &name_suffix) {
    if constexpr (std::is_same_v<Op, mux_op>) {
        auto cond_operand = to_operand(cond);
        auto true_operand = to_operand(true_val);
        auto false_operand = to_operand(false_val);

        auto *mux_node = node_builder::instance().build_mux(
            cond_operand, true_operand, false_operand,
            std::string(Op::name()) + "_" + name_suffix,
            std::source_location::current());

        constexpr unsigned result_width =
            std::max(ch_width_v<TrueVal>, ch_width_v<FalseVal>);

        if constexpr (result_width <= 1) {
            return make_bool_result(mux_node);
        } else {
            return make_uint_result<result_width>(mux_node);
        }
    } else {
        // 其他三元操作的处理...
        static_assert(sizeof(Op) == 0, "Unsupported ternary operation");
    }
}

// === 条件选择操作 ===
template <typename Cond, typename T, typename U>
auto select(const Cond &condition, const T &true_val, const U &false_val) {
    static_assert(ValidOperand<Cond>, "Condition must be a valid operand");
    static_assert(ValidOperand<T>, "True value must be a valid operand");
    static_assert(ValidOperand<U>, "False value must be a valid operand");

    return ternary_operation<mux_op>(condition, true_val, false_val, "select");
}

static_assert(HardwareType<ch_bool>, "ch_bool must be HardwareType");

} // namespace ch::core
