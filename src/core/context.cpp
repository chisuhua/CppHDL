// src/core/context.cpp
#include "core/context.h"
#include "ast_nodes.h" // Include specific node implementations HERE
#include "lnodeimpl.h" // For lnodeimpl base and lnodetype
#include <iostream> // For debugging prints if needed
#include <stack>    // For topological sort
#include <unordered_set> // For topological sort

namespace ch { namespace core {

    context* ctx_curr_ = nullptr;
    bool debug_context_lifetime = true; // Set to false to disable prints

    ctx_swap::ctx_swap(context* new_ctx) : old_ctx_(ctx_curr_) {
        ctx_curr_ = new_ctx;
        if (debug_context_lifetime) { // --- NEW: Debug print ---
            std::cout << "[ctx_swap::ctor] Swapped context from " << old_ctx_ << " to " << new_ctx << std::endl;
        }
    }

    ctx_swap::~ctx_swap() {
        ctx_curr_ = old_ctx_;
        if (debug_context_lifetime) { // --- NEW: Debug print ---
            std::cout << "[ctx_swap::dtor] Restored context to " << old_ctx_ << std::endl;
        }
    }

    context::context(const std::string& name, context* parent)
        : name_(name), parent_(parent) {
        if (debug_context_lifetime) { // --- NEW: Debug print ---
            std::cout << "[context::ctor] Created new context " << this << ", name: " << name_ << std::endl;
        }
        init();
    }

    context::~context() {
        if (debug_context_lifetime) { // --- NEW: Debug print ---
            std::cout << "[context::dtor] Destroying context " << this << ", name: " << name_ << " and its " << nodes_.size() << " nodes." << std::endl;
        }
        node_map_.clear();
        if (ctx_curr_ == this) {
            if (debug_context_lifetime) { // --- NEW: Debug print ---
                std::cout << "[context::dtor] Warning: Context " << this << " being destroyed was still the active ctx_curr_!" << std::endl;
            }
            ctx_curr_ = nullptr; // Avoid dangling pointer in global variable
        }
    }

    void context::init() {
        nodes_.reserve(100); // Example reservation
    }

    // --- Factory method for literal nodes ---
    litimpl* context::create_literal(const sdata_type& value, const std::string& name, const std::source_location& sloc) {
         // Create the literal node using the context's generic factory
         // Pass 'this' as the context pointer to the litimpl constructor
         auto* lit_node = this->create_node<litimpl>(value, name, sloc); // Pass 'this' via create_node's internal 'this'
         std::cout << "  [context::create_literal] Created litimpl node '" << name << "'" << std::endl;
         return lit_node;
    }

    inputimpl* context::create_input(uint32_t size, const std::string& name, const std::source_location& sloc) {
        sdata_type init_val(0, size); // 默认初始值 0
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
    // --- Topological Sort Implementation ---
    std::vector<lnodeimpl*> context::get_eval_list() const {
        std::vector<lnodeimpl*> sorted;
        sorted.reserve(nodes_.size());

        std::unordered_map<lnodeimpl*, bool> visited; // Permanent mark
        std::unordered_map<lnodeimpl*, bool> temp_mark; // Temporary mark (for cycle detection)

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
            // Cycle detected! This should ideally not happen in a valid hardware description.
            std::cerr << "[context] Error: Cycle detected in IR graph during topological sort!" << std::endl;
            // Handle error appropriately (throw, return error code, etc.)
            return;
        }

        if (visited[node]) {
            return; // Already processed
        }

        temp_mark[node] = true;

        // Visit all source nodes (dependencies)
        for (uint32_t i = 0; i < node->num_srcs(); ++i) { // Use num_srcs() and src(uint32_t)
            lnodeimpl* src_node = node->src(i);
            if (src_node) { // Check for null in case of unconnected inputs
                topological_sort_visit(src_node, sorted, visited, temp_mark);
            }
        }

        temp_mark[node] = false;
        visited[node] = true;
        sorted.push_back(node);
    }

    // --- Clock/Reset Management ---
    clockimpl* context::current_clock(const std::source_location& sloc) {
        if (!current_clock_) {
            // Could create a default clock here or require explicit setup
            // For now, let's assume one must be set or passed explicitly to ch_reg etc.
            std::cerr << "[context] Warning: No current clock set at " << sloc.file_name() << ":" << sloc.line() << std::endl;
        }
        return current_clock_;
    }

    resetimpl* context::current_reset(const std::source_location& sloc) {
        if (!current_reset_) {
             // Could create a default reset here or require explicit setup
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
