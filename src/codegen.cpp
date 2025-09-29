// src/codegen.cpp
#include "codegen.h"
#include "ast_nodes.h" // Include specific node implementations
#include "lnodeimpl.h" // For ch_op enum, lnodeimpl base
#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <algorithm> // For std::replace, std::remove, std::sort
#include <unordered_map>
#include <unordered_set>
#include <cctype> // For std::isdigit
#include <cassert> // For assert

namespace ch {

// --- Main Function ---
void toVerilog(const std::string& filename, ch::core::context* ctx) {
    if (!ctx) {
        std::cerr << "[toVerilog] Error: Context is null!" << std::endl;
        return;
    }

    std::ofstream file(filename);
    if (!file.is_open()) {
        std::cerr << "[toVerilog] Failed to open " << filename << std::endl;
        return;
    }

    verilogwriter writer(ctx);
    writer.print(file);

    file.close();
    std::cout << "[toVerilog] Generated " << filename << std::endl;
}

// --- verilogwriter Class Implementation ---

verilogwriter::verilogwriter(ch::core::context* ctx) : ctx_(ctx) {
    // Build the 'uses' map: for each node, find which other nodes use it as a source.
    for (auto& node_ptr : ctx_->get_nodes()) {
        auto* node = node_ptr.get();
        for (uint32_t i = 0; i < node->num_srcs(); ++i) {
            auto* src = node->src(i);
            if (src) { // Ensure source is not null
                node_uses_[src].insert(node);
            }
        }
    }

    // Build the initial name map based on node names and ensure uniqueness.
    // Also build the sorted_nodes_ list.
    std::unordered_map<std::string, int> name_counts;
    auto eval_list = ctx_->get_eval_list(); // Use the topologically sorted list from context
    for (auto* node : eval_list) {
        std::string base_name = sanitize_name(node->name());
        std::string unique_name = base_name;
        int counter = name_counts[base_name]++;
        if (counter > 0) {
            unique_name = base_name + "_" + std::to_string(counter);
        }
        node_names_[node] = unique_name;
    }

    // Store the sorted list for later use
    sorted_nodes_ = eval_list;
}

void verilogwriter::print(std::ostream& out) {
    print_header(out);
    print_body(out);
    print_footer(out);
}

void verilogwriter::print_header(std::ostream& out) {
    out << "module top (\n";

    std::vector<ch::core::lnodeimpl*> ports;
    // Collect inputs and outputs from the context's node list
    for (auto& node_ptr : ctx_->get_nodes()) {
        auto* node = node_ptr.get();
        if (node->type() == ch::core::lnodetype::type_input) {
            ports.push_back(node);
        } else if (node->type() == ch::core::lnodetype::type_output) {
            ports.push_back(node);
        }
    }

    // Sort ports by their ID to maintain order
    std::sort(ports.begin(), ports.end(), [](ch::core::lnodeimpl* a, ch::core::lnodeimpl* b) {
        return a->id() < b->id();
    });

    for (size_t i = 0; i < ports.size(); ++i) {
        auto* port_node = ports[i];
        if (port_node->type() == ch::core::lnodetype::type_input) {
            print_input(out, static_cast<ch::core::inputimpl*>(port_node));
        } else if (port_node->type() == ch::core::lnodetype::type_output) {
            print_output(out, static_cast<ch::core::outputimpl*>(port_node));
        }
        if (i < ports.size() - 1) {
            out << ",\n";
        }
    }

    out << "\n);\n\n";
}

void verilogwriter::print_body(std::ostream& out) {
    print_decl(out);
    out << "\n";
    print_logic(out);
    out << "\n";
}

void verilogwriter::print_footer(std::ostream& out) {
    out << "endmodule\n";
}

void verilogwriter::print_decl(std::ostream& out) {
    // Declare inputs, outputs, wires, regs
    // Iterate through the sorted list, but handle types differently for declaration
    std::vector<ch::core::lnodeimpl*> inputs, outputs, wires, regs;

    for (auto* node : sorted_nodes_) {
        if (declared_nodes_.count(node)) continue; // Skip if already processed

        switch (node->type()) {
            case ch::core::lnodetype::type_input:
                inputs.push_back(node);
                declared_nodes_.insert(node);
                break;
            case ch::core::lnodetype::type_output:
                outputs.push_back(node);
                declared_nodes_.insert(node);
                break;
            case ch::core::lnodetype::type_reg:
                regs.push_back(node);
                declared_nodes_.insert(node);
                break;
            case ch::core::lnodetype::type_op:
            case ch::core::lnodetype::type_proxy: // Proxy nodes often require wires for intermediate connections
            case ch::core::lnodetype::type_lit:   // Literals might need wires if used multiple times or by complex nodes
                // For simplicity, declare a wire for ops and proxies.
                // A more advanced check would see if the node is used inline or requires a dedicated wire.
                // For now, assume they need a wire if they are sources to non-literal/proxy nodes.
                wires.push_back(node);
                declared_nodes_.insert(node);
                break;
            default:
                // Other types like cd, etc., might not need declarations here
                break;
        }
    }

    // Print inputs
    for (auto* node : inputs) {
        out << "    input " << get_width_str(node->size()) << " " << node_names_[node] << ";\n";
    }
    if (!inputs.empty()) out << "\n";

    // Print outputs
    for (auto* node : outputs) {
        out << "    output " << get_width_str(node->size()) << " " << node_names_[node] << ";\n";
    }
    if (!outputs.empty()) out << "\n";

    // Print wires
    for (auto* node : wires) {
        out << "    wire " << get_width_str(node->size()) << " " << node_names_[node] << ";\n";
    }
    if (!wires.empty()) out << "\n";

    // Print regs
    for (auto* node : regs) {
        out << "    reg " << get_width_str(node->size()) << " " << node_names_[node] << ";\n";
    }
    if (!regs.empty()) out << "\n";
}

void verilogwriter::print_logic(std::ostream& out) {
    // Iterate through the sorted list and print logic for each node type
    // This assumes the sorted list correctly orders combinational logic before sequential.
    for (auto* node : sorted_nodes_) {
        if (printed_logic_nodes_.count(node)) continue; // Skip if already processed

        switch (node->type()) {
            case ch::core::lnodetype::type_op:
                print_op(out, static_cast<ch::core::opimpl*>(node));
                printed_logic_nodes_.insert(node);
                break;
            case ch::core::lnodetype::type_proxy:
                print_proxy(out, static_cast<ch::core::proxyimpl*>(node));
                printed_logic_nodes_.insert(node);
                break;
            case ch::core::lnodetype::type_reg:
                print_reg(out, static_cast<ch::core::regimpl*>(node));
                printed_logic_nodes_.insert(node);
                break;
            case ch::core::lnodetype::type_output: // Handle output assignments
                // Outputs are typically assigned via an 'assign' statement from their source.
                // Find the source of the outputimpl and generate the assign.
                {
                    auto* output_node = static_cast<ch::core::outputimpl*>(node);
                    if (output_node->num_srcs() > 0) {
                        auto* src_node = output_node->src(0);
                        if (node_names_.count(src_node)) {
                            out << "    assign " << node_names_[node] << " = " << node_names_[src_node] << ";\n";
                        }
                    }
                }
                printed_logic_nodes_.insert(node);
                break;
            // Literals are handled inline within operations or assignments, no specific logic printing needed here.
            default:
                break;
        }
    }
}

void verilogwriter::print_input(std::ostream& out, ch::core::inputimpl* node) {
    out << "    input " << get_width_str(node->size()) << " " << node_names_[node];
}

void verilogwriter::print_output(std::ostream& out, ch::core::outputimpl* node) {
    out << "    output " << get_width_str(node->size()) << " " << node_names_[node];
}

void verilogwriter::print_reg(std::ostream& out, ch::core::regimpl* node) {
    // Find the 'next' source node for this register
    auto* next_node = node->get_next();
    if (next_node && node_names_.count(next_node)) {
        out << "    always @(posedge clk) begin // Register update for " << node_names_[node] << "\n";
        out << "        " << node_names_[node] << " <= " << node_names_[next_node] << ";\n";
        out << "    end\n";
    } else {
        // If next is not found or not named, print a warning
        out << "    // Warning: Register '" << node_names_[node] << "' has no named 'next' source.\n";
    }
}

void verilogwriter::print_op(std::ostream& out, ch::core::opimpl* node) {
    if (node->num_srcs() >= 2) { // Binary operations
        auto* lhs_node = node->lhs();
        auto* rhs_node = node->rhs();
        if (lhs_node && rhs_node && node_names_.count(lhs_node) && node_names_.count(rhs_node)) {
            std::string op_str = get_op_str(node->op());
            out << "    assign " << node_names_[node] << " = " << node_names_[lhs_node] << " " << op_str << " " << node_names_[rhs_node] << ";\n";
        } else {
            out << "    // Warning: Operation '" << node_names_[node] << "' has missing or unnamed sources.\n";
        }
    } else if (node->num_srcs() == 1) { // Unary operations (e.g., not)
        auto* src_node = node->lhs(); // For unary, lhs is the operand
        if (src_node && node_names_.count(src_node)) {
            std::string op_str = get_op_str(node->op());
            out << "    assign " << node_names_[node] << " = " << op_str << node_names_[src_node] << ";\n";
        } else {
             out << "    // Warning: Unary operation '" << node_names_[node] << "' has missing or unnamed source.\n";
        }
    } else {
        out << "    // Warning: Operation '" << node_names_[node] << "' has no sources.\n";
    }
}

void verilogwriter::print_proxy(std::ostream& out, ch::core::proxyimpl* node) {
    // A proxy typically connects its source to its destination.
    // In Verilog, this often translates to an 'assign' statement.
    // However, proxyimpl in CASH can represent more complex things like concatenation/subselection.
    // For CppHDL, if proxyimpl is just a simple connection (like outputimpl -> proxyimpl),
    // it might be handled in print_logic when the outputimpl is processed.
    // If proxyimpl is used for intermediate connections, it needs an assign.
    // Check if it has sources.
    if (node->num_srcs() > 0) {
        auto* src_node = node->src(0);
        if (src_node && node_names_.count(src_node)) {
            // If the proxy itself is an output, this assign might be redundant.
            // Check if any users are outputimpl.
            bool is_used_by_output = false;
            auto use_it = node_uses_.find(node);
            if (use_it != node_uses_.end()) {
                for (auto* user : use_it->second) {
                    if (user->type() == ch::core::lnodetype::type_output) {
                        is_used_by_output = true;
                        break;
                    }
                }
            }
            // If used by output, the assign is handled in print_logic for the outputimpl.
            // If used by other nodes (opimpl, regimpl->next, etc.), generate the assign here.
            if (!is_used_by_output) {
                 out << "    assign " << node_names_[node] << " = " << node_names_[src_node] << ";\n";
            }
            // If used by output, we might not need to print anything here, or we might print if the proxy is complex.
            // For now, let's print it assuming it's a simple intermediate wire connection.
            // The output assignment will handle the final connection.
            // This might lead to redundant assigns if proxy is just passthrough.
            // A more complex check involving proxyimpl's internal logic (ranges) would be needed for CASH-style proxyimpls.
            // For CppHDL's simpler usage, this is likely sufficient.
        } else {
             out << "    // Warning: Proxy '" << node_names_[node] << "' has missing or unnamed source.\n";
        }
    } else {
         out << "    // Warning: Proxy '" << node_names_[node] << "' has no source.\n";
    }
}

// --- Helper Function Implementations ---

std::string verilogwriter::sanitize_name(const std::string& name) const {
    std::string sanitized = name;
    // Replace spaces, hyphens, etc., with underscores
    std::replace(sanitized.begin(), sanitized.end(), ' ', '_');
    std::replace(sanitized.begin(), sanitized.end(), '-', '_');
    std::replace(sanitized.begin(), sanitized.end(), '.', '_');
    // Remove other potentially problematic characters if needed
    // Ensure it doesn't start with a number (prefix with 'n' if necessary)
    if (!sanitized.empty() && std::isdigit(sanitized[0])) {
        sanitized = "n" + sanitized;
    }
    return sanitized;
}

std::string verilogwriter::get_width_str(uint32_t size) const {
    if (size == 1) {
        return ""; // No width needed for single bit
    }
    return "[" + std::to_string(size - 1) + ":0]";
}

std::string verilogwriter::get_literal_str(const ch::sdata_type& val) const {
    // Corrected: Format the literal string correctly for Verilog
    // e.g., 8'hFF, 4'd5, 1'b1
    // Use a stringstream to apply std::hex formatting to the raw value
    std::stringstream ss;
    // Determine the base character ('h' for hex, 'd' for decimal, 'b' for binary)
    // For simplicity, always use hex ('h').
    ss << val.bv_.size() << "'h" << std::hex << static_cast<uint64_t>(val.bv_);
    return ss.str(); // Return the formatted string
}
std::string verilogwriter::get_op_str(ch::core::ch_op op) const {
    switch (op) {
        case ch::core::ch_op::add: return "+";
        case ch::core::ch_op::sub: return "-";
        case ch::core::ch_op::mul: return "*";
        case ch::core::ch_op::div: return "/";
        case ch::core::ch_op::mod: return "%";
        case ch::core::ch_op::and_: return "&";
        case ch::core::ch_op::or_: return "|";
        case ch::core::ch_op::xor_: return "^";
        case ch::core::ch_op::not_: return "~"; // Bitwise NOT
        case ch::core::ch_op::eq: return "==";
        case ch::core::ch_op::ne: return "!=";
        case ch::core::ch_op::lt: return "<";
        case ch::core::ch_op::le: return "<=";
        case ch::core::ch_op::gt: return ">";
        case ch::core::ch_op::ge: return ">=";
        // Add other operations as needed
        default:
            return "<OP_NOT_IMPLEMENTED>";
    }
}

} // namespace ch
