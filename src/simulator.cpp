// src/simulator.cpp
#include "simulator.h"
#include "core/lnodeimpl.h"
#include "ast/ast_nodes.h"
#include <cassert>
#include <iostream>

namespace ch {

Simulator::Simulator(ch::core::context* ctx)
    : ctx_(ctx) {
    
    std::cout << "[Simulator::ctor] Received context " << ctx_ << std::endl;
    if (!ctx_) {
        throw std::invalid_argument("Context cannot be null");
    }
    initialize();
}

Simulator::~Simulator() {
    std::cout << "[Simulator::dtor] Destroying simulator for context " << ctx_ << std::endl;
    // unique_ptr 会自动清理 instr_cache_ 中的指令
}

void Simulator::initialize() {
    if (initialized_) {
        // 如果已经初始化，只需更新缓冲区指针
        update_instruction_pointers();
        return;
    }
    
    std::cout << "[Simulator::initialize] Starting initialization for context " << ctx_ << std::endl;
    
    // 获取评估列表
    eval_list_ = ctx_->get_eval_list();
    std::cout << "[Simulator::initialize] Retrieved eval_list with " << eval_list_.size() << " nodes." << std::endl;
    
    // 清理之前的状态
    instr_map_.clear();
    instr_cache_.clear();
    data_map_.clear();

    // 1. 分配数据缓冲区
    std::cout << "[Simulator::initialize] Phase 1: Allocating data buffers." << std::endl;
    for (auto* node : eval_list_) {
        if (!node) continue;
        
        std::cout << "[Simulator::initialize] Processing node ID " << node->id() << " (ptr " << node << ")" << std::endl;
        uint32_t node_id = node->id();
        uint32_t size = node->size();
        
        if (node->type() == ch::core::lnodetype::type_lit) {
            auto* lit_node = static_cast<ch::core::litimpl*>(node);
            std::cout << "[Simulator::initialize] Node " << node_id << " is literal, copying value." << std::endl;
            data_map_[node_id] = lit_node->value();
        } else {
            data_map_[node_id] = ch::core::sdata_type(0, size);
        }
        std::cout << "[Simulator::initialize] Allocated buffer for node " << node_id << std::endl;
    }
    std::cout << "[Simulator::initialize] Finished data_map_ initialization loop." << std::endl;

    // 2. 创建指令缓存（一次性创建，后续重用）
    std::cout << "[Simulator::initialize] Phase 2: Creating instruction cache." << std::endl;
    for (auto* node : eval_list_) {
        if (!node) continue;
        
        std::cout << "[Simulator::initialize] Creating instruction for node ID " << node->id() << " (ptr " << node << ")" << std::endl;
        uint32_t node_id = node->id();
        
        // 让节点自己创建对应的指令
        auto instr = node->create_instruction(data_map_);
        if (instr) {
            instr_cache_[node_id] = std::move(instr);
            instr_map_[node_id] = instr_cache_[node_id].get(); // 存储原始指针用于快速访问
            std::cout << "[Simulator::initialize] Stored instruction for node_id:" << node_id << std::endl;
        } else {
            std::cout << "[Simulator::initialize] No instruction created for node_id:" << node_id << std::endl;
        }
    }
    
    initialized_ = true;
}

void Simulator::update_instruction_pointers() {
    // 更新指令映射中的指针（当数据缓冲区可能发生变化时）
    instr_map_.clear();
    for (const auto& pair : instr_cache_) {
        instr_map_[pair.first] = pair.second.get();
    }
}

void Simulator::eval() {
    if (!initialized_) {
        std::cerr << "[Simulator::eval] Error: Simulator not initialized!" << std::endl;
        return;
    }
    
    std::cout << "[Simulator::eval] Starting evaluation loop." << std::endl;
    for (auto* node : eval_list_) {
        if (!node) continue;
        
        std::cout << "[Simulator::eval] Processing node ID " << node->id() << " (ptr " << node << ")" << std::endl;
        uint32_t node_id = node->id();
        auto it = instr_map_.find(node_id);
        if (it != instr_map_.end() && it->second) {
            std::cout << "[Simulator::eval] Found instruction for node " << node_id << ", calling eval()." << std::endl;
            it->second->eval(data_map_);
        } else {
            // 文字节点等不需要指令的节点会进入这里，这是正常的
            std::cout << "[Simulator::eval] No instruction for node ID: " << node_id << std::endl;
        }
    }
    std::cout << "[Simulator::eval] Finished evaluation loop." << std::endl;
}

void Simulator::eval_range(size_t start, size_t end) {
    // 用于并行执行的辅助方法
    if (start >= eval_list_.size() || end > eval_list_.size() || start >= end) {
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
    std::cout << "[Simulator::tick] Calling eval()." << std::endl;
    eval();
}

void Simulator::tick(size_t count) {
    for (size_t i = 0; i < count; ++i) {
        std::cout << "[Simulator::tick] Tick " << (i + 1) << "/" << count << std::endl;
        tick();
    }
}

void Simulator::reset() {
    std::cout << "[Simulator::reset] Resetting simulator state." << std::endl;
    
    // 重置数据缓冲区（保留结构，只重置值）
    for (auto& pair : data_map_) {
        uint32_t node_id = pair.first;
        auto* node = ctx_->get_node_by_id(node_id);
        if (node && node->type() != ch::core::lnodetype::type_lit) {
            // 只重置非文字节点
            pair.second = ch::core::sdata_type(0, pair.second.bitwidth());
        }
    }
    
    // 重置指令状态（如果指令有重置方法）
    for (auto& pair : instr_cache_) {
        // 可以在这里调用指令的重置方法
        // pair.second->reset();
    }
    
    std::cout << "[Simulator::reset] Simulator state reset completed." << std::endl;
}

uint64_t Simulator::get_value_by_name(const std::string& name) const {
    if (!ctx_) return 0;
    
    // 遍历上下文中的所有节点
    for (const auto& node_ptr : ctx_->get_nodes()) {
        if (node_ptr && node_ptr->name() == name) {
            uint32_t node_id = node_ptr->id();
            auto it = data_map_.find(node_id);
            if (it != data_map_.end()) {
                const auto& bv = it->second.bv_;
                if (bv.num_words() > 0) {
                    return bv.words()[0];
                }
            }
        }
    }
    return 0;
}

} // namespace ch
