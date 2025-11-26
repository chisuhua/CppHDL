// include/utils/destruction_manager.h
#ifndef DESTRUCTION_MANAGER_H
#define DESTRUCTION_MANAGER_H

#include <unordered_set>
#include <unordered_map>
#include <mutex>
#include <iostream>
#include <atomic>

namespace ch {
    namespace core {
        class context;
    }
    class Simulator;
    class Component;
    
namespace detail {

// Destruction order manager
class destruction_manager {
public:
    static destruction_manager& instance() {
        static destruction_manager instance;
        return instance;
    }
    
    void register_context(core::context* ctx) {
        if (!in_static_destruction()) {
            // Use a simpler approach without mutex during normal operation
            contexts_.insert(ctx);
            std::cout << "Registered context: " << ctx << std::endl;
        }
    }
    
    void unregister_context(core::context* ctx) {
        if (!in_static_destruction()) {
            contexts_.erase(ctx);
            std::cout << "Unregistered context: " << ctx << std::endl;
        }
    }
    
    void register_component(Component* comp) {
        if (!in_static_destruction()) {
            components_.insert(comp);
            std::cout << "Registered component: " << comp << std::endl;
        }
    }
    
    void unregister_component(Component* comp) {
        if (!in_static_destruction()) {
            components_.erase(comp);
            std::cout << "Unregistered component: " << comp << std::endl;
        }
    }
    
    void register_simulator(Simulator* sim) {
        if (!in_static_destruction()) {
            simulators_.insert(sim);
            std::cout << "Registered simulator: " << sim << std::endl;
        }
    }
    
    void unregister_simulator(Simulator* sim) {
        if (!in_static_destruction()) {
            simulators_.erase(sim);
            std::cout << "Unregistered simulator: " << sim << std::endl;
        }
    }
    
    // 通知所有依赖特定context的simulator断开连接
    void notify_context_destruction(core::context* ctx) {
        if (!in_static_destruction()) {
            // 查找所有依赖该context的simulator并通知它们断开连接
            for (auto* sim : simulators_) {
                if (sim) {
                    // 注意：这里我们不能直接调用simulator的方法，因为可能存在不完整的类型问题
                    // 我们只能依靠simulator自己检查context有效性
                    std::cout << "Notifying simulator " << sim << " about context destruction" << std::endl;
                }
            }
        }
    }
    
    void pre_static_destruction() {
        std::cout << "Pre-static destruction cleanup called" << std::endl;
        // Clean up all simulators first
        for (auto* sim : simulators_) {
            if (sim) {
                std::cout << "Cleaning up simulator: " << sim << std::endl;
                // Mark simulator as disconnected to prevent operations during destruction
                // We can't directly call disconnect because of incomplete type issues
            }
        }
        simulators_.clear();
        
        // Then clean up components
        for (auto* comp : components_) {
            if (comp) {
                std::cout << "Cleaning up component: " << comp << std::endl;
            }
        }
        components_.clear();
        
        // Then clean up contexts
        for (auto* ctx : contexts_) {
            if (ctx) {
                std::cout << "Cleaning up context: " << ctx << std::endl;
                // Clear node references to prevent circular dependencies
            }
        }
        contexts_.clear();
        
        // Set the static destruction flag
        in_static_destruction_flag() = true;
        std::cout << "Static destruction flag set to true" << std::endl;
    }
    
    bool is_in_static_destruction() const {
        return const_cast<destruction_manager*>(this)->in_static_destruction_flag();
    }
    
private:
    destruction_manager() = default;
    ~destruction_manager() {
        // Ensure cleanup even if pre_static_destruction wasn't called
        std::cout << "Destruction manager destructor called" << std::endl;
        pre_static_destruction();
    }
    
    std::unordered_set<core::context*> contexts_;
    std::unordered_set<Simulator*> simulators_;
    std::unordered_set<Component*> components_;
    
    // Flag to indicate if we're in static destruction phase
    std::atomic<bool>& in_static_destruction_flag() {
        static std::atomic<bool> flag{false};
        return flag;
    }
};

} // namespace detail

// Function to call before program exits to ensure proper cleanup
inline void pre_static_destruction_cleanup() {
    std::cout << "Calling pre_static_destruction_cleanup" << std::endl;
    detail::destruction_manager::instance().pre_static_destruction();
}

// Check if we're in static destruction phase
inline bool in_static_destruction() {
    bool result = detail::destruction_manager::instance().is_in_static_destruction();
    if (result) {
        std::cout << "in_static_destruction returned true" << std::endl;
    }
    return result;
}

} // namespace ch

#endif // DESTRUCTION_MANAGER_H