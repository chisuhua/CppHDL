// include/core/operators.h
#ifndef CH_CORE_OPERATORS_H
#define CH_CORE_OPERATORS_H

#include "core/bool.h"
#include "core/context.h"
#include "core/io.h"
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
    if constexpr (HardwareType<T>) {
        return ch::core::get_lnode(value);
    } else if constexpr (ArithmeticLiteral<T>) {
        uint64_t val = static_cast<uint64_t>(value);
        // FIXME , how to compute value 's real width in compile time?
        constexpr uint32_t width = ch_width_v<T>;
        ch_literal_dynamic lit(val, width);
        auto *lit_impl = node_builder::instance().build_literal(lit, "lit");
        return lnode<ch_uint<width>>(lit_impl);
    } else if constexpr (CHLiteral<T>) {
        auto *lit_impl =
            node_builder::instance().build_literal(value, "ch_lit");
        // constexpr auto vaidth = value.width();
        constexpr uint32_t width = ch_width_v<T>;
        return lnode<ch_uint<width>>(lit_impl);
    } else {
        static_assert(sizeof(T) == 0,
                      "Unsupported operand type - this should never happen due "
                      "to concept constraints");
    }
}

// === 编译期计算结果宽度 ===
template <typename Op, typename LHS, typename RHS = void>
consteval unsigned get_binary_result_width() {
    if constexpr (Op::is_comparison) {
        return 1;
    } else if constexpr (requires { Op::template result_width<1, 1>; }) {
        if constexpr (
            requires { ch_width_v<LHS>; } && requires { ch_width_v<RHS>; }) {
            return Op::template result_width<ch_width_v<LHS>, ch_width_v<RHS>>;
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
                return make_bool_result(op_node);
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

// === 通用三元操作模板 ===
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
template <typename T, unsigned Index> auto bit_select(const T &operand) {
    static_assert(HardwareType<T>, "Operand must be a hardware type");

    auto operand_node = to_operand(operand);

    auto *op_node = node_builder::instance().build_bit_select(
        operand_node, Index, "bit_select", std::source_location::current());

    return make_uint_result<1>(op_node);
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

// === 符号扩展操作 ===
template <typename T, unsigned NewWidth> auto sext(const T &operand) {
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
template <typename T, unsigned NewWidth> auto zext(const T &operand) {
    static_assert(HardwareType<T>, "Operand must be a hardware type");
    static_assert(NewWidth >= ch_width_v<T>,
                  "New width must be >= original width");

    auto operand_node = to_operand(operand);

    auto *op_node = node_builder::instance().build_operation(
        ch_op::zext, operand_node, NewWidth,
        false, // 无符号操作
        "zext", std::source_location::current());

    return make_uint_result<NewWidth>(op_node);
}

// === 位域提取操作 ===
template <typename T, unsigned MSB, unsigned LSB> auto bits(const T &operand) {
    static_assert(HardwareType<T>, "Operand must be a hardware type");
    static_assert(MSB < ch_width_v<T>, "MSB must be < operand width");
    static_assert(LSB <= MSB, "LSB must be <= MSB");

    auto operand_node = to_operand(operand);

    auto *op_node = node_builder::instance().build_bits(
        operand_node, MSB, LSB, "bits", std::source_location::current());

    return make_uint_result<MSB - LSB + 1>(op_node);
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

// === 条件选择操作 ===
template <typename Cond, typename T, typename U>
auto select(const Cond &condition, const T &true_val, const U &false_val) {
    static_assert(ValidOperand<Cond>, "Condition must be a valid operand");
    static_assert(ValidOperand<T>, "True value must be a valid operand");
    static_assert(ValidOperand<U>, "False value must be a valid operand");

    return ternary_operation<mux_op>(condition, true_val, false_val, "select");
}

// === 算术右移操作 ===
/*
template<ValidOperand LHS, ValidOperand RHS>
auto operator>>>(const LHS& lhs, const RHS& rhs) {
    return binary_operation<sshr_op>(lhs, rhs, "sshr");
}
*/

// === 二元操作符重载 ===
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

template <ValidOperand LHS, ValidOperand RHS>
auto operator<<(const LHS &lhs, const RHS &rhs) {
    return binary_operation<shl_op>(lhs, rhs, "shl");
}

template <ValidOperand LHS, ValidOperand RHS>
auto operator>>(const LHS &lhs, const RHS &rhs) {
    return binary_operation<shr_op>(lhs, rhs, "shr");
}

// === 添加除法和取模操作符重载 ===
template <ValidOperand LHS, ValidOperand RHS>
auto operator/(const LHS &lhs, const RHS &rhs) {
    return binary_operation<div_op>(lhs, rhs, "div");
}

template <ValidOperand LHS, ValidOperand RHS>
auto operator%(const LHS &lhs, const RHS &rhs) {
    return binary_operation<mod_op>(lhs, rhs, "mod");
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

// === 一元操作符重载 ===
template <ValidOperand T> auto operator~(const T &operand) {
    return unary_operation<not_op>(operand, "not");
}

template <ValidOperand T> auto operator-(const T &operand) {
    return unary_operation<neg_op>(operand, "neg");
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

static_assert(HardwareType<ch_bool>, "ch_bool must be HardwareType");

} // namespace ch::core

#endif // CH_CORE_OPERATORS_H