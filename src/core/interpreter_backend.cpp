// src/core/interpreter_backend.cpp
// ADR-035 / Phase 2.2: InterpreterBackend implementation.
// Wraps the existing instr_base::eval() virtual dispatch loop that was
// inlined in Simulator::eval_combinational() and eval_sequential().
// This is the default backend for environments without LLVM/Verilator.
#include "core/interpreter_backend.h"
#include "core/lnodeimpl.h"
#include "logger.h"

namespace ch {

bool InterpreterBackend::initialize(ch::core::context * /*ctx*/,
                                   ch::data_map_t & /*data_map*/) {
    CHDBG_FUNC();
    return true;
}

void InterpreterBackend::clear() {
    CHDBG_FUNC();
}

void InterpreterBackend::eval_combinational(
    ch::data_map_t &data_map,
    const std::vector<std::pair<uint32_t, ch::instr_base *>> &input_instr_list,
    const std::vector<std::pair<uint32_t, ch::instr_base *>>
        &combinational_instr_list) {
    for (const auto &kv : input_instr_list) {
        kv.second->eval();
        CHDBG("Evaluating input instruction for node %u: %s", kv.first,
              data_map[kv.first].to_string_verbose().c_str());
    }
    for (const auto &kv : combinational_instr_list) {
        kv.second->eval();
        CHDBG("Evaluating combinational instruction for node %u: %s",
              kv.first, data_map[kv.first].to_string_verbose().c_str());
    }
}

void InterpreterBackend::eval_sequential(
    ch::data_map_t &data_map,
    const std::vector<std::pair<uint32_t, ch::instr_base *>>
        &sequential_instr_list) {
    for (const auto &kv : sequential_instr_list) {
        kv.second->eval();
        CHDBG("Evaluating sequential instruction for node %u: %s", kv.first,
              data_map[kv.first].to_string_verbose().c_str());
    }
}

void InterpreterBackend::reset(ch::data_map_t &data_map) {
    // Minimal reset: zero all state. The Simulator's canonical reset
    // (which preserves literals) still lives in Simulator::reset();
    // this is the no-ctx fallback for callers that have only the
    // data_map reference.
    for (auto &pair : data_map) {
        pair.second = ch::core::sdata_type(0, pair.second.bitwidth());
    }
    (void)data_map;
}

} // namespace ch
