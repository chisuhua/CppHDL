// SPDX-License-Identifier: MIT
// Internal header for the split compile_to_llvm() per-JitOp-category helpers.
// Provides shared LLVM IR building helpers (get_node_ptr / load_node /
// store_node) so each jit_llvm_*.cpp file doesn't have to redefine them.

#pragma once

#include "jit/jit_compiler.h"

#include <cstdint>
#include <vector>

#if defined(CH_JIT_ENABLED) && __has_include(<llvm/IR/LLVMContext.h>)
#include <llvm/IR/IRBuilder.h>
#endif

namespace ch::jit {

#if defined(CH_JIT_ENABLED) && __has_include(<llvm/IR/LLVMContext.h>)

// Returns i64* pointer into data_buffer at the given node_id index.
inline llvm::Value *jit_get_node_ptr(llvm::IRBuilder<> &builder,
                                     llvm::Value *data_buffer_ptr,
                                     uint32_t node_id) {
  auto *indices = llvm::ConstantInt::get(builder.getInt32Ty(), node_id);
  return builder.CreateGEP(builder.getInt64Ty(), data_buffer_ptr, indices,
                           "node_ptr");
}

// Load i64 from data_buffer[node_id], masked to bitwidth if bitwidth < 64.
inline llvm::Value *jit_load_node(llvm::IRBuilder<> &builder,
                                  llvm::Value *data_buffer_ptr,
                                  uint32_t node_id, BitWidth bw) {
  auto *ptr = jit_get_node_ptr(builder, data_buffer_ptr, node_id);
  auto *val = builder.CreateLoad(builder.getInt64Ty(), ptr, "load_node");
  if (bw < 64) {
    uint64_t mask = (1ULL << bw) - 1;
    return builder.CreateAnd(val, builder.getInt64(mask), "mask_load");
  }
  return val;
}

// Store val to data_buffer[node_id], masked to bitwidth if bitwidth < 64.
inline void jit_store_node(llvm::IRBuilder<> &builder,
                           llvm::Value *data_buffer_ptr, uint32_t node_id,
                           llvm::Value *val, BitWidth bw) {
  auto *ptr = jit_get_node_ptr(builder, data_buffer_ptr, node_id);
  llvm::Value *store_val = val;
  if (bw < 64) {
    uint64_t mask = (1ULL << bw) - 1;
    store_val =
        builder.CreateAnd(store_val, builder.getInt64(mask), "mask_store");
  }
  builder.CreateStore(store_val, ptr);
}

// Apply the standard <64-bit mask for arithmetic / shift / bitwise results.
// NO-OP if instr.bitwidth >= 64 (UB territory for `(1ULL << bw) - 1`).
inline llvm::Value *jit_mask_to_bw(llvm::IRBuilder<> &builder,
                                   llvm::Value *val, BitWidth bw) {
  if (bw < 64) {
    uint64_t mask = (1ULL << bw) - 1;
    return builder.CreateAnd(val, builder.getInt64(mask), "mask");
  }
  return val;
}

#endif // CH_JIT_ENABLED

} // namespace ch::jit