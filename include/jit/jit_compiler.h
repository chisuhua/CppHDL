#ifndef JIT_COMPILER_H
#define JIT_COMPILER_H

#include "jit_ir.h"
#include <memory>
#include <string>
#include <vector>

namespace ch {
namespace core {
class context;
}

namespace jit {

enum class JitResult {
    SUCCESS = 0,
    LLVM_INIT_FAILED,
    IR_GENERATION_FAILED,
    COMPILATION_FAILED,
    EXECUTION_FAILED,
    UNSUPPORTED_OP
};

struct JitCompileResult {
    JitResult result;
    std::string error_msg;
    uint64_t compile_time_ns;
    uint32_t ir_instr_count;
    uint32_t vreg_count;

    JitCompileResult() : result(JitResult::SUCCESS), compile_time_ns(0),
                         ir_instr_count(0), vreg_count(0) {}
};

class JitCompiler {
public:
    JitCompiler();
    ~JitCompiler();

    JitCompiler(const JitCompiler&) = delete;
    JitCompiler& operator=(const JitCompiler&) = delete;

    bool is_available() const { return available_; }
    JitCompileResult compile(ch::core::context* ctx);
    void execute_tick();
    void clear();
    uint32_t get_ir_instr_count() const { return last_ir_instr_count_; }

private:
    bool available_;
    void* jit_session_;
    void* compiled_func_;
    uint32_t last_ir_instr_count_;
    uint32_t last_vreg_count_;

    JitResult generate_ir(ch::core::context* ctx, JitFunction& func);
    JitResult compile_to_llvm(const JitFunction& func);
    JitResult finalize_compilation();
};

} // namespace jit
} // namespace ch

#endif