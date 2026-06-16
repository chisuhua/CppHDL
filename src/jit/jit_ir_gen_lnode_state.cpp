// SPDX-License-Identifier: MIT
// CppHDL — JIT IR generation: stateful lnode types
//   (type_reg / type_mem / type_mem_read_port / type_mem_write_port)
//
// Split out from src/jit/jit_compiler.cpp:generate_ir() during Phase 1 of the
// C-class refactor. type_mem_read_port returns UNSUPPORTED_OP because the
// memory array state isn't accessible from the JIT-compiled tick function —
// we must fall back to the interpreter path. type_mem and type_mem_write_port
// are silently skipped (memory write happens via interpreter on every tick).

#include "jit/jit_compiler.h"

#include "ast/ast_nodes.h"
#include "core/context.h"
#include "core/lnodeimpl.h"

namespace ch::jit {

// Handles lnodetype::type_reg, type_mem, type_mem_read_port, type_mem_write_port.
// Only type_reg increments block_seq.node_count.
JitResult generate_ir_lnode_state(ch::core::lnodeimpl *node,
                                  JitBlock &block_seq,
                                  VRegId &next_seq_vreg) {
  auto node_type = node->type();
  auto node_id = node->id();
  auto bitwidth = static_cast<BitWidth>(node->size());

  if (node_type == ch::core::lnodetype::type_reg) {
    auto *reg_node = static_cast<ch::core::regimpl *>(node);
    auto *next_node = reg_node->get_next();

    // Load the "next" value from the source node into a vreg
    VRegId next_vreg = INVALID_VREG;
    if (next_node && next_node->size() <= 64) {
      next_vreg = next_seq_vreg++;
      auto next_bitwidth = static_cast<BitWidth>(next_node->size());
      block_seq.instrs.push_back(
          make_load(next_vreg, next_node->id(), next_bitwidth));
    }

    // REG_NEXT: store the "next" value to the register's output node
    auto dst_vreg = next_seq_vreg++;
    auto reg_next = make_reg_next(dst_vreg, node_id, bitwidth);
    reg_next.src0 = next_vreg; // Set source (INVALID_VREG means no update)
    block_seq.instrs.push_back(reg_next);
    block_seq.node_count++;
    return JitResult::SUCCESS;
  }

  if (node_type == ch::core::lnodetype::type_mem) {
    // Memory nodes are processed separately - skip here
    return JitResult::SUCCESS;
  }

  // Memory read ports have combinational outputs that depend on memory array
  // state. Without the memory array accessible in JIT, we must fall back to
  // interpreter.
  if (node_type == ch::core::lnodetype::type_mem_read_port) {
    return JitResult::UNSUPPORTED_OP;
  }

  if (node_type == ch::core::lnodetype::type_mem_write_port) {
    return JitResult::SUCCESS;
  }

  return JitResult::SUCCESS;
}

} // namespace ch::jit