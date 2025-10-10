// include/component.h
#ifndef COMPONENT_H
#define COMPONENT_H

#include "io.h"
#include <string>
#include <memory>
#include <vector>

// 只前向声明，不包含具体实现
namespace ch { namespace core { class context; } }

namespace ch {

class Component {
public:
    explicit Component(Component* parent = nullptr, const std::string& name = "");
    virtual ~Component() = default;

    virtual void create_ports() {}
    virtual void describe() = 0;
    void build(ch::core::context* external_ctx = nullptr);

    // 访问器
    ch::core::context* context() const { return ctx_.get(); }
    Component* parent() const { return parent_; }
    const std::string& name() const { return name_; }

    // 获取当前组件（线程局部）
    static Component* current() { return current_; }
    
    // 设置当前组件（用于特殊场景）
    static void set_current(Component* comp) { current_ = comp; }

    // 统一的子组件管理方法
    template<typename T, typename... Args>
    T* create_child(const std::string& name, Args&&... args) {
        static_assert(std::is_base_of_v<Component, T>, "T must derive from Component");
        auto child = std::make_unique<T>(this, name, std::forward<Args>(args)...);
        T* raw_ptr = child.get();
        children_.push_back(std::move(child));
        return raw_ptr;
    }

    Component* add_child(std::unique_ptr<Component> child);
    
    // 获取子组件数量
    size_t child_count() const { return children_.size(); }
    
    // 获取所有子组件（只读）
    const std::vector<std::unique_ptr<Component>>& children() const { return children_; }

protected:
    std::unique_ptr<ch::core::context> ctx_;
    Component* parent_;
    std::string name_;
    std::vector<std::unique_ptr<Component>> children_;

private:
    static thread_local Component* current_;
    bool built_ = false;
    
    // 内部构建方法
    void build_internal(ch::core::context* target_ctx);

    // 禁止拷贝/移动
    Component(const Component&) = delete;
    Component& operator=(const Component&) = delete;
    Component(Component&&) = delete;
    Component& operator=(Component&&) = delete;
};

} // namespace ch
#endif // COMPONENT_H
