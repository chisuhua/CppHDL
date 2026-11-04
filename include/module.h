// include/module.h
#pragma once

#include "component.h"
#include "core/context.h"
#include "logger.h"
#include <memory>
#include <string>
#include <type_traits>
#include <typeinfo>

namespace ch {

// ⚠️ `build_hierarchical_name()` 已**移到** `include/component.h`
// （那里是**唯一**的全路径拼接点 —— 留在本头会与 `Component` 的构造函数
// 构成**两个真源** ⇒ 前缀重复会复发）。
// ⇒ ⇒ 本头经 `#include "component.h"` 继续**对外可见**该函数（签名与语义不变）。
// ⇒ ⇒ 它 MUST 用 `parent->name()`（**不是** `hierarchical_name()`）—— ADR-059 D3。

/// 实例级粒度声明（`ch_layer<T>` / `ch_inline<T>` 用的**唯一**载体）。
///
/// ⚠️ 为什么不用「给 `ch_module` 加一个 tag 参数」：`ch_module` 的构造参数是
/// **转发给组件的构造函数**的 ⇒ tag 参数会要求**每个**组件的 ctor 都接受它
/// ⇒ ⇒ 破坏现有组件。⇒ ⇒ 改为**受保护的第二个 ctor**（公开签名**零变更**）。
struct InstanceDecl {
    /// `ch_layer` 的实例级模块类型名（档 ③）；`nullptr` ⇒ 继承类级名（档 ②）
    const char* module_type_name = nullptr;
    /// 显式 inline（`ch_inline`）
    bool force_inline = false;
};

template <typename T, typename... Args>
class ch_module {
public:
    explicit ch_module(const std::string& instance_name, Args&&... args)
        : ch_module(InstanceDecl{}, instance_name, std::forward<Args>(args)...) {
    }

protected:
    /// 受保护入口（公开面只有 `ch_module` 的原 ctor + `ch_layer` / `ch_inline`）。
    ch_module(const InstanceDecl& decl, const std::string& instance_name,
              Args&&... args) {
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

        // 3.5 实例级粒度声明（`ch_layer` / `ch_inline`）—— MUST 在 `build()` **之前**
        //     写入（否则组件在自己的 `describe()` 里读到的 `is_layer()` 是错的）。
        if (decl.force_inline) {
            local_child_ptr->set_instance_layer(false);
        } else if (decl.module_type_name != nullptr) {
            local_child_ptr->set_instance_layer(true);
            local_child_ptr->set_instance_module_type_name(decl.module_type_name);
        }

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

public:
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

namespace detail {
/// `T` 是否**类级**声明成层（`CH_HIERARCHY_LAYER`）—— **编译期**可判定
/// ⇒ ⇒ `ch_layer<T>` 才能在「② 继承」与「③ 必须另给」之间**编译期**分派。
template <typename T, typename = void>
struct has_class_layer_name : std::false_type {};

template <typename T>
struct has_class_layer_name<T, std::void_t<decltype(T::kChIsLayer)>>
    : std::bool_constant<T::kChIsLayer> {};
} // namespace detail

/// **显式成层**（实例级，ADR-061 决策①）。
///
/// ⚠️ 模块类型名的来源按 ADR-059 §2.2.1 的四档：
///   · `T` **已**类级声明 ⇒ **继承**类级名字（⇒ MUST NOT 另给 ⇒ 单参形态）
///   · `T` **未**声明（helper 被提升）⇒ MUST **在实例处**给名
///     ⇒ ⇒ **缺名是编译期错误**（没有可用重载 ⇒ 不会推迟到发射时的运行期硬错）
template <typename T, typename... Args>
class ch_layer : public ch_module<T, Args...> {
public:
    /// 档 ②：`T` 已声明成层 ⇒ 继承其类级名字
    explicit ch_layer(const std::string& instance_name, Args&&... args)
        requires detail::has_class_layer_name<T>::value
        : ch_module<T, Args...>(InstanceDecl{}, instance_name,
                                std::forward<Args>(args)...) {}

    /// 档 ③：`T` 未声明成层 ⇒ MUST 在实例处给模块类型名
    ch_layer(const std::string& instance_name, const std::string& module_type_name,
             Args&&... args)
        requires(!detail::has_class_layer_name<T>::value)
        : ch_module<T, Args...>(InstanceDecl{module_type_name.c_str()}, instance_name,
                                std::forward<Args>(args)...) {}
};

/// **显式 inline**（实例级覆盖，即使 `T` 类级声明成层）。
template <typename T, typename... Args>
class ch_inline : public ch_module<T, Args...> {
public:
    explicit ch_inline(const std::string& instance_name, Args&&... args)
        : ch_module<T, Args...>(InstanceDecl{nullptr, /*force_inline=*/true},
                                instance_name, std::forward<Args>(args)...) {}
};

} // namespace ch
