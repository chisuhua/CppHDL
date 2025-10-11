// src/simulator.cpp
#include "simulator.h"
#include "core/lnodeimpl.h"
#include "ast/ast_nodes.h"
#include "logger.h"
#include <cassert>

namespace ch {

Simulator::Simulator(ch::core::context* ctx)
    : ctx_(ctx) {
    CHDBG_FUNC();
    CHREQUIRE(ctx_ != nullptr, "Context cannot be null");
    
    initialize();
}

Simulator::~Simulator() {
    CHDBG_FUNC();
    // unique_ptr 会自动清理 instr_cache_ 中的指令
}

void Simulator::initialize() {
    CHDBG_FUNC();
    
    if (initialized_) {
        update_instruction_pointers();
        return;
    }
    
    CHREQUIRE(ctx_ != nullptr, "Context is null during initialization");
    
    eval_list_ = ctx_->get_eval_list();
    CHDBG("Retrieved eval_list with %zu nodes", eval_list_.size());
    
    instr_map_.clear();
    instr_cache_.clear();
    data_map_.clear();

    // 分配数据缓冲区
    for (auto* node : eval_list_) {
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
        if (!node) continue;
        
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
    
    instr_map_.clear();
    for (const auto& pair : instr_cache_) {
        instr_map_[pair.first] = pair.second.get();
    }
    CHDBG("Updated %zu instruction pointers", instr_map_.size());
}

void Simulator::eval() {
    CHDBG_FUNC();
    
    if (!initialized_) {
        CHERROR("Simulator not initialized");
        return;
    }
    
    if (eval_list_.empty()) {
        CHDBG("No nodes to evaluate");
        return;
    }
    
    CHDBG("Starting evaluation loop with %zu nodes", eval_list_.size());
    for (auto* node : eval_list_) {
        if (!node) continue;
        
        uint32_t node_id = node->id();
        auto it = instr_map_.find(node_id);
        if (it != instr_map_.end() && it->second) {
            CHDBG("Executing instruction for node %u", node_id);
            it->second->eval(data_map_);
        } else {
            CHDBG("No instruction for node ID: %u", node_id);
        }
    }
    CHDBG("Evaluation loop completed");
}

void Simulator::eval_range(size_t start, size_t end) {
    CHDBG_FUNC();
    
    if (!initialized_) {
        CHERROR("Simulator not initialized");
        return;
    }
    
    if (start >= eval_list_.size() || end > eval_list_.size() || start >= end) {
        CHERROR("Invalid evaluation range: start=%zu, end=%zu, list_size=%zu", start, end, eval_list_.size());
        return;
    }
    
    for (size_t i = start; i < end; ++i) {
        auto* node = eval_list_[i];
        if (!node) continue;
        
        uint32_t node_id = node->id();
        auto it = instr_map_.find(node_id);
        if (it != instr_map_.end() && it->second) {
            it->second->eval(data_map_);
        }
    }
}

void Simulator::tick() {
    CHDBG_FUNC();
    eval();
}

void Simulator::tick(size_t count) {
    CHDBG_FUNC();
    CHDBG_VAR(count);
    
    if (count == 0) {
        return;
    }
    
    for (size_t i = 0; i < count; ++i) {
        CHDBG("Tick %zu/%zu", i + 1, count);
        tick();
    }
}

void Simulator::reset() {
    CHDBG_FUNC();
    
    if (!initialized_) {
        CHERROR("Simulator not initialized");
        return;
    }
    
    for (auto& pair : data_map_) {
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

    if (!initialized_) {
        CHERROR("Simulator not initialized");
        return ch::core::constants::empty;
    }
    
    if (!ctx_) {
        CHERROR("Context is null");
        return ch::core::constants::empty;
    }
    
    if (name.empty()) {
        CHERROR("Node name cannot be empty");
        return ch::core::constants::empty;
    }
    
    // 查找节点
    for (const auto& node_ptr : ctx_->get_nodes()) {
        if (node_ptr && node_ptr->name() == name) {
            uint32_t node_id = node_ptr->id();
            auto it = data_map_.find(node_id);
            if (it != data_map_.end()) {
                return it->second;
            }
            CHWARN("Data not found for node '%s' with ID %u", name.c_str(), node_id);
            return ch::core::constants::empty;
        }
    }
    
    CHWARN("Node with name '%s' not found", name.c_str());
    return ch::core::constants::empty;
}

} // namespace ch
