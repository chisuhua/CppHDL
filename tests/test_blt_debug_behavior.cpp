// ⚠️⚠️ **A5 试点**（2026-11-01 · change `dual-backend-parity-batch-2026-11`）
//
// 目的：**取真实成本数据**（转换形态 + 墙钟 + 是否暴露真分歧）⇒ ⇒
// ⇒ 然后据此出**批次清单**（⇒ ⇒ 盘点已警告「墙钟 MUST 实测」）。
//
// ⚠️ **形态 = 仓内既有 pattern**（`test_axi_spi_behavior` /
//    `test_rv32i_pipeline_behavior`）：**新增 `_behavior` 文件** ⇒ ⇒
// ⇒ **原文件 MUST NOT 修改**（保留 interpreter-only baseline）⇒ ⇒
// ⇒ ⇒ **零回归风险**（⇒ ⇒ 原文件的断言语义逐位不变）。
//
// ⚠️ **DUT** = `riscv::BranchConditionUnit`（组合逻辑，无 clk 风险）
//    ⇒ ⇒ 对应原用例 `tests/test_blt_debug.cpp`（**1 个 TEST_CASE**）
#include "catch_amalgamated.hpp"
#include "ch.hpp"
#include "component.h"
#include "device.h"
#include "dual_backend_fixture.h"
#include "simulator.h"

#include "cpu/pipeline/rv32i_branch_predict.h"

using namespace ch;
using namespace ch::core;
using namespace riscv;

// BranchConditionUnit - BLT 有符号比较（对应 `tests/test_blt_debug.cpp`）
//
// ⚠️ **期望值手推**：rs1 = 10（正）、rs2 = 20（正）⇒ ⇒ 有符号比较
//    `10 < 20` ⇒ ⇒ **true** ⇒ ⇒ `branch_condition == 1`
//    （⇒ ⇒ 原用例的 `REQUIRE(condition == 1)` 逐位沿用）
DUAL_BACKEND_TEST_PAIR(
    BranchConditionUnit,
    "BranchConditionUnit - BLT signed comparison",
    "[pipeline][branch][behavior][blt][dual]",
    [&]() {
        sim.set_input_value(dev.instance().io().branch_type, 4);
        sim.set_input_value(dev.instance().io().rs1_data, 10);
        sim.set_input_value(dev.instance().io().rs2_data, 20);

        sim.tick();

        auto condition = static_cast<uint64_t>(
            sim.get_port_value(dev.instance().io().branch_condition));

        REQUIRE(condition == 1);
    });
