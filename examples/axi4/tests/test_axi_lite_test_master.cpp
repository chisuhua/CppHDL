// tests/test_axi_lite_test_master.cpp
//
// **`AxiLiteTestMaster` 的直接判别性用例**（`axi_soc_example` 的测试主设备）
//
// ⚠️ **为什么需要它**（`input-port-required-registry` R2/R3）：
//   该组件声明了 `rst_n`（永久复位类端口）⇒ ⇒ MUST 登记 ⇒ ⇒ 登记行 MUST 有
//   **判别性用例**（一个**实例化**该组件的文件）⇒ ⇒ 该组件此前只作为
//   `AxiSocTop` 的**成层子组件**存在 ⇒ ⇒ **没有**直接实例化它的文件
//   ⇒ ⇒ 2026-11-17（顶层组件上提进 `include/axi4/`）后 R2 要求登记 ⇒ ⇒ 本文件补上。
//
// ⚠️ **本用例断言的事实**（ADR-073 事实⑭ 的邻条）：
//   ① `rst_n` 是**纯发射前置**（`AxiLiteTestMaster::describe()` **不消费**它）
//      ⇒ ⇒ 拉低它**不改变**行为 ⇒ ⇒ 这里断言「行为不变」（= 把这条事实钉住）
//   ② 一笔写事务：拉 `start` + `write` ⇒ ⇒ `awvalid`/`wvalid` 拉高
//      ⇒ ⇒ 从端回 `bvalid` + `bready` ⇒ ⇒ `done` 出现**完成脉冲**
//
// ⚠️ **全部 23 个端口 MUST 显式驱动**（`ch_in` 未驱动 ⇒ 读回未定义值）。

#include "axi4/axi_soc_example_top.h"
#include "device.h"
#include "simulator.h"

#include <catch_amalgamated.hpp>

namespace {

using axi4::example::AxiLiteTestMaster;

/// 把从设备侧的输入全部驱动到一个**确定**状态（AXI4-Lite 从端不主动拉 valid）
void drive_slave_inputs(ch::Simulator &sim, axi4::example::AxiLiteTestMaster &m) {
    sim.set_input_value(m.io().awready, 1);
    sim.set_input_value(m.io().wready, 1);
    sim.set_input_value(m.io().bresp, 0);
    sim.set_input_value(m.io().bvalid, 0);
    sim.set_input_value(m.io().arready, 1);
    sim.set_input_value(m.io().rdata_axi, 0);
    sim.set_input_value(m.io().rresp, 0);
    sim.set_input_value(m.io().rvalid, 0);
}

void drive_master_inputs(ch::Simulator &sim, axi4::example::AxiLiteTestMaster &m) {
    sim.set_input_value(m.io().start, 0);
    sim.set_input_value(m.io().write, 0);
    sim.set_input_value(m.io().addr, 0);
    sim.set_input_value(m.io().wdata, 0);
}

/// 发起一笔写事务并**在沿前**观察（组合值）
void launch_write(ch::Simulator &sim, axi4::example::AxiLiteTestMaster &m, uint64_t addr,
                  uint64_t wdata) {
    sim.set_input_value(m.io().addr, addr);
    sim.set_input_value(m.io().wdata, wdata);
    sim.set_input_value(m.io().write, 1);
    sim.set_input_value(m.io().start, 1);
    sim.eval_combinational();
}

} // namespace

TEST_CASE("AxiLiteTestMaster: 写事务 ⇒ awvalid/wvalid ⇒ bready ⇒ done 完成脉冲", "[axi]") {
    ch::ch_device<AxiLiteTestMaster> dev;
    auto &m = dev.instance();
    ch::Simulator sim(dev.context());

    drive_master_inputs(sim, m);
    drive_slave_inputs(sim, m);
    sim.set_input_value(m.io().rst_n, 1);
    sim.tick();

    // ── 沿前：事务已发起（`awvalid`/`wvalid` 是组合输出）────────────────
    launch_write(sim, m, 0x1000, 0x1234);
    REQUIRE(static_cast<uint64_t>(sim.get_port_value(m.io().awvalid)) == 1);
    REQUIRE(static_cast<uint64_t>(sim.get_port_value(m.io().wvalid)) == 1);
    REQUIRE(static_cast<uint64_t>(sim.get_port_value(m.io().awaddr)) == 0x1000);
    REQUIRE(static_cast<uint64_t>(sim.get_port_value(m.io().wdata_axi)) == 0x1234);

    // ── 沿后：AW 握手完成 ⇒ `awvalid` 落下 · `state` 进入写态 ⇒ `bready` 拉高
    sim.tick();
    REQUIRE(static_cast<uint64_t>(sim.get_port_value(m.io().awvalid)) == 0);
    REQUIRE(static_cast<uint64_t>(sim.get_port_value(m.io().bready)) == 1);

    // ── 从端回响应 ⇒ ⇒ `done` 出现完成脉冲
    sim.set_input_value(m.io().bvalid, 1);
    sim.set_input_value(m.io().start, 0);
    sim.set_input_value(m.io().write, 0);
    sim.tick();
    REQUIRE(static_cast<uint64_t>(sim.get_port_value(m.io().done)) == 1);
}

TEST_CASE("AxiLiteTestMaster: `rst_n` 是纯发射前置（不消费 ⇒ 行为不变）", "[axi]") {
    ch::ch_device<AxiLiteTestMaster> dev;
    auto &m = dev.instance();
    ch::Simulator sim(dev.context());

    drive_master_inputs(sim, m);
    drive_slave_inputs(sim, m);
    // ⚠️ **拉低** rst_n（= 永久复位会造成的状态）
    sim.set_input_value(m.io().rst_n, 0);
    sim.tick();

    launch_write(sim, m, 0x1000, 0x1234);
    // ⇒ ⇒ 组件**不消费** rst_n ⇒ ⇒ 事务照常发起
    REQUIRE(static_cast<uint64_t>(sim.get_port_value(m.io().awvalid)) == 1);
    REQUIRE(static_cast<uint64_t>(sim.get_port_value(m.io().wvalid)) == 1);
    REQUIRE(static_cast<uint64_t>(sim.get_port_value(m.io().awaddr)) == 0x1000);
}
