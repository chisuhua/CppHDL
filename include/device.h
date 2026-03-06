// include/device.h
#pragma once
#include "component.h"
#include "context.h"

namespace ch {

template <typename T, typename... Args>
class ch_device {
public:
    explicit ch_device(Args&&... args)
    {
        auto ctx = std::make_unique<ch::core::context>("top_ctx");
        ch::core::ctx_swap ctx_guard(ctx.get());
        // Use shared_ptr so that enable_shared_from_this (used by Component
        // for weak_ptr parent tracking) is properly initialised for the
        // top-level instance and any children it creates.
        top_ = std::make_shared<T>(nullptr, "top", std::forward<Args>(args)...);
        top_->build();
    }

    T& instance() { return *top_; }
    const T& instance() const { return *top_; }
    ch::core::context* context() const { return top_->context(); }
    
    // 添加io()方法，暴露顶层模块的IO接口
    auto& io() { return top_->io(); }
    const auto& io() const { return top_->io(); }

private:
    std::shared_ptr<T> top_;
};

} // namespace ch