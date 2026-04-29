#include "jit/jit_compiler.h"
#include "core/context.h"
#include "core/lnodeimpl.h"
#include "ast/ast_nodes.h"
#include <iostream>

#if defined(CH_JIT_ENABLED) && __has_include(<llvm/IR/LLVMContext.h>)
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/Verifier.h>
#include <llvm/IR/Type.h>
#include <llvm/IR/Constants.h>
#include <llvm/IR/Instructions.h>
#include <llvm/ExecutionEngine/Orc/LLJIT.h>
#include <llvm/ExecutionEngine/Orc/ThreadSafeModule.h>
#include <llvm/ExecutionEngine/JITSymbol.h>
#include <llvm/Support/TargetSelect.h>
#include <llvm/Support/Error.h>
#include <llvm-c/Target.h>
#include <llvm-c/Core.h>
#endif

namespace ch::jit {

JitCompiler::JitCompiler()
    : available_(false), jit_session_(nullptr), compiled_func_(nullptr),
      last_ir_instr_count_(0), last_vreg_count_(0) {
#if defined(CH_JIT_ENABLED) && __has_include(<llvm/Support/TargetSelect.h>)
    LLVMInitializeNativeTarget();
    LLVMInitializeNativeAsmPrinter();
    LLVMInitializeNativeAsmParser();
    available_ = true;
#endif
}

JitCompiler::~JitCompiler() {
    clear();
}

JitCompileResult JitCompiler::compile(ch::core::context* ctx) {
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

    JitFunction func("tick");
    auto ir_result = generate_ir(ctx, func);
    if (ir_result != JitResult::SUCCESS) {
        result.result = ir_result;
        return result;
    }

    last_ir_instr_count_ = func.blocks.empty() ? 0 :
        static_cast<uint32_t>(func.blocks[0].instrs.size());
    for (size_t i = 1; i < func.blocks.size(); ++i) {
        last_ir_instr_count_ += static_cast<uint32_t>(func.blocks[i].instrs.size());
    }
    last_vreg_count_ = func.vreg_count;

    result.ir_instr_count = last_ir_instr_count_;
    result.vreg_count = last_vreg_count_;
    result.result = compile_to_llvm(func);
    return result;
}

void JitCompiler::execute_tick() {
#if defined(CH_JIT_ENABLED) && __has_include(<llvm/IR/LLVMContext.h>)
    if (!compiled_func_) {
        return;
    }

    using TickFunc = void(*)(uint64_t*);
    auto tick_func = reinterpret_cast<TickFunc>(compiled_func_);
    tick_func(data_buffer_.data());
#endif
}

void JitCompiler::clear() {
    if (compiled_func_) {
        compiled_func_ = nullptr;
    }
#if defined(CH_JIT_ENABLED) && __has_include(<llvm/ExecutionEngine/Orc/LLJIT.h>)
    if (jit_session_) {
        delete static_cast<llvm::orc::LLJIT*>(jit_session_);
        jit_session_ = nullptr;
    }
#else
    if (jit_session_) {
        jit_session_ = nullptr;
    }
#endif
    data_buffer_.clear();
}

JitResult JitCompiler::allocate_buffer(ch::core::context* ctx) {
    if (!ctx) {
        return JitResult::IR_GENERATION_FAILED;
    }

    auto eval_list = ctx->get_eval_list();
    if (eval_list.empty()) {
        return JitResult::SUCCESS;
    }

    uint32_t max_node_id = 0;
    for (auto* node : eval_list) {
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

void JitCompiler::sync_to_buffer(const ch::data_map_t& data_map) {
    if (data_buffer_.empty()) {
        return;
    }
    for (const auto& [node_id, value] : data_map) {
        if (node_id < data_buffer_.size()) {
            data_buffer_[node_id] = static_cast<uint64_t>(value);
        }
    }
}

void JitCompiler::sync_from_buffer(ch::data_map_t& data_map) {
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

JitResult JitCompiler::generate_ir(ch::core::context* ctx, JitFunction& func) {
    if (!ctx) {
        return JitResult::IR_GENERATION_FAILED;
    }

    auto eval_list = ctx->get_eval_list();
    if (eval_list.empty()) {
        return JitResult::SUCCESS;
    }

    JitBlock block("combinational");
    VRegId next_vreg = 0;

    for (auto* node : eval_list) {
        auto node_type = node->type();
        auto node_id = node->id();
        auto bitwidth = static_cast<BitWidth>(node->size());

        // 时序节点由解释器处理，JIT 只编译组合逻辑
        if (node_type == ch::core::lnodetype::type_reg ||
            node_type == ch::core::lnodetype::type_mem ||
            node_type == ch::core::lnodetype::type_mem_read_port ||
            node_type == ch::core::lnodetype::type_mem_write_port) {
            continue;
        }

        switch (node_type) {
        case ch::core::lnodetype::type_lit: {
            auto* lit = static_cast<ch::core::litimpl*>(node);
            auto value = static_cast<uint64_t>(lit->value());
            auto vreg = next_vreg++;
            block.instrs.push_back(make_const(vreg, value, bitwidth));
            block.instrs.push_back(make_store(node_id, vreg, bitwidth));
            break;
        }

        case ch::core::lnodetype::type_input: {
            auto vreg = next_vreg++;
            block.instrs.push_back(make_load(vreg, node_id, bitwidth));
            auto store_instr = make_store(node_id, vreg, bitwidth);
            block.instrs.push_back(store_instr);
            break;
        }

        case ch::core::lnodetype::type_op: {
            auto* op_node = static_cast<ch::core::opimpl*>(node);
            auto ch_op = op_node->op();

            auto src0_vreg = next_vreg++;
            auto src1_vreg = next_vreg++;
            auto dst_vreg = next_vreg++;

            if (node->num_srcs() > 0 && node->src(0)) {
                block.instrs.push_back(make_load(src0_vreg, node->src(0)->id(), bitwidth));
            }
            if (node->num_srcs() > 1 && node->src(1)) {
                block.instrs.push_back(make_load(src1_vreg, node->src(1)->id(), bitwidth));
            }

            JitOp jit_op;
            switch (ch_op) {
            case ch::core::ch_op::add: jit_op = JitOp::ADD; break;
            case ch::core::ch_op::sub: jit_op = JitOp::SUB; break;
            case ch::core::ch_op::mul: jit_op = JitOp::MUL; break;
            case ch::core::ch_op::and_: jit_op = JitOp::AND; break;
            case ch::core::ch_op::or_: jit_op = JitOp::OR; break;
            case ch::core::ch_op::xor_: jit_op = JitOp::XOR; break;
            case ch::core::ch_op::not_: jit_op = JitOp::NOT; break;
            case ch::core::ch_op::eq: jit_op = JitOp::EQ; break;
            case ch::core::ch_op::ne: jit_op = JitOp::NE; break;
            case ch::core::ch_op::lt: jit_op = JitOp::LT; break;
            case ch::core::ch_op::le: jit_op = JitOp::LE; break;
            case ch::core::ch_op::gt: jit_op = JitOp::GT; break;
            case ch::core::ch_op::ge: jit_op = JitOp::GE; break;
            case ch::core::ch_op::shl: jit_op = JitOp::SHIFT_LEFT; break;
            case ch::core::ch_op::shr: jit_op = JitOp::SHIFT_RIGHT; break;
            case ch::core::ch_op::concat: jit_op = JitOp::CONCAT; break;
            default: jit_op = JitOp::CALL_EXTERNAL; break;
            }

            if (jit_op != JitOp::CALL_EXTERNAL) {
                block.instrs.push_back(make_binary(jit_op, dst_vreg, src0_vreg, src1_vreg, bitwidth));
            } else {
                auto load_instr = make_load(dst_vreg, node_id, bitwidth);
                load_instr.op = JitOp::CALL_EXTERNAL;
                block.instrs.push_back(load_instr);
            }
            block.instrs.push_back(make_store(node_id, dst_vreg, bitwidth));
            break;
        }

        case ch::core::lnodetype::type_mux: {
            auto cond_vreg = next_vreg++;
            auto src0_vreg = next_vreg++;
            auto src1_vreg = next_vreg++;
            auto dst_vreg = next_vreg++;

            if (node->num_srcs() >= 1 && node->src(0)) {
                block.instrs.push_back(make_load(cond_vreg, node->src(0)->id(), 1));
            }
            if (node->num_srcs() >= 2 && node->src(1)) {
                block.instrs.push_back(make_load(src0_vreg, node->src(1)->id(), bitwidth));
            }
            if (node->num_srcs() >= 3 && node->src(2)) {
                block.instrs.push_back(make_load(src1_vreg, node->src(2)->id(), bitwidth));
            }

            block.instrs.push_back(make_select(dst_vreg, cond_vreg, src0_vreg, src1_vreg, bitwidth));
            block.instrs.push_back(make_store(node_id, dst_vreg, bitwidth));
            break;
        }

        case ch::core::lnodetype::type_proxy: {
            if (node->num_srcs() > 0 && node->src(0)) {
                auto src_vreg = next_vreg++;
                auto dst_vreg = next_vreg++;
                block.instrs.push_back(make_load(src_vreg, node->src(0)->id(), bitwidth));
                block.instrs.push_back(make_store(node_id, src_vreg, bitwidth));
            }
            break;
        }

        case ch::core::lnodetype::type_clock:
        case ch::core::lnodetype::type_reset:
        case ch::core::lnodetype::type_output:
        case ch::core::lnodetype::type_mem:
        case ch::core::lnodetype::type_bitsupdate:
        default:
            break;
        }

        block.node_count++;
    }

    func.vreg_count = next_vreg;
    func.blocks.push_back(std::move(block));
    func.node_count = static_cast<uint32_t>(eval_list.size());

    return JitResult::SUCCESS;
}

JitResult JitCompiler::compile_to_llvm(const JitFunction& func) {
#if defined(CH_JIT_ENABLED) && __has_include(<llvm/IR/LLVMContext.h>)
    if (func.blocks.empty() || func.blocks[0].instrs.empty()) {
        return JitResult::SUCCESS;
    }

    for (const auto& instr : func.blocks[0].instrs) {
        if (instr.op == JitOp::CALL_EXTERNAL ||
            instr.op == JitOp::CONCAT ||
            instr.op == JitOp::SLICE ||
            instr.op == JitOp::MEM_READ ||
            instr.op == JitOp::MEM_WRITE ||
            instr.op == JitOp::JUMP ||
            instr.op == JitOp::BRANCH ||
            instr.op == JitOp::LABEL) {
            return JitResult::UNSUPPORTED_OP;
        }
    }

    auto* context = new llvm::LLVMContext();
    std::string module_name = "cpphdl_jit_" + func.name;
    auto* module = new llvm::Module(module_name, *context);
    llvm::IRBuilder<> builder(*context);

    auto* ptr_ty = builder.getInt64Ty()->getPointerTo();
    auto* func_ty = llvm::FunctionType::get(builder.getVoidTy(), ptr_ty, false);

    auto* tick_func = llvm::Function::Create(
        func_ty, llvm::Function::ExternalLinkage, func.name, module);
    tick_func->arg_begin()->setName("data_buffer");

    auto* entry = llvm::BasicBlock::Create(*context, "entry", tick_func);
    builder.SetInsertPoint(entry);

    std::vector<llvm::Value*> vregs;
    if (func.vreg_count > 0) {
        vregs.resize(func.vreg_count, nullptr);
        for (uint32_t i = 0; i < func.vreg_count; ++i) {
            vregs[i] = builder.CreateAlloca(builder.getInt64Ty(), nullptr, "vreg_" + std::to_string(i));
        }
    }

    llvm::Argument* data_buffer_arg = &*tick_func->arg_begin();
    llvm::Value* data_buffer_ptr = data_buffer_arg;

    // Helper lambdas
    auto get_node_ptr = [&](uint32_t node_id) -> llvm::Value* {
        auto* indices = llvm::ConstantInt::get(builder.getInt32Ty(), node_id);
        return builder.CreateGEP(builder.getInt64Ty(), data_buffer_ptr, indices, "node_ptr");
    };

    auto load_node = [&](uint32_t node_id, BitWidth bw) -> llvm::Value* {
        auto* ptr = get_node_ptr(node_id);
        auto* val = builder.CreateLoad(builder.getInt64Ty(), ptr, "load_node");
        if (bw < 64) {
            return builder.CreateTrunc(val, builder.getIntNTy(bw), "trunc");
        }
        return val;
    };

    auto store_node = [&](uint32_t node_id, llvm::Value* val, BitWidth bw) {
        auto* ptr = get_node_ptr(node_id);
        llvm::Value* store_val = val;
        if (bw < 64) {
            store_val = builder.CreateZExt(store_val, builder.getInt64Ty(), "zext");
        }
        builder.CreateStore(store_val, ptr);
    };

    // Generate IR for each instruction
    for (const auto& instr : func.blocks[0].instrs) {
        switch (instr.op) {
        case JitOp::LOAD_CONST: {
            llvm::Value* const_val;
            if (instr.bitwidth < 64) {
                const_val = builder.getIntN(instr.bitwidth, instr.imm.value);
                const_val = builder.CreateZExt(const_val, builder.getInt64Ty(), "zext_const");
            } else {
                const_val = builder.getInt64(instr.imm.value);
            }
            builder.CreateStore(const_val, vregs[instr.dst], "store_const");
            break;
        }

        case JitOp::LOAD_DATA: {
            auto* val = load_node(instr.node_id, instr.bitwidth);
            builder.CreateStore(val, vregs[instr.dst], "store_load");
            break;
        }

        case JitOp::STORE_DATA: {
            if (instr.src0 == INVALID_VREG) {
                break;  // Skip invalid store
            }
            auto* val = builder.CreateLoad(builder.getInt64Ty(), vregs[instr.src0], "load_src");
            store_node(instr.node_id, val, instr.bitwidth);
            break;
        }

        case JitOp::ADD: {
            auto* a = builder.CreateLoad(builder.getInt64Ty(), vregs[instr.src0], "load_a");
            auto* b = builder.CreateLoad(builder.getInt64Ty(), vregs[instr.src1], "load_b");
            auto* res = builder.CreateAdd(a, b, "add");
            builder.CreateStore(res, vregs[instr.dst], "store_add");
            break;
        }

        case JitOp::SUB: {
            auto* a = builder.CreateLoad(builder.getInt64Ty(), vregs[instr.src0], "load_a");
            auto* b = builder.CreateLoad(builder.getInt64Ty(), vregs[instr.src1], "load_b");
            auto* res = builder.CreateSub(a, b, "sub");
            builder.CreateStore(res, vregs[instr.dst], "store_sub");
            break;
        }

        case JitOp::MUL: {
            auto* a = builder.CreateLoad(builder.getInt64Ty(), vregs[instr.src0], "load_a");
            auto* b = builder.CreateLoad(builder.getInt64Ty(), vregs[instr.src1], "load_b");
            auto* res = builder.CreateMul(a, b, "mul");
            builder.CreateStore(res, vregs[instr.dst], "store_mul");
            break;
        }

        case JitOp::AND: {
            auto* a = builder.CreateLoad(builder.getInt64Ty(), vregs[instr.src0], "load_a");
            auto* b = builder.CreateLoad(builder.getInt64Ty(), vregs[instr.src1], "load_b");
            auto* res = builder.CreateAnd(a, b, "and");
            builder.CreateStore(res, vregs[instr.dst], "store_and");
            break;
        }

        case JitOp::OR: {
            auto* a = builder.CreateLoad(builder.getInt64Ty(), vregs[instr.src0], "load_a");
            auto* b = builder.CreateLoad(builder.getInt64Ty(), vregs[instr.src1], "load_b");
            auto* res = builder.CreateOr(a, b, "or");
            builder.CreateStore(res, vregs[instr.dst], "store_or");
            break;
        }

        case JitOp::XOR: {
            auto* a = builder.CreateLoad(builder.getInt64Ty(), vregs[instr.src0], "load_a");
            auto* b = builder.CreateLoad(builder.getInt64Ty(), vregs[instr.src1], "load_b");
            auto* res = builder.CreateXor(a, b, "xor");
            builder.CreateStore(res, vregs[instr.dst], "store_xor");
            break;
        }

        case JitOp::NOT: {
            auto* a = builder.CreateLoad(builder.getInt64Ty(), vregs[instr.src0], "load_a");
            auto* all1 = builder.getInt64(uint64_t(-1));
            auto* res = builder.CreateXor(a, all1, "not");
            builder.CreateStore(res, vregs[instr.dst], "store_not");
            break;
        }

        case JitOp::SHIFT_LEFT: {
            auto* a = builder.CreateLoad(builder.getInt64Ty(), vregs[instr.src0], "load_a");
            auto* b = builder.CreateLoad(builder.getInt64Ty(), vregs[instr.src1], "load_b");
            auto* res = builder.CreateShl(a, b, "shl");
            builder.CreateStore(res, vregs[instr.dst], "store_shl");
            break;
        }

        case JitOp::SHIFT_RIGHT: {
            auto* a = builder.CreateLoad(builder.getInt64Ty(), vregs[instr.src0], "load_a");
            auto* b = builder.CreateLoad(builder.getInt64Ty(), vregs[instr.src1], "load_b");
            auto* res = builder.CreateLShr(a, b, "shr");
            builder.CreateStore(res, vregs[instr.dst], "store_shr");
            break;
        }

        case JitOp::EQ: {
            auto* a = builder.CreateLoad(builder.getInt64Ty(), vregs[instr.src0], "load_a");
            auto* b = builder.CreateLoad(builder.getInt64Ty(), vregs[instr.src1], "load_b");
            auto* cmp = builder.CreateICmpEQ(a, b, "eq");
            auto* res = builder.CreateZExt(cmp, builder.getInt64Ty(), "zext_eq");
            builder.CreateStore(res, vregs[instr.dst], "store_eq");
            break;
        }

        case JitOp::NE: {
            auto* a = builder.CreateLoad(builder.getInt64Ty(), vregs[instr.src0], "load_a");
            auto* b = builder.CreateLoad(builder.getInt64Ty(), vregs[instr.src1], "load_b");
            auto* cmp = builder.CreateICmpNE(a, b, "ne");
            auto* res = builder.CreateZExt(cmp, builder.getInt64Ty(), "zext_ne");
            builder.CreateStore(res, vregs[instr.dst], "store_ne");
            break;
        }

        case JitOp::LT: {
            auto* a = builder.CreateLoad(builder.getInt64Ty(), vregs[instr.src0], "load_a");
            auto* b = builder.CreateLoad(builder.getInt64Ty(), vregs[instr.src1], "load_b");
            auto* cmp = builder.CreateICmpULT(a, b, "lt");
            auto* res = builder.CreateZExt(cmp, builder.getInt64Ty(), "zext_lt");
            builder.CreateStore(res, vregs[instr.dst], "store_lt");
            break;
        }

        case JitOp::LE: {
            auto* a = builder.CreateLoad(builder.getInt64Ty(), vregs[instr.src0], "load_a");
            auto* b = builder.CreateLoad(builder.getInt64Ty(), vregs[instr.src1], "load_b");
            auto* cmp = builder.CreateICmpULE(a, b, "le");
            auto* res = builder.CreateZExt(cmp, builder.getInt64Ty(), "zext_le");
            builder.CreateStore(res, vregs[instr.dst], "store_le");
            break;
        }

        case JitOp::GT: {
            auto* a = builder.CreateLoad(builder.getInt64Ty(), vregs[instr.src0], "load_a");
            auto* b = builder.CreateLoad(builder.getInt64Ty(), vregs[instr.src1], "load_b");
            auto* cmp = builder.CreateICmpUGT(a, b, "gt");
            auto* res = builder.CreateZExt(cmp, builder.getInt64Ty(), "zext_gt");
            builder.CreateStore(res, vregs[instr.dst], "store_gt");
            break;
        }

        case JitOp::GE: {
            auto* a = builder.CreateLoad(builder.getInt64Ty(), vregs[instr.src0], "load_a");
            auto* b = builder.CreateLoad(builder.getInt64Ty(), vregs[instr.src1], "load_b");
            auto* cmp = builder.CreateICmpUGE(a, b, "ge");
            auto* res = builder.CreateZExt(cmp, builder.getInt64Ty(), "zext_ge");
            builder.CreateStore(res, vregs[instr.dst], "store_ge");
            break;
        }

        case JitOp::SELECT: {
            auto* cond = builder.CreateLoad(builder.getInt64Ty(), vregs[instr.src0], "load_cond");
            auto* a = builder.CreateLoad(builder.getInt64Ty(), vregs[instr.src1], "load_a");
            auto* b = builder.CreateLoad(builder.getInt64Ty(), vregs[instr.src2], "load_b");
            auto* cmp = builder.CreateICmpNE(cond, builder.getInt64(0), "cond_nz");
            auto* res = builder.CreateSelect(cmp, a, b, "select");
            builder.CreateStore(res, vregs[instr.dst], "store_select");
            break;
        }

        case JitOp::REG_NEXT: {
            if (instr.src0 == INVALID_VREG) {
                break;  // Skip invalid reg_next
            }
            auto* val = builder.CreateLoad(builder.getInt64Ty(), vregs[instr.src0], "load_reg");
            store_node(instr.node_id, val, instr.bitwidth);
            break;
        }

        case JitOp::NOP:
        default:
            break;
        }
    }

    builder.CreateRetVoid();

    llvm_module_ = module;
    return finalize_compilation();
#else
    return JitResult::UNSUPPORTED_OP;
#endif
}

JitResult JitCompiler::finalize_compilation() {
#if defined(CH_JIT_ENABLED) && __has_include(<llvm/IR/LLVMContext.h>)
    if (!llvm_module_) {
        return JitResult::COMPILATION_FAILED;
    }

    auto* module = static_cast<llvm::Module*>(llvm_module_);
    auto* context = &module->getContext();

    llvm::orc::LLJITBuilder lljit_builder;
    auto JIT_or_err = lljit_builder.create();
    if (!JIT_or_err) {
        return JitResult::COMPILATION_FAILED;
    }

    auto JIT = std::move(JIT_or_err.get());
    auto tsm = llvm::orc::ThreadSafeModule(std::unique_ptr<llvm::Module>(module), std::unique_ptr<llvm::LLVMContext>(context));
    if (auto Err = JIT->addIRModule(std::move(tsm))) {
        return JitResult::COMPILATION_FAILED;
    }

    auto Sym = JIT->lookup("tick");
    if (!Sym) {
        return JitResult::COMPILATION_FAILED;
    }

    compiled_func_ = reinterpret_cast<void*>(Sym->getValue());

    // 将 LLJIT 对象的所有权转移到 jit_session_ 中
    // 防止 LLJIT 析构后 compiled_func_ 指向已释放的内存
    jit_session_ = static_cast<void*>(JIT.release());
    llvm_module_ = nullptr;
    return JitResult::SUCCESS;
#else
    return JitResult::UNSUPPORTED_OP;
#endif
}

}
