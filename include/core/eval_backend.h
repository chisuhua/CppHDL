// include/core/eval_backend.h
// ADR-035 / Phase 2.1: Polymorphic evaluation backend interface.
// The ch::Simulator historically embedded the interpreter loop and the
// JIT dispatch in-line (see src/simulator.cpp:748-812). This header
// extracts that contract so that a third backend (VerilatorBackend)
// can be plugged in without modifying Simulator.
#pragma once

#include "ast/instr_base.h"  // for data_map_t
#include "core/context.h"
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace ch {

class IEvalBackend {
public:
    virtual ~IEvalBackend() = default;

    // Lifecycle: prepare the backend for a given context. Returns
    // true on success. The backend owns the data_map_ for the
    // Simulator (read/write access is via the instr_list accessors).
    virtual bool initialize(ch::core::context *ctx,
                            ch::data_map_t &data_map) = 0;

    // Lifecycle: release all resources (compiled .so, LLVM state, etc.)
    virtual void clear() = 0;

    // Per-tick: evaluate all combinational nodes. The instruction
    // lists are passed by Simulator so the backend can dispatch
    // CALL_EXTERNAL nodes via the interpreter if needed.
    virtual void eval_combinational(
        ch::data_map_t &data_map,
        const std::vector<std::pair<uint32_t, ch::instr_base *>>
            &input_instr_list,
        const std::vector<std::pair<uint32_t, ch::instr_base *>>
            &combinational_instr_list) = 0;

    // Per-tick: evaluate all sequential nodes (registers).
    virtual void eval_sequential(
        ch::data_map_t &data_map,
        const std::vector<std::pair<uint32_t, ch::instr_base *>>
            &sequential_instr_list) = 0;

    // Per-reset: zero all non-literal state.
    virtual void reset(ch::data_map_t &data_map) = 0;

    // Metadata
    virtual std::string name() const = 0;

    // True if this backend executes natively (i.e., NOT via the
    // interpreter's instr_base::eval() virtual dispatch). Used to
    // gate features that only make sense for the native path.
    virtual bool is_native() const { return false; }
};

} // namespace ch
