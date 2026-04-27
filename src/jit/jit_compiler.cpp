#include "jit/jit_compiler.h"
#include "core/context.h"
#include "core/lnodeimpl.h"
#include "ast/ast_nodes.h"
#include <iostream>

namespace ch::jit {

JitCompiler::JitCompiler()
    : available_(false), jit_session_(nullptr), compiled_func_(nullptr),
      last_ir_instr_count_(0), last_vreg_count_(0) {
#if __has_include(<llvm/Support/TargetSelect.h>)
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
    last_vreg_count_ = func.vreg_count;

    result.result = compile_to_llvm(func);
    return result;
}

void JitCompiler::execute_tick() {
}

void JitCompiler::clear() {
    if (compiled_func_) {
        compiled_func_ = nullptr;
    }
    if (jit_session_) {
        jit_session_ = nullptr;
    }
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

    JitBlock block("main");
    VRegId next_vreg = 0;

    for (auto* node : eval_list) {
        auto node_type = node->type();
        auto node_id = node->id();
        auto bitwidth = static_cast<BitWidth>(node->size());

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

        case ch::core::lnodetype::type_reg: {
            auto vreg = next_vreg++;
            auto load_instr = make_load(vreg, node_id, bitwidth);
            load_instr.op = JitOp::REG_NEXT;
            block.instrs.push_back(load_instr);
            block.instrs.push_back(make_store(node_id, vreg, bitwidth));
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
        case ch::core::lnodetype::type_mem_read_port:
        case ch::core::lnodetype::type_mem_write_port:
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

    return JitResult::SUCCESS;
#else
    return JitResult::UNSUPPORTED_OP;
#endif
}

JitResult JitCompiler::finalize_compilation() {
    return JitResult::SUCCESS;
}

}
