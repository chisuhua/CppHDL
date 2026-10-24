/**
 * @file test_wrapper_ifstage.cpp
 * @brief IfStage 的**行为**断言（PC 复位值 + 流水寄存器锁存 + stall 冻结）
 *
 * ⚠️ 2026-10-23（change `riscv-mini-test-triage-2026-10`）：本文件此前 2 个用例
 * 全是 `SUCCEED`（0 断言）= 账面覆盖。现把 `instr_addr` / `pc` / `instruction`
 * 引到顶层 `ch_out` 并断言真实语义。
 *
 * ⚠️ `ch_module` MUST 位于 `describe()` 内部（根 AGENTS.md
 * §COMPONENT LIFECYCLE PATTERNS）。
 */

#include "../../../tests/catch_amalgamated.hpp"
#include "ch.hpp"
#include "simulator.h"
#include "component.h"
#include "cpu/pipeline/stages/if_stage.h"

using namespace ch::core;
using namespace riscv;

class IfStageWrapper : public ch::Component {
public:
    __io(
        ch_in<ch_uint<32>>  instr_data;
        ch_in<ch_uint<1>>   instr_ready;
        ch_in<ch_uint<1>>   stall;
        ch_out<ch_uint<32>> instr_addr;
        ch_out<ch_uint<32>> pc;
        ch_out<ch_uint<32>> instruction;
    )

    IfStageWrapper(ch::Component* parent = nullptr, const std::string& name = "if_wrapper")
        : ch::Component(parent, name) {}
    void create_ports() override { new (io_storage_) io_type; }
    void describe() override {
        ch::ch_module<IfStage> if_stage{"if_stage"};
        if_stage.io().stall <<= io().stall;
        if_stage.io().flush <<= ch_bool(false);
        if_stage.io().rst <<= ch_bool(false);
        if_stage.io().branch_target <<= ch_uint<32>(0_d);
        if_stage.io().branch_valid <<= ch_bool(false);
        if_stage.io().instr_data <<= io().instr_data;
        if_stage.io().instr_ready <<= io().instr_ready;
        io().instr_addr <<= if_stage.io().instr_addr;
        io().pc <<= if_stage.io().pc;
        io().instruction <<= if_stage.io().instruction;
    }
};

// ===== 用例 1：PC 复位值 + 本拍推进 =====
TEST_CASE("IfStageWrapper: pc resets to ITCM base then advances", "[if][1]") {
    ch::ch_device<IfStageWrapper> top;
    ch::Simulator sim(top.context());

    sim.set_input_value(top.instance().io().instr_data, 0x00000013);
    sim.set_input_value(top.instance().io().instr_ready, 1);
    sim.set_input_value(top.instance().io().stall, 0);
    sim.tick();

    // `instr_addr` = pc_reg（if_stage.h:109）；pc_reg 的 INIT = 0x80000000
    // （if_stage.h:66），instr_ready=1 且 stall=0 ⇒ 本拍前进 4
    REQUIRE(sim.get_port_value(top.instance().io().instr_addr) == 0x80000004u);
}

// ===== 用例 2：IF/ID 流水寄存器锁存 + stall 冻结 PC =====
TEST_CASE("IfStageWrapper: IF/ID latch holds, stall freezes PC", "[if][2]") {
    ch::ch_device<IfStageWrapper> top;
    ch::Simulator sim(top.context());

    sim.set_input_value(top.instance().io().instr_data, 0x00000013);  // ADDI x0,x0,0
    sim.set_input_value(top.instance().io().instr_ready, 1);
    sim.set_input_value(top.instance().io().stall, 0);
    sim.tick();

    // `pc` / `instruction` 是 **IF/ID 流水寄存器**的输出（if_stage.h:165-166）
    // ⇒ 它们锁存的是**上一拍**的 pc 与取到的指令
    REQUIRE(sim.get_port_value(top.instance().io().pc) == 0x80000000u);
    REQUIRE(sim.get_port_value(top.instance().io().instruction) == 0x00000013u);

    // stall=1 ⇒ 当前 PC（`instr_addr`）MUST 冻结
    auto frozen = sim.get_port_value(top.instance().io().instr_addr);
    sim.set_input_value(top.instance().io().stall, 1);
    sim.tick(3);
    REQUIRE(sim.get_port_value(top.instance().io().instr_addr) == frozen);
}
