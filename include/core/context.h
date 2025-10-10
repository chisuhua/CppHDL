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
#include "context_interface.h"
#include "lnodeimpl.h"
#include "ast_nodes.h"

namespace ch { namespace core {

// 改为 thread_local 确保线程安全
extern thread_local context* ctx_curr_;

class ctx_swap {
public:
    explicit ctx_swap(context* new_ctx);
    ~ctx_swap();
    
    // 明确禁止拷贝和移动
    ctx_swap(const ctx_swap&) = delete;
    ctx_swap& operator=(const ctx_swap&) = delete;
    ctx_swap(ctx_swap&&) = delete;
    ctx_swap& operator=(ctx_swap&&) = delete;

private:
    context* old_ctx_;
};

extern bool debug_context_lifetime;

class context : public abstract::context_interface {
public:
    explicit context(const std::string& name = "unnamed", context* parent = nullptr);
    ~context();

    // 实现抽象接口
    uint32_t next_node_id() override;
    std::vector<lnodeimpl*> get_eval_list() const override;
    void set_as_current_context() override;
    lnodeimpl* get_node_by_id(uint32_t id) const override;

    // 统一的节点创建方法，确保内存管理一致性
    template <typename T, typename... Args>
    T* create_node(Args&&... args) {
        static_assert(std::is_base_of_v<lnodeimpl, T>, "T must derive from lnodeimpl");
        uint32_t new_id = next_node_id_++;
        
        // 使用 make_unique 确保异常安全
        auto node = std::make_unique<T>(new_id, std::forward<Args>(args)..., this);
        T* raw_ptr = node.get();
        
        // 统一存储管理
        node_storage_.push_back(std::move(node));
        node_map_[raw_ptr->id()] = raw_ptr;
        
        if (debug_context_lifetime) {
            std::cout << "[context::create_node] Created node ID " << new_id 
                      << " (" << raw_ptr->name() << ") of type " << static_cast<int>(raw_ptr->type()) 
                      << " in context " << this << std::endl;
        }
        return raw_ptr;
    }

    // 专门的节点创建方法，确保接口一致性
    litimpl* create_literal(const sdata_type& value, 
                           const std::string& name = "literal", 
                           const std::source_location& sloc = std::source_location::current());
    
    inputimpl* create_input(uint32_t size, 
                           const std::string& name, 
                           const std::source_location& sloc = std::source_location::current());
    
    outputimpl* create_output(uint32_t size, 
                             const std::string& name, 
                             const std::source_location& sloc = std::source_location::current());

    // 提供安全的节点访问接口
    const std::vector<std::unique_ptr<lnodeimpl>>& get_nodes() const { return node_storage_; }

    void print_debug_info() const {
        std::cout << "[context::print_debug_info] Context " << this << ", name: " << name_ 
                  << ", nodes: " << node_storage_.size() << std::endl;
    }

    // 时钟和复位管理
    clockimpl* current_clock(const std::source_location& sloc);
    resetimpl* current_reset(const std::source_location& sloc);
    void set_current_clock(clockimpl* clk);
    void set_current_reset(resetimpl* rst);

private:
    void topological_sort_visit(lnodeimpl* node, std::vector<lnodeimpl*>& sorted,
                                std::unordered_map<lnodeimpl*, bool>& visited,
                                std::unordered_map<lnodeimpl*, bool>& temp_mark) const;
    void init();

    // 统一的节点存储容器
    std::vector<std::unique_ptr<lnodeimpl>> node_storage_;
    std::unordered_map<uint32_t, lnodeimpl*> node_map_;
    
    // 节点ID管理
    uint32_t next_node_id_ = 0;
    
    // 时钟和复位信号
    clockimpl* current_clock_ = nullptr;
    resetimpl* current_reset_ = nullptr;
    
    // 上下文信息
    std::string name_;
    context* parent_ = nullptr;
};

}} // namespace ch::core

#endif // CH_CORE_CONTEXT_H
