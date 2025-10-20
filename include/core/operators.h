// include/core/operators.h
#ifndef CH_CORE_OPERATORS_H
#define CH_CORE_OPERATORS_H

#include "core/bool.h"
#include "core/uint.h"
#include "core/literal.h"
#include "core/lnode.h"
#include "core/lnodeimpl.h"
#include "core/context.h"
#include "core/traits.h"
#include "core/node_builder.h"
#include "utils/logger.h"
#include <source_location>
#include <type_traits>
#include <algorithm>
#include <cstdint>

#include "../lnode/operators.h"

namespace ch::core {
// === 严格的硬件类型概念 ===
template<typename T>
concept HardwareType = requires(const T& t) {
    { t.impl() } -> std::convertible_to<lnodeimpl*>;
} && (!std::is_arithmetic_v<std::remove_cvref_t<T>>);

// === 字面量类型概念 ===
template<typename T>
concept ArithmeticLiteral = std::is_arithmetic_v<std::remove_cvref_t<T>>;

// === ch_literal概念 ===
template<typename T>
concept CHLiteral = is_ch_literal_v<std::remove_cvref_t<T>>;

// === 有效的操作数概念 ===
template<typename T>
concept ValidOperand = HardwareType<T> || ArithmeticLiteral<T> || CHLiteral<T>;

// === 统一的操作数转换 ===
template<typename T>
auto to_operand(const T& value) {
    if constexpr (HardwareType<T>) {
        return ch::core::get_lnode(value);
    } else if constexpr (ArithmeticLiteral<T>) {
        using Decayed = std::decay_t<T>;
        uint64_t val = static_cast<uint64_t>(value);
        uint32_t width = 0;
        
        if (val == 0) {
            width = 1;
        } else if constexpr (std::is_same_v<Decayed, bool>) {
            width = 1;
        } else if constexpr (std::is_integral_v<Decayed>) {
            using UnsignedType = std::make_unsigned_t<Decayed>;
            width = 64 - std::countl_zero(static_cast<UnsignedType>(val));
        } else {
            static_assert(std::is_integral_v<Decayed>, "Only integral types and bool are supported");
        }
        
        if (width == 0) width = 1;
        if (width > 64) width = 64;
        
        ch_literal lit(val, width);
        auto* lit_impl = node_builder::instance().build_literal(lit, "lit");
        return lnode<ch_uint<64>>(lit_impl);
    } else if constexpr (CHLiteral<T>) {
        auto* lit_impl = node_builder::instance().build_literal(value, "ch_lit");
        return lnode<ch_uint<64>>(lit_impl);
    } else {
        static_assert(sizeof(T) == 0, "Unsupported operand type - this should never happen due to concept constraints");
    }
}

// === 编译期计算结果宽度 ===
template<typename Op, typename LHS, typename RHS = void>
consteval unsigned get_binary_result_width() {
    if constexpr (Op::is_comparison) {
        return 1;
    } else if constexpr (requires { Op::template result_width<1, 1>; }) {
        if constexpr (HardwareType<LHS> && HardwareType<RHS>) {
            if constexpr (requires { ch_width_v<LHS>; } && requires { ch_width_v<RHS>; }) {
                return Op::template result_width<ch_width_v<LHS>, ch_width_v<RHS>>;
            }
        }
        if constexpr (HardwareType<LHS>) {
            if constexpr (requires { ch_width_v<LHS>; }) {
                return Op::template result_width<ch_width_v<LHS>, 32>;
            }
        }
        if constexpr (HardwareType<RHS>) {
            if constexpr (requires { ch_width_v<RHS>; }) {
                return Op::template result_width<32, ch_width_v<RHS>>;
            }
        }
    } else if constexpr (requires { Op::template result_width_v<1>; }) {
        // 一元操作的情况
        if constexpr (HardwareType<LHS>) {
            if constexpr (requires { ch_width_v<LHS>; }) {
                return Op::template result_width_v<ch_width_v<LHS>>;
            }
        }
        return 32; // 默认宽度
    }
    return 32; // 默认宽度
}

// === 根据宽度选择合适的ch_uint类型 ===
template<unsigned Width>
constexpr auto make_uint_result(lnodeimpl* node) {
    if constexpr (Width <= 1) {
        return ch_uint<1>(node);
    } else if constexpr (Width <= 8) {
        return ch_uint<8>(node);
    } else if constexpr (Width <= 16) {
        return ch_uint<16>(node);
    } else if constexpr (Width <= 32) {
        return ch_uint<32>(node);
    } else {
        return ch_uint<64>(node);
    }
}

// === 为ch_bool创建结果的特殊函数（改为 inline）===
inline ch_bool make_bool_result(lnodeimpl* node) {
    return ch_bool(node);
}

// === 通用二元操作模板 ===
template<typename Op, ValidOperand LHS, ValidOperand RHS>
auto binary_operation(const LHS& lhs, const RHS& rhs, const std::string& name_suffix) {
    constexpr ch_op op_type = Op::op_type;
    
    auto lhs_operand = to_operand(lhs);
    auto rhs_operand = to_operand(rhs);
    
    constexpr unsigned result_width = get_binary_result_width<Op, LHS, RHS>();
    
    auto* op_node = node_builder::instance().build_operation(
        op_type, lhs_operand, rhs_operand, false,
        std::string(Op::name()) + "_" + name_suffix,
        std::source_location::current()
    );
    
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

// === 通用一元操作模板 ===
template<typename Op, ValidOperand T>
auto unary_operation(const T& operand, const std::string& name_suffix) {
    constexpr ch_op op_type = Op::op_type;
    
    auto operand_node = to_operand(operand);
    
    auto* op_node = node_builder::instance().build_unary_operation(
        op_type, operand_node,
        std::string(Op::name()) + "_" + name_suffix,
        std::source_location::current()
    );
    
    constexpr unsigned result_width = get_binary_result_width<Op, T>();
    
    // 特殊处理：如果结果宽度为1且操作数是ch_bool，则返回ch_bool
    if constexpr (result_width <= 1 && std::is_same_v<std::remove_cvref_t<T>, ch_bool>) {
        return make_bool_result(op_node);
    } else if constexpr (result_width <= 1) {
        return make_uint_result<1>(op_node);
    } else {
        return make_uint_result<result_width>(op_node);
    }
}

// === 通用三元操作模板 ===
template<typename Op, ValidOperand Cond, ValidOperand TrueVal, ValidOperand FalseVal>
auto ternary_operation(const Cond& cond, const TrueVal& true_val, const FalseVal& false_val, const std::string& name_suffix) {
    if constexpr (std::is_same_v<Op, mux_op>) {
        auto cond_operand = to_operand(cond);
        auto true_operand = to_operand(true_val);
        auto false_operand = to_operand(false_val);
        
        auto* mux_node = node_builder::instance().build_mux(
            cond_operand, true_operand, false_operand,
            std::string(Op::name()) + "_" + name_suffix,
            std::source_location::current()
        );
        
        constexpr unsigned result_width = std::max(ch_width_v<TrueVal>, ch_width_v<FalseVal>);
        
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
template<typename Op>
ch_bool binary_bool_operation(const ch_bool& lhs, const ch_bool& rhs, const std::string& name_suffix) {
    constexpr ch_op op_type = Op::op_type;
    
    auto lhs_operand = to_operand(lhs);
    auto rhs_operand = to_operand(rhs);
    
    auto* op_node = node_builder::instance().build_operation(
        op_type, lhs_operand, rhs_operand, false,
        std::string(Op::name()) + "_" + name_suffix,
        std::source_location::current()
    );
    
    return ch_bool(op_node);
}

template<typename Op>
ch_bool unary_bool_operation(const ch_bool& operand, const std::string& name_suffix) {
    constexpr ch_op op_type = Op::op_type;
    
    auto operand_node = to_operand(operand);
    
    auto* op_node = node_builder::instance().build_unary_operation(
        op_type, operand_node,
        std::string(Op::name()) + "_" + name_suffix,
        std::source_location::current()
    );
    
    return ch_bool(op_node);
}

// === 位选择操作 ===
template<typename T, unsigned Index>
auto bit_select(const T& operand) {
    static_assert(HardwareType<T>, "Operand must be a hardware type");
    
    auto operand_node = to_operand(operand);

    constexpr unsigned index_width = bit_width(Index);
    using IndexType = ch_uint<index_width>;
    IndexType index_const(Index);

    auto idx_node = get_lnode(index_const);
    
    auto* op_node = node_builder::instance().build_operation(
        ch_op::bit_sel,
        operand_node,
        idx_node,
        false,
        "bit_select",
        std::source_location::current()
    );
    
    return make_uint_result<1>(op_node);
}

// === 位切片操作 ===
template<typename T, unsigned Msb, unsigned Lsb>
auto bits(const T& operand) {
    static_assert(HardwareType<T>, "Operand must be a hardware type");
    static_assert(Msb >= Lsb, "MSB must be greater than or equal to LSB");
    static_assert(Msb < ch_width_v<T>, "MSB out of bounds");
    
    constexpr unsigned width = Msb - Lsb + 1;
    constexpr unsigned msb_w = bit_width(Msb);
    constexpr unsigned lsb_w = bit_width(Lsb);

    using MsbType = ch_uint<msb_w>;
    using LsbType = ch_uint<lsb_w>;

    MsbType msb_val(Msb);
    LsbType lsb_val(Lsb);
    auto msb_node = get_lnode(msb_val);
    auto lsb_node = get_lnode(lsb_val);

    auto operand_node = to_operand(operand);
    
    // 构建位切片操作（这里简化处理，实际可能需要特殊处理）
    auto* op_node = node_builder::instance().build_operation(
        ch_op::bits_extract,
        operand_node,
        msb_node,
        lsb_node,
        false,
        "bits",
        std::source_location::current()
    );
    
    return make_uint_result<width>(op_node);
}

// === 位拼接操作 ===
template<typename T1, typename T2>
auto concat(const T1& lhs, const T2& rhs) {
    static_assert(HardwareType<T1> || ArithmeticLiteral<T1>, "Invalid operand type");
    static_assert(HardwareType<T2> || ArithmeticLiteral<T2>, "Invalid operand type");
    
    auto lhs_operand = to_operand(lhs);
    auto rhs_operand = to_operand(rhs);
    
    constexpr unsigned result_width = ch_width_v<T1> + ch_width_v<T2>;
    
    auto* op_node = node_builder::instance().build_operation(
        ch_op::concat,
        lhs_operand,
        rhs_operand,
        false,
        "concat",
        std::source_location::current()
    );
    
    return make_uint_result<result_width>(op_node);
}

// === 符号扩展操作 ===
template<typename T>
auto sign_extend(const T& operand, unsigned new_width) {
    static_assert(HardwareType<T>, "Operand must be a hardware type");
    
    auto operand_node = to_operand(operand);
    
    auto* op_node = node_builder::instance().build_unary_operation(
        ch_op::sext,
        operand_node,
        "sext",
        std::source_location::current()
    );
    
    return make_uint_result<new_width>(op_node);
}

// === 零扩展操作 ===
template<typename T>
auto zero_extend(const T& operand, unsigned new_width) {
    static_assert(HardwareType<T>, "Operand must be a hardware type");
    
    auto operand_node = to_operand(operand);
    
    auto* op_node = node_builder::instance().build_unary_operation(
        ch_op::zext,
        operand_node,
        "zext",
        std::source_location::current()
    );
    
    return make_uint_result<new_width>(op_node);
}

// === 约简操作 ===
template<typename T>
ch_bool and_reduce(const T& operand) {
    static_assert(HardwareType<T>, "Operand must be a hardware type");
    
    auto operand_node = to_operand(operand);
    
    auto* op_node = node_builder::instance().build_unary_operation(
        ch_op::and_,
        operand_node,
        "and_reduce",
        std::source_location::current()
    );
    
    return make_bool_result(op_node);
}

template<typename T>
ch_bool or_reduce(const T& operand) {
    static_assert(HardwareType<T>, "Operand must be a hardware type");
    
    auto operand_node = to_operand(operand);
    
    auto* op_node = node_builder::instance().build_unary_operation(
        ch_op::or_,
        operand_node,
        "or_reduce",
        std::source_location::current()
    );
    
    return make_bool_result(op_node);
}

template<typename T>
ch_bool xor_reduce(const T& operand) {
    static_assert(HardwareType<T>, "Operand must be a hardware type");
    
    auto operand_node = to_operand(operand);
    
    auto* op_node = node_builder::instance().build_unary_operation(
        ch_op::xor_,
        operand_node,
        "xor_reduce",
        std::source_location::current()
    );
    
    return make_bool_result(op_node);
}

// === 条件选择操作 ===
template<typename Cond, typename T, typename U>
auto select(const Cond& condition, const T& true_val, const U& false_val) {
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
template<ValidOperand LHS, ValidOperand RHS>
auto operator+(const LHS& lhs, const RHS& rhs) {
    return binary_operation<add_op>(lhs, rhs, "add");
}

template<ValidOperand LHS, ValidOperand RHS>
auto operator-(const LHS& lhs, const RHS& rhs) {
    return binary_operation<sub_op>(lhs, rhs, "sub");
}

template<ValidOperand LHS, ValidOperand RHS>
auto operator*(const LHS& lhs, const RHS& rhs) {
    return binary_operation<mul_op>(lhs, rhs, "mul");
}

template<ValidOperand LHS, ValidOperand RHS>
auto operator&(const LHS& lhs, const RHS& rhs) {
    return binary_operation<and_op>(lhs, rhs, "and");
}

template<ValidOperand LHS, ValidOperand RHS>
auto operator|(const LHS& lhs, const RHS& rhs) {
    return binary_operation<or_op>(lhs, rhs, "or");
}

template<ValidOperand LHS, ValidOperand RHS>
auto operator^(const LHS& lhs, const RHS& rhs) {
    return binary_operation<xor_op>(lhs, rhs, "xor");
}

template<ValidOperand LHS, ValidOperand RHS>
auto operator==(const LHS& lhs, const RHS& rhs) {
    return binary_operation<eq_op>(lhs, rhs, "eq");
}

template<ValidOperand LHS, ValidOperand RHS>
auto operator!=(const LHS& lhs, const RHS& rhs) {
    return binary_operation<ne_op>(lhs, rhs, "ne");
}

template<ValidOperand LHS, ValidOperand RHS>
auto operator<(const LHS& lhs, const RHS& rhs) {
    return binary_operation<lt_op>(lhs, rhs, "lt");
}

template<ValidOperand LHS, ValidOperand RHS>
auto operator<=(const LHS& lhs, const RHS& rhs) {
    return binary_operation<le_op>(lhs, rhs, "le");
}

template<ValidOperand LHS, ValidOperand RHS>
auto operator>(const LHS& lhs, const RHS& rhs) {
    return binary_operation<gt_op>(lhs, rhs, "gt");
}

template<ValidOperand LHS, ValidOperand RHS>
auto operator>=(const LHS& lhs, const RHS& rhs) {
    return binary_operation<ge_op>(lhs, rhs, "ge");
}

template<ValidOperand LHS, ValidOperand RHS>
auto operator<<(const LHS& lhs, const RHS& rhs) {
    return binary_operation<shl_op>(lhs, rhs, "shl");
}

template<ValidOperand LHS, ValidOperand RHS>
auto operator>>(const LHS& lhs, const RHS& rhs) {
    return binary_operation<shr_op>(lhs, rhs, "shr");
}

// === 一元操作符重载 ===
template<ValidOperand T>
auto operator~(const T& operand) {
    return unary_operation<not_op>(operand, "not");
}

template<ValidOperand T>
auto operator-(const T& operand) {
    return unary_operation<neg_op>(operand, "neg");
}

// === 布尔专用操作符重载 ===
inline ch_bool operator&&(const ch_bool& lhs, const ch_bool& rhs) {
    return binary_bool_operation<logical_and_op>(lhs, rhs, "logical_and");
}

inline ch_bool operator||(const ch_bool& lhs, const ch_bool& rhs) {
    return binary_bool_operation<logical_or_op>(lhs, rhs, "logical_or");
}

inline ch_bool operator!(const ch_bool& operand) {
    return unary_bool_operation<logical_not_op>(operand, "logical_not");
}

// === 布尔与其他类型的操作 ===
template<unsigned N>
inline ch_bool operator&&(const ch_bool& lhs, const ch_uint<N>& rhs) {
    return binary_bool_operation<logical_and_op>(lhs, rhs != ch_uint<N>(0), "logical_and");
}

template<unsigned N>
inline ch_bool operator&&(const ch_uint<N>& lhs, const ch_bool& rhs) {
    return binary_bool_operation<logical_and_op>(lhs != ch_uint<N>(0), rhs, "logical_and");
}

template<unsigned N>
inline ch_bool operator||(const ch_bool& lhs, const ch_uint<N>& rhs) {
    return binary_bool_operation<logical_or_op>(lhs, rhs != ch_uint<N>(0), "logical_or");
}

template<unsigned N>
inline ch_bool operator||(const ch_uint<N>& lhs, const ch_bool& rhs) {
    return binary_bool_operation<logical_or_op>(lhs != ch_uint<N>(0), rhs, "logical_or");
}

static_assert(HardwareType<ch_bool>, "ch_bool must be HardwareType");

} // namespace ch::core

#endif // CH_CORE_OPERATORS_H
