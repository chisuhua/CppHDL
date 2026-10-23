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
class PipelineSmokeTop : public ch::Component {
public:
    PipelineSmokeTop(ch::Component* parent = nullptr, const std::string& name = "top")
        : ch::Component(parent, name) {}
    void create_ports() override {}
    void describe() override {
        ch::ch_module<Rv32iPipeline> pipeline{"pipeline"};
        pipeline.io().rst <<= ch_bool(true);
        pipeline.io().instr_data <<= ch_uint<32>(0_d);
        pipeline.io().instr_ready <<= ch_bool(true);
        pipeline.io().data_read_data <<= ch_uint<32>(0_d);
        pipeline.io().data_ready <<= ch_bool(true);
    }
};

TEST_CASE("Pipeline instantiation only", "[pipeline][minimal]") {
    ch::ch_device<PipelineSmokeTop> top;
    ch::Simulator sim(top.context());
    sim.tick();

    SUCCEED("Pipeline instantiated and ticked");
}
