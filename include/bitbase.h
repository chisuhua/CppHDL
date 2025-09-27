#ifndef BITBASE_H
#define BITBASE_H

#include "logic.h" // For lnode, get_lnode, lnodeimpl, ch_width_v, ch_op
#include "lnodeimpl.h" // For opimpl, lnodeimpl, ch_op
#include "core/context.h" // For ctx_curr_
#include "traits.h" // For ctx_curr_
#include <source_location> // C++20
#include <type_traits> // For std::max
#include <algorithm> // For std::max if needed

namespace ch { namespace core {

// --- Base class for logic types (simplified logic_buffer) ---
template<typename T>
struct logic_buffer {
    lnodeimpl* impl() const { return node_impl_; }
    // Constructor from lnodeimpl* (for use when created from an expression result or node creation)
    logic_buffer(lnodeimpl* node) : node_impl_(node) {}
    // Default constructor (for declaring ports/io, node_impl_ is nullptr initially)
    logic_buffer() : node_impl_(nullptr) {}
    lnodeimpl* node_impl_ = nullptr;
};

// --- ch_uint<N> type (Modified) ---
template<unsigned N>
struct ch_uint : public logic_buffer<ch_uint<N>> { // Inherit from logic_buffer
    static constexpr unsigned width = N;
    static constexpr unsigned ch_width = N; // <<--- New marker for ch_width_v
    //
    // Inherit constructors from base
    using logic_buffer<ch_uint<N>>::logic_buffer;

    // Constructor from a literal value (for simulation, not IR building)
    // ch_uint(uint64_t val) : logic_buffer<ch_uint<N>>(/* create lit node for val * /) {} // Not for describe phase
};

// --- Specialization for ch_width_v for ch_uint<N> ---
template<unsigned N>
constexpr unsigned ch_width_v<ch_uint<N>> = N; // Specialize the template variable for ch_uint<N>

template<unsigned N>
struct ch_width_impl<ch_uint<N>, void> { // <<--- EXPLICIT specialization
    static constexpr unsigned value = N;
};

// --- Forward declaration for createOpNodeImpl (from logic.h) ---
template<typename T, typename U> // T is the type of LHS, U is the type of RHS
lnodeimpl* createOpNodeImpl(ch_op op, uint32_t size, bool is_signed,
                            const lnode<T>& lhs, const lnode<U>& rhs,
                            const std::string& name, const std::source_location& sloc) {
    auto ctx = ctx_curr_;
    if (!ctx) {
        std::cerr << "[createOpNodeImpl] Error: No active context!" << std::endl;
        return nullptr;
    }

    // Get the lnodeimpl* for the left and right hand side operand nodes
    lnodeimpl* lhs_node_impl = lhs.impl();
    lnodeimpl* rhs_node_impl = rhs.impl();

    if (!lhs_node_impl || !rhs_node_impl) {
         std::cerr << "[createOpNodeImpl] Error: One or both operand lnodes are invalid!" << std::endl;
         return nullptr;
    }

    // --- STEP 1: Create the opimpl node ---
    // Assuming opimpl constructor takes appropriate arguments.
    opimpl* op_node = ctx->create_node<opimpl>(
        size, op, is_signed, lhs_node_impl, rhs_node_impl,
        name, sloc
    );
    std::cout << "  [createOpNodeImpl] Created opimpl node '" << name << "' for operation " << static_cast<int>(op) << std::endl;

    // --- STEP 2: Create the proxyimpl node ---
    // The proxyimpl node represents the *result value* of the operation.
    proxyimpl* result_proxy = ctx->create_node<proxyimpl>(op_node, name, sloc);
    std::cout << "  [createOpNodeImpl] Created result proxyimpl node '" << name << "'" << std::endl;

    // Return the proxyimpl node representing the result.
    return result_proxy;
}

// include/bitbase.h (修改后的 operator+)
template<typename T, typename U>
auto operator+(const T& lhs, const U& rhs) {
    std::cout << "  [operator+] Called." << std::endl;

    lnode<T> lhs_lnode = get_lnode(lhs);
    lnode<U> rhs_lnode = get_lnode(rhs);

    if (!lhs_lnode.impl() || !rhs_lnode.impl()) {
        std::cerr << "[operator+] Error: One or both operands are not valid lnode types!" << std::endl;
        std::abort();
    }

    // --- 使用 ch_width_v 计算编译时结果宽度 ---
    // std::max is constexpr if its arguments are constexpr
    constexpr unsigned result_width = std::max(
        ch_width_v<std::remove_cvref_t<T>>, 
        ch_width_v<std::remove_cvref_t<U>>
    );

    //std::cout << "  [operator+] Compile-time result width: " << result_width_compile_time << std::endl;
    // --- 结束编译时宽度计算 ---

    // --- (可选) 使用运行时节点大小进行验证或创建 IR 节点 ---
    // While ch_width_v gives the *type's* width, the *node's* size might be set during creation.
    // It's good practice to ensure they match or use the node size for IR creation if it's authoritative.
    // For now, we'll use the compile-time width for IR node creation as well, assuming consistency.
    // If needed, we could assert: assert(lhs_lnode.impl()->size() == ch_width_v<T>); etc.
    //unsigned result_size_for_ir = result_width_compile_time; // Use compile-time width for IR node

    lnodeimpl* op_node = createOpNodeImpl(ch_op::add, result_width, /* is_signed */ false,
                                          lhs_lnode, rhs_lnode,
                                          "add_op", std::source_location::current());
    // --- 结束运行时 IR 节点创建 ---

    // --- 返回类型现在由编译时宽度决定 ---
    // The return type of 'auto' will be deduced as ch_uint<result_width_compile_time>
    return ch_uint<result_width>(op_node);
    // --- 结束返回 ---
}


}} // namespace ch::core

#endif // BITBASE_H
