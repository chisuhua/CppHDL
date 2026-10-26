---
id: feat-riscv-mini-convergence
kind: feature
status: completed
phase_refs: [phase-5]
主题: 消除 riscv-mini 技术债 + 把 include/cpu 与 examples/riscv-mini 收敛成单一 RV32I 实现
---

## 概述

`include/cpu/pipeline/` 与 `examples/riscv-mini/src/` 目前是 **两套不同架构的 RV32I 实现**，且互相遮蔽：

- **Cluster A（lib，`include/cpu/pipeline/`）** — 集成式 `Rv32iPipeline`：内部 `ch_mem<ch_uint<32>,32> reg_file`、
  ALU 内联在 `ExStage`、5 个 stage 组件、`hazard_unit`（**前推已禁用**，注释「会导致 IR 图循环依赖」）。
  被 `rv32i_pipeline_top.h` / `tests/test_verilator_complex_e2e.cpp` / 7 个 example 测试消费。
- **Cluster B（example，`examples/riscv-mini/src/`）** — 组件库式：`Rv32iAlu` / `Rv32iRegFile` /
  `riscv::ForwardingUnit` / `BranchDetector` / `BranchConditionUnit` / `BranchPredictor` /
  `ControlHazardUnit` / `HazardDetectionUnit`（在 `rv32i_core.h`）/ `rv32i_pc`。
  被**根 tests/** 的 `test_rv32i_pipeline.cpp` + `test_rv32i_pipeline_behavior.cpp`（dual-backend）消费。
- Cluster B 的 `rv32i_pipeline.h`(207) + `stages/*.h`(988) 是**死代码**（全部消费者解析到 lib 版）。

同名头靠 **`-I` 顺序**消歧（`examples/riscv-mini/CMakeLists.txt:19-23` 注释维护，无自动化护栏）⇒
**同一份源码在不同 TU 编译出不同实现**。

| 子项 | 状态 |
|---|---|
| C1: 19 个「编译但从不执行」target 清零 + 7 orphan 测试定性 | ✅ 已归档（`2026-10-24-riscv-mini-test-triage-2026-10`） |
| C2: 单源化收敛（17 死头删除 + 10 组同名裁决 + 根 tests/ 反向 include 消除 + grep-lint 护栏） | ✅ 已归档（`2026-10-25-rv32i-canonical-convergence-2026-10`；ADR-046 ✅ 已落盘） |
| `include/cpu/AGENTS.md` STRUCTURE 漂移修正（6 个不存在文件的条目） | ✅ 已实施（随 C2） |
| `tests/CMakeLists.txt` 6 处 `-Iexamples/riscv-mini/src` 反向依赖消除 | ✅ 已实施（随 C2） |

## 跨阶段拆分

### phase-5
- ✅ **C1 `riscv-mini-test-triage-2026-10`** — 2026-10-24 归档；15 绿注册 / 7 红修或删 / 7 orphan 定性；新护栏 `scripts/verify_example_test_wiring.sh`
- ✅ **ADR-046** `rv32i-canonical-home`（rdd-arch 职责，2026-10-23 落盘）— 裁决方向：`include/cpu/` 唯一权威 + example 只留 SoC 顶层；C2 实施依据其 D1–D12
- ✅ **C2 `rv32i-canonical-convergence-2026-10`** — 2026-10-25 归档（逐文件归属表见 improvement 草稿）
- ✅ `include/cpu/AGENTS.md` STRUCTURE / NAMESPACE MAP 同步（随 C2）

## 验收标准
- [x] 收敛后 `include/cpu/**` 与 `examples/riscv-mini/src/**` **无同 basename 头文件**（grep-lint 护栏）— 2026-10-25 C2 归档
- [x] `examples/riscv-mini/src/` 只剩 SoC 顶层 + example 专属件（逐文件归属表落地）— 2026-10-25 C2（32 头 → **1** 头）
- [x] 17 个从未编译的头（3,663 LOC）已 `git rm`（或逐文件给出保留理由 + 加消费者）— 2026-10-25 C2
- [x] 4 组 dual-live 同名对（`hazard_unit` / `rv32i_decoder` / `rv32i_pipeline_regs` / `rv32i_tcm`）裁决并单源化 — 2026-10-25 C2
- [x] 根 `tests/CMakeLists.txt` 无 `target_include_directories(... examples/riscv-mini/src)` — 2026-10-25 C2（6 处反向 include 消除）
- [x] `include/cpu/AGENTS.md` STRUCTURE 表与磁盘一致（无幽灵文件）— 护栏 C5 绿
- [x] C1 清零后 `ctest -N` 与 `CTEST_TOTAL` 同步，`test_verify_test_counts.sh` 绿 — 2026-10-25（`CTEST_TOTAL` = **200**）
