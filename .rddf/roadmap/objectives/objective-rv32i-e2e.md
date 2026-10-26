---
id: objective-rv32i-e2e
status: active
created: 2026-10-19
last_revised: 2026-10-26
review_by: 2027-01-17
owner: rdd-planner
priority: P0
manual_deps: []
supersedes: null
theme: RV32I 完整 e2e 在 interpreter + verilator 双路 trace parity 通过（+ JIT 回落行为显式断言）
---

# Objective: objective-rv32i-e2e

## 1. 驱动诊断（Why now）

RV32I 5-stage 流水线（`include/cpu/pipeline/rv32i_pipeline.h`）是 CppHDL 的旗舰设计，但当前只走到"编译期修复完成 + 3 条无 hazard 指令的 verilator trace parity"。真正的端到端集成（`examples/riscv-mini/` SOC，含 i_cache / d_cache / AXI 互连）尚未在任何后端跑通：`i_cache_prefetch.h` wrapper instantiate 曾触发 SIGSEGV（**2026-10-20 已修复**，change `fix-riscv-mini-cache-prefetch-sigsegv`），且 cache 子系统的 verilator codegen 路径从未 e2e 验证。

> ⚠️ **2026-10-23 定义修订（用户裁决）**：原目标「interpreter + verilator + JIT **三路** trace parity」
> **在本仓库当前架构下不可能原生成立**。实测证据：
> 1. `src/jit/jit_ir_gen_lnode_state.cpp:58-60` 对 `type_mem_read_port` 返回 `UNSUPPORTED_OP`；
> 2. RV32I 必有 imem（`ch_mem` 读端口）⇒ 整个设计 JIT 编译失败 ⇒ 必然回落 interpreter
>    （`src/jit/jit_compiler.cpp:256-264` 的注释即为此设计意图）；
> 3. 另有 `JIT_MIN_NODES = 5`（`include/jit/jit_compiler.h:46`）小图回落。
>
> 故本 objective 的目标改为「**双路 parity（interpreter + verilator）+ JIT 回落行为显式断言**」。
> JIT-native 三路（即实现 `ch_mem` 读端口的 JIT lowering）作为**独立 objective**
> `jit-mem-read-port-support` 排期，不属本 objective。

> ⚠️ **2026-10-23 前置依赖**：本 objective 的 e2e 目标**依赖 `objective-riscv-mini-convergence`**。
> 审计确认 `include/cpu/pipeline/` 与 `examples/riscv-mini/src/` 是**两套不同架构**，各自被不同测试覆盖
> ⇒ 不存在「单一 SoC」可供 parity。必须先收敛（C1 + C2）才有 parity 对象。

> ⚠️ **2026-10-26 状态更新（诊断已被后续三个 change 消解）**：
> 1. **2026-10-24** C1 `riscv-mini-test-triage-2026-10` 归档（19 个「编译但从不执行」的
>    target 清零 ⇒ CTest 条目与实际执行对齐）。
> 2. **2026-10-25** C2 `rv32i-canonical-convergence-2026-10` 归档 ⇒ **两套架构已收敛**
>    （`examples/riscv-mini/src/` 32 头 → **1** 头；`include/cpu/**` 成为唯一权威；
>    ADR-046 D10 端口契约）。
> 3. **2026-10-26** 前置 change `rv32i-pipeline-commit-path-2026-10` 归档 ⇒
>    **SoC 从「跑不动程序」变成能正确执行 48 字固件**（观测面 **32 端口**；
>    **17 个流水线缺陷**修复，含 load-use 多拍停顿 A1-a 与 flush 多杀一拍）。
> 4. **2026-10-26** C3 `rv32i-soc-dual-parity-2026-10` 归档 ⇒ 本 objective 的
>    **SoC 级双路 parity**（200 拍 × **27** 个输出观测值**逐拍全等**，无掩码）
>    与 **JIT 回落契约**（回落原因可归因：`unsupported node width: mem
>    (…, 131072 bits > 64)`）落地。
> ⇒ 本 objective 的**主线已闭合**；仅剩「`dual-backend-rv32i-integration` change
> archived」一条未达成（Batch 2/3 未做）⇒ objective 保持 `active`。

适用场景：RV32I 顶层集成 + 双路 trace parity 验证；不适用场景：单模块单元测试（已有 dual-backend Phase 1 覆盖）、JIT mem-read lowering（见独立 objective）。

## 2. 目标愿景 + 完成判据

- riscv-mini SOC 能在 interpreter / verilator 两后端跑同一指令序列产出相同 commit 序列
- RV32I 短指令 + load/store + branch 三类路径双路 parity 通过
- JIT 回落行为**显式断言**：含 `ch_mem` 读端口的设计 → JIT 返回 `UNSUPPORTED_OP` → 回落 interpreter 且结果一致
- `i_cache_prefetch.h` / `d_cache_write_buffer.h` wrapper 无 SIGSEGV
- 完成判据（done-when）：
  - [x] `examples/riscv-mini/src/i_cache_prefetch.h` 实例化 `ICacheSync` 通过（无 SIGSEGV）— 2026-10-20 已修复
  - [x] `objective-riscv-mini-convergence` 完成（单一 RV32I 实现，本 objective 的前置）— 2026-10-25
  - [x] riscv-mini SOC **双路** trace parity 通过（≥ 20 指令序列，含 load/store/branch）— 2026-10-26 C3
  - [x] JIT 回落行为显式断言通过 — 2026-10-26 C3 的 T6
  - [ ] `dual-backend-rv32i-integration` change archived

## 3. 架构依据

- **ADR-035** — Verilator 后端整体设计（64-bit port cap / clock toggle / reset pulse）
- **ADR-039** — cache complete rewrite via ch_mem（RV32I L1 cache 依赖）
- **ADR-041** — cache Sync/Async variant（RV32I L1 用 Sync）
- **ADR-046** `rv32i-canonical-home` — RV32I canonical home（本 objective 的前置 objective 的架构裁决；✅ 2026-10-23 起草落盘）
- `openspec/specs/verilator-rv32i-e2e/spec.md` — RV32I + Verilator e2e 契约
  （⚠️ **2026-10-26 更新**：`## Purpose` 已手写；C3 归档时 **+5 个 Requirement**
  —— SoC 观测输出 / 固件写端口与死端口 / 双路逐周期 parity / JIT 跳过原因可归因 /
  JIT 回落行为显式断言）
- `openspec/specs/rv32i-commit-path/spec.md` — **RV32I 流水线 commit/valid 通路契约**
  （2026-10-26 归档的新 capability，6 个 Requirement：退休语义 / 有效链 /
  级间信号不串级 / 观测面契约 / 全观测值 parity / **load-use 保持型停顿** +
  **停顿·冲刷期间有效链自洽**）
- **ADR-046 `rv32i-canonical-home` D10-rev** — SoC 端口契约修订（**32 端口**
  = 5 输入 + 27 输出 = 11 功能 `obs_*` + 16 诊断 `dbg_*`）
- `openspec/specs/dual-backend-test-coverage/spec.md` — dual-backend 契约（Phase 4 续接）
- root `AGENTS.md §KNOWN LIMITATIONS` — JIT 对 `ch_mem` 读端口回落记录（本 objective 把它变成显式断言）

## 5. 反例

- 不要在 objective §6 写执行指令（违反 planner/builder 边界）
- 不要把 RV32I e2e 与 cache-stub 退役捆绑成单 change（修复与退役是两个独立关注点，违反 scope discipline）
- 不要假设 wrapper SIGSEGV 是 dual-backend 的 bug——它是 wrapper 内部 `ch_reg + C-style 数组` 架构 bug（**已于 2026-10-20 修复**）
- **不要**在收敛（`objective-riscv-mini-convergence`）之前启动 e2e parity —— 没有单一实现，parity 对象不确定
- **不要**为了让「三路」字面成立而去实现 JIT 的 mem-read lowering —— 那是独立 objective（`jit-mem-read-port-support`）的 scope
- **不要**把「JIT 回落」当成失败信号 —— 它是框架的**有意设计**（`jit_compiler.cpp:256-264`），本 objective 把它变成显式断言

## 9. 目标依赖与 Decision Gate

### 9.1 前置 objective 依赖
- **`objective-riscv-mini-convergence`**（**强前置**，2026-10-23 新增）— 两套 RV32I 实现收敛成单一
  ⇒ 才有确定的 parity 对象
- `objective-test-coverage-100pct` — RV32I e2e 是 dual-backend 全量覆盖的子集，需先有 macro 基础设施（**已具备**）
- `objective-cache-stub-retired` — cache stub 退役后 RV32I 才能用真实 cache（**已 completed 2026-10-20**）

### 9.2 Go / No-Go Decision Gate
- **Go 判据**（满足全部 5 条 → 转 completed）：
  - [x] `i_cache_prefetch.h` wrapper SIGSEGV 修复（真实实例化 ICacheSync 通过）— 2026-10-20
  - [x] `objective-riscv-mini-convergence` 完成（单一 RV32I 实现）— 2026-10-25
  - [x] riscv-mini SOC **双路**（interpreter + verilator）trace parity 通过 — 2026-10-26
  - [x] JIT 回落行为显式断言通过（检测 + 回落结果一致）— 2026-10-26
  - [ ] `dual-backend-rv32i-integration` change archived（Batch 2/3）
        —— 与 §2 的第 5 条 done-when 同一条（2026-10-26 补齐：此前 §9.2 只列 4 条，
        导致「4 条全 `[x]`」与 `status: active` 互相矛盾）
- **No-Go 判据**（任一成立 → defer）：
  - verilator codegen 对 RV32I 完整设计暴露无法在合理工期内修复的 bug
  - 收敛（`objective-riscv-mini-convergence`）判定 No-Go

### 9.3 跨 objective 影响
- 受此 objective 影响的下游：无（旗舰目标）
- 受上游 objective 影响：`objective-riscv-mini-convergence`（强前置）+ `objective-test-coverage-100pct`（已具备）+ `objective-cache-stub-retired`（已 completed）
- **新拆出的独立 objective**：`jit-mem-read-port-support`（JIT-native 三路，非本 objective 范围）

## 10. next_sprint_candidates
- sprint-2026-10: ~~ADR-046（归 `objective-riscv-mini-convergence`）~~ ✅ 2026-10-23 完成
- sprint-2026-10: ~~`rv32i-canonical-convergence-2026-10`（C2）~~ ✅ 2026-10-25 归档
- sprint-2026-11: ~~`rv32i-pipeline-commit-path-2026-10`（C3 的前置：流水线 commit/valid 修复 + 观测面）~~ ✅ 2026-10-26 归档
- sprint-2026-11: ~~`rv32i-soc-dual-parity-2026-10`（C3）~~ ✅ 2026-10-26 归档
- sprint-2026-11+: `dual-backend-rv32i-integration` Batch 2/3（`IfIdPipelineReg` clk-spike + `BranchPredictor`）
  ⇒ **本 objective Go 判据的最后一条**（`dual-backend-rv32i-integration` archived）
- sprint-2026-11+: `axi4-interconnect-routing-2026-10`（AXI 4×4 全路由；与本 objective 无耦合）
- sprint-2026-12+: `jit-mem-read-port-support`（独立 objective：JIT-native 三路）

## 11. 跟踪台账（append-only）
| Sprint | kind | 内容 | Decision/调整 | 原因 |
|--------|------|------|---------------|------|
| sprint-2026-10 | sprint-review | objective 创建（roadmap 首次初始化） | 激活为 Phase 5 旗舰目标 | Phase 4 dual-backend 基础设施就绪后即可启动 |
| sprint-2026-10 | revise | 「三路 → 双路 + JIT 回落断言」重定义 | 采纳用户裁决（2026-10-23） | `type_mem_read_port` → `UNSUPPORTED_OP` ⇒ RV32I 的 JIT 路必然回落，三路不可能原生成立 |
| sprint-2026-10 | revise | 新增强前置 `objective-riscv-mini-convergence` | 采纳用户裁决（2026-10-23） | 审计发现两个 cluster 是**两套不同架构**，无单一实现则 parity 无对象 |
| sprint-2026-11 | sprint-review | C1 / C2 / 前置 change / C3 连续四个 change 归档 | 主线闭合（2026-10-26） | C3 起步时实测证明「SoC 跑不动程序」⇒ 挂起并立前置 change；前置落地后 SoC 能正确执行 48 字固件 ⇒ C3 的 SoC 级双路 parity 可写 |
| sprint-2026-11 | revise | JIT 归因的失败出口从 1 个改为 **2 个** | 采纳实测（2026-10-26） | `allocate_buffer` 的「节点宽度 > 64」在 `generate_ir` **之前** ⇒ 它才是首个命中点 ⇒ 原方案只改 `generate_ir` ⇒ 回落日志的原因**是空的** |
| sprint-2026-11 | revise | SoC 端口契约 15 → **32**（ADR-046 **D10-rev**） | 采纳（2026-10-26） | 观测需要「指令流 / 有效链 / 写回 / 冒险脉冲」的可观测点，而 verilator 只能读**顶层**端口 |
