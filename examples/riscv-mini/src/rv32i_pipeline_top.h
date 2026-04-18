// rv32i_pipeline_top.h
#pragma once

#include "ch.hpp"
#include "component.h"
#include "cpu/pipeline/rv32i_pipeline.h"
#include "cpu/pipeline/rv32i_tcm.h"

using namespace ch::core;
using namespace riscv;

class Rv32iPipelineTop : public ch::Component {
public:
    __io(
        ch_in<ch_bool>      ext_rst;
        ch_in<ch_bool>      ext_clk;
        ch_in<ch_uint<32>>  ext_instr_data;
        ch_in<ch_bool>      ext_instr_ready;
        ch_in<ch_uint<32>>  ext_data_read_data;
        ch_in<ch_bool>      ext_data_ready;
    )

    Rv32iPipelineTop(ch::Component* parent = nullptr,
                     const std::string& name = "rv32i_pipeline_top")
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

        dtcm.io().addr <<= pipeline.io().data_addr;
        dtcm.io().wdata <<= pipeline.io().data_write_data;
        dtcm.io().write <<= pipeline.io().data_write_en;
        dtcm.io().valid <<= ch_bool(true);
        pipeline.io().data_read_data <<= dtcm.io().rdata;
        pipeline.io().data_ready <<= dtcm.io().ready;

        pipeline.io().rst <<= io().ext_rst;
        pipeline.io().clk <<= io().ext_clk;
    }
};