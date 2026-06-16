// SPDX-License-Identifier: MIT
// CppHDL — JIT finalization: move LLVM module into LLJIT and resolve symbols.
//
// Split out from src/jit/jit_compiler.cpp:finalize_compilation_dual() during
// Phase 1 of the C-class refactor. This is the only consumer of the
// llvm_module_ / compiled_comb_func_ / compiled_seq_func_ / jit_session_
// private fields.

#include "jit/jit_compiler.h"

#if defined(CH_JIT_ENABLED) && __has_include(<llvm/IR/LLVMContext.h>)
#include <llvm-c/Core.h>
#include <llvm/ExecutionEngine/Orc/LLJIT.h>
#include <llvm/ExecutionEngine/Orc/ThreadSafeModule.h>
#include <llvm/IR/Module.h>
#include <llvm/Support/Error.h>
#endif

namespace ch::jit {

JitResult JitCompiler::finalize_compilation_dual() {
#if defined(CH_JIT_ENABLED) && __has_include(<llvm/IR/LLVMContext.h>)
  if (!llvm_module_) {
    return JitResult::COMPILATION_FAILED;
  }

  auto *module = static_cast<llvm::Module *>(llvm_module_);
  auto *context = &module->getContext();

  llvm::orc::LLJITBuilder lljit_builder;
  auto JIT_or_err = lljit_builder.create();
  if (!JIT_or_err) {
    std::string err_msg;
    llvm::raw_string_ostream os(err_msg);
    os << JIT_or_err.takeError();
    last_error_msg_ = std::string("LLJIT create failed: ") + err_msg;
    return JitResult::COMPILATION_FAILED;
  }

  auto JIT = std::move(JIT_or_err.get());
  auto tsm =
      llvm::orc::ThreadSafeModule(std::unique_ptr<llvm::Module>(module),
                                  std::unique_ptr<llvm::LLVMContext>(context));
  if (auto Err = JIT->addIRModule(std::move(tsm))) {
    std::string err_msg;
    llvm::raw_string_ostream os(err_msg);
    os << Err;
    last_error_msg_ = std::string("addIRModule failed: ") + err_msg;
    return JitResult::COMPILATION_FAILED;
  }

  auto Sym_comb = JIT->lookup("tick_comb");
  if (!Sym_comb) {
    std::string err_msg;
    llvm::raw_string_ostream os(err_msg);
    os << Sym_comb.takeError();
    last_error_msg_ = std::string("lookup tick_comb failed: ") + err_msg;
    return JitResult::COMPILATION_FAILED;
  }
  compiled_comb_func_ = reinterpret_cast<void *>(Sym_comb->getValue());

  auto Sym_seq = JIT->lookup("tick_seq");
  if (!Sym_seq) {
    std::string err_msg;
    llvm::raw_string_ostream os(err_msg);
    os << Sym_seq.takeError();
    last_error_msg_ = std::string("lookup tick_seq failed: ") + err_msg;
    return JitResult::COMPILATION_FAILED;
  }
  compiled_seq_func_ = reinterpret_cast<void *>(Sym_seq->getValue());

  jit_session_ = static_cast<void *>(JIT.release());
  llvm_module_ = nullptr;
  return JitResult::SUCCESS;
#else
  return JitResult::UNSUPPORTED_OP;
#endif
}

} // namespace ch::jit