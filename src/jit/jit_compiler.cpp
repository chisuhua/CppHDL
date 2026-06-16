#include "jit/jit_compiler.h"
#include "ast/ast_nodes.h"
#include "core/context.h"
#include "core/lnodeimpl.h"
#include <fstream>
#include <iostream>

#if defined(CH_JIT_ENABLED) && __has_include(<llvm/IR/LLVMContext.h>)
#include <llvm-c/Core.h>
#include <llvm-c/Target.h>
#include <llvm/ExecutionEngine/JITSymbol.h>
#include <llvm/ExecutionEngine/Orc/LLJIT.h>
#include <llvm/ExecutionEngine/Orc/ThreadSafeModule.h>
#include <llvm/IR/Constants.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/Instructions.h>
#include <llvm/IR/Intrinsics.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/Type.h>
#include <llvm/IR/Verifier.h>
#include <llvm/Support/Error.h>
#include <llvm/Support/TargetSelect.h>
#endif

namespace ch::jit {

JitResult generate_ir_lnode_io(ch::core::lnodeimpl *node,
                               JitBlock &block_comb,
                               VRegId &next_comb_vreg);

JitResult generate_ir_lnode_state(ch::core::lnodeimpl *node,
                                  JitBlock &block_seq,
                                  VRegId &next_seq_vreg);

JitResult generate_ir_lnode_logic(
    ch::core::lnodeimpl *node, ch::core::context *ctx, JitBlock &block_comb,
    VRegId &next_comb_vreg,
    std::unordered_set<uint32_t> &external_node_ids);

JitResult generate_ir_lnode_meta(ch::core::lnodeimpl *node,
                                 JitBlock &block_comb,
                                 VRegId &next_comb_vreg);

void compile_to_llvm_mem(const JitInstr &instr, llvm::IRBuilder<> &builder,
                         llvm::Value *data_buffer_ptr,
                         std::vector<llvm::Value *> &vregs);

void compile_to_llvm_const(const JitInstr &instr, llvm::IRBuilder<> &builder,
                           std::vector<llvm::Value *> &vregs);

void compile_to_llvm_arith(const JitInstr &instr, llvm::IRBuilder<> &builder,
                           std::vector<llvm::Value *> &vregs);

void compile_to_llvm_bit(const JitInstr &instr, llvm::IRBuilder<> &builder,
                         std::vector<llvm::Value *> &vregs);

void compile_to_llvm_compare(const JitInstr &instr, llvm::IRBuilder<> &builder,
                             std::vector<llvm::Value *> &vregs);

void compile_to_llvm_control(const JitInstr &instr, llvm::IRBuilder<> &builder,
                             llvm::Value *data_buffer_ptr,
                             std::vector<llvm::Value *> &vregs);

JitCompiler::JitCompiler()
    : available_(false), jit_session_(nullptr), compiled_func_(nullptr),
      compiled_comb_func_(nullptr), compiled_seq_func_(nullptr),
      last_ir_instr_count_(0), last_seq_ir_instr_count_(0),
      last_vreg_count_(0) {
#if defined(CH_JIT_ENABLED) && __has_include(<llvm/Support/TargetSelect.h>)
  LLVMInitializeNativeTarget();
  LLVMInitializeNativeAsmPrinter();
  LLVMInitializeNativeAsmParser();
  available_ = true;
#endif
}

JitCompiler::~JitCompiler() { clear(); }

JitCompileResult JitCompiler::compile(ch::core::context *ctx) {
  JitCompileResult result;

  if (!available_) {
    result.result = JitResult::LLVM_INIT_FAILED;
    result.error_msg = "LLVM not available";
    return result;
  }

  if (!ctx) {
    result.result = JitResult::IR_GENERATION_FAILED;
    result.error_msg = "null context";
    return result;
  }

  auto eval_list = ctx->get_eval_list();
  if (eval_list.empty()) {
    result.result = JitResult::SUCCESS;
    return result;
  }

  auto buf_result = allocate_buffer(ctx);
  if (buf_result != JitResult::SUCCESS) {
    result.result = buf_result;
    return result;
  }

  JitFunction func_comb("tick_comb");
  JitFunction func_seq("tick_seq");
  auto ir_result = generate_ir(ctx, func_comb, func_seq);
  if (ir_result != JitResult::SUCCESS) {
    result.result = ir_result;
    return result;
  }

  last_ir_instr_count_ =
      func_comb.blocks.empty()
          ? 0
          : static_cast<uint32_t>(func_comb.blocks[0].instrs.size());
  last_seq_ir_instr_count_ =
      func_seq.blocks.empty()
          ? 0
          : static_cast<uint32_t>(func_seq.blocks[0].instrs.size());
  last_vreg_count_ = func_comb.vreg_count + func_seq.vreg_count;

  result.ir_instr_count = last_ir_instr_count_;
  result.vreg_count = last_vreg_count_;
  result.result = compile_to_llvm(func_comb, func_seq);
  return result;
}

void JitCompiler::execute_tick() {
#if defined(CH_JIT_ENABLED) && __has_include(<llvm/IR/LLVMContext.h>)
  if (!compiled_func_) {
    return;
  }

  using TickFunc = void (*)(uint64_t *);
  auto tick_func = reinterpret_cast<TickFunc>(compiled_func_);
  tick_func(data_buffer_.data());
#endif
}

void JitCompiler::execute_comb_tick() {
#if defined(CH_JIT_ENABLED) && __has_include(<llvm/IR/LLVMContext.h>)
  if (!compiled_comb_func_) {
    return;
  }

  using CombTickFunc = void (*)(uint64_t *);
  auto tick_func = reinterpret_cast<CombTickFunc>(compiled_comb_func_);
  tick_func(data_buffer_.data());
#endif
}

void JitCompiler::execute_seq_tick() {
#if defined(CH_JIT_ENABLED) && __has_include(<llvm/IR/LLVMContext.h>)
  if (!compiled_seq_func_) {
    return;
  }

  using SeqTickFunc = void (*)(uint64_t *);
  auto tick_func = reinterpret_cast<SeqTickFunc>(compiled_seq_func_);
  tick_func(data_buffer_.data());
#endif
}

void JitCompiler::clear() {
  if (compiled_func_) {
    compiled_func_ = nullptr;
  }
  if (compiled_comb_func_) {
    compiled_comb_func_ = nullptr;
  }
  if (compiled_seq_func_) {
    compiled_seq_func_ = nullptr;
  }
#if defined(CH_JIT_ENABLED) && __has_include(<llvm/ExecutionEngine/Orc/LLJIT.h>)
  if (jit_session_) {
    delete static_cast<llvm::orc::LLJIT *>(jit_session_);
    jit_session_ = nullptr;
  }
#else
  if (jit_session_) {
    jit_session_ = nullptr;
  }
#endif
  data_buffer_.clear();
  external_node_ids_.clear();
}

JitResult JitCompiler::allocate_buffer(ch::core::context *ctx) {
  if (!ctx) {
    return JitResult::IR_GENERATION_FAILED;
  }

  auto eval_list = ctx->get_eval_list();
  if (eval_list.empty()) {
    return JitResult::SUCCESS;
  }

  uint32_t max_node_id = 0;
  for (auto *node : eval_list) {
    if (node->id() > max_node_id) {
      max_node_id = node->id();
    }
    if (node->size() > 64) {
      return JitResult::UNSUPPORTED_OP;
    }
  }

  data_buffer_.resize(max_node_id + 1, 0);
  return JitResult::SUCCESS;
}

void JitCompiler::sync_to_buffer(const ch::data_map_t &data_map) {
  if (data_buffer_.empty()) {
    return;
  }
  for (const auto &[node_id, value] : data_map) {
    if (node_id < data_buffer_.size()) {
      data_buffer_[node_id] = static_cast<uint64_t>(value);
    }
  }
}

void JitCompiler::sync_from_buffer(ch::data_map_t &data_map) {
  if (data_buffer_.empty()) {
    return;
  }
  for (size_t i = 0; i < data_buffer_.size(); ++i) {
    auto it = data_map.find(static_cast<uint32_t>(i));
    if (it != data_map.end()) {
      it->second = ch::core::sdata_type(data_buffer_[i], it->second.bitwidth());
    }
  }
}

bool JitCompiler::is_external_node(uint32_t node_id) const {
  return external_node_ids_.count(node_id) > 0;
}

JitResult JitCompiler::generate_ir(ch::core::context *ctx,
                                   JitFunction &func_comb,
                                   JitFunction &func_seq) {
  if (!ctx) {
    return JitResult::IR_GENERATION_FAILED;
  }

  auto eval_list = ctx->get_eval_list();
  if (eval_list.empty()) {
    return JitResult::SUCCESS;
  }

  // W2 (perf-report-followup.md): small graphs lose to interpreter because
  // LLVM ORC JIT setup (~10-50 ms cold) exceeds savings. Return
  // UNSUPPORTED_OP so the caller falls back to the interpreter path
  // instead of paying the compilation cost.
  if (eval_list.size() < JIT_MIN_NODES) {
    last_error_msg_ = "graph too small for JIT ("
                      + std::to_string(eval_list.size())
                      + " nodes < JIT_MIN_NODES)";
    return JitResult::UNSUPPORTED_OP;
  }

  JitBlock block_comb("combinational");
  JitBlock block_seq("sequential");
  VRegId next_comb_vreg = 0;
  VRegId next_seq_vreg = 0;

  for (auto *node : eval_list) {
    auto node_type = node->type();
    JitResult r = JitResult::SUCCESS;
    bool count_comb = false;

    switch (node_type) {
    case ch::core::lnodetype::type_input:
    case ch::core::lnodetype::type_output:
    case ch::core::lnodetype::type_proxy:
      r = generate_ir_lnode_io(node, block_comb, next_comb_vreg);
      count_comb = true;
      break;

    case ch::core::lnodetype::type_op:
    case ch::core::lnodetype::type_mux:
    case ch::core::lnodetype::type_bitsupdate:
      r = generate_ir_lnode_logic(node, ctx, block_comb, next_comb_vreg,
                                  external_node_ids_);
      count_comb = true;
      break;

    case ch::core::lnodetype::type_reg:
    case ch::core::lnodetype::type_mem:
    case ch::core::lnodetype::type_mem_read_port:
    case ch::core::lnodetype::type_mem_write_port:
      r = generate_ir_lnode_state(node, block_seq, next_seq_vreg);
      break;

    case ch::core::lnodetype::type_clock:
    case ch::core::lnodetype::type_reset:
    case ch::core::lnodetype::type_none:
      r = generate_ir_lnode_meta(node, block_comb, next_comb_vreg);
      count_comb = true;
      break;

    case ch::core::lnodetype::type_lit:
      r = generate_ir_lnode_meta(node, block_comb, next_comb_vreg);
      break;
    }

    if (r != JitResult::SUCCESS) {
      return r;
    }
    if (count_comb) {
      block_comb.node_count++;
    }
  }

  func_comb.vreg_count = next_comb_vreg;
  func_comb.blocks.push_back(std::move(block_comb));
  func_comb.node_count = static_cast<uint32_t>(eval_list.size());

  func_seq.vreg_count = next_seq_vreg;
  func_seq.blocks.push_back(std::move(block_seq));

  return JitResult::SUCCESS;
}

JitResult JitCompiler::compile_to_llvm(const JitFunction &func_comb,
                                       const JitFunction &func_seq) {
#if defined(CH_JIT_ENABLED) && __has_include(<llvm/IR/LLVMContext.h>)
  auto *context = new llvm::LLVMContext();
  std::string module_name = "cpphdl_jit_dual";
  auto *module = new llvm::Module(module_name, *context);
  llvm::IRBuilder<> builder(*context);

  auto *ptr_ty = builder.getInt64Ty()->getPointerTo();
  auto *func_ty = llvm::FunctionType::get(builder.getVoidTy(), ptr_ty, false);

  auto compile_single_func = [&](const JitFunction &func, void *&out_func_ptr) {
    // Always emit the tick function (even as an empty no-op) so that the
    // subsequent JIT->lookup() call has a resolvable symbol.  Without this, a
    // purely combinational design would leave tick_seq undefined and the JIT
    // would fail to compile even though there is nothing wrong with the IR.
    auto *tick_func = llvm::Function::Create(
        func_ty, llvm::Function::ExternalLinkage, func.name, module);
    tick_func->arg_begin()->setName("data_buffer");

    auto *entry = llvm::BasicBlock::Create(*context, "entry", tick_func);
    builder.SetInsertPoint(entry);

    if (func.blocks.empty() || func.blocks[0].instrs.empty()) {
      builder.CreateRetVoid();
      out_func_ptr = nullptr;
      return JitResult::SUCCESS;
    }

    std::vector<llvm::Value *> vregs;
    if (func.vreg_count > 0) {
      vregs.resize(func.vreg_count, nullptr);
      for (uint32_t i = 0; i < func.vreg_count; ++i) {
        vregs[i] = builder.CreateAlloca(builder.getInt64Ty(), nullptr,
                                        "vreg_" + std::to_string(i));
      }
    }

    llvm::Argument *data_buffer_arg = &*tick_func->arg_begin();
    llvm::Value *data_buffer_ptr = data_buffer_arg;

    for (const auto &instr : func.blocks[0].instrs) {
      switch (instr.op) {
      case JitOp::LOAD_DATA:
      case JitOp::STORE_DATA:
        compile_to_llvm_mem(instr, builder, data_buffer_ptr, vregs);
        break;

      case JitOp::LOAD_CONST:
        compile_to_llvm_const(instr, builder, vregs);
        break;

      case JitOp::ADD:
      case JitOp::SUB:
      case JitOp::MUL:
      case JitOp::MOD:
      case JitOp::DIV:
        compile_to_llvm_arith(instr, builder, vregs);
        break;

      case JitOp::AND:
      case JitOp::OR:
      case JitOp::XOR:
      case JitOp::NOT:
      case JitOp::SHIFT_LEFT:
      case JitOp::SHIFT_RIGHT:
      case JitOp::ROTATE_LEFT:
      case JitOp::ROTATE_RIGHT:
      case JitOp::SEXT:
      case JitOp::ZEXT:
      case JitOp::SSHRSHN:
      case JitOp::NEG:
      case JitOp::AND_REDUCE:
      case JitOp::OR_REDUCE:
      case JitOp::XOR_REDUCE:
      case JitOp::POPCOUNT:
        compile_to_llvm_bit(instr, builder, vregs);
        break;

      case JitOp::EQ:
      case JitOp::NE:
      case JitOp::LT:
      case JitOp::LE:
      case JitOp::GT:
      case JitOp::GE:
        compile_to_llvm_compare(instr, builder, vregs);
        break;

      case JitOp::SELECT:
      case JitOp::REG_NEXT:
      case JitOp::BITS_UPDATE:
      case JitOp::BIT_SELECT:
      case JitOp::CONCAT:
      case JitOp::BITS_EXTRACT:
      case JitOp::CALL_EXTERNAL:
      case JitOp::ASSIGN:
        compile_to_llvm_control(instr, builder, data_buffer_ptr, vregs);
        break;

      case JitOp::NOP:
      default:
        break;
      }
    }

    std::string ir_str;
    llvm::raw_string_ostream ir_stream(ir_str);
    module->print(ir_stream, nullptr);
    std::ofstream ir_file("/tmp/jit_ir.ll");
    ir_file << ir_str;
    ir_file.close();
    builder.CreateRetVoid();
    return JitResult::SUCCESS;
  };

  auto result_comb = compile_single_func(func_comb, compiled_comb_func_);
  if (result_comb != JitResult::SUCCESS) {
    return result_comb;
  }

  auto result_seq = compile_single_func(func_seq, compiled_seq_func_);
  if (result_seq != JitResult::SUCCESS) {
    return result_seq;
  }

  llvm_module_ = module;
  return finalize_compilation_dual();
#else
  return JitResult::UNSUPPORTED_OP;
#endif
}

} // namespace ch::jit
