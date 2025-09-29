// include/component.h
#ifndef COMPONENT_H
#define COMPONENT_H

#include "io.h"
#include "core/context.h"
#include <string>
#include <memory>
#include <vector>

namespace ch {

class Component {
public:
    explicit Component(Component* parent = nullptr, const std::string& name = "");
    virtual ~Component() = default;

    virtual void create_ports() {}
    virtual void describe() = 0;
    void build(ch::core::context* external_ctx = nullptr); // 式构建 IR

    // 访问器
    ch::core::context* context() const { return ctx_.get(); }
    Component* parent() const { return parent_; }
    const std::string& name() const { return name_; }

    static Component* current() { return current_; }

    Component* add_child(std::unique_ptr<Component> child);
protected:
    std::unique_ptr<ch::core::context> ctx_;
    Component* parent_;
    std::string name_;
    std::vector<std::unique_ptr<Component>> children_;

private:
    static thread_local Component* current_;
    bool built_ = false;
  
    // 禁止拷贝/移动
    Component(const Component&) = delete;
    Component& operator=(const Component&) = delete;
    Component(Component&&) = delete;
    Component& operator=(Component&&) = delete;

};

} // namespace ch
#endif // COMPONENT_H
