// include/core/operators_width.h
// Phase 3: Width traits, zext, sext, bits, bits_update, concat, helpers
// Split from operators.h to keep each file <250 lines per Phase 3 plan.
#pragma once

#include "core/bool.h"
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

// Forward decl: many helpers in this file call binary_operation /
// unary_operation / ternary_operation defined in operators_arith.h.
// The aggregator (operators.h) forward-declares these so the operator
// overloads in this file can be declared here (the full definition is
// made visible when the aggregator is included by the user).

// === 编译期计算结果宽度 ===
template <typename Op, typename LHS, typename RHS = void>
consteval unsigned get_binary_result_width() {
    if constexpr (Op::is_comparison) {
        return 1;
    } else if constexpr (requires { Op::template result_width<1, 1>; }) {
        if constexpr (
            requires { ch_width_v<LHS>; } && requires { ch_width_v<RHS>; }) {

            // 特殊处理mod_op，当右操作数是编译期字面量时
            if constexpr (std::is_same_v<Op, ch::core::mod_op>) {
                // 检查右操作数是否是ch_literal_impl类型
                if constexpr (is_ch_literal_v<std::remove_cvref_t<RHS>>) {
                    // RHS是ch_literal_impl类型，获取其值并计算所需位宽
                    constexpr uint64_t value =
                        std::remove_cvref_t<RHS>::actual_value;
                    if constexpr (value > 0) {
                        // 模运算的结果最大为 value-1，所以需要的位宽是
                        // compute_bit_width(value-1)
                        return compute_bit_width(value - 1);
                    } else {
                        // 如果字面量值为0，这是无效的模运算，返回左操作数的宽度
                        return ch_width_v<LHS>;
                    }
                } else {
                    // RHS不是编译期字面量，使用默认的右操作数宽度
                    return Op::template result_width<ch_width_v<LHS>,
                                                     ch_width_v<RHS>>;
                }
            } else {
                // 非mod_op，使用原来逻辑
                return Op::template result_width<ch_width_v<LHS>,
                                                 ch_width_v<RHS>>;
            }
        }
        static_assert(1, "Invalid");
    } else if constexpr (requires { Op::template result_width_v<1>; }) {
        // 一元操作的情况
        if constexpr (ValidWidthOperand<LHS>) {
            if constexpr (requires { ch_width_v<LHS>; }) {
                return Op::template result_width_v<ch_width_v<LHS>>;
            }
        }
        return 32; // 默认宽度
    }
    return 32; // 默认宽度
}

template <typename Op, typename LHS>
consteval unsigned get_unary_result_width() {
    if constexpr (Op::is_comparison) {
        return 1;
    } else if constexpr (std::is_same_v<Op, popcount_op>) {
        // popcount的结果宽度是输入位宽的对数加上1
        // 例如：1位输入最多有1个1，需要1位表示（0或1）
        //      8位输入最多有8个1，需要4位表示（0-8）
        //      16位输入最多有16个1，需要5位表示（0-16）
        constexpr unsigned input_width = ch_width_v<LHS>;
        if constexpr (input_width <= 1) {
            return 1;
        } else {
            // 计算表示input_width所需的位数
            unsigned bits_needed = 1;
            unsigned max_count = input_width;
            while (max_count > 1) {
                max_count >>= 1;
                bits_needed++;
            }
            return bits_needed;
        }
    } else {
        return ch_width_v<LHS>;
    }
}

// === 根据宽度选择合适的ch_uint类型 ===
template <unsigned Width> constexpr auto make_uint_result(lnodeimpl *node) {
    return ch_uint<Width>(node);
}

// === 为ch_bool创建结果的特殊函数（改为 inline）===
inline ch_bool make_bool_result(lnodeimpl *node) { return ch_bool(node); }

// === 符号扩展操作 ===
template <unsigned NewWidth, typename T> auto sext(const T &operand) {
    static_assert(HardwareType<T>, "Operand must be a hardware type");
    static_assert(NewWidth >= ch_width_v<T>,
                  "New width must be >= original width");

    auto operand_node = to_operand(operand);

    auto *op_node = node_builder::instance().build_operation(
        ch_op::sext, operand_node, NewWidth,
        true, // 有符号操作
        "sext", std::source_location::current());

    return make_uint_result<NewWidth>(op_node);
}

// === 零扩展操作 ===
template <unsigned NewWidth, typename T>
auto zext(const T &operand, const std::string &name = "zext") {
    static_assert(HardwareType<T>, "Operand must be a hardware type");
    static_assert(NewWidth >= ch_width_v<T>,
                  "New width must be >= original width");

    auto operand_node = to_operand(operand);

    auto *op_node = node_builder::instance().build_operation(
        ch_op::zext, operand_node, NewWidth,
        false, // 无符号操作
        name, std::source_location::current());

    return make_uint_result<NewWidth>(op_node);
}

// === 位域提取操作 ===
template <unsigned MSB, unsigned LSB, typename T> auto bits(const T &operand) {
    static_assert(HardwareType<T>, "Operand must be a hardware type");
    static_assert(MSB < ch_width_v<T>, "MSB must be < operand width");
    static_assert(LSB <= MSB, "LSB must be <= MSB");

    auto operand_node = to_operand(operand);

    auto *op_node = node_builder::instance().build_bits(
        operand_node, MSB, LSB, "bits", std::source_location::current());

    return make_uint_result<MSB - LSB + 1>(op_node);
}

template <unsigned Width, typename T>
auto bits(const T &operand, unsigned lsb) {
    static_assert(HardwareType<T>, "Operand must be a hardware type");
    static_assert(Width <= ch_width_v<T>, "MSB must be < operand width");

    auto operand_node = to_operand(operand);

    auto *op_node = node_builder::instance().build_bits(
        operand_node, lsb + Width - 1, lsb, "bits",
        std::source_location::current());

    return make_uint_result<Width>(op_node);
}

// === 位段更新操作 ===
// 该函数接收一个目标值、一个源值和最低位索引，返回一个新值，
// 该新值是目标值的部分位被源值替换后的结果
template <unsigned Width, typename Target, typename Source>
auto bits_update(const Target &target, const Source &source, unsigned lsb) {
    static_assert(HardwareType<Target>, "Target must be a hardware type");
    static_assert(HardwareType<Source> || CHLiteral<Source> ||
                      ArithmeticLiteral<Source>,
                  "Source must be a hardware type, literal or arithmetic type");
    static_assert(Width <= ch_width_v<Target>, "Width must be <= target width");

    // 获取目标和源的操作节点
    auto target_operand = to_operand(target);
    auto source_operand = to_operand(source);

    // 计算最高位索引
    unsigned msb = lsb + Width - 1;

    // 创建位段更新操作节点
    auto *op_node = node_builder::instance().build_bits_update(
        target_operand, source_operand, msb, lsb);

    // 返回结果，位宽与目标相同
    return make_uint_result<ch_width_v<Target>>(op_node);
}

// === 位拼接操作 ===
template <typename T1, typename T2> auto concat(const T1 &lhs, const T2 &rhs) {
    static_assert(HardwareType<T1> || CHLiteral<T1>, "Invalid operand type");
    static_assert(HardwareType<T2> || CHLiteral<T2>, "Invalid operand type");

    auto lhs_operand = to_operand(lhs);
    auto rhs_operand = to_operand(rhs);

    constexpr unsigned result_width = ch_width_v<T1> + ch_width_v<T2>;

    auto *op_node = node_builder::instance().build_operation(
        ch_op::concat, lhs_operand, rhs_operand, result_width, false, "concat",
        std::source_location::current());

    return make_uint_result<result_width>(op_node);
}

// === to_bits 包装函数，用于将值转换为位表示 ===
template <typename T> auto to_bits_wrapper(const T &value) {
    if constexpr (requires { value.to_bits(); }) {
        // 如果类型有 to_bits 方法，直接使用它
        return value.to_bits();
    } else if constexpr (HardwareType<T> || CHLiteral<T>) {
        // 如果是硬件类型或字面量类型，获取其节点并构建 ch_uint
        constexpr unsigned W = ch_width_v<T>;
        return ch_uint<W>(value.impl());
    } else if constexpr (ArithmeticLiteral<T>) {
        // 如果是算术字面量，将其转换为适当宽度的 ch_uint
        constexpr unsigned W = sizeof(T) * 8; // 按类型大小确定位宽
        sdata_type data(static_cast<uint64_t>(value), W);
        auto *literal_node =
            node_builder::instance().build_literal(data, "literal");
        return ch_uint<W>(literal_node);
    } else {
        static_assert(sizeof(T) == 0, "Unsupported type for to_bits_wrapper");
    }
}

} // namespace ch::core
