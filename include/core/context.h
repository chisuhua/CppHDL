#pragma once

#include <cstdint>
#include <memory>
#include <source_location>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "ast/ast_nodes.h"
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

extern thread_local context *ctx_curr_;

/// ADR-059 D10：节点归属抑制。
/// `context::init()` 建 `default_clock` / `default_reset` 期间 MUST 开启
/// ⇒ ⇒ 「context 级节点不归属任何组件」这条契约才成立。
/// ⇒ 由 `Component::node_owner()` 查询。
bool node_ownership_suppressed();

/// **节点名唯一性策略**（**opt-in** · 默认宽松 · `core-ctx-node-name-uniqueness`）。
///
/// 为什么需要它：解释器侧**不改名**（同名节点共存），而 Verilog codegen 会**自动加
/// `_N` 后缀** ⇒ ⇒ 两后端看到**不同的节点名** ⇒ ⇒ 「按名匹配观测点」的契约在两后端
/// **不等价** ⇒ ⇒ 解释器侧会**静默采到错的那个节点**（读到貌似合理的错值 ⇒ 假绿）。
///
/// ⚠️ **默认 MUST 是宽松**（`kAllowDuplicates`）：仓内 `include/` 有 **641** 个 `ch_in`
///    声明**零**显式名字（`.rddf/roadmap` 的实测）⇒ ⇒ 默认开启会让全仓大面积报错
///    ⇒ ⇒ 那是**规格预期**，不是缺陷 ⇒ ⇒ 检测 MUST 是 opt-in。
/// ⚠️ ctx MUST **不改名**：冲突只由「放行」或「报错」两种方式处理（自动改名会让不同
///    后端看到不同名字 ⇒ 契约按名匹配失效）。
enum class NodeNamePolicy {
    /// 默认：同名节点可共存 ⇒ **既有行为零变化**
    kAllowDuplicates = 0,
    /// 严格：同名节点 MUST 在**创建点** fail-fast（抛异常中止）
    kRejectDuplicates = 1,
};

struct NodeOwnershipSuppression {
    NodeOwnershipSuppression();
    ~NodeOwnershipSuppression();
    NodeOwnershipSuppression(const NodeOwnershipSuppression &) = delete;
    NodeOwnershipSuppression &operator=(const NodeOwnershipSuppression &) = delete;
};

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

class context {
public:
    explicit context(const std::string &name = "unnamed",
                     context *parent = nullptr);
    ~context();

    // 上下文方法
    uint32_t next_node_id();
    std::vector<lnodeimpl *> get_eval_list() const;
    void set_as_current_context();
    lnodeimpl *get_node_by_id(uint32_t id) const;

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

    // ── 节点名唯一性策略（**opt-in** · `core-ctx-node-name-uniqueness`）──────────
    //
    // ⚠️ **唯一判定实现**在本类的 `check_node_name_unique()` ⇒ ⇒ `create_node` 只调用它
    //    （MUST NOT 另立第二份判定）。
    // ⚠️ 默认 = `kAllowDuplicates` ⇒ ⇒ 既有代码行为**零变化**（含产物逐字节不变）。

    /// 当前策略（默认 `kAllowDuplicates`）
    NodeNamePolicy node_name_policy() const { return node_name_policy_; }

    /// 切策略（同 ctx 上可随时切换；重复调用幂等）。
    ///
    /// ⚠️ **切到 `kRejectDuplicates` 时 MUST 先把既有节点的名字「播种」进登记表**
    ///    （见 `context.cpp`）—— 否则「先宽松建 DUT、后开严格保护观测点」这个用法下，
    ///    对**已存在**的名字**零判别力** ⇒ 保护是**幻觉** ⇒ 恰好回到本 feature 要治的
    ///    「按名采到错的节点 ⇒ 假绿」。
    /// ⚠️ **适用边界**（用户面须知道）：严格模式判的是 **ctx 内全部节点**（含框架内部
    ///    派生节点）⇒ ⇒ ① `_` 前缀是**框架保留**命名空间（`create_input` / `create_output`
    ///    会额外建 `"_" + name` 的 proxy）⇒ 用户自己的 `"a"` 与 `"_a"` 会被判重；
    ///    ② 内部默认名（`ch_reg` 的 `reg` / `mux` 的默认名等）会让「未经显式命名」的
    ///    普通设计必然报错 ⇒ 该模式适用于**全部节点都显式命名**的设计与框架自测。
    void set_node_name_policy(NodeNamePolicy policy);

    /// 静态析构期**降级路径**的违规记录（正常路径**抛异常** ⇒ 不写这里）。
    ///
    /// 为什么需要：所有 `CHFATAL_EXCEPTION` 一类宏都有 `in_static_destruction()` 守卫
    /// ⇒ ⇒ 静态析构期**抛异常**会 `std::terminate` ⇒ ⇒ 必须降级为「记录 + 失败结果」
    /// （否则「off 不抛」与「on 抛」之间存在**未定义带**）。
    const std::vector<std::string> &duplicate_name_violations() const {
        return duplicate_name_violations_;
    }

    /// **节点名唯一性检查**（唯一判定实现 · 由 `create_node` 在构造**成功之后**调用）。
    ///
    /// 返回 `true` = 通过（名字已登记）；`false` = **降级失败**
    /// （仅静态析构期：记录到 `duplicate_name_violations()` 且**不**登记 ⇒ 调用方 MUST
    /// **放弃**该节点）。
    ///
    /// ⚠️ **非**静态析构期命中冲突 ⇒ **抛 `std::runtime_error`**（fail-fast）
    ///    ⇒ ⇒ 调用方（`create_node`）MUST 把本调用放在自己的 `try` **之外** ——
    ///    否则它的 `catch (const std::exception &)` 会**吞掉**本异常 ⇒ 退化成静默 `nullptr`。
    bool check_node_name_unique(const std::string &name,
                               const std::source_location &sloc);

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
    // Per-context counter retained for backward compatibility; IDs are now
    // allocated from a process-wide atomic counter to guarantee uniqueness
    // across nested contexts (ch::ch_device creates a device-level context for
    // IO ports and a child context for opimpls, so per-context counters
    // collide on node_id == 2 for the first input and the first opimpl).
    uint32_t next_node_id_ = 0;
    clockimpl *current_clock_ = nullptr;
    resetimpl *current_reset_ = nullptr;
    std::string name_;
    context *parent_ = nullptr;
    bool destructing_ = false; // 标记是否正在析构

    // 节点名唯一性（**仅在 `kRejectDuplicates` 下维护** ⇒ 默认路径**零开销**）
    NodeNamePolicy node_name_policy_ = NodeNamePolicy::kAllowDuplicates;
    /// 已登记的名字（值 = 出现次数；`> 1` 在 strict 下不可能出现 —— 冲突当场失败）
    std::unordered_map<std::string, uint32_t> node_name_counts_;
    /// 静态析构期降级路径的违规记录（正常路径为空）
    std::vector<std::string> duplicate_name_violations_;

    // ID溢出保护
    static constexpr uint32_t MAX_NODE_ID = UINT32_MAX - 1000;

    // ⚠️ 单时钟域约束：CppHDL 当前仅支持单时钟域
    // 所有寄存器共享 default_clock_ 和 default_reset_，不支持多时钟设计
    // 不支持 CDC（Clock Domain Crossing），不支持多时钟 FIFO
    // 参见: docs/adr/ADR-008-single-clock-domain-constraint.md
    core::clockimpl *default_clock_ = nullptr; // 默认时钟（唯一）
    core::resetimpl *default_reset_ = nullptr; // 默认复位（唯一）
};

} // namespace ch::core

#include "../lnode/context.tpp"
