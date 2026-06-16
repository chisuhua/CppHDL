// SPDX-License-Identifier: MIT
// CppHDL — JIT IR generation: IO port lnode types (type_input / type_output / type_proxy)
//
// Split out from src/jit/jit_compiler.cpp:generate_ir() during Phase 1 of the
// C-class refactor. Each helper handles its lnode category and owns its own
// node_count bookkeeping. Returns JitResult::SUCCESS or JitResult::UNSUPPORTED_OP.
//
// CRITICAL JIT RULES (AGENTS.md §108-115):
//   - `type_input` MUST check the driver (node->num_srcs() > 0). Connected
//     input ports (Component IO wiring <<=) have num_srcs()>0 and must load
//     from src(0)->id(), not the input's own node_id. Otherwise Component
//     IO hierarchy silently reads zero.

#include "jit/jit_compiler.h"

#include "ast/ast_nodes.h"
#include "core/context.h"
#include "core/lnodeimpl.h"

namespace ch::jit {

// Handles lnodetype::type_input, type_output, type_proxy.
// All three are combinational and increment block_comb.node_count.
JitResult generate_ir_lnode_io(ch::core::lnodeimpl *node,
                               JitBlock &block_comb,
                               VRegId &next_comb_vreg) {
  auto node_type = node->type();
  auto node_id = node->id();
  auto bitwidth = static_cast<BitWidth>(node->size());

  if (node_type == ch::core::lnodetype::type_input) {
    auto vreg = next_comb_vreg++;
    // 连接后的输入端口需要从 driver 加载值，而非从自身
    // (CRITICAL: Component IO wiring <<= uses set_driver which adds a src).
    if (node->num_srcs() > 0 && node->src(0)) {
      auto src_bitwidth = static_cast<BitWidth>(node->src(0)->size());
      block_comb.instrs.push_back(
          make_load(vreg, node->src(0)->id(), src_bitwidth));
      block_comb.instrs.push_back(make_store(node_id, vreg, bitwidth));
    } else {
      block_comb.instrs.push_back(make_load(vreg, node_id, bitwidth));
      block_comb.instrs.push_back(make_store(node_id, vreg, bitwidth));
    }
    block_comb.node_count++;
    return JitResult::SUCCESS;
  }

  if (node_type == ch::core::lnodetype::type_output ||
      node_type == ch::core::lnodetype::type_proxy) {
    if (node->num_srcs() > 0 && node->src(0)) {
      auto src_vreg = next_comb_vreg++;
      block_comb.instrs.push_back(
          make_load(src_vreg, node->src(0)->id(), bitwidth));
      block_comb.instrs.push_back(make_store(node_id, src_vreg, bitwidth));
    }
    block_comb.node_count++;
    return JitResult::SUCCESS;
  }

  return JitResult::SUCCESS;
}

} // namespace ch::jit