#ifndef CH_CHLIB_IF_H
#define CH_CHLIB_IF_H

#include "core/bool.h"
#include "core/context.h"
#include "core/io.h"
#include "core/literal.h"
#include "core/lnode.h"
#include "core/lnodeimpl.h"
#include "core/node_builder.h"
#include "core/traits.h"
#include "core/uint.h"
#include "core/operators.h"
#include <functional>
#include <type_traits>

namespace ch::core {

// 定义一个trait来检测类型是否为ch_uint特化
template <typename T>
struct is_ch_uint : std::false_type {};

template <unsigned N>
struct is_ch_uint<ch_uint<N>> : std::true_type {};

template <typename T>
inline constexpr bool is_ch_uint_v = is_ch_uint<T>::value;

} // namespace ch::core

namespace ch::chlib {

// 定义一个IfThenElse结构，用于实现if-then-else语法
template <typename T>
class IfThenElse {
private:
    ch::core::ch_bool condition_;
    T then_value_;
    bool has_else_ = false;
    T else_value_;

public:
    // 构造函数，只接收条件和then分支的值
    IfThenElse(const ch::core::ch_bool &condition, const T &then_value)
        : condition_(condition), then_value_(then_value) {}

    // else_方法，用于设置else分支的值
    IfThenElse<T> &else_(const T &else_value) {
        else_value_ = else_value;
        has_else_ = true;
        return *this;
    }

    // 转换为T类型的运算符，执行实际的选择操作
    operator T() const {
        if (has_else_) {
            return ch::core::select(condition_, then_value_, else_value_);
        } else {
            // 如果没有else分支，需要提供默认值或抛出错误
            // 对于硬件类型，我们使用零值作为默认值
            using ValueType = std::remove_cv_t<std::remove_reference_t<T>>;
            if constexpr (std::is_same_v<ValueType, ch::core::ch_bool>) {
                return ch::core::select(condition_, then_value_, ch::core::ch_bool(false));
            } else if constexpr (ch::core::is_ch_uint_v<ValueType>) {
                constexpr unsigned width = ch::core::ch_width_v<ValueType>;
                return ch::core::select(condition_, then_value_, ch::core::ch_uint<width>(0_d));
            } else {
                // 对于其他类型，尝试使用默认构造
                static_assert(std::is_default_constructible_v<T>, 
                             "Type T must be default constructible when no else branch is provided");
                return ch::core::select(condition_, then_value_, T{});
            }
        }
    }
};

// 主要的if_函数，返回IfThenElse对象
template <typename T>
IfThenElse<T> if_(const ch::core::ch_bool &condition, const T &then_value) {
    return IfThenElse<T>(condition, then_value);
}

// 为更复杂的代码块使用函数式方法
template <typename ThenFunc, typename ElseFunc>
auto if_else(const ch::core::ch_bool &condition, 
             const ThenFunc &then_func, 
             const ElseFunc &else_func) {
    // 执行两个分支以生成硬件节点，然后使用select选择结果
    auto then_result = then_func();
    auto else_result = else_func();
    
    // 使用现有的select函数进行条件选择
    return ch::core::select(condition, then_result, else_result);
}

// 仅带then分支的版本，没有else时使用零值
template <typename Func>
auto if_then(const ch::core::ch_bool &condition, const Func &then_func) {
    auto then_result = then_func();
    
    // 根据结果类型使用适当的零值
    using ResultType = decltype(then_result);
    if constexpr (std::is_same_v<ResultType, ch::core::ch_bool>) {
        return ch::core::select(condition, then_result, ch::core::ch_bool(false));
    } else if constexpr (ch::core::is_ch_uint_v<ResultType>) {
        constexpr unsigned width = ch::core::ch_width_v<ResultType>;
        return ch::core::select(condition, then_result, ch::core::ch_uint<width>(0_d));
    } else {
        // 对于其他类型，尝试使用默认构造
        static_assert(std::is_default_constructible_v<ResultType>, 
                     "Result type must be default constructible when no else branch is provided");
        return ch::core::select(condition, then_result, ResultType{});
    }
}

} // namespace ch::chlib

#endif // CH_CHLIB_IF_H