// SPDX-License-Identifier: MIT
// CppHDL — LLVM IR lowering for arithmetic JitOps:
//   ADD, SUB, MUL, DIV, MOD
//
// Split out from src/jit/jit_compiler.cpp:compile_to_llvm() during Phase 1.
// DIV and MOD guard against divisor == 0 by substituting the dividend (MOD)
// or 0 (DIV). All results are masked to instr.bitwidth when bitwidth < 64.

#include "jit_llvm_helpers.h"

#include <vector>

namespace ch::jit {

#if defined(CH_JIT_ENABLED) && __has_include(<llvm/IR/LLVMContext.h>)

void compile_to_llvm_arith(const JitInstr &instr, llvm::IRBuilder<> &builder,
                           std::vector<llvm::Value *> &vregs) {
  auto *a =
      builder.CreateLoad(builder.getInt64Ty(), vregs[instr.src0], "load_a");
  auto *b =
      builder.CreateLoad(builder.getInt64Ty(), vregs[instr.src1], "load_b");
  llvm::Value *res = nullptr;

  switch (instr.op) {
  case JitOp::ADD:
    res = builder.CreateAdd(a, b, "add");
    break;
  case JitOp::SUB:
    res = builder.CreateSub(a, b, "sub");
    break;
  case JitOp::MUL:
    res = builder.CreateMul(a, b, "mul");
    break;
  case JitOp::MOD: {
    auto *is_zero =
        builder.CreateICmpEQ(b, builder.getInt64(0), "is_zero");
    auto *rem = builder.CreateURem(a, b, "mod");
    res = builder.CreateSelect(is_zero, a, rem, "mod_safe");
    break;
  }
  case JitOp::DIV: {
    auto *is_zero =
        builder.CreateICmpEQ(b, builder.getInt64(0), "is_zero");
    auto *quotient = builder.CreateUDiv(a, b, "div");
    res = builder.CreateSelect(is_zero, builder.getInt64(0), quotient, "div_safe");
    break;
  }
  default:
    return;
  }

  res = jit_mask_to_bw(builder, res, instr.bitwidth);
  builder.CreateStore(res, vregs[instr.dst], "store_arith");
}

#else
void compile_to_llvm_arith(const JitInstr &, llvm::IRBuilder<> &,
                           std::vector<llvm::Value *> &) {}
#endif

} // namespace ch::jit