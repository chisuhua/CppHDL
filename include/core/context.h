#ifndef CH_CORE_CONTEXT_H
#define CH_CORE_CONTEXT_H

#include <cstdint>
#include <memory>
#include <source_location>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "abstract/context_interface.h"
#include "ast_nodes.h"
#include "lnodeimpl.h"
#include "types.h"

// Forward declaration
namespace ch {
namespace detail {
class destruction_manager;
}
} // namespace ch

namespace ch::core {

/* 使用thread_local的理由:
1. **线程隔离性**：每个线程都有独立的变量副本，互不干扰
2. **上下文切换**：在线程内串行执行不同的 context，`thread_local`
能正确维护当前状态
3. **性能优势**：无需锁机制，访问速度快
4. **简化设计**：避免了复杂的线程同步代码
*/

extern /*thread_local*/ context *ctx_curr_;

class ctx_swap {
public:
    explicit ctx_swap(context *new_ctx);
    ~ctx_swap();

    ctx_swap(const ctx_swap &) = delete;
    ctx_swap &operator=(const ctx_swap &) = delete;
    ctx_swap(ctx_swap &&) = delete;
    ctx_swap &operator=(ctx_swap &&) = delete;

private:
    context *old_ctx_;
};

// Function to access the debug flag to avoid static initialization issues
// Returns false during static destruction to prevent segfaults
bool &debug_context_lifetime();

class context : public abstract::context_interface {
public:
    explicit context(const std::string &name = "unnamed",
                     context *parent = nullptr);
    ~context();

    // 实现抽象接口
    uint32_t next_node_id() override;
    std::vector<lnodeimpl *> get_eval_list() const override;
    void set_as_current_context() override;
    lnodeimpl *get_node_by_id(uint32_t id) const override;

    // 统一的节点创建方法，添加错误检查
    template <typename T, typename... Args> T *create_node(Args &&...args);

    // 专门的节点创建方法
    litimpl *create_literal(
        const sdata_type &value, const std::string &name = "literal",
        const std::source_location &sloc = std::source_location::current());

    inputimpl *create_input(
        uint32_t size, const std::string &name,
        const std::source_location &sloc = std::source_location::current());

    outputimpl *create_output(
        uint32_t size, const std::string &name,
        const std::source_location &sloc = std::source_location::current());

    memimpl *create_memory(
        uint32_t addr_width, uint32_t data_width, uint32_t depth,
        uint32_t num_banks, bool has_byte_enable, bool is_rom,
        const std::vector<sdata_type> &init_data, // 使用vector格式
        const std::string &name,
        const std::source_location &sloc = std::source_location::current());

    mem_read_port_impl *create_mem_read_port(
        memimpl *parent, uint32_t port_id, uint32_t size, lnodeimpl *cd,
        lnodeimpl *addr, lnodeimpl *enable, lnodeimpl *data_output,
        const std::string &name,
        const std::source_location &sloc = std::source_location::current());

    mem_write_port_impl *create_mem_write_port(
        memimpl *parent, uint32_t port_id, uint32_t size, lnodeimpl *cd,
        lnodeimpl *addr, lnodeimpl *wdata, lnodeimpl *enable,
        const std::string &name,
        const std::source_location &sloc = std::source_location::current());

    const std::vector<std::unique_ptr<lnodeimpl>> &get_nodes() const {
        return node_storage_;
    }

    void print_debug_info() const;

    // 时钟和复位管理
    clockimpl *current_clock(
        const std::source_location &sloc = std::source_location::current());
    resetimpl *current_reset(
        const std::source_location &sloc = std::source_location::current());
    void set_current_clock(clockimpl *clk);
    void set_current_reset(resetimpl *rst);

    clockimpl *create_clock(
        const sdata_type &init_value, bool posedge, bool negedge,
        const std::string &name,
        const std::source_location &sloc = std::source_location::current());

    resetimpl *create_reset(
        const sdata_type &init_value, resetimpl::reset_type rtype,
        const std::string &name,
        const std::source_location &sloc = std::source_location::current());
    // 获取上下文信息
    const std::string &name() const { return name_; }
    context *parent() const { return parent_; }

    // 默认时钟相关方法
    void set_default_clock(core::clockimpl *clk);
    core::clockimpl *get_default_clock() const;
    bool has_default_clock() const { return default_clock_ != nullptr; }

    // 默认复位相关方法
    core::resetimpl *get_default_reset() const;
    bool has_default_reset() const { return default_reset_ != nullptr; }

private:
    void
    topological_sort_visit(lnodeimpl *node, std::vector<lnodeimpl *> &sorted,
                           std::unordered_map<lnodeimpl *, bool> &visited,
                           std::unordered_map<lnodeimpl *, bool> &temp_mark,
                           std::unordered_set<lnodeimpl *> &cyclic_nodes,
                           std::unordered_set<lnodeimpl *> &update_list) const;
    void init();

    std::vector<std::unique_ptr<lnodeimpl>> node_storage_;
    uint32_t next_node_id_ = 0;
    clockimpl *current_clock_ = nullptr;
    resetimpl *current_reset_ = nullptr;
    std::string name_;
    context *parent_ = nullptr;
    bool destructing_ = false; // 标记是否正在析构

    // ID溢出保护
    static constexpr uint32_t MAX_NODE_ID = UINT32_MAX - 1000;
    core::clockimpl *default_clock_ = nullptr; // 默认时钟
    core::resetimpl *default_reset_ = nullptr; // 默认复位
};

} // namespace ch::core

#include "../lnode/context.tpp"

#endif // CH_CORE_CONTEXT_H