// include/component.h
#ifndef COMPONENT_H
#define COMPONENT_H

#include "io.h"
#include "logger.h"
#include <string>
#include <memory>
#include <vector>

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

    static Component* current() { return current_; }
    static void set_current(Component* comp) { current_ = comp; }

    // 统一的子组件管理方法
    template<typename T, typename... Args>
    T* create_child(const std::string& name, Args&&... args) {
        CHDBG_FUNC();
        CHREQUIRE(!name.empty(), "Child component name cannot be empty");
        
        try {
            auto child = std::make_unique<T>(this, name, std::forward<Args>(args)...);
            T* raw_ptr = child.get();
            children_.push_back(std::move(child));
            CHDBG("Created child component: %s", name.c_str());
            return raw_ptr;
        } catch (const std::bad_alloc&) {
            CHERROR("Failed to allocate memory for child component: %s", name.c_str());
            return nullptr;
        } catch (const std::exception& e) {
            CHERROR("Failed to create child '%s': %s", name.c_str(), e.what());
            return nullptr;
        }
    }

    Component* add_child(std::unique_ptr<Component> child);
    size_t child_count() const { return children_.size(); }
    const std::vector<std::unique_ptr<Component>>& children() const { return children_; }

protected:
    std::unique_ptr<ch::core::context> ctx_;
    Component* parent_;
    std::string name_;
    std::vector<std::unique_ptr<Component>> children_;

private:
    static thread_local Component* current_;
    bool built_ = false;
    
    void build_internal(ch::core::context* target_ctx);

    Component(const Component&) = delete;
    Component& operator=(const Component&) = delete;
    Component(Component&&) = delete;
    Component& operator=(Component&&) = delete;
};

} // namespace ch
#endif // COMPONENT_H
