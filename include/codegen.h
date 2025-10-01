// include/codegen.h
#ifndef CODEGEN_H
#define CODEGEN_H

#include "core/context.h" // Include context definition to access the IR
#include <string>         // For filename string
#include <fstream>        // For file stream operations
#include <iostream>       // For standard output/error
#include <sstream>        // For string stream operations
#include <unordered_map>  // For mapping node IDs to names, uses
#include <unordered_set>  // For tracking visited nodes, dependencies
#include <vector>         // For storing node lists
#include <algorithm>      // For sorting, find_if
#include <cctype>         // For character checks (isdigit)
#include <functional>     // For std::hash
#include <memory>         // For unique_ptr if needed

// Forward declarations for node types used by the writer
namespace ch { namespace core {
    class lnodeimpl;
    class litimpl;
    class inputimpl;
    class outputimpl;
    class regimpl;
    class opimpl;
    class proxyimpl;
    enum class ch_op; // Assuming ch_op is defined in lnodeimpl.h or a related header
}}

namespace ch {
    class sdata_type;

// Main function to generate Verilog code from the CppHDL IR context.
// Uses the verilogwriter class internally.
void toVerilog(const std::string& filename, ch::core::context* ctx);

// Class responsible for generating Verilog code from the IR graph.
class verilogwriter {
public:
    explicit verilogwriter(ch::core::context* ctx);

    // Generates the complete Verilog module and writes it to the given stream.
    void print(std::ostream& out);

private:
    // --- Helper functions for name generation and validation ---
    std::string sanitize_name(const std::string& name) const;
    std::string get_width_str(uint32_t size) const;
    std::string get_literal_str(const sdata_type& val) const;

    // --- Helper function to print operators ---
    std::string get_op_str(ch::core::ch_op op) const;

    // --- Core printing methods ---
    void print_header(std::ostream& out);
    void print_body(std::ostream& out);
    void print_footer(std::ostream& out);

    // --- Declaration printing (wires, regs, assigns for literals) ---
    void print_decl(std::ostream& out);

    // --- Logic printing (assign statements, always blocks) ---
    void print_logic(std::ostream& out);

    // --- Specific node type printers ---
    void print_input(std::ostream& out, ch::core::inputimpl* node);
    void print_output(std::ostream& out, ch::core::outputimpl* node);
    void print_reg(std::ostream& out, ch::core::regimpl* node);
    void print_op(std::ostream& out, ch::core::opimpl* node);
    void print_proxy(std::ostream& out, ch::core::proxyimpl* node);

    // --- Data members ---
    ch::core::context* ctx_;
    // Map to store the generated Verilog name for each node
    std::unordered_map<ch::core::lnodeimpl*, std::string> node_names_;
    // Map to store which nodes *use* a given node (reverse dependencies)
    std::unordered_map<ch::core::lnodeimpl*, std::unordered_set<ch::core::lnodeimpl*>> node_uses_;
    // Set to track nodes already declared/printed to avoid duplicates
    std::unordered_set<ch::core::lnodeimpl*> declared_nodes_;
    // Set to track nodes already printed in the logic section to avoid duplicates
    std::unordered_set<ch::core::lnodeimpl*> printed_logic_nodes_;
    // Topologically sorted list of nodes (excluding inputs/outputs/literals)
    std::vector<ch::core::lnodeimpl*> sorted_nodes_;
};

} // namespace ch

#endif // CODEGEN_H
