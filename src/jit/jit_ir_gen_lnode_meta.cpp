// SPDX-License-Identifier: MIT
// CppHDL — JIT IR generation: metadata / pseudo-lnode types
//   (type_clock / type_reset / type_none / type_lit)
//
// Split out from src/jit/jit_compiler.cpp:generate_ir() during Phase 1 of the
// C-class refactor. The dispatch loop in jit_compiler.cpp handles
// block_comb.node_count++ for combinational cases; this helper only emits IR
// for type_lit (LOAD_DATA so set_value() updates persist via sync_from_buffer).
//
// CRITICAL JIT RULES (AGENTS.md §108-115):
//   - type_lit MUST emit a LOAD_DATA instruction (read from data_buffer_) NOT
//     a LOAD_CONST (compile-time constant). The interpreter doesn't evaluate
//     litimpl either (no create_instruction), so JIT must also avoid
//     overwriting runtime values with compile-time constants.

#include "jit/jit_compiler.h"

#include "ast/ast_nodes.h"
#include "core/context.h"
#include "core/lnodeimpl.h"

namespace ch::jit {

// Handles lnodetype::type_clock, type_reset, type_none, type_lit.
// Clock/reset/none are no-ops (the dispatch loop counts them as combinational).
// type_lit emits a LOAD_DATA so runtime set_value() updates persist.
JitResult generate_ir_lnode_meta(ch::core::lnodeimpl *node,
                                 JitBlock &block_comb,
                                 VRegId &next_comb_vreg) {
  auto node_type = node->type();

  if (node_type == ch::core::lnodetype::type_lit) {
    auto node_id = node->id();
    auto bitwidth = static_cast<BitWidth>(node->size());
    // type_lit: literal values are set at runtime via set_value().
    // The interpreter does NOT evaluate them (litimpl has no
    // create_instruction), so JIT must also skip them to avoid overwriting
    // runtime values with compile-time constants. Use LOAD_DATA
    // (read from data_buffer_) instead of LOAD_CONST (compile-time constant).
    auto vreg = next_comb_vreg++;
    block_comb.instrs.push_back(make_load(vreg, node_id, bitwidth));
    return JitResult::SUCCESS;
  }

  // type_clock, type_reset, type_none: no-op (counted by dispatch loop)
  return JitResult::SUCCESS;
}

} // namespace ch::jit