#ifndef IO_H
#define IO_H

#include "logic.h" // For lnode, get_lnode - Now in ch::core
#include "core/context.h" // For ctx_curr_, context::create_output - This now declares create_output
#include "lnodeimpl.h" // For inputimpl, outputimpl - Now in ch::core
#include <source_location> // C++20

namespace ch { namespace core {

// --- Macro to define I/O ports ---
// Simplified macro for now, expecting expanded type name
#define __io(ports) struct { ports; } io; // Standard definition, requires expanded type name in counter.cpp

// --- ch_logic_out: Output Port (Modified) ---
// This class represents an output port. It should hold the outputimpl node
// and provide an assignment operator to connect other nodes to it.
// It doesn't necessarily need to inherit from T (e.g., ch_uint<N>) like ch_reg does.
template<typename T>
class ch_logic_out {
public:
    using value_type = T;
    static constexpr bool has_ch_width_trait = true; // <<--- New marker for ch_width_v

    ch_logic_out(const std::string& name = "io", const std::source_location& sloc = std::source_location::current()) : name_(name) {
        auto ctx = ch::core::ctx_curr_;
        if (!ctx) {
            std::cerr << "[ch_logic_out] Error: No active context for output '" << name << "'!" << std::endl;
            return;
        }
        // Create the outputimpl node using the context's factory method
        // This call internally creates the source proxyimpl as well.
        // Cast the return type to the expected type within ch::core namespace
        outputimpl* node_ptr = ctx->create_output(ch_width_v<T>, name, sloc); // Now ctx has create_output
        output_node_ = node_ptr; // Assign the correctly typed pointer
        std::cout << "  [ch_logic_out] Created outputimpl node for '" << name << "'" << std::endl;
    }

    // Assignment operator: io.port = value
    // This is the key method for connecting the output to another node during describe().
    template<typename U>
    void operator=(const U& value) {
        lnode<U> src_lnode = get_lnode(value); // Use get_lnode to get the source lnode wrapper
        if (output_node_ && src_lnode.impl()) {
            // The outputimpl node needs to be connected to the source.
            // Assuming outputimpl->add_src(0, src_lnode.impl()) or similar exists.
            output_node_->set_src(0, src_lnode.impl()); // Now outputimpl definition is needed for add_src
            std::cout << "  [ch_logic_out::operator=] Connected source to output '" << name_ << "'" << std::endl;
        } else {
            std::cerr << "[ch_logic_out::operator=] Error: output_node_ or src_lnode is null for '" << name_ << "'!" << std::endl;
        }
    }
    // Optional: Provide impl() if ch_logic_out itself needs to be used in expressions (less common for outputs)
    // lnodeimpl* impl() const { return output_node_; }

private:
    std::string name_;
    outputimpl* output_node_ = nullptr; // This requires outputimpl definition, which is in ast_nodes.h if included before io.h
};

// --- ch_logic_in: Input Port (Example) ---
template<typename T>
class ch_logic_in {
public:
    using value_type = T; // Example value type
    static constexpr bool has_ch_width_trait = true; // <<--- New marker for ch_width_v

    ch_logic_in(const std::string& name = "io", const std::source_location& sloc = std::source_location::current()) : name_(name) {
         auto ctx = ch::core::ctx_curr_;
         if (ctx) {
             // input_node_ = ctx->create_input(ch_width_v<T>, name, sloc);
             // proxy_node_ = /* Get proxy from input_node_ or create one */ nullptr; // Placeholder
             std::cout << "  [ch_logic_in] Created inputimpl node for '" << name << "'" << std::endl; // Placeholder
         }
    }
    // Operator to get the value (returns lnode for use in expressions)
    // lnode<T> operator()() const { // Could be operator* or operator() depending on style
    //     return lnode<T>(proxy_node_); // Or input_node_ if it acts as proxy
    // }
    // get_lnode support
    // lnodeimpl* impl() const { return proxy_node_; } // Or input_node_
private:
    std::string name_;
    // inputimpl* input_node_ = nullptr;
    // lnodeimpl* proxy_node_ = nullptr; // Placeholder
};

template <typename T>
struct ch_width_impl<ch_logic_out<T>, void> { // Specialize for ch_logic_out<T>
    static constexpr unsigned value = ch_width_v<T>; // Delegate to the underlying type T
    // Example: ch_width_v<ch_logic_out<ch_uint<4>>> -> ch_width_v<ch_uint<4>> -> 4
};

template <typename T>
struct ch_width_impl<ch_logic_in<T>, void> { // Specialize for ch_logic_in<T>
    static constexpr unsigned value = ch_width_v<T>; // Delegate to the underlying type T
    // Example: ch_width_v<ch_logic_in<ch_uint<4>>> -> ch_width_v<ch_uint<4>> -> 4
};

}} // namespace ch::core

#endif // IO_H
