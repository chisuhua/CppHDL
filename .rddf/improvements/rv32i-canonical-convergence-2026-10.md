---
name: rv32i-canonical-convergence-2026-10
priority: P0
source: objective-riscv-mini-convergence §10（2026-10-23 roadmap 重梳理）
phase: phase-5
category: refactor
type: feature
created_at: 2026-10-23
---

# Improvement: RV32I 单源化收敛 —— include/cpu 为唯一权威，example 只留 SoC 顶层

- **Date drafted**: 2026-10-23
- **Drafted by**: build-agent (planner role)
- **Status**: 📋 Handoff document — 5-segment draft，等待 rdd-builder 启动
- **Objective**: `objective-riscv-mini-convergence`（C2）
- **强前置**：ADR-046 `rv32i-canonical-home`（rdd-arch 职责）+ C1 `riscv-mini-test-triage-2026-10` archive

## 1. Why (背景与动机)

`include/cpu/pipeline/` 与 `examples/riscv-mini/src/` 是**两套不同架构**的 RV32I 实现，同名头靠 `-I` 顺序
消歧（`examples/riscv-mini/CMakeLists.txt:19-23` 注释维护，**无护栏**）。实测（2026-10-23，见
`objective-riscv-mini-convergence` §1 D1-D5）：

- **Cluster A（lib）**：集成式 `Rv32iPipeline` —— 内部 `ch_mem<ch_uint<32>,32> reg_file` + 内联 ALU（`ExStage`）
  + 5 stage 组件。被 `rv32i_pipeline_top.h` / `tests/test_verilator_complex_e2e.cpp` / 7 个 example 测试消费。
- **Cluster B（example）**：组件库式 —— `Rv32iAlu` / `Rv32iRegFile` / `riscv::ForwardingUnit` /
  `BranchDetector` / `BranchConditionUnit` / `BranchPredictor` / `ControlHazardUnit` / `HazardDetectionUnit`（在
  `rv32i_core.h`）。被根 `tests/test_rv32i_pipeline.cpp`（12 TEST_CASE）+ `tests/test_rv32i_pipeline_behavior.cpp`
  （13 dual pairs）+ `rv32i_phase{1,2,3}_test.cpp` 消费。
- **17 个头从未被任何 TU 编译**（3,663 LOC），其中 8 个是 `ba04fdb`(2026-06-15) 从 `include/cpu/` 移出的
  ⇒ 「提升」对它们是反向操作，须逐个裁决。
- 根 `tests/CMakeLists.txt:54,56,66,381,554,562` 共 6 处 `target_include_directories(... examples/riscv-mini/src)`
  + `tests/test_rv32i_pipeline*.cpp` 直接 `#include "../examples/riscv-mini/src/..."` ⇒ **库测试依赖 example 源码**。

**Why now**：e2e parity（`objective-rv32i-e2e`）需要「单一 SoC」对象，而当前没有；继续双轨意味着每改一处
要同步两处、且同名头换序即换实现。

## 2. What Changes (变更范围)

### C2-a 死头删除（17 个，3,663 LOC）
`branch_predictor_bundle.h(133) / d_tcm.h(329) / firmware_loader.h(279) / hazard_unit_bundle.h(194) /
i_tcm.h(255) / rv32i_axi_interface.h(171) / rv32i_control.h(142) / rv32i_core_simple.h(324) /
rv32i_isa.h(243) / rv32i_pipeline.h(207) / rv32i_regs.h(160) / rv32i_system.h(238) /
stages/{ex,id,if,mem,wb}_stage.h(988)`
- `git rm` 上述文件；C1 已确认 0 消费者（`.o.d` 实测）。
- `stages/` 目录随最后 5 个 stage 头删除后 rmdir。

### C2-b 逐文件归属表（**本 change 的核心交付**）
对 Cluster B 的 live 文件，逐个裁决「提升 / 保留为 example 专属」：

| 文件 | LOC | 消费者 | 建议归属 |
|---|---|---|---|
| `rv32i_alu.h`（`Rv32iAlu`） | 172 | phase1/2 测试 + `rv32i_core.h` | 提升 → `include/cpu/pipeline/` |
| `rv32i_regfile.h`（`Rv32iRegFile`） | 316 | phase1 测试 + `rv32i_core.h` | 提升 → `include/cpu/pipeline/` |
| `rv32i_pc.h` | 47 | `rv32i_core.h` | 提升 → `include/cpu/pipeline/` |
| `rv32i_forwarding.h`（`riscv::ForwardingUnit`） | 174 | `rv32i_core.h` + 根 behavior 测试 | 提升 → `include/cpu/pipeline/`（⚠️ 与 `chlib::ForwardingUnit` 重复，衔接 ADR-038） |
| `rv32i_branch_predict.h`（BranchDetector/BranchConditionUnit/BranchPredictor/ControlHazardUnit） | 475 | 根 tests/ | 提升 → `include/cpu/pipeline/` |
| `rv32i_core.h`（`HazardDetectionUnit`） | 65 | 根 behavior 测试 | 提升 → `include/cpu/pipeline/`（⚠️ 类与文件名不符，可拆 `hazard_detection_unit.h`） |
| `rv32i_pipeline_regs.h`（struct 版） | 490 | 根 tests/ + Cluster B 组件 | **裁决点**（见 C2-c #4） |
| `rv32i_decoder.h` | 161 | 根 tests/ + Cluster B 组件 | **裁决点**（见 C2-c #3） |
| `rv32i_tcm.h`（InstrTCM/DataTCM/BootROM） | 134 | `rv32i_pipeline_top.h` + example 测试 | **裁决点**（见 C2-c #5） |
| `hazard_unit.h` | 188 | 根 behavior 测试 | **裁决点**（见 C2-c #1，语义分叉最大） |
| `rv32i_pipeline_top.h`（SoC 顶层） | 49 | `test_pipeline_smoke` | **保留 example**（SoC 装配） |
| `branch_predictor_v2.h` | 151 | `tests/chlib/test_branch_predictor_v2.cpp` | 提升 → `include/cpu/pipeline/`（或 `include/cpu/riscv/`） |
| `rv32i_hazard_complete.h` | 195 | `tests/chlib/test_hazard_complete.cpp` | 提升 → `include/cpu/pipeline/` |
| `i_cache_prefetch.h` / `d_cache_write_buffer.h` | 145/146 | `test_cache_smoke` | 提升 → `include/cpu/cache/`（**AGENTS.md 已声称在此**，实际在 example） |

> ⚠️ 上表是 planner 的建议起点；builder 实施时逐文件确认，**任何「提升」都要更新 include 路径 + 同步
> `include/cpu/AGENTS.md` STRUCTURE/NAMESPACE MAP**。

### C2-c 4 组 dual-live 同名对裁决（lib 版为权威起点）
| # | basename | 冲突 | 裁决方向 |
|---|---|---|---|
| 1 | `hazard_unit.h` | example 启用前推 / lib 禁用前推（「会导致 IR 图循环依赖」） | lib 版为权威；**前推语义变更须单独裁决 + 回归证据**，不在本 change 顺手改 |
| 2 | `rv32i_decoder.h` | 4 行 diff（`using namespace` 位置） | lib 版；example 副本删，消费者重指 |
| 3 | `rv32i_pipeline_regs.h` | 4 行 diff | lib 版；example 副本删，消费者重指 |
| 4 | `rv32i_tcm.h` | 5 行 diff | lib 版；example 副本删 |
| 5 | `stages/*.h` + `rv32i_pipeline.h` | example 版整体死（988+207 LOC） | example 副本已在 C2-a 删 |

### C2-d 反向 include 消除
- 删根 `tests/CMakeLists.txt` 6 处 `target_include_directories(... examples/riscv-mini/src)`。
- 根 `tests/test_rv32i_pipeline.cpp:25-28` / `test_rv32i_pipeline_behavior.cpp:55-59` 的
  `#include "../examples/riscv-mini/src/..."` 改为 `#include "cpu/pipeline/..."`。
- `rv32i_phase{1,2,3}_test.cpp` 的 `#include "src/rv32i_*.h"` 改为 lib 路径。

### C2-e 新护栏
- 新增 `scripts/verify_no_duplicate_rv32i_headers.sh`：
  ① `include/cpu/**` 与 `examples/riscv-mini/src/**` **同 basename 头文件数 ≤ 1**；
  ② 根 `tests/CMakeLists.txt` 无 `examples/riscv-mini/src` 的 `-I`；
  ③ 任何 TU 不得 `#include` example 路径（`grep -rn '\.\./examples/riscv-mini/src' tests/` 为空）。
- 挂进 CI。

### C2-f 文档同步
- `include/cpu/AGENTS.md` STRUCTURE 表（当前列 6 个不存在文件：`riscv/rv32i_control.h` /
  `riscv/rv32i_system.h` / `riscv/firmware_loader.h` / `riscv/rv32i_soc.h` / `cache/i_cache_prefetch.h` /
  `cache/d_cache_write_buffer.h`）→ 与实际磁盘一致。
- root `AGENTS.md` STRUCTURE / NOTES 同步（`cpu/` 条目）。
- 新 ADR-046（rdd-arch）在 C2 前落盘；C2 archive 时在 ADR-046 §5 标 executed。

## 3. Acceptance (验收标准)

### 3.1 编译 + 测试 (PHASE GATES 🟢)
- [ ] `cmake --build build -j$(nproc)` 0 error 0 warning
- [ ] `ctest -LE perf` 全绿（2026-10-23 基线 174/174；C2 不得引入回归）
- [ ] `ctest -L verilator_e2e` 34 个保持绿（含 `test_verilator_complex_e2e` / `test_rv32i_pipeline_behavior`）

### 3.2 单源化护栏（本 change 的核心验收）
- [ ] `bash scripts/verify_no_duplicate_rv32i_headers.sh` 绿（含 ①同名头 ≤1 ②无反向 `-I` ③根 tests 无 example include）
- [ ] `git ls-files examples/riscv-mini/src | wc -l` = 收敛后剩余数（SoC 顶层 + 裁决保留件）
- [ ] `find examples/riscv-mini/src -name '*.h' | wc -l` 与上一条一致（无未跟踪新增）

### 3.3 文档同步
- [ ] `include/cpu/AGENTS.md` STRUCTURE 与 `ls include/cpu/**` 一致（无幽灵文件）
- [ ] root `AGENTS.md` STRUCTURE/NOTES 同步；ADR-046 §5 标 executed

## 4. Capabilities (涉及的能力维度)

| Capability | 影响 | 文档 |
|------------|------|------|
| （新）`rv32i-canonical-home` | 单源化 + 归属表 + 护栏 | 新 capability（ADR-046 关联）|
| `example-runtime-coverage` | example/src 收缩到 SoC 顶层 | MODIFIED |
| `verilator-e2e-coverage` | `test_verilator_complex_e2e` 头路径更新 | MODIFIED（若涉及）|

## 5. Impact (影响面分析)

### 5.1 风险

| 风险 | 等级 | 缓解 |
|------|------|------|
| `hazard_unit.h` 前推语义分歧改坏（lib 禁用前推可能是有意规避 IR 环） | 🔴 高 | **本 change 不动前推语义**；单独立项（见 phase-5 No-Go 判据） |
| 提升 Cluster B 组件时 include 路径改漏（同名头换序即换实现） | 🟡 中 | C2-e 护栏 + C1 绿色基线回归 |
| `chlib::ForwardingUnit` 与 `riscv::ForwardingUnit` 重复（ADR-038） | 🟡 中 | 提升时并存，namespace 区分；合并留给独立裁决 |
| 根 tests 的 6 处 `-I` 移除后某些 test 编译失败 | 🟢 低 | 逐个 target 验证（C1 已全注册） |
| 与 C1 并行冲突（同 CMakeLists） | 🟡 中 | **串行：C1 archive 后启 C2** |

### 5.2 明确不做（Non-Goals）
- 不改 hazard 前推语义（独立裁决）
- 不改 `Rv32iPipeline` 内部装配（除非提升 Cluster B 组件必须）
- 不做 e2e parity（C3）
- 不删 `rv32i_pipeline_top.h`（SoC 顶层是 example 的合法保留件）
