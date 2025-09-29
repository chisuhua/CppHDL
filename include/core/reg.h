#ifndef REG_H
#define REG_H

#include "logic.h" // For lnode, get_lnode, lnodeimpl - Now in ch::core
#include "core/context.h" // For ctx_curr_
#include "lnodeimpl.h" // For regimpl, proxyimpl, lnodetype - Now in ch::core
#include "bitbase.h" // For ch_uint, ch_width_v, logic_buffer
#include <string>
#include <typeinfo>
#include <memory> // For std::unique_ptr
#include <source_location> // C++20
#include <iostream>

namespace ch { namespace core {


// Forward declaration of the internal implementation
template<typename T>
class ch_reg_impl;

// Public alias for the user-facing register type
template<typename T>
using ch_reg = const ch_reg_impl<T>;

// --- Helper class for next assignment operator (as per cash) ---
// This class is the *target* of the assignment when using reg->next.
template<typename T>
struct next_assignment_proxy {
    lnodeimpl* regimpl_node_;

    next_assignment_proxy(lnodeimpl* impl) : regimpl_node_(impl) {}

    // --- ADD const QUALIFIER ---
    template<typename U>
    void operator=(const U& value) const { // Marked as const
        lnode<U> src_lnode = get_lnode(value);
        // --- DEBUG PRINT ---
        std::cout << "[next_assignment_proxy::operator=] Assigning value (node ID: " << (src_lnode.impl() ? src_lnode.impl()->id() : -1) << ") to regimpl node ID " << (regimpl_node_ ? regimpl_node_->id() : -1) << std::endl;
        // --- END DEBUG PRINT ---
        if (regimpl_node_ && src_lnode.impl() && regimpl_node_->type() == lnodetype::type_reg) {
            regimpl* reg_node_impl = static_cast<regimpl*>(regimpl_node_);
            reg_node_impl->set_next(src_lnode.impl()); // Connect RHS to reg's next input
        } else {
             std::cerr << "[next_assignment_proxy::operator=] Error: regimpl_node_ is null, not a regimpl, or src_lnode is null!" << std::endl;
        }
    }
};

// The ch_reg_impl::operator-> returns a pointer to this struct.
// Then, reg->next accesses the 'next' member variable of this struct.
template<typename T>
struct next_proxy {
    // This *is* the 'next' member accessed by reg->next
    next_assignment_proxy<T> next;

    next_proxy(lnodeimpl* impl) : next(impl) {}
};

// Inherits from T (likely a logic_buffer or similar base type derived from logic_buffer<ch_uint<N>>)
// This allows reg to be used directly where T is expected (e.g., in expressions like reg + 1)
// The *current value* of the reg is represented by its base class (logic_buffer), which holds a proxyimpl node.
template<typename T>
class ch_reg_impl : public T {
public:
    using value_type = T;
    using next_type = next_proxy<T>;

    // Constructor: Creates the register node and its proxy in the current context
    // Accepts an initial value of any type U (e.g., literal int '2', another ch_uint, etc.)
    // Uses get_lnode to handle the initial value.
    template<typename U>
    ch_reg_impl(const U& initial_value) {
        std::cout << "  [ch_reg] Creating register with initial value (placeholder)" << std::endl;

        auto ctx = ch::core::ctx_curr_;
        if (!ctx) {
            std::cerr << "[ch_reg] Error: No active context!" << std::endl;
            return;
        }

        // 1. Get the lnode for the initial value (could be a literal like 2, or another node)
        lnode<U> init_lnode = get_lnode(initial_value); // This should call get_lnode(LiteralType) for '2', or get_lnode(const T& t) requires { t.impl(); } for nodes

        // 2. Create the register node (and its next proxy) using the initial value lnode
        // This calls the version of createRegNode that accepts an initial value lnode

        lnodeimpl* reg_proxy_node = createRegNodeImpl<T>(init_lnode, "reg", std::source_location::current()); // Now calls the T-specific version from logic.h

        // 3. Initialize the base logic_buffer part of ch_reg_impl with the regimpl's *proxyimpl* node
        // The regimpl constructor returns the proxyimpl node representing the register's *current* value.
        // DESIGN_phase2.md: proxyimpl is returned by createRegNode and represents current value.
        // The base class T (which inherits from logic_buffer<T>) needs its node_impl_ set.
        // Since ch_reg_impl inherits from T, and T inherits from logic_buffer<T>,
        // the 'this' pointer is of type ch_reg_impl<T>*, which is-a logic_buffer<T>*.
        // We need to set the node_impl_ member of the logic_buffer<T> base.
        // The object 'this' *is* a logic_buffer<T> via inheritance (ch_reg_impl<T> -> T -> logic_buffer<T>).
        // Therefore, static_cast<logic_buffer<T>*>(this) should work.
        static_cast<logic_buffer<T>*>(this)->node_impl_ = reg_proxy_node; // Set the base class's node pointer using T

        // 4. Create the next_proxy object, which holds the regimpl node pointer for the 'next' assignment
        // The proxyimpl node (reg_proxy_node) should have the regimpl node as its source (src(0)).
        // DESIGN_phase2.md: proxyimpl(reg, ...) means regimpl is the source of the proxyimpl.
        if (reg_proxy_node && reg_proxy_node->num_srcs() > 0 && reg_proxy_node->src(0) && reg_proxy_node->src(0)->type() == lnodetype::type_reg) {
             regimpl_node_ = reg_proxy_node->src(0); // Get regimpl node (R) from proxyimpl's source
        } else {
             std::cerr << "[ch_reg_impl] Error: Could not get regimpl node from proxyimpl source!" << std::endl;
             regimpl_node_ = nullptr;
        }
        __next__ = std::make_unique<next_type>(regimpl_node_); // Pass the regimpl node to next_proxy constructor

        std::cout << "  [ch_reg] Created regimpl and next_proxy." << std::endl;
    }

    // Constructor without initial value
    ch_reg_impl() {
         std::cout << "  [ch_reg] Creating register without initial value." << std::endl;

         auto ctx = ch::core::ctx_curr_;
         if (!ctx) {
             std::cerr << "[ch_reg] Error: No active context!" << std::endl;
             return;
         }

         constexpr unsigned size = ch_width_v<T>; // Calculate size based on template param T
         std::cout << "[ch_reg_impl::ctor (no init)] Type T is: " << typeid(T).name() << std::endl; // Requires #include <typeinfo>
         std::cout << "[ch_reg_impl::ctor (no init)] Calculated register size (ch_width_v<T>): " << size << std::endl;

         lnodeimpl* reg_proxy_node = createRegNodeImpl<T>(size, "reg", std::source_location::current());
         static_cast<logic_buffer<T>*>(this)->node_impl_ = reg_proxy_node; // Set base class node using T
         if (reg_proxy_node && reg_proxy_node->num_srcs() > 0 && reg_proxy_node->src(0) && reg_proxy_node->src(0)->type() == lnodetype::type_reg) {
              regimpl_node_ = reg_proxy_node->src(0);
         } else {
              std::cerr << "[ch_reg_impl] Error: Could not get regimpl node from proxyimpl source (no init)!" << std::endl;
              regimpl_node_ = nullptr;
         }
         __next__ = std::make_unique<next_type>(regimpl_node_);

         std::cout << "  [ch_reg] Created regimpl (no init) and next_proxy." << std::endl;
    }

    // --- Operator-> is crucial for accessing next_proxy ---
    // This returns a pointer to the next_proxy object, which contains the 'next' member.
    // reg->next then accesses the 'next' member variable of that object.
    const next_type* operator->() const { return __next__.get(); } // Allows reg->next = ...

    // Optional: Operator* to get the current value (for simulation access, not IR building)
    // T operator*() const { /* Read from simulation buffer using node_impl_ */ return T(); } // Implementation depends on simulation buffer access

private:
    std::unique_ptr<next_type> __next__; // Holds the next proxy object (contains the 'next' member)
    lnodeimpl* regimpl_node_ = nullptr;  // Holds the regimpl node pointer for next assignment

};

template <typename T>
struct ch_width_impl<ch_reg_impl<T>, void> { // Specialize for ch_reg_impl<T>
    static constexpr unsigned value = ch_width_v<T>; // Delegate to the underlying type T
    // Example: ch_width_v<ch_reg_impl<ch_uint<4>>> -> ch_width_v<ch_uint<4>> -> 4
};

template<typename T, typename U> // U is the type of the initial value (e.g., int for literal 2)
lnodeimpl* createRegNodeImpl(const lnode<U>& init, const std::string& name, const std::source_location& sloc) {
    auto ctx = ctx_curr_; // Assuming ctx_curr_ is accessible here, likely via context.h
    if (!ctx) {
        std::cerr << "[createRegNodeImpl (with init)] Error: No active context!" << std::endl;
        return nullptr;
    }

    // Get the lnodeimpl* for the initial value node (e.g., a litimpl created by get_lnode(2))
    lnodeimpl* init_node_impl = init.impl();
    if (!init_node_impl) {
         std::cerr << "[createRegNodeImpl (with init)] Error: Initial value lnode is invalid!" << std::endl;
         return nullptr;
    }

    // --- STEP 1: Create the regimpl node ---
    constexpr unsigned size = ch_width_v<T>; // Use ch_width_v of the register type T
    std::cout << "[createRegNodeImpl (with init, T)] Calculated register size (ch_width_v<T>): " << size << " for type T" << std::endl; // Debug print
    //
    // --- END KEY CHANGE ---
    uint32_t cd = 0; // Placeholder clock domain
    lnodeimpl* rst = nullptr; // Placeholder reset signal
    lnodeimpl* clk_en = nullptr; // Placeholder clock enable
    lnodeimpl* rst_val = nullptr; // Placeholder reset value
    lnodeimpl* next = nullptr; // Next value source (set later)
    lnodeimpl* init_val = init_node_impl; // Initial value source

    // Create the regimpl node using the context's factory.
    // Assuming regimpl constructor matches these arguments plus context* at the end.
    regimpl* reg_node = ctx->create_node<regimpl>(
        size, cd, rst, clk_en, rst_val, next, init_val,
        name, sloc
    );
    std::cout << "  [createRegNodeImpl (with init)] Created regimpl node '" << name << "'" << std::endl;

    // --- STEP 2: Create the proxyimpl node ---
    proxyimpl* proxy_node = ctx->create_node<proxyimpl>(reg_node, name, sloc);
    std::cout << "  [createRegNodeImpl (with init)] Created proxyimpl node '" << name << "'" << std::endl;

    // Return the proxyimpl node, as this is what represents the register's value in expressions.
    return proxy_node;
}

// --- Implementation for createRegNodeImpl (Overload 2: Without initial value, just size) ---
template<typename U> // U is a dummy here, just to match the signature pattern if needed, or remove template if not generic
lnodeimpl* createRegNodeImpl(unsigned size, const std::string& name, const std::source_location& sloc) {
    auto ctx = ctx_curr_;
    if (!ctx) {
        std::cerr << "[createRegNodeImpl (no init)] Error: No active context!" << std::endl;
        return nullptr;
    }


    std::cout << "[createRegNodeImpl (no init)] Received size argument: " << size << std::endl;

    // Placeholders for clock domain, reset, etc.
    uint32_t cd = 0; // Placeholder clock domain
    lnodeimpl* rst = nullptr; // Placeholder reset signal
    lnodeimpl* clk_en = nullptr; // Placeholder clock enable
    lnodeimpl* rst_val = nullptr; // Placeholder reset value
    lnodeimpl* next = nullptr; // Next value source (set later)
    lnodeimpl* init_val = nullptr; // No initial value node

    // Create the regimpl node (without initial value node)
    regimpl* reg_node = ctx->create_node<regimpl>(
        size, cd, rst, clk_en, rst_val, next, init_val,
        name, sloc
    );
    std::cout << "  [createRegNodeImpl (no init)] Created regimpl node '" << name << "'" << std::endl;

    // --- STEP 2: Create the proxyimpl node ---
    proxyimpl* proxy_node = ctx->create_node<proxyimpl>(reg_node, name, sloc);
    std::cout << "  [createRegNodeImpl (no init)] Created proxyimpl node '" << name << "'" << std::endl;

    // Return the proxyimpl node.
    return proxy_node;
}



}} // namespace ch::core

#endif // REG_H
