#ifndef CHLIB_COMBINATIONAL_H
#define CHLIB_COMBINATIONAL_H

#include "ch.hpp"
#include "chlib/logic.h"
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
 * 优先级编码器 - 函数式接口
 *
 * 将输入的one-hot编码转换为二进制索引，优先处理低位
 */
template <unsigned N>
ch_uint<compute_bit_width(N - 1)> priority_encoder(ch_uint<N> input) {
    static_assert(N > 0, "Priority encoder must have at least 1 bit");
    static constexpr unsigned OUTPUT_WIDTH = compute_bit_width(N - 1);

    ch_uint<OUTPUT_WIDTH> result = 0_d;

    // 从低位到高位检查，优先处理低位
    for (unsigned i = 0; i < N; ++i) {
        ch_bool bit_at_i = bit_select(input, i);
        ch_uint<OUTPUT_WIDTH> current_value = make_uint<OUTPUT_WIDTH>(i);
        result = select(bit_at_i, current_value, result);
    }

    return result;
}

/**
 * 二进制编码器 - 函数式接口
 *
 * 将输入的one-hot编码转换为二进制索引
 */
template <unsigned N>
ch_uint<compute_bit_width(N - 1)> binary_encoder(ch_uint<N> input) {
    static_assert(N > 0, "Binary encoder must have at least 1 bit");
    static constexpr unsigned OUTPUT_WIDTH = compute_bit_width(N - 1);

    ch_uint<OUTPUT_WIDTH> result = 0_d;

    for (unsigned i = 0; i < N; ++i) {
        ch_bool bit_at_i = bit_select(input, i);
        ch_uint<OUTPUT_WIDTH> current_value = make_uint<OUTPUT_WIDTH>(i);
        result = select(bit_at_i, current_value, result);
    }

    return result;
}

/**
 * 二进制解码器 - 函数式接口
 *
 * 将输入的二进制索引转换为one-hot编码
 */
template <unsigned N>
ch_uint<N> binary_decoder(ch_uint<compute_bit_width(N - 1)> input) {
    static_assert(N > 0, "Binary decoder must have at least 1 bit");
    static constexpr unsigned INPUT_WIDTH = compute_bit_width(N - 1);

    ch_uint<N> result = 0_d;

    for (unsigned i = 0; i < N; ++i) {
        ch_bool idx_matches = (input == make_uint<INPUT_WIDTH>(i));
        ch_uint<N> one_hot_val = ch_uint<N>(1_d) << make_uint<INPUT_WIDTH>(i);
        result = select(idx_matches, one_hot_val, result);
    }

    return result;
}

// demux函数已经在chlib/logic.h中定义，不需要重复定义

/**
 * 奇偶校验生成器 - 函数式接口
 *
 * 生成输入的奇偶校验位（奇校验）
 */
template <unsigned N> ch_bool odd_parity_gen(ch_uint<N> input) {
    ch_bool parity = bit_select(input, 0);
    for (unsigned i = 1; i < N; ++i) {
        parity = parity ^ bit_select(input, i);
    }
    // 为奇校验，翻转结果
    return !parity;
}

/**
 * 奇偶校验生成器 - 函数式接口
 *
 * 生成输入的奇偶校验位（偶校验）
 */
template <unsigned N> ch_bool even_parity_gen(ch_uint<N> input) {
    ch_bool parity = bit_select(input, 0);
    for (unsigned i = 1; i < N; ++i) {
        parity = parity ^ bit_select(input, i);
    }
    return parity;
}

/**
 * 海明码生成器 - 函数式接口
 *
 * 生成海明码的校验位
 */
template <unsigned N>
ch_uint<compute_bit_width(N + compute_bit_width(N) - 1)>
hamming_gen(ch_uint<N> input) {
    // 简化版海明码生成，实际应用中需要更复杂的算法
    // 这里只是示例实现
    static constexpr unsigned TOTAL_BITS =
        N + compute_bit_width(N + compute_bit_width(N));

    ch_bool parity = bit_select(input, 0);
    for (unsigned i = 1; i < N; ++i) {
        parity = parity ^ bit_select(input, i);
    }

    return ch_uint<compute_bit_width(TOTAL_BITS)>(parity);
}

/**
 * 全加器 - 函数式接口
 *
 * 实现一位全加器功能
 */
struct FullAdderResult {
    ch_bool sum;
    ch_bool carry_out;
};

inline FullAdderResult full_adder(ch_bool a, ch_bool b, ch_bool carry_in) {
    FullAdderResult result;

    // 计算和
    result.sum = a ^ b ^ carry_in;

    // 计算进位
    result.carry_out = (a & b) | (carry_in & (a ^ b));

    return result;
}

/**
 * 行波进位加法器 - 函数式接口
 *
 * 使用全加器构建多位加法器
 */
template <unsigned N> struct RippleCarryAdderResult {
    ch_uint<N> sum;
    ch_bool carry_out;
};

template <unsigned N>
RippleCarryAdderResult<N> ripple_carry_adder(ch_uint<N> a, ch_uint<N> b,
                                             ch_bool carry_in = false) {
    RippleCarryAdderResult<N> result;

    ch_bool carry = carry_in;
    ch_uint<N> sum = 0_d;

    for (unsigned i = 0; i < N; ++i) {
        ch_bool a_bit = bit_select(a, i);
        ch_bool b_bit = bit_select(b, i);

        FullAdderResult fa_result = full_adder(a_bit, b_bit, carry);

        // 设置当前位的和
        ch_uint<N> sum_bit = fa_result.sum ? make_uint<N>(1) : make_uint<N>(0);
        sum = sum | (sum_bit << make_uint<compute_bit_width(N-1)>(i));

        // 更新进位
        carry = fa_result.carry_out;
    }

    result.sum = sum;
    result.carry_out = carry;

    return result;
}

/**
 * 比较器 - 函数式接口
 *
 * 比较两个数的大小关系
 */
template <unsigned N> struct ComparatorResult {
    ch_bool greater;
    ch_bool equal;
    ch_bool less;
};

template <unsigned N>
ComparatorResult<N> comparator(ch_uint<N> a, ch_uint<N> b) {
    ComparatorResult<N> result;

    // 初始化结果
    result.greater = ch_bool(false);
    result.equal = ch_bool(true);
    result.less = ch_bool(false);

    // 从高位到低位比较
    for (int i = N - 1; i >= 0; --i) {
        ch_bool a_bit = bit_select(a, i);
        ch_bool b_bit = bit_select(b, i);

        // 如果当前位不同
        ch_bool a_gt_b = a_bit && !b_bit;
        ch_bool a_lt_b = !a_bit && b_bit;

        // 更新比较结果
        ch_bool update = result.equal; // 只有在前面的位相等时才更新

        ch_bool new_greater = select(a_gt_b, ch_bool(true), result.greater);
        result.greater = select(update, new_greater, result.greater);

        ch_bool new_less = select(a_lt_b, ch_bool(true), result.less);
        result.less = select(update, new_less, result.less);

        ch_bool new_equal =
            select(a_gt_b || a_lt_b, ch_bool(false), result.equal);
        result.equal = select(update, new_equal, result.equal);
    }

    return result;
}

/**
 * 多路选择器 - 函数式接口
 *
 * 根据选择信号选择多个输入中的一个
 */
template <unsigned N, unsigned M>
ch_uint<N> multiplexer(const std::array<ch_uint<N>, M> &inputs,
                       ch_uint<compute_bit_width(M - 1)> sel) {
    ch_uint<N> result = inputs[0];

    for (unsigned i = 0; i < M; ++i) {
        ch_bool sel_matches = (sel == make_uint<compute_bit_width(M-1)>(i));
        ch_uint<N> current_input = inputs[i];
        result = select(sel_matches, current_input, result);
    }

    return result;
}

/**
 * 多路选择器 - 函数式接口（简化版）
 *
 * 8选1多路选择器
 */
template <unsigned N>
ch_uint<N> mux8to1(ch_uint<N> in0, ch_uint<N> in1, ch_uint<N> in2,
                   ch_uint<N> in3, ch_uint<N> in4, ch_uint<N> in5,
                   ch_uint<N> in6, ch_uint<N> in7, ch_uint<3> sel) {
    std::array<ch_uint<N>, 8> inputs = {in0, in1, in2, in3, in4, in5, in6, in7};
    return multiplexer<N, 8>(inputs, sel);
}

/**
 * 多路选择器 - 函数式接口（简化版）
 *
 * 16选1多路选择器
 */
template <unsigned N>
ch_uint<N> mux16to1(ch_uint<N> in0, ch_uint<N> in1, ch_uint<N> in2,
                    ch_uint<N> in3, ch_uint<N> in4, ch_uint<N> in5,
                    ch_uint<N> in6, ch_uint<N> in7, ch_uint<N> in8,
                    ch_uint<N> in9, ch_uint<N> in10, ch_uint<N> in11,
                    ch_uint<N> in12, ch_uint<N> in13, ch_uint<N> in14,
                    ch_uint<N> in15, ch_uint<4> sel) {
    std::array<ch_uint<N>, 16> inputs = {in0,  in1,  in2,  in3, in4,  in5,
                                         in6,  in7,  in8,  in9, in10, in11,
                                         in12, in13, in14, in15};
    return multiplexer<N, 16>(inputs, sel);
}

/**
 * 数值比较器 - 函数式接口
 *
 * 检查输入是否等于特定值
 */
template <unsigned N> ch_bool equals(ch_uint<N> input, unsigned value) {
    ch_uint<N> literal_value = make_literal(value, N);
    return input == literal_value;
}

/**
 * 范围检查器 - 函数式接口
 *
 * 检查输入是否在指定范围内
 */
template <unsigned N>
ch_bool in_range(ch_uint<N> input, unsigned min_val, unsigned max_val) {
    ch_uint<N> min_literal = make_literal(min_val, N);
    ch_uint<N> max_literal = make_literal(max_val, N);
    ch_bool greater_equal_min = input >= min_literal;
    ch_bool less_equal_max = input <= max_literal;
    return greater_equal_min && less_equal_max;
}

} // namespace chlib

#endif // CHLIB_COMBINATIONAL_H