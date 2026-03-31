#ifndef CHLIB_CONVERTER_H
#define CHLIB_CONVERTER_H

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
 * OneHot 编码转换器
 */
template <unsigned N>
ch_uint<N> binary_to_onehot(ch_uint<compute_idx_width(N)> input) {
    static_assert(N > 0, "Binary to onehot converter must have at least 1 bit");
    static constexpr unsigned INPUT_WIDTH = compute_idx_width(N);

    ch_uint<N> result = 0_d;
    for (unsigned i = 0; i < N; ++i) {
        ch_bool idx_matches = (input == make_uint<INPUT_WIDTH>(i));
        ch_uint<N> one_hot_val = ch_uint<N>(1_d) << make_uint<N>(i);
        result = select(idx_matches, one_hot_val, result);
    }
    return result;
}

/**
 * OneHot 转二进制
 */
template <unsigned N>
ch_uint<compute_idx_width(N)> onehot_to_binary(ch_uint<N> input) {
    static_assert(N > 0, "Onehot to binary converter must have at least 1 bit");
    static constexpr unsigned OUTPUT_WIDTH = compute_idx_width(N);

    ch_uint<OUTPUT_WIDTH> result = 0_d;
    for (unsigned i = 0; i < N; ++i) {
        ch_bool bit_at_i = bit_select(input, i);
        ch_uint<OUTPUT_WIDTH> current_value = make_uint<OUTPUT_WIDTH>(i);
        result = select(bit_at_i, current_value, result);
    }
    return result;
}

/**
 * BCD 到二进制转换器
 * 例如：BCD 0x12 (0001 0010) → 二进制 18
 */
template <unsigned N> ch_uint<N> bcd_to_binary(ch_uint<N> input) {
    static_assert(N > 0, "BCD to binary converter must have at least 1 bit");
    
    if constexpr (N >= 8) {
        ch_uint<N> tens = input >> ch_uint<N>(4_d);
        ch_uint<N> ones = input & ch_uint<N>(15_d);  // 0x0F = 15
        return (tens * ch_uint<N>(10_d)) + ones;
    } else {
        return input & ch_uint<N>(15_d);
    }
}

/**
 * 二进制到 BCD 转换器 (简化版)
 */
template <unsigned N> ch_uint<N> binary_to_bcd(ch_uint<N> input) {
    static_assert(N > 0, "Binary to BCD converter must have at least 1 bit");
    
    // 简化实现：处理 0-99 范围内的值
    if constexpr (N >= 8) {
        ch_uint<4> tens = input / ch_uint<N>(10_d);
        ch_uint<4> ones = input % ch_uint<N>(10_d);
        return (tens << ch_uint<N>(4_d)) | ones;
    } else {
        return input;
    }
}

/**
 * 格雷码到二进制转换器
 */
template <unsigned N> ch_uint<N> gray_to_binary(ch_uint<N> input) {
    static_assert(N > 0, "Gray to binary converter must have at least 1 bit");

    if constexpr (N == 1) {
        return input;
    } else {
        ch_bool msb = bit_select(input, N - 1);
        ch_uint<N> result =
            select(msb, (1_d << make_uint<compute_bit_width(N)>(N - 1)), 0_d);

        [&]<std::size_t... I>(std::index_sequence<I...>) {
            auto compute_next_bit = [&](unsigned idx) {
                ch_bool input_bit = bit_select(input, idx);
                ch_bool higher_result_bit = bit_select(result, idx + 1);
                ch_bool binary_bit = input_bit ^ higher_result_bit;

                result = select(
                    binary_bit,
                    result | (1_d << make_uint<compute_bit_width(N)>(idx)),
                    result);
            };
            ((compute_next_bit(N - 2 - I)), ...);
        }(std::make_index_sequence<N - 1>{});

        return result;
    }
}

/**
 * 二进制到格雷码转换器
 */
template <unsigned N> ch_uint<N> binary_to_gray(ch_uint<N> input) {
    static_assert(N > 0, "Binary to gray converter must have at least 1 bit");
    return (input >> ch_uint<N>(1_d)) ^ input;
}

} // namespace chlib

#endif // CHLIB_CONVERTER_H
