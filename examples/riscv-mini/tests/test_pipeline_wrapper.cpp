/**
 * @file test_pipeline_wrapper.cpp
 * @brief Minimal: just instantiate PipelineTestTop
 */

#include "../../../tests/catch_amalgamated.hpp"
#include "ch.hpp"
#include "simulator.h"
#include "component.h"
#include "../../include/cpu/pipeline/rv32i_pipeline.h"
#include "../../include/cpu/pipeline/rv32i_tcm.h"

using namespace ch::core;
using namespace riscv;

class PipelineTestTop : public ch::Component {
public:
    __io(
        ch_in<ch_uint<20>>  itcm_write_addr;
        ch_in<ch_uint<32>>  itcm_write_data;
        ch_in<ch_bool>       itcm_write_en;
        ch_in<ch_uint<20>>  dtcm_read_addr;
        ch_out<ch_uint<32>> dtcm_read_data;
        ch_in<ch_uint<20>>  dtcm_write_addr;
        ch_in<ch_uint<32>>  dtcm_write_data;
        ch_in<ch_bool>       dtcm_write_en;
    )

    PipelineTestTop(ch::Component* parent = nullptr, const std::string& name = "pipeline_top")
        : ch::Component(parent, name) {}

    void create_ports() override {
        new (io_storage_) io_type;
    }

    void describe() override {
        ch::ch_module<Rv32iPipeline> pipeline{"pipeline"};
        ch::ch_module<InstrTCM<20, 32>> itcm{"itcm"};
        ch::ch_module<DataTCM<20, 32>> dtcm{"dtcm"};

        itcm.io().addr <<= pipeline.io().instr_addr;
        pipeline.io().instr_data <<= itcm.io().data;
        pipeline.io().instr_ready <<= itcm.io().ready;

        dtcm.io().valid <<= ch_bool(true);
        pipeline.io().data_read_data <<= dtcm.io().rdata;
        pipeline.io().data_ready <<= dtcm.io().ready;

        pipeline.io().rst <<= ch_bool(false);
        pipeline.io().clk <<= ch_bool(true);

        itcm.io().write_addr <<= io().itcm_write_addr;
        itcm.io().write_data <<= io().itcm_write_data;
        itcm.io().write_en   <<= io().itcm_write_en;

        dtcm.io().addr <<= select(io().dtcm_write_en | pipeline.io().data_write_en,
                                  io().dtcm_write_addr,
                                  io().dtcm_read_addr);
        io().dtcm_read_data <<= dtcm.io().rdata;
        dtcm.io().wdata <<= select(io().dtcm_write_en, io().dtcm_write_data, pipeline.io().data_write_data);
        dtcm.io().write <<= io().dtcm_write_en | pipeline.io().data_write_en;
    }
};

TEST_CASE("PipelineTestTop: construct only", "[wrap][1]") {
    context ctx("wrap1");
    ctx_swap swap(&ctx);
    ch::ch_device<PipelineTestTop> top;
    SUCCEED("Construct OK");
}

TEST_CASE("PipelineTestTop: construct + tick", "[wrap][2]") {
    context ctx("wrap2");
    ctx_swap swap(&ctx);
    ch::ch_device<PipelineTestTop> top;
    Simulator sim(top.context());
    sim.tick();
    SUCCEED("Tick OK");
}

TEST_CASE("PipelineTestTop: construct + tick + set input", "[wrap][3]") {
    context ctx("wrap3");
    ctx_swap swap(&ctx);
    ch::ch_device<PipelineTestTop> top;
    Simulator sim(top.context());
    sim.set_input_value(top.instance().io().itcm_write_addr, 0);
    sim.set_input_value(top.instance().io().itcm_write_data, 0x00000013);
    sim.set_input_value(top.instance().io().itcm_write_en, 1);
    sim.tick();
    SUCCEED("Set input + tick OK");
}
