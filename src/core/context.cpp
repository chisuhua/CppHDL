// src/core/context.cpp
#include "core/context.h"
#include "ast_nodes.h"
#include "lnodeimpl.h"
#include "logger.h"
#include <iostream>
#include <stack>
#include <unordered_set>

namespace ch { namespace core {

thread_local context* ctx_curr_ = nullptr;
bool debug_context_lifetime = true;

ctx_swap::ctx_swap(context* new_ctx) : old_ctx_(ctx_curr_) {
    CHDBG_FUNC();
    CHREQUIRE(new_ctx != nullptr, "Cannot swap to null context");
    
    ctx_curr_ = new_ctx;
    if (debug_context_lifetime) {
        CHINFO("Swapped context from 0x%llx to 0x%llx", 
               (unsigned long long)old_ctx_, (unsigned long long)new_ctx);
    }
}

ctx_swap::~ctx_swap() {
    CHDBG_FUNC();
    ctx_curr_ = old_ctx_;
    if (debug_context_lifetime) {
        CHINFO("Restored context to 0x%llx", (unsigned long long)old_ctx_);
    }
}

context::context(const std::string& name, context* parent)
    : name_(name.empty() ? "unnamed" : name), parent_(parent) {
    CHDBG_FUNC();
    
    if (debug_context_lifetime) {
        CHINFO("Created new context 0x%llx, name: %s", 
               (unsigned long long)this, name_.c_str());
    }
    
    init();
}

context::~context() {
    CHDBG_FUNC();
    
    if (debug_context_lifetime) {
        CHINFO("Destroying context 0x%llx, name: %s and its %zu nodes.", 
               (unsigned long long)this, name_.c_str(), node_storage_.size());
    }
    
    try {
        CHDBG("Cleaning up %zu nodes", node_storage_.size());
        if (ctx_curr_ == this) {
            if (debug_context_lifetime) {
                CHWARN("Context 0x%llx being destroyed was still the active ctx_curr_!", 
                       (unsigned long long)this);
            }
            ctx_curr_ = nullptr;
        }
        
        node_storage_.clear();
    } catch (const std::exception& e) {
        CHERROR("Error during context cleanup: %s", e.what());
        // 不抛出异常，因为在析构函数中
    }
}

void context::init() {
    CHDBG_FUNC();
    try {
        node_storage_.reserve(100);
        CHDBG("Reserved capacity for context containers");
    } catch (const std::bad_alloc&) {
        CHERROR("Failed to reserve memory for context containers");
    }
}

uint32_t context::next_node_id() {
    CHDBG_FUNC();
    CHENSURE(next_node_id_ < MAX_NODE_ID, "Node ID overflow approaching");
    return next_node_id_++;
}

std::vector<lnodeimpl*> context::get_eval_list() const {
    CHDBG_FUNC();
    std::vector<lnodeimpl*> sorted;
    
    try {
        sorted.reserve(node_storage_.size());
        CHDBG("Reserved space for evaluation list with %zu nodes", node_storage_.size());
    } catch (const std::bad_alloc&) {
        CHERROR("Failed to reserve memory for evaluation list");
        return sorted; // 返回空列表
    }

    std::unordered_map<lnodeimpl*, bool> visited;
    std::unordered_map<lnodeimpl*, bool> temp_mark;

    for (const auto& node_ptr : node_storage_) {
        CHCHECK_NULL(node_ptr.get(), "Null node pointer found in context");
        
        if (!node_ptr) continue;
        
        lnodeimpl* node = node_ptr.get();
        if (visited.find(node) == visited.end()) {
            topological_sort_visit(node, sorted, visited, temp_mark);
        }
    }

    CHDBG("Topological sort completed with %zu nodes", sorted.size());
    return sorted;
}

void context::set_as_current_context() {
    CHDBG_FUNC();
    ctx_curr_ = this;
}

lnodeimpl* context::get_node_by_id(uint32_t id) const {
    CHDBG_FUNC();
    CHDBG("Looking up node ID %u", id);
    
    //get_node_by_id() 主要在仿真初始化时调用 运行时很少使用
    // 使用线性查找替代 map 查找, O(n)影响比较小
    for (const auto& node_ptr : node_storage_) {
        if (node_ptr && node_ptr->id() == id) {
            return node_ptr.get();
        }
    }
    
    CHDBG("Node ID %u not found", id);
    return nullptr;
}

litimpl* context::create_literal(const sdata_type& value, 
                                const std::string& name, 
                                const std::source_location& sloc) {
    CHDBG_FUNC();
    CHREQUIRE(!name.empty(), "Literal name cannot be empty");
    
    auto* lit_node = this->create_node<litimpl>(value, name, sloc);
    if (debug_context_lifetime && lit_node) {
        CHINFO("Created litimpl node '%s'", name.c_str());
    }
    return lit_node;
}

inputimpl* context::create_input(uint32_t size, 
                                const std::string& name, 
                                const std::source_location& sloc) {
    CHDBG_FUNC();
    CHDBG("Creating input: %s with size %u", name.c_str(), size);
    
    CHREQUIRE(!name.empty(), "Input name cannot be empty");
    CHREQUIRE(size > 0, "Input size must be greater than 0");
    
    sdata_type init_val(0, size);
    auto* input_node = this->create_node<inputimpl>(size, init_val, name, sloc);
    if (input_node) {
        auto* proxy = this->create_node<proxyimpl>(input_node, "_" + name, sloc);
        CHDBG("Created input node and proxy for: %s", name.c_str());
    } else {
        CHERROR("Failed to create input node for: %s", name.c_str());
    }
    return input_node;
}

outputimpl* context::create_output(uint32_t size, 
                                  const std::string& name, 
                                  const std::source_location& sloc) {
    CHDBG_FUNC();
    CHDBG("Creating output: %s with size %u", name.c_str(), size);
    
    CHREQUIRE(!name.empty(), "Output name cannot be empty");
    CHREQUIRE(size > 0, "Output size must be greater than 0");
    
    sdata_type init_val(0, size);
    auto* proxy = this->create_node<proxyimpl>(size, "_" + name, sloc);
    auto* output_node = this->create_node<outputimpl>(size, proxy, init_val, name, sloc);
    
    if (output_node) {
        CHDBG("Created output node: %s", name.c_str());
    }
    return output_node;
}

void context::topological_sort_visit(lnodeimpl* node, std::vector<lnodeimpl*>& sorted,
                                     std::unordered_map<lnodeimpl*, bool>& visited,
                                     std::unordered_map<lnodeimpl*, bool>& temp_mark) const {
    CHDBG_FUNC();
    CHREQUIRE(node != nullptr, "Null node pointer in topological sort");
    
    if (temp_mark[node]) {
        // 循环检测 - 记录错误但不终止程序
        CHFATAL_RECOVERABLE("Cycle detected in IR graph during topological sort");
        return; // 继续执行而不是终止程序
    }

    if (visited[node]) {
        return;
    }

    temp_mark[node] = true;

    for (uint32_t i = 0; i < node->num_srcs(); ++i) {
        lnodeimpl* src_node = node->src(i);
        if (src_node) {
            topological_sort_visit(src_node, sorted, visited, temp_mark);
        }
    }

    temp_mark[node] = false;
    visited[node] = true;
    sorted.push_back(node);
}

clockimpl* context::current_clock(const std::source_location& sloc) {
    if (!current_clock_) {
        CHWARN("No current clock set at %s:%d", sloc.file_name(), sloc.line());
    }
    return current_clock_;
}

resetimpl* context::current_reset(const std::source_location& sloc) {
    if (!current_reset_) {
        CHWARN("No current reset set at %s:%d", sloc.file_name(), sloc.line());
    }
    return current_reset_;
}

void context::set_current_clock(clockimpl* clk) {
    CHDBG_FUNC();
    current_clock_ = clk;
}

void context::set_current_reset(resetimpl* rst) {
    CHDBG_FUNC();
    current_reset_ = rst;
}

void context::print_debug_info() const {
    CHDBG_FUNC();
    try {
        CHINFO("Context 0x%llx, name: %s, nodes: %zu", 
               (unsigned long long)this, name_.c_str(), node_storage_.size());
    } catch (const std::exception& e) {
        CHERROR("Error in print_debug_info: %s", e.what());
    }
}

}} // namespace ch::core
