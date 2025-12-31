#ifndef CHLIB_BITWISE_H
#define CHLIB_BITWISE_H

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
    for (int i = N - 1; i >= 0; --i) {
        ch_bool is_zero = !bit_select(input, static_cast<unsigned>(i));
        ch_uint<OUTPUT_WIDTH> current_count =
            make_uint<OUTPUT_WIDTH>(N - i - 1);

        // 如果当前位为0，则更新计数
        lz_count =
            select(is_zero && (lz_count == make_uint<OUTPUT_WIDTH>(N - i - 1)),
                   current_count + 1_d, lz_count);
    }

    // 如果所有位都是0，则返回N
    ch_bool all_zero = (input == 0_d);
    lz_count = select(all_zero, make_uint<OUTPUT_WIDTH>(N), lz_count);

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
    for (int i = N - 1; i >= 0; --i) {
        ch_bool is_one = bit_select(input, i);
        ch_uint<OUTPUT_WIDTH> current_count =
            make_uint<OUTPUT_WIDTH>(N - i - 1);

        // 如果当前位为1，则更新计数
        lo_count =
            select(is_one && (lo_count == make_uint<OUTPUT_WIDTH>(N - i - 1)),
                   current_count + 1_d, lo_count);
    }

    // 如果所有位都是1，则返回N
    ch_bool all_ones = (input == ~ch_uint<N>(0_d));
    lo_count = select(all_ones, make_uint<OUTPUT_WIDTH>(N), lo_count);

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
        ch_uint<OUTPUT_WIDTH> current_count = make_uint<OUTPUT_WIDTH>(i);

        // 如果当前位为0且前面都是0，则更新计数
        tz_count = select(is_zero && !found_one, current_count + 1_d, tz_count);
        found_one = select(is_zero, found_one, 1_b);
    }

    // 如果所有位都是0，则返回N
    ch_bool all_zero = (input == 0_d);
    tz_count = select(all_zero, make_uint<OUTPUT_WIDTH>(N), tz_count);

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
        ch_uint<OUTPUT_WIDTH> current_count = make_uint<OUTPUT_WIDTH>(i);

        // 如果当前位为1且前面都是1，则更新计数
        to_count = select(is_one && !found_zero, current_count + 1_d, to_count);
        found_zero = select(is_one, found_zero, 1_b);
    }

    // 如果所有位都是1，则返回N
    ch_bool all_ones = (input == ~ch_uint<N>(0_d));
    to_count = select(all_ones, make_uint<OUTPUT_WIDTH>(N), to_count);

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
template <unsigned N> ch_uint<N> bit_reversal(ch_uint<N> input) {
    ch_uint<N> result = 0_d;

    for (unsigned i = 0; i < N; ++i) {
        ch_bool bit_i = bit_select(input, i);
        // 使用constexpr计算位宽，避免在模板参数中使用运行时变量
        constexpr unsigned shift_amount = compute_bit_width(N - 1);
        ch_uint<N> reversed_bit = bit_i << make_uint<shift_amount>(N - 1 - i);
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
    static constexpr unsigned BIT_WIDTH = compute_bit_width(N - 1);

    ch_bool bit1 = bit_select(input, pos1);
    ch_bool bit2 = bit_select(input, pos2);

    // 清除两个位置的值
    ch_uint<N> mask1 = ~(ch_uint<N>(1_d) << make_uint<BIT_WIDTH>(pos1));
    ch_uint<N> mask2 = ~(ch_uint<N>(1_d) << make_uint<BIT_WIDTH>(pos2));

    // 先清除两个位置的位，然后设置交换后的值
    ch_uint<N> cleared = input & mask1 & mask2;

    // 设置交换后的值，将ch_bool转换为ch_uint<1>后再进行零扩展和位移
    ch_uint<N> new_bit1 = zext<N>(ch_uint<1>(bit1))
                          << make_uint<BIT_WIDTH>(pos1);
    ch_uint<N> new_bit2 = zext<N>(ch_uint<1>(bit2))
                          << make_uint<BIT_WIDTH>(pos2);

    return cleared | new_bit1 | new_bit2;
}

/**
 * 第一个设置位检测器 - 函数式接口
 *
 * 检测输入中第一个设置位（从最低位开始）的位置
 */
template <unsigned N>
ch_uint<compute_bit_width(N - 1)> first_set_bit_detector(ch_uint<N> input) {
    static_assert(N > 0, "First set bit detector must have at least 1 bit");
    static constexpr unsigned OUTPUT_WIDTH = compute_bit_width(N - 1);

    ch_uint<OUTPUT_WIDTH> result =
        make_uint<OUTPUT_WIDTH>(N); // 如果没有设置位，返回N

    for (int i = N - 1; i >= 0; --i) {
        ch_uint<1> bit_at_i = bit_select(input, i);
        ch_uint<OUTPUT_WIDTH> current_value = make_uint<OUTPUT_WIDTH>(i);
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

    // 使用固定宽度的运行时字面量来处理运行时值
    ch_uint<N> mask_val = 1_d;
    for (unsigned i = 1; i < width; ++i) {
        mask_val = (mask_val << 1_d) + 1_d;
    }

    // 右移并掩码
    ch_uint<N> result = input;
    for (unsigned i = 0; i < start; ++i) {
        result = result >> 1_d;
    }

    return result & mask_val;
}

/**
 * 位域插入器 - 函数式接口
 *
 * 将一个值插入到输入的指定位范围
 */
template <unsigned N>
ch_uint<N> bit_field_insert(ch_uint<N> input, ch_uint<N> value, unsigned start,
                            unsigned width) {
    static_assert(N > 0, "Bit field insert must have at least 1 bit");

    // 创建掩码
    ch_uint<N> mask_val = 1_d;
    for (unsigned i = 1; i < width; ++i) {
        mask_val = (mask_val << 1_d) + 1_d;
    }

    // 将掩码左移到正确位置
    ch_uint<N> shifted_mask = mask_val;
    for (unsigned i = 0; i < start; ++i) {
        shifted_mask = shifted_mask << 1_d;
    }

    // 将值左移到正确位置
    ch_uint<N> shifted_value = value & mask_val;
    for (unsigned i = 0; i < start; ++i) {
        shifted_value = shifted_value << 1_d;
    }

    // 清除目标位并插入新值
    return (input & ~shifted_mask) | shifted_value;
}

} // namespace chlib

#endif // CHLIB_BITWISE_H