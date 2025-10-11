// include/module.h
#ifndef CH_MODULE_H
#define CH_MODULE_H

#include "component.h"
#include "logger.h"
#include <memory>
#include <string>
#include <typeinfo>

namespace ch {

// Helper function to construct the hierarchical path name
std::string build_hierarchical_name(Component* parent, const std::string& local_name) {
    CHDBG_FUNC();
    
    if (!parent) {
        CHDBG("No parent, returning local name: %s", local_name.c_str());
        return local_name;
    }
    
    std::string parent_path = parent->hierarchical_name();
    CHDBG("Parent path: '%s', local name: '%s'", parent_path.c_str(), local_name.c_str());
    
    if (parent_path.empty() || parent_path == "unnamed") {
        CHDBG("Parent path empty or unnamed, returning local name: %s", local_name.c_str());
        return local_name;
    }
    
    std::string result = parent_path + "." + local_name;
    CHDBG("Built hierarchical name: %s", result.c_str());
    return result;
}

template <typename T, typename... Args>
class ch_module {
public:
    explicit ch_module(const std::string& instance_name, Args&&... args) {
        CHDBG_FUNC();
        CHINFO("[ch_module::ctor] Creating module for component %s", typeid(T).name());

        // 1. Get the currently active component (the parent)
        auto* parent_component = Component::current();
        if (!parent_component) {
            CHERROR("[ch_module::ctor] Error: No active parent Component found when creating ch_module!");
            return;
        }

        // 2. Get the parent's context pointer
        auto* parent_context = parent_component->context();
        if (!parent_context) {
            CHERROR("[ch_module::ctor] Error: Parent Component has no valid context!");
            return;
        }

        // 3. Construct the hierarchical path name
        std::string child_path_name = build_hierarchical_name(parent_component, instance_name);
        CHDBG("Child path name: %s", child_path_name.c_str());

        // 4. Create the child component instance
        auto local_child_ptr = std::make_unique<T>(parent_component, child_path_name, std::forward<Args>(args)...);

        // 5. Transfer ownership of the child component to the parent.
        // Get shared_ptr from parent's add_child method
        auto shared_child = parent_component->add_child(std::move(local_child_ptr));
        child_component_sptr_ = std::static_pointer_cast<T>(shared_child);

        // 6. Build the child component within its own temporary context
        if (auto locked_ptr = child_component_sptr_.lock()) {
            locked_ptr->build(parent_context);
        } else {
            CHERROR("[ch_module::ctor] Error: Child component was destroyed immediately after creation!");
        }

        CHINFO("[ch_module::ctor] Finished creating module");
    }

    // Provide access to the child component's I/O and instance
    auto& io() {
        CHDBG_FUNC();
        
        auto shared = child_component_sptr_.lock();
        if (!shared) {
            CHFATAL("Child component has been destroyed unexpectedly in io()!");
        }
        return shared->io();
    }

    T& instance() {
        CHDBG_FUNC();
        
        auto shared = child_component_sptr_.lock();
        if (!shared) {
            CHFATAL("Child component has been destroyed unexpectedly in instance()!");
        }
        return *shared;
    }

private:
    std::weak_ptr<T> child_component_sptr_; // 使用 weak_ptr 存储
};

#define CH_MODULE(type, name, ...) ch::ch_module<type> name(#name, ##__VA_ARGS__)

} // namespace ch
#endif // CH_MODULE_H
