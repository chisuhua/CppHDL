// src/core/context.cpp
#include "core/context.h"
#include "ast_nodes.h"
#include "lnodeimpl.h"
#include <iostream>
#include <stack>
#include <unordered_set>

namespace ch { namespace core {

context* ctx_curr_ = nullptr;
bool debug_context_lifetime = true;

ctx_swap::ctx_swap(context* new_ctx) : old_ctx_(ctx_curr_) {
    ctx_curr_ = new_ctx;
    if (debug_context_lifetime) {
        std::cout << "[ctx_swap::ctor] Swapped context from " << old_ctx_ << " to " << new_ctx << std::endl;
    }
}

ctx_swap::~ctx_swap() {
    ctx_curr_ = old_ctx_;
    if (debug_context_lifetime) {
        std::cout << "[ctx_swap::dtor] Restored context to " << old_ctx_ << std::endl;
    }
}

context::context(const std::string& name, context* parent)
    : name_(name), parent_(parent) {
    if (debug_context_lifetime) {
        std::cout << "[context::ctor] Created new context " << this << ", name: " << name_ << std::endl;
    }
    init();
}

context::~context() {
    if (debug_context_lifetime) {
        std::cout << "[context::dtor] Destroying context " << this << ", name: " << name_ 
                  << " and its " << nodes_.size() << " nodes." << std::endl;
    }
    node_map_.clear();
    if (ctx_curr_ == this) {
        if (debug_context_lifetime) {
            std::cout << "[context::dtor] Warning: Context " << this << " being destroyed was still the active ctx_curr_!" << std::endl;
        }
        ctx_curr_ = nullptr;
    }
}

void context::init() {
    nodes_.reserve(100);
}

litimpl* context::create_literal(const sdata_type& value, const std::string& name, const std::source_location& sloc) {
    auto* lit_node = this->create_node<litimpl>(value, name, sloc);
    if (debug_context_lifetime) {
        std::cout << "  [context::create_literal] Created litimpl node '" << name << "'" << std::endl;
    }
    return lit_node;
}

inputimpl* context::create_input(uint32_t size, const std::string& name, const std::source_location& sloc) {
    sdata_type init_val(0, size);
    auto* input_node = this->create_node<inputimpl>(size, init_val, name, sloc);
    // Create proxy for expression use
    auto* proxy = this->create_node<proxyimpl>(input_node, '_' + name, sloc);
    return input_node;
}

outputimpl* context::create_output(uint32_t size, const std::string& name, const std::source_location& sloc) {
    sdata_type init_val(0, size);
    auto* proxy = this->create_node<proxyimpl>(size, '_' + name, sloc);
    auto* output_node = this->create_node<outputimpl>(size, proxy, init_val, name, sloc);
    return output_node;
}

std::vector<lnodeimpl*> context::get_eval_list() const {
    std::vector<lnodeimpl*> sorted;
    sorted.reserve(nodes_.size());

    std::unordered_map<lnodeimpl*, bool> visited;
    std::unordered_map<lnodeimpl*, bool> temp_mark;

    for (const auto& node_ptr : nodes_) {
        lnodeimpl* node = node_ptr.get();
        if (visited.find(node) == visited.end()) {
            topological_sort_visit(node, sorted, visited, temp_mark);
        }
    }

    return sorted;
}

void context::topological_sort_visit(lnodeimpl* node, std::vector<lnodeimpl*>& sorted,
                                     std::unordered_map<lnodeimpl*, bool>& visited,
                                     std::unordered_map<lnodeimpl*, bool>& temp_mark) const {
    if (temp_mark[node]) {
        std::cerr << "[context] Error: Cycle detected in IR graph during topological sort!" << std::endl;
        return;
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
        std::cerr << "[context] Warning: No current clock set at " << sloc.file_name() << ":" << sloc.line() << std::endl;
    }
    return current_clock_;
}

resetimpl* context::current_reset(const std::source_location& sloc) {
    if (!current_reset_) {
        std::cerr << "[context] Warning: No current reset set at " << sloc.file_name() << ":" << sloc.line() << std::endl;
    }
    return current_reset_;
}

void context::set_current_clock(clockimpl* clk) {
    current_clock_ = clk;
}

void context::set_current_reset(resetimpl* rst) {
    current_reset_ = rst;
}

}} // namespace ch::core
