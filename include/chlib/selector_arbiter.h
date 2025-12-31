#ifndef CHLIB_SELECTOR_ARBITER_H
#define CHLIB_SELECTOR_ARBITER_H

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
 * 优先级选择器 - 函数式接口
 *
 * 从多个请求中选择优先级最高的一个，通常低位优先级更高
 */
template <unsigned N> struct PrioritySelectorResult {
    ch_uint<N> grant; // 授予信号，one-hot编码
    ch_bool valid;    // 是否有有效请求
};

template <unsigned N>
PrioritySelectorResult<N> priority_selector(ch_uint<N> request) {
    static_assert(N > 0, "Priority selector must have at least 1 bit");
    static constexpr unsigned BIT_WIDTH = compute_bit_width(N - 1);

    PrioritySelectorResult<N> result;
    result.grant = 0_d;
    result.valid = false;

    // 从低位到高位检查请求，低位优先级更高
    for (unsigned i = 0; i < N; ++i) {
        ch_bool req_at_i = bit_select(request, i);
        ch_uint<N> grant_one_hot = ch_uint<N>(1_d) << make_uint<BIT_WIDTH>(i);

        // 如果当前位有请求且尚未确定优先级更高的请求，则授予当前位
        result.grant =
            select(req_at_i && !result.valid, grant_one_hot, result.grant);
        result.valid = select(req_at_i, 1_b, result.valid);
    }

    return result;
}

/**
 * 循环仲裁器 - 函数式接口
 *
 * 循环仲裁器，确保公平访问，记录上次服务的请求者
 */
template <unsigned N> struct RoundRobinArbiterResult {
    ch_uint<N> grant;    // 授予信号，one-hot编码
    ch_bool valid;       // 是否有有效请求
    ch_uint<N> next_ptr; // 下一个优先级指针
};

template <unsigned N>
RoundRobinArbiterResult<N> round_robin_arbiter(ch_uint<N> request,
                                               ch_uint<N> priority_ptr,
                                               ch_bool clk, ch_bool rst) {

    static_assert(N > 0, "Round robin arbiter must have at least 1 bit");

    RoundRobinArbiterResult<N> result;
    result.grant = 0_d;
    result.valid = false;

    // 首先从优先级指针开始向高位查找
    for (unsigned i = 0; i < N; ++i) {
        unsigned pos = (priority_ptr.value() + i) % N; // 从优先级指针开始循环
        ch_bool req_at_pos = bit_select(request, pos);
        ch_uint<N> grant_one_hot = ch_uint<N>(1_d) << make_literal(pos);

        // 如果当前位有请求且尚未确定授予，则授予当前位
        result.grant =
            select(req_at_pos && !result.valid, grant_one_hot, result.grant);
        result.valid = select(req_at_pos, true, result.valid);
    }

    // 计算下一个优先级指针
    ch_uint<N> next_priority = priority_ptr;

    if (result.valid) {
        // 如果有请求被授予，下一次从该位置的下一个位置开始
        for (unsigned i = 0; i < N; ++i) {
            unsigned pos = (priority_ptr.value() + i) % N;
            ch_bool req_at_pos = bit_select(request, pos);
            ch_uint<N> next_pos = make_literal((pos + 1) % N);
            next_priority =
                select(req_at_pos && (result.grant ==
                                      (ch_uint<N>(1_d) << make_literal(pos))),
                       next_pos, next_priority);
        }
    }

    // 更新优先级指针（在时钟边沿）
    ch_reg<ch_uint<N>> ptr_reg(priority_ptr, "rr_arbiter_ptr");
    ptr_reg->next = select(rst, ch_uint<N>(0_d), next_priority);

    result.next_ptr = ptr_reg;

    return result;
}

/**
 * 简化版循环仲裁器 - 函数式接口
 *
 * 简化版循环仲裁器，不需要外部时钟和复位
 */
template <unsigned N>
PrioritySelectorResult<N> round_robin_selector(ch_uint<N> request,
                                               ch_uint<N> last_grant) {
    static_assert(N > 0, "Round robin selector must have at least 1 bit");
    static constexpr unsigned BIT_WIDTH = compute_bit_width(N - 1);

    PrioritySelectorResult<N> result;
    result.grant = 0_d;
    result.valid = false;

    // 从last_grant的下一位开始查找
    ch_uint<N> start_pos = (last_grant + 1) % N;

    // 先从start_pos到高位查找
    for (unsigned i = 0; i < N; ++i) {
        ch_uint<N> pos = (start_pos + i) % N;
        ch_bool req_at_pos = bit_select(request, pos);
        ch_uint<N> grant_one_hot = ch_uint<N>(1_d) << pos;

        // 如果当前位有请求且尚未确定授予，则授予当前位
        result.grant =
            select(req_at_pos && !result.valid, grant_one_hot, result.grant);
        result.valid = select(req_at_pos, 1_b, result.valid);
    }

    return result;
}

} // namespace chlib

#endif // CHLIB_SELECTOR_ARBITER_H