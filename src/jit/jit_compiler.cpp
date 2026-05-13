#include <fstream>
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
      compiled_comb_func_(nullptr), compiled_seq_func_(nullptr),
      last_ir_instr_count_(0), last_seq_ir_instr_count_(0), last_vreg_count_(0) {
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

    JitFunction func_comb("tick_comb");
    JitFunction func_seq("tick_seq");
    auto ir_result = generate_ir(ctx, func_comb, func_seq);
    if (ir_result != JitResult::SUCCESS) {
        result.result = ir_result;
        return result;
    }

    last_ir_instr_count_ = func_comb.blocks.empty() ? 0 :
        static_cast<uint32_t>(func_comb.blocks[0].instrs.size());
    last_seq_ir_instr_count_ = func_seq.blocks.empty() ? 0 :
        static_cast<uint32_t>(func_seq.blocks[0].instrs.size());
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

    using TickFunc = void(*)(uint64_t*);
    auto tick_func = reinterpret_cast<TickFunc>(compiled_func_);
    tick_func(data_buffer_.data());
#endif
}

void JitCompiler::execute_comb_tick() {
#if defined(CH_JIT_ENABLED) && __has_include(<llvm/IR/LLVMContext.h>)
    if (!compiled_comb_func_) {
        return;
    }

    using CombTickFunc = void(*)(uint64_t*);
    auto tick_func = reinterpret_cast<CombTickFunc>(compiled_comb_func_);
    tick_func(data_buffer_.data());
#endif
}

void JitCompiler::execute_seq_tick() {
#if defined(CH_JIT_ENABLED) && __has_include(<llvm/IR/LLVMContext.h>)
    if (!compiled_seq_func_) {
        return;
    }

    using SeqTickFunc = void(*)(uint64_t*);
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
        delete static_cast<llvm::orc::LLJIT*>(jit_session_);
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

bool JitCompiler::is_external_node(uint32_t node_id) const {
    return external_node_ids_.count(node_id) > 0;
}

JitResult JitCompiler::generate_ir(ch::core::context* ctx, JitFunction& func_comb, JitFunction& func_seq) {
    if (!ctx) {
        return JitResult::IR_GENERATION_FAILED;
    }

    auto eval_list = ctx->get_eval_list();
    if (eval_list.empty()) {
        return JitResult::SUCCESS;
    }

    JitBlock block_comb("combinational");
    JitBlock block_seq("sequential");
    VRegId next_comb_vreg = 0;
    VRegId next_seq_vreg = 0;

    for (auto* node : eval_list) {
        auto node_type = node->type();
        auto node_id = node->id();
        auto bitwidth = static_cast<BitWidth>(node->size());

        // Sequential nodes go to seq_block
        if (node_type == ch::core::lnodetype::type_reg) {
            auto* reg_node = static_cast<ch::core::regimpl*>(node);
            auto* next_node = reg_node->get_next();

            // Load the "next" value from the source node into a vreg
            VRegId next_vreg = INVALID_VREG;
            if (next_node && next_node->size() <= 64) {
                next_vreg = next_seq_vreg++;
                auto next_bitwidth = static_cast<BitWidth>(next_node->size());
                block_seq.instrs.push_back(make_load(next_vreg, next_node->id(), next_bitwidth));
            }

            // REG_NEXT: store the "next" value to the register's output node
            auto dst_vreg = next_seq_vreg++;
            auto reg_next = make_reg_next(dst_vreg, node_id, bitwidth);
            reg_next.src0 = next_vreg;  // Set source (INVALID_VREG means no update)
            block_seq.instrs.push_back(reg_next);
            block_seq.node_count++;
            continue;
        }

        if (node_type == ch::core::lnodetype::type_mem) {
            // Memory nodes are processed separately - skip here
            continue;
        }

        // Memory read ports have combinational outputs that depend on memory array state.
        // Without the memory array accessible in JIT, we must fall back to interpreter.
        if (node_type == ch::core::lnodetype::type_mem_read_port) {
            return JitResult::UNSUPPORTED_OP;
        }

        if (node_type == ch::core::lnodetype::type_mem_write_port) {
            continue;
        }

        // type_lit nodes: literal values are set at runtime via set_value().
        // The interpreter does NOT evaluate them (litimpl has no create_instruction),
        // so JIT must also skip them to avoid overwriting runtime values with compile-time constants.
        if (node_type == ch::core::lnodetype::type_lit) {
            // Literals should be loaded via sync_from_buffer so set_value() updates persist.
            // Use LOAD_DATA (read from data_buffer_) instead of LOAD_CONST (compile-time constant).
            auto vreg = next_comb_vreg++;
            block_comb.instrs.push_back(make_load(vreg, node_id, bitwidth));
            continue;
        }

        // Combinational nodes
        switch (node_type) {
        case ch::core::lnodetype::type_input: {
            auto vreg = next_comb_vreg++;
            // 连接后的输入端口需要从 driver 加载值，而非从自身
            if (node->num_srcs() > 0 && node->src(0)) {
                auto src_bitwidth = static_cast<BitWidth>(node->src(0)->size());
                block_comb.instrs.push_back(make_load(vreg, node->src(0)->id(), src_bitwidth));
                block_comb.instrs.push_back(make_store(node_id, vreg, bitwidth));
            } else {
                block_comb.instrs.push_back(make_load(vreg, node_id, bitwidth));
                block_comb.instrs.push_back(make_store(node_id, vreg, bitwidth));
            }
            break;
        }

        case ch::core::lnodetype::type_op: {
            auto* op_node = static_cast<ch::core::opimpl*>(node);
            auto ch_op = op_node->op();

            // 检查操作是否 JIT 原生支持
            // 使用 -Wswitch-enum 确保所有 ch_op 值被显式处理
            JitOp jit_op;
#pragma GCC diagnostic push
#pragma GCC diagnostic error "-Wswitch-enum"
            switch (ch_op) {
            case ch::core::ch_op::add: jit_op = JitOp::ADD; break;
            case ch::core::ch_op::sub: jit_op = JitOp::SUB; break;
            case ch::core::ch_op::mul: jit_op = JitOp::MUL; break;
            case ch::core::ch_op::mod: jit_op = JitOp::MOD; break;
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
            case ch::core::ch_op::bit_sel: jit_op = JitOp::BIT_SELECT; break;
            case ch::core::ch_op::bits_extract: jit_op = JitOp::BITS_EXTRACT; break;
            case ch::core::ch_op::concat: jit_op = JitOp::CONCAT; break;
            // 显式列出的 JIT 未支持操作（call_external 回退）
            case ch::core::ch_op::div:
            case ch::core::ch_op::bits_update:
            case ch::core::ch_op::mux:
            case ch::core::ch_op::and_reduce:
            case ch::core::ch_op::or_reduce:
            case ch::core::ch_op::xor_reduce:
            case ch::core::ch_op::rotate_l:
            case ch::core::ch_op::rotate_r:
            case ch::core::ch_op::popcount:
            case ch::core::ch_op::assign:
                jit_op = JitOp::CALL_EXTERNAL;
                break;
            // 原生支持的操作（使用 src_bitwidth）
            case ch::core::ch_op::sext: jit_op = JitOp::SEXT; break;
            case ch::core::ch_op::zext: jit_op = JitOp::ZEXT; break;
            case ch::core::ch_op::sshr: jit_op = JitOp::SSHRSHN; break;
            case ch::core::ch_op::neg: jit_op = JitOp::NEG; break;
            }
#pragma GCC diagnostic pop

            if (jit_op == JitOp::CALL_EXTERNAL) {
                external_node_ids_.insert(node_id);
                continue;
            }

            auto result_bitwidth = static_cast<BitWidth>(node->size());
            auto src0_vreg = next_comb_vreg++;
            auto src1_vreg = next_comb_vreg++;
            auto dst_vreg = next_comb_vreg++;

            if (node->num_srcs() > 0 && node->src(0)) {
                auto src0_bitwidth = static_cast<BitWidth>(node->src(0)->size());
                block_comb.instrs.push_back(make_load(src0_vreg, node->src(0)->id(), src0_bitwidth));
            }
            if (node->num_srcs() > 1 && node->src(1)) {
                auto src1_bitwidth = static_cast<BitWidth>(node->src(1)->size());
                block_comb.instrs.push_back(make_load(src1_vreg, node->src(1)->id(), src1_bitwidth));
            }

            JitInstr jit_instr;
            if (jit_op == JitOp::SEXT || jit_op == JitOp::ZEXT || jit_op == JitOp::NEG) {
                jit_instr = JitInstr(jit_op, result_bitwidth);
                jit_instr.dst = dst_vreg;
                jit_instr.src0 = src0_vreg;
                auto src0_bitwidth = node->num_srcs() > 0 && node->src(0)
                    ? static_cast<BitWidth>(node->src(0)->size()) : result_bitwidth;
                jit_instr.src_bitwidth = src0_bitwidth;
                jit_instr.node_id = node_id;
            } else if (jit_op == JitOp::SSHRSHN) {
                jit_instr = JitInstr(jit_op, result_bitwidth);
                jit_instr.dst = dst_vreg;
                jit_instr.src0 = src0_vreg;
                jit_instr.src1 = src1_vreg;
                auto src0_bitwidth = node->num_srcs() > 0 && node->src(0)
                    ? static_cast<BitWidth>(node->src(0)->size()) : result_bitwidth;
                jit_instr.src_bitwidth = src0_bitwidth;
                jit_instr.node_id = node_id;
            } else {
                block_comb.instrs.push_back(make_binary(jit_op, dst_vreg, src0_vreg, src1_vreg, result_bitwidth));
                block_comb.instrs.push_back(make_store(node_id, dst_vreg, result_bitwidth));
            }
            if (jit_op == JitOp::SEXT || jit_op == JitOp::ZEXT || jit_op == JitOp::SSHRSHN || jit_op == JitOp::NEG) {
                block_comb.instrs.push_back(jit_instr);
                block_comb.instrs.push_back(make_store(node_id, dst_vreg, result_bitwidth));
            }
            break;
        }

        case ch::core::lnodetype::type_mux: {
            auto cond_vreg = next_comb_vreg++;
            auto src0_vreg = next_comb_vreg++;
            auto src1_vreg = next_comb_vreg++;
            auto dst_vreg = next_comb_vreg++;

            if (node->num_srcs() >= 1 && node->src(0)) {
                block_comb.instrs.push_back(make_load(cond_vreg, node->src(0)->id(), 1));
            }
            if (node->num_srcs() >= 2 && node->src(1)) {
                block_comb.instrs.push_back(make_load(src0_vreg, node->src(1)->id(), bitwidth));
            }
            if (node->num_srcs() >= 3 && node->src(2)) {
                block_comb.instrs.push_back(make_load(src1_vreg, node->src(2)->id(), bitwidth));
            }

            block_comb.instrs.push_back(make_select(dst_vreg, cond_vreg, src0_vreg, src1_vreg, bitwidth));
            block_comb.instrs.push_back(make_store(node_id, dst_vreg, bitwidth));
            break;
        }

        case ch::core::lnodetype::type_proxy:
        case ch::core::lnodetype::type_output: {
            if (node->num_srcs() > 0 && node->src(0)) {
                auto src_vreg = next_comb_vreg++;
                auto dst_vreg = next_comb_vreg++;
                block_comb.instrs.push_back(make_load(src_vreg, node->src(0)->id(), bitwidth));
                block_comb.instrs.push_back(make_store(node_id, src_vreg, bitwidth));
            }
            break;
        }

        case ch::core::lnodetype::type_clock:
        case ch::core::lnodetype::type_reset:
        case ch::core::lnodetype::type_bitsupdate:
        default:
            break;
        }

        block_comb.node_count++;
    }

    func_comb.vreg_count = next_comb_vreg;
    func_comb.blocks.push_back(std::move(block_comb));
    func_comb.node_count = static_cast<uint32_t>(eval_list.size());

    func_seq.vreg_count = next_seq_vreg;
    func_seq.blocks.push_back(std::move(block_seq));

    return JitResult::SUCCESS;
}

JitResult JitCompiler::compile_to_llvm(const JitFunction& func_comb, const JitFunction& func_seq) {
#if defined(CH_JIT_ENABLED) && __has_include(<llvm/IR/LLVMContext.h>)
    auto* context = new llvm::LLVMContext();
    std::string module_name = "cpphdl_jit_dual";
    auto* module = new llvm::Module(module_name, *context);
    llvm::IRBuilder<> builder(*context);

    auto* ptr_ty = builder.getInt64Ty()->getPointerTo();
    auto* func_ty = llvm::FunctionType::get(builder.getVoidTy(), ptr_ty, false);

    auto compile_single_func = [&](const JitFunction& func, void*& out_func_ptr) {
        if (func.blocks.empty() || func.blocks[0].instrs.empty()) {
            out_func_ptr = nullptr;
            return JitResult::SUCCESS;
        }

        for (const auto& instr : func.blocks[0].instrs) {
            if (instr.op == JitOp::CONCAT ||
                instr.op == JitOp::SLICE ||
                instr.op == JitOp::MEM_READ ||
                instr.op == JitOp::MEM_WRITE ||
                instr.op == JitOp::JUMP ||
                instr.op == JitOp::BRANCH ||
                instr.op == JitOp::LABEL) {
                return JitResult::UNSUPPORTED_OP;
            }
        }

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

        auto get_node_ptr = [&](uint32_t node_id) -> llvm::Value* {
            auto* indices = llvm::ConstantInt::get(builder.getInt32Ty(), node_id);
            return builder.CreateGEP(builder.getInt64Ty(), data_buffer_ptr, indices, "node_ptr");
        };

        auto load_node = [&](uint32_t node_id, BitWidth bw) -> llvm::Value* {
            auto* ptr = get_node_ptr(node_id);
            auto* val = builder.CreateLoad(builder.getInt64Ty(), ptr, "load_node");
            if (bw < 64) {
                uint64_t mask = (1ULL << bw) - 1;
                return builder.CreateAnd(val, builder.getInt64(mask), "mask_load");
            }
            return val;
        };

        auto store_node = [&](uint32_t node_id, llvm::Value* val, BitWidth bw) {
            auto* ptr = get_node_ptr(node_id);
            llvm::Value* store_val = val;
            if (bw < 64) {
                uint64_t mask = (1ULL << bw) - 1;
                store_val = builder.CreateAnd(store_val, builder.getInt64(mask), "mask_store");
            }
            builder.CreateStore(store_val, ptr);
        };

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
                    break;
                }
                auto* val = builder.CreateLoad(builder.getInt64Ty(), vregs[instr.src0], "load_src");
                store_node(instr.node_id, val, instr.bitwidth);
                break;
            }

            case JitOp::ADD: {
                auto* a = builder.CreateLoad(builder.getInt64Ty(), vregs[instr.src0], "load_a");
                auto* b = builder.CreateLoad(builder.getInt64Ty(), vregs[instr.src1], "load_b");
                auto* res = builder.CreateAdd(a, b, "add");
                if (instr.bitwidth < 64) {
                    uint64_t mask = (1ULL << instr.bitwidth) - 1;
                    res = builder.CreateAnd(res, builder.getInt64(mask), "mask_add");
                }
                builder.CreateStore(res, vregs[instr.dst], "store_add");
                break;
            }

            case JitOp::SUB: {
                auto* a = builder.CreateLoad(builder.getInt64Ty(), vregs[instr.src0], "load_a");
                auto* b = builder.CreateLoad(builder.getInt64Ty(), vregs[instr.src1], "load_b");
                auto* res = builder.CreateSub(a, b, "sub");
                if (instr.bitwidth < 64) {
                    uint64_t mask = (1ULL << instr.bitwidth) - 1;
                    res = builder.CreateAnd(res, builder.getInt64(mask), "mask_sub");
                }
                builder.CreateStore(res, vregs[instr.dst], "store_sub");
                break;
            }

            case JitOp::MUL: {
                auto* a = builder.CreateLoad(builder.getInt64Ty(), vregs[instr.src0], "load_a");
                auto* b = builder.CreateLoad(builder.getInt64Ty(), vregs[instr.src1], "load_b");
                auto* res = builder.CreateMul(a, b, "mul");
                if (instr.bitwidth < 64) {
                    uint64_t mask = (1ULL << instr.bitwidth) - 1;
                    res = builder.CreateAnd(res, builder.getInt64(mask), "mask_mul");
                }
                builder.CreateStore(res, vregs[instr.dst], "store_mul");
                break;
            }

            case JitOp::MOD: {
                auto* a = builder.CreateLoad(builder.getInt64Ty(), vregs[instr.src0], "load_a");
                auto* b = builder.CreateLoad(builder.getInt64Ty(), vregs[instr.src1], "load_b");
                auto* is_zero = builder.CreateICmpEQ(b, builder.getInt64(0), "is_zero");
                auto* rem = builder.CreateURem(a, b, "mod");
                auto* res = builder.CreateSelect(is_zero, a, rem, "mod_safe");
                if (instr.bitwidth < 64) {
                    uint64_t mask = (1ULL << instr.bitwidth) - 1;
                    res = builder.CreateAnd(res, builder.getInt64(mask), "mask_mod");
                }
                builder.CreateStore(res, vregs[instr.dst], "store_mod");
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
                if (instr.bitwidth < 64) {
                    uint64_t mask = (1ULL << instr.bitwidth) - 1;
                    res = builder.CreateAnd(res, builder.getInt64(mask), "mask_not");
                }
                builder.CreateStore(res, vregs[instr.dst], "store_not");
                break;
            }

            case JitOp::SHIFT_LEFT: {
                auto* a = builder.CreateLoad(builder.getInt64Ty(), vregs[instr.src0], "load_a");
                auto* b = builder.CreateLoad(builder.getInt64Ty(), vregs[instr.src1], "load_b");
                auto* res = builder.CreateShl(a, b, "shl");
                if (instr.bitwidth < 64) {
                    uint64_t mask = (1ULL << instr.bitwidth) - 1;
                    res = builder.CreateAnd(res, builder.getInt64(mask), "mask_shl");
                }
                builder.CreateStore(res, vregs[instr.dst], "store_shl");
                break;
            }

            case JitOp::SHIFT_RIGHT: {
                auto* a = builder.CreateLoad(builder.getInt64Ty(), vregs[instr.src0], "load_a");
                auto* b = builder.CreateLoad(builder.getInt64Ty(), vregs[instr.src1], "load_b");
                auto* res = builder.CreateLShr(a, b, "shr");
                if (instr.bitwidth < 64) {
                    uint64_t mask = (1ULL << instr.bitwidth) - 1;
                    res = builder.CreateAnd(res, builder.getInt64(mask), "mask_shr");
                }
                builder.CreateStore(res, vregs[instr.dst], "store_shr");
                break;
            }

            case JitOp::SEXT: {
                auto* a = builder.CreateLoad(builder.getInt64Ty(), vregs[instr.src0], "load_a");
                auto* res = builder.CreateTrunc(a, builder.getIntNTy(instr.src_bitwidth), "trunc_sext");
                res = builder.CreateSExt(res, builder.getInt64Ty(), "sext");
                if (instr.bitwidth < 64) {
                    uint64_t mask = (1ULL << instr.bitwidth) - 1;
                    res = builder.CreateAnd(res, builder.getInt64(mask), "mask_sext");
                }
                builder.CreateStore(res, vregs[instr.dst], "store_sext");
                break;
            }

            case JitOp::ZEXT: {
                auto* a = builder.CreateLoad(builder.getInt64Ty(), vregs[instr.src0], "load_a");
                auto* res = builder.CreateTrunc(a, builder.getIntNTy(instr.src_bitwidth), "trunc_zext");
                res = builder.CreateZExt(res, builder.getInt64Ty(), "zext");
                if (instr.bitwidth < 64) {
                    uint64_t mask = (1ULL << instr.bitwidth) - 1;
                    res = builder.CreateAnd(res, builder.getInt64(mask), "mask_zext");
                }
                builder.CreateStore(res, vregs[instr.dst], "store_zext");
                break;
            }

            case JitOp::SSHRSHN: {
                auto* a = builder.CreateLoad(builder.getInt64Ty(), vregs[instr.src0], "load_a");
                auto* b = builder.CreateLoad(builder.getInt64Ty(), vregs[instr.src1], "load_b");
                auto* narrowed = builder.CreateTrunc(a, builder.getIntNTy(instr.src_bitwidth), "trunc_sshr");
                auto* res = builder.CreateAShr(narrowed, b, "sshr");
                if (instr.bitwidth < 64) {
                    uint64_t mask = (1ULL << instr.bitwidth) - 1;
                    res = builder.CreateAnd(res, builder.getInt64(mask), "mask_sshr");
                }
                builder.CreateStore(res, vregs[instr.dst], "store_sshr");
                break;
            }

            case JitOp::NEG: {
                auto* a = builder.CreateLoad(builder.getInt64Ty(), vregs[instr.src0], "load_a");
                auto* res = builder.CreateNeg(a, "neg");
                if (instr.bitwidth < 64) {
                    uint64_t mask = (1ULL << instr.bitwidth) - 1;
                    res = builder.CreateAnd(res, builder.getInt64(mask), "mask_neg");
                }
                builder.CreateStore(res, vregs[instr.dst], "store_neg");
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

            case JitOp::BIT_SELECT: {
                auto* data = builder.CreateLoad(builder.getInt64Ty(), vregs[instr.src0], "load_data");
                auto* idx = builder.CreateLoad(builder.getInt64Ty(), vregs[instr.src1], "load_idx");
                auto* shifted = builder.CreateLShr(data, idx, "shift_idx");
                auto* masked = builder.CreateAnd(shifted, builder.getInt64(1), "mask_bit");
                builder.CreateStore(masked, vregs[instr.dst], "store_bit_sel");
                break;
            }

            case JitOp::BITS_EXTRACT: {
                auto* val = builder.CreateLoad(builder.getInt64Ty(), vregs[instr.src0], "load_val");
                auto* range = builder.CreateLoad(builder.getInt64Ty(), vregs[instr.src1], "load_range");
                auto* lsb = builder.CreateAnd(range, builder.getInt64(0xFFFFFFFF), "lsb");
                auto* msb = builder.CreateLShr(range, builder.getInt64(32), "msb_shift");
                msb = builder.CreateAnd(msb, builder.getInt64(0xFFFFFFFF), "msb");
                auto* shifted = builder.CreateLShr(val, lsb, "shift_val");
                auto* width = builder.CreateSub(msb, lsb, "width_sub");
                width = builder.CreateAdd(width, builder.getInt64(1), "width_add1");
                auto* mask = builder.CreateShl(builder.getInt64(1), width, "mask_shl");
                mask = builder.CreateSub(mask, builder.getInt64(1), "mask_sub1");
                auto* res = builder.CreateAnd(shifted, mask, "extract");
                if (instr.bitwidth < 64) {
                    uint64_t bw_mask = (1ULL << instr.bitwidth) - 1;
                    res = builder.CreateAnd(res, builder.getInt64(bw_mask), "mask_bw");
                }
                builder.CreateStore(res, vregs[instr.dst], "store_bits_extract");
                break;
            }

            case JitOp::REG_NEXT: {
                if (instr.src0 == INVALID_VREG) {
                    break;
                }
                auto* val = builder.CreateLoad(builder.getInt64Ty(), vregs[instr.src0], "load_reg");
                store_node(instr.node_id, val, instr.bitwidth);
                break;
            }

            case JitOp::CALL_EXTERNAL: {
                auto* ptr = get_node_ptr(instr.node_id);
                auto* val = builder.CreateLoad(builder.getInt64Ty(), ptr, "call_ext_load");
                builder.CreateStore(val, vregs[instr.dst], "call_ext_store");
                break;
            }

            case JitOp::NOP:
            default:
                break;
            }
        }

        std::string ir_str; llvm::raw_string_ostream ir_stream(ir_str); module->print(ir_stream, nullptr); std::ofstream ir_file("/tmp/jit_ir.ll"); ir_file << ir_str; ir_file.close();
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

JitResult JitCompiler::finalize_compilation_dual() {
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

    auto Sym_comb = JIT->lookup("tick_comb");
    if (!Sym_comb) {
        return JitResult::COMPILATION_FAILED;
    }
    compiled_comb_func_ = reinterpret_cast<void*>(Sym_comb->getValue());

    auto Sym_seq = JIT->lookup("tick_seq");
    if (!Sym_seq) {
        return JitResult::COMPILATION_FAILED;
    }
    compiled_seq_func_ = reinterpret_cast<void*>(Sym_seq->getValue());

    jit_session_ = static_cast<void*>(JIT.release());
    llvm_module_ = nullptr;
    return JitResult::SUCCESS;
#else
    return JitResult::UNSUPPORTED_OP;
#endif
}

}
