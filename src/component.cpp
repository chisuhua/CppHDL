// src/component.cpp
#include "component.h"
#include "core/context.h"

namespace ch {

thread_local Component* Component::current_ = nullptr;

Component::Component(Component* parent, const std::string& name)
    : parent_(parent)
    , name_(name.empty() ? "unnamed" : name)
{
}

void Component::build(ch::core::context* external_ctx) {
    if (built_) return;
    built_ = true;

    ch::core::context* target_ctx = nullptr;
    ch::core::context* old_ctx = nullptr; // To store the context we are switching *from*
    Component* old_comp = nullptr; // To store the component we are switching *from*
 
    if (external_ctx) {
        // Use the provided external context (e.g., parent's context)
        target_ctx = external_ctx;

        old_ctx = ch::core::ctx_curr_;
        old_comp = current_;
        // Switch to the target context
        ch::core::ctx_curr_ = target_ctx;
        current_ = this;
        create_ports(); // using ctx_curr_
        describe(); //  using ctx_curr_

        ch::core::ctx_curr_ = old_ctx;
        current_ = old_comp;
        // std::cout << "[Component::build] Using external context " << target_ctx << std::endl; // Debug print
    } else {
        // Create own context (e.g., for top-level component via ch_device)
        auto parent_ctx = parent_ ? parent_->context() : nullptr;
        ctx_ = std::make_unique<ch::core::context>(name_, parent_ctx);
        target_ctx = ctx_.get();
        // Use RAII for own context - this is the traditional way
        // The RAII ctx_swap will handle saving/restoring old_ctx and current_
        ch::core::ctx_swap ctx_guard(target_ctx);
        old_comp = current_;
        current_ = this;
        // std::cout << "[Component::build] Created own context " << target_ctx << std::endl; // Debug print
        create_ports(); // using ctx_curr_ (which points to target_ctx via RAII)
        // std::cout << "[Component::build] Finished create_ports" << std::endl; // Debug print
        describe(); //  using ctx_curr_ (which points to target_ctx via RAII)
        // std::cout << "[Component::build] Finished describe" << std::endl; // Debug print
        current_ = old_comp;
        // ctx_guard's destructor automatically restores the old context
        // std::cout << "[Component::build] Restored context and current_" << std::endl; // Debug print
        return; // Early return for the own-context case to avoid the explicit restore below
        // std::cout << "[Component::build] Created own context " << target_ctx << std::endl; // Debug print
    }

}

Component* Component::add_child(std::unique_ptr<Component> child) {
    auto* raw_ptr = child.get();
    children_.push_back(std::move(child)); // Transfer ownership
    return raw_ptr;
}

} // namespace ch
