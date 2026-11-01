// include/lnodeimpl.h
#pragma once

#include <algorithm> // Add this for std::remove
#include <cstdint>
#include <memory>
#include <source_location>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

#include "instr_base.h"
#include "logger.h"

// Forward declarations to avoid circular dependencies
namespace ch {
class Component;
namespace core {
class context;
struct sdata_type;
} // namespace core
} // namespace ch

namespace ch::core {
#define CH_LNODE_TYPE(t) type_##t,
#define CH_LNODE_COUNT(n) +1
#define CH_LNODE_ENUM(m)                                                       \
    m(none) m(lit) m(proxy) m(input) m(output) m(op) m(reg) m(mem) m(mux)      \
        m(mem_read_port) m(mem_write_port) m(clock) m(reset) m(bitsupdate)

enum class lnodetype { CH_LNODE_ENUM(CH_LNODE_TYPE) };

constexpr std::size_t ch_lnode_type_count() {
    return (0 CH_LNODE_ENUM(CH_LNODE_COUNT));
}

// --- Declare to_string (defined in .cpp) ---
const char *to_string(lnodetype type);

enum class ch_op {
    add,
    sub,
    mul,
    div,
    mod,
    and_,
    or_,
    xor_,
    not_,
    eq,
    ne,
    lt,
    le,
    gt,
    ge,
    shl,          // 左移
    shr,          // 右移（逻辑右移）
    sshr,         // 算术右移
    neg,          // 负号（一元负）
    bit_sel,      // 位选择
    bits_extract, // 位提取
    bits_update,  // 位赋值
    concat,       // 连接
    sext,         // 符号扩展
    zext,         // 零扩展
    mux,          // 多路选择器
    and_reduce,
    or_reduce,
    xor_reduce,
    rotate_l, // 循环左移（预留）
    rotate_r, // 循环右移（预留）
    popcount, // 添加popcount操作
    assign    // 添加assign操作
};

// 内存端口类型
enum class mem_port_type {
    async_read, // 异步读端口
    sync_read,  // 同步读端口
    write       // 写端口
};

class lnodeimpl;

// Forward declarations
template <typename T> struct lnode;
using clone_map = std::unordered_map<uint32_t, lnodeimpl *>;

// --- lnodeimpl base class ---
class lnodeimpl {
public:
    lnodeimpl(uint32_t id, lnodetype type, uint32_t size, context *ctx,
              const std::string &name, const std::source_location &sloc);

    virtual ~lnodeimpl() = default;

    // Accessors
    uint32_t id() const { return id_; }
    lnodetype type() const { return type_; }
    uint32_t size() const { return size_; }
    context *ctx() const { return ctx_; }
    const std::string &name() const { return name_; }
    void set_name(const std::string &name) { name_ = name; }  // T002: 允许设置节点名称
    const std::source_location &sloc() const { return sloc_; }

    // Source management
    uint32_t add_src(lnodeimpl *src) {
        if (src) {
            srcs_.emplace_back(src);
            // Automatically add this node as a user of the source
            src->add_user(this);
            return static_cast<uint32_t>(srcs_.size() - 1);
        }
        return static_cast<uint32_t>(-1);
    }

    void set_src(uint32_t index, lnodeimpl *src) {
        if (index < srcs_.size() && src) {
            // Remove user relationship from old source if it exists
            if (srcs_[index]) {
                srcs_[index]->remove_user(this);
            }
            srcs_[index] = src;
            // Add user relationship to new source
            src->add_user(this);
        } else if (index == srcs_.size() && src) {
            add_src(src);
        }
    }

    lnodeimpl *src(uint32_t index) const {
        return (index < srcs_.size()) ? srcs_[index] : nullptr;
    }

    uint32_t num_srcs() const { return static_cast<uint32_t>(srcs_.size()); }
    const std::vector<lnodeimpl *> &srcs() const { return srcs_; }

    // User management
    void add_user(lnodeimpl *user) {
        if (user) {
            users_.push_back(user);
            CHDBG(" DAG chain: node ID %u(%s)  -> %u(%s)", this->id_,
                  this->to_string().c_str(), user->id(),
                  user->to_string().c_str());
        }
    }

    void remove_user(lnodeimpl *user) {
        if (user) {
            users_.erase(std::remove(users_.begin(), users_.end(), user),
                         users_.end());
        }
    }

    const std::vector<lnodeimpl *> &get_users() const { return users_; }

    // Virtual methods
    virtual std::string to_string() const {
        std::ostringstream oss;
        oss << name_ << " (" << ::ch::core::to_string(type_) << ", " << size_
            << " bits)";
        return oss.str();
    }

    virtual bool is_const() const { return type_ == lnodetype::type_lit; }

    virtual lnodeimpl *clone(context *new_ctx,
                             const clone_map &cloned_nodes) const {
        (void)new_ctx;      // 显式标记为已使用以避免编译器警告
        (void)cloned_nodes; // 显式标记为已使用以避免编译器警告
        return nullptr;
    }

    virtual bool equals(const lnodeimpl &other) const {
        if (type_ != other.type_ || size_ != other.size_ ||
            name_ != other.name_ || srcs_.size() != other.srcs_.size()) {
            return false;
        }
        for (size_t i = 0; i < srcs_.size(); ++i) {
            if (srcs_[i] != other.srcs_[i]) {
                return false;
            }
        }
        return true;
    }

    // 新增：创建对应的仿真指令
    virtual std::unique_ptr<ch::instr_base>
    create_instruction(ch::data_map_t &data_map) const {
        // 默认实现返回空指针，具体节点类可以重写
        return nullptr;
    }

    // 新增：清理所有引用关系，用于销毁时断开 DAG 链接
    // 同时清理 srcs_（指向输入）和 users_（指向输出），确保双向引用完整断开
    void clear_sources() {
        srcs_.clear();
        users_.clear();
    }
    ch::Component *get_parent() { return parent_; }

    // Returns the currently active Component (the one whose describe() is
    // running). Forwarded from ch::Component::current() so headers that only
    // see the forward declaration of ch::Component (e.g. io.h) can still
    // obtain a "current module" pointer for runtime assertions.
    static ch::Component *current_component();

protected:
    uint32_t id_;
    lnodetype type_;
    uint32_t size_;
    context *ctx_;
    std::string name_;
    std::source_location sloc_;
    std::vector<lnodeimpl *> srcs_;
    std::vector<lnodeimpl *> users_; // Track which nodes use this node
    ch::Component *parent_;
};

/**
 * @brief 节点的**驱动状态**（只读诊断；MUST NOT 改变任何读取语义）
 *
 * ⚠️ **为什么需要它**（B7 · 框架层）：框架对「读一个**未被驱动**的字段 /
 *   代理 / 端口」返回 **0** 且**不报错** ⇒ ⇒ **静默**（本仓第 3 个实例，
 *   前两个是 `ch_in` 的 `operator=` 与未驱动代理节点）。
 *   ⇒ ⇒ 「**诊断**」必须与「**读取**」**分开**：本枚举**只回答「它被驱动了
 *   没有」**，MUST NOT 改变任何读取行为（⇒ ⇒ 对既有 194+ 个测试**零回归**）。
 *
 * ⚠️ **为什么是枚举而不是三个 bool**：两个「响亮拒绝」的实现（`stream.h` 的
 *   `detail::drive_master_ready` 与 `stream_pipeline.h` 的
 *   `detail::drive_upstream_ready`）需要区分**三种**「驱动不了」+ **一种**
 *   「可以覆盖默认值」⇒ ⇒ 若各自用 bool 组合表达 ⇒ ⇒ **两份实现必然漂移**
 *   ⇒ ⇒ ⇒ 用**同一个枚举** ⇒ ⇒ 判据只有**一处**。
 */
enum class drive_state {
    /// 节点**不存在**（空指针 / 字段从未建节点）⇒ ⇒ `<<=` 会另建节点并
    /// **只绑到本地副本** ⇒ ⇒ 调用方**看不到**任何效果（静默失效）
    no_node,
    /// 节点本身是**字面量**（值不随 src 变化）⇒ ⇒ 驱动它**无效**
    literal,
    /// 节点存在但**没有驱动源**（未接的 proxy / 未接的顶层端口）
    undriven,
    /// 有驱动源，但那个源是**字面量**（= 组合子的**默认值** ⇒ ⇒ **可以**被
    /// 消费端覆盖 —— ADR-054 形态 E）
    driven_by_literal,
    /// 被**真信号**驱动（⇒ ⇒ 再驱动是 last-wins ⇒ ⇒ **静默接管**别人的信号）
    driven,
};

/// 只读诊断：判定一个节点的驱动状态（`nullptr` ⇒ `no_node`）
///
/// ⚠️ **判定细则**（唯一的**一处**实现 ⇒ ⇒ 护栏与组合子共用 ⇒ ⇒ 不会漂移）：
/// · `literal` 节点**自驱动**（值固定）⇒ ⇒ `literal`；
/// · `proxy` / `input` 且**无 src** ⇒ ⇒ `undriven`；
/// · 有 src 且 src 是**字面量** ⇒ ⇒ `driven_by_literal`；
/// · 其余（组合逻辑 / 寄存器 / 已接真信号的 proxy / 端口）⇒ ⇒ `driven`。
inline drive_state diagnose_drive(const lnodeimpl *n) {
    if (n == nullptr) {
        return drive_state::no_node;
    }
    if (n->type() == lnodetype::type_lit) {
        return drive_state::literal;
    }
    if (n->num_srcs() == 0) {
        switch (n->type()) {
        case lnodetype::type_proxy:
        case lnodetype::type_input:
            return drive_state::undriven;
        default:
            // 寄存器 / 组合逻辑 / 内存等**自驱动**节点（它们不从 src 取值）
            return drive_state::driven;
        }
    }
    const lnodeimpl *src = n->src(0);
    if (src != nullptr && src->type() == lnodetype::type_lit) {
        return drive_state::driven_by_literal;
    }
    return drive_state::driven;
}

/// 只读诊断的**便捷谓词**（与 `diagnose_drive` 同源 ⇒ ⇒ 不会漂移）
inline bool node_is_driven(const lnodeimpl *n) {
    const drive_state st = diagnose_drive(n);
    return st == drive_state::driven || st == drive_state::driven_by_literal;
}

inline bool node_is_literal(const lnodeimpl *n) {
    return diagnose_drive(n) == drive_state::literal;
}

inline bool node_exists(const lnodeimpl *n) { return n != nullptr; }

} // namespace ch::core

