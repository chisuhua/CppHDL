// include/core/operators.h
#ifndef CH_CORE_OPERATORS_H
#define CH_CORE_OPERATORS_H

#include "lnode.h"
#include "lnodeimpl.h"
#include "core/context.h"
#include "uint.h"
#include "bool.h"
#include "traits.h"
#include "core/node_builder.h"
#include "literal.h"
#include "utils/logger.h"
#include <source_location>
#include <type_traits>
#include <algorithm>
#include <cstdint>

#include "../lnode/operators.h"

namespace ch { namespace core {
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

/*
template<typename T>
struct to_operand_return_type;

template<typename T>
struct to_operand_return_type<T> {
requires HardwareType<T>
    using type = decltype(get_lnode(std::declval<const T&>()));
};

template<typename T>
requires ArithmeticLiteral<T>
struct to_operand_return_type<T> {
    using type = lnode<ch_uint<64>>;
};

template<typename T>
requires CHLiteral<T>
struct to_operand_return_type<T> {
    using type = lnode<ch_uint<64>>;
};

template<typename T>
using to_operand_result_t = typename to_operand_return_type<T>::type;

template<typename T>
to_operand_result_t<T> to_operand(const T& value) {
    if constexpr (HardwareType<T>) {
        return get_lnode(value);
    } else if constexpr (ArithmeticLiteral<T>) {
        using Decayed = std::decay_t<T>;
        uint64_t val = static_cast<uint64_t>(value);
        uint32_t width = 0;
        
        if (val == 0) {
            width = 1;
        } else {
            if constexpr (std::is_same_v<Decayed, bool>) {
                width = 1;
            } else {
                using UnsignedType = std::make_unsigned_t<Decayed>;
                width = 64 - std::countl_zero(static_cast<UnsignedType>(val));
            }
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
        static_assert(sizeof(T) == 0, "Unsupported operand type");
    }
}
*/

// === 统一的操作数转换 ===
template<typename T>
auto to_operand(const T& value) {
    if constexpr (HardwareType<T>) {
        return get_lnode(value);
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
        /*
        } else {
            if constexpr (std::is_same_v<Decayed, bool>) {
                width = 1;
            } else {
                // 仅对非 bool 使用 make_unsigned
                using UnsignedType = std::make_unsigned_t<Decayed>;
                width = 64 - std::countl_zero(static_cast<UnsignedType>(val));
            }
        }
        */
        
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
template<typename Op, typename LHS, typename RHS>
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
    
    constexpr unsigned result_width = get_binary_result_width<Op, T, T>();
    
    // 特殊处理：如果结果宽度为1且操作数是ch_bool，则返回ch_bool
    if constexpr (result_width <= 1 && std::is_same_v<std::remove_cvref_t<T>, ch_bool>) {
        return make_bool_result(op_node);
    } else if constexpr (result_width <= 1) {
        return make_uint_result<1>(op_node);
    } else {
        return make_uint_result<result_width>(op_node);
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
}} // namespace ch::core

#endif // CH_CORE_OPERATORS_H
