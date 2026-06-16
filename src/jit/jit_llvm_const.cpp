// SPDX-License-Identifier: MIT
// CppHDL — LLVM IR lowering for compile-time constants:
//   JitOp::LOAD_CONST
//
// Split out from src/jit/jit_compiler.cpp:compile_to_llvm() during Phase 1.
// For narrow bitwidths, embeds as iN then zero-extends to i64; for 64-bit
// uses an i64 constant directly.

#include "jit_llvm_helpers.h"

#include <vector>

namespace ch::jit {

#if defined(CH_JIT_ENABLED) && __has_include(<llvm/IR/LLVMContext.h>)

void compile_to_llvm_load_const(const JitInstr &instr, llvm::IRBuilder<> &builder,
                                std::vector<llvm::Value *> &vregs) {
  llvm::Value *const_val;
  if (instr.bitwidth < 64) {
    const_val = builder.getIntN(instr.bitwidth, instr.imm.value);
    const_val =
        builder.CreateZExt(const_val, builder.getInt64Ty(), "zext_const");
  } else {
    const_val = builder.getInt64(instr.imm.value);
  }
  builder.CreateStore(const_val, vregs[instr.dst], "store_const");
}

void compile_to_llvm_const(const JitInstr &instr, llvm::IRBuilder<> &builder,
                           std::vector<llvm::Value *> &vregs) {
  if (instr.op == JitOp::LOAD_CONST) {
    compile_to_llvm_load_const(instr, builder, vregs);
  }
}

#else
void compile_to_llvm_const(const JitInstr &, llvm::IRBuilder<> &,
                           std::vector<llvm::Value *> &) {}
#endif

} // namespace ch::jit