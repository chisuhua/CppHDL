/**
 * @file test_wrapper_pipeline.cpp
 * @brief `ch_module<Rv32iPipeline>` 直接包裹 + 行为断言
 *
 * ⚠️ 2026-10-23（change `riscv-mini-test-triage-2026-10`）：本文件此前 2 个用例
 * 全是 `SUCCEED`（0 断言）= 账面覆盖。现把 `perf_cycles` 引到顶层并断言
 * 「复位期冻结 / 释放后递增」。
 *
 * ⚠️ `ch_module` MUST 位于 `describe()` 内部（根 AGENTS.md
 * §COMPONENT LIFECYCLE PATTERNS）。
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
        ch_in<ch_uint<32>>  instr_data;
        ch_in<ch_uint<1>>   instr_ready;
        ch_in<ch_uint<32>>  data_read_data;
        ch_in<ch_uint<1>>   data_ready;
        ch_in<ch_uint<1>>   rst;
        ch_out<ch_uint<48>> perf_cycles;
    )

    PipelineWrapper(ch::Component* parent = nullptr, const std::string& name = "pipe_wrapper")
        : ch::Component(parent, name) {}
    void create_ports() override { new (io_storage_) io_type; }
    void describe() override {
        ch::ch_module<Rv32iPipeline> pipeline{"pipeline"};
        pipeline.io().rst <<= io().rst;
        pipeline.io().clk <<= ch_bool(true);
        pipeline.io().instr_data <<= io().instr_data;
        pipeline.io().instr_ready <<= io().instr_ready;
        pipeline.io().data_read_data <<= io().data_read_data;
        pipeline.io().data_ready <<= io().data_ready;
        io().perf_cycles <<= pipeline.io().perf_cycles;
    }
};

// ===== 用例 1：复位保持 ⇒ 周期计数冻结 =====
TEST_CASE("PipelineWrapper: cycles frozen while reset held", "[pipe][1]") {
    ch::ch_device<PipelineWrapper> top;
    ch::Simulator sim(top.context());

    sim.set_input_value(top.instance().io().rst, 1);
    sim.set_input_value(top.instance().io().instr_data, 0x00000013);
    sim.set_input_value(top.instance().io().instr_ready, 1);
    sim.set_input_value(top.instance().io().data_read_data, 0);
    sim.set_input_value(top.instance().io().data_ready, 1);
    sim.tick(4);

    // `cycle_count->next = select(reset_active, 0, cycle_count + 1)` ⇒ rst=1 时恒 0
    REQUIRE(sim.get_port_value(top.instance().io().perf_cycles) == 0u);
}

// ===== 用例 2：释放复位 ⇒ 周期计数递增 =====
TEST_CASE("PipelineWrapper: cycles advance after reset release", "[pipe][2]") {
    ch::ch_device<PipelineWrapper> top;
    ch::Simulator sim(top.context());

    sim.set_input_value(top.instance().io().instr_data, 0x00000013);
    sim.set_input_value(top.instance().io().instr_ready, 1);
    sim.set_input_value(top.instance().io().data_read_data, 0);
    sim.set_input_value(top.instance().io().data_ready, 1);

    sim.set_input_value(top.instance().io().rst, 1);
    sim.tick();
    auto held = sim.get_port_value(top.instance().io().perf_cycles);

    sim.set_input_value(top.instance().io().rst, 0);
    sim.tick(5);
    auto advanced = sim.get_port_value(top.instance().io().perf_cycles);

    REQUIRE(held == 0u);
    REQUIRE(advanced >= 5u);
}
