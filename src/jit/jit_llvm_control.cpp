// SPDX-License-Identifier: MIT
// CppHDL — LLVM IR lowering for control-flow JitOps:
//   SELECT, REG_NEXT, BITS_UPDATE, BIT_SELECT, CONCAT, BITS_EXTRACT,
//   CALL_EXTERNAL, ASSIGN
//
// Split out from src/jit/jit_compiler.cpp:compile_to_llvm() during Phase 1.

#include "jit_llvm_helpers.h"

#include <vector>

namespace ch::jit {

#if defined(CH_JIT_ENABLED) && __has_include(<llvm/IR/LLVMContext.h>)

void compile_to_llvm_select(const JitInstr &instr, llvm::IRBuilder<> &builder,
                            std::vector<llvm::Value *> &vregs) {
  auto *cond =
      builder.CreateLoad(builder.getInt64Ty(), vregs[instr.src0], "load_cond");
  auto *a =
      builder.CreateLoad(builder.getInt64Ty(), vregs[instr.src1], "load_a");
  auto *b =
      builder.CreateLoad(builder.getInt64Ty(), vregs[instr.src2], "load_b");
  auto *cmp = builder.CreateICmpNE(cond, builder.getInt64(0), "cond_nz");
  auto *res = builder.CreateSelect(cmp, a, b, "select");
  builder.CreateStore(res, vregs[instr.dst], "store_select");
}

void compile_to_llvm_reg_next(const JitInstr &instr, llvm::IRBuilder<> &builder,
                              llvm::Value *data_buffer_ptr,
                              std::vector<llvm::Value *> &vregs) {
  if (instr.src0 == INVALID_VREG) {
    return;
  }
  auto *val =
      builder.CreateLoad(builder.getInt64Ty(), vregs[instr.src0], "load_reg");
  jit_store_node(builder, data_buffer_ptr, instr.node_id, val, instr.bitwidth);
}

void compile_to_llvm_bits_update(const JitInstr &instr,
                                 llvm::IRBuilder<> &builder,
                                 std::vector<llvm::Value *> &vregs) {
  auto *target =
      builder.CreateLoad(builder.getInt64Ty(), vregs[instr.src0], "load_target");
  auto *source =
      builder.CreateLoad(builder.getInt64Ty(), vregs[instr.src1], "load_source");
  uint32_t lsb = static_cast<uint32_t>(instr.imm.value);
  uint32_t width = static_cast<uint32_t>(instr.imm.bitwidth);

  if (width > 0 && width < 64) {
    uint64_t clear_mask_val = ~(((1ULL << width) - 1) << lsb);
    auto *clear_mask = builder.getInt64(clear_mask_val);
    auto *cleared = builder.CreateAnd(target, clear_mask, "bits_cleared");

    auto *shifted =
        builder.CreateShl(source, builder.getInt64(lsb), "bits_shifted");
    uint64_t keep_mask_val = ((1ULL << width) - 1) << lsb;
    auto *keep_mask = builder.getInt64(keep_mask_val);
    auto *inserted = builder.CreateAnd(shifted, keep_mask, "bits_inserted");

    auto *res = builder.CreateOr(cleared, inserted, "bits_update");
    res = jit_mask_to_bw(builder, res, instr.bitwidth);
    builder.CreateStore(res, vregs[instr.dst], "store_bits_update");
  } else {
    builder.CreateStore(source, vregs[instr.dst], "store_bits_update");
  }
}

void compile_to_llvm_bit_select(const JitInstr &instr,
                                llvm::IRBuilder<> &builder,
                                std::vector<llvm::Value *> &vregs) {
  auto *data = builder.CreateLoad(builder.getInt64Ty(), vregs[instr.src0],
                                  "load_data");
  auto *idx =
      builder.CreateLoad(builder.getInt64Ty(), vregs[instr.src1], "load_idx");
  auto *shifted = builder.CreateLShr(data, idx, "shift_idx");
  auto *masked = builder.CreateAnd(shifted, builder.getInt64(1), "mask_bit");
  builder.CreateStore(masked, vregs[instr.dst], "store_bit_sel");
}

void compile_to_llvm_concat(const JitInstr &instr, llvm::IRBuilder<> &builder,
                            std::vector<llvm::Value *> &vregs) {
  // result = (src0 << src1_width) | src1
  // src0 -> HIGH bits, src1 -> LOW bits (matches Concat::eval in
  // include/ast/instr_op.h). instr.src_bitwidth must be set to src1_width
  // by generate_ir() (the amount we need to shift src0 up by).
  auto *src0_val = builder.CreateLoad(builder.getInt64Ty(), vregs[instr.src0],
                                      "load_concat_src0");
  auto *src1_val = builder.CreateLoad(builder.getInt64Ty(), vregs[instr.src1],
                                      "load_concat_src1");
  auto *shift_amt = builder.getInt64(instr.src_bitwidth);
  auto *shifted = builder.CreateShl(src0_val, shift_amt, "concat_shift");
  auto *res = builder.CreateOr(shifted, src1_val, "concat_or");
  res = jit_mask_to_bw(builder, res, instr.bitwidth);
  builder.CreateStore(res, vregs[instr.dst], "store_concat");
}

void compile_to_llvm_bits_extract(const JitInstr &instr,
                                  llvm::IRBuilder<> &builder,
                                  std::vector<llvm::Value *> &vregs) {
  auto *val =
      builder.CreateLoad(builder.getInt64Ty(), vregs[instr.src0], "load_val");
  auto *range = builder.CreateLoad(builder.getInt64Ty(), vregs[instr.src1],
                                   "load_range");
  auto *lsb = builder.CreateAnd(range, builder.getInt64(0xFFFFFFFF), "lsb");
  auto *msb = builder.CreateLShr(range, builder.getInt64(32), "msb_shift");
  msb = builder.CreateAnd(msb, builder.getInt64(0xFFFFFFFF), "msb");
  auto *shifted = builder.CreateLShr(val, lsb, "shift_val");
  auto *width = builder.CreateSub(msb, lsb, "width_sub");
  width = builder.CreateAdd(width, builder.getInt64(1), "width_add1");
  auto *mask = builder.CreateShl(builder.getInt64(1), width, "mask_shl");
  mask = builder.CreateSub(mask, builder.getInt64(1), "mask_sub1");
  auto *res = builder.CreateAnd(shifted, mask, "extract");
  res = jit_mask_to_bw(builder, res, instr.bitwidth);
  builder.CreateStore(res, vregs[instr.dst], "store_bits_extract");
}

void compile_to_llvm_call_external(const JitInstr &instr,
                                   llvm::IRBuilder<> &builder,
                                   llvm::Value *data_buffer_ptr,
                                   std::vector<llvm::Value *> &vregs) {
  auto *ptr = jit_get_node_ptr(builder, data_buffer_ptr, instr.node_id);
  auto *val =
      builder.CreateLoad(builder.getInt64Ty(), ptr, "call_ext_load");
  builder.CreateStore(val, vregs[instr.dst], "call_ext_store");
}

void compile_to_llvm_assign(const JitInstr &instr, llvm::IRBuilder<> &builder,
                            std::vector<llvm::Value *> &vregs) {
  // wire pass-through: dst = src0, masked to bitwidth.
  llvm::Value *src = builder.CreateLoad(builder.getInt64Ty(), vregs[instr.src0],
                                        "load_assign_src");
  src = jit_mask_to_bw(builder, src, instr.bitwidth);
  builder.CreateStore(src, vregs[instr.dst], "store_assign");
}

void compile_to_llvm_control(const JitInstr &instr, llvm::IRBuilder<> &builder,
                             llvm::Value *data_buffer_ptr,
                             std::vector<llvm::Value *> &vregs) {
  switch (instr.op) {
  case JitOp::SELECT:
    compile_to_llvm_select(instr, builder, vregs);
    break;
  case JitOp::REG_NEXT:
    compile_to_llvm_reg_next(instr, builder, data_buffer_ptr, vregs);
    break;
  case JitOp::BITS_UPDATE:
    compile_to_llvm_bits_update(instr, builder, vregs);
    break;
  case JitOp::BIT_SELECT:
    compile_to_llvm_bit_select(instr, builder, vregs);
    break;
  case JitOp::CONCAT:
    compile_to_llvm_concat(instr, builder, vregs);
    break;
  case JitOp::BITS_EXTRACT:
    compile_to_llvm_bits_extract(instr, builder, vregs);
    break;
  case JitOp::CALL_EXTERNAL:
    compile_to_llvm_call_external(instr, builder, data_buffer_ptr, vregs);
    break;
  case JitOp::ASSIGN:
    compile_to_llvm_assign(instr, builder, vregs);
    break;
  default:
    break;
  }
}

#else
void compile_to_llvm_control(const JitInstr &, llvm::IRBuilder<> &,
                             llvm::Value *, std::vector<llvm::Value *> &) {}
#endif

} // namespace ch::jit