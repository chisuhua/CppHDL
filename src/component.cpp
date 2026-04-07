// src/component.cpp
#include "component.h"
#include "core/context.h"
#include "logger.h"
#include "utils/destruction_manager.h"
#include <set>

namespace ch {

thread_local Component *Component::current_ = nullptr;

Component::Component(Component *parent, const std::string &name)
    : ctx_(nullptr), name_(name.empty() ? "unnamed" : name) {
    CHDBG_FUNC();

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
    , name_(std::move(other.name_))
    , children_shared_(std::move(other.children_shared_))
    , ctx_owner_(other.ctx_owner_)
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
    other.built_ = false;
    other.destructing_ = false;
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
        name_ = std::move(other.name_);
        children_shared_ = std::move(other.children_shared_);
        ctx_owner_ = other.ctx_owner_;
        built_ = other.built_;
        destructing_ = other.destructing_;

        // The transferred children's parent_ weak_ptrs still reference 'other'
        // (the move source) and cannot be updated here for the same reason as
        // in the move constructor: 'this' is not yet (or still) managed by a
        // shared_ptr so shared_from_this() / weak_from_this() are unavailable.
        // See the move constructor comment for the associated constraint.

        other.ctx_ = nullptr;
        other.ctx_owner_ = false;
        other.built_ = false;
        other.destructing_ = false;
    }
    return *this;
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
        auto parent_ptr = parent_.lock();
        auto parent_ctx = parent_ptr ? parent_ptr->context() : nullptr;
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

    destructing_ = true;

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
    CHDBG("Added child component: %s", shared_child->name().c_str());
    return shared_child;
}

} // namespace ch