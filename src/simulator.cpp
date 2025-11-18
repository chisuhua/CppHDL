// src/simulator.cpp
#include "simulator.h"
#include "core/lnodeimpl.h"
#include "ast/ast_nodes.h"
#include "logger.h"
#include "utils/destruction_manager.h"
#include <cassert>
#include <iostream>

namespace ch {

Simulator::Simulator(ch::core::context* ctx)
    : ctx_(ctx) {
    CHDBG_FUNC();
    CHREQUIRE(ctx_ != nullptr, "Context cannot be null");
    
    // Register with destruction manager
    std::cout << "Registering simulator " << this << std::endl;
    std::cout.flush();
    ch::detail::destruction_manager::instance().register_simulator(this);
    
    // 创建默认时钟
    set_default_clock();
    
    initialize();
}

Simulator::~Simulator() {
    std::cout << "Unregistering simulator " << this << std::endl;
    std::cout.flush();
    // Unregister from destruction manager
    ch::detail::destruction_manager::instance().unregister_simulator(this);
    
    // Check if we're in static destruction phase
    if (ch::detail::in_static_destruction()) {
        std::cout << "Simulator destructor called during static destruction" << std::endl;
        std::cout.flush();
        // During static destruction, minimize operations to prevent segfaults
        // Just return immediately without doing any cleanup
        return;
    }
    
    std::cout << "Simulator destructor normal cleanup" << std::endl;
    std::cout.flush();
    
    // Explicitly disconnect to prevent accessing destroyed context
    disconnect();
}

void Simulator::disconnect() {
    CHDBG_FUNC();
    
    // Check if we're in static destruction phase
    if (ch::detail::in_static_destruction()) {
        // During static destruction, minimize operations to prevent segfaults
        // Just mark as disconnected and return immediately
        disconnected_ = true;
        return;
    }
    
    if (disconnected_) {
        return;
    }
    
    // Mark as disconnected first to prevent any further operations
    disconnected_ = true;
    
    // Clear all data that references the context
    instr_map_.clear();
    instr_cache_.clear();
    data_map_.clear();
    eval_list_.clear();
    
    // Safely reset the default clock
    if (default_clock_) {
        default_clock_.reset();
    }
    
    // Clear the context pointer
    ctx_ = nullptr;
    
    // Mark as uninitialized
    initialized_ = false;
}

void Simulator::set_default_clock() {
    if (!ctx_ || disconnected_) return;
    
    // 创建默认时钟
    default_clock_ = std::unique_ptr<ch::core::clockimpl>(
        ctx_->create_clock(ch::core::sdata_type(0, 1), true, false, "default_clock")
    );
    
    // 设置为上下文的默认时钟
    if (!disconnected_ && ctx_ && default_clock_) {
        ctx_->set_default_clock(default_clock_.get());
    }
}

void Simulator::initialize() {
    CHDBG_FUNC();
    
    if (initialized_ || disconnected_ || !ctx_) {
        update_instruction_pointers();
        return;
    }
    
    eval_list_ = ctx_->get_eval_list();
    CHDBG("Retrieved eval_list with %zu nodes", eval_list_.size());
    
    instr_map_.clear();
    instr_cache_.clear();
    data_map_.clear();

    // 分配数据缓冲区
    for (auto* node : eval_list_) {
        if (disconnected_) return;
        
        CHCHECK_NULL(node, "Null node found in evaluation list, skipping");
        
        if (!node) continue;
        
        uint32_t node_id = node->id();
        uint32_t size = node->size();
        CHDBG("Processing node ID %u with size %u", node_id, size);
        
        if (node->type() == ch::core::lnodetype::type_lit) {
            auto* lit_node = static_cast<ch::core::litimpl*>(node);
            data_map_[node_id] = lit_node->value();
            CHDBG("Set literal value for node %u", node_id);
        } else {
            data_map_[node_id] = ch::core::sdata_type(0, size);
            CHDBG("Allocated buffer for node %u", node_id);
        }
    }

    // 创建指令缓存
    for (auto* node : eval_list_) {
        if (!node || disconnected_) continue;
        
        uint32_t node_id = node->id();
        auto instr = node->create_instruction(data_map_);
        if (instr) {
            instr_cache_[node_id] = std::move(instr);
            instr_map_[node_id] = instr_cache_[node_id].get();
            CHDBG("Created instruction for node %u", node_id);
        } else {
            CHDBG("No instruction created for node %u", node_id);
        }
    }
    
    initialized_ = true;
    CHINFO("Simulator initialization completed successfully");
}

void Simulator::update_instruction_pointers() {
    CHDBG_FUNC();
    
    if (disconnected_) return;
    
    instr_map_.clear();
    for (const auto& pair : instr_cache_) {
        if (disconnected_) return;
        instr_map_[pair.first] = pair.second.get();
    }
    CHDBG("Updated %zu instruction pointers", instr_map_.size());
}

void Simulator::eval() {
    CHDBG_FUNC();
    
    if (!initialized_ || disconnected_ || !ctx_) {
        CHERROR("Simulator not initialized or disconnected");
        return;
    }
    
    if (eval_list_.empty()) {
        CHDBG("No nodes to evaluate");
        return;
    }
    
    // Create a complete snapshot of the data map at the beginning of evaluation
    // This ensures all instructions read from values at the start of the cycle
    auto eval_start_data_map = data_map_;
    
    CHDBG("Starting evaluation loop with %zu nodes", eval_list_.size());
    for (auto* node : eval_list_) {
        if (!node || disconnected_) continue;
        
        uint32_t node_id = node->id();
        auto it = instr_map_.find(node_id);
        if (it != instr_map_.end() && it->second) {
            CHDBG("Executing instruction for node %u", node_id);
            // Pass the snapshot for reading
            it->second->eval(eval_start_data_map, data_map_);
        } else {
            CHDBG("No instruction for node ID: %u", node_id);
        }
    }
    
    // Update register proxy nodes to match their corresponding register nodes
    // Based on the observed pattern, each register node is immediately followed by its proxy node
    for (size_t i = 0; i < eval_list_.size(); ++i) {
        auto* node = eval_list_[i];
        if (!node || disconnected_) continue;
        
        // If this is a register node and there's a next node that is a proxy
        if (node->type() == ch::core::lnodetype::type_reg && i + 1 < eval_list_.size()) {
            auto* next_node = eval_list_[i + 1];
            if (next_node && next_node->type() == ch::core::lnodetype::type_proxy) {
                uint32_t reg_node_id = node->id();
                uint32_t proxy_node_id = next_node->id();
                
                // Copy the register value to the proxy
                auto reg_it = data_map_.find(reg_node_id);
                auto proxy_it = data_map_.find(proxy_node_id);
                
                if (reg_it != data_map_.end() && proxy_it != data_map_.end()) {
                    proxy_it->second = reg_it->second;
                    CHDBG("Updated proxy node %u to match register node %u", proxy_node_id, reg_node_id);
                }
            }
        }
    }
    
    CHDBG("Evaluation loop completed");
}

void Simulator::eval_range(size_t start, size_t end) {
    CHDBG_FUNC();
    
    if (!initialized_ || disconnected_ || !ctx_) {
        CHERROR("Simulator not initialized or disconnected");
        return;
    }
    
    if (start >= eval_list_.size() || end > eval_list_.size() || start >= end) {
        CHERROR("Invalid evaluation range: start=%zu, end=%zu, list_size=%zu", start, end, eval_list_.size());
        return;
    }
    
    // Create a complete snapshot of the data map at the beginning of evaluation
    auto eval_start_data_map = data_map_;
    
    for (size_t i = start; i < end; ++i) {
        auto* node = eval_list_[i];
        if (!node || disconnected_) continue;
        
        uint32_t node_id = node->id();
        auto it = instr_map_.find(node_id);
        if (it != instr_map_.end() && it->second) {
            it->second->eval(eval_start_data_map, data_map_);
        }
    }
}

void Simulator::tick() {
    CHDBG_FUNC();
    
    // Toggle the default clock if it exists
    if (default_clock_ && ctx_ && !disconnected_) {
        auto it = data_map_.find(default_clock_->id());
        if (it != data_map_.end()) {
            // Toggle the clock value (0 -> 1, 1 -> 0)
            it->second = ch::core::sdata_type(
                it->second.is_zero() ? 1 : 0, 
                it->second.bitwidth()
            );
            CHDBG("Toggled default clock to %llu", 
                  static_cast<unsigned long long>(static_cast<uint64_t>(it->second)));
        }
    }
    
    eval();
}

void Simulator::tick(size_t count) {
    CHDBG_FUNC();
    CHDBG_VAR(count);
    
    if (count == 0) {
        return;
    }
    
    for (size_t i = 0; i < count; ++i) {
        if (disconnected_) return;
        CHDBG("Tick %zu/%zu", i + 1, count);
        tick();
    }
}

void Simulator::reset() {
    CHDBG_FUNC();
    
    if (!initialized_ || disconnected_ || !ctx_) {
        CHERROR("Simulator not initialized or disconnected");
        return;
    }
    
    for (auto& pair : data_map_) {
        if (disconnected_) return;
        uint32_t node_id = pair.first;
        auto* node = ctx_->get_node_by_id(node_id);
        if (node && node->type() != ch::core::lnodetype::type_lit) {
            pair.second = ch::core::sdata_type(0, pair.second.bitwidth());
        }
    }
    CHDBG("Simulator state reset completed");
}

const ch::core::sdata_type& Simulator::get_value_by_name(const std::string& name) const {
    CHDBG_FUNC();
    CHDBG("Looking for node with name: %s", name.c_str());

    if (!initialized_ || disconnected_ || !ctx_) {
        CHERROR("Simulator not initialized or disconnected");
        // Return a safe empty value instead of crashing
        static ch::core::sdata_type empty_value(0, 1);
        return empty_value;
    }
    
    if (name.empty()) {
        CHERROR("Node name cannot be empty");
        static ch::core::sdata_type empty_value(0, 1);
        return empty_value;
    }
    
    // 查找节点
    for (const auto& node_ptr : ctx_->get_nodes()) {
        if (!node_ptr || disconnected_) continue;
        if (node_ptr && node_ptr->name() == name) {
            uint32_t node_id = node_ptr->id();
            auto it = data_map_.find(node_id);
            if (it != data_map_.end()) {
                return it->second;
            }
            CHWARN("Data not found for node '%s' with ID %u", name.c_str(), node_id);
            static ch::core::sdata_type empty_value(0, 1);
            return empty_value;
        }
    }
    
    CHWARN("Node with name '%s' not found", name.c_str());
    static ch::core::sdata_type empty_value(0, 1);
    return empty_value;
}

} // namespace ch