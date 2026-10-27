# CppHDL Roadmap

## 元信息
- **版本**: 1
- **创建时间**: 2026-10-19
- **最后更新**: 2026-10-27
- **当前阶段**: phase-5
- **当前 Sprint**: sprint-2026-10

## Phase Skeleton
| Phase | Theme | Status | Started | Done |
|-------|-------|--------|---------|------|
| phase-1 | 核心抽象与算子基础设施 | archived | 2026-05 | 2026-05 |
| phase-2 | 高级特性与 JIT 编译器 | archived | 2026-05 | 2026-06 |
| phase-3 | Verilator 后端集成与 Verilog codegen | archived | 2026-06 | 2026-09 |
| phase-4 | dual-backend 测试完整化 + cache 子系统重建 | archived | 2026-10 | 2026-10 |
| phase-5 | riscv-mini 收敛 + SoC 双路 parity + RV32I ISA 语义边界 + 平台矩阵 | active | 2026-10 | |
<!-- AUTO-INDEX -->

## Fragment Index (auto-generated)

### Phases
- `phase-1` — 核心抽象与算子基础设施
- `phase-2` — 高级特性与 JIT 编译器
- `phase-3` — Verilator 后端集成
- `phase-4` — dual-backend 测试完整化
- `phase-5` — riscv-mini 单源化收敛 · RV32I 双路 parity · ISA 语义边界 · RTL 缺陷收口

### Features
- `feat-cache-rewrite` — cache 子系统从 C-style 数组重写为 ch_mem（ADR-039）+ Sync/Async 双变体（ADR-041）+ stub 物理退役（ADR-037） (refs: phase-4, phase-5)
- `feat-dual-backend-coverage` — 单元测试统一跑 interpreter + verilator 双后端（5 阶段推进） (refs: phase-4, phase-5)
- `feat-hdl-logical-deprecation` — ADR-036 逻辑算子 deprecation 标记 → 全量迁移 → 移除（removal gate） (refs: phase-4, phase-5)
- `feat-platform-matrix` — CI 平台矩阵扩展（macOS-14 已启用 → Windows-2022） (refs: phase-3, phase-5)
- `feat-riscv-mini-convergence` — 消除 riscv-mini 技术债 + 把 include/cpu 与 examples/riscv-mini 收敛成单一 RV32I 实现 (refs: phase-5)
- `feat-riscv-mini-rtl-debt` — riscv-mini 实测挖出的两个 RTL 缺陷收口（cache wrapper 分支从不发射 · CLINT mtime 不自增） (refs: phase-5)
- `feat-rv32i-integration` — RV32I 5-stage 流水线 + riscv-mini SOC 端到端集成（双路 parity + JIT 回落断言） (refs: phase-5)
- `feat-rv32i-isa-completeness` — RV32I 语义边界收口（SB/SH 字节使能 · JAL/JALR 重定向 · SRA）+ 流水线健壮性（skid buffer · 取指双装） (refs: phase-5)

<!-- AUTO-SPRINT-START -->
## Current Sprint: sprint-2026-10

| Project | Phase | Priority | Feedback | Proposal |
|---------|-------|----------|----------|----------|
| RV32I ISA ④ 取指 skid buffer | phase-5 | P0 | none | ⏳ **待 architect**：裁决已定（开 **ADR-048**）⇒ 架构依据先于 change ⇒ 顺序 = architect 写 ADR-048 → planner 起草 change → builder 实施。5 段草案：`.rddf/improvements/rv32i-fetch-skid-buffer-2026-10.md` |
| dual-backend RV32I Batch 2/3 | phase-5 | P1 | none | ✅ **proposal 已就绪**：`openspec/changes/dual-backend-rv32i-integration-2026-10/`（**无 spec 变更**，`.openspec.yaml` 已声明 `skip_specs`）⇒ 可直接进 builder |

> ✅ **sprint-2026-10 又归档一项**（2026-10-27）：`rv32i-forwarding-decision-2026-10`（P0）——
> ADR-047 裁决「接」数据前推：mux = 既有 `chlib::ForwardingMux`（**零新增组件**）+ 档位 select
> 打一拍 + 档位 1 源选择收进组件（`is_jump`/`jump_result`）+ `HazardUnit` 删 5 个预留端口 +
> **D11 寄存器堆读点 WB→ID 旁路**（修掉「距离 3 读旧值」的既有缺陷 —— 两后端**同构地错** ⇒
> 双后端 parity 抓不到）。第 1 轮对抗审稿 **APPROVE_WITH_CONDITIONS / 无 BLOCKER** ⇒ 按预算
> 不发起第 2 轮 ⇒ `openspec/changes/archive/2026-10-27-rv32i-forwarding-decision-2026-10/`，
> **新 capability `rv32i-data-forwarding`**（5 条 Requirement，距离矩阵的唯一权威）+ 4 个 MODIFIED。
> 变异 **6/6** 全被抓到；`CTEST_TOTAL` **202 不变**。

> ✅ **sprint-2026-10 又归档一项**（2026-10-27）：`rv32i-jump-sra-2026-10`（P0）——
> JAL/JALR 重定向 + 返回地址写回 + SRA 真算术右移，**连带修复 3 个既有缺陷**
> （U 型立即数 / `alu_op` 对 I 型的 SUB 误判 / 错误路径的 JAL·JALR 劫持 PC）。
> 2 轮对抗审稿（第 1 轮 **REJECT** 1 BLOCKER ⇒ 整改 ⇒ 第 2 轮 APPROVE_WITH_CONDITIONS）
> ⇒ `openspec/changes/archive/2026-10-27-rv32i-jump-sra-2026-10/`，
> capability `rv32i-isa-semantics` **2 → 6** 条 Requirement。

> ✅ **sprint-2026-10 本轮归档**（2026-10-26）：`example-assertion-quality-2026-10`（P1，
> 新 capability `example-assertion-quality`）与 `rv32i-store-byte-enable-2026-10`（P0，
> 新 capability `rv32i-isa-semantics`）—— 两者各经 **2 轮对抗审稿**（第 1 轮
> APPROVE_WITH_CONDITIONS ⇒ 条件项同批整改；第 2 轮 **APPROVE**）⇒ 均已归档。
> ⚠️ 两者共同的**合并门** = CI 的 `macos-14` job 绿（本机 Linux 不可完成）⇒
> 该 job 红 ⇒ MUST revert 归档 commit。

### Unmapped (3)

> 三个 improvement 草案尚无 change 目录（已归属 feature / objective，待立项）：
> `riscv-mini-cache-wrapper-dead-branch-2026-10` → `feat-riscv-mini-rtl-debt` ①
> （⚠️ 名字与候选 change 名 `riscv-mini-cache-wrapper-live-logic-2026-10` **不一致**
> ⇒ 立项时 MUST 统一，见「实施规划」§遗留项 3）；
> `clint-mtime-autoincrement-2026-10` → `feat-riscv-mini-rtl-debt` ②；
> `rv32i-fetch-skid-buffer-2026-10`（P0，2026-10-27 起草）→ `feat-rv32i-isa-completeness` ④
> （⏳ 待 **ADR-048**；4 个决策门已由用户裁决：缺陷 B 按判据执行 / 框架层**加 API** /
> **开 ADR** / NOP 护栏实施时先定位）。
>
> ✅ **已立项**：`dual-backend-rv32i-integration-2026-10`（P1，2026-10-27）
> → `openspec/changes/dual-backend-rv32i-integration-2026-10/`（proposal 已就绪，
> **无 spec 变更**）⇒ 覆盖 `feat-dual-backend-coverage` Phase 4 残余 +
> `objective-rv32i-e2e` 判据第 5 条。

> ✅ **sprint-2026-10 已完成**：`axi4-interconnect-routing-2026-10`（P0）
> 于 2026-10-26 归档（`openspec/changes/archive/2026-10-26-axi4-interconnect-routing-2026-10/`），
> 新 capability `axi-interconnect-routing`（6 条 Requirement）。
<!-- AUTO-SPRINT-END -->

### Objectives
- `objective-cache-stub-retired` — cache stub 物理删除（ADR-037 deadline 2027-04-17）— **completed**
- `objective-riscv-mini-convergence` — 消除 riscv-mini 技术债 + 把 include/cpu 与 examples/riscv-mini 收敛成单一 RV32I 实现 — **completed**
- `objective-rv32i-e2e` — RV32I 完整 e2e 在 interpreter + verilator 双路 trace parity 通过（+ JIT 回落行为显式断言）— **active**（Go 判据 6 条：**5 ✅ + 1 ⏳** —— 仅剩 `dual-backend-rv32i-integration` Batch 2/3 archived；「RV32I ISA 语义边界」判据已于 2026-10-27 达成 —— 见该 objective §9.2 的范围说明）
- `objective-test-coverage-100pct` — 单元测试 100% 双后端（interpreter + verilator）覆盖 — **active**（`ctest -L verilator_e2e` = 37，目标 ≥ 100）
- `jit-mem-read-port-support` — JIT 对 `ch_mem` 读端口的 lowering（真三路 parity 前提）— **backlog，未建档**（当前 JIT 回落是有意设计且已断言；建档前置 = 出现必须 JIT-native 跑 RV32I 的需求）
- `roadmap-fragments-consistency-guard` — roadmap fragments 一致性自检脚本 + CI 挂载 — **backlog 候选**（2026-10-26 planner 登记；起因 = `rddf roadmap` 子命令全坏 ⇒ AUTO 块 / Fragment Index / R1–R8 校验只能手工 ⇒ 多类漂移的根因；独立小 change，不占 phase-5 完成条件）

> Sprint 编号口径：以本文件为准（`sprint-2026-10`）。三个 objective 的 append-only 台账曾把
> C1/C2/C3 归档记为 `sprint-2026-11`，属编号漂移 ⇒ 已在 `objective-rv32i-e2e` §11 追加澄清行
> （台账不可改写，只追加）。

