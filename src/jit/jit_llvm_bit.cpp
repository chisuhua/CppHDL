// SPDX-License-Identifier: MIT
// CppHDL — LLVM IR lowering for bitwise / shift JitOps:
//   AND, OR, XOR, NOT, SHIFT_LEFT, SHIFT_RIGHT, ROTATE_LEFT, ROTATE_RIGHT,
//   SEXT, ZEXT, SSHRSHN, NEG, AND_REDUCE, OR_REDUCE, XOR_REDUCE, POPCOUNT
//
// Split out from src/jit/jit_compiler.cpp:compile_to_llvm() during Phase 1.

#include "jit_llvm_helpers.h"

#include <vector>

namespace ch::jit {

#if defined(CH_JIT_ENABLED) && __has_include(<llvm/IR/LLVMContext.h>)

void compile_to_llvm_bit(const JitInstr &instr, llvm::IRBuilder<> &builder,
                         std::vector<llvm::Value *> &vregs) {
  auto *a =
      builder.CreateLoad(builder.getInt64Ty(), vregs[instr.src0], "load_a");
  llvm::Value *res = nullptr;

  switch (instr.op) {
  case JitOp::AND:
  case JitOp::OR:
  case JitOp::XOR: {
    auto *b =
        builder.CreateLoad(builder.getInt64Ty(), vregs[instr.src1], "load_b");
    if (instr.op == JitOp::AND)
      res = builder.CreateAnd(a, b, "and");
    else if (instr.op == JitOp::OR)
      res = builder.CreateOr(a, b, "or");
    else
      res = builder.CreateXor(a, b, "xor");
    break;
  }
  case JitOp::NOT: {
    auto *all1 = builder.getInt64(uint64_t(-1));
    res = builder.CreateXor(a, all1, "not");
    break;
  }
  case JitOp::SHIFT_LEFT:
  case JitOp::SHIFT_RIGHT: {
    auto *b =
        builder.CreateLoad(builder.getInt64Ty(), vregs[instr.src1], "load_b");
    if (instr.op == JitOp::SHIFT_LEFT)
      res = builder.CreateShl(a, b, "shl");
    else
      res = builder.CreateLShr(a, b, "shr");
    break;
  }
  case JitOp::ROTATE_LEFT:
  case JitOp::ROTATE_RIGHT: {
    auto *b =
        builder.CreateLoad(builder.getInt64Ty(), vregs[instr.src1], "load_b");
    auto *bw_val = builder.getInt64(instr.bitwidth);
    auto *shift_count = builder.CreateURem(b, bw_val, "rot_count");
    auto *inv_count =
        builder.CreateSub(bw_val, shift_count, "rot_inv_count");
    llvm::Value *left, *right;
    if (instr.op == JitOp::ROTATE_LEFT) {
      left = builder.CreateShl(a, shift_count, "rot_left");
      right = builder.CreateLShr(a, inv_count, "rot_right");
      res = builder.CreateOr(left, right, "rotl");
    } else {
      right = builder.CreateLShr(a, shift_count, "rot_right");
      left = builder.CreateShl(a, inv_count, "rot_left");
      res = builder.CreateOr(right, left, "rotr");
    }
    break;
  }
  case JitOp::SEXT:
  case JitOp::ZEXT: {
    auto *truncated =
        builder.CreateTrunc(a, builder.getIntNTy(instr.src_bitwidth), "trunc");
    if (instr.op == JitOp::SEXT)
      res = builder.CreateSExt(truncated, builder.getInt64Ty(), "sext");
    else
      res = builder.CreateZExt(truncated, builder.getInt64Ty(), "zext");
    break;
  }
  case JitOp::SSHRSHN: {
    auto *b =
        builder.CreateLoad(builder.getInt64Ty(), vregs[instr.src1], "load_b");
    auto *narrowed = builder.CreateTrunc(a, builder.getIntNTy(instr.src_bitwidth),
                                        "trunc_sshr");
    res = builder.CreateAShr(narrowed, b, "sshr");
    break;
  }
  case JitOp::NEG:
    res = builder.CreateNeg(a, "neg");
    break;
  case JitOp::AND_REDUCE: {
    uint64_t all_ones =
        (instr.bitwidth < 64) ? ((1ULL << instr.bitwidth) - 1) : ~0ULL;
    auto *mask = builder.getInt64(all_ones);
    auto *masked = builder.CreateAnd(a, mask, "and_reduce_masked");
    auto *cmp = builder.CreateICmpEQ(masked, mask, "and_reduce_cmp");
    res = builder.CreateZExt(cmp, builder.getInt64Ty(), "and_reduce_ext");
    break;
  }
  case JitOp::OR_REDUCE: {
    auto *cmp = builder.CreateICmpNE(a, builder.getInt64(0), "or_reduce_cmp");
    res = builder.CreateZExt(cmp, builder.getInt64Ty(), "or_reduce_ext");
    break;
  }
  case JitOp::XOR_REDUCE: {
    // CreateIntrinsic handles both declaration lookup and call emission in
    // one call. Avoids getOrInsertDeclaration (LLVM 20+ rename of
    // getDeclaration) which is not a member in LLVM 18.
    auto *count = builder.CreateIntrinsic(llvm::Intrinsic::ctpop,
                                          {builder.getInt64Ty()}, {a}, nullptr,
                                          "xor_reduce_pop");
    res = builder.CreateAnd(count, builder.getInt64(1), "xor_reduce_parity");
    break;
  }
  case JitOp::POPCOUNT: {
    res = builder.CreateIntrinsic(llvm::Intrinsic::ctpop,
                                  {builder.getInt64Ty()}, {a}, nullptr,
                                  "popcount");
    break;
  }
  default:
    return;
  }

  res = jit_mask_to_bw(builder, res, instr.bitwidth);
  builder.CreateStore(res, vregs[instr.dst], "store_bit");
}

#else
void compile_to_llvm_bit(const JitInstr & /*instr*/, ... /*args*/) {}
#endif

} // namespace ch::jit