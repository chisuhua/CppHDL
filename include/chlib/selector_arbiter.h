#ifndef CHLIB_SELECTOR_ARBITER_H
#define CHLIB_SELECTOR_ARBITER_H

#include "ch.hpp"
#include "chlib/converter.h"
#include "chlib/logic.h"
#include "component.h"
#include "core/bool.h"
#include "core/literal.h"
#include "core/operators.h"
#include "core/operators_runtime.h"
#include "core/uint.h"
#include <cassert>
#include <cstddef>

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
    static constexpr unsigned BIT_WIDTH = compute_bit_width(N);

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

// 辅助结构体用于实现串行依赖计算
template <unsigned N, unsigned Iter = 0> struct ProcessRequests {
    static void process(ch_uint<N> request, ch_uint<N> priority_ptr,
                        RoundRobinArbiterResult<N> &result) {
        ch_uint<compute_bit_width(N)> pos = (priority_ptr + Iter) % N;
        ch_bool req_at_pos = bit_select(request, pos);
        ch_uint<N> grant_one_hot = ch_uint<N>(1_d) << pos;

        // 如果当前位有请求且尚未确定授予，则授予当前位
        result.grant =
            select(req_at_pos && !result.valid, grant_one_hot, result.grant);
        result.valid = select(req_at_pos, 1_b, result.valid);

        // 递归处理下一个迭代
        if constexpr (Iter + 1 < N) {
            ProcessRequests<N, Iter + 1>::process(request, priority_ptr,
                                                  result);
        }
    }
};

// 特化终止条件
template <unsigned N> struct ProcessRequests<N, N> {
    static void process(ch_uint<N> request, ch_uint<N> priority_ptr,
                        RoundRobinArbiterResult<N> &result) {
        // 什么都不做，递归终止
    }
};

// 辅助结构体用于更新优先级指针
template <unsigned N, unsigned Iter = 0> struct UpdatePriority {
    static void process(const RoundRobinArbiterResult<N> &result,
                        ch_uint<N> &next_priority, ch_uint<N> request,
                        ch_uint<N> priority_ptr) {
        if (result.valid) {
            ch_uint<compute_bit_width(N)> pos = (priority_ptr + Iter) % N;
            ch_bool req_at_pos = bit_select(request, pos);
            ch_uint<compute_bit_width(N)> next_pos = ((pos + 1) % N);
            next_priority =
                select(req_at_pos && (result.grant == (ch_uint<N>(1_d) << pos)),
                       next_pos, next_priority);

            // 递归处理下一个迭代
            if constexpr (Iter + 1 < N) {
                UpdatePriority<N, Iter + 1>::process(result, next_priority,
                                                     request, priority_ptr);
            }
        }
    }
};

// 特化终止条件
template <unsigned N> struct UpdatePriority<N, N> {
    static void process(const RoundRobinArbiterResult<N> &result,
                        ch_uint<N> &next_priority, ch_uint<N> request,
                        ch_uint<N> priority_ptr) {
        // 什么都不做，递归终止
    }
};

template <unsigned N>
RoundRobinArbiterResult<N> round_robin_arbiter(ch_uint<N> request) {

    static_assert(N > 0, "Round robin arbiter must have at least 1 bit");

    RoundRobinArbiterResult<N> result;
    result.grant = 0_d;
    result.valid = false;

    // 使用内部寄存器来管理优先级指针
    ch_reg<ch_uint<N>> ptr_reg(0_d, "rr_arbiter_ptr"); // 初始化为0
    ch_uint<N> priority_ptr = ptr_reg; // 获取当前优先级指针值

    // 使用递归模板处理第一个循环，确保串行依赖
    ProcessRequests<N>::process(request, priority_ptr, result);

    // 计算下一个优先级指针
    ch_uint<N> next_priority = priority_ptr;

    // 使用递归模板处理第二个循环，确保串行依赖
    UpdatePriority<N>::process(result, next_priority, request, priority_ptr);

    // 更新优先级指针寄存器（在时钟边沿）
    ptr_reg->next = next_priority;

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
    static constexpr unsigned BIT_WIDTH = compute_bit_width(N);

    PrioritySelectorResult<N> result;
    result.grant = 0_d;
    result.valid = false;

    // 将one-hot编码的last_grant转换为二进制索引，然后加1
    // 需要确保当last_grant为0时，使用默认值0
    ch_bool has_last_grant = last_grant != 0_d;
    ch_uint<compute_idx_width(N)> last_grant_idx =
        select(has_last_grant, onehot_to_binary<N>(last_grant), 0_d);
    ch_uint<BIT_WIDTH> start_pos = (last_grant_idx + 1) % N;

    // 先从start_pos到高位查找
    [&]<std::size_t... I>(std::index_sequence<I...>) {
        auto process_request = [&](unsigned i) {
            ch_uint<BIT_WIDTH> pos = (start_pos + i) % N;
            ch_bool req_at_pos = bit_select(request, pos);
            ch_uint<N> grant_one_hot = ch_uint<N>(1_d) << pos;

            // 如果当前位有请求且尚未确定授予，则授予当前位
            result.grant = select(req_at_pos && !result.valid, grant_one_hot,
                                  result.grant);
            result.valid = select(req_at_pos, 1_b, result.valid);
        };

        (process_request(I), ...);
    }(std::make_index_sequence<N>{});

    return result;
}

} // namespace chlib

#endif // CHLIB_SELECTOR_ARBITER_H