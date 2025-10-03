// include/lnodeimpl.h
#ifndef LNODEIMPL_H
#define LNODEIMPL_H

#include <cstdint>
#include <vector>
#include <string>
#include <source_location>
#include <unordered_map>
#include <sstream>

// Forward declarations to avoid circular dependencies
namespace ch { namespace core {
    class context;
    struct sdata_type;
}}

namespace ch { namespace core {
#define CH_LNODE_TYPE(t) type_##t,
#define CH_LNODE_COUNT(n) +1
#define CH_LNODE_ENUM(m) \
  m(none) \
  m(lit) \
  m(proxy) \
  m(input) \
  m(output) \
  m(op) \
  m(reg) \
  m(mem)


enum class lnodetype {
    CH_LNODE_ENUM(CH_LNODE_TYPE)
};

constexpr std::size_t ch_lnode_type_count() {
    return (0 CH_LNODE_ENUM(CH_LNODE_COUNT));
}


// --- Declare to_string (defined in .cpp) ---
const char* to_string(lnodetype type);

// --- ch_op enum ---
enum class ch_op {
    add, sub, mul, div, mod,
    and_, or_, xor_, not_,
    eq, ne, lt, le, gt, ge,
};

class lnodeimpl;

// Forward declarations
template<typename T> struct lnode;
using clone_map = std::unordered_map<uint32_t, lnodeimpl*>;

// --- lnodeimpl base class ---
class lnodeimpl {
public:
    lnodeimpl(uint32_t id, lnodetype type, uint32_t size, context* ctx,
              const std::string& name, const std::source_location& sloc)
        : id_(id), type_(type), size_(size), ctx_(ctx), name_(name), sloc_(sloc) {}

    virtual ~lnodeimpl() = default;

    // Accessors
    uint32_t id() const { return id_; }
    lnodetype type() const { return type_; }
    uint32_t size() const { return size_; }
    context* ctx() const { return ctx_; }
    const std::string& name() const { return name_; }
    const std::source_location& sloc() const { return sloc_; }

    // Source management
    uint32_t add_src(lnodeimpl* src) {
        if (src) {
            srcs_.emplace_back(src);
            return static_cast<uint32_t>(srcs_.size() - 1);
        }
        return static_cast<uint32_t>(-1);
    }

    void set_src(uint32_t index, lnodeimpl* src) {
        if (index < srcs_.size() && src) {
            srcs_[index] = src;
        } else if (index == srcs_.size() && src) {
            add_src(src);
        }
    }

    lnodeimpl* src(uint32_t index) const {
        return (index < srcs_.size()) ? srcs_[index] : nullptr;
    }

    uint32_t num_srcs() const { return static_cast<uint32_t>(srcs_.size()); }
    const std::vector<lnodeimpl*>& srcs() const { return srcs_; }

    // Virtual methods
    virtual std::string to_string() const {
        std::ostringstream oss;
        oss << name_ << " (" << ::ch::core::to_string(type_) << ", " << size_ << " bits)";
        return oss.str();
    }

    virtual bool is_const() const { return type_ == lnodetype::type_lit; }

    virtual lnodeimpl* clone(context* new_ctx, const clone_map& cloned_nodes) const {
        return nullptr;
    }

    virtual bool equals(const lnodeimpl& other) const {
        if (type_ != other.type_ ||
            size_ != other.size_ ||
            name_ != other.name_ ||
            srcs_.size() != other.srcs_.size()) {
            return false;
        }
        for (size_t i = 0; i < srcs_.size(); ++i) {
            if (srcs_[i] != other.srcs_[i]) {
                return false;
            }
        }
        return true;
    }

protected:
    uint32_t id_;
    lnodetype type_;
    uint32_t size_;
    context* ctx_;
    std::string name_;
    std::source_location sloc_;
    std::vector<lnodeimpl*> srcs_;
};

}} // namespace ch::core

#endif // LNODEIMPL_H
