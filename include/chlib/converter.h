#ifndef CHLIB_CONVERTER_H
#define CHLIB_CONVERTER_H

#include "ch.hpp"
#include "component.h"
#include "core/bool.h"
#include "core/literal.h"
#include "core/operators.h"
#include "core/operators_runtime.h"
#include "core/uint.h"
#include "chlib/logic.h"
#include <cassert>

using namespace ch::core;

namespace chlib {

/**
 * OneHot编码转换器 - 函数式接口
 * 
 * 将二进制索引转换为OneHot编码
 */
template <unsigned N>
ch_uint<N> binary_to_onehot(ch_uint<compute_bit_width(N-1)> input) {
    static_assert(N > 0, "Binary to onehot converter must have at least 1 bit");
    static constexpr unsigned INPUT_WIDTH = compute_bit_width(N-1);
    
    ch_uint<N> result = 0_d;
    
    for (unsigned i = 0; i < N; ++i) {
        ch_bool idx_matches = (input == make_literal(i));
        ch_uint<N> one_hot_val = ch_uint<N>(1_d) << make_literal(i);
        result = select(idx_matches, one_hot_val, result);
    }
    
    return result;
}

/**
 * OneHot编码转换器 - 函数式接口
 * 
 * 将OneHot编码转换为二进制索引
 */
template <unsigned N>
ch_uint<compute_bit_width(N-1)> onehot_to_binary(ch_uint<N> input) {
    static_assert(N > 0, "Onehot to binary converter must have at least 1 bit");
    static constexpr unsigned OUTPUT_WIDTH = compute_bit_width(N-1);
    
    ch_uint<OUTPUT_WIDTH> result = 0_d;
    
    // 从低位到高位检查，优先处理低位
    for (unsigned i = 0; i < N; ++i) {
        ch_bool bit_at_i = bit_select(input, i);
        ch_uint<OUTPUT_WIDTH> current_value = make_literal(i);
        result = select(bit_at_i, current_value, result);
    }
    
    return result;
}

/**
 * BCD到二进制转换器 - 函数式接口
 * 
 * 将BCD码转换为二进制码
 */
template <unsigned N>
ch_uint<N> bcd_to_binary(ch_uint<N> input) {
    static_assert(N > 0, "BCD to binary converter must have at least 1 bit");
    
    // BCD码是以4位为一组表示十进制数的编码方式
    // 每4位BCD码表示0-9的十进制数
    ch_uint<N> result = 0_d;
    
    // 计算BCD组数
    constexpr unsigned bcd_groups = (N + 3) / 4;  // 每4位一组
    
    for (unsigned i = 0; i < bcd_groups; ++i) {
        // 提取每组4位BCD码
        ch_uint<4> bcd_digit = bit_field_extract<4>(input, i * 4, 4);
        
        // 验证BCD码的有效性（只能是0-9）
        ch_bool is_valid = bcd_digit <= 9_d;
        ch_uint<N> digit_value = select(is_valid, ch_uint<N>(bcd_digit), 0_d);
        
        // 计算该位的权重（10^i）
        unsigned weight = 1;
        for (unsigned j = 0; j < i; ++j) {
            weight *= 10;
        }
        
        // 累加到结果中
        result = result + (digit_value * make_literal(weight));
    }
    
    return result;
}

/**
 * 二进制到BCD转换器 - 函数式接口
 * 
 * 将二进制码转换为BCD码
 */
template <unsigned N>
ch_uint<N> binary_to_bcd(ch_uint<N> input) {
    static_assert(N > 0, "Binary to BCD converter must have at least 1 bit");
    
    // 使用Double Dabble算法进行二进制到BCD的转换
    // 这里使用一个简化的实现，适用于较小的数值
    ch_uint<N> result = 0_d;
    
    // 计算BCD组数
    constexpr unsigned bcd_groups = (N + 3) / 4;  // 每4位一组
    
    // 将输入转换为整数以便处理
    unsigned input_val = input.value();
    
    // 逐位处理输入值并转换为BCD
    for (unsigned i = 0; i < bcd_groups; ++i) {
        ch_uint<4> digit = make_literal(input_val % 10);
        result = result | (ch_uint<N>(digit) << make_literal(i * 4));
        input_val /= 10;
        
        if (input_val == 0) break;  // 如果已经处理完所有数字，则退出
    }
    
    return result;
}

/**
 * 格雷码到二进制转换器 - 函数式接口
 * 
 * 将格雷码转换为二进制码
 */
template <unsigned N>
ch_uint<N> gray_to_binary(ch_uint<N> input) {
    static_assert(N > 0, "Gray to binary converter must have at least 1 bit");
    
    ch_uint<N> result = 0_d;
    
    // 格雷码到二进制的转换：result[i] = input[i] XOR result[i+1]
    // 最高位保持不变，然后逐位异或
    for (int i = N-1; i >= 0; --i) {
        ch_bool bit = bit_select(input, i);
        
        if (i == N-1) {
            // 最高位保持不变
            result = result | (ch_uint<N>(bit) << make_literal(i));
        } else {
            // 其他位是输入位与结果高位的异或
            ch_bool result_higher_bit = bit_select(result, i+1);
            ch_bool binary_bit = bit ^ result_higher_bit;
            result = result | (ch_uint<N>(binary_bit) << make_literal(i));
        }
    }
    
    return result;
}

/**
 * 二进制到格雷码转换器 - 函数式接口
 * 
 * 将二进制码转换为格雷码
 */
template <unsigned N>
ch_uint<N> binary_to_gray(ch_uint<N> input) {
    static_assert(N > 0, "Binary to gray converter must have at least 1 bit");
    
    // 格雷码的计算公式: G(i) = B(i) XOR B(i+1)
    // 其中G(i)是格雷码的第i位，B(i)是二进制码的第i位
    ch_uint<N> shifted = input >> 1_d;
    return input ^ shifted;
}

/**
 * 辅助函数：位域提取
 * 
 * 从输入中提取指定位范围的值
 */
template <unsigned N>
ch_uint<N> bit_field_extract(ch_uint<N> input, unsigned start, unsigned width) {
    static_assert(N > 0, "Bit field extract must have at least 1 bit");
    
    // 创建掩码
    ch_uint<N> mask = (ch_uint<N>(1_d) << make_literal(width)) - 1_d;
    
    // 右移并掩码
    return (input >> make_literal(start)) & mask;
}

} // namespace chlib

#endif // CHLIB_CONVERTER_H