// src/simulator.cpp
#include "simulator.h"
#include "ast/ast_nodes.h"
#include "ast/instr_mem.h"
#include "ast/mem_port_impl.h"
#include "bv/utils.h"
#include "core/lnodeimpl.h"
#include "logger.h"
#include "types.h"
#include "utils/destruction_manager.h"
#include <cassert>
#include <cstring>
#include <fstream>
#include <iostream>
#include <set>
#include <unordered_set>
#include <chrono>
#include <iomanip>

namespace ch {

using ch::core::ctx_curr_;

Simulator::Simulator(ch::core::context *ctx, bool trace_on)
    : ctx_(ctx), trace_on_(trace_on) {
    CHDBG_FUNC();
    CHREQUIRE(ctx_ != nullptr, "Context cannot be null");

    // 检查当前目录是否存在trace.ini文件
    std::ifstream trace_ini_file("./trace.ini");
    if (trace_ini_file.is_open()) {
        trace_ini_file.close();
        // 从配置文件加载跟踪配置
        load_trace_config_from_file("./trace.ini");

        // 检查是否需要启用跟踪
        trace_on_ = !trace_configs_.empty(); // 如果有任何配置，则启用跟踪
    }

    // 如果不存在trace.ini文件，则按原来的流程执行
    ctx_curr_backup_ = ctx_curr_;
    ctx_curr_ = ctx_;

    initialize();

    // 如果启用了跟踪，则收集信号
    if (trace_on_) {
        collect_signals();
    }

#if __has_include("jit/jit_compiler.h")
    jit_compiler_ = std::make_unique<ch::jit::JitCompiler>();
    if (jit_compiler_->is_available()) {
        CHINFO("JIT compiler available, attempting compilation...");
        try_jit_compile();
    } else {
        CHINFO("JIT compiler not available, using interpreter");
    }
#endif
}

// 新增构造函数实现
Simulator::Simulator(ch::core::context *ctx, const std::string &config_file)
    : ctx_(ctx), trace_on_(false) {
    CHDBG_FUNC();
    CHREQUIRE(ctx_ != nullptr, "Context cannot be null");

    ctx_curr_backup_ = ctx_curr_;
    ctx_curr_ = ctx_;

    // 从配置文件加载跟踪配置
    load_trace_config_from_file(config_file);

    initialize();

    // 检查是否需要启用跟踪
    trace_on_ = !trace_configs_.empty(); // 如果有任何配置，则启用跟踪
    if (trace_on_) {
        collect_signals();
    }
}

Simulator::Simulator(ch::core::context *ctx, const char* config_file)
    : ctx_(ctx), trace_on_(false) {
    CHDBG_FUNC();
    CHREQUIRE(ctx_ != nullptr, "Context cannot be null");

    ctx_curr_backup_ = ctx_curr_;
    ctx_curr_ = ctx_;

    // 从配置文件加载跟踪配置
    load_trace_config_from_file(std::string(config_file));

    initialize();

    // 检查是否需要启用跟踪
    trace_on_ = !trace_configs_.empty(); // 如果有任何配置，则启用跟踪
    if (trace_on_) {
        collect_signals();
    }
}

Simulator::~Simulator() {
    // 释放跟踪数据块
    for (auto *block : trace_blocks_) {
        delete block;
    }
    trace_blocks_.clear();

    // ch::pre_static_destruction_cleanup();
    // ch::detail::set_static_destruction();

    // Explicitly disconnect to prevent accessing destroyed context
    disconnect();

    ctx_curr_ = ctx_curr_backup_;
}

void Simulator::disconnect() {
    CHDBG_FUNC();

    // Check if we're in static destruction phase
    // if (ch::detail::in_static_destruction()) {
    //     // During static destruction, minimize operations to prevent
    //     segfaults
    //     // Just mark as disconnected and return immediately
    //     disconnected_ = true;
    //     return;
    // }

    if (disconnected_) {
        return;
    }

    // Mark as disconnected first to prevent any further operations
    disconnected_ = true;

    // Clear all data that references the context
    instr_map_.clear();
    instr_cache_.clear();
    data_map_.clear();
    eval_list_.clear();

    // Clear the context pointer
    ctx_ = nullptr;

    // Mark as uninitialized
    initialized_ = false;
}

void Simulator::reinitialize() {
    initialized_ = false;
    initialize();

    // 重新初始化跟踪
    if (trace_on_) {
        collect_signals();
    }
}

#ifdef ENABLE_MEMORY_API
std::vector<uint8_t> Simulator::get_memory(uint32_t addr, size_t size) const {
    return {};
}

void Simulator::set_memory(uint32_t addr, const std::vector<uint8_t>& data) {
}
#endif

void Simulator::update_instruction_pointers() {
    CHDBG_FUNC();

    if (disconnected_)
        return;

    instr_map_.clear();
    for (const auto &pair : instr_cache_) {
        if (disconnected_)
            return;
        instr_map_[pair.first] = pair.second.get();
    }
    CHDBG("Updated %zu instruction pointers", instr_map_.size());

}

void Simulator::eval_sequential() {
    CHDBG_FUNC();

#if __has_include("jit/jit_compiler.h")
    if (jit_enabled_ && jit_compiled_ && jit_compiler_ && jit_compiler_->has_seq_func()) {
        // 先执行 CALL_EXTERNAL 解释器节点（修复: JIT 之前执行避免陈旧值）
        for (const auto &[node_id, instr] : sequential_instr_list_) {
            if (jit_compiler_->is_external_node(node_id)) {
                instr->eval();
            }
        }

        jit_compiler_->sync_to_buffer(data_map_);
        jit_compiler_->execute_seq_tick();
        jit_compiler_->sync_from_buffer(data_map_);
    } else {
        for (const auto &[node_id, instr] : sequential_instr_list_) {
            instr->eval();
            CHDBG("Evaluating sequential instruction for node %u, %s", node_id,
                  data_map_[node_id].to_string_verbose().c_str());
        }
    }
#else
    for (const auto &[node_id, instr] : sequential_instr_list_) {
        instr->eval();
        CHDBG("Evaluating sequential instruction for node %u, %s", node_id,
              data_map_[node_id].to_string_verbose().c_str());
    }
#endif

    CHDBG("Evaluation sequential completed");
}

void Simulator::eval_combinational() {
    CHDBG_FUNC();

#if __has_include("jit/jit_compiler.h")
    if (jit_enabled_ && jit_compiled_ && jit_compiler_ && jit_compiler_->has_comb_func()) {
        for (const auto &[node_id, instr] : combinational_instr_list_) {
            if (jit_compiler_->is_external_node(node_id)) {
                instr->eval();
            }
        }

        jit_compiler_->sync_to_buffer(data_map_);
        jit_compiler_->execute_comb_tick();
        jit_compiler_->sync_from_buffer(data_map_);
        return;
    }
#endif

    for (const auto &[node_id, instr] : input_instr_list_) {
        instr->eval();
        CHDBG("Evaluating input instruction for node %u: %s", node_id,
              data_map_[node_id].to_string_verbose().c_str());
    }

    for (const auto &[node_id, instr] : combinational_instr_list_) {
        instr->eval();
        CHDBG("Evaluating combinational instruction for node %u: %s", node_id,
              data_map_[node_id].to_string_verbose().c_str());
    }

    CHDBG("Evaluation combinational completed");
}

void Simulator::tick() {
    CHDBG_FUNC();

    // 检查context是否有效
    if (!initialized_ || disconnected_ || !ctx_) {
        CHERROR("Simulator not initialized or disconnected");
        return;
    }

    CHDBG("ticks count: %u", ticks_++);

    // 进度报告 (每1秒报告一次)
    {
        static auto last_report = std::chrono::steady_clock::now();
        static size_t last_ticks = 0;
        auto now = std::chrono::steady_clock::now();
        double elapsed = std::chrono::duration<double>(now - last_report).count();
        if (elapsed >= 1.0) {
            double rate = (ticks_ - last_ticks) / elapsed;
            std::cout << "[PROGRESS] tick=" << ticks_
                      << " rate=" << std::fixed << std::setprecision(0) << rate << " ticks/s"
                      << std::endl << std::flush;
            last_report = now;
            last_ticks = ticks_;
        }
    }

    // A/B verification: save interpreted state before execution
    // 注意：A/B 验证基础设施目前不完整（dual-function JIT 重构后尚未重新设计）
    // 启用后只会打印警告，不会触发比对逻辑。
    ch::data_map_t ab_result_a;
    bool run_ab_verify = false;
#if __has_include("jit/jit_compiler.h")
    run_ab_verify = ab_verification_;
#endif

#ifdef ENABLE_PERF_PROFILING
    auto tick_start = std::chrono::high_resolution_clock::now();
    uint64_t eval_start_ns = 0;
#endif

    eval_combinational();

    if (default_clock_instr_) {
#ifdef ENABLE_PERF_PROFILING
        eval_start_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(
            std::chrono::high_resolution_clock::now() - tick_start).count();
#endif
        eval();
        eval();

#if __has_include("jit/jit_compiler.h")
        if (jit_enabled_ && jit_compiled_ && jit_compiler_ && jit_compiler_->has_comb_func()) {
            for (const auto &[node_id, instr] : combinational_instr_list_) {
                if (jit_compiler_->is_external_node(node_id)) {
                    instr->eval();
                }
            }
        }
#endif

        eval_combinational();
    }

#if __has_include("jit/jit_compiler.h")
    if (run_ab_verify) {
        // A/B 验证当前仅作为"启用标记"——dual-function JIT（tick_comb + tick_seq）
        // 重构后尚未重新设计比对逻辑。此处仅打印警告，不中断仿真。
        // TODO: 重新实现 A/B 验证（需要保存 JIT 前后的 data_map_ 并比对）。
        CHWARN("A/B verification enabled but not implemented for dual-function JIT (tick_comb + tick_seq). Skipping comparison.");
    }
#endif

    if (trace_on_) {
        trace();
    }

#ifdef ENABLE_PERF_PROFILING
    auto tick_end = std::chrono::high_resolution_clock::now();
    uint64_t tick_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(
        tick_end - tick_start).count();
    total_tick_ns_ += tick_ns;
    if (default_clock_instr_) {
        eval_ns_ += (tick_ns - eval_start_ns);
    }
    profile_tick_count_++;
#endif
}

void Simulator::eval() {
    CHDBG_FUNC();

    // 1. 首先执行默认时钟指令以更新时钟边沿状态
    CHDBG("Evaluating default clock instruction");
    default_clock_instr_->eval();

    // 2. 然后执行其他时钟指令
    for (const auto &[node_id, instr] : other_clock_instr_list_) {
        CHDBG("Evaluating other clock instruction for node %u", node_id);
        instr->eval();
    }

    // 对于包含时序逻辑的电路，按照特定顺序执行各类节点
    // 如果时钟为0，执行组合逻辑；如果时钟为1，执行时序逻辑
    if (default_clock_data_->is_zero()) {
        eval_combinational();
    } else {
        eval_sequential();
    }

    // 如果启用了跟踪，则记录信号值
    if (trace_on_) {
        trace();
    }

    CHDBG("Executed post-clock eval based on clock state %llu",
          static_cast<unsigned long long>(
              static_cast<uint64_t>(*default_clock_data_)));
}

void Simulator::tick(size_t count) {
    CHDBG_FUNC();
    CHDBG_VAR(count);

    if (count == 0) {
        return;
    }

    for (size_t i = 0; i < count; ++i) {
        if (disconnected_)
            return;
        CHDBG("Tick %zu/%zu", i + 1, count);
        tick();
    }
}

#ifdef ENABLE_PERF_PROFILING
void Simulator::print_perf_report() const {
    if (profile_tick_count_ == 0) {
        std::cout << "[PERF] No profiling data collected" << std::endl;
        return;
    }
    double avg_tick_us = static_cast<double>(total_tick_ns_) / profile_tick_count_ / 1000.0;
    double avg_eval_us = static_cast<double>(eval_ns_) / profile_tick_count_ / 1000.0;
    double eval_pct = total_tick_ns_ > 0
        ? 100.0 * static_cast<double>(eval_ns_) / total_tick_ns_
        : 0.0;
    std::cout << "[PERF] Profiled ticks: " << profile_tick_count_
              << ", avg tick: " << std::fixed << std::setprecision(1) << avg_tick_us << " us"
              << ", avg eval: " << avg_eval_us << " us"
              << " (" << std::setprecision(0) << eval_pct << "%)"
              << std::endl;
}
#endif

void Simulator::reset() {
    CHDBG_FUNC();

    // 检查context是否有效
    if (!initialized_ || disconnected_ || !ctx_) {
        CHERROR("Simulator not initialized or disconnected");
        return;
    }

    for (auto &pair : data_map_) {
        if (disconnected_)
            return;
        uint32_t node_id = pair.first;
        auto *node = ctx_->get_node_by_id(node_id);
        if (node && node->type() != ch::core::lnodetype::type_lit) {
            pair.second = ch::core::sdata_type(0, pair.second.bitwidth());
        }
    }
    CHDBG("Simulator state reset completed");
}

const ch::core::sdata_type &
Simulator::get_value_by_name(const std::string &name) const {
    CHDBG_FUNC();
    CHDBG("Looking for node with name: %s", name.c_str());

    // 检查context是否有效
    if (!initialized_ || disconnected_ || !ctx_) {
        CHERROR("Simulator not initialized or disconnected");
        // Return a safe empty value instead of crashing
        static ch::core::sdata_type empty_value(0, 1);
        return empty_value;
    }

    if (name.empty()) {
        CHERROR("Node name cannot be empty");
        static ch::core::sdata_type empty_value(0, 1);
        return empty_value;
    }

    // 查找节点
    for (const auto &node_ptr : ctx_->get_nodes()) {
        if (!node_ptr || disconnected_)
            continue;
        if (node_ptr && node_ptr->name() == name) {
            uint32_t node_id = node_ptr->id();
            auto it = data_map_.find(node_id);
            if (it != data_map_.end()) {
                return it->second;
            }
            CHWARN("Data not found for node '%s' with ID %u", name.c_str(),
                   node_id);
            static ch::core::sdata_type empty_value(0, 1);
            return empty_value;
        }
    }

    CHWARN("Node with name '%s' not found", name.c_str());
    static ch::core::sdata_type empty_value(0, 1);
    return empty_value;
}

#if __has_include("jit/jit_compiler.h")
void Simulator::try_jit_compile() {
    if (!jit_compiler_ || !initialized_) {
        return;
    }

    auto result = jit_compiler_->compile(ctx_);
    if (result.result == ch::jit::JitResult::SUCCESS && result.ir_instr_count > 0) {
        jit_compiled_ = true;
        CHINFO("JIT compilation successful: %u IR instructions, %u vregs, %llu ns",
               result.ir_instr_count, result.vreg_count,
               (unsigned long long)result.compile_time_ns);
    } else if (result.result == ch::jit::JitResult::UNSUPPORTED_OP) {
        // W2 (perf-report-followup.md): small graphs intentionally skip JIT
        // to avoid paying LLVM ORC setup cost. Silent fallback to interpreter.
        jit_compiled_ = false;
        CHINFO("JIT skipped (small graph or unsupported op): %s",
               jit_compiler_->last_error_msg().c_str());
    } else {
        std::string combined = result.error_msg;
        if (combined.empty() && jit_compiler_) {
            combined = jit_compiler_->last_error_msg();
        }
        CHWARN("JIT compilation failed: %s (ir_instr_count=%u)",
               combined.c_str(), result.ir_instr_count);
    }
}

void Simulator::set_jit_enabled(bool enabled) {
    if (!jit_compiler_) {
        CHWARN("JIT compiler not available");
        return;
    }
    jit_enabled_ = enabled;
    CHINFO("JIT %s", enabled ? "enabled" : "disabled");
    if (enabled && !jit_compiled_) {
        try_jit_compile();
    }
}

void Simulator::set_ab_verification(bool enabled) {
    ab_verification_ = enabled;
    CHINFO("A/B verification %s", enabled ? "enabled" : "disabled");
}
#endif

void Simulator::set_backend(std::unique_ptr<ch::IEvalBackend> backend) {
    if (backend_) {
        backend_->clear();
    }
    backend_ = std::move(backend);
    if (backend_) {
        backend_->initialize(ctx_, data_map_);
        backend_name_ = backend_->name();
        CHINFO("Simulator backend set to '%s' (ADR-035 Phase 2.3)",
               backend_name_.c_str());
    } else {
        backend_name_ = "inlined";
        CHINFO("Simulator backend cleared (falling back to inlined "
               "interpreter + JIT pipeline)");
    }
}

const std::string &Simulator::active_backend_name() const {
    return backend_name_;
}

} // namespace ch
