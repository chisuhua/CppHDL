# Change: dual-backend-parity-batch-2026-11

> ⚠️ **本 change = T7 的【批次 A】（试点）** —— 详见
> `.rddf/plans/dual-backend-full-coverage-2026-11.md`（批次清单 + 墙钟预算）。
>
> ⚠️⚠️ **零 spec 变更声明**（归档时**允许** `--skip-specs` 的依据）：
> 新增的 `_behavior` 文件**不违反** capability `dual-backend-test-coverage`
> 的任何条款 ⇒ ⇒ ⇒ **无 Requirement 需新增 / 修改 / 退役**
> ⇒ ⇒ ⇒ ⇒ ⇒ **⇒ 计数锚点（`AGENTS.md`）与 objective 的 Go 判据都不是
> spec 面** ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ **⇒ 故本 change 显式声明「no spec changes」**。

## Why

🔴 **objective `objective-test-coverage-100pct` 的 Go 判据** =
「双后端 parity **源文件数** ≥ 60」⇒ ⇒ **现状 29** ⇒ ⇒ **差 31**
⇒ ⇒ ⇒ **⇒ 但「只转换既有文件」这条路【到不了 60】**（见 design.md §2）⇒ ⇒
⇒ ⇒ **⇒ 必须先把「可达上限」量化清楚**（T7 步骤 1 的 **planner 决策门**）
⇒ ⇒ ⇒ ⇒ ⇒ **⇒ 本 change = 批次 A（试点）：用最小成本实测【转换形态 +
墙钟 + 是否暴露真分歧】** ⇒ ⇒ ⇒ ⇒ ⇒ **⇒ 据此出批次清单**。

⚠️ **为什么先做试点而不是直接批量**：T7 明写「每批的**真实**墙钟
**必须实测**（`RESOURCE_LOCK` 会把 verilator 测试**串行化**⇒ ⇒ 墙钟不可
拍脑袋）」⇒ ⇒ ⇒ **⇒ 墙钟是 CI 预算（45 min）的直接输入** ⇒ ⇒ ⇒ ⇒ ⇒
**⇒ 不先实测就分批 ⇒ ⇒ 分批依据是拍的**。

## What Changes

### ① 新增 `tests/test_blt_debug_behavior.cpp`（**1 个 dual pair**）

- **形态 = 仓内既有 pattern**（`test_axi_spi_behavior` /
  `test_rv32i_pipeline_behavior`）：**新增 `_behavior` 文件**
  ⇒ ⇒ **原 `tests/test_blt_debug.cpp` MUST NOT 修改**
  （保留 interpreter-only baseline ⇒ ⇒ **零回归**）；
- DUT = `riscv::BranchConditionUnit`（**纯组合逻辑** ⇒ 无 clk 风险）；
- ⚠️ **接线 MUST 用 `as_master()` + `<<=`**（**禁** `operator=` 喂常量）
  ⇒ ⇒ 理由：A4 实测发现「假绿的新形态」—— 用 `operator=` 喂常量 ready
  ⇒ ⇒ 会**恰好绕过**真实依赖 ⇒ ⇒ 把「常量接线下能过」固化成「真实用法能过」；
  （⚠️ 本 DUT 无 ready 端口 ⇒ ⇒ 该纪律在本 pair 尚不触发 ⇒ ⇒ 但**后续批次
  MUST 遵守** ⇒ ⇒ 已写进批次清单的验收模板）

### ② 计数锚点同步（**每批必做**）

| 锚点 | 前 | 后 |
|---|---|---|
| `CTEST_TOTAL` | 205 | **206** |
| `VERILATOR_E2E_TOTAL` | 38 | **39** |
| `DUAL_BACKEND_PARITY_TOTAL` | 29 | **30** |
| `DUAL_BACKEND_PARITY_PAIRS` | 184 | **185** |
| `tests/test_*.cpp` 文件数 | 118 | **119** |

⚠️ 且根 `AGENTS.md` 的**叙述值**（「206 CTest tests registered」·
「195 non-perf」·「现值 30」）**同步更新** ⇒ ⇒ 否则**锚点与叙述漂移**。

### ③ 批次清单（`.rddf/plans/dual-backend-full-coverage-2026-11.md`）

量化盘点 + 成本模型（实测墙钟）+ **8 批**清单 + **墙钟账本** + 风险对策。

## 为什么不做什么（Non-Goals）

- ❌ **不**在本 change 里做批次 B–H ⇒ ⇒ 它们各有独立的墙钟/风险档
  ⇒ ⇒ ⇒ **每批一个 change**（T7 步骤 2）；
- ❌ **不**改 objective 的 Go 判据（**planner 职责**）⇒ ⇒⇒ 本 change 只
  **交数字**（可达上限 58 < 60）；
- ❌ **不**改 capability `dual-backend-test-coverage` 的排除清单 ⇒ ⇒⇒
  ⚠️ 其中「Catch2 SECTION 重入」这一条**措辞可能已陈旧**（宏是**两个独立
  TEST_CASE** ⇒ ⇒ SECTION 在每个后端各跑一次 ⇒ ⇒ 语义正确）⇒ ⇒
  ⇒ ⇒ **登记为待 planner 复核**（**不在本 change 擅自改契约**）；
- ❌ **不**在覆盖 change 里顺手修 codegen ⇒ ⇒⇒ 若 dual 暴露真实 parity
  分歧 ⇒ ⇒⇒ **停下来开独立 change**（T7 步骤 4）。

## Impact

- **改动文件**：`tests/test_blt_debug_behavior.cpp`（新）·
  `tests/CMakeLists.txt`（+1 个 `add_dual_backend_test`）·
  `AGENTS.md`（4 个锚点 + 3 处叙述值）·
  `.rddf/plans/dual-backend-full-coverage-2026-11.md`（新）·
  本 change 的工件
- **爆炸半径**：**极小** —— 1 个新测试文件 + 1 行 CMake
- **行为变更**：**零**（原文件 MUST NOT 修改 ⇒ ⇒ 既有语义逐位不变）
- **风险**：**低** —— 唯一风险是「verilator 半边被 skip ⇒ ⇒ 假绿」
  ⇒ ⇒ ⇒ **已用「断言数 = 2 × pair 数」作判别**（实测 2/2 ✔）
- **计数锚点**：**+1** ctest 条目 · **+1** dual 源文件 · **+1** pair

## 验收实测（2026-11-01）

```
冷缓存首跑（含 verilator 编译）：real 0m11.963s
  → Verilator Walltime 0.017s（2 modules）⇒ DUT 极小
  → 「All tests passed (2 assertions in 2 test cases)」
  ⇒ ⇒ **2 = 2 × pair 数** ⇒ ⇒ verilator 半边【真跑】✔（不是 skip）
```

⚠️ **热缓存**：0.6 – 3.0 s（小 DUT）· 7.7 s（RV32I 流水线）
⚠️ **冷缓存 mem 类 DUT**（`ch_mem` + FIFO，假 HOME 测得）：**72.0 s**
⇒ ⇒ ⇒ **⇒ 冷编译是主成本 ⇒ ⇒ 批次必须按【DUT 聚合】组织**
