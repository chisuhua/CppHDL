// include/component.h
#ifndef COMPONENT_H
#define COMPONENT_H

#include "io.h"
#include "logger.h"
#include <string>
#include <memory>
#include <vector>
#include <unordered_set>

namespace ch { namespace core { class context; } }

namespace ch {

class Component : public std::enable_shared_from_this<Component> {
public:
    explicit Component(Component* parent = nullptr, const std::string& name = "");
    virtual ~Component();

    // Move operations: valid only before the object is managed by a shared_ptr.
    // Once owned by shared_ptr (e.g. via create_child / add_child), the
    // underlying shared_ptr control block is bound to the original object
    // address.  Moving the object after that point causes children's parent_
    // weak_ptrs to become stale (they still reference the move source).
    // Prefer moving the shared_ptr itself (std::move(shared_ptr<T>)) instead.
    Component(Component&& other) noexcept;
    Component& operator=(Component&& other) noexcept;

    virtual void create_ports() {}
    virtual void describe() = 0;
    void build(ch::core::context* external_ctx = nullptr);

    // 访问器
    ch::core::context* context() const { return ctx_; }
    std::shared_ptr<Component> parent() const { return parent_.lock(); }
    const std::string& name() const { return name_; }

    static Component* current() { return current_; }
    static void set_current(Component* comp) { current_ = comp; }

    // 统一的子组件管理方法 - 返回 shared_ptr
    template<typename T, typename... Args>
    std::shared_ptr<T> create_child(const std::string& name, Args&&... args) {
        CHDBG_FUNC();
        CHREQUIRE(!name.empty(), "Child component name cannot be empty");
        
        try {
            auto child = std::make_shared<T>(this, name, std::forward<Args>(args)...);
            children_shared_.push_back(std::static_pointer_cast<Component>(child));
            CHDBG("Created child component: %s", name.c_str());
            return child;
        } catch (const std::bad_alloc&) {
            CHERROR("Failed to allocate memory for child component: %s", name.c_str());
            return nullptr;
        } catch (const std::exception& e) {
            CHERROR("Failed to create child '%s': %s", name.c_str(), e.what());
            return nullptr;
        }
    }

    std::string hierarchical_name() const {
        auto p = parent_.lock();
        if (!p) {
            return name_;
        }
        
        std::string parent_path = p->hierarchical_name();
        if (parent_path.empty() || parent_path == "unnamed") {
            return name_;
        }
        
        return parent_path + "." + name_;
    }

    // 修改返回类型为 shared_ptr
    std::shared_ptr<Component> add_child(std::unique_ptr<Component> child);
    size_t child_count() const { return children_shared_.size(); }
    const std::vector<std::shared_ptr<Component>>& children() const { return children_shared_; }

protected:
    ch::core::context* ctx_;
    // Stored as weak_ptr to avoid a reference cycle: each child holds a
    // weak reference to its parent so the parent's lifetime is governed
    // solely by its owner (e.g. ch_device or children_shared_ of the
    // grandparent), not by its children.
    std::weak_ptr<Component> parent_;
    std::string name_;
    std::vector<std::shared_ptr<Component>> children_shared_;
    bool ctx_owner_ = false;

private:
    static thread_local Component* current_;
    bool built_ = false;
    bool destructing_ = false;  // 标记是否正在析构
    
    void build_internal(ch::core::context* target_ctx);

    Component(const Component&) = delete;
    Component& operator=(const Component&) = delete;
};

} // namespace ch
#endif // COMPONENT_H