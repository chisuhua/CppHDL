// include/device.h （重构后）
#pragma once
#include "component.h"

namespace ch {

template <typename T, typename... Args>
class ch_device {
public:
    explicit ch_device(Args&&... args)
        : top_(std::make_unique<T>(nullptr, "top", std::forward<Args>(args)...))
    {
        top_->build();
    }

    T& instance() { return *top_; }
    const T& instance() const { return *top_; }

    // 供仿真器/后端使用
    ch::core::context* context() const { return top_->context(); }

private:
    std::unique_ptr<T> top_;
};

} // namespace ch
