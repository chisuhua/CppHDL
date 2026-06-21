// SPDX-License-Identifier: MIT
// CppHDL — LLVM IR lowering for comparison JitOps:
//   EQ, NE, LT, LE, GT, GE
//
// Split out from src/jit/jit_compiler.cpp:compile_to_llvm() during Phase 1.
// All comparisons are unsigned (matches interpreter semantics for ch_uint<N>)
// and the resulting i1 is zero-extended to i64.

#include "jit_llvm_helpers.h"

#include <vector>

namespace ch::jit {

#if defined(CH_JIT_ENABLED) && __has_include(<llvm/IR/LLVMContext.h>)

void compile_to_llvm_compare(const JitInstr &instr, llvm::IRBuilder<> &builder,
                             std::vector<llvm::Value *> &vregs) {
  auto *a =
      builder.CreateLoad(builder.getInt64Ty(), vregs[instr.src0], "load_a");
  auto *b =
      builder.CreateLoad(builder.getInt64Ty(), vregs[instr.src1], "load_b");
  llvm::Value *cmp = nullptr;

  switch (instr.op) {
  case JitOp::EQ: cmp = builder.CreateICmpEQ(a, b, "eq"); break;
  case JitOp::NE: cmp = builder.CreateICmpNE(a, b, "ne"); break;
  case JitOp::LT: cmp = builder.CreateICmpULT(a, b, "lt"); break;
  case JitOp::LE: cmp = builder.CreateICmpULE(a, b, "le"); break;
  case JitOp::GT: cmp = builder.CreateICmpUGT(a, b, "gt"); break;
  case JitOp::GE: cmp = builder.CreateICmpUGE(a, b, "ge"); break;
  default: return;
  }

  auto *res = builder.CreateZExt(cmp, builder.getInt64Ty(), "zext_cmp");
  builder.CreateStore(res, vregs[instr.dst], "store_cmp");
}

#else
void compile_to_llvm_compare(const JitInstr & /*instr*/, ... /*args*/) {}
#endif

} // namespace ch::jit