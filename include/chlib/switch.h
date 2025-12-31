#ifndef CH_CHLIB_SWITCH_H
#define CH_CHLIB_SWITCH_H

#include <ch.hpp>
#include <core/operators.h>
#include <core/operators_runtime.h>
#include <core/types.h>
#include <tuple>
#include <utility>

using namespace ch::core;

namespace chlib {

// 用于表示case分支的结构
template <typename TCond, typename TResult> struct case_entry {
    TCond condition;
    TResult result;

    constexpr case_entry(TCond c, TResult r) : condition(c), result(r) {}
};

// 便捷函数用于创建case_entry
template <typename TCond, typename TResult>
constexpr case_entry<TCond, TResult> case_(TCond &&condition,
                                           TResult &&result) {
    return case_entry<TCond, TResult>(std::forward<TCond>(condition),
                                      std::forward<TResult>(result));
}

// 为两个参数都是字面量的情况提供支持（优先级最高）
template <typename TCond, typename TResult>
    requires(is_ch_literal_v<std::decay_t<TCond>> &&
             is_ch_literal_v<std::decay_t<TResult>>)
constexpr auto case_(TCond &&condition, TResult &&result) {
    // 将字面量转换为对应宽度的ch_uint
    using cond_uint_type = ch_uint<ch_width_v<std::decay_t<TCond>>>;
    using result_uint_type = ch_uint<ch_width_v<std::decay_t<TResult>>>;
    cond_uint_type cond_uint(condition);
    result_uint_type result_uint(result);
    return case_entry<cond_uint_type, result_uint_type>(std::move(cond_uint),
                                                        std::move(result_uint));
}

// 仅为条件参数是字面量的情况提供支持
template <typename TCond, typename TResult>
    requires(is_ch_literal_v<std::decay_t<TCond>> && 
             !is_ch_literal_v<std::decay_t<TResult>>)
constexpr auto case_(TCond &&condition, TResult &&result) {
    // 将条件字面量转换为对应宽度的ch_uint
    using uint_type = ch_uint<ch_width_v<std::decay_t<TCond>>>;
    uint_type cond_uint(condition);
    return case_entry<uint_type, TResult>(std::move(cond_uint),
                                          std::forward<TResult>(result));
}

// 仅为结果参数是字面量的情况提供支持
template <typename TCond, typename TResult>
    requires(!is_ch_literal_v<std::decay_t<TCond>> && 
             is_ch_literal_v<std::decay_t<TResult>>)
constexpr auto case_(TCond &&condition, TResult &&result) {
    // 将结果字面量转换为对应宽度的ch_uint
    using uint_type = ch_uint<ch_width_v<std::decay_t<TResult>>>;
    uint_type result_uint(result);
    return case_entry<TCond, uint_type>(std::forward<TCond>(condition),
                                        std::move(result_uint));
}

// 递归基础情况：没有更多case分支时，返回默认值
template <typename TDefault>
constexpr auto switch_impl(TDefault &&default_result) {
    return std::forward<TDefault>(default_result);
}

// 递归实现：处理当前case并继续处理剩余参数
// 电路行为：实现优先级编码器，从第一个case开始依次检查，找到第一个匹配的case
// 硬件结构：多个比较器串联，形如 if (value == case1) result1
//           else if (value == case2) result2
//           else if (value == case3) result3
//           else default_result
template <typename TValue, typename TDefault, typename TCaseValue,
          typename TResult, typename... TRest>
constexpr auto switch_impl(TValue &&value, TDefault &&default_result,
                           case_entry<TCaseValue, TResult> current_case,
                           TRest &&...rest_cases) {
    // 检查当前case是否匹配
    ch_bool is_match = (value == current_case.condition);
    // 如果匹配则返回当前结果，否则继续处理剩余case
    if constexpr (sizeof...(rest_cases) == 0) {
        // 这是最后一个case
        return is_match ? current_case.result : default_result;
    } else {
        // 还有更多case，递归处理
        return is_match ? current_case.result
                        : switch_impl(std::forward<TValue>(value),
                                      std::forward<TDefault>(default_result),
                                      std::forward<TRest>(rest_cases)...);
    }
}

// 主switch函数，接受一个值和多个case_entry，最后是默认结果
// 电路行为：优先级编码器结构，优先级从第一个case到最后一个case
template <typename TValue, typename... Args>
constexpr auto switch_(TValue &&value, Args &&...args) {
    // 最后一个参数是默认结果
    // 其余参数是case_entry
    return [&value](auto &&...params) {
        return switch_impl(value, std::forward<decltype(params)>(params)...);
    }(std::forward<Args>(args)...);
}

// 硬件并行实现：所有比较同时进行
// 电路行为：并行比较器，所有case同时被评估，然后通过优先级选择器选择结果
// 硬件结构：N个比较器并行执行 + 优先级选择逻辑
// 优点：关键路径仅包含1个比较延迟 + 1个MUX选择延迟，与case数量无关
// 缺点：需要N个比较器并行，资源消耗随case数量线性增长
template <typename TValue, typename TDefault, typename... TCaseEntries>
constexpr auto switch_parallel(TValue &&value, TDefault &&default_result,
                               TCaseEntries &&...cases) {
    if constexpr (sizeof...(cases) == 0) {
        return std::forward<TDefault>(default_result);
    } else {
        // 创建case数组
        auto case_array = std::make_tuple(cases...);

        // 所有比较并行执行
        return [&]<size_t... I>(std::index_sequence<I...>) {
            // 创建所有条件
            auto conditions = std::make_tuple(
                (value == std::get<I>(case_array).condition)...);
            auto results = std::make_tuple(std::get<I>(case_array).result...);

            // 使用优先级编码器逻辑：第一个匹配的case获胜
            auto result = default_result;

            // 从最高优先级到最低优先级依次检查
            auto _ = ((static_cast<ch_bool>(std::get<I>(conditions))
                           ? (result = std::get<I>(results), true)
                           : false) ||
                      ... || true);
            (void)_; // 避免未使用变量警告

            return result;
        }(std::index_sequence_for<TCaseEntries...>{});
    }
}

// 便捷函数，允许直接传入值对而不需要显式创建case_entry
// 电路行为：与switch_相同，使用优先级编码器结构
template <typename TValue, typename TDefault, typename... TArgs>
constexpr auto switch_case(TValue &&value, TDefault &&default_result,
                           TArgs &&...args) {
    static_assert(sizeof...(args) % 2 == 0,
                  "Arguments must come in pairs: condition, result, condition, "
                  "result, ...");

    // 将参数转换为case_entry
    constexpr size_t case_count = sizeof...(args) / 2;

    if constexpr (case_count == 0) {
        return std::forward<TDefault>(default_result);
    } else {
        return [&]<size_t... I>(std::index_sequence<I...>) {
            // 构造case_entry
            auto cases = std::make_tuple(
                case_(std::get<I * 2>(std::forward_as_tuple(args...)),
                      std::get<I * 2 + 1>(std::forward_as_tuple(args...)))...);

            // 使用parallel_switch处理
            return switch_parallel(std::forward<TValue>(value),
                                   std::forward<TDefault>(default_result),
                                   std::get<I>(cases)...);
        }(std::make_index_sequence<case_count>{});
    }
}

template <typename TCond, typename TResult>
constexpr auto case_value(TCond &&condition, TResult &&result) {
    return case_<TCond, TResult>(std::forward<TCond>(condition),
                                 std::forward<TResult>(result));
}

// 为字面量类型提供特化版本
template <typename TCond, typename TResult>
    requires is_ch_literal_v<std::decay_t<TCond>>
constexpr auto case_value(TCond &&condition, TResult &&result) {
    using uint_type = ch_uint<ch_width_v<std::decay_t<TCond>>>;
    uint_type cond_uint(condition);
    return case_<uint_type, TResult>(std::move(cond_uint),
                                     std::forward<TResult>(result));
}

// 为字面量作为result的情况也提供支持
template <typename TCond, typename TResult>
    requires is_ch_literal_v<std::decay_t<TResult>>
constexpr auto case_value(TCond &&condition, TResult &&result) {
    using uint_type = ch_uint<ch_width_v<std::decay_t<TResult>>>;
    uint_type result_uint(result);
    return case_<TCond, uint_type>(std::forward<TCond>(condition),
                                   std::move(result_uint));
}

// 为两个参数都是字面量的情况提供支持
template <typename TCond, typename TResult>
    requires(is_ch_literal_v<std::decay_t<TCond>> &&
             is_ch_literal_v<std::decay_t<TResult>>)
constexpr auto case_value(TCond &&condition, TResult &&result) {
    using cond_uint_type = ch_uint<ch_width_v<std::decay_t<TCond>>>;
    using result_uint_type = ch_uint<ch_width_v<std::decay_t<TResult>>>;
    cond_uint_type cond_uint(condition);
    result_uint_type result_uint(result);
    return case_<cond_uint_type, result_uint_type>(std::move(cond_uint),
                                                   std::move(result_uint));
}

// switch_pairs函数，接受case_entry参数
template <typename TValue, typename TDefault, typename... TCaseEntries>
constexpr auto switch_pairs(TValue &&value,
                            TCaseEntries &&...cases_and_default) {
    // 最后一个参数是默认值，其余是case_entry
    constexpr size_t total_args = sizeof...(cases_and_default);
    static_assert(total_args > 0,
                  "At least one case or default value must be provided");

    if constexpr (total_args == 1) {
        // 只有默认值
        return std::forward<TDefault>(
            std::get<0>(std::forward_as_tuple(cases_and_default...)));
    } else {
        // 提取默认值（最后一个参数）
        auto args_tuple = std::forward_as_tuple(
            std::forward<TCaseEntries>(cases_and_default)...);
        auto default_val = std::get<total_args - 1>(args_tuple);

        // 从第一个到倒数第二个参数是case_entries
        constexpr size_t case_count = total_args - 1;
        return [&]<size_t... I>(std::index_sequence<I...>) {
            return switch_parallel(std::forward<TValue>(value),
                                   std::forward<TDefault>(default_val),
                                   std::get<I>(args_tuple)...);
        }(std::make_index_sequence<case_count>{});
    }
}

} // namespace chlib

#endif