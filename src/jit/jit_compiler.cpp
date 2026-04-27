#include "jit/jit_compiler.h"
#include "core/context.h"
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
}

JitResult JitCompiler::generate_ir(ch::core::context* ctx, JitFunction& func) {
    return JitResult::SUCCESS;
}

JitResult JitCompiler::compile_to_llvm(const JitFunction& func) {
    return JitResult::SUCCESS;
}

JitResult JitCompiler::finalize_compilation() {
    return JitResult::SUCCESS;
}

}
