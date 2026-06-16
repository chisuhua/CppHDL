// SPDX-License-Identifier: MIT
// CppHDL — JIT IR generation: combinational logic lnode types
//   (type_op / type_mux / type_bitsupdate)

#include "jit/jit_compiler.h"

#include "ast/ast_nodes.h"
#include "core/context.h"
#include "core/lnodeimpl.h"

namespace ch::jit {

namespace {

// Map ch_op -> JitOp. mux and bits_update map to CALL_EXTERNAL because
// production code uses the lnodetype path (type_mux / type_bitsupdate) for
// JIT-native ops.
JitOp ch_op_to_jit_op(ch::core::ch_op op) {
  using CO = ch::core::ch_op;
  using JO = JitOp;
  switch (op) {
  case CO::add:         return JO::ADD;
  case CO::sub:         return JO::SUB;
  case CO::mul:         return JO::MUL;
  case CO::div:         return JO::DIV;
  case CO::mod:         return JO::MOD;
  case CO::and_:        return JO::AND;
  case CO::or_:         return JO::OR;
  case CO::xor_:        return JO::XOR;
  case CO::not_:        return JO::NOT;
  case CO::eq:          return JO::EQ;
  case CO::ne:          return JO::NE;
  case CO::lt:          return JO::LT;
  case CO::le:          return JO::LE;
  case CO::gt:          return JO::GT;
  case CO::ge:          return JO::GE;
  case CO::shl:         return JO::SHIFT_LEFT;
  case CO::shr:         return JO::SHIFT_RIGHT;
  case CO::bit_sel:     return JO::BIT_SELECT;
  case CO::bits_extract: return JO::BITS_EXTRACT;
  case CO::and_reduce:  return JO::AND_REDUCE;
  case CO::or_reduce:   return JO::OR_REDUCE;
  case CO::xor_reduce:  return JO::XOR_REDUCE;
  case CO::rotate_l:    return JO::ROTATE_LEFT;
  case CO::rotate_r:    return JO::ROTATE_RIGHT;
  case CO::assign:      return JO::ASSIGN;
  case CO::concat:      return JO::CONCAT;
  case CO::popcount:    return JO::POPCOUNT;
  case CO::sext:        return JO::SEXT;
  case CO::zext:        return JO::ZEXT;
  case CO::sshr:        return JO::SSHRSHN;
  case CO::neg:         return JO::NEG;
  case CO::mux:         return JO::CALL_EXTERNAL;
  case CO::bits_update: return JO::CALL_EXTERNAL;
  }
  return JO::CALL_EXTERNAL;
}

void emit_type_op(ch::core::opimpl *op_node, JitBlock &block_comb,
                  VRegId &next_comb_vreg,
                  std::unordered_set<uint32_t> &external_node_ids) {
  auto node_id = op_node->id();
  auto result_bw = static_cast<BitWidth>(op_node->size());
  JitOp jit_op = ch_op_to_jit_op(op_node->op());
  if (jit_op == JitOp::CALL_EXTERNAL) {
    external_node_ids.insert(node_id);
    return;
  }

  auto src0_vreg = next_comb_vreg++;
  auto src1_vreg = next_comb_vreg++;
  auto dst_vreg = next_comb_vreg++;

  if (op_node->num_srcs() > 0 && op_node->src(0)) {
    auto bw = static_cast<BitWidth>(op_node->src(0)->size());
    block_comb.instrs.push_back(make_load(src0_vreg, op_node->src(0)->id(), bw));
  }
  if (op_node->num_srcs() > 1 && op_node->src(1)) {
    auto bw = static_cast<BitWidth>(op_node->src(1)->size());
    block_comb.instrs.push_back(make_load(src1_vreg, op_node->src(1)->id(), bw));
  }

  // SEXT/ZEXT/NEG/SSHRSHN/CONCAT need src_bitwidth on the JitInstr (CONCAT
  // uses src1's bitwidth as the shift amount).
  const bool needs_src_bw = (jit_op == JitOp::SEXT || jit_op == JitOp::ZEXT ||
                             jit_op == JitOp::NEG || jit_op == JitOp::SSHRSHN ||
                             jit_op == JitOp::CONCAT);
  if (needs_src_bw) {
    JitInstr jit_instr(jit_op, result_bw);
    jit_instr.dst = dst_vreg;
    jit_instr.src0 = src0_vreg;
    if (jit_op == JitOp::CONCAT) {
      jit_instr.src1 = src1_vreg;
      jit_instr.src_bitwidth = (op_node->num_srcs() > 1 && op_node->src(1))
                                   ? static_cast<BitWidth>(op_node->src(1)->size())
                                   : BitWidth{0};
    } else {
      jit_instr.src_bitwidth = (op_node->num_srcs() > 0 && op_node->src(0))
                                   ? static_cast<BitWidth>(op_node->src(0)->size())
                                   : result_bw;
      if (jit_op == JitOp::SSHRSHN) jit_instr.src1 = src1_vreg;
    }
    jit_instr.node_id = node_id;
    block_comb.instrs.push_back(jit_instr);
    block_comb.instrs.push_back(make_store(node_id, dst_vreg, result_bw));
  } else {
    block_comb.instrs.push_back(
        make_binary(jit_op, dst_vreg, src0_vreg, src1_vreg, result_bw));
    block_comb.instrs.push_back(make_store(node_id, dst_vreg, result_bw));
  }
}

void emit_type_mux(ch::core::lnodeimpl *node, JitBlock &block_comb,
                   VRegId &next_comb_vreg) {
  auto node_id = node->id();
  auto bw = static_cast<BitWidth>(node->size());
  auto cond_vreg = next_comb_vreg++;
  auto src0_vreg = next_comb_vreg++;
  auto src1_vreg = next_comb_vreg++;
  auto dst_vreg = next_comb_vreg++;
  if (node->num_srcs() >= 1 && node->src(0))
    block_comb.instrs.push_back(make_load(cond_vreg, node->src(0)->id(), 1));
  if (node->num_srcs() >= 2 && node->src(1))
    block_comb.instrs.push_back(make_load(src0_vreg, node->src(1)->id(), bw));
  if (node->num_srcs() >= 3 && node->src(2))
    block_comb.instrs.push_back(make_load(src1_vreg, node->src(2)->id(), bw));
  block_comb.instrs.push_back(
      make_select(dst_vreg, cond_vreg, src0_vreg, src1_vreg, bw));
  block_comb.instrs.push_back(make_store(node_id, dst_vreg, bw));
}

void emit_type_bitsupdate(ch::core::bitsupdateimpl *bn, JitBlock &block_comb,
                          VRegId &next_comb_vreg) {
  auto node_id = bn->id();
  auto bw = static_cast<BitWidth>(bn->size());
  auto target_vreg = next_comb_vreg++;
  auto source_vreg = next_comb_vreg++;
  auto dst_vreg = next_comb_vreg++;
  if (bn->target() && bn->target()->id() != 0)
    block_comb.instrs.push_back(make_load(target_vreg, bn->target()->id(), bw));
  if (bn->source() && bn->source()->id() != 0)
    block_comb.instrs.push_back(make_load(source_vreg, bn->source()->id(), bw));
  uint32_t lsb = 0, width = 0;
  if (bn->range() && bn->range()->type() == ch::core::lnodetype::type_lit) {
    auto *lit = static_cast<ch::core::litimpl *>(bn->range());
    uint64_t val = static_cast<uint64_t>(lit->value());
    uint32_t msb = static_cast<uint32_t>(val >> 32);
    lsb = static_cast<uint32_t>(val & 0xFFFFFFFF);
    width = msb - lsb + 1;
  }
  JitInstr instr(JitOp::BITS_UPDATE, bw);
  instr.dst = dst_vreg;
  instr.src0 = target_vreg;
  instr.src1 = source_vreg;
  instr.imm = ImmValue(lsb, static_cast<BitWidth>(width));
  block_comb.instrs.push_back(instr);
  block_comb.instrs.push_back(make_store(node_id, dst_vreg, bw));
}

} // namespace

JitResult generate_ir_lnode_logic(
    ch::core::lnodeimpl *node, ch::core::context * /*ctx*/,
    JitBlock &block_comb, VRegId &next_comb_vreg,
    std::unordered_set<uint32_t> &external_node_ids) {
  auto t = node->type();
  if (t == ch::core::lnodetype::type_op)
    emit_type_op(static_cast<ch::core::opimpl *>(node), block_comb,
                 next_comb_vreg, external_node_ids);
  else if (t == ch::core::lnodetype::type_mux)
    emit_type_mux(node, block_comb, next_comb_vreg);
  else if (t == ch::core::lnodetype::type_bitsupdate)
    emit_type_bitsupdate(static_cast<ch::core::bitsupdateimpl *>(node),
                         block_comb, next_comb_vreg);
  block_comb.node_count++;
  return JitResult::SUCCESS;
}

} // namespace ch::jit