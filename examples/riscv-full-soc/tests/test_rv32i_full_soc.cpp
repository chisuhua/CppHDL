// examples/riscv-full-soc/tests/test_rv32i_full_soc.cpp
//
// **full 产品线空壳的判据**（ADR-078 · S4c-0）
//
// ⚠️ **本文件只判「空壳形态」**（端口面 + 恒定输出）—— **不**判任何功能
//    （功能分属 S4c-1 取指/I-L1 · S4c-2 DMA · S4c-3 互连/L2）。
//
// ⚠️ **为什么空壳也要有判据**：新线的存在意义 = 「**立即进入**所有护栏的扫描面」
//    ⇒ ⇒ 本文件的断言是那把探针的**自证**（它证明空壳可构建、可运行、端口面如登记）。

#include "catch_amalgamated.hpp"
#include "ch.hpp"
#include "component.h"
#include "device.h"
#include "rv32i_full_soc_top.h"
#include "simulator.h"

#include <cstdint>

using riscv_full_soc::Rv32iFullSocTop;
using riscv_full_soc::kFullLineId;
using riscv_full_soc::kFullSocTopPorts;

TEST_CASE("full-soc 空壳：端口面是框架 2 + 根自有 2", "[full-soc]") {
    // ⚠️ **期望值手推**（不是抄实现）：`__io` 声明了 2 根 `ch_out` ⇒ ⇒ 根端口面
    // = 框架 2（`default_clock` / `default_reset`）+ 2 = **4**。
    REQUIRE(kFullSocTopPorts == 4u);

    auto dev = std::make_unique<ch::ch_device<Rv32iFullSocTop>>();
    Simulator sim(dev->context());
    auto &io = dev->instance().io();

    // ⚠️ `line_ready` 恒 true（空壳无输入 ⇒ 无复位态差异 ⇒ 恒定）
    sim.tick(1);
    REQUIRE(static_cast<uint64_t>(sim.get_port_value(io.line_ready)) == 1u);
}

TEST_CASE("full-soc 空壳：line_id == 产品线序号 2", "[full-soc]") {
    auto dev = std::make_unique<ch::ch_device<Rv32iFullSocTop>>();
    Simulator sim(dev->context());
    auto &io = dev->instance().io();

    // ⚠️ **手推**：`kFullLineId = 2`（`riscv-mini` = 1 · `riscv-full` = 2）⇒ ⇒ 读回 2。
    sim.tick(1);
    REQUIRE(static_cast<uint64_t>(sim.get_port_value(io.line_id)) == 2u);
}

TEST_CASE("full-soc 空壳：输出**跨拍恒定**（空壳无状态）", "[full-soc]") {
    auto dev = std::make_unique<ch::ch_device<Rv32iFullSocTop>>();
    Simulator sim(dev->context());
    auto &io = dev->instance().io();

    // ⚠️ **负向对照**：若空壳被误接了状态（寄存器 / 计数），本用例会在第二拍读到不同值
    //   ⇒ ⇒ 它证明「空壳确实无状态」⇒ ⇒ 未来加功能时**本用例 MUST 同步改写**
    //   （否则它会变成一条恒绿的无判别力断言）。
    sim.tick(1);
    const uint64_t id1 = static_cast<uint64_t>(sim.get_port_value(io.line_id));
    const uint64_t ready1 = static_cast<uint64_t>(sim.get_port_value(io.line_ready));
    REQUIRE(id1 == static_cast<uint64_t>(kFullLineId));
    REQUIRE(ready1 == 1u);

    for (int i = 0; i < 7; ++i) {
        sim.tick(1);
    }
    REQUIRE(static_cast<uint64_t>(sim.get_port_value(io.line_id)) == id1);
    REQUIRE(static_cast<uint64_t>(sim.get_port_value(io.line_ready)) == ready1);
}

TEST_CASE("full-soc 空壳：端口**具名**（ADR-075 E1）", "[full-soc]") {
    // ⚠️ **端口名 MUST 可解析到 C++ 成员名**（ADR-075 E1）⇒ ⇒ 具名构造给出字符串字面量
    //   ⇒ ⇒ 本用例断言「端口名 == 成员名」，而不是「端口存在」。
    auto dev = std::make_unique<ch::ch_device<Rv32iFullSocTop>>();
    auto &io = dev->instance().io();

    REQUIRE(io.line_id.name() == "line_id");
    REQUIRE(io.line_ready.name() == "line_ready");
}
