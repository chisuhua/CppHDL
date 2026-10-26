---
id: phase-5
kind: phase
status: active
phase_refs: []
主题: [riscv-mini 单源化收敛与 RV32I 双路 parity, riscv-mini 技术债清零, dual-backend 全量覆盖残余, 平台矩阵扩展]
---

## phase-5 概览

Phase 5（**2026-10-23 激活，承接自已归档的 phase-4**）的核心是把 riscv-mini 从"两套并行 RV32I 实现"
收敛成"一套"，并在此之上跑到端到端 parity。2026-10-23 的实测审计改变了本 phase 的性质（见
`objective-riscv-mini-convergence` §1）：

> **`include/cpu/pipeline/` 与 `examples/riscv-mini/src/` 不是"同一实现的两份拷贝"，而是两套不同架构**：
> Cluster A（lib）= 集成式 `Rv32iPipeline`（内部 `ch_mem` 寄存器堆 + 内联 ALU + 5 stage 组件）；
> Cluster B（example）= 组件库式（`Rv32iAlu` / `Rv32iRegFile` / `riscv::ForwardingUnit` / `BranchDetector` /
> `BranchConditionUnit` / `BranchPredictor` / `ControlHazardUnit` / `HazardDetectionUnit`，被根 tests/
> 的 dual-backend behavior 测试当单元组件测）。**"收敛"因此不是机械删除，而是架构合并**。

| Theme | 覆盖 | 状态 |
|-------|------|------|
| riscv-mini 技术债清零 | 19 个「编译但从不执行」的 Catch2 target + 7 个 orphan 测试文件 | ✅ 完成（C1，2026-10-24 归档） |
| RV32I 单源化收敛 | 17 个死头删除 + 10 组同名头裁决 + 根 tests/ 反向 include 消除 | ✅ 完成（C2，2026-10-25 归档；ADR-046 已落盘） |
| SoC 双路 parity | `Rv32iPipelineTop` interpreter + verilator 逐周期 parity + JIT 回落断言 | ✅ 完成（C3，2026-10-26 归档；前置 `rv32i-pipeline-commit-path-2026-10` 同日归档） |
| dual-backend 全量覆盖 | `feat-dual-backend-coverage` Phase 4 RV32I / Phase 5 全量（跨阶段残余） | 🟡 进行中 |
| cache-stub 退役 | `include/cpu/cache/{i,d}_cache_stub.h` 物理删除 | ✅ 完成（2026-10-20，提前 6 个月，variant b） |
| 平台矩阵 | Windows-2022 CI 启用 | ⏳ 计划中 |

## 计划能力（change 序列 C1 → C2 → C3）

1. **C1 `riscv-mini-test-triage-2026-10`** — 19 个「编译但从不执行」target 清零：
   15 绿 → 注册进 CTest；7 红（`test_pipeline_minimal` / `test_pipeline_debug` / `test_pipeline_debug4` /
   `test_pipeline_debug5` / `test_rv32i_pipeline_device` / `test_address_decoder` SIGSEGV +
   `test_riscv_tests` 44/49 断言失败）→ 修或删（带理由登记）。7 个 orphan 测试源文件定性。
   **目的：收敛会大改 include 路径，必须先有绿色基线做回归护栏。**
2. **C2 `rv32i-canonical-convergence-2026-10`** — 单源化收敛（`objective-riscv-mini-convergence`，
   前置：**ADR-046 架构裁决**，rdd-arch 职责）：删 17 个死头（3,663 LOC）；裁决 4 组 dual-live 同名对
   （`hazard_unit` 前推语义分叉最需裁决）；提升 Cluster B 的 live 组件到 `include/cpu/`；移除根 tests/
   6 处 `-Iexamples/riscv-mini/src` 反向依赖；加 grep-lint 护栏（同名头唯一 + 无反向 include）。
3. **C3 `rv32i-soc-dual-parity-2026-10`** — SoC 级 parity：interpreter + verilator 逐周期 trace parity
   （≥20 指令序列含 load/store/branch）+ **JIT 回落行为显式断言**（`type_mem_read_port` →
   `UNSUPPORTED_OP` → 回落 interpreter 且结果一致）。依赖 C1 + C2。
4. **（独立 objective，非本 sprint）** `jit-mem-read-port-support` — JIT 对 `ch_mem` 读端口的 lowering，
   真三路 parity 的前提。仅当未来需要 JIT-native RV32I 时立项。

> ⚠️ **「三路 parity」定义已裁决**（2026-10-23 用户决定）：实测 `src/jit/jit_ir_gen_lnode_state.cpp:58`
> 对 `type_mem_read_port` 返回 `UNSUPPORTED_OP` ⇒ 含 imem 的 RV32I 设计 JIT 必然整体回落 interpreter。
> 故 `objective-rv32i-e2e` 从「interpreter + verilator + JIT 三路」**重定义为
> 「双路 parity（interpreter + verilator）+ JIT 回落行为显式断言」**；JIT-native 三路作为独立 objective 排期。

## 依赖关系

```
C1 test-triage（绿色基线，无依赖）
    ↓
C2 canonical-convergence（前置 ADR-046 架构裁决；依赖 C1 基线）
    ↓
C3 soc-dual-parity（双路 parity + JIT 回落断言；依赖 C2 单源化）
```

并行：`feat-dual-backend-coverage` Phase 4 RV32I 残余（IfIdPipelineReg clk-spike + BranchPredictor
follow-up）可与 C1 并行（互不触碰）。

## 架构文档锚点

| 文档 | 与本 phase 关联 |
|------|----------------|
| `docs/adr/ADR-046-rv32i-canonical-home.md`（✅ 2026-10-23 落盘，C2 实施依据 D1–D12） | 收敛方向裁决：`include/cpu/` 为唯一权威 + example 只留 SoC 顶层 |
| `docs/adr/ADR-038-forwarding-namespace-orphan.md` | forwarding 重复的既有裁决（C2 收敛时需衔接） |
| `docs/adr/ADR-039-cache-complete-rewrite-via-ch-mem.md` | cache 重写路径 |
| `docs/adr/ADR-041-cache-sync-async-variant.md` | cache Sync/Async 拆分 |
| `.rddf/improvements/riscv-mini-test-triage-2026-10.md` | C1 improvement 草稿 |
| `.rddf/improvements/rv32i-canonical-convergence-2026-10.md` | C2 improvement 草稿 |
| `.rddf/improvements/rv32i-soc-dual-parity-2026-10.md` | C3 improvement 草稿 |
| `include/cpu/AGENTS.md` | STRUCTURE 表漂移（C2 需同步） |
| root `AGENTS.md §KNOWN LIMITATIONS` | i_cache_prefetch SIGSEGV（已修复）+ Windows CI 延后 |

## 完成条件

- [x] C1 `riscv-mini-test-triage-2026-10` archive（19 target 清零 + 护栏 + CTEST_TOTAL 同步）— 2026-10-24
- [x] ADR-046 `rv32i-canonical-home` 落盘（2026-10-23，rdd-arch 职责，C2 前置）
- [x] C2 `rv32i-canonical-convergence-2026-10` archive（17 死头删除 + 10 组同名裁决 + 反向 include 消除 + 护栏）— 2026-10-25
- [x] **前置** `rv32i-pipeline-commit-path-2026-10` archive（SoC 观测面 **32 端口** + **17 个流水线缺陷**）— 2026-10-26
- [x] C3 `rv32i-soc-dual-parity-2026-10` archive（双路 parity + JIT 回落断言）— 2026-10-26
- [x] cache-stub 物理删除（ADR-037）— 2026-10-20 落地（variant b，change `cache-stub-physical-removal-2026-10` 已 archive）
- [ ] dual-backend 全量覆盖（`feat-dual-backend-coverage` Phase 4 RV32I / Phase 5 全量）
- [ ] 逻辑算子 `[[deprecated]]` 移除（ADR-036 §8，deadline 2027-04-17）
- [ ] Windows-2022 CI 启用
