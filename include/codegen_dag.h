// include/codegen_dag.h
#ifndef CODEGEN_DAG_H
#define CODEGEN_DAG_H

#include "core/context.h"
#include <algorithm>
#include <cctype>
#include <fstream>
#include <functional>
#include <iostream>
#include <memory>
#include <sstream>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

// Forward declarations for node types used by the writer
namespace ch {
namespace core {
class lnodeimpl;
class litimpl;
class inputimpl;
class outputimpl;
class regimpl;
class opimpl;
class proxyimpl;
} // namespace core
} // namespace ch

namespace ch {

void toDAG(const std::string &filename, ch::core::context *ctx);

// Class responsible for generating DAG representation from the IR graph.
class dagwriter {
public:
    explicit dagwriter(ch::core::context *ctx);

    // Generates the complete DAG representation and writes it to the given
    // stream.
    void print(std::ostream &out);

private:
    // --- Helper functions for name generation ---
    std::string sanitize_name(const std::string &name) const;
    std::string get_node_type_str(ch::core::lnodetype type) const;

    // --- Core printing methods ---
    void print_header(std::ostream &out);
    void print_nodes(std::ostream &out);
    void print_edges(std::ostream &out);
    void print_footer(std::ostream &out);

    // --- Data members ---
    ch::core::context *ctx_;
    // Map to store the generated name for each node
    std::unordered_map<ch::core::lnodeimpl *, std::string> node_names_;
    // Topologically sorted list of nodes
    std::vector<ch::core::lnodeimpl *> sorted_nodes_;
    // Map to store which nodes *use* a given node (reverse dependencies)
    std::unordered_map<ch::core::lnodeimpl *,
                       std::unordered_set<ch::core::lnodeimpl *>>
        node_uses_;
};

} // namespace ch

#endif // CODEGEN_DAG_H