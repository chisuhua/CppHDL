#ifndef CHLIB_BITWISE_H
#define CHLIB_BITWISE_H

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
 * 前导零检测器 - 函数式接口
 * 
 * 计算输入值从最高位开始连续的零的个数
 */
template <unsigned N>
ch_uint<compute_bit_width(N)> leading_zero_detector(ch_uint<N> input) {
    static_assert(N > 0, "Leading zero detector must have at least 1 bit");
    static constexpr unsigned OUTPUT_WIDTH = compute_bit_width(N);
    
    ch_uint<OUTPUT_WIDTH> lz_count = 0_d;
    
    // 从最高位开始向下检查
    for (int i = N-1; i >= 0; --i) {
        ch_bool is_zero = !bit_select(input, i);
        ch_uint<OUTPUT_WIDTH> current_count = make_literal(N - i - 1);
        
        // 如果当前位为0，则更新计数
        lz_count = select(is_zero && (lz_count == make_literal(N - i - 1)), 
                         current_count + 1_d, 
                         lz_count);
    }
    
    // 如果所有位都是0，则返回N
    ch_bool all_zero = (input == 0_d);
    lz_count = select(all_zero, make_literal(N), lz_count);
    
    return lz_count;
}

/**
 * 前导一检测器 - 函数式接口
 * 
 * 计算输入值从最高位开始连续的一的个数
 */
template <unsigned N>
ch_uint<compute_bit_width(N)> leading_one_detector(ch_uint<N> input) {
    static_assert(N > 0, "Leading one detector must have at least 1 bit");
    static constexpr unsigned OUTPUT_WIDTH = compute_bit_width(N);
    
    ch_uint<OUTPUT_WIDTH> lo_count = 0_d;
    
    // 从最高位开始向下检查
    for (int i = N-1; i >= 0; --i) {
        ch_bool is_one = bit_select(input, i);
        ch_uint<OUTPUT_WIDTH> current_count = make_literal(N - i - 1);
        
        // 如果当前位为1，则更新计数
        lo_count = select(is_one && (lo_count == make_literal(N - i - 1)), 
                         current_count + 1_d, 
                         lo_count);
    }
    
    // 如果所有位都是1，则返回N
    ch_bool all_ones = (input == ~ch_uint<N>(0_d));
    lo_count = select(all_ones, make_literal(N), lo_count);
    
    return lo_count;
}

/**
 * 后导零检测器 - 函数式接口
 * 
 * 计算输入值从最低位开始连续的零的个数
 */
template <unsigned N>
ch_uint<compute_bit_width(N)> trailing_zero_detector(ch_uint<N> input) {
    static_assert(N > 0, "Trailing zero detector must have at least 1 bit");
    static constexpr unsigned OUTPUT_WIDTH = compute_bit_width(N);
    
    ch_uint<OUTPUT_WIDTH> tz_count = 0_d;
    
    // 从最低位开始向上检查
    ch_bool found_one = false;
    for (unsigned i = 0; i < N; ++i) {
        ch_bool is_zero = !bit_select(input, i);
        ch_uint<OUTPUT_WIDTH> current_count = make_literal(i);
        
        // 如果当前位为0且前面都是0，则更新计数
        tz_count = select(is_zero && !found_one, current_count + 1_d, tz_count);
        found_one = select(is_zero, found_one, true);
    }
    
    // 如果所有位都是0，则返回N
    ch_bool all_zero = (input == 0_d);
    tz_count = select(all_zero, make_literal(N), tz_count);
    
    return tz_count;
}

/**
 * 后导一检测器 - 函数式接口
 * 
 * 计算输入值从最低位开始连续的一的个数
 */
template <unsigned N>
ch_uint<compute_bit_width(N)> trailing_one_detector(ch_uint<N> input) {
    static_assert(N > 0, "Trailing one detector must have at least 1 bit");
    static constexpr unsigned OUTPUT_WIDTH = compute_bit_width(N);
    
    ch_uint<OUTPUT_WIDTH> to_count = 0_d;
    
    // 从最低位开始向上检查
    ch_bool found_zero = false;
    for (unsigned i = 0; i < N; ++i) {
        ch_bool is_one = bit_select(input, i);
        ch_uint<OUTPUT_WIDTH> current_count = make_literal(i);
        
        // 如果当前位为1且前面都是1，则更新计数
        to_count = select(is_one && !found_zero, current_count + 1_d, to_count);
        found_zero = select(is_one, found_zero, true);
    }
    
    // 如果所有位都是1，则返回N
    ch_bool all_ones = (input == ~ch_uint<N>(0_d));
    to_count = select(all_ones, make_literal(N), to_count);
    
    return to_count;
}

/**
 * Population Count (Popcount) - 函数式接口
 * 
 * 计算输入中1的个数
 */
template <unsigned N>
ch_uint<compute_bit_width(N)> population_count(ch_uint<N> input) {
    static_assert(N > 0, "Population count must have at least 1 bit");
    static constexpr unsigned OUTPUT_WIDTH = compute_bit_width(N);
    
    ch_uint<OUTPUT_WIDTH> count = 0_d;
    
    for (unsigned i = 0; i < N; ++i) {
        ch_bool bit_i = bit_select(input, i);
        ch_uint<OUTPUT_WIDTH> current_count = count + select(bit_i, 1_d, 0_d);
        count = current_count;
    }
    
    return count;
}

/**
 * 位反转 - 函数式接口
 * 
 * 反转输入的所有位的顺序
 */
template <unsigned N>
ch_uint<N> bit_reversal(ch_uint<N> input) {
    ch_uint<N> result = 0_d;
    
    for (unsigned i = 0; i < N; ++i) {
        ch_bool bit_i = bit_select(input, i);
        ch_uint<N> reversed_bit = ch_uint<N>(bit_i) << make_literal(N - 1 - i);
        result = result | reversed_bit;
    }
    
    return result;
}

/**
 * 位交换 - 函数式接口
 * 
 * 交换输入的两个位位置
 */
template <unsigned N>
ch_uint<N> bit_swap(ch_uint<N> input, unsigned pos1, unsigned pos2) {
    static_assert(N > 0, "Bit swap must have at least 1 bit");
    
    ch_bool bit1 = bit_select(input, pos1);
    ch_bool bit2 = bit_select(input, pos2);
    
    // 清除两个位置的值
    ch_uint<N> mask1 = ~(ch_uint<N>(1_d) << make_literal(pos1));
    ch_uint<N> mask2 = ~(ch_uint<N>(1_d) << make_literal(pos2));
    ch_uint<N> cleared = input & mask1 & mask2;
    
    // 设置交换后的值
    ch_uint<N> new_bit1 = ch_uint<N>(bit2) << make_literal(pos1);
    ch_uint<N> new_bit2 = ch_uint<N>(bit1) << make_literal(pos2);
    
    return cleared | new_bit1 | new_bit2;
}

/**
 * 第一个设置位检测器 - 函数式接口
 * 
 * 检测输入中第一个设置位（从最低位开始）的位置
 */
template <unsigned N>
ch_uint<compute_bit_width(N-1)> first_set_bit_detector(ch_uint<N> input) {
    static_assert(N > 0, "First set bit detector must have at least 1 bit");
    static constexpr unsigned OUTPUT_WIDTH = compute_bit_width(N-1);
    
    ch_uint<OUTPUT_WIDTH> result = make_literal(N);  // 如果没有设置位，返回N
    
    for (int i = N-1; i >= 0; --i) {
        ch_bool bit_at_i = bit_select(input, i);
        ch_uint<OUTPUT_WIDTH> current_value = make_literal(i);
        result = select(bit_at_i, current_value, result);
    }
    
    return result;
}

/**
 * 位域提取器 - 函数式接口
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

/**
 * 位域插入器 - 函数式接口
 * 
 * 将一个值插入到输入的指定位范围
 */
template <unsigned N>
ch_uint<N> bit_field_insert(ch_uint<N> input, ch_uint<N> value, unsigned start, unsigned width) {
    static_assert(N > 0, "Bit field insert must have at least 1 bit");
    
    // 创建掩码
    ch_uint<N> mask = (ch_uint<N>(1_d) << make_literal(width)) - 1_d;
    
    // 将值左移并掩码
    ch_uint<N> shifted_value = (value & mask) << make_literal(start);
    
    // 创建清除掩码
    ch_uint<N> clear_mask = ~(mask << make_literal(start));
    
    // 清除目标位并插入新值
    return (input & clear_mask) | shifted_value;
}

} // namespace chlib

#endif // CHLIB_BITWISE_H