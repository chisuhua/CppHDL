#ifndef LNODEIMPL_H
#define LNODEIMPL_H

#include <cstdint>
#include <vector>
#include <memory> // For std::unique_ptr, potentially for ownership
#include <string>
#include <source_location> // C++20
#include <iostream> // For debug prints if needed
#include <unordered_map> // For clone_map
#include <cassert> // For assertions

// Forward declaration of context - but we need its full definition for constructors
// So we include it, assuming context.h includes lnodeimpl.h in a way that doesn't cause circular dependency
// (e.g., context.h declares node types, includes lnodeimpl, then defines methods in .cpp)
#include "core/context.h" // Now includes types.h and has node_id()

namespace ch { namespace core {

// --- Enum for Node Types using macros (借鉴 cash) ---
#define CH_LNODE_TYPE(t) type_##t,
// --- 定义用于计数的辅助宏 ---
#define CH_LNODE_COUNT(n) +1
#define CH_LNODE_ENUM(m) \
  m(none) \
  m(lit) \
  m(proxy) \
  m(input) \
  m(output) \
  m(op) \
  m(reg) \
  m(mem) \
  /* Add other types as needed, e.g., m(cd), m(module), etc. */

enum class lnodetype {
  CH_LNODE_ENUM(CH_LNODE_TYPE)
};

// --- 定义计算节点类型数量的 constexpr ---
// 使用 CH_LNODE_ENUM 宏和 CH_LNODE_COUNT 辅助宏来计算总数
// CH_LNODE_ENUM(CH_LNODE_COUNT) 会展开为 +1+1+1... (每个条目一个 +1)
// (0 CH_LNODE_ENUM(CH_LNODE_COUNT)) 计算最终的计数
constexpr std::size_t ch_lnode_type_count() {
    return (0 CH_LNODE_ENUM(CH_LNODE_COUNT));
}
// --- 结束计数辅助宏定义 ---
#undef CH_LNODE_COUNT // 不再需要此宏

// Helper to get string name for lnodetype (can be implemented later)
const char* to_string(lnodetype type);

// Forward declaration for sdata_type (defined in types.h, now included via context.h)
// struct sdata_type; // Already included via context.h

// Forward declaration for lnode (defined elsewhere, likely in logic.h)
template<typename T> struct lnode;

// --- Enum for Operation Types (ch_op) ---
// Placed here for completeness, used by opimpl
enum class ch_op {
    add, sub, mul, div, mod, // Arithmetic
    and_, or_, xor_, not_,   // Bitwise
    eq, ne, lt, le, gt, ge,  // Comparison
    // Add other operations as needed
    // Note: '_' is added to 'and', 'or', 'not' to avoid C++ keywords.
};

// --- Base Class for all IR Nodes (lnodeimpl) ---
// This is the core abstract base class representing a node in the IR graph.
// Borrowing concepts from cash: ID, type, size, context, name, sloc, srcs.
class lnodeimpl {
public:
    // Constructor: Initializes common properties like ID, type, size, context, name, location
    // Now takes ID as the first argument from context::create_node
    lnodeimpl(uint32_t id, lnodetype type, uint32_t size, context* ctx,
              const std::string& name, const std::source_location& sloc)
        : id_(id), type_(type), size_(size), ctx_(ctx), name_(name), sloc_(sloc) {}

    // Virtual destructor for safe polymorphic deletion
    virtual ~lnodeimpl() = default;

    // --- Accessor Methods (借鉴 cash) ---
    uint32_t id() const { return id_; }
    lnodetype type() const { return type_; }
    uint32_t size() const { return size_; }
    context* ctx() const { return ctx_; }
    const std::string& name() const { return name_; }
    const std::source_location& sloc() const { return sloc_; }

    // --- Source Management (数据流图构建, 借鉴 cash) ---
    // Add a source node (this node depends on the source)
    uint32_t add_src(lnodeimpl* src) {
        if (src) {
            srcs_.emplace_back(src); // Using emplace_back for efficiency
            return static_cast<uint32_t>(srcs_.size() - 1); // Return index of added source
        }
        return static_cast<uint32_t>(-1); // Or throw an exception for invalid src
    }

    // Set a source node at a specific index (NEW)
    void set_src(uint32_t index, lnodeimpl* src) {
        if (index < srcs_.size() && src) { // Check bounds
            srcs_[index] = src; // Replace the source at the given index
        } else if (index == srcs_.size() && src) {
             add_src(src); // If index == size, append (extend the list)
        }
        // Optionally: Handle index > size (e.g., resize with nullptrs, or error)
        // For now, silently ignore invalid indices > size or null src for existing slots.
    }

    // Get a source node by index
    lnodeimpl* src(uint32_t index) const {
        if (index < srcs_.size()) {
            return srcs_[index];
        }
        return nullptr; // Or throw an exception for invalid index
    }

    // Get the number of source nodes
    uint32_t num_srcs() const { return static_cast<uint32_t>(srcs_.size()); }

    // Get the entire source vector (const reference)
    const std::vector<lnodeimpl*>& srcs() const { return srcs_; }


    // --- Virtual Methods for Specific Node Behavior (借鉴 cash) ---
    // Example: Pretty printing the node
    virtual void print(std::ostream& os) const {
        os << name_ << " (" << to_string(type_) << ", " << size_ << " bits)";
    }

    // Example: Checking if the node is a constant/literal (can be overridden)
    virtual bool is_const() const { return type_ == lnodetype::type_lit; }

    // Example: Clone method (for optimizations like constant folding, cloning modules)
    // This is a simplified version, real implementation needs context and map
    virtual lnodeimpl* clone(context* new_ctx, const std::unordered_map<uint32_t, lnodeimpl*>& cloned_nodes) const {
        // Default implementation: return nullptr, derived classes should override
        return nullptr;
    }

    // Example: Equality check (for optimizations)
    // REMOVED sloc_ comparison due to std::source_location not being comparable
    virtual bool equals(const lnodeimpl& other) const {
        // Check type, size, name, and *direct* srcs equality
        // Note: This doesn't recursively check srcs' equality, which might be needed for full IR equality.
        if (this->type_ != other.type_ ||
            this->size_ != other.size_ ||
            this->name_ != other.name_ ||
            // this->sloc_ != other.sloc_ || // Removed
            this->srcs_.size() != other.srcs_.size()) {
            return false;
        }
        for (size_t i = 0; i < this->srcs_.size(); ++i) {
            if (this->srcs_[i] != other.srcs_[i]) { // Compare pointers directly for now
                return false;
            }
        }
        return true;
    }


protected:
    uint32_t id_;                     // Unique identifier within the context (借鉴 cash)
    lnodetype type_;                  // Type of the node (reg, op, etc.) (借鉴 cash)
    uint32_t size_;                   // Bit width of the node's value (借鉴 cash)
    context* ctx_;                    // Pointer to the context this node belongs to (借鉴 cash)
    std::string name_;                // Name for debugging/printing (借鉴 cash)
    std::source_location sloc_;       // Source location for error reporting (借鉴 cash)
    std::vector<lnodeimpl*> srcs_;    // List of source nodes this node depends on (借鉴 cash)
    // mutable size_t hash_;          // Cache hash value (借鉴 cash, optional for now)
    // lnodeimpl* prev_, *next_;      // Linked list pointers (借鉴 cash, for context lists, managed by context)
    // lnode* users_;                 // Reverse references (借鉴 cash, for optimization, managed by context/users)
};

// --- Helper to print lnodeimpl* ---
inline std::ostream& operator<<(std::ostream& os, const lnodeimpl* node) {
    if (node) {
        node->print(os);
    } else {
        os << "nullptr";
    }
    return os;
}

// --- Helper for clone_map (借鉴 cash) ---
using clone_map = std::unordered_map<uint32_t, lnodeimpl*>; // Defined elsewhere in cash, included here for completeness if needed in lnodeimpl methods

}} // namespace ch::core

#endif // LNODEIMPL_H
