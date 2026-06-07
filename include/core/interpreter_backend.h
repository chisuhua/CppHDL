// include/core/interpreter_backend.h
// ADR-035 / Phase 2.2: InterpreterBackend — wraps the existing
// instr_base::eval() virtual dispatch loop that was inlined in
// Simulator::eval_combinational() and Simulator::eval_sequential().
#pragma once

#include "core/eval_backend.h"
#include <string>
#include <vector>

namespace ch {

class InterpreterBackend : public IEvalBackend {
public:
    InterpreterBackend() = default;
    ~InterpreterBackend() override = default;

    bool initialize(ch::core::context *ctx,
                    ch::data_map_t &data_map) override;

    void clear() override;

    void eval_combinational(
        ch::data_map_t &data_map,
        const std::vector<std::pair<uint32_t, ch::instr_base *>>
            &input_instr_list,
        const std::vector<std::pair<uint32_t, ch::instr_base *>>
            &combinational_instr_list) override;

    void eval_sequential(
        ch::data_map_t &data_map,
        const std::vector<std::pair<uint32_t, ch::instr_base *>>
            &sequential_instr_list) override;

    void reset(ch::data_map_t &data_map) override;

    std::string name() const override { return "interpreter"; }
    bool is_native() const override { return false; }
};

} // namespace ch
