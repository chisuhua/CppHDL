// ⚠️⚠️ **A5 批次 B1**（2026-11-01 · change `dual-backend-parity-batch-b-2026-11`）
//
// ⚠️ **形态 = 仓内既有 pattern**：**新增 `_behavior` 文件** ⇒ ⇒
// ⇒ **原 `tests/chlib/test_assert.cpp` MUST NOT 修改**（保留 interpreter-only
// ⇒ ⇒ baseline）⇒ ⇒ ⇒ **零回归**。
//
// ⚠️⚠️ **为什么只转这 4 个用例**（**筛选纪律**，不是随手挑）：
//   ① `AssertChecker`（DUT = `chlib::AssertChecker`，**库头类型**）⇒ ⇒
//      ⇒ **零局部类** ⇒ ⇒ **零「把内联 DUT 提升到共享头」的改动**;
//   ② 每个用例**恰 1 条 `REQUIRE`** ⇒ ⇒ ⇒ 满足批次清单 §4 的判别力模板
//      （「总断言数 == 2 × pair 数」**只在每 pair 恰 1 条 REQUIRE 时成立**）;
//   ③ ⚠️ **原文件的 2 个宏用例（`CH_ASSERT` / `ch_assert`）MUST NOT 转**：
//      它们断言的是**重定向 `std::cout` 捕获到的 C++ 侧打印**
//      ⇒ ⇒ **不是硬件信号** ⇒ ⇒ ⇒ **双后端对它【毫无意义】**
//      ⇒ ⇒ ⇒ ⇒ **⇒ 且 stdout 重定向是【全局状态】⇒ ⇒ 双跑结构下会互相干扰**
//      ⇒ ⇒ ⇒ ⇒ ⇒ **⇒ 排除**（⇒ ⇒ 不是「忘了转」，是**不该转**）。
//
// ⚠️⚠️ **期望值全部【手推】**（MUST NOT 抄仿真输出）：
//   `AssertChecker` 是**纯组合** ⇒ ⇒ `failed = enable & !condition`
//   ⇒ ⇒ 4 个用例的真值表逐条手推（见每个 pair 的注释）⇒ ⇒ 与实现无耦合。
//
// ⚠️ **接线**：`AssertChecker` **无 ready / 无 rst_n**（纯 `ch_in` × 2 +
//   `ch_out` × 1）⇒ ⇒ ⇒ **「`as_master()` + `<<=`」纪律在本文件【不涉及
//   ready】** ⇒ ⇒ ⇒ 但**输入驱动**一律走 `sim.set_input_value`
//   ⇒ ⇒ ⇒ ⇒ **⇒ 禁** `io().X = value` 形态（那在 `ch_in` 上是**静默失效**）。
#include "catch_amalgamated.hpp"
#include "ch.hpp"
#include "component.h"
#include "device.h"
#include "dual_backend_fixture.h"
#include "simulator.h"

#include "chlib/assert.h"

using namespace ch;
using namespace ch::core;
using namespace ch::chlib;

// ── ① enable=1 ∧ condition=0 ⇒ ⇒ failed = 1（手推：1 ∧ ¬0 = 1）────────
DUAL_BACKEND_TEST_PAIR(
    AssertChecker,
    "AssertChecker: condition=false with enable=true reports failed",
    "[chlib][assert][checker][fail][dual]",
    [&]() {
        sim.set_input_value(dev.instance().io().enable, true);
        sim.set_input_value(dev.instance().io().condition, false);

        sim.tick();

        REQUIRE(static_cast<uint64_t>(
                    sim.get_port_value(dev.instance().io().failed)) == 1);
    });

// ── ② enable=1 ∧ condition=1 ⇒ ⇒ failed = 0（手推：1 ∧ ¬1 = 0）────────
DUAL_BACKEND_TEST_PAIR(
    AssertChecker,
    "AssertChecker: condition=true with enable=true reports no failure",
    "[chlib][assert][checker][pass][dual]",
    [&]() {
        sim.set_input_value(dev.instance().io().enable, true);
        sim.set_input_value(dev.instance().io().condition, true);

        sim.tick();

        REQUIRE(static_cast<uint64_t>(
                    sim.get_port_value(dev.instance().io().failed)) == 0);
    });

// ── ③ enable=0 ⇒ ⇒ failed = 0（手推：0 ∧ ¬0 = 0，**与 condition 无关**）──
DUAL_BACKEND_TEST_PAIR(
    AssertChecker,
    "AssertChecker: enable=false suppresses reporting regardless of condition",
    "[chlib][assert][checker][disabled][dual]",
    [&]() {
        sim.set_input_value(dev.instance().io().enable, false);
        sim.set_input_value(dev.instance().io().condition, false);

        sim.tick();

        REQUIRE(static_cast<uint64_t>(
                    sim.get_port_value(dev.instance().io().failed)) == 0);
    });

// ── ④ 输入变化在**后续拍**传播（手推：拍 1 = 1 ∧ ¬0 = 1；拍 2 = 1 ∧ ¬1 = 0）─
DUAL_BACKEND_TEST_PAIR(
    AssertChecker,
    "AssertChecker: input changes propagate on subsequent ticks",
    "[chlib][assert][checker][toggle][dual]",
    [&]() {
        sim.set_input_value(dev.instance().io().enable, true);
        sim.set_input_value(dev.instance().io().condition, false);

        sim.tick();
        // 手推：拍 1 ⇒ enable=1 ∧ condition=0 ⇒ failed = 1
        REQUIRE(static_cast<uint64_t>(
                    sim.get_port_value(dev.instance().io().failed)) == 1);

        // 手推：拍 2 ⇒ condition 改为 1 ⇒ failed = 0
        sim.set_input_value(dev.instance().io().condition, true);
        sim.tick();
        REQUIRE(static_cast<uint64_t>(
                    sim.get_port_value(dev.instance().io().failed)) == 0);
    });
