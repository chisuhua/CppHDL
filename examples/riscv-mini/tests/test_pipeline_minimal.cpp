#include "../../../tests/catch_amalgamated.hpp"
#include "ch.hpp"
#include "simulator.h"
#include "../../include/cpu/pipeline/rv32i_pipeline.h"

using namespace ch::core;
using namespace riscv;

// ⚠️ `ch_module` MUST 位于某个 `Component::describe()` 内部 ——
// 根 AGENTS.md §COMPONENT LIFECYCLE PATTERNS：TEST_CASE 顶层的 `ch_module`
// 没有 parent ⇒ `ch_module` ctor 早退（`include/module.h:47-50`）⇒
// 后续 `io()` 触发 `CHFATAL` ⇒ SIGSEGV。
//
// 这里用一个承载组件把 `ch_module<Rv32iPipeline>` 包起来；`ch_module` 的子组件
// 由 parent 的 `children_shared_` 持有 ⇒ `describe()` 返回后依然存活。
//
// ⚠️ 2026-10-23：本用例此前只有 `SUCCEED`（0 断言）= 账面覆盖 —— 它只能证明
// 「构造没崩」，不能证明「设计在动」。现在把 pipeline 的 `perf_cycles` 引到顶层
// `ch_out` 并断言复位期冻结在 0（`cycle_count->next = select(reset_active, 0, …)`）。
class PipelineSmokeTop : public ch::Component {
public:
    __io(ch_out<ch_uint<48>> perf_cycles;)

    PipelineSmokeTop(ch::Component* parent = nullptr, const std::string& name = "top")
        : ch::Component(parent, name) {}
    void create_ports() override { new (io_storage_) io_type; }
    void describe() override {
        ch::ch_module<Rv32iPipeline> pipeline{"pipeline"};
        pipeline.io().rst <<= ch_bool(true);
        pipeline.io().instr_data <<= ch_uint<32>(0x13_h);
        pipeline.io().instr_ready <<= ch_bool(true);
        pipeline.io().data_read_data <<= ch_uint<32>(0_d);
        pipeline.io().data_ready <<= ch_bool(true);
        io().perf_cycles <<= pipeline.io().perf_cycles;
    }
};

TEST_CASE("Pipeline instantiation only", "[pipeline][minimal]") {
    ch::ch_device<PipelineSmokeTop> top;
    ch::Simulator sim(top.context());
    sim.tick();

    // rst 硬接 true ⇒ 周期计数 MUST 冻结在 0
    REQUIRE(sim.get_port_value(top.instance().io().perf_cycles) == 0u);
}
