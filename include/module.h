// include/module.h
#pragma once

#include "component.h"
#include "logger.h"
#include <memory>
#include <string>
#include <typeinfo>

namespace ch {

// ⚠️ `build_hierarchical_name()` 已**移到** `include/component.h`
// （那里是**唯一**的全路径拼接点 —— 留在本头会与 `Component` 的构造函数
// 构成**两个真源** ⇒ 前缀重复会复发）。
// ⇒ ⇒ 本头经 `#include "component.h"` 继续**对外可见**该函数（签名与语义不变）。
// ⇒ ⇒ 它 MUST 用 `parent->name()`（**不是** `hierarchical_name()`）—— ADR-059 D3。

template <typename T, typename... Args>
class ch_module {
public:
    explicit ch_module(const std::string& instance_name, Args&&... args) {
        CHDBG_FUNC();
        CHINFO("[ch_module::ctor] Creating module for component %s", typeid(T).name());

        // 1. Get the currently active component (the parent)
        auto* parent_component = Component::current();
        if (!parent_component) {
            CHERROR("[ch_module::ctor] Error: No active parent Component found when creating ch_module!");
            return;
        }

        // 2. Get the parent's context pointer
        auto* parent_context = parent_component->context();
        if (!parent_context) {
            CHERROR("[ch_module::ctor] Error: Parent Component has no valid context!");
            return;
        }

        // 3. Create the child component instance
        //    ⚠️ 传**局部名**（不是全路径）—— 全路径由 `Component` 的构造函数计算
        //    （**唯一**拼接点）⇒ ⇒ 深度 ≥3 不再出现 `top.top.m.lf`（ADR-059 D3）。
        auto local_child_ptr = std::make_unique<T>(parent_component, instance_name, std::forward<Args>(args)...);
        CHDBG("Child path name: %s", local_child_ptr->name().c_str());

        // 5. Transfer ownership of the child component to the parent.
        // Get shared_ptr from parent's add_child method
        auto shared_child = parent_component->add_child(std::move(local_child_ptr));
        child_component_sptr_ = std::static_pointer_cast<T>(shared_child);

        // 6. Build the child component within its own temporary context
        if (child_component_sptr_) {
            child_component_sptr_->build(parent_context);
        } else {
            CHERROR("[ch_module::ctor] Error: Child component was destroyed immediately after creation!");
        }

        CHINFO("[ch_module::ctor] Finished creating module");
    }

    // Provide access to the child component's I/O and instance
    auto& io() {
        CHDBG_FUNC();
        if (!child_component_sptr_) {
            CHFATAL("Child component has been destroyed unexpectedly in io()!");
        }
        return child_component_sptr_->io();
    }

    T& instance() {
        CHDBG_FUNC();
        if (!child_component_sptr_) {
            CHFATAL("Child component has been destroyed unexpectedly in instance()!");
        }
        return *child_component_sptr_;
    }

private:
    std::shared_ptr<T> child_component_sptr_; // 使用 shared_ptr 保持组件生命周期
};
} // namespace ch
