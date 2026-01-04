#ifndef CH_CHLIB_IF_H
#define CH_CHLIB_IF_H

#include "core/bool.h"
#include "core/context.h"
#include "core/literal.h"
#include "core/lnode.h"
#include "core/lnodeimpl.h"
#include "core/node_builder.h"
#include "core/operators.h"
#include "core/traits.h"
#include "core/uint.h"
#include <functional>
#include <memory>
#include <vector>

using namespace ch::core;

namespace chlib {

// 多条件选择器类 - 表达式风格
template <typename T> class multi_if {
private:
    struct branch_info {
        ch_bool condition;
        T value;
    };

    std::vector<branch_info> branches_;
    bool has_else_ = false;
    std::source_location creation_loc_;

public:
    multi_if(const ch_bool &condition, const T &value,
             std::source_location sloc = std::source_location::current())
        : creation_loc_(sloc) {
        static_assert(ch::core::HardwareType<T> ||
                          std::is_arithmetic_v<std::remove_cvref_t<T>>,
                      "Only hardware types or arithmetic types are allowed in "
                      "if_ constructs");
        branches_.push_back({condition, value});
    }

    multi_if &elif (const ch_bool &condition, const T &value) {
        if (has_else_) {
            throw std::runtime_error("Cannot add elif after else");
        }
        branches_.push_back({condition, value});
        return *this;
    }

    multi_if &else_(const T &value) {
        if (has_else_) {
            throw std::runtime_error("Else branch already added");
        }
        branches_.push_back({ch_bool(true), value});
        has_else_ = true;
        return *this;
    }

    // 转换操作符：将条件语句转换为T类型的值
    operator T() const {
        // 从后往前构建嵌套的select语句
        if (branches_.empty()) {
            throw std::runtime_error("Empty if block");
        }

        T current_value = branches_.back().value;

        // 从倒数第二个开始向前处理
        for (auto it = branches_.rbegin() + 1; it != branches_.rend(); ++it) {
            current_value =
                ::ch::core::select(it->condition, it->value, current_value);
        }

        return current_value;
    }

    // 便捷方法：获取最终值
    T value() const { return static_cast<T>(*this); }
};

// 便捷函数：创建if-then-else表达式
template <typename T> auto if_then(const ch_bool &condition, const T &value) {
    return multi_if<T>(condition, value);
}

// 优先级条件构建器（用于优化互斥条件）
template <typename T> class priority_if {
private:
    struct branch_info {
        ch_bool condition;
        T value;
    };

    std::vector<branch_info> branches_;
    bool has_else_ = false;
    std::source_location creation_loc_;

public:
    priority_if(const ch_bool &condition, const T &value,
                std::source_location sloc = std::source_location::current())
        : creation_loc_(sloc) {
        static_assert(ch::core::HardwareType<T> ||
                          std::is_arithmetic_v<std::remove_cvref_t<T>>,
                      "Only hardware types or arithmetic types are allowed in "
                      "if_ constructs");
        branches_.push_back({condition, value});
    }

    priority_if &elif (const ch_bool &condition, const T &value) {
        if (has_else_) {
            throw std::runtime_error("Cannot add elif after else");
        }
        branches_.push_back({condition, value});
        return *this;
    }

    priority_if &else_(const T &value) {
        if (has_else_) {
            throw std::runtime_error("Else branch already added");
        }
        branches_.push_back({ch_bool(true), value});
        has_else_ = true;
        return *this;
    }

    // 转换操作符：将条件语句转换为T类型的值
    operator T() const {
        if (branches_.empty()) {
            throw std::runtime_error("Empty if block");
        }

        T current_value = branches_.back().value;

        // 从倒数第二个开始向前处理，但第一个条件之后的条件会与前面的条件互斥
        for (auto it = branches_.rbegin() + 1; it != branches_.rend(); ++it) {
            // 使用当前值作为默认值，条件成立时选择对应值
            current_value =
                ::ch::core::select(it->condition, it->value, current_value);
        }

        return current_value;
    }

    // 便捷方法：获取最终值
    T value() const { return static_cast<T>(*this); }
};

// 便捷函数：创建优先级if-then-else表达式
template <typename T>
auto priority_if_then(const ch_bool &condition, const T &value) {
    return priority_if<T>(condition, value);
}

} // namespace chlib

#endif // CH_CHLIB_IF_H