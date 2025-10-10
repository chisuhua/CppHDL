// src/component.cpp
#include "component.h"
#include "core/context.h"
#include "logger.h"

namespace ch {

thread_local Component* Component::current_ = nullptr;

Component::Component(Component* parent, const std::string& name)
    : parent_(parent)
    , name_(name.empty() ? "unnamed" : name) {
    CHDBG_FUNC();
    CHDBG("Creating component: %s with parent: %s", 
          name_.c_str(), parent_ ? parent_->name().c_str() : "null");
}

void Component::build(ch::core::context* external_ctx) {
    CHDBG_FUNC();
    
    if (built_) {
        CHDBG("Component already built: %s", name_.c_str());
        return;
    }
    
    built_ = true;
    std::unique_ptr<ch::core::context> local_ctx;
    ch::core::context* target_ctx = nullptr;
    
    if (external_ctx) {
        CHDBG("Using external context for component: %s", name_.c_str());
        target_ctx = external_ctx;
    } else {
        auto parent_ctx = parent_ ? parent_->context() : nullptr;
        CHDBG("Creating internal context for component: %s", name_.c_str());
        local_ctx = std::make_unique<ch::core::context>(name_, parent_ctx);
        target_ctx = local_ctx.get();
        ctx_ = std::move(local_ctx);
    }

    build_internal(target_ctx);
    CHDBG("Component build completed: %s", name_.c_str());
}

void Component::build_internal(ch::core::context* target_ctx) {
    CHDBG_FUNC();
    CHREQUIRE(target_ctx != nullptr, "Target context cannot be null");
    
    ch::core::ctx_swap ctx_guard(target_ctx);
    Component* old_comp = current_;
    current_ = this;
    
    CHSCOPE_EXIT({
        current_ = old_comp;
    });
    
    CHDBG("Creating ports for component: %s", name_.c_str());
    create_ports();
    
    CHDBG("Describing behavior for component: %s", name_.c_str());
    describe();
}

Component* Component::add_child(std::unique_ptr<Component> child) {
    CHDBG_FUNC();
    CHREQUIRE(child != nullptr, "Cannot add null child component");
    
    auto* raw_ptr = child.get();
    children_.push_back(std::move(child));
    CHDBG("Added child component: %s", raw_ptr ? raw_ptr->name().c_str() : "unknown");
    return raw_ptr;
}

} // namespace ch
