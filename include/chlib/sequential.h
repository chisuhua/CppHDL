#ifndef CHLIB_SEQUENTIAL_H
#define CHLIB_SEQUENTIAL_H

#include "ch.hpp"
#include "component.h"
#include "core/bool.h"
#include "core/literal.h"
#include "core/operators.h"
#include "core/operators_runtime.h"
#include "core/reg.h"
#include "core/uint.h"
#include <cassert>

using namespace ch::core;

namespace chlib {

/**
 * 时序寄存器 - 函数式接口
 *
 * 基本的带有时钟和复位的寄存器
 */
template <unsigned N>
ch_uint<N> register_(ch_uint<N> d, ch_bool clk, ch_bool rst, ch_bool en,
                     const std::string &name = "register") {
    ch_reg<ch_uint<N>> reg(0_d, name);

    // 根据复位和使能信号更新寄存器
    reg->next = select(rst, ch_uint<N>(0_d), select(en, d, reg));

    return reg;
}

/**
 * D触发器 - 函数式接口
 *
 * 基本的D触发器，带使能和异步复位
 */
template <unsigned N>
ch_uint<N> dff_(ch_uint<N> d, ch_bool clk, ch_bool rst, ch_bool en,
                const std::string &name = "dff") {
    ch_reg<ch_uint<N>> reg(0_d, name);

    reg->next = select(rst, ch_uint<N>(0_d), select(en, d, reg));

    return reg;
}

/**
 * 二进制计数器 - 函数式接口
 *
 * 标准二进制计数器，递增计数
 */
template <unsigned N>
ch_uint<N> binary_counter(ch_bool clk, ch_bool rst, ch_bool en,
                          const std::string &name = "binary_counter") {
    ch_reg<ch_uint<N>> counter(0_d, name);

    ch_uint<N> next_value = counter + 1_d;

    counter->next =
        select(rst, ch_uint<N>(0_d), select(en, next_value, counter));

    return counter;
}

/**
 * BCD计数器 - 函数式接口
 *
 * BCD (Binary-Coded Decimal) 计数器，计数从0000到1001 (0-9)
 */
template <unsigned N> struct BCDCounterResult {
    ch_uint<N> count;
    ch_bool carry;
};

template <unsigned N>
BCDCounterResult<N> bcd_counter(ch_bool clk, ch_bool rst, ch_bool en,
                                const std::string &name = "bcd_counter") {
    static_assert(N >= 4, "BCD counter needs at least 4 bits");

    ch_reg<ch_uint<N>> counter(0_d, name);

    ch_uint<N> next_value = select(counter == 9_d, 0_d, counter + 1_d);
    ch_bool has_carry = (counter == 9_d) && en;

    counter->next =
        select(rst, ch_uint<N>(0_d), select(en, next_value, counter));

    BCDCounterResult<N> result;
    result.count = counter;
    result.carry = has_carry;

    return result;
}

/**
 * 格雷码计数器 - 函数式接口
 *
 * 格雷码计数器，相邻数值之间仅有一位发生变化
 */
template <unsigned N>
ch_uint<N> gray_counter(ch_bool clk, ch_bool rst, ch_bool en,
                        const std::string &name = "gray_counter") {
    ch_reg<ch_uint<N>> binary_counter(0_d, name + "_bin");
    ch_uint<N> gray_code;

    // 先实现二进制计数器
    ch_uint<N> next_binary =
        select(binary_counter == (1_d << N_d) - 1_d, // 检查是否达到最大值
               0_d, binary_counter + 1_d);

    binary_counter->next =
        select(rst, ch_uint<N>(0_d), select(en, next_binary, binary_counter));

    // 将二进制转换为格雷码: G(i) = B(i) XOR B(i+1)
    gray_code = binary_counter ^ (binary_counter >> 1_d);

    return gray_code;
}

/**
 * 计数器模块 - 函数式接口
 *
 * 可配置的计数器，支持向上/向下计数，带复位和使能
 */
template <unsigned N>
ch_uint<N> counter(ch_bool clk, ch_bool rst, ch_bool en, ch_bool up_down,
                   const std::string &name = "counter") {
    ch_reg<ch_uint<N>> counter(0_d, name);

    ch_uint<N> next_value = select(up_down, counter + 1_d, counter - 1_d);

    counter->next =
        select(rst, ch_uint<N>(0_d), select(en, next_value, counter));

    return counter;
}

/**
 * 约翰逊计数器 - 函数式接口
 *
 * 循环移位计数器，产生特定序列
 */
template <unsigned N>
ch_uint<N> johnson_counter(ch_bool clk, ch_bool rst, ch_bool en,
                           const std::string &name = "johnson_counter") {
    ch_reg<ch_uint<N>> counter(0_d, name);

    // 约翰逊计数器：移位寄存器，将反相的最高位反馈到最低位
    ch_bool feedback = !bit_select(counter, N - 1);
    ch_uint<N> shifted = (counter << 1_d) | ch_uint<N>(feedback);

    counter->next = select(rst, ch_uint<N>(0_d), select(en, shifted, counter));

    return counter;
}

/**
 * 环形计数器 - 函数式接口
 *
 * 循环移位计数器，只有一个位为1
 */
template <unsigned N>
ch_uint<N> ring_counter(ch_bool clk, ch_bool rst, ch_bool en,
                        const std::string &name = "ring_counter") {
    ch_reg<ch_uint<N>> counter(1_d, name); // 初始化为000...001

    // 环形移位：将最高位移到最低位
    ch_bool msb = bit_select(counter, N - 1);
    ch_uint<N> shifted = (counter << 1_d) | ch_uint<N>(msb);

    counter->next = select(rst, ch_uint<N>(1_d), select(en, shifted, counter));

    return counter;
}

/**
 * 移位寄存器 - 函数式接口
 *
 * 可配置的移位寄存器，支持左右移位
 */
template <unsigned N> struct ShiftRegisterResult {
    ch_uint<N> out;
    ch_bool serial_out;
};

template <unsigned N>
ShiftRegisterResult<N>
shift_register(ch_bool clk, ch_bool rst, ch_bool en, ch_bool shift_dir,
               ch_uint<N> parallel_in, ch_bool load,
               const std::string &name = "shift_register") {

    ch_reg<ch_uint<N>> reg(0_d, name);

    // 左移或右移
    ch_uint<N> shifted = select(shift_dir, reg << 1_d, reg >> 1_d);

    // 串行输出是移出的位
    ch_bool serial_out =
        select(shift_dir, bit_select(reg, N - 1), // 左移时输出最高位
               bit_select(reg, 0));               // 右移时输出最低位

    // 决定下一个值
    ch_uint<N> next_value = select(load, parallel_in, shifted);

    reg->next = select(rst, ch_uint<N>(0_d), select(en, next_value, reg));

    ShiftRegisterResult<N> result;
    result.out = reg;
    result.serial_out = serial_out;

    return result;
}

/**
 * 边沿检测器 - 函数式接口
 *
 * 检测输入信号的上升沿或下降沿
 */
struct EdgeDetectorResult {
    ch_bool pos_edge;
    ch_bool neg_edge;
    ch_bool any_edge;
};

inline EdgeDetectorResult
edge_detector(ch_bool clk, ch_bool rst, ch_bool signal,
              const std::string &name = "edge_detector") {
    ch_reg<ch_bool> prev_signal(false, name + "_prev");

    prev_signal->next = select(rst, false, signal);

    EdgeDetectorResult result;
    result.pos_edge = signal && !prev_signal;
    result.neg_edge = !signal && prev_signal;
    result.any_edge = result.pos_edge || result.neg_edge;

    return result;
}

/**
 * 可重配置计数器 - 函数式接口
 *
 * 支持多种计数模式的计数器
 */
template <unsigned N> struct ConfigurableCounterResult {
    ch_uint<N> count;
    ch_bool overflow;
};

template <unsigned N>
ConfigurableCounterResult<N>
configurable_counter(ch_bool clk, ch_bool rst, ch_bool en, ch_uint<2> mode,
                     ch_uint<N> max_val,
                     const std::string &name = "configurable_counter") {

    ch_reg<ch_uint<N>> counter(0_d, name);

    ch_uint<N> next_value = counter;
    ch_bool is_overflow = false;

    // 根据模式决定下一个值
    ch_bool is_max = (counter == max_val);
    ch_bool is_zero = (counter == 0_d);

    // 递增模式
    ch_uint<N> inc_value = select(is_max, 0_d, counter + 1_d);
    ch_bool inc_overflow = select(is_max, true, false);

    // 递减模式
    ch_uint<N> dec_value = select(is_zero, max_val, counter - 1_d);
    ch_bool dec_overflow = select(is_zero, true, false);

    // 递增到最大值重置模式
    ch_uint<N> up_mod_value = select(is_max, 0_d, counter + 1_d);
    ch_bool up_mod_overflow = select(is_max, true, false);

    // 递减到0重置模式
    ch_uint<N> down_mod_value = select(is_zero, max_val, counter - 1_d);
    ch_bool down_mod_overflow = select(is_zero, true, false);

    // 根据模式选择
    next_value =
        select(mode == 0_d, inc_value,
               select(mode == 1_d, dec_value,
                      select(mode == 2_d, up_mod_value, down_mod_value)));

    is_overflow =
        select(mode == 0_d, inc_overflow,
               select(mode == 1_d, dec_overflow,
                      select(mode == 2_d, up_mod_overflow, down_mod_overflow)));

    counter->next =
        select(rst, ch_uint<N>(0_d), select(en, next_value, counter));

    ConfigurableCounterResult<N> result;
    result.count = counter;
    result.overflow = is_overflow;

    return result;
}

} // namespace chlib

#endif // CHLIB_SEQUENTIAL_H