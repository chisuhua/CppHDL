#ifndef CHLIB_ARITHMETIC_ADVANCE_H
#define CHLIB_ARITHMETIC_ADVANCE_H

#include "ch.hpp"
#include "chlib/logic.h"
#include "chlib/switch.h"
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
 * Carry Look-Ahead Adder - 超前进位加法器
 *
 * 通过提前计算进位信号来提高加法运算速度
 */
template <unsigned N> struct CLAResult {
    ch_uint<N> sum;
    ch_bool carry_out;
};

template <unsigned N>
CLAResult<N> carry_lookahead_adder(ch_uint<N> a, ch_uint<N> b,
                                   ch_bool carry_in = false) {
    static_assert(N > 0, "Carry lookahead adder must have at least 1 bit");
    constexpr unsigned shift_amount = compute_bit_width(N - 1);

    CLAResult<N> result;

    // 计算生成和传播信号
    ch_uint<N> g = a & b; // 生成信号 (Generate)
    ch_uint<N> p = a ^ b; // 传播信号 (Propagate)

    // 计算各级进位
    ch_bool carry = carry_in;
    ch_uint<N> carries = 0_d;

    for (unsigned i = 0; i < N; ++i) {
        ch_bool g_i = bit_select(g, i);
        ch_bool p_i = bit_select(p, i);

        // 计算当前位的进位
        ch_bool current_carry = g_i || (p_i && carry);
        carries = carries | (current_carry << make_uint<shift_amount>(i));

        carry = current_carry;
    }

    // 计算最终结果
    result.sum = p + carries;
    result.carry_out = carry;

    return result;
}

/**
 * Carry Save Adder - 保留进位加法器
 *
 * 用于三个操作数的加法，将和与进位分别输出，避免进位传播延迟
 */
template <unsigned N> struct CSAResult {
    ch_uint<N> sum;
    ch_uint<N> carry;
};

template <unsigned N>
CSAResult<N> carry_save_adder(ch_uint<N> a, ch_uint<N> b, ch_uint<N> c) {
    static_assert(N > 0, "Carry save adder must have at least 1 bit");

    CSAResult<N> result;

    // 计算保留进位加法
    result.sum = a ^ b ^ c;
    result.carry = ((a & b) | (b & c) | (a & c)) << 1_d;

    return result;
}

/**
 * Wallace Tree Multiplier - 华莱士树乘法器
 *
 * 高效的乘法器设计，使用压缩技术减少部分积
 */
template <unsigned N>
ch_uint<2 * N> wallace_tree_multiplier(ch_uint<N> a, ch_uint<N> b) {
    static_assert(N > 0, "Wallace tree multiplier must have at least 1 bit");

    if constexpr (N == 1) {
        // 特殊情况：1位乘法
        ch_uint<2> result = ch_uint<2>(a & b);
        return result;
    } else {
        // 对于多位乘法，我们使用简化实现
        // 实际的Wallace树涉及复杂的部分积压缩
        ch_uint<2 * N> result = 0_d;

        // 生成部分积并累加
        for (unsigned i = 0; i < N; ++i) {
            ch_bool b_bit = bit_select(b, i);
            // 修复：使用compute_bit_width计算正确的位宽，而不是手动的三元运算表达式
            constexpr unsigned shift_amount = compute_bit_width(N - 1);
            ch_uint<N> partial_product =
                select(b_bit, a << make_uint<shift_amount>(i), 0_d);
            result = result + ch_uint<2 * N>(partial_product);
        }

        return result;
    }
}

/**
 * Booth Multiplier - 班纳赫乘法器
 *
 * 使用Booth算法减少乘法操作中的加法次数
 */
template <unsigned N>
ch_uint<2 * N> booth_multiplier(ch_uint<N> a, ch_uint<N> b) {
    static_assert(N > 0, "Booth multiplier must have at least 1 bit");
    constexpr unsigned BIT_WIDTH = compute_bit_width(N - 1);

    // 简化版Booth乘法器实现
    ch_uint<2 * N> result = 0_d;
    ch_uint<N> multiplicand = a;
    ch_uint<N> multiplier = b;

    // 扩展乘数以处理Booth算法的最后一位
    ch_uint<N + 1> extended_multiplier = (multiplier << 1_d) | 0_d;

    for (unsigned i = 0; i < N; i += 2) {
        // 获取当前两位及前一位（用于Booth编码）
        ch_bool bit_i = bit_select(extended_multiplier, i);
        ch_bool bit_i_plus_1 = bit_select(extended_multiplier, i + 1);
        ch_bool bit_i_plus_2 =
            i + 2 < N + 1 ? bit_select(extended_multiplier, i + 2) : false;

        // Booth编码
        ch_uint<2> booth_code =
            select(bit_i, 1_d, 0_d) | (select(bit_i_plus_1, 2_d, 0_d));

        // 根据Booth编码执行操作
        ch_uint<2 * N> partial_val = switch_(
            booth_code, 0_d,
            case_(1_d, ch_uint<2 * N>(multiplicand) << make_uint<BIT_WIDTH>(i)),
            case_(2_d, (0_d - ch_uint<2 * N>(multiplicand))
                           << make_uint<BIT_WIDTH>(i)));

        // ch_uint<2 * N> partial_val = 0_d;
        // switch (booth_code) {
        // case 0: // 00 -> 0
        //     partial_val = 0_d;
        //     break;
        // case 1: // 01 -> +1
        //     partial_val = ch_uint<2 * N>(multiplicand)
        //                   << make_uint<BIT_WIDTH>(i);
        //     break;
        // case 2: // 10 -> -1
        //     partial_val = (0_d - ch_uint<2 * N>(multiplicand))
        //                   << make_uint<BIT_WIDTH>(i);
        //     break;
        // case 3: // 11 -> 0
        //     partial_val = 0_d;
        //     break;
        // }

        result = result + partial_val;
    }

    return result;
}

/**
 * Non-Restoring Divider - 非恢复除法器
 *
 * 实现非恢复除法算法，避免恢复步骤
 */
template <unsigned N> struct DividerResult {
    ch_uint<N> quotient;
    ch_uint<N> remainder;
};

template <unsigned N>
DividerResult<N> non_restoring_divider(ch_uint<N> dividend,
                                       ch_uint<N> divisor) {
    static_assert(N > 0, "Non-restoring divider must have at least 1 bit");

    DividerResult<N> result;
    result.quotient = 0_d;
    result.remainder = dividend;

    if (divisor == 0_d) {
        // 除零处理
        result.quotient = ~ch_uint<N>(0_d); // 返回全1
        result.remainder = 0_d;
        return result;
    }

    ch_uint<N> remainder = 0_d;
    ch_bool sign = false; // 用于跟踪符号

    // 简化的非恢复除法实现
    for (int i = N - 1; i >= 0; --i) {
        // 左移余数
        remainder = (remainder << 1_d) | bit_select(dividend, i);

        // 尝试减去除数
        ch_uint<N> temp = remainder - divisor;

        if (temp[N - 1] == false) { // 如果余数为正
            bit_select(result.quotient, i) = true;
            remainder = temp;
        } else {
            bit_select(result.quotient, i) = false;
        }
    }

    result.remainder = remainder;

    return result;
}

/**
 * Square Root Calculator - 平方根计算器
 *
 * 使用二分法或牛顿法计算平方根
 */
template <unsigned N>
ch_uint<(N + 1) / 2> square_root_calculator(ch_uint<N> input) {
    static_assert(N > 0, "Square root calculator must have at least 1 bit");
    static constexpr unsigned RESULT_WIDTH = (N + 1) / 2;

    ch_uint<RESULT_WIDTH> result = 0_d;

    // 使用二分法计算平方根
    ch_uint<RESULT_WIDTH> low = 0_d;
    ch_uint<RESULT_WIDTH> high = ~ch_uint<RESULT_WIDTH>(0_d); // 最大可能值

    // 限制high为input的近似平方根上限
    ch_uint<RESULT_WIDTH> max_sqrt = high;
    if (N < 32) {
        // 对于较小的N，我们可以使用更精确的估算
        unsigned est = 1;
        for (unsigned i = 0; i < N / 2; ++i) {
            est *= 2;
        }
        max_sqrt = make_uint<RESULT_WIDTH>(min(high, est));
    }
    high = max_sqrt;

    while (low <= high) {
        ch_uint<RESULT_WIDTH> mid = (low + high) >> 1_d;
        ch_uint<N> mid_sq = mid * mid; // 扩展到N位

        if (mid_sq == input) {
            result = mid;
            break;
        } else if (mid_sq < input) {
            result = mid; // 记录当前可能的解
            low = mid + 1_d;
        } else {
            high = mid - 1_d;
        }
    }

    return result;
}

/**
 * Fixed-Point Arithmetic Units - 定点运算单元
 *
 * 支持定点数的运算，定点格式为Qm.n，其中m位整数，n位小数
 */
template <unsigned N, unsigned Q> struct FixedPointResult {
    ch_uint<N> result;
};

template <unsigned N, unsigned Q>
FixedPointResult<N, Q> fixed_point_adder(ch_uint<N> a, ch_uint<N> b) {
    FixedPointResult<N, Q> result;
    result.result = a + b;
    return result;
}

template <unsigned N, unsigned Q>
FixedPointResult<N, Q> fixed_point_subtractor(ch_uint<N> a, ch_uint<N> b) {
    FixedPointResult<N, Q> result;
    result.result = a - b;
    return result;
}

template <unsigned N, unsigned Q>
FixedPointResult<N, Q> fixed_point_multiplier(ch_uint<N> a, ch_uint<N> b) {
    FixedPointResult<N, Q> result;
    ch_uint<2 * N> temp_result = ch_uint<2 * N>(a) * ch_uint<2 * N>(b);
    // 移位以保持Q格式
    result.result = temp_result >> make_literal(Q);
    return result;
}

template <unsigned N, unsigned Q>
FixedPointResult<N, Q> fixed_point_divider(ch_uint<N> a, ch_uint<N> b) {
    FixedPointResult<N, Q> result;
    if (b == 0_d) {
        result.result = ~ch_uint<N>(0_d); // 除零处理
        return result;
    }

    // 为了保持精度，我们将被除数左移Q位
    ch_uint<2 * N> shifted_a = ch_uint<2 * N>(a) << make_literal(Q);
    ch_uint<2 * N> temp_result = shifted_a / ch_uint<2 * N>(b);
    result.result = temp_result;
    return result;
}

} // namespace chlib

#endif // CHLIB_ARITHMETIC_ADVANCE_H