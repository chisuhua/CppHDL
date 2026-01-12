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
    PrioritySelectorResult()
        : grant(0, "grant"),
          valid(false, "valid") // 或 valid(0, "valid")，取决于ch_bool的实现
    {}
};

template <unsigned N>
PrioritySelectorResult<N> priority_selector(ch_uint<N> request) {
    static_assert(N > 0, "Priority selector must have at least 1 bit");
    static constexpr unsigned BIT_WIDTH = compute_idx_width(N);

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
        ch_uint<compute_idx_width(N)> pos = (priority_ptr + Iter) % N;
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
            ch_uint<compute_idx_width(N)> pos = (priority_ptr + Iter) % N;
            ch_bool req_at_pos = bit_select(request, pos);
            ch_uint<compute_idx_width(N)> next_pos = ((pos + 1) % N);
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
    static constexpr unsigned IDX_WIDTH = compute_idx_width(N);

    PrioritySelectorResult<N> result;

    // 将one-hot编码的last_grant转换为二进制索引，然后加1
    // 需要确保当last_grant为0时，使用默认值0
    ch_bool has_last_grant = last_grant != 0_d;
    ch_uint<IDX_WIDTH> last_grant_idx =
        select(has_last_grant, onehot_to_binary<N>(last_grant), 0_d);
    ch_uint<IDX_WIDTH> start_pos = (last_grant_idx + 1_d) % make_literal<N>();

    // 初始化第一个元素
    ch_uint<IDX_WIDTH> pos_0 = (start_pos + 0) % make_literal<N>();
    ch_bool req_at_pos_0 = bit_select(request, pos_0);
    ch_uint<N> grant_one_hot_0 = shl<N>(ch_uint<N>(1_d), pos_0);

    result.grant = select(req_at_pos_0, grant_one_hot_0, 0_d);
    result.valid = select(req_at_pos_0, 1_b, 0_b);

    // ch_uint<N> request_tap(request, "request");
    // ch_uint<N> last_grant_tap(last_grant, "last_grant");
    // ch_uint<1> has_last_grant_tap(has_last_grant, "has_last_grant");
    // ch_uint<IDX_WIDTH> last_grant_idx_tap(last_grant_idx, "last_grant_idx");
    // ch_uint<IDX_WIDTH> start_pos_tap(start_pos, "start_pos");

    // ch_uint<IDX_WIDTH> pos0_tap(pos_0, "pos0"); // 授予信号，one-hot编码
    // ch_uint<1> req_at_pos0_tap(req_at_pos_0, "req_at_pos0"); //
    // 是否有有效请求 ch_uint<N> grant_onehot0_tap(grant_one_hot_0,
    //                              "grant_onehot0"); // 授予信号，one-hot编码
    // ch_uint<N> grant0_tap(result.grant,
    //                       "grant0"); // 授予信号，one-hot编码
    // ch_uint<1> valid0_tap(result.valid, "valid0"); // 是否有有效请求

    // 从start_pos开始循环检查，直到找到请求或完成一圈
    for (unsigned i = 1; i < N; ++i) {
        ch_uint<IDX_WIDTH> pos = (start_pos + i) % make_literal<N>();
        ch_bool req_at_pos = bit_select(request, pos);
        ch_uint<N> grant_one_hot = shl<N>(ch_uint<N>(1_d), pos);

        // 如果当前位有请求且尚未确定授予，则授予当前位
        result.grant =
            select(req_at_pos && !result.valid, grant_one_hot, result.grant);
        result.valid = select(req_at_pos, 1_b, result.valid);

        // ch_uint<IDX_WIDTH> pos_tap(
        //     pos, "pos" + std::to_string(i)); // 授予信号，one-hot编码
        // ch_uint<1> req_at_pos_tap(req_at_pos, "req_at_pos" +
        // std::to_string(i)); ch_uint<N> grant_onehot_tap(grant_one_hot,
        //                             "grant_onehot" + std::to_string(i));
        // ch_uint<N> grant1_tap(result.grant, "grant" + std::to_string(i));
        // ch_uint<1> valid1_tap(result.valid, "valid" + std::to_string(i));
    }

    ch_uint<N> grant1_tap(result.grant, "grant");
    ch_uint<1> valid1_tap(result.valid, "valid");
    return result; // 返回最后一个结果，即最终结果
}

} // namespace chlib

#endif // CHLIB_SELECTOR_ARBITER_H