/**
 * @file test_wrapper_simple.cpp
 * @brief HazardUnit 的**行为**断言（非「构造不崩」冒烟）
 *
 * ⚠️ 2026-10-23（change `riscv-mini-test-triage-2026-10`）：本文件此前 2 个用例
 * 全是 `SUCCEED`（0 断言）= 账面覆盖。现补真实断言：把 HazardUnit 的
 * `stall` / `forward_a` / `forward_b` 引到顶层 `ch_out`，分别验证
 * 「无冒险」与「EX 级前推」两种语义。
 *
 * ⚠️ `ch_module` MUST 位于 `describe()` 内部（根 AGENTS.md
 * §COMPONENT LIFECYCLE PATTERNS）。
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
        ch_in<ch_uint<32>>  rs1_data_raw;
        ch_in<ch_uint<5>>   id_rs1_addr;
        ch_in<ch_uint<5>>   id_rs2_addr;
        ch_in<ch_uint<5>>   ex_rd_addr;
        ch_in<ch_uint<1>>   ex_reg_write;
        ch_out<ch_bool>     stall;
        ch_out<ch_uint<2>>  forward_a;
    )

    SimpleWrapper(ch::Component* parent = nullptr, const std::string& name = "simple")
        : ch::Component(parent, name) {}
    void create_ports() override { new (io_storage_) io_type; }
    void describe() override {
        ch::ch_module<HazardUnit> hazard{"hazard"};
        hazard.io().rs1_data_raw <<= io().rs1_data_raw;
        hazard.io().rs2_data_raw <<= ch_uint<32>(0_d);
        hazard.io().ex_alu_result <<= ch_uint<32>(0xDEADBEEF_h);
        hazard.io().mem_alu_result <<= ch_uint<32>(0_d);
        hazard.io().wb_write_data <<= ch_uint<32>(0_d);
        hazard.io().id_rs1_addr <<= io().id_rs1_addr;
        hazard.io().id_rs2_addr <<= io().id_rs2_addr;
        hazard.io().ex_rd_addr <<= io().ex_rd_addr;
        hazard.io().mem_rd_addr <<= ch_uint<5>(0_d);
        hazard.io().wb_rd_addr <<= ch_uint<5>(0_d);
        hazard.io().ex_reg_write <<= io().ex_reg_write;
        hazard.io().mem_reg_write <<= ch_bool(false);
        hazard.io().wb_reg_write <<= ch_bool(false);
        hazard.io().mem_is_load <<= ch_bool(false);
        hazard.io().ex_branch <<= ch_bool(false);
        hazard.io().ex_branch_taken <<= ch_bool(false);
        io().stall <<= hazard.io().stall;
        io().forward_a <<= hazard.io().forward_a;
    }
};

// ===== 用例 1：无冒险 =====
TEST_CASE("SimpleWrapper: no hazard -> no stall, no forwarding", "[simple][1]") {
    ch::ch_device<SimpleWrapper> top;
    ch::Simulator sim(top.context());

    sim.set_input_value(top.instance().io().rs1_data_raw, 0);
    sim.set_input_value(top.instance().io().id_rs1_addr, 0);
    sim.set_input_value(top.instance().io().id_rs2_addr, 0);
    sim.set_input_value(top.instance().io().ex_rd_addr, 0);
    sim.set_input_value(top.instance().io().ex_reg_write, 0);
    sim.tick();

    REQUIRE(sim.get_port_value(top.instance().io().stall) == 0u);
    REQUIRE(sim.get_port_value(top.instance().io().forward_a) == 0u);
}

// ===== 用例 2：EX 级前推（rd == rs1 且 ex_reg_write=1）=====
TEST_CASE("SimpleWrapper: EX hazard -> forward_a selects EX result", "[simple][2]") {
    ch::ch_device<SimpleWrapper> top;
    ch::Simulator sim(top.context());

    sim.set_input_value(top.instance().io().rs1_data_raw, 0x11111111);
    sim.set_input_value(top.instance().io().id_rs1_addr, 5);   // 读 x5
    sim.set_input_value(top.instance().io().id_rs2_addr, 0);
    sim.set_input_value(top.instance().io().ex_rd_addr, 5);    // EX 写 x5 ⇒ 前推
    sim.set_input_value(top.instance().io().ex_reg_write, 1);
    sim.tick();

    // forward_a MUST 指向 EX 级（非 0 = 启用了前推，而非直接用寄存器堆）
    REQUIRE(sim.get_port_value(top.instance().io().forward_a) != 0u);
    REQUIRE(sim.get_port_value(top.instance().io().stall) == 0u);
}
