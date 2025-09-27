// include/component.h
#ifndef COMPONENT_H
#define COMPONENT_H

#include "core/context.h"
#include <string>
#include <memory>

namespace ch {

class Component {
public:
    explicit Component(Component* parent = nullptr, const std::string& name = "");
    virtual ~Component() = default;

    virtual void describe() = 0;
    void build(); // 新增：显式构建 IR

    // 访问器
    ch::core::context* context() const { return ctx_.get(); }
    Component* parent() const { return parent_; }
    const std::string& name() const { return name_; }

    // 静态接口：获取当前活跃 Component
    static Component* current() { return current_; }

protected:
    std::unique_ptr<ch::core::context> ctx_;
    Component* parent_;
    std::string name_;

private:
    static thread_local Component* current_;  // 当前建模中的 Component
    bool built_ = false;  // 防止重复 build
  
    // 禁止拷贝/移动
    Component(const Component&) = delete;
    Component& operator=(const Component&) = delete;
    Component(Component&&) = delete;
    Component& operator=(Component&&) = delete;

};

namespace internal {
// 供 ch_device / ch_module 使用
Component* current_component();
void set_current_component(Component* comp);
} // namespace internal

} // namespace ch
#endif // COMPONENT_H
