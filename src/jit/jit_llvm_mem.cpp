// SPDX-License-Identifier: MIT
// CppHDL — LLVM IR lowering for memory access JitOps:
//   JitOp::LOAD_DATA, JitOp::STORE_DATA
//
// Split out from src/jit/jit_compiler.cpp:compile_to_llvm() during Phase 1.

#include "jit_llvm_helpers.h"

namespace ch::jit {

#if defined(CH_JIT_ENABLED) && __has_include(<llvm/IR/LLVMContext.h>)

// Lower JitOp::LOAD_DATA: read i64 from data_buffer[node_id], mask to bitwidth,
// store into vreg[dst].
void compile_to_llvm_load_data(const JitInstr &instr, llvm::IRBuilder<> &builder,
                               llvm::Value *data_buffer_ptr,
                               std::vector<llvm::Value *> &vregs) {
  auto *val = jit_load_node(builder, data_buffer_ptr, instr.node_id,
                            instr.bitwidth);
  builder.CreateStore(val, vregs[instr.dst], "store_load");
}

// Lower JitOp::STORE_DATA: load vreg[src0] (i64), mask to bitwidth, write to
// data_buffer[node_id]. Skips when src0 is INVALID_VREG (no update).
void compile_to_llvm_store_data(const JitInstr &instr, llvm::IRBuilder<> &builder,
                                llvm::Value *data_buffer_ptr,
                                std::vector<llvm::Value *> &vregs) {
  if (instr.src0 == INVALID_VREG) {
    return;
  }
  auto *val =
      builder.CreateLoad(builder.getInt64Ty(), vregs[instr.src0], "load_src");
  jit_store_node(builder, data_buffer_ptr, instr.node_id, val, instr.bitwidth);
}

// Dispatch for the mem category.
void compile_to_llvm_mem(const JitInstr &instr, llvm::IRBuilder<> &builder,
                         llvm::Value *data_buffer_ptr,
                         std::vector<llvm::Value *> &vregs) {
  switch (instr.op) {
  case JitOp::LOAD_DATA:
    compile_to_llvm_load_data(instr, builder, data_buffer_ptr, vregs);
    break;
  case JitOp::STORE_DATA:
    compile_to_llvm_store_data(instr, builder, data_buffer_ptr, vregs);
    break;
  default:
    break;
  }
}

#else // !CH_JIT_ENABLED

void compile_to_llvm_mem(const JitInstr & /*instr*/, ... /*args*/) {}

#endif

} // namespace ch::jit