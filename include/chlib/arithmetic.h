#ifndef CHLIB_ARITHMETIC_H
#define CHLIB_ARITHMETIC_H

#include "ch.hpp"
#include "component.h"
#include "core/bool.h"
#include "core/literal.h"
#include "core/operators.h"
#include "core/operators_runtime.h"
#include "core/uint.h"
#include <cassert>

using namespace ch::core;

namespace chlib {

/**
 * 加法器 - 函数式接口
 *
 * 接收两个相同位宽的输入并返回它们的和
 */
template <unsigned N> ch_uint<N> add(ch_uint<N> a, ch_uint<N> b) {
    return a + b;
}

/**
 * 带进位加法器 - 函数式接口
 *
 * 接收两个相同位宽的输入和一个进位输入，返回和与进位输出
 */
template <unsigned N> struct AddWithCarryResult {
    ch_uint<N> sum;
    ch_bool carry_out;
};

template <unsigned N>
AddWithCarryResult<N> add_with_carry(ch_uint<N> a, ch_uint<N> b,
                                     ch_bool carry_in) {
    ch_uint<N + 1> extended_a = ch_uint<N + 1>(a);
    ch_uint<N + 1> extended_b = ch_uint<N + 1>(b);
    
    // 使用make_literal创建单bit值
    ch_uint<N + 1> extended_cin = ch_uint<N + 1>(make_literal(carry_in ? 1ULL : 0ULL, 1));

    ch_uint<N + 1> result = extended_a + extended_b + extended_cin;

    AddWithCarryResult<N> output;
    output.sum = bits<N-1, 0>(result);
    output.carry_out = bit_select(result, N);

    return output;
}

/**
 * 减法器 - 函数式接口
 *
 * 接收两个相同位宽的输入并返回它们的差
 */
template <unsigned N> ch_uint<N> subtract(ch_uint<N> a, ch_uint<N> b) {
    return a - b;
}

/**
 * 带借位减法器 - 函数式接口
 *
 * 接收两个相同位宽的输入和一个借位输入，返回差与借位输出
 */
template <unsigned N> struct SubtractWithBorrowResult {
    ch_uint<N> diff;
    ch_bool borrow_out;
};

template <unsigned N>
SubtractWithBorrowResult<N> sub_with_borrow(ch_uint<N> a, ch_uint<N> b,
                                       ch_bool borrow_in) {
    ch_uint<N + 1> extended_a = ch_uint<N + 1>(a);
    ch_uint<N + 1> extended_b = ch_uint<N + 1>(b);
    ch_uint<N + 1> extended_bin = ch_uint<N + 1>(make_literal(borrow_in ? 1ULL : 0ULL, 1));

    ch_uint<N + 1> result = extended_a - extended_b - extended_bin;

    SubtractWithBorrowResult<N> output;
    output.diff = bits<N-1, 0>(result);
    output.borrow_out = bit_select(result, N);

    return output;
}

/**
 * 乘法器 - 函数式接口
 *
 * 接收两个相同位宽的输入并返回它们的积
 */
template <unsigned N> ch_uint<2 * N> multiply(ch_uint<N> a, ch_uint<N> b) {
    return a * b;
}

/**
 * 比较器 - 函数式接口
 *
 * 接收两个相同位宽的输入并返回比较结果
 */
template <unsigned N> struct ComparisonResult {
    ch_bool equal;         // a == b
    ch_bool not_equal;     // a != b
    ch_bool greater;       // a > b
    ch_bool less;          // a < b
    ch_bool greater_equal; // a >= b
    ch_bool less_equal;    // a <= b
};

template <unsigned N> ComparisonResult<N> compare(ch_uint<N> a, ch_uint<N> b) {
    ComparisonResult<N> result;
    result.equal = (a == b);
    result.not_equal = (a != b);
    result.greater = (a > b);
    result.less = (a < b);
    result.greater_equal = (a >= b);
    result.less_equal = (a <= b);

    return result;
}

/**
 * 绝对值 - 函数式接口
 *
 * 计算有符号数的绝对值（最高位为符号位）
 */
template <unsigned N> ch_uint<N> abs(ch_uint<N> a) {
    ch_bool is_negative = bit_select(a, N - 1);
    ch_uint<N> result = select(is_negative, ch_uint<N>(0_d) - a, a);
    return result;
}

/**
 * 左移运算器 - 函数式接口
 *
 * 执行左移操作
 */
template <unsigned N>
ch_uint<N> left_shift(ch_uint<N> a, unsigned shift_amount) {
    if (shift_amount >= N) {
        return ch_uint<N>(0_d); // 超出范围则返回0
    }
    return a << make_literal(shift_amount);
}

/**
 * 逻辑右移运算器 - 函数式接口
 *
 * 执行逻辑右移操作（填0）
 */
template <unsigned N>
ch_uint<N> logical_right_shift(ch_uint<N> a, unsigned shift_amount) {
    if (shift_amount >= N) {
        return ch_uint<N>(0_d); // 超出范围则返回0
    }
    return a >> make_literal(shift_amount);
}

/**
 * 算术右移运算器 - 函数式接口
 *
 * 执行算术右移操作（填符号位）
 */
template <unsigned N>
ch_uint<N> arithmetic_right_shift(ch_uint<N> a, unsigned shift_amount) {
    if (shift_amount >= N) {
        // 如果移位量超过位宽，返回符号位填充的结果
        ch_bool sign_bit = bit_select(a, N - 1);
        if (sign_bit) {
            return ch_uint<N>(~0_d); // 全1
        } else {
            return ch_uint<N>(0_d); // 全0
        }
    }

    // 获取符号位
    ch_bool sign_bit = bit_select(a, N - 1);

    // 执行右移
    ch_uint<N> shifted = a >> make_literal(shift_amount);

    // 构造符号位填充的掩码
    ch_uint<N> mask = 0_d;
    for (unsigned i = N - 1; i > N - 1 - shift_amount && i < N; --i) {
        mask = mask | (ch_uint<N>(1_d) << make_literal(i));
    }

    // 如果符号位为1，则用1填充高位
    ch_uint<N> sign_fill = select(sign_bit, mask, ch_uint<N>(0_d));

    return shifted | sign_fill;
}

} // namespace chlib

#endif // CHLIB_ARITHMETIC_H