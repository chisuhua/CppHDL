#ifndef JIT_COMPILER_H
#define JIT_COMPILER_H

#include "jit_ir.h"
#include "ast/instr_base.h"
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

    uint64_t* data_buffer() { return data_buffer_.data(); }
    const uint64_t* data_buffer() const { return data_buffer_.data(); }
    size_t data_buffer_size() const { return data_buffer_.size(); }

    void sync_to_buffer(const ch::data_map_t& data_map);
    void sync_from_buffer(ch::data_map_t& data_map);

private:
    bool available_;
    void* jit_session_;
    void* compiled_func_;
    uint32_t last_ir_instr_count_;
    uint32_t last_vreg_count_;

    std::vector<uint64_t> data_buffer_;

#if defined(CH_JIT_ENABLED) && __has_include(<llvm/IR/LLVMContext.h>)
    void* llvm_module_;  // Stores llvm::Module* as void*
#endif

    JitResult allocate_buffer(ch::core::context* ctx);
    JitResult generate_ir(ch::core::context* ctx, JitFunction& func);
    JitResult compile_to_llvm(const JitFunction& func);
    JitResult finalize_compilation();
};

} // namespace jit
} // namespace ch

#endif