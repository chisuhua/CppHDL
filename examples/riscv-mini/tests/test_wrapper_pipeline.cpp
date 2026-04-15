/**
 * @file test_wrapper_pipeline.cpp
 * @brief Test ch_module<Rv32iPipeline> directly
 */

#include "../../../tests/catch_amalgamated.hpp"
#include "ch.hpp"
#include "simulator.h"
#include "component.h"
#include "../../include/cpu/pipeline/rv32i_pipeline.h"

using namespace ch::core;
using namespace riscv;

class PipelineWrapper : public ch::Component {
public:
    __io(
        ch_in<ch_uint<32>> instr_data;
        ch_in<ch_bool>      instr_ready;
        ch_in<ch_uint<32>>  data_read_data;
        ch_in<ch_bool>       data_ready;
    )

    PipelineWrapper(ch::Component* parent = nullptr, const std::string& name = "pipe_wrapper")
        : ch::Component(parent, name) {}
    void create_ports() override { new (io_storage_) io_type; }
    void describe() override {
        ch::ch_module<Rv32iPipeline> pipeline{"pipeline"};
        pipeline.io().rst <<= ch_bool(false);
        pipeline.io().clk <<= ch_bool(true);
        pipeline.io().instr_data <<= io().instr_data;
        pipeline.io().instr_ready <<= io().instr_ready;
        pipeline.io().data_read_data <<= io().data_read_data;
        pipeline.io().data_ready <<= io().data_ready;
    }
};

TEST_CASE("PipelineWrapper: construct only", "[pipe][1]") {
    context ctx("pipe1");
    ctx_swap swap(&ctx);
    ch::ch_device<PipelineWrapper> top;
    SUCCEED("OK");
}

TEST_CASE("PipelineWrapper: construct + tick", "[pipe][2]") {
    context ctx("pipe2");
    ctx_swap swap(&ctx);
    ch::ch_device<PipelineWrapper> top;
    Simulator sim(top.context());
    sim.set_input_value(top.instance().io().instr_data, 0x00000013);
    sim.set_input_value(top.instance().io().instr_ready, 1);
    sim.set_input_value(top.instance().io().data_read_data, 0);
    sim.set_input_value(top.instance().io().data_ready, 1);
    sim.tick();
    SUCCEED("OK");
}
