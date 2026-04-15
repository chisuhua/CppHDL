/**
 * @file test_wrapper_simple.cpp
 * @brief Test ch_module inside Component::describe() with minimal component
 */

#include "../../../tests/catch_amalgamated.hpp"
#include "ch.hpp"
#include "simulator.h"
#include "component.h"
#include "../../include/cpu/pipeline/hazard_unit.h"

using namespace ch::core;
using namespace riscv;

class SimpleWrapper : public ch::Component {
public:
    __io(
        ch_in<ch_uint<32>> rs1_data_raw;
    )

    SimpleWrapper(ch::Component* parent = nullptr, const std::string& name = "simple")
        : ch::Component(parent, name) {}
    void create_ports() override { new (io_storage_) io_type; }
    void describe() override {
        ch::ch_module<HazardUnit> hazard{"hazard"};
        hazard.io().rs1_data_raw <<= io().rs1_data_raw;
        hazard.io().rs2_data_raw <<= ch_uint<32>(0_d);
        hazard.io().ex_alu_result <<= ch_uint<32>(0_d);
        hazard.io().mem_alu_result <<= ch_uint<32>(0_d);
        hazard.io().wb_write_data <<= ch_uint<32>(0_d);
        hazard.io().id_rs1_addr <<= ch_uint<5>(0_d);
        hazard.io().id_rs2_addr <<= ch_uint<5>(0_d);
        hazard.io().ex_rd_addr <<= ch_uint<5>(0_d);
        hazard.io().mem_rd_addr <<= ch_uint<5>(0_d);
        hazard.io().wb_rd_addr <<= ch_uint<5>(0_d);
        hazard.io().ex_reg_write <<= ch_bool(false);
        hazard.io().mem_reg_write <<= ch_bool(false);
        hazard.io().wb_reg_write <<= ch_bool(false);
        hazard.io().mem_is_load <<= ch_bool(false);
        hazard.io().ex_branch <<= ch_bool(false);
        hazard.io().ex_branch_taken <<= ch_bool(false);
    }
};

TEST_CASE("SimpleWrapper: construct only", "[simple][1]") {
    context ctx("simple1");
    ctx_swap swap(&ctx);
    ch::ch_device<SimpleWrapper> top;
    SUCCEED("OK");
}

TEST_CASE("SimpleWrapper: construct + tick", "[simple][2]") {
    context ctx("simple2");
    ctx_swap swap(&ctx);
    ch::ch_device<SimpleWrapper> top;
    Simulator sim(top.context());
    sim.tick();
    SUCCEED("OK");
}
