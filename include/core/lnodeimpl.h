// include/lnodeimpl.h
#ifndef LNODEIMPL_H
#define LNODEIMPL_H

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
        m(mem_read_port) m(mem_write_port) m(clock) m(reset)

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

    // 新增：清理源节点引用，用于销毁时避免循环引用
    void clear_sources() { srcs_.clear(); }
    ch::Component *get_parent() { return parent_; }

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

} // namespace ch::core

#endif // LNODEIMPL_H
