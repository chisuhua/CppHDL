// src/component.cpp
#include "component.h"
#include "core/context.h"
#include "logger.h"
#include "utils/destruction_manager.h"
#include <algorithm>
#include <iterator>
#include <set>
#include <typeinfo>

namespace ch {

thread_local Component *Component::current_ = nullptr;
thread_local std::vector<Component *> Component::scope_;

Component *Component::node_owner() {
    // D10：context::init() 期间（建 default_clock / default_reset）不归属任何组件
    if (ch::core::node_ownership_suppressed()) {
        return nullptr;
    }
    // 最内层「构造 / build 作用域」优先；空则回落 current_（与变更前一致）
    if (!scope_.empty()) {
        return scope_.back();
    }
    return current_;
}

Component::ConstructionFrame::ConstructionFrame() : depth_(scope_.size()) {}

Component::ConstructionFrame::~ConstructionFrame() {
    // 只截断到构造时的深度（栈比它短 ⇒ 已有人正确弹出 ⇒ 不动）
    if (scope_.size() > depth_) {
        scope_.resize(depth_);
    }
}

void Component::defer_build(std::function<void(ch::core::context *)> fn) {
    deferred_builds_.push_back(std::move(fn));
}

void Component::erase_scope_frame() {
    // 移除**最后一次**出现（一个组件最多一帧；用 rbegin 找最后一个更稳）
    auto it = std::find(scope_.rbegin(), scope_.rend(), this);
    if (it != scope_.rend()) {
        scope_.erase(std::next(it).base());
    }
}

void Component::rebind_children_parents() {
    // ADR-059 D9：成员形态的 `ch_module` 在**宿主构造期**创建子组件
    // ⇒ 那时宿主还没被 shared_ptr 接管 ⇒ `weak_from_this()` 为空
    // ⇒ ⇒ 子组件的 `parent_` 必须在这里（宿主 `build()` 时）补绑。
    auto self = weak_from_this();
    if (self.expired()) {
        return;  // 宿主本身不是 shared_ptr 管理的（栈上对象）⇒ 无从绑定
    }
    for (auto &child : children_shared_) {
        if (child) {
            child->parent_ = self;
        }
    }
}

Component::Component(Component *parent, const std::string &name)
    : ctx_(nullptr),
      local_name_(name.empty() ? "unnamed" : name),
      // ⚠️ 全路径由本构造函数计算（**唯一**拼接点）⇒ ⇒ `name()` 恒为全路径
      name_(build_hierarchical_name(parent, local_name_)) {
    CHDBG_FUNC();

    // ADR-059 D2″ 分支 ① 的基准：构造期的环境 ctx（可能为 nullptr）
    construction_ctx_ = ch::core::ctx_curr_;

    // ADR-059 §2.1 / D6①：进入 CONSTRUCT 作用域。
    // ⚠️ 基类构造**先于**派生类成员构造（C++ 保证）⇒ 压栈后，本组件的成员
    // 建的节点归属**本组件**（而不是构造中的祖先 / nullptr）。
    // ⚠️ 弹出由 `ConstructionFrame` 完成（基类无法感知「派生类成员造完」）。
    scope_.push_back(this);

    if (parent != nullptr) {
        // Attempt to obtain a weak_ptr to parent via enable_shared_from_this.
        // This succeeds when the parent is already owned by a shared_ptr
        // (e.g. created through create_child, add_child, or ch_device with
        // shared_ptr). For stack-allocated or unique_ptr-managed parents the
        // weak_from_this() call returns an empty weak_ptr and parent_ stays
        // empty; in that scenario the parent reference is unavailable through
        // the weak_ptr mechanism.
        parent_ = parent->weak_from_this();
        if (parent_.lock() == nullptr) {
            CHDBG("Parent '%s' is not shared_ptr-managed; parent reference "
                  "not stored in weak_ptr", parent->name().c_str());
        }
    }

    CHDBG("Creating component: %s with parent: %s", name_.c_str(),
          parent ? parent->name().c_str() : "null");
}

Component::Component(Component&& other) noexcept
    : ctx_(other.ctx_)
    , parent_(std::move(other.parent_))
    , local_name_(std::move(other.local_name_))
    , name_(std::move(other.name_))
    , children_shared_(std::move(other.children_shared_))
    , ctx_owner_(other.ctx_owner_)
    , construction_ctx_(other.construction_ctx_)
    , build_ctx_(other.build_ctx_)
    , deferred_builds_(std::move(other.deferred_builds_))
    , built_(other.built_)
    , destructing_(other.destructing_) {
    CHDBG_FUNC();
    // Limitation: children's parent_ weak_ptrs still reference 'other' (the
    // move source) after this constructor runs.  Because 'this' is not yet
    // managed by a shared_ptr at move time, shared_from_this() is unavailable
    // and the weak_ptrs cannot be updated.  If 'other' was itself shared_ptr-
    // managed, those weak_ptrs will expire when 'other' is destroyed, causing
    // children's parent() calls to return nullptr.  Move operations should
    // therefore only be used on components that have not yet been inserted into
    // a shared_ptr (i.e. before add_child / create_child).
    other.ctx_ = nullptr;
    other.ctx_owner_ = false;
    other.construction_ctx_ = nullptr;
    other.build_ctx_ = nullptr;
    other.built_ = false;
    other.destructing_ = false;
}

std::string Component::module_type_name() const {
    // ⚠️ 虚调用 ⇒ MUST 在**运行期**（构造完成后）才取得到派生类的覆写
    // （构造函数里调虚函数会取到**基类**版本 ⇒ ⇒ 故不在构造函数里调用）。
    if (const char* literal = layer_name_literal()) {
        return std::string(literal);
    }
    // ⚠️ 回落：同类型 ⇒ 同值 ✔；但 `typeid` 拼写**实现相关** ⇒ 跨平台不可复算
    // ⇒ ⇒ 契约限定「同一编译器 / 同一 ABI 内」。
    // ⚠️ 返回**未 sanitize** 的字面量 ⇒ 发射侧（Verilog 模块名）负责 sanitize。
    return std::string(typeid(*this).name());
}

Component& Component::operator=(Component&& other) noexcept {
    CHDBG_FUNC();
    if (this != &other) {
        // Release current context if we own it.
        if (ctx_owner_ && ctx_) {
            delete ctx_;
        }
        // Reset current children's parent references before releasing them.
        // This prevents them from accessing 'this' via an (about-to-be-
        // invalid) weak_ptr during their own destruction.
        for (auto& child : children_shared_) {
            if (child) {
                child->parent_.reset();
            }
        }

        ctx_ = other.ctx_;
        parent_ = std::move(other.parent_);
        local_name_ = std::move(other.local_name_);
        name_ = std::move(other.name_);
        children_shared_ = std::move(other.children_shared_);
        ctx_owner_ = other.ctx_owner_;
        construction_ctx_ = other.construction_ctx_;
        build_ctx_ = other.build_ctx_;
        deferred_builds_ = std::move(other.deferred_builds_);
        built_ = other.built_;
        destructing_ = other.destructing_;

        // The transferred children's parent_ weak_ptrs still reference 'other'
        // (the move source) and cannot be updated here for the same reason as
        // in the move constructor: 'this' is not yet (or still) managed by a
        // shared_ptr so shared_from_this() / weak_from_this() are unavailable.
        // See the move constructor comment for the associated constraint.

        other.ctx_ = nullptr;
        other.ctx_owner_ = false;
        other.construction_ctx_ = nullptr;
        other.build_ctx_ = nullptr;
        other.built_ = false;
        other.destructing_ = false;
    }
    return *this;
}

void Component::build(ch::core::context *external_ctx) {
    CHDBG_FUNC();

    if (built_) {
        // ADR-059 D2″ 分支 ②：**再次**以不同 ctx 调用 build MUST 被拒绝。
        // ⚠️ 守卫 MUST 在 `built_` 早退**之前** —— 否则重复 build 直接返回，
        //    守卫永远不生效（子模块节点留在旧 ctx ⇒ 新 ctx 的 eval list
        //    看不见整块子模块 ⇒ **静默消失**）。
        if (external_ctx != nullptr && build_ctx_ != nullptr &&
            external_ctx != build_ctx_) {
            CHFATAL_EXCEPTION(
                "Component '%s' was already built into context '%s' but build() "
                "was called again with a different context '%s' "
                "(cross-context re-assembly is rejected: the sub-tree's nodes "
                "would stay in the old context and silently disappear from the "
                "new one)",
                name_.c_str(), build_ctx_->name().c_str(),
                external_ctx->name().c_str());
        }
        CHDBG("Component already built: %s", name_.c_str());
        return;
    }

    built_ = true;
    ch::core::context *target_ctx = nullptr;

    if (external_ctx) {
        CHDBG("Using external context for component: %s", name_.c_str());
        ctx_ = external_ctx;
        target_ctx = external_ctx;
    } else {
        auto parent_ptr = parent_.lock();
        auto parent_ctx = parent_ptr ? parent_ptr->context() : nullptr;
        CHDBG("Creating internal context for component: %s", name_.c_str());
        ctx_ = new ch::core::context(name_, parent_ctx);
        target_ctx = ctx_;
        ctx_owner_ = true;
    }

    // ADR-059 D2″：跨 ctx 守卫 —— 归属只在**一个** ctx 里成立。
    // 分支 ①：构造期**有**环境 ctx ⇒ 它就是基准（MUST 相等）。
    // ⚠️ 不等时的后果是**静默**的：子模块节点在 ctx1、宿主端口在 ctx2
    // ⇒ `ctx2->get_eval_list()` 看不见子模块 ⇒ 整块逻辑消失，且等价性判据
    // 也看不见（子模块不在树里）。⇒ ⇒ MUST 响亮拒绝。
    if (construction_ctx_ != nullptr && target_ctx != construction_ctx_) {
        CHFATAL_EXCEPTION(
            "Component '%s' was constructed inside context '%s' but build() was "
            "called with a different context '%s' "
            "(cross-context re-assembly is rejected: nodes created during "
            "construction stay in the construction context)",
            name_.c_str(), construction_ctx_->name().c_str(),
            target_ctx->name().c_str());
    }
    build_ctx_ = target_ctx;

    // ADR-059 D9：宿主已被 shared_ptr 接管 ⇒ 现在才可能把子组件的 parent_ 绑上
    rebind_children_parents();

    // ADR-059 D2′：把「构造期拿不到 ctx」的子组件 build 补齐。
    // ⚠️ MUST 在 `build_internal()` **之前** —— 宿主的 `describe()` 会用到
    // 子组件端口（`sub.io().x <<= ...`）⇒ 端口 MUST 已存在。
    if (!deferred_builds_.empty()) {
        auto pending = std::move(deferred_builds_);
        deferred_builds_.clear();
        for (auto &fn : pending) {
            fn(target_ctx);
        }
    }

    build_internal(target_ctx);
    CHDBG("Component build completed: %s", name_.c_str());
}

void Component::build_internal(ch::core::context *target_ctx) {
    CHDBG_FUNC();
    CHREQUIRE(target_ctx != nullptr, "Target context cannot be null");

    ch::core::ctx_swap ctx_guard(target_ctx);
    Component *old_comp = current_;
    current_ = this;

    // ADR-059 D6②：BUILD 作用域（RAII 弹出 ⇒ 与压入配对）
    scope_.push_back(this);

    CHSCOPE_EXIT({
        scope_.pop_back();
        current_ = old_comp;
    });

    CHDBG("Creating ports for component: %s", name_.c_str());
    create_ports();

    CHDBG("Describing behavior for component: %s", name_.c_str());
    describe();
}

Component::~Component() {
    CHDBG_FUNC();

    destructing_ = true;

    // 作用域兜底：**直接构造**的组件（未经 ConstructionFrame 包裹）会残留
    // 一个作用域帧 ⇒ 析构时移除（否则之后所有"组件外"建的节点都归错它）。
    erase_scope_frame();

    CHDBG("Component destructor normal cleanup: %s", name_.c_str());

    // Clear children. Because children hold only a weak_ptr to this component,
    // there is no reference cycle; when the shared_ptrs in children_shared_ are
    // released, the children's weak_ptr (parent_) to this object will
    // automatically expire, so no manual nulling of parent_ is required.
    children_shared_.clear();

    if (ctx_owner_ && ctx_) {
        delete ctx_;
        ctx_ = nullptr;
    }
}

std::shared_ptr<Component>
Component::add_child(std::unique_ptr<Component> child) {
    CHDBG_FUNC();
    CHREQUIRE(child != nullptr, "Cannot add null child component");

    auto shared_child = std::shared_ptr<Component>(std::move(child));
    children_shared_.push_back(shared_child);
    // ADR-059 D9：补绑 parent_。
    // ⚠️ 成员形态的 `ch_module` 调用本函数时，**本组件还在构造中**
    // ⇒ `weak_from_this()` 为空 ⇒ 这次绑定不生效 ⇒ ⇒ `build()` 里会再绑一次
    // （`rebind_children_parents()`）⇒ ⇒ 两处合起来才覆盖全部路径。
    auto self = weak_from_this();
    if (!self.expired()) {
        shared_child->parent_ = self;
    }
    CHDBG("Added child component: %s", shared_child->name().c_str());
    return shared_child;
}

} // namespace ch