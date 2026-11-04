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
        // Allocate the device-level context first so that the ch_in/ch_out
        // members constructed by T (and any opimpls created by describe()) all
        // land in the same context.  Mixing them across two contexts (the
        // older "top_ctx for IO, child context for ops" design) caused node-id
        // aliasing that corrupted the JIT data_buffer_ and dropped the
        // component's output ports from the simulator's data_map.  See
        // .omo/evidence/task-6-debug-notes.md for the full analysis.
        ctx_ = std::make_unique<ch::core::context>("top_ctx");
        ch::core::ctx_swap ctx_guard(ctx_.get());
        {
            // ADR-059 §2.1 / D6①：顶层组件的 CONSTRUCT 作用域帧在这里截断
            // （基类构造无法感知「派生类成员造完」，唯一知道的那一方是这里）⇒
            // 顶层的成员节点（`ch_reg` / `ch_mem` / bundle 字段）归属**顶层组件**，
            // 而不是「无归属」。
            ch::Component::ConstructionFrame frame;
            top_ = std::make_shared<T>(nullptr, "top", std::forward<Args>(args)...);
        }
        top_->build(ctx_.get());
    }

    T& instance() { return *top_; }
    const T& instance() const { return *top_; }
    ch::core::context* context() const { return top_->context(); }
    
    // 添加io()方法，暴露顶层模块的IO接口
    auto& io() { return top_->io(); }
    const auto& io() const { return top_->io(); }

private:
    std::shared_ptr<T> top_;
    std::unique_ptr<ch::core::context> ctx_;
};

} // namespace ch