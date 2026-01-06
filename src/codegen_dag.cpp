// src/codegen_dag.cpp
#include "codegen_dag.h"
#include "ast_nodes.h"
#include "lnodeimpl.h"
#include "simulator.h"
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

// --- DAG Generation Function ---
void toDAG(const std::string &filename, ch::core::context *ctx) {
    try {
        if (!ctx) {
            std::cerr << "[toDAG] Error: Context is null!" << std::endl;
            return;
        }

        std::ofstream file(filename);
        if (!file.is_open()) {
            std::cerr << "[toDAG] Failed to open " << filename << std::endl;
            return;
        }

        dagwriter writer(ctx);
        writer.print(file);

        file.close();
        std::cout << "[toDAG] Generated " << filename << std::endl;
    } catch (...) {
        // Silently ignore exceptions during static destruction
        // if (!ch::detail::in_static_destruction()) {
        //     std::cerr << "[toDAG] Exception caught during DAG generation"
        //               << std::endl;
        // }
    }
}

// New DAG Generation Function that accepts a simulator
void toDAG(const std::string &filename, ch::core::context *ctx, const ch::Simulator &simulator) {
    try {
        if (!ctx) {
            std::cerr << "[toDAG] Error: Context is null!" << std::endl;
            return;
        }

        std::ofstream file(filename);
        if (!file.is_open()) {
            std::cerr << "[toDAG] Failed to open " << filename << std::endl;
            return;
        }

        dagwriter writer(ctx, simulator);
        writer.print(file);

        file.close();
        std::cout << "[toDAG] Generated " << filename << " with simulation values" << std::endl;
    } catch (...) {
        // Silently ignore exceptions during static destruction
        // if (!ch::detail::in_static_destruction()) {
        //     std::cerr << "[toDAG] Exception caught during DAG generation"
        //               << std::endl;
        // }
    }
}

// --- dagwriter Class Implementation ---
dagwriter::dagwriter(ch::core::context *ctx) : ctx_(ctx) {
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

// New constructor that accepts a simulator to access data_map
dagwriter::dagwriter(ch::core::context *ctx, const ch::Simulator &simulator) : ctx_(ctx) {
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
        
        // Get the data_map from simulator
        data_map_ = simulator.data_map();
    } catch (...) {
        // Silently ignore exceptions
    }
}

void dagwriter::print(std::ostream &out) {
    try {
        // Check if we're in static destruction phase

        print_header(out);
        print_nodes(out);
        print_edges(out);
        print_footer(out);
    } catch (...) {
        // Silently ignore exceptions
    }
}

std::string dagwriter::sanitize_name(const std::string &name) const {
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

std::string dagwriter::get_node_type_str(ch::core::lnodetype type) const {
    switch (type) {
    case ch::core::lnodetype::type_input:
        return "input";
    case ch::core::lnodetype::type_output:
        return "output";
    case ch::core::lnodetype::type_lit:
        return "literal";
    case ch::core::lnodetype::type_reg:
        return "register";
    case ch::core::lnodetype::type_op:
        return "operation";
    case ch::core::lnodetype::type_proxy:
        return "proxy";
    case ch::core::lnodetype::type_mem:
        return "memory";
    case ch::core::lnodetype::type_mem_read_port:
        return "mem_read_port";
    case ch::core::lnodetype::type_mem_write_port:
        return "mem_write_port";
    case ch::core::lnodetype::type_mux:
        return "mux";
    default:
        return "unknown";
    }
}

void dagwriter::print_header(std::ostream &out) {
    out << "digraph G {\n";
    out << "  rankdir=TB;\n";
    out << "  node [shape=box, style=filled, fillcolor=lightgray];\n\n";
}

void dagwriter::print_nodes(std::ostream &out) {
    // Print all nodes with their properties
    for (size_t i = 0; i < sorted_nodes_.size(); ++i) {
        auto *node = sorted_nodes_[i];

        out << "  \"" << node_names_[node] << "\" [";
        out << "label=\"" << node_names_[node] << "\\n";
        out << "(ID: " << node->id() << ", " << get_node_type_str(node->type())
            << ")\\n";
        out << "width=" << node->size() << " bits";

        // Add additional info based on node type
        switch (node->type()) {
        case ch::core::lnodetype::type_lit: {
            auto *lit_node = static_cast<ch::core::litimpl *>(node);
            out << "\\nvalue=" << lit_node->value().to_string();
        } break;
        case ch::core::lnodetype::type_op: {
            auto *op_node = static_cast<ch::core::opimpl *>(node);
            out << "\\nop=" << static_cast<int>(op_node->op());
        } break;
        default:
            break;
        }

        out << "\", ";

        // Color nodes based on type
        switch (node->type()) {
        case ch::core::lnodetype::type_input:
            out << "fillcolor=lightblue";
            break;
        case ch::core::lnodetype::type_output:
            out << "fillcolor=lightyellow";
            break;
        case ch::core::lnodetype::type_lit:
            out << "fillcolor=lightgreen";
            break;
        case ch::core::lnodetype::type_reg:
            out << "fillcolor=lightcoral";
            break;
        case ch::core::lnodetype::type_op:
            out << "fillcolor=lightpink";
            break;
        default:
            out << "fillcolor=lightgray";
            break;
        }

        out << "];\n";
    }
    out << "\n";
}

void dagwriter::print_edges(std::ostream &out) {
    // Print edges between nodes
    for (auto *node : sorted_nodes_) {

        for (uint32_t i = 0; i < node->num_srcs(); ++i) {
            auto *src = node->src(i);
            if (src && node_names_.count(src)) {
                out << "  \"" << node_names_[src] << "\" -> \""
                    << node_names_[node] << "\"";

                // Create edge label that includes source value if available
                std::string edge_label = "src" + std::to_string(i);
                
                // Check if we have data_map available and if the source node has a value
                auto data_it = data_map_.find(src->id());
                if (data_it != data_map_.end()) {
                    // Add the value to the edge label
                    edge_label += "\\nval=" + data_it->second.to_string();
                }

                // Add edge label for multi-source nodes (like operations)
                if (node->num_srcs() > 1 || data_it != data_map_.end()) {
                    out << " [label=\"" << edge_label << "\"]";
                }

                out << ";\n";
            }
        }
    }
    out << "\n";
}

void dagwriter::print_footer(std::ostream &out) { out << "}\n"; }

} // namespace ch