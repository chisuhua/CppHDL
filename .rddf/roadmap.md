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
- `feat-rv32i-isa-completeness` — RV32I 语义边界收口（SB/SH 写字节道 · LB/LH 读字节道 · JAL/JALR 重定向 · SRA）+ 流水线健壮性（skid buffer · 取指双装） (refs: phase-5)

<!-- AUTO-SPRINT-START -->
## Current Sprint: sprint-2026-10

| Project | Phase | Priority | Feedback | Proposal |
|---------|-------|----------|----------|----------|
| RV32I ISA ⑤ load 字节道固件验证 | phase-5 | P0 | none | ✅ **已归档**（2026-10-28）：`openspec/changes/archive/2026-10-28-rv32i-load-byte-lane-2026-10/`（⚠️ **原计划是「只验证」⇒ 实施期发现 `LB` 符号扩展是**错的** ⇒ 改为「**含 2 行实现修复**」；spec delta = `rv32i-isa-semantics` ADDED ×1 ⇒ 归档**禁** `--skip-specs` ⇒ 主 spec 合并 +1 条）⇒ **`feat-rv32i-isa-completeness` ⑤ 收口** |
| cache wrapper 活逻辑（T8 ①） | phase-5 | P2 | none | ✅ **已归档**（2026-10-28）：`openspec/changes/archive/2026-10-28-riscv-mini-cache-wrapper-live-logic-2026-10/`（⚠️ **既有断言确实转红** ⇒ 反转断言与修实现**同 commit**；spec delta = `cpu-cache-subsystem` MODIFIED ×2（**路径订正**）+ ADDED ×1 ⇒ 归档**禁** `--skip-specs`）⇒ **`feat-riscv-mini-rtl-debt` ① 收口** |
| T7 可 dual 上限盘点 | phase-5 | P1 | none | ✅ **已归档**（2026-10-28）：`openspec/changes/archive/2026-10-28-dual-backend-full-coverage-inventory-2026-10/`（**只读**；**172** 行盘点表）⇒ ⚠️ **决策门已触发**：**诚实**可达上限口径 A ∈ [**71**, **96**] / 口径 B ∈ [**62**, **87**] ⇒ **两种口径的乐观上限都 < 100** ⇒ ⇒ **判据在现有语料内不可达** ⇒ **MUST 由 planner** 修订 `objective-test-coverage-100pct` 的 Go 判据（**取舍集 4 个轴**见该 change 的 `design.md` §9.2.2；含 **3 处被证伪的前提**；⚠️ **口径 A 的技术上限其实无界** ⇒ 71/96 是「语义诚实」的上限）⇒ 已立项 **`.rddf/improvements/dual-backend-criterion-revision-2026-10.md`** |
| roadmap 簿记一致性护栏 | phase-5 | P0 | none | ✅ **已归档**（2026-10-28）：`openspec/changes/archive/2026-10-28-roadmap-fragments-consistency-guard-2026-10/`（新增 capability `roadmap-bookkeeping-consistency` ⇒ **未**用 `--skip-specs`）⇒ `scripts/verify_roadmap_fragments.sh` + CI 2 处挂载 + `--self-test` 13 fixture |
| perf baseline 重采集 | phase-5 | P0 | none | ✅ **已归档**（2026-10-28）：`openspec/changes/archive/2026-10-28-perf-baseline-refresh-2026-10/`（**3 次采样取逐行中位数** ⇒ `git_sha=0758fa0`；MODIFIED ×1 ⇒ **未**用 `--skip-specs`）⇒ `perf_regression` 恢复为「Compared 18 rows ⇒ No regressions」 |
| 输入端口驱动护栏（spike） | phase-5 | P1 | none | ✅ **已归档**（2026-10-28）：`openspec/changes/archive/2026-10-28-input-port-driving-guard-2026-10/`（**只出文档**；零代码改动）⇒ ⚠️ **决策门结论：通用状态型护栏关闭**（实测：① 运行期**无法识别端口身份** ⇒ 诊断不可行动；② **语义判定缺承重件**（「从未被驱动」≠「应该被驱动」⇒ 需「必需端口注册表」，规范层已明文要求）且 **per-TEST-CASE 是必需粒度**而平规则在该粒度必然误报 ⇒ **死结**；③ 驱动藏在 29 个 helper + 186 次宏里 ⇒ 需过程间分析）⇒ ⇒ **义务继续留在 `AGENTS.md §ANTI-PATTERNS`** ⇒ ⇒ 3 个后续已立项：`input-port-driving-event-diff-check-2026-10`（**事件驱动增量检查**，实测新增端口负载 0–2/commit）/ `input-port-required-registry-2026-10`（**必需端口注册表**，规范层已承认需要）/ `input-port-assignment-antipattern-fix-2026-10`（**10 处存量静默失效**，独立可做） |
| CLINT `mtime` 自增（T9 ②） | phase-5 | P2 | none | ⛔ **未立项 —— ADR-049 前置**：`mtime` 从静态比较器变自由运行计数器 = **契约变更**，而 **CLINT 不在任何 capability 覆盖内** ⇒ 归属本身待裁决 ⇒ delta 写不出 ⇒ **不得**先登记 change 再标 `skip_specs`（那会把契约变更静默丢掉）⇒ 决策门 D1–D5 已写入 `.rddf/improvements/clint-mtime-autoincrement-2026-10.md` §6 |
| RV32I ISA ④ 取指 skid buffer | phase-5 | P0 | none | ✅ **已归档**（2026-10-27）：`openspec/changes/archive/2026-10-27-rv32i-fetch-skid-buffer-2026-10/`（ADR-048；一个 change、两个 commit = C1 skid / C2 `primed`）⇒ `rv32i-commit-path` MODIFIED ×2 ⇒ **`feat-rv32i-isa-completeness` ④ 收口** |

> ✅ **sprint-2026-10 又归档一项**（2026-10-27）：`dual-backend-rv32i-integration-2026-10`（P1）——
> RV32I 子组件的 dual-backend 覆盖补齐 **Batch 2/3**（`IfIdPipelineReg` ×2 + `BranchPredictor` ×1
> ⇒ 该文件 13 → **16** dual pair / 26 → 32 用例）；**变异 3/3 全被抓**（第 1 轮 critic 抓到的
> BLOCKER = Batch 2-b 第 1 拍漏驱动 `flush` ⇒ 已修；M3' 实测**零判别力** ⇒ 如实记录
> 「未驱动端口没有通用变异判别力 ⇒ 靠端口驱动表 + 代码评审兜住」）；
> **无 spec 变更**（`.openspec.yaml` 显式声明 `skip_specs`）⇒
> `openspec/changes/archive/2026-10-27-dual-backend-rv32i-integration-2026-10/`；
> `CTEST_TOTAL` 202 / `EXAMPLES_TEST_SOURCES` 17 / `ctest -L verilator_e2e` 37 **均不变**。
> ⇒ **`objective-rv32i-e2e` 的 Go 判据 6/6 全部达成 ⇒ 转 `completed`**。

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

### Unmapped (2)

> 两个 improvement 草案尚无 change 目录（已归属 feature / objective，待立项）：
> `riscv-mini-cache-wrapper-live-logic-2026-10` → `feat-riscv-mini-rtl-debt` ①
> （⚠️ 名字与候选 change 名 `riscv-mini-cache-wrapper-live-logic-2026-10` **不一致**
> ⇒ 立项时 MUST 统一，见「实施规划」§遗留项 3）；
> `clint-mtime-autoincrement-2026-10` → `feat-riscv-mini-rtl-debt` ②。
>
> ✅ **sprint-2026-10 再归档一项**（2026-10-27）：`rv32i-fetch-skid-buffer-2026-10`（P0）——
> **RV32I 取指 skid buffer + 填充期 `primed` 门控**（ADR-048，一个 change、两个 commit）：
> C1 消除「load-use 停顿丢弃消费者之后第一条指令」（⇒ 固件**不再**需要在停顿点后插 NOP）；
> C2 消除「取指填充期同一条指令被装两次」（退休数 **179 → 178**、`non_retired` **20 → 21**，
> fill 段 2 → 3 拍，停顿/冲刷段不变）⇒ 同时消掉 ADR-047 之后升级成的**正确性**地雷
> （自依赖首条 + 前推 ⇒ 双装非幂等）；
> **变异 6/6 全被抓**（其中「删掉 flush 清空 skid」**只有组件级**能抓 ⇒ 与 ADR-048 D5 一致）；
> ⚠️ 实测补充：停顿后还有一次**重复装载** ⇒ C1 的「救回 1 条」与它**相抵** ⇒ C1 单独零观测面扰动。
> ⇒ `feat-rv32i-isa-completeness` ④ 收口（ISA 语义边界除 CSR/ECALL/FENCE/LB·LH 外全部落地）。

> ✅ **sprint-2026-10 已完成**：`axi4-interconnect-routing-2026-10`（P0）
> 于 2026-10-26 归档（`openspec/changes/archive/2026-10-26-axi4-interconnect-routing-2026-10/`），
> 新 capability `axi-interconnect-routing`（6 条 Requirement）。
<!-- AUTO-SPRINT-END -->

### Objectives
- `objective-cache-stub-retired` — cache stub 物理删除（ADR-037 deadline 2027-04-17）— **completed**
- `objective-riscv-mini-convergence` — 消除 riscv-mini 技术债 + 把 include/cpu 与 examples/riscv-mini 收敛成单一 RV32I 实现 — **completed**
- `objective-rv32i-e2e` — RV32I 完整 e2e 在 interpreter + verilator 双路 trace parity 通过（+ JIT 回落行为显式断言）— **completed**（Go 判据 **6/6** 全部达成，2026-10-27：判据 5「`dual-backend-rv32i-integration` archived」由 Batch 2/3 归档达成；判据 6「ISA 语义边界」由 ①②③ 达成 —— 范围注解见该 objective §2/§9.2）
- `objective-test-coverage-100pct` — 单元测试 100% 双后端（interpreter + verilator）覆盖 — **active**（`ctest -L verilator_e2e` = 37，目标 ≥ 100）
- `jit-mem-read-port-support` — JIT 对 `ch_mem` 读端口的 lowering（真三路 parity 前提）— **backlog，未建档**（当前 JIT 回落是有意设计且已断言；建档前置 = 出现必须 JIT-native 跑 RV32I 的需求）
- ~~`roadmap-fragments-consistency-guard` — roadmap fragments 一致性自检脚本 + CI 挂载~~ —— ✅ **2026-10-28 已实施并归档**（见上表）。⚠️ 实施期的**根因订正**：初稿写「CLI flag 存在、脚本 payload 缺失」⇒ **实测两头都反了** —— payload **存在可用**（工具链 `rdd-workflow` 的 `roadmap_update_agent_md.sh`），缺的是 **CLI dispatch**（`rddf roadmap` 没暴露 `--update-agent-md`）⇒ 工具**不可达** ⇒ AUTO 块只能手工维护。⇒ 护栏因此采用**与生成器同源的渲染规则**（渲染期望块 + 逐字比对），并用工具做**一次性 oracle** 等价性验证（工具链 SHA 已记录 + 再验证触发条件已写明）

> Sprint 编号口径：以本文件为准（`sprint-2026-10`）。三个 objective 的 append-only 台账曾把
> C1/C2/C3 归档记为 `sprint-2026-11`，属编号漂移 ⇒ 已在 `objective-rv32i-e2e` §11 追加澄清行
> （台账不可改写，只追加）。

