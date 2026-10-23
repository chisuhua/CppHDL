/**
 * @file test_pipeline_wrapper.cpp
 * @brief `ch_module<Rv32iPipeline>` + ITCM/DTCM 的装配 + 行为断言
 *
 * ⚠️ 2026-10-23（change `riscv-mini-test-triage-2026-10`）：
 * 本文件此前 3 个用例**全是 `SUCCEED`（0 断言）**= 账面覆盖。现补真实断言：
 * ① 复位期周期计数冻结；② ITCM 写入后能被流水线取到（`instr_addr` 推进）；
 * ③ 释放复位后周期计数开始递增。
 *
 * ⚠️ `ch_module` MUST 位于 `describe()` 内部（根 AGENTS.md
 * §COMPONENT LIFECYCLE PATTERNS）—— 本文件原本就是正确写法，保留。
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
        ch_in<ch_uint<1>>    rst;
        ch_in<ch_uint<20>>   itcm_write_addr;
        ch_in<ch_uint<32>>   itcm_write_data;
        ch_in<ch_uint<1>>    itcm_write_en;
        ch_out<ch_uint<32>>  instr_addr;
        ch_out<ch_uint<48>>  perf_cycles;
    )

    PipelineTestTop(ch::Component* parent = nullptr, const std::string& name = "pipeline_top")
        : ch::Component(parent, name) {}

    void create_ports() override {
        new (io_storage_) io_type;
    }

    void describe() override {
        ch::ch_module<Rv32iPipeline> pipeline{"pipeline"};
        ch::ch_module<InstrTCM<20, 32>> itcm{"itcm"};

        itcm.io().addr <<= pipeline.io().instr_addr;
        pipeline.io().instr_data <<= itcm.io().data;
        pipeline.io().instr_ready <<= itcm.io().ready;

        itcm.io().write_addr <<= io().itcm_write_addr;
        itcm.io().write_data <<= io().itcm_write_data;
        itcm.io().write_en   <<= io().itcm_write_en;

        // 数据侧接一个恒 ready 的哑端口（本用例只观测取指路径）
        pipeline.io().data_read_data <<= ch_uint<32>(0_d);
        pipeline.io().data_ready <<= ch_bool(true);

        pipeline.io().rst <<= io().rst;
        pipeline.io().clk <<= ch_bool(true);

        io().instr_addr <<= pipeline.io().instr_addr;
        io().perf_cycles <<= pipeline.io().perf_cycles;
    }
};

// ===== 用例 1：构造 + 复位保持 =====
TEST_CASE("PipelineTestTop: construct + reset held", "[pipe][1]") {
    ch::ch_device<PipelineTestTop> top;
    ch::Simulator sim(top.context());

    sim.set_input_value(top.instance().io().rst, 1);
    sim.set_input_value(top.instance().io().itcm_write_en, 0);
    sim.tick();

    // rst=1 ⇒ 周期计数冻结在 0
    REQUIRE(sim.get_port_value(top.instance().io().perf_cycles) == 0u);
    // PC 复位值 = ITCM 基址 0x80000000（if_stage.h:66 的 pc_reg INIT）
    REQUIRE(sim.get_port_value(top.instance().io().instr_addr) == 0x80000000u);
}

// ===== 用例 2：向 ITCM 写入后，取指地址推进 =====
TEST_CASE("PipelineTestTop: ITCM write then fetch advances PC", "[pipe][2]") {
    ch::ch_device<PipelineTestTop> top;
    ch::Simulator sim(top.context());

    sim.set_input_value(top.instance().io().rst, 0);
    sim.set_input_value(top.instance().io().itcm_write_en, 1);
    // PC 从 0x80000000 开始 ⇒ ITCM 索引 = (pc >> 2) & 0xFFFFF = 0
    sim.set_input_value(top.instance().io().itcm_write_addr, 0);
    sim.set_input_value(top.instance().io().itcm_write_data, 0x00000013);  // NOP
    sim.tick();
    sim.set_input_value(top.instance().io().itcm_write_en, 0);

    auto pc_before = sim.get_port_value(top.instance().io().instr_addr);
    sim.tick(4);
    auto pc_after = sim.get_port_value(top.instance().io().instr_addr);

    // 取指路径活着 ⇒ PC 必须推进（若 ITCM 接线断了，PC 会卡住）
    REQUIRE(pc_after > pc_before);
}

// ===== 用例 3：释放复位后周期计数递增 =====
TEST_CASE("PipelineTestTop: cycles advance after reset release", "[pipe][3]") {
    ch::ch_device<PipelineTestTop> top;
    ch::Simulator sim(top.context());

    sim.set_input_value(top.instance().io().rst, 1);
    sim.set_input_value(top.instance().io().itcm_write_en, 0);
    sim.tick();
    auto held = sim.get_port_value(top.instance().io().perf_cycles);

    sim.set_input_value(top.instance().io().rst, 0);
    sim.tick(5);
    auto advanced = sim.get_port_value(top.instance().io().perf_cycles);

    REQUIRE(held == 0u);
    REQUIRE(advanced >= 5u);
}
