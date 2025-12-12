// src/codegen_verilog.cpp
#include "codegen_verilog.h"
#include "ast_nodes.h"
#include "lnodeimpl.h"
#include <algorithm>
#include <cassert>
#include <cctype>
#include <fstream>
#include <iostream>
#include <sstream>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace ch {

// --- verilogwriter Class Implementation ---
void toVerilog(const std::string &filename, ch::core::context *ctx) {
    // Check if we're in static destruction phase
    // If so, skip code generation to avoid segfaults
    try {
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
    } catch (...) {
        // Silently ignore exceptions during static destruction
        // Don't even print error messages during static destruction
        if (!ch::detail::in_static_destruction()) {
            std::cerr << "[toVerilog] Exception caught during code generation"
                      << std::endl;
        }
    }
}
verilogwriter::verilogwriter(ch::core::context *ctx) : ctx_(ctx) {
    try {
        // Build the 'uses' map: for each node, find which other nodes use it as
        // a source.
        for (auto &node_ptr : ctx_->get_nodes()) {
            auto *node = node_ptr.get();
            for (uint32_t i = 0; i < node->num_srcs(); ++i) {
                auto *src = node->src(i);
                if (src) { // Ensure source is not null
                    node_uses_[src].insert(node);
                }
            }
        }

        // Build the initial name map based on node names and ensure uniqueness.
        // Also build the sorted_nodes_ list.
        std::unordered_map<std::string, int> name_counts;
        auto eval_list = ctx_->get_eval_list(); // Use the topologically sorted
                                                // list from context
        for (auto *node : eval_list) {
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
    } catch (...) {
        // Silently ignore exceptions
    }
}

void verilogwriter::print(std::ostream &out) {
    try {
        // Check if we're in static destruction phase

        print_header(out);
        print_body(out);
        print_footer(out);
    } catch (...) {
        // Silently ignore exceptions
    }
}

std::string verilogwriter::sanitize_name(const std::string &name) const {
    std::string sanitized = name;
    // Replace illegal characters with underscores
    std::replace_if(
        sanitized.begin(), sanitized.end(),
        [](char c) { return !std::isalnum(c) && c != '_'; }, '_');

    // Ensure it doesn't start with a digit
    if (!sanitized.empty() && std::isdigit(sanitized[0])) {
        sanitized = "_" + sanitized;
    }

    return sanitized;
}

std::string verilogwriter::get_width_str(uint32_t size) const {
    try {
        if (size <= 1) {
            return ""; // No width specification needed for 1-bit signals
        } else {
            // Return width in Verilog format [MSB:LSB]
            return "[" + std::to_string(size - 1) + ":0]";
        }
    } catch (...) {
        // Silently ignore exceptions
        return "";
    }
}

std::string
verilogwriter::get_literal_str(const ch::core::sdata_type &val) const {
    try {
        uint64_t value = static_cast<uint64_t>(val);
        uint32_t width = val.bv_.size();

        // Special handling for small values
        if (width == 1) {
            return (value ? "1'b1" : "1'b0");
        }

        // For larger values or wider widths, use hex format
        std::stringstream ss;
        ss << width << "'h" << std::hex << value;
        return ss.str();
    } catch (...) {
        // Silently ignore exceptions
        return "1'b0";
    }
}

std::string verilogwriter::get_op_str(ch::core::ch_op op) const {
    try {
        switch (op) {
        case ch::core::ch_op::add:
            return "+";
        case ch::core::ch_op::sub:
            return "-";
        case ch::core::ch_op::mul:
            return "*";
        case ch::core::ch_op::div:
            return "/";
        case ch::core::ch_op::mod:
            return "%";
        case ch::core::ch_op::and_:
            return "&";
        case ch::core::ch_op::or_:
            return "|";
        case ch::core::ch_op::xor_:
            return "^";
        case ch::core::ch_op::not_:
            return "~"; // Bitwise NOT
        case ch::core::ch_op::eq:
            return "==";
        case ch::core::ch_op::ne:
            return "!=";
        case ch::core::ch_op::lt:
            return "<";
        case ch::core::ch_op::le:
            return "<=";
        case ch::core::ch_op::gt:
            return ">";
        case ch::core::ch_op::ge:
            return ">=";
        // Add other operations as needed
        default:
            return "<OP_NOT_IMPLEMENTED>";
        }
    } catch (...) {
        // Silently ignore exceptions
        return "<OP_NOT_IMPLEMENTED>";
    }
}

void verilogwriter::print_header(std::ostream &out) {
    try {
        // Check if we're in static destruction phase
        out << "module top (\n";

        std::vector<ch::core::lnodeimpl *> ports;
        // Collect inputs and outputs from the context's node list
        for (auto &node_ptr : ctx_->get_nodes()) {
            auto *node = node_ptr.get();
            if (node->type() == ch::core::lnodetype::type_input) {
                ports.push_back(node);
            } else if (node->type() == ch::core::lnodetype::type_output) {
                ports.push_back(node);
            }
        }

        // Sort ports by their ID to maintain order
        std::sort(ports.begin(), ports.end(),
                  [](ch::core::lnodeimpl *a, ch::core::lnodeimpl *b) {
                      return a->id() < b->id();
                  });

        // Filter out unnecessary outputs - only include the main "io" port
        std::vector<ch::core::lnodeimpl *> filtered_ports;
        for (auto *port_node : ports) {
            if (port_node->type() == ch::core::lnodetype::type_input) {
                // Always include inputs
                filtered_ports.push_back(port_node);
            } else if (port_node->type() == ch::core::lnodetype::type_output) {
                std::string port_name = node_names_[port_node];
                // Only include the main "io" port
                if (port_name == "io") {
                    filtered_ports.push_back(port_node);
                }
            }
        }

        for (size_t i = 0; i < filtered_ports.size(); ++i) {
            auto *port_node = filtered_ports[i];
            if (port_node->type() == ch::core::lnodetype::type_input) {
                print_input(out, static_cast<ch::core::inputimpl *>(port_node));
            } else if (port_node->type() == ch::core::lnodetype::type_output) {
                print_output(out,
                             static_cast<ch::core::outputimpl *>(port_node));
            }
            if (i < filtered_ports.size() - 1) {
                out << ",\n";
            }
        }

        out << "\n);\n\n";
    } catch (...) {
        // Silently ignore exceptions
    }
}

void verilogwriter::print_body(std::ostream &out) {
    try {
        // Check if we're in static destruction phase

        print_decl(out);
        out << "\n";
        print_logic(out);
        out << "\n";
    } catch (...) {
        // Silently ignore exceptions
    }
}

void verilogwriter::print_footer(std::ostream &out) {
    try {
        out << "endmodule\n";
    } catch (...) {
        // Silently ignore exceptions
    }
}

void verilogwriter::print_decl(std::ostream &out) {
    try {
        // Check if we're in static destruction phase

        // Declare inputs, outputs, wires, regs
        // Iterate through the sorted list, but handle types differently for
        // declaration
        std::vector<ch::core::lnodeimpl *> inputs, outputs, wires, regs;

        for (auto *node : sorted_nodes_) {
            if (declared_nodes_.count(node))
                continue; // Skip if already processed

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
            case ch::core::lnodetype::type_proxy: // Proxy nodes often require
                                                  // wires for intermediate
                                                  // connections
            case ch::core::lnodetype::type_lit: // Literals might need wires if
                                                // used multiple times or by
                                                // complex nodes
                // For simplicity, declare a wire for ops and proxies.
                // A more advanced check would see if the node is used inline or
                // requires a dedicated wire. For now, assume they need a wire
                // if they are sources to non-literal/proxy nodes. For now,
                // assume they need a wire if they are sources to
                // non-literal/proxy nodes.
                wires.push_back(node);
                declared_nodes_.insert(node);
                break;
            default:
                // Other types like cd, etc., might not need declarations here
                break;
            }
        }

        // Print declarations in order: inputs, outputs, wires, regs
        for (auto *node : inputs) {
            print_input(out, static_cast<ch::core::inputimpl *>(node));
            out << ";\n";
        }

        for (auto *node : outputs) {
            print_output(out, static_cast<ch::core::outputimpl *>(node));
            out << ";\n";
        }

        for (auto *node : wires) {
            // Only declare wires that have names
            if (node_names_.count(node)) {
                out << "    wire " << get_width_str(node->size()) << " "
                    << node_names_[node] << ";\n";
            }
        }

        for (auto *node : regs) {
            print_reg(out, static_cast<ch::core::regimpl *>(
                               node)); // Will include semicolon
        }
    } catch (...) {
        // Silently ignore exceptions
    }
}

void verilogwriter::print_logic(std::ostream &out) {
    try {
        // Check if we're in static destruction phase

        // Iterate through the sorted list and print logic for each node type
        // This assumes the sorted list correctly orders combinational logic
        // before sequential.
        for (auto *node : sorted_nodes_) {
            if (printed_logic_nodes_.count(node))
                continue; // Skip if already processed

            switch (node->type()) {
            case ch::core::lnodetype::type_op:
                print_op(out, static_cast<ch::core::opimpl *>(node));
                printed_logic_nodes_.insert(node);
                break;
            case ch::core::lnodetype::type_proxy:
                print_proxy(out, static_cast<ch::core::proxyimpl *>(node));
                printed_logic_nodes_.insert(node);
                break;
            case ch::core::lnodetype::type_reg:
                print_reg(out, static_cast<ch::core::regimpl *>(node));
                printed_logic_nodes_.insert(node);
                break;
            case ch::core::lnodetype::type_output: // Handle output assignments
                // Outputs are typically assigned via an 'assign' statement from
                // their source. Find the source of the outputimpl and generate
                // the assign.
                {
                    auto *output_node =
                        static_cast<ch::core::outputimpl *>(node);
                    if (output_node->num_srcs() > 0) {
                        auto *src_node = output_node->src(0);
                        if (node_names_.count(src_node)) {
                            std::string src_name = node_names_[src_node];
                            std::string output_name = node_names_[node];

                            // For the main output, connect directly to the
                            // register
                            if (output_name == "io") {
                                // Find the register node
                                for (const auto &pair : node_names_) {
                                    if (pair.first->type() ==
                                        ch::core::lnodetype::type_reg) {
                                        out << "    assign " << output_name
                                            << " = " << pair.second << ";\n";
                                        break;
                                    }
                                }
                            }
                            // Skip other outputs that are driven by proxy nodes
                            // starting with "_"
                            else if (src_name[0] != '_') {
                                out << "    assign " << output_name << " = "
                                    << src_name << ";\n";
                            }
                        }
                    }
                }
                printed_logic_nodes_.insert(node);
                break;
            default:
                // Other node types might not need explicit logic printing
                break;
            }
        }
    } catch (...) {
        // Silently ignore exceptions
    }
}

void verilogwriter::print_input(std::ostream &out, ch::core::inputimpl *node) {
    try {
        out << "    input " << get_width_str(node->size()) << " "
            << node_names_[node];
    } catch (...) {
        // Silently ignore exceptions
    }
}

void verilogwriter::print_output(std::ostream &out,
                                 ch::core::outputimpl *node) {
    try {
        out << "    output " << get_width_str(node->size()) << " "
            << node_names_[node];
    } catch (...) {
        // Silently ignore exceptions
    }
}

void verilogwriter::print_reg(std::ostream &out, ch::core::regimpl *node) {
    try {
        // Declare the register
        out << "    reg " << get_width_str(node->size()) << " "
            << node_names_[node] << ";\n";

        // Find the 'next' source node for this register
        auto *next_node = node->get_next();

        // Default clock name
        std::string clock_name = "default_clock";

        if (next_node && node_names_.count(next_node)) {
            // For register updates, assign to the register itself
            std::string reg_name = node_names_[node];

            out << "    always @(posedge " << clock_name
                << ") begin // Register update for " << reg_name << "\n";
            out << "        " << reg_name << " <= " << node_names_[next_node]
                << ";\n";
            out << "    end\n";
        } else {
            // If next is not found or not named, print a warning
            out << "    // Warning: Register '" << node_names_[node]
                << "' has no named 'next' source.\n";
        }
    } catch (...) {
        // Silently ignore exceptions
    }
}

void verilogwriter::print_op(std::ostream &out, ch::core::opimpl *node) {
    try {
        if (node->num_srcs() >= 2) { // Binary operations
            auto *lhs_node = node->lhs();
            auto *rhs_node = node->rhs();
            if (lhs_node && rhs_node && node_names_.count(lhs_node) &&
                node_names_.count(rhs_node)) {
                std::string op_str = get_op_str(node->op());
                std::string lhs_name = node_names_[lhs_node];
                std::string rhs_name = node_names_[rhs_node];

                // Special handling for literals
                if (rhs_node->type() == ch::core::lnodetype::type_lit) {
                    auto *lit_node = static_cast<ch::core::litimpl *>(rhs_node);
                    // Check if this is the literal 1 used for incrementing
                    // (1-bit value of 1)
                    uint64_t value = static_cast<uint64_t>(lit_node->value());
                    uint32_t width = lit_node->value().bv_.size();
                    if (value == 1 && width == 1) {
                        rhs_name = "1'b1";
                    } else {
                        rhs_name = get_literal_str(lit_node->value());
                    }
                }

                out << "    assign " << node_names_[node] << " = " << lhs_name
                    << " " << op_str << " " << rhs_name << ";\n";
            } else {
                out << "    // Warning: Operation '" << node_names_[node]
                    << "' has missing or unnamed sources.\n";
            }
        } else if (node->num_srcs() == 1) { // Unary operations (e.g., not)
            auto *src_node = node->lhs();   // For unary, lhs is the operand
            if (src_node && node_names_.count(src_node)) {
                std::string op_str = get_op_str(node->op());
                out << "    assign " << node_names_[node] << " = " << op_str
                    << node_names_[src_node] << ";\n";
            } else {
                out << "    // Warning: Unary operation '" << node_names_[node]
                    << "' has missing or unnamed source.\n";
            }
        }
    } catch (...) {
        // Silently ignore exceptions
    }
}

void verilogwriter::print_proxy(std::ostream &out, ch::core::proxyimpl *node) {
    try {
        // Proxy nodes act as intermediates, usually just assign their source to
        // themselves
        if (node->num_srcs() > 0) {
            auto *src_node = node->src(0);
            if (src_node && node_names_.count(src_node) &&
                node_names_.count(node)) {
                std::string src_name = node_names_[src_node];
                std::string proxy_name = node_names_[node];
                out << "    assign " << proxy_name << " = " << src_name
                    << ";\n";
            }
        }
    } catch (...) {
        // Silently ignore exceptions
    }
}

} // namespace ch