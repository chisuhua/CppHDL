// include/module.h
#pragma once

#include "component.h"
#include "core/context.h"
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

        // 1. parent = **节点归属的唯一真源**（ADR-059 D2）。
        //    ⚠️ MUST NOT 用 `Component::current()` —— 那在**构造期**指向
        //    「构造中的祖先」（或 nullptr）⇒ 成员形态的 `ch_module` 会挂**祖父**
        //    或者直接消失（⇒ 之后 `io()` 解引用空指针 ⇒ SIGSEGV）。
        auto* parent_component = Component::node_owner();
        if (!parent_component) {
            // ADR-059 D8：MUST 响亮失败（`CHERROR` + `return` 会让后续 `io()`
            // 解引用空指针 ⇒ SIGSEGV ⇒ 与本仓「响亮失败」原则不符）。
            CHFATAL_EXCEPTION(
                "[ch_module<%s>] No active parent Component: `ch_module` MUST be "
                "created inside (a) a `Component` constructor body, or (b) a "
                "`Component::describe()` body, or (c) a `Component::create_child` "
                "/ `ch_device` managed construction. Creating it at TEST_CASE / "
                "freestanding scope has no owner to attach it to.",
                typeid(T).name());
        }

        // 2. build context = **环境 context**（ADR-059 D2′）。
        //    ⚠️ MUST NOT 用 `parent_component->context()` —— 成员形态的父组件
        //    在**构造期** `ctx_ == nullptr` ⇒ 早退 ⇒ 子组件整个消失。
        //    `describe()` 内创建时 env ctx == 父的 ctx（与变更前**相同**）
        //    ⇒ 零行为变化；成员形态时 env ctx 是祖父的 ctx 作用域
        //    ⇒ 仍是**同一条** ctx（因为父也是在那个作用域里构造的）。
        auto* env_ctx = ch::core::ctx_curr_;

        // 3. 构造子组件。
        //    ⚠️ 传**局部名**（不是全路径）—— 全路径由 `Component` 的构造函数计算
        //    （**唯一**拼接点）⇒ ⇒ 深度 ≥3 不再出现 `top.top.m.lf`（ADR-059 D3）。
        //    ⚠️ `ConstructionFrame` 必须包住 `make_unique` —— 子组件的
        //       基类构造压入的 CONSTRUCT 作用域帧由它在构造完成后截断
        //       （基类无法感知「派生类成员造完」）。
        std::unique_ptr<T> local_child_ptr;
        {
            Component::ConstructionFrame frame;
            local_child_ptr =
                std::make_unique<T>(parent_component, instance_name,
                                    std::forward<Args>(args)...);
        }
        CHDBG("Child path name: %s", local_child_ptr->name().c_str());

        // 4. Transfer ownership of the child component to the parent.
        auto shared_child = parent_component->add_child(std::move(local_child_ptr));
        child_component_sptr_ = std::static_pointer_cast<T>(shared_child);

        if (!child_component_sptr_) {
            CHFATAL_EXCEPTION(
                "[ch_module<%s>] Child component was destroyed immediately after "
                "creation!",
                typeid(T).name());
        }

        // 5. build 子组件。
        if (env_ctx != nullptr) {
            child_component_sptr_->build(env_ctx);
        } else {
            // ADR-059 D2′：环境 ctx 为空 MUST NOT 硬失败。
            // 下游装配形态是 `make_unique<Unit>()` **然后** `build(ctx)`
            // ⇒ Unit 的 ctor 跑在 `build` **之前** ⇒ `ctx_curr_` 常为 nullptr。
            // ⇒ ⇒ 把子组件的 build 推迟到宿主的 `build(ctx)`（那时 ctx 已知）。
            auto child = child_component_sptr_;
            parent_component->defer_build(
                [child](ch::core::context* ctx) { child->build(ctx); });
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
