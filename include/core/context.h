// include/core/context.h
#ifndef CH_CORE_CONTEXT_H
#define CH_CORE_CONTEXT_H

#include <vector>
#include <unordered_map>
#include <memory>
#include <string>
#include <cstdint>
#include <source_location>

#include "types.h"
#include "context_interface.h"
#include "lnodeimpl.h"
#include "ast_nodes.h"
#include "logger.h"

namespace ch { namespace core {

extern thread_local context* ctx_curr_;

class ctx_swap {
public:
    explicit ctx_swap(context* new_ctx);
    ~ctx_swap();
    
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

    // 统一的节点创建方法，添加错误检查
    template <typename T, typename... Args>
    T* create_node(Args&&... args) {
        CHDBG_FUNC();
        CHREQUIRE(this != nullptr, "Context cannot be null");
        
        try {
            uint32_t new_id = next_node_id();
            auto node = std::make_unique<T>(new_id, std::forward<Args>(args)..., this);
            T* raw_ptr = node.get();
            
            node_storage_.push_back(std::move(node));
            node_map_[raw_ptr->id()] = raw_ptr;
            
            if (debug_context_lifetime) {
                CHINFO("Created node ID %u (%s) of type %d in context 0x%llx", 
                       new_id, raw_ptr->name().c_str(), static_cast<int>(raw_ptr->type()), 
                       (unsigned long long)this);
            }
            return raw_ptr;
        } catch (const std::bad_alloc&) {
            CHERROR("Failed to allocate memory for node creation");
            return nullptr;
        } catch (const std::exception& e) {
            CHERROR("Node creation failed: %s", e.what());
            return nullptr;
        }
    }

    // 专门的节点创建方法
    litimpl* create_literal(const sdata_type& value, 
                           const std::string& name = "literal", 
                           const std::source_location& sloc = std::source_location::current());
    
    inputimpl* create_input(uint32_t size, 
                           const std::string& name, 
                           const std::source_location& sloc = std::source_location::current());
    
    outputimpl* create_output(uint32_t size, 
                             const std::string& name, 
                             const std::source_location& sloc = std::source_location::current());

    const std::vector<std::unique_ptr<lnodeimpl>>& get_nodes() const { return node_storage_; }

    void print_debug_info() const;
    
    // 时钟和复位管理
    clockimpl* current_clock(const std::source_location& sloc);
    resetimpl* current_reset(const std::source_location& sloc);
    void set_current_clock(clockimpl* clk);
    void set_current_reset(resetimpl* rst);
    
    // 获取上下文信息
    const std::string& name() const { return name_; }
    context* parent() const { return parent_; }

private:
    void topological_sort_visit(lnodeimpl* node, std::vector<lnodeimpl*>& sorted,
                                std::unordered_map<lnodeimpl*, bool>& visited,
                                std::unordered_map<lnodeimpl*, bool>& temp_mark) const;
    void init();

    std::vector<std::unique_ptr<lnodeimpl>> node_storage_;
    std::unordered_map<uint32_t, lnodeimpl*> node_map_;
    uint32_t next_node_id_ = 0;
    clockimpl* current_clock_ = nullptr;
    resetimpl* current_reset_ = nullptr;
    std::string name_;
    context* parent_ = nullptr;
    
    // ID溢出保护
    static constexpr uint32_t MAX_NODE_ID = UINT32_MAX - 1000;
    
};

}} // namespace ch::core

#endif // CH_CORE_CONTEXT_H
