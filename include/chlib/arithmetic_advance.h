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
#include <utility>

using namespace ch::core;

namespace chlib {

/**
 * Carry Look-Ahead Adder Result - 超前进位加法器结果
 */
template <unsigned N> struct CLAResult {
    ch_uint<N> sum;
    ch_bool carry_out;
};

// 计算第pos位的进位值
template <unsigned N, unsigned POS>
constexpr ch_bool compute_carry_at_position(ch_uint<N> g, ch_uint<N> p,
                                            ch_bool carry_in) {
    if constexpr (POS == 0) {
        return carry_in;
    } else {
        // C_pos = G_pos + P_pos*G_{pos-1} + P_pos*P_{pos-1}*G_{pos-2} + ... +
        // P_pos*...*P_1*G_0 + P_pos*...*P_0*Cin
        ch_bool result = bit_select<POS>(g); // 从G_pos开始

        // 使用循环展开替代复杂的参数包展开，避免编译器内部错误
        if constexpr (POS > 0) {
            for (unsigned j = 0; j < POS; j++) {
                ch_bool term = bit_select<POS>(p); // 开始为 P_pos

                // 乘以 P_{pos-1}, P_{pos-2}, ..., P_{j+1}
                for (unsigned k = j + 1; k < POS; k++) {
                    term = term & bit_select(p, k);
                }

                // 乘以 G_j
                term = term & bit_select(g, j);
                result = result | term;
            }
        }

        // 添加 P_pos*...*P_0*Cin 项
        ch_bool propagate_all = bit_select<POS>(p);
        for (unsigned j = 0; j < POS; j++) {
            propagate_all = propagate_all & bit_select(p, j);
        }
        result = result | (propagate_all & carry_in);

        return result;
    }
}

/**
 * Carry Look-Ahead Adder - 超前进位加法器
 *
 * 通过提前计算进位信号来提高加法运算速度
 * 真正的超前进位加法器实现，所有进位并行计算
 */
template <unsigned N>
CLAResult<N> carry_lookahead_adder(ch_uint<N> a, ch_uint<N> b,
                                   ch_bool carry_in = false) {
    static_assert(N > 0, "Carry lookahead adder must have at least 1 bit");

    CLAResult<N> result;

    // 计算生成和传播信号
    ch_uint<N> g = a & b; // 生成信号 (Generate)
    ch_uint<N> p = a ^ b; // 传播信号 (Propagate)

    // 计算所有进位，使用真正的超前进位逻辑
    ch_uint<N + 1> all_carries = 0_d;

    // 设置初始进位
    all_carries = all_carries | (carry_in << make_literal<0>());

    // 使用真正的超前进位逻辑 - 所有进位并行计算
    [&]<std::size_t... I>(std::index_sequence<I...>) {
        (([&]() {
             if constexpr (I > 0) {
                 // 使用真正的超前进位公式计算第I位进位
                 // C_i = G_i + P_i*G_{i-1} + P_i*P_{i-1}*G_{i-2} + ... +
                 // P_i*...*P_1*G_0 + P_i*...*P_0*Cin
                 ch_bool carry_i =
                     compute_carry_at_position<N, I>(g, p, carry_in);

                 all_carries = all_carries | (carry_i << make_literal<I>());
             }
         }()),
         ...);
    }(std::make_index_sequence<N + 1>{});

    // 计算最终结果
    ch_uint<N> carry_bits =
        bits<N, 1>(all_carries); // 获取C1到CN位 (C0是输入的carry_in)
    result.sum = p ^ carry_bits;
    result.carry_out = bit_select<N>(all_carries);

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
    constexpr unsigned shift_amount = compute_idx_width(N);

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
            ch_uint<2 * N> partial_product = select(
                b_bit, ch_uint<2 * N>(a) << make_uint<shift_amount>(i), 0_d);
            result = result + partial_product;
        }

        return result;
    }
}

/**
 * Booth Multiplier - 班纳赫乘法器
 *
 * 使用Booth算法减少乘法操作中的加法次数
 */
/**
 * Booth Multiplier - 班纳赫乘法器
 *
 * 使用Booth算法减少乘法操作中的加法次数
 */
template <unsigned N>
ch_uint<2 * N> booth_multiplier(ch_uint<N> a, ch_uint<N> b) {
    static_assert(N > 0, "Booth multiplier must have at least 1 bit");

    // 扩展位宽以防止溢出
    ch_uint<2 * N> result = 0_d;
    ch_uint<2 * N> multiplicand(a); // 被乘数，扩展到2*N位
    ch_uint<2 * N> multiplier(b);   // 乘数，扩展到2*N位

    // 初始化扩展位，用于Booth算法中的前一位
    ch_bool prev_bit = 0_b; // 初始化为0

    for (unsigned i = 0; i < N; i++) {
        // 获取当前位
        ch_bool curr_bit = select(i < N, bit_select(multiplier, i), 0_b);

        // 根据Booth算法，比较当前位和前一位
        // Booth编码: 检查当前位(y_i)和前一位(y_{i-1})的组合
        ch_uint<2> booth_code = (curr_bit << 1_d) | prev_bit;

        // 根据Booth编码执行操作
        ch_uint<2 * N> partial_val;
        if (booth_code == 0_d) {
            // 00 -> +0 (不需要操作)
            partial_val = 0_d;
        } else if (booth_code == 1_d) {
            // 01 -> +M * 2^i
            partial_val = shl<2 * N>(multiplicand, i);
        } else if (booth_code == 2_d) {
            // 10 -> -M * 2^i
            partial_val = 0_d - shl<2 * N>(multiplicand, i);
        } else { // booth_code == 3_d
            // 11 -> +0 (不需要操作)
            partial_val = 0_d;
        }

        result = result + partial_val;

        // 更新前一位为当前位
        prev_bit = curr_bit;
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
        result.remainder = dividend;
        return result;
    }

    ch_uint<N> temp_divisor = divisor;

    // 非恢复除法算法
    for (int i = N - 1; i >= 0; --i) {
        // 左移余数
        result.remainder = (result.remainder << 1_d) | bit_select(dividend, i);

        // 尝试减去除数
        ch_uint<N> temp =
            result.remainder - temp_divisor; // 修正：使用已声明的变量

        // 检查结果是否为负
        ch_bool is_negative = bit_select<N - 1>(temp); // 检查符号位

        if (is_negative) {
            // 如果为负，恢复余数（通过加法）
            bit_select(result.quotient, i) = false;
        } else {
            // 如果为正，接受结果
            result.remainder = temp;
            bit_select(result.quotient, i) = true;
        }
    }

    return result;
}

/**
 * Square Root Calculator - 平方根计算器
 *
 * 使用牛顿法计算平方根
 */
template <unsigned N>
ch_uint<(N + 1) / 2> square_root_calculator(ch_uint<N> input) {
    static_assert(N > 0, "Square root calculator must have at least 1 bit");
    static constexpr unsigned RESULT_WIDTH = (N + 1) / 2;

    ch_uint<RESULT_WIDTH> result = 0_d;

    if (input == 0_d) {
        return result;
    }

    // 特殊处理小数值
    if (input == 1_d) {
        ch_uint<RESULT_WIDTH> one = 1_d;
        return one;
    }

    // 使用牛顿法计算平方根
    ch_uint<RESULT_WIDTH> x = input >> 1_d; // 初始猜测 (input / 2)

    // 最多迭代固定次数，确保收敛
    for (int iter = 0; iter < 10; ++iter) {
        // 牛顿迭代: x_new = (x + input/x) / 2
        ch_uint<RESULT_WIDTH> div_result = input / x;
        ch_uint<RESULT_WIDTH> sum = x + div_result;
        ch_uint<RESULT_WIDTH> new_x = sum >> 1_d; // 除以2

        // 如果不再改变，则停止
        if (new_x >= x) {
            x = new_x;
            break;
        }

        x = new_x;
    }

    result = x;

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
    result.result = temp_result >> Q;
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
    ch_uint<2 * N> shifted_a = ch_uint<2 * N>(a) << make_literal<Q>();
    ch_uint<2 * N> temp_result = shifted_a / ch_uint<2 * N>(b);
    result.result = temp_result;
    return result;
}

} // namespace chlib

#endif // CHLIB_ARITHMETIC_ADVANCE_H