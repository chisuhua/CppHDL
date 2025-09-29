#ifndef BITBASE_H
#define BITBASE_H

#include "logic.h" // For lnode, get_lnode, lnodeimpl, ch_width_v, ch_op
#include "lnodeimpl.h" // For opimpl, lnodeimpl, ch_op
#include "core/context.h" // For ctx_curr_
#include "traits.h" // For ctx_curr_
#include <source_location> // C++20
#include <type_traits> // For std::max
#include <algorithm> // For std::max if needed
#include <cstdint>

namespace ch { namespace core {

// --- Base class for logic types (simplified logic_buffer) ---
template<typename T>
struct logic_buffer {
    lnodeimpl* impl() const { return node_impl_; }
    logic_buffer(lnodeimpl* node) : node_impl_(node) {}
    logic_buffer() : node_impl_(nullptr) {}
    lnodeimpl* node_impl_ = nullptr;
};

// --- ch_uint<N> type (Modified) ---
template<unsigned N>
struct ch_uint : public logic_buffer<ch_uint<N>> {
    static constexpr unsigned width = N;
    static constexpr unsigned ch_width = N;
    // Inherit constructors from base
    using logic_buffer<ch_uint<N>>::logic_buffer;

    // Constructor from a literal value (for simulation, not IR building)
    // ch_uint(uint64_t val) : logic_buffer<ch_uint<N>>(/* create lit node for val * /) {} // Not for describe phase
};

// --- Specialization for ch_width_v for ch_uint<N> ---
template<unsigned N>
constexpr unsigned ch_width_v<ch_uint<N>> = N;

template<unsigned N>
struct ch_width_impl<ch_uint<N>, void> {
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

// --- Concept to check if get_lnode is applicable and returns an lnode<T> ---
// This concept checks if calling get_lnode on type U returns a type that is convertible to lnode<V> for some V.
template<typename U>
concept CppHDLCapable = requires(U u) {
    { get_lnode(u) } -> std::convertible_to<lnode<U>>; // Or potentially lnode<SomeType>, not necessarily U itself
    // A more precise check might involve checking the return type more specifically,
    // but std::convertible_to<lnode<U>> is a good proxy if lnode<T> implicitly converts from lnode<T>.
    // The original requires-clause in get_lnode itself (lnode<T> get_lnode(const T& t) requires requires { t.impl(); })
    // already ensures T has an impl() method.
    // The arithmetic overload requires std::is_arithmetic_v<LiteralType>.
    // So, CppHDLCapable<T> will be true if T has impl() or is arithmetic.
    // This should correctly identify ch_uint, ch_reg, ch_logic_in/out, port, and literals.
};

// --- Operator Implementations ---
// --- Addition ---
template<typename T, typename U>
requires CppHDLCapable<std::remove_cvref_t<T>> && CppHDLCapable<std::remove_cvref_t<U>>
auto operator+(const T& lhs, const U& rhs) {
    std::cout << "  [operator+] Called." << std::endl;

    lnode<T> lhs_lnode = get_lnode(lhs);
    lnode<U> rhs_lnode = get_lnode(rhs);

    if (!lhs_lnode.impl() || !rhs_lnode.impl()) {
        std::cerr << "[operator+] Error: One or both operands are not valid lnode types!" << std::endl;
        std::abort();
    }


    // Calculate result width: max operand width + 1 for potential carry
    // std::max is constexpr if its arguments are constexpr
    constexpr unsigned lhs_width = ch_width_v<std::remove_cvref_t<T>>;
    constexpr unsigned rhs_width = ch_width_v<std::remove_cvref_t<U>>;
    constexpr unsigned result_width = std::max(lhs_width, rhs_width) + 1; // Ensure at least 32 bits, or max+1
    
    lnodeimpl* op_node = createOpNodeImpl(ch_op::add, result_width, /* is_signed */ false,
                                          lhs_lnode, rhs_lnode,
                                          "add_op", std::source_location::current());

    return ch_uint<result_width>(op_node);
}

// --- Subtraction ---
template <typename T, typename U >
requires CppHDLCapable<std::remove_cvref_t<T>> && CppHDLCapable<std::remove_cvref_t<U>>
auto operator-(const T & lhs, const U & rhs) {
    lnode<T> lhs_lnode = get_lnode(lhs);
    lnode<U> rhs_lnode = get_lnode(rhs);

    if (!lhs_lnode.impl() || !rhs_lnode.impl()) {
        std::cerr  <<  "[operator-] Error: One or both operands are not valid lnode types! "  << std::endl;
        std::abort();
    }

    // Calculate result width: max operand width (subtraction doesn't typically increase width beyond operands)
    constexpr unsigned lhs_width = ch_width_v<std::remove_cvref_t <T>>;
    constexpr unsigned rhs_width = ch_width_v<std::remove_cvref_t <U>>;
    constexpr unsigned result_width = std::max(lhs_width, rhs_width);

    lnodeimpl* op_node = createOpNodeImpl(ch_op::sub, result_width, /* is_signed */ false,
                                          lhs_lnode, rhs_lnode,
                                          "sub_op ", std::source_location::current());

    return ch_uint<result_width >(op_node);
}

// --- Multiplication ---
template <typename T, typename U >
requires CppHDLCapable<std::remove_cvref_t<T>> && CppHDLCapable<std::remove_cvref_t<U>>
auto operator*(const T & lhs, const U & rhs) {
    lnode<T> lhs_lnode = get_lnode(lhs);
    lnode<U> rhs_lnode = get_lnode(rhs);

    if (!lhs_lnode.impl() || !rhs_lnode.impl()) {
        std::cerr  <<  "[operator*] Error: One or both operands are not valid lnode types! "  << std::endl;
        std::abort();
    }

    // Calculate result width: sum of operand widths
    constexpr unsigned lhs_width = ch_width_v<std::remove_cvref_t<T>>;
    constexpr unsigned rhs_width = ch_width_v<std::remove_cvref_t<U>>;
    constexpr unsigned result_width = lhs_width + rhs_width;

    lnodeimpl* op_node = createOpNodeImpl(ch_op::mul, result_width, /* is_signed */ false,
                                          lhs_lnode, rhs_lnode,
                                          "mul_op ", std::source_location::current());

    return ch_uint<result_width >(op_node);
}

// --- Bitwise AND ---
template <typename T, typename U >
requires CppHDLCapable<std::remove_cvref_t<T>> && CppHDLCapable<std::remove_cvref_t<U>>
auto operator&(const T & lhs, const U & rhs) {
    lnode<T> lhs_lnode = get_lnode(lhs);
    lnode<U> rhs_lnode = get_lnode(rhs);

    if (!lhs_lnode.impl() || !rhs_lnode.impl()) {
        std::cerr  <<  "[operator&] Error: One or both operands are not valid lnode types! "  << std::endl;
        std::abort();
    }

    // Calculate result width: max operand width
    constexpr unsigned lhs_width = ch_width_v<std::remove_cvref_t<T>>;
    constexpr unsigned rhs_width = ch_width_v<std::remove_cvref_t<U>>;
    constexpr unsigned result_width = std::max(lhs_width, rhs_width);

    lnodeimpl* op_node = createOpNodeImpl(ch_op::and_, result_width, /* is_signed */ false,
                                          lhs_lnode, rhs_lnode,
                                          "and_op ", std::source_location::current());

    return ch_uint<result_width >(op_node);
}

// --- Bitwise OR ---
template <typename T, typename U >
requires CppHDLCapable<std::remove_cvref_t<T>> && CppHDLCapable<std::remove_cvref_t<U>>
auto operator|(const T & lhs, const U & rhs) {
    lnode<T> lhs_lnode = get_lnode(lhs);
    lnode<U> rhs_lnode = get_lnode(rhs);

    if (!lhs_lnode.impl() || !rhs_lnode.impl()) {
        std::cerr  <<  "[operator|] Error: One or both operands are not valid lnode types! "  << std::endl;
        std::abort();
    }

    // Calculate result width: max operand width
    constexpr unsigned lhs_width = ch_width_v <std::remove_cvref_t <T > >;
    constexpr unsigned rhs_width = ch_width_v <std::remove_cvref_t <U > >;
    constexpr unsigned result_width = std::max(lhs_width, rhs_width);

    lnodeimpl* op_node = createOpNodeImpl(ch_op::or_, result_width, /* is_signed */ false,
                                          lhs_lnode, rhs_lnode,
                                          "or_op ", std::source_location::current());

    return ch_uint<result_width >(op_node);
}

// --- Bitwise XOR ---
template <typename T, typename U >
requires CppHDLCapable<std::remove_cvref_t<T>> && CppHDLCapable<std::remove_cvref_t<U>>
auto operator^(const T & lhs, const U & rhs) {
    lnode<T> lhs_lnode = get_lnode(lhs);
    lnode<U> rhs_lnode = get_lnode(rhs);

    if (!lhs_lnode.impl() || !rhs_lnode.impl()) {
        std::cerr  <<  "[operator^] Error: One or both operands are not valid lnode types! "  << std::endl;
        std::abort();
    }

    // Calculate result width: max operand width
    constexpr unsigned lhs_width = ch_width_v<std::remove_cvref_t<T>>;
    constexpr unsigned rhs_width = ch_width_v<std::remove_cvref_t<U>>;
    constexpr unsigned result_width = std::max(lhs_width, rhs_width);

    lnodeimpl* op_node = createOpNodeImpl(ch_op::xor_, result_width, /* is_signed */ false,
                                          lhs_lnode, rhs_lnode,
                                          "xor_op ", std::source_location::current());

    return ch_uint<result_width>(op_node);
}

// --- Unary Bitwise NOT ---
template <typename T >
auto operator~(const T & operand) {
    lnode<T> op_lnode = get_lnode(operand);

    if (!op_lnode.impl()) {
        std::cerr  <<  "[operator~] Error: Operand is not a valid lnode type! "  << std::endl;
        std::abort();
    }

    // Calculate result width: same as operand width
    constexpr unsigned op_width = ch_width_v<std::remove_cvref_t<T>>;
    constexpr unsigned result_width = op_width;

    // Note: For unary operators, we pass the operand as both LHS and RHS to createOpNodeImpl
    // The simulator/CodeGen will need to handle this correctly based on the op type.
    lnodeimpl* op_node = createOpNodeImpl(ch_op::not_, result_width, /* is_signed */ false,
                                          op_lnode, op_lnode, // Pass operand twice for unary
                                          "not_op ", std::source_location::current());

    return ch_uint<result_width>(op_node);
}

// --- Equality Comparison ---
template <typename T, typename U >
requires CppHDLCapable<std::remove_cvref_t<T>> && CppHDLCapable<std::remove_cvref_t<U>>
auto operator==(const T & lhs, const U & rhs) {
    lnode<T> lhs_lnode = get_lnode(lhs);
    lnode<U> rhs_lnode = get_lnode(rhs);

    if (!lhs_lnode.impl() || !rhs_lnode.impl()) {
        std::cerr  <<  "[operator==] Error: One or both operands are not valid lnode types! "  << std::endl;
        std::abort();
    }

    // Calculate result width: always 1 for boolean comparison
    constexpr unsigned result_width = 1;

    lnodeimpl* op_node = createOpNodeImpl(ch_op::eq, result_width, /* is_signed */ false,
                                          lhs_lnode, rhs_lnode,
                                          "eq_op ", std::source_location::current());

    return ch_uint<result_width>(op_node); // Represents boolean true/false
}

// --- Inequality Comparison ---
template <typename T, typename U>
requires CppHDLCapable<std::remove_cvref_t<T>> && CppHDLCapable<std::remove_cvref_t<U>>
auto operator!=(const T & lhs, const U & rhs) {
    lnode<T> lhs_lnode = get_lnode(lhs);
    lnode<U> rhs_lnode = get_lnode(rhs);

    if (!lhs_lnode.impl() || !rhs_lnode.impl()) {
        std::cerr  <<  "[operator!=] Error: One or both operands are not valid lnode types! "  << std::endl;
        std::abort();
    }

    // Calculate result width: always 1 for boolean comparison
    constexpr unsigned result_width = 1;

    lnodeimpl* op_node = createOpNodeImpl(ch_op::ne, result_width, /* is_signed */ false,
                                          lhs_lnode, rhs_lnode,
                                          "ne_op ", std::source_location::current());

    return ch_uint<result_width>(op_node); // Represents boolean true/false
}

// --- Less Than Comparison ---
template <typename T, typename U >
auto operator<(const T & lhs, const U & rhs) {
    lnode<T> lhs_lnode = get_lnode(lhs);
    lnode<U> rhs_lnode = get_lnode(rhs);

    if (!lhs_lnode.impl() || !rhs_lnode.impl()) {
        std::cerr  <<  "[operator<] Error: One or both operands are not valid lnode types! "  << std::endl;
        std::abort();
    }

    // Calculate result width: always 1 for boolean comparison
    constexpr unsigned result_width = 1;

    lnodeimpl* op_node = createOpNodeImpl(ch_op::lt, result_width, /* is_signed */ false, // Note: is_signed might be relevant for signed comparisons later
                                          lhs_lnode, rhs_lnode,
                                          "lt_op ", std::source_location::current());

    return ch_uint <result_width >(op_node); // Represents boolean true/false
}

// --- Less Than or Equal Comparison ---
template <typename T, typename U >
auto operator<=(const T & lhs, const U & rhs) {
    lnode<T> lhs_lnode = get_lnode(lhs);
    lnode<U> rhs_lnode = get_lnode(rhs);

    if (!lhs_lnode.impl() || !rhs_lnode.impl()) {
        std::cerr  <<  "[operator<=] Error: One or both operands are not valid lnode types! "  << std::endl;
        std::abort();
    }

    // Calculate result width: always 1 for boolean comparison
    constexpr unsigned result_width = 1;

    lnodeimpl* op_node = createOpNodeImpl(ch_op::le, result_width, /* is_signed */ false,
                                          lhs_lnode, rhs_lnode,
                                          "le_op ", std::source_location::current());

    return ch_uint <result_width >(op_node); // Represents boolean true/false
}

// --- Greater Than Comparison ---
template <typename T, typename U >
auto operator>(const T & lhs, const U & rhs) {
    lnode<T> lhs_lnode = get_lnode(lhs);
    lnode<U> rhs_lnode = get_lnode(rhs);

    if (!lhs_lnode.impl() || !rhs_lnode.impl()) {
        std::cerr  <<  "[operator>] Error: One or both operands are not valid lnode types! "  << std::endl;
        std::abort();
    }

    // Calculate result width: always 1 for boolean comparison
    constexpr unsigned result_width = 1;

    lnodeimpl* op_node = createOpNodeImpl(ch_op::gt, result_width, /* is_signed */ false,
                                          lhs_lnode, rhs_lnode,
                                          "gt_op ", std::source_location::current());

    return ch_uint <result_width >(op_node); // Represents boolean true/false
}

// --- Greater Than or Equal Comparison ---
template <typename T, typename U >
auto operator>=(const T & lhs, const U & rhs) {
    lnode<T> lhs_lnode = get_lnode(lhs);
    lnode<U> rhs_lnode = get_lnode(rhs);

    if (!lhs_lnode.impl() || !rhs_lnode.impl()) {
        std::cerr  <<  "[operator>=] Error: One or both operands are not valid lnode types! "  << std::endl;
        std::abort();
    }

    // Calculate result width: always 1 for boolean comparison
    constexpr unsigned result_width = 1;

    lnodeimpl* op_node = createOpNodeImpl(ch_op::ge, result_width, /* is_signed */ false,
                                          lhs_lnode, rhs_lnode,
                                          "ge_op ", std::source_location::current());

    return ch_uint <result_width >(op_node); // Represents boolean true/false
}

// ... (rest of the file remains the same, including createOpNodeImpl and ch_width_v specializations) ...

}} // namespace ch::core

#endif // BITBASE_H
