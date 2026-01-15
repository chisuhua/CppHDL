#ifndef CHLIB_LOGIC_H
#define CHLIB_LOGIC_H

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
 * 与门 - 函数式接口
 *
 * 对两个相同位宽的输入执行按位与操作
 */
template <unsigned N> ch_uint<N> and_gate(ch_uint<N> a, ch_uint<N> b) {
    return a & b;
}

/**
 * 或门 - 函数式接口
 *
 * 对两个相同位宽的输入执行按位或操作
 */
template <unsigned N> ch_uint<N> or_gate(ch_uint<N> a, ch_uint<N> b) {
    return a | b;
}

/**
 * 非门 - 函数式接口
 *
 * 对输入执行按位取反操作
 */
template <unsigned N> ch_uint<N> not_gate(ch_uint<N> a) { return ~a; }

/**
 * 异或门 - 函数式接口
 *
 * 对两个相同位宽的输入执行按位异或操作
 */
template <unsigned N> ch_uint<N> xor_gate(ch_uint<N> a, ch_uint<N> b) {
    return a ^ b;
}

/**
 * 与非门 - 函数式接口
 *
 * 对两个相同位宽的输入执行按位与非操作
 */
template <unsigned N> ch_uint<N> nand_gate(ch_uint<N> a, ch_uint<N> b) {
    return ~(a & b);
}

/**
 * 或非门 - 函数式接口
 *
 * 对两个相同位宽的输入执行按位或非操作
 */
template <unsigned N> ch_uint<N> nor_gate(ch_uint<N> a, ch_uint<N> b) {
    return ~(a | b);
}

/**
 * 同或门/异或非门 - 函数式接口
 *
 * 对两个相同位宽的输入执行按位同或操作
 */
template <unsigned N> ch_uint<N> xnor_gate(ch_uint<N> a, ch_uint<N> b) {
    return ~(a ^ b);
}

/**
 * 多输入与门 - 函数式接口
 *
 * 对多个相同位宽的输入执行按位与操作
 */
template <unsigned N>
ch_uint<N> multi_and_gate(std::initializer_list<ch_uint<N>> inputs) {
    if (inputs.size() == 0) {
        return ~ch_uint<N>(0_d); // 返回全1
    }

    ch_uint<N> result = *(inputs.begin());
    for (auto it = std::next(inputs.begin()); it != inputs.end(); ++it) {
        result = result & *it;
    }
    return result;
}

/**
 * 多输入或门 - 函数式接口
 *
 * 对多个相同位宽的输入执行按位或操作
 */
template <unsigned N>
ch_uint<N> multi_or_gate(std::initializer_list<ch_uint<N>> inputs) {
    if (inputs.size() == 0) {
        return ch_uint<N>(0_d); // 返回全0
    }

    ch_uint<N> result = *(inputs.begin());
    for (auto it = std::next(inputs.begin()); it != inputs.end(); ++it) {
        result = result | *it;
    }
    return result;
}

/**
 * 多输入异或门 - 函数式接口
 *
 * 对多个相同位宽的输入执行按位异或操作
 */
template <unsigned N>
ch_uint<N> multi_xor_gate(std::initializer_list<ch_uint<N>> inputs) {
    if (inputs.size() == 0) {
        return ch_uint<N>(0_d); // 返回全0
    }

    ch_uint<N> result = *(inputs.begin());
    for (auto it = std::next(inputs.begin()); it != inputs.end(); ++it) {
        result = result ^ *it;
    }
    return result;
}

/**
 * 单输入多路选择器 - 函数式接口
 *
 * 根据选择信号选择一个输入
 */
template <unsigned M, typename T>
T mux(std::array<T, M> inputs, ch_uint<compute_idx_width(M)> sel) {
    T result = inputs[0];

    for (unsigned i = 0; i < M; ++i) {
        ch_bool sel_matches = (sel == make_uint<compute_idx_width(M)>(i));
        T current_input = inputs[i];
        result = select(sel_matches, current_input, result);
    }

    return result;
}

/**
 * 2输入多路选择器 - 函数式接口
 *
 * 根据选择信号选择两个输入中的一个
 */
template <unsigned N>
ch_uint<N> mux2(ch_uint<N> in0, ch_uint<N> in1, ch_bool sel) {
    return select(sel, in1, in0);
}

/**
 * 4输入多路选择器 - 函数式接口
 *
 * 根据选择信号选择四个输入中的一个
 */
template <unsigned N>
ch_uint<N> mux4(ch_uint<N> in0, ch_uint<N> in1, ch_uint<N> in2, ch_uint<N> in3,
                ch_uint<2> sel) {
    ch_bool sel_bit0 = bit_select(sel, 0);
    ch_bool sel_bit1 = bit_select(sel, 1);

    ch_uint<N> upper_mux = select(sel_bit1, in3, in2);
    ch_uint<N> lower_mux = select(sel_bit1, in1, in0);

    return select(sel_bit0, upper_mux, lower_mux);
}

/**
 * 分配器 - 函数式接口
 *
 * 将输入分配到指定输出
 */
template <unsigned M, typename T>
std::array<T, M> demux(T input, ch_uint<compute_idx_width(M)> sel) {
    std::array<T, M> outputs;

    for (unsigned i = 0; i < M; ++i) {
        ch_bool sel_matches = (sel == make_uint<compute_idx_width(M)>(i));
        outputs[i] = select(sel_matches, input, 0_d);
    }

    return outputs;
}

/**
 * 奇偶校验生成器 - 函数式接口
 *
 * 生成输入的奇偶校验位
 */
template <unsigned N> ch_bool parity_gen(ch_uint<N> input) {
    ch_bool parity = bit_select(input, 0);
    for (unsigned i = 1; i < N; ++i) {
        parity = parity ^ bit_select(input, i);
    }
    return parity;
}

/**
 * 奇偶校验校验器 - 函数式接口
 *
 * 检查输入的奇偶校验是否正确
 */
template <unsigned N>
ch_bool parity_check(ch_uint<N> input, ch_bool expected_parity) {
    ch_bool actual_parity = parity_gen(input);
    return actual_parity == expected_parity;
}

/**
 * 缓冲器 - 函数式接口
 *
 * 对输入进行缓冲（输出等于输入）
 */
template <unsigned N> ch_uint<N> buffer(ch_uint<N> input) { return input; }

/**
 * 三态缓冲器 - 函数式接口
 *
 * 根据使能信号控制输出
 */
template <unsigned N>
ch_uint<N> tri_state_buffer(ch_uint<N> input, ch_bool enable) {
    return select(enable, input, ch_uint<N>(0_d));
}

} // namespace chlib

#endif // CHLIB_LOGIC_H