// include/core/context.h
#ifndef CH_CORE_CONTEXT_H
#define CH_CORE_CONTEXT_H

#include <vector>
#include <unordered_map>
#include <memory>
#include <string>
#include <cstdint>
#include <iostream>
#include <source_location>

#include "types.h"

namespace ch { namespace core {

// Forward declarations
class lnodeimpl;
class litimpl;
class inputimpl;
class outputimpl;
class regimpl;
class opimpl;
class proxyimpl;
class clockimpl;
class resetimpl;
class context;

extern context* ctx_curr_;

class ctx_swap {
public:
    explicit ctx_swap(context* new_ctx);
    ~ctx_swap();
    ctx_swap(const ctx_swap&) = delete;
    ctx_swap& operator=(const ctx_swap&) = delete;
private:
    context* old_ctx_;
};

extern bool debug_context_lifetime;

class context {
public:
    explicit context(const std::string& name = "unnamed", context* parent = nullptr);
    ~context();

    template <typename T, typename... Args>
    T* create_node(Args&&... args) {
        static_assert(std::is_base_of_v<lnodeimpl, T>, "T must derive from lnodeimpl");
        uint32_t new_id = next_node_id_++;
        auto node = std::make_unique<T>(new_id, std::forward<Args>(args)..., this);
        T* raw_ptr = node.get();
        nodes_.push_back(std::move(node));
        node_map_[raw_ptr->id()] = raw_ptr;
        if (debug_context_lifetime) {
            std::cout << "[context::create_node] Created node ID " << new_id 
                      << " (" << raw_ptr->name() << ") of type " << static_cast<int>(raw_ptr->type()) 
                      << " in context " << this << std::endl;
        }
        return raw_ptr;
    }

    uint32_t node_id() { return next_node_id_++; }

    litimpl* create_literal(const sdata_type& value, const std::string& name = "literal", const std::source_location& sloc = std::source_location::current());
    inputimpl* create_input(uint32_t size, const std::string& name, const std::source_location& sloc = std::source_location::current());
    outputimpl* create_output(uint32_t size, const std::string& name, const std::source_location& sloc = std::source_location::current());

    const std::vector<std::unique_ptr<lnodeimpl>>& get_nodes() const { return nodes_; }
    lnodeimpl* get_node_by_id(uint32_t id) const {
        auto it = node_map_.find(id);
        return (it != node_map_.end()) ? it->second : nullptr;
    }

    std::vector<lnodeimpl*> get_eval_list() const;

    void print_debug_info() const {
        std::cout << "[context::print_debug_info] Context " << this << ", name: " << name_ 
                  << ", nodes: " << nodes_.size() << std::endl;
    }

    clockimpl* current_clock(const std::source_location& sloc);
    resetimpl* current_reset(const std::source_location& sloc);
    void set_current_clock(clockimpl* clk);
    void set_current_reset(resetimpl* rst);

private:
    void topological_sort_visit(lnodeimpl* node, std::vector<lnodeimpl*>& sorted,
                                std::unordered_map<lnodeimpl*, bool>& visited,
                                std::unordered_map<lnodeimpl*, bool>& temp_mark) const;
    void init();

    std::vector<std::unique_ptr<lnodeimpl>> nodes_;
    std::unordered_map<uint32_t, lnodeimpl*> node_map_;
    uint32_t next_node_id_ = 0;
    clockimpl* current_clock_ = nullptr;
    resetimpl* current_reset_ = nullptr;
    std::string name_;
    context* parent_ = nullptr;
};

}} // namespace ch::core

#endif // CH_CORE_CONTEXT_H
