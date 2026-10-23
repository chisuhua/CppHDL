/**
 * @file test_ifstage_device.cpp
 * @brief `ch_device<IfStage>` 直接实例化（无 wrapper）+ 行为断言
 *
 * ⚠️ 2026-10-23（change `riscv-mini-test-triage-2026-10`）：本文件此前只有
 * `SUCCEED`（0 断言）= 账面覆盖。`ch_device<T>` 让 T 成为**顶层**组件 ⇒
 * 它的 `ch_out` 就是顶层端口 ⇒ 可直接 `get_port_value` 观测，无需 wrapper。
 * 现断言 PC 复位值 + 推进 + stall 冻结。
 */

#include "../../../tests/catch_amalgamated.hpp"
#include "ch.hpp"
#include "simulator.h"
#include "../../include/cpu/pipeline/stages/if_stage.h"

using namespace ch::core;
using namespace riscv;

TEST_CASE("ch_device<IfStage>: construct + tick", "[ifdev]") {
    ch::ch_device<IfStage> if_stage;
    Simulator sim(if_stage.context());
    sim.set_input_value(if_stage.instance().io().stall, 0);
    sim.set_input_value(if_stage.instance().io().flush, 0);
    sim.set_input_value(if_stage.instance().io().rst, 0);
    sim.set_input_value(if_stage.instance().io().branch_target, 0);
    sim.set_input_value(if_stage.instance().io().branch_valid, 0);
    sim.set_input_value(if_stage.instance().io().instr_data, 0x00000013);
    sim.set_input_value(if_stage.instance().io().instr_ready, 1);
    sim.tick();

    // PC 复位值 0x80000000 + 本拍推进 4 ⇒ 0x80000004
    REQUIRE(sim.get_port_value(if_stage.instance().io().instr_addr) == 0x80000004u);

    // stall=1 ⇒ PC 冻结
    sim.set_input_value(if_stage.instance().io().stall, 1);
    sim.tick(3);
    REQUIRE(sim.get_port_value(if_stage.instance().io().instr_addr) == 0x80000004u);
}
