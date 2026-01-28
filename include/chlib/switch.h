#ifndef CH_CHLIB_SWITCH_H
#define CH_CHLIB_SWITCH_H

#include <ch.hpp>
#include <core/operators.h>
#include <core/operators_runtime.h>
#include <core/types.h>
#include <cstdint>
#include <tuple>
#include <type_traits>
#include <utility>

using namespace ch::core;

// 为ch_uint类型特化std::common_type
namespace std {
template <unsigned N, unsigned M>
struct common_type<ch::core::ch_uint<N>, ch::core::ch_uint<M>> {
    using type = ch::core::ch_uint<(N > M ? N : M)>; // 取较大宽度作为公共类型
};

// 处理ch_uint与字面量类型的通用情况
template <unsigned N, uint64_t V, uint32_t W>
struct common_type<ch::core::ch_uint<N>, ch::core::ch_literal_impl<V, W>> {
    using type = ch::core::ch_uint<N>;
};

template <uint64_t V, uint32_t W, unsigned N>
struct common_type<ch::core::ch_literal_impl<V, W>, ch::core::ch_uint<N>> {
    using type = ch::core::ch_uint<N>;
};

// 处理ch_uint与ch_bool类型的通用情况
template <unsigned N>
struct common_type<ch::core::ch_uint<N>, ch::core::ch_bool> {
    using type = ch::core::ch_uint<(N > 1 ? N : 1)>; // ch_bool可视为ch_uint<1>
};

template <unsigned N>
struct common_type<ch::core::ch_bool, ch::core::ch_uint<N>> {
    using type = ch::core::ch_uint<(N > 1 ? N : 1)>; // ch_bool可视为ch_uint<1>
};

} // namespace std

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

// 辅助函数：处理不同类型间的转换，特别是ch_bool到ch_uint的转换
template<typename FromType, typename ToType>
constexpr auto convert_for_select(FromType&& value) -> ToType {
    if constexpr (std::is_same_v<std::decay_t<FromType>, ch::core::ch_bool> && 
                  std::is_same_v<ToType, ch::core::ch_uint<1>>) {
        // 当源类型是ch_bool，目标类型是ch_uint<1>时，直接转换
        return ToType{value};
    } else if constexpr (std::is_same_v<std::decay_t<FromType>, ch::core::ch_bool> && 
                        (std::is_same_v<ToType, ch::core::ch_uint<2>> || 
                         std::is_same_v<ToType, ch::core::ch_uint<3>> || 
                         std::is_same_v<ToType, ch::core::ch_uint<4>> || 
                         std::is_same_v<ToType, ch::core::ch_uint<8>> || 
                         std::is_same_v<ToType, ch::core::ch_uint<15>>)) {
        // 当源类型是ch_bool而目标类型是ch_uint（且N>1）时，需要特殊处理
        // 将ch_bool转换为ch_uint<1>，然后再扩展
        constexpr unsigned to_width = ch::core::ch_width_v<std::decay_t<ToType>>;
        if constexpr (to_width > 1) {
            // 使用zext进行零扩展
            ch::core::ch_uint<1> temp{value};
            return ch::core::zext<to_width>(temp);
        } else {
            return ToType{value}; // N==1的情况
        }
    } else {
        // 对于其他情况，执行标准转换
        return static_cast<ToType>(std::forward<FromType>(value));
    }
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
        using result_type = std::common_type_t<TResult, TDefault>;
        return select(is_match, convert_for_select<TResult, result_type>(std::forward<TResult>(current_case.result)),
                      convert_for_select<TDefault, result_type>(std::forward<TDefault>(default_result)));
    } else {
        // 还有更多case，递归处理
        using recursive_result_type = decltype(switch_impl(
            std::forward<TValue>(value), std::forward<TDefault>(default_result),
            std::forward<TRest>(rest_cases)...));
        using result_type = std::common_type_t<TResult, recursive_result_type>;
        
        return select(is_match, 
                      convert_for_select<TResult, result_type>(std::forward<TResult>(current_case.result)),
                      convert_for_select<recursive_result_type, result_type>(
                          switch_impl(std::forward<TValue>(value),
                                    std::forward<TDefault>(default_result),
                                    std::forward<TRest>(rest_cases)...)));
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
            // 从最后一个case开始往前处理，逐层构建mux树
            // 这样确保第一个case具有最高优先级
            auto result = default_result;
            
            // 使用folding expression，从右到左处理（最后的case优先级低）
            // 但由于我们从后往前构建，实际上最前面的case优先级最高
            [&]<size_t... J>(std::index_sequence<J...>) {
                // 从后往前反向遍历
                ((result = select(std::get<sizeof...(I) - 1 - J>(conditions),
                                  std::get<sizeof...(I) - 1 - J>(results),
                                  result)), ...);
            }(std::make_index_sequence<sizeof...(I)>{});

            return result;
        }(std::index_sequence_for<TCaseEntries...>{});
    }
}

// 为默认值是字面量类型的情况提供特化版本
template <typename TValue, typename TDefault, typename... TCaseEntries>
    requires is_ch_literal_v<std::decay_t<TDefault>>
constexpr auto switch_parallel(TValue &&value, TDefault &&default_result,
                               TCaseEntries &&...cases) {
    // 将默认值字面量转换为对应宽度的ch_uint
    using default_uint_type = ch_uint<ch_width_v<std::decay_t<TDefault>>>;
    default_uint_type default_uint(default_result);

    if constexpr (sizeof...(cases) == 0) {
        return default_uint;
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
            auto result = default_uint;

            // 从最后一个case开始往前处理，逐层构建mux树
            [&]<size_t... J>(std::index_sequence<J...>) {
                // 从后往前反向遍历
                ((result = select(std::get<sizeof...(I) - 1 - J>(conditions),
                                  std::get<sizeof...(I) - 1 - J>(results),
                                  result)), ...);
            }(std::make_index_sequence<sizeof...(I)>{});

            return result;
        }(std::index_sequence_for<TCaseEntries...>{});
    }
}

// 便捷函数，允许直接传入值对而不需要显式创建case_entry
// 电路行为：与switch_相同，使用优先级编码器结构
template <typename TValue, typename TDefault, typename TCond1, typename TResult1>
constexpr auto switch_case(TValue &&value, TDefault &&default_result,
                           TCond1 &&cond1, TResult1 &&result1) {
    return switch_parallel(std::forward<TValue>(value),
                          std::forward<TDefault>(default_result),
                          case_(std::forward<TCond1>(cond1),
                                std::forward<TResult1>(result1)));
}

template <typename TValue, typename TDefault, typename TCond1, typename TResult1,
          typename TCond2, typename TResult2>
constexpr auto switch_case(TValue &&value, TDefault &&default_result,
                           TCond1 &&cond1, TResult1 &&result1,
                           TCond2 &&cond2, TResult2 &&result2) {
    return switch_parallel(std::forward<TValue>(value),
                          std::forward<TDefault>(default_result),
                          case_(std::forward<TCond1>(cond1),
                                std::forward<TResult1>(result1)),
                          case_(std::forward<TCond2>(cond2),
                                std::forward<TResult2>(result2)));
}

template <typename TValue, typename TDefault, typename TCond1, typename TResult1,
          typename TCond2, typename TResult2, typename TCond3, typename TResult3>
constexpr auto switch_case(TValue &&value, TDefault &&default_result,
                           TCond1 &&cond1, TResult1 &&result1,
                           TCond2 &&cond2, TResult2 &&result2,
                           TCond3 &&cond3, TResult3 &&result3) {
    return switch_parallel(std::forward<TValue>(value),
                          std::forward<TDefault>(default_result),
                          case_(std::forward<TCond1>(cond1),
                                std::forward<TResult1>(result1)),
                          case_(std::forward<TCond2>(cond2),
                                std::forward<TResult2>(result2)),
                          case_(std::forward<TCond3>(cond3),
                                std::forward<TResult3>(result3)));
}

// template <typename TCond, typename TResult>
//     requires(!is_ch_literal_v<std::decay_t<TCond>> &&
//              !is_ch_literal_v<std::decay_t<TResult>>)
// constexpr auto case_value(TCond &&condition, TResult &&result) {
//     return case_<TCond, TResult>(std::forward<TCond>(condition),
//                                  std::forward<TResult>(result));
// }

// // 为字面量类型提供特化版本
// template <typename TCond, typename TResult>
//     requires(is_ch_literal_v<std::decay_t<TCond>> &&
//              !is_ch_literal_v<std::decay_t<TResult>>)
// constexpr auto case_value(TCond &&condition, TResult &&result) {
//     using uint_type = ch_uint<ch_width_v<std::decay_t<TCond>>>;
//     uint_type cond_uint(condition);
//     return case_<uint_type, TResult>(std::move(cond_uint),
//                                      std::forward<TResult>(result));
// }

// // 为字面量作为result的情况也提供支持
// template <typename TCond, typename TResult>
//     requires(!is_ch_literal_v<std::decay_t<TCond>> &&
//              is_ch_literal_v<std::decay_t<TResult>>)
// constexpr auto case_value(TCond &&condition, TResult &&result) {
//     using uint_type = ch_uint<ch_width_v<std::decay_t<TResult>>>;
//     uint_type result_uint(result);
//     return case_<TCond, uint_type>(std::forward<TCond>(condition),
//                                    std::move(result_uint));
// }

// // 为两个参数都是字面量的情况提供支持
// template <typename TCond, typename TResult>
//     requires(is_ch_literal_v<std::decay_t<TCond>> &&
//              is_ch_literal_v<std::decay_t<TResult>>)
// constexpr auto case_value(TCond &&condition, TResult &&result) {
//     using cond_uint_type = ch_uint<ch_width_v<std::decay_t<TCond>>>;
//     using result_uint_type = ch_uint<ch_width_v<std::decay_t<TResult>>>;
//     cond_uint_type cond_uint(condition);
//     result_uint_type result_uint(result);
//     return case_<cond_uint_type, result_uint_type>(std::move(cond_uint),
//                                                    std::move(result_uint));
// }

// // switch_pairs函数，接受case_entry参数
// template <typename TValue, typename TDefault, typename... TCaseEntries>
// constexpr auto switch_pairs(TValue &&value,
//                             TCaseEntries &&...cases_and_default) {
//     // 最后一个参数是默认值，其余是case_entries
//     constexpr size_t total_args = sizeof...(cases_and_default);
//     static_assert(total_args > 0,
//                   "At least one case or default value must be provided");

//     if constexpr (total_args == 1) {
//         // 只有默认值
//         return std::forward<TDefault>(
//             std::get<0>(std::forward_as_tuple(cases_and_default...)));
//     } else {
//         // 提取默认值（最后一个参数）
//         auto args_tuple = std::forward_as_tuple(
//             std::forward<TCaseEntries>(cases_and_default)...);
//         auto default_val = std::get<total_args - 1>(args_tuple);

//         // 从第一个到倒数第二个参数是case_entries
//         constexpr size_t case_count = total_args - 1;
//         return [&]<size_t... I>(std::index_sequence<I...>) {
//             return switch_parallel(std::forward<TValue>(value),
//                                    std::forward<TDefault>(default_val),
//                                    std::get<I>(args_tuple)...);
//         }(std::make_index_sequence<case_count>{});
//     }
// }

} // namespace chlib

#endif