// include/ch_module.h
#ifndef CH_MODULE_H
#define CH_MODULE_H

#include "component.h"
#include <memory>
#include <iostream>
#include <string>

namespace ch {

// Helper function to construct the hierarchical path name
std::string build_hierarchical_name(Component* parent, const std::string& local_name) {
    if (!parent) {
        return local_name; // Root component, path is just its name
    }
    const std::string& parent_path = parent->name(); // Get parent's path/name
    if (parent_path.empty()) {
        return local_name; // Parent has no path, use local name
    }
    return parent_path + "." + local_name; // Concatenate: parent_path.local_name
}

template <typename T, typename... Args>
class ch_module {
public:
    explicit ch_module(const std::string& instance_name, Args&&... args) {
        std::cout << "[ch_module::ctor] Creating module for component " << typeid(T).name() << std::endl;

        // 1. Get the currently active component (the parent)
        auto* parent_component = Component::current();
        if (!parent_component) {
             std::cerr << "[ch_module::ctor] Error: No active parent Component found when creating ch_module!" << std::endl;
             return; // Or throw an exception
        }

        // 2. Get the parent's context pointer
        auto* parent_context = parent_component->context();
        if (!parent_context) {
             std::cerr << "[ch_module::ctor] Error: Parent Component has no valid context!" << std::endl;
             return; // Or throw an exception
        }

        // 3. Construct the hierarchical path name
        std::string child_path_name = build_hierarchical_name(parent_component, instance_name);

        // 4. Create the child component instance
        std::unique_ptr<T> local_child_ptr = std::make_unique<T>(parent_component, child_path_name);

        // 5. Transfer ownership of the child component to the parent.
        // We will cast it to T* when accessing via io() or instance().
        child_component_ptr_ = dynamic_cast<T*>(parent_component->add_child(std::move(local_child_ptr)));
        if (!child_component_ptr_) {
             std::cerr << "[ch_module::ctor] Error: add_child returned null or failed dynamic_cast!" << std::endl;
        }

        // 6. Build the child component within its own temporary context (managed by T::build and ctx_swap)
        child_component_ptr_->build(parent_context);

        std::cout << "[ch_module::ctor] Finished creating module, child ptr is " << child_component_ptr_ << std::endl;
    }

    // Provide access to the child component's I/O and instance
    // These methods use the stored raw pointer (T*), which is valid as long as the parent owns the child.
    auto& io() {
        if (child_component_ptr_) {
            return child_component_ptr_->io();
       } else {
            // Handle error: child_component_ptr_ is null (should not happen if constructor succeeded)
            static typename T::io_type dummy_io; // This might not be possible depending on io_type definition
            std::cerr << "[ch_module::io] Error: Child component pointer is null!" << std::endl;
            return dummy_io; // Or return an optional/variant type, or throw
        }
    }

    T& instance() {
        if (child_component_ptr_) {
            return *child_component_ptr_;
        } else {
             std::cerr << "[ch_module::instance] Error: Child component pointer is null!" << std::endl;
             static T dummy_instance; // Not ideal
             return dummy_instance; // Or throw
        }
    }

private:
    T* child_component_ptr_ = nullptr; // Raw pointer, owned by parent component. Cast from Component* returned by add_child.
};

#define CH_MODULE(type, name, ...) ch::ch_module<type> name(#name, ##__VA_ARGS__)

} // namespace ch
#endif // CH_MODULE_H
