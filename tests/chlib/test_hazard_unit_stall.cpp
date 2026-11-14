// tests/chlib/test_hazard_unit_stall.cpp
//
// **ADR-069 V12 的正确落点** —— `riscv::HazardUnit` 的**组件级停顿链真值表**。
//
// ## 为什么是**这个**文件（V12 原文的订正）
//
// ADR-069 §5 **V12** 原文写「`tests/chlib/test_hazard_complete.cpp`」⇒ ⇒ 但该文件
// 已在 phase-6 **B0 批**（`rv32i-dead-component-consolidation` **C5+C6 同批退役**）
// **物理删除** ⇒ ⇒ **该判据按原文不可满足** ⇒ ⇒ V12 的订正指定：
// 「**新建** `HazardUnit` 的**组件级停顿链真值表**用例」⇒ ⇒ **载体 = 本文件**。
//
// ## 为什么必须是**组件级**（固件级不可替代）
//
// ① 固件级只能观测「停顿拍数」的**总数**（`perf_load_use_stalls`）⇒ ⇒ 判不了
//    「**哪一拍**开始停 / 停几拍 / 何时放行」⇒ ⇒ **零判别力**地钉住契约。
// ② 组件级可以**直接**驱动 `ex_is_load=1` ⇒ ⇒ 命中 `ex_load_use` 这一路 ——
//    它在固件里**只有**「紧邻 load→use」那一种形态 ⇒ ⇒ 覆盖面窄但**精确**。
// ③ ⚠️ **`ex_is_load` 的门控在固件级不可达**（`hazard_unit.h` 的注释明写）：
//    「该门控在**固件级不可达** ⇒ ⇒ 它的验收 MUST 走**组件级真值表**
//    （独立驱动 `ex_is_load=1`）」⇒ ⇒ **本文件就是那条门控的唯一验收面**。
//
// ## 拍级模型（**手推**，禁抄仿真输出）
//
// 停顿链 = 「组合置位 + **N** 级移位保持」（ADR-069 §2.1 · A0 后 N = 3）：
//
//   拍 T   ：组合置位（`ex_load_use`）⇒ 停顿**当拍**生效
//   拍 T+1 ：`stall_hold_d1` 为真
//   拍 T+2 ：`stall_hold_d2` 为真
//   拍 T+3 ：`stall_hold_d3` 为真
//   拍 T+4 ：全假 ⇒ 放行
//
// ⚠️ **但链的输入是「**当前**的检测结果」**（不是**「检测发生过的锁存」**）⇒ ⇒
// **移位链会被「重新装载」** ⇒ ⇒ 实测（两后端一致）：
//
//   · 检测条件**持续 N 拍** ⇒ ⇒ `stall` 高电平 **N + 2** 拍
//     ⇒ ⇒ ⚠️ **为什么 3 级链只贡献 2 拍尾巴**（critic 反问过，这里是推导）：
//        `d1 <= ex_load_use` 与「组合项 `load_use_hazard`」在**同一条沿**上生效
//        ⇒ ⇒ 条件在场的那些拍里，`d1` 与组合项**重叠**（不额外加拍）
//        ⇒ ⇒ 撤除后剩下的尾巴只有 `d2`（+1）与 `d3`（+1）= **2 拍**
//        ⇒ ⇒ 故 `N + 2`，而不是 `N + 3`
//   · 检测条件**掉一拍** ⇒ ⇒ 高电平**提前结束**（链被重新装载 ⇒ d2/d3 跟着清零）
//
// ⇒ ⇒ **流水线里的 4 拍**（ADR-069 V8）来自「检测条件在流水线里**持续 2 拍**」
// （ID 级被冻住 ⇒ 消费者与那条 load 的 rd 匹配关系**不变** ⇒ 检测被重复命中）
// ⇒ ⇒ **2 + 2 = 4** ✓ ⇒ ⇒ 与固件级实测的 `perf_load_use_stalls == 8`（2 处 × 4 拍）一致。
//
// ⇒ ⇒ **本表因此断言「条件持续 2 拍 ⇒ `stall` 高 4 拍」**（**唯一**能把「链的级数」
// 与「停顿拍数」绑在一起的形态）⇒ ⇒ 变异（删掉 `stall_hold_d3`）⇒ ⇒ **必红**。
//
// ## ⚠️ **两条「注释 vs 行为」的差异**（**如实登记** · 本文件钉的是**实测行为**）
//
// ① `hazard_unit.h` 的注释称移位链是「**固定延迟**」且「**不依赖 rd** ⇒ 免疫
//    「load 被冲刷」」⇒ ⇒ **实测不成立**：检测条件掉一拍 ⇒ ⇒ 停顿**提前结束**
//    （⇒ ⇒ 该「免疫」结论**只在流水线里**成立，因为那里的条件**持续**）。
// ② `hazard_unit.h` 称 `load_use_detected` 是「**单拍脉冲**」⇒ ⇒ **实测**：
//    它 = 检测条件的**组合**输出（条件持续 ⇒ 它持续为高）⇒ ⇒ **不是**自清脉冲。
//
// ⇒ ⇒ 这两条**不属本 change 的范围**（是**既有**行为）⇒ ⇒ **只登记，不改行为**
// ⇒ ⇒ 登记落点 = `.rddf/roadmap/phase-6-next-steps.md`（R14 的「仍欠」清单）。

#include "catch_amalgamated.hpp"

#include "ch.hpp"
#include "component.h"
#include "core/context.h"
#include "cpu/pipeline/hazard_unit.h"
#include "device.h"
#include "simulator.h"

#include <cstdint>

using namespace ch;

namespace {

// ⚠️ **全部输入端口 MUST 显式驱动**（未驱动 = 未定义值 ⇒ ⇒ 不得靠「碰巧通过」）
void drive_all(Simulator &sim, riscv::HazardUnit &hu) {
    auto &io = hu.io();
    sim.set_input_value(io.id_rs1_addr, 0u);
    sim.set_input_value(io.id_rs2_addr, 0u);
    sim.set_input_value(io.ex_rd_addr, 0u);
    sim.set_input_value(io.mem_rd_addr, 0u);
    sim.set_input_value(io.ex_reg_write, 0u);
    sim.set_input_value(io.mem_reg_write, 0u);
    sim.set_input_value(io.mem_is_load, 0u);
    sim.set_input_value(io.ex_is_load, 0u);
    sim.set_input_value(io.ex_branch, 0u);
    sim.set_input_value(io.ex_branch_taken, 0u);
    sim.set_input_value(io.rs1_data_raw, 0u);
    sim.set_input_value(io.rs2_data_raw, 0u);
}

// 构造 **EX 级** load-use 冒险：`ex_is_load=1` 且 `ex_rd_addr == id_rs1_addr`
void arm_load_use(Simulator &sim, riscv::HazardUnit &hu, uint64_t rd) {
    auto &io = hu.io();
    sim.set_input_value(io.ex_rd_addr, rd);
    sim.set_input_value(io.id_rs1_addr, rd);
    sim.set_input_value(io.ex_is_load, 1u);
}

void disarm_load_use(Simulator &sim, riscv::HazardUnit &hu) {
    auto &io = hu.io();
    sim.set_input_value(io.ex_is_load, 0u);
    sim.set_input_value(io.ex_rd_addr, 0u);
    sim.set_input_value(io.id_rs1_addr, 0u);
}

uint64_t stall_of(Simulator &sim, riscv::HazardUnit &hu) {
    return static_cast<uint64_t>(sim.get_port_value(hu.io().stall));
}

uint64_t pulse_of(Simulator &sim, riscv::HazardUnit &hu) {
    return static_cast<uint64_t>(sim.get_port_value(hu.io().load_use_detected));
}

}  // namespace

TEST_CASE("HazardUnit：EX 级 load-use 的**组件级停顿链真值表**（ADR-069 V12）",
          "[cpu][rv32i][hazard][stall][dual-agnostic]") {
    ch::ch_device<riscv::HazardUnit> dev;
    auto &hu = dev.instance();
    Simulator sim(dev.context());

    drive_all(sim, hu);
    sim.tick();
    // 前置：无冒险 ⇒ 停顿恒假
    REQUIRE(stall_of(sim, hu) == 0u);
    REQUIRE(pulse_of(sim, hu) == 0u);

    // ---- 检测条件**持续 2 拍**（= 流水线里的形态：ID 级被冻住 ⇒ 匹配关系不变）----
    arm_load_use(sim, hu, 5u);
    sim.tick();
    REQUIRE(stall_of(sim, hu) == 1u);   // 拍 T：组合置位
    REQUIRE(pulse_of(sim, hu) == 1u);   // 脉冲 = 检测条件（组合输出）

    sim.tick();
    REQUIRE(stall_of(sim, hu) == 1u);   // 拍 T+1：d1
    REQUIRE(pulse_of(sim, hu) == 1u);   // 条件仍在 ⇒ 组合输出仍高

    // 条件撤除（= 消费者已进 EX ⇒ 不再检测到）
    disarm_load_use(sim, hu);
    sim.tick();
    REQUIRE(stall_of(sim, hu) == 1u);   // 拍 T+2：d2
    REQUIRE(pulse_of(sim, hu) == 0u);   // 条件已撤 ⇒ 组合输出低

    sim.tick();
    REQUIRE(stall_of(sim, hu) == 1u);   // 拍 T+3：d3 ⇒ ⇒ **4 拍**（2 拍检测 + 2 拍尾部）
    REQUIRE(pulse_of(sim, hu) == 0u);

    sim.tick();
    REQUIRE(stall_of(sim, hu) == 0u);   // 拍 T+4：全假 ⇒ 放行
    REQUIRE(pulse_of(sim, hu) == 0u);
}

TEST_CASE("HazardUnit：**检测条件持续 N 拍 ⇒ 停顿 N+2 拍**（链的级数 ⇒ 停顿拍数）",
          "[cpu][rv32i][hazard][stall][dual-agnostic]") {
    // ⚠️ 这是**唯一**把「链的级数」与「停顿拍数」绑在一起的形态 ⇒ ⇒
    //    变异（删掉 `stall_hold_d3`）⇒ ⇒ 高电平少 1 拍 ⇒ ⇒ **必红**。
    for (int n = 1; n <= 3; ++n) {
        ch::ch_device<riscv::HazardUnit> dev;
        auto &hu = dev.instance();
        Simulator sim(dev.context());
        drive_all(sim, hu);
        sim.tick();

        arm_load_use(sim, hu, 5u);
        for (int k = 0; k < n; ++k) {
            sim.tick();
            REQUIRE(stall_of(sim, hu) == 1u);
        }
        disarm_load_use(sim, hu);

        // 撤除后**还剩 2 拍**（d2 / d3 的尾巴）⇒ ⇒ 高电平共 n + 2 拍
        sim.tick();
        REQUIRE(stall_of(sim, hu) == 1u);
        sim.tick();
        REQUIRE(stall_of(sim, hu) == 1u);
        sim.tick();
        REQUIRE(stall_of(sim, hu) == 0u);
    }
}

TEST_CASE("HazardUnit：三种「**看起来像**但**不是**」的形态 MUST NOT 触发停顿",
          "[cpu][rv32i][hazard][stall][dual-agnostic]") {
    struct Case {
        uint64_t ex_rd, id_rs1, ex_is_load;
        const char *why;
    };
    const Case kNoHazard[] = {
        {5u, 6u, 1u, "rd 不匹配"},
        {5u, 5u, 0u, "不是 load"},
        {0u, 0u, 1u, "rd == x0"},
    };

    for (const auto &tc : kNoHazard) {
        ch::ch_device<riscv::HazardUnit> dev;
        auto &hu = dev.instance();
        auto &io = hu.io();
        Simulator sim(dev.context());
        drive_all(sim, hu);
        sim.set_input_value(io.ex_rd_addr, tc.ex_rd);
        sim.set_input_value(io.id_rs1_addr, tc.id_rs1);
        sim.set_input_value(io.ex_is_load, tc.ex_is_load);
        sim.set_input_value(io.ex_reg_write, 1u);
        sim.tick();
        INFO("形态：" << tc.why);
        REQUIRE(stall_of(sim, hu) == 0u);
        REQUIRE(pulse_of(sim, hu) == 0u);
        sim.tick();
        REQUIRE(stall_of(sim, hu) == 0u);
    }
}
