// src/component.cpp
#include "component.h"
#include "core/context.h"
#include "logger.h"
#include "utils/destruction_manager.h"
#include <set>

namespace ch {

/*thread_local*/ Component *Component::current_ = nullptr;

Component::Component(Component *parent, const std::string &name)
    : ctx_(nullptr), parent_(parent), name_(name.empty() ? "unnamed" : name) {
    CHDBG_FUNC();
    CHDBG("Creating component: %s with parent: %s", name_.c_str(),
          parent_ ? parent_->name().c_str() : "null");

    // Register with destruction manager
    // ch::detail::destruction_manager::instance().register_component(this);
}

void Component::build(ch::core::context *external_ctx) {
    CHDBG_FUNC();

    if (built_) {
        CHDBG("Component already built: %s", name_.c_str());
        return;
    }

    built_ = true;
    ch::core::context *target_ctx = nullptr;

    if (external_ctx) {
        CHDBG("Using external context for component: %s", name_.c_str());
        target_ctx = external_ctx;
    } else {
        auto parent_ctx = parent_ ? parent_->context() : nullptr;
        CHDBG("Creating internal context for component: %s", name_.c_str());
        ctx_ = new ch::core::context(name_, parent_ctx);
        target_ctx = ctx_;
        ctx_owner_ = true;
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

    CHSCOPE_EXIT({ current_ = old_comp; });

    CHDBG("Creating ports for component: %s", name_.c_str());
    create_ports();

    CHDBG("Describing behavior for component: %s", name_.c_str());
    describe();
}

Component::~Component() {
    CHDBG_FUNC();

    // 标记正在析构，防止重复析构
    destructing_ = true;

    // Unregister from destruction manager
    // ch::detail::destruction_manager::instance().unregister_component(this);

    // Check if we're in static destruction phase
    // if (ch::detail::in_static_destruction()) {
    //     // During static destruction, minimize operations to prevent
    //     segfaults CHDBG("Component destructor called during static
    //     destruction: %s", name_.c_str()); return;
    // }

    CHDBG("Component destructor normal cleanup: %s", name_.c_str());

    // 清理子组件
    // FIXME it cause segfault
    // children_shared_.clear();

    // 如果拥有context，则删除它
    if (ctx_owner_ && ctx_) {
        // 在删除context之前，确保所有依赖它的simulator都已断开连接
        // ch::detail::destruction_manager::instance().notify_context_destruction(ctx_);
        delete ctx_;
        ctx_ = nullptr;
    }
}

// 修改返回类型和实现
std::shared_ptr<Component>
Component::add_child(std::unique_ptr<Component> child) {
    CHDBG_FUNC();
    CHREQUIRE(child != nullptr, "Cannot add null child component");

    auto shared_child = std::shared_ptr<Component>(std::move(child));
    children_shared_.push_back(shared_child);
    CHDBG("Added child component: %s", shared_child->name().c_str());
    return shared_child;
}

} // namespace ch