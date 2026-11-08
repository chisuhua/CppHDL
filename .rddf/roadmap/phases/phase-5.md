---
id: phase-5
kind: phase
status: done
phase_refs: []
主题: [riscv-mini 单源化收敛与 RV32I 双路 parity, RV32I ISA 语义边界收口, riscv-mini RTL 缺陷收口, riscv-mini 技术债清零, dual-backend 全量覆盖残余, 平台矩阵扩展]
---

## phase-5 概览

> ✅ **2026-11-09 收口**（`status: active → done`）：9 条完成条件**全部有结论**
> —— 8 条 ✅ 达成 + 1 条（Windows-2022 CI）⛔ 不做（2026-11-03 用户裁决）。
> 核对依据见 `.rddf/reviews/phase-6-p2-roadmap-audit-2026-11.md`（phase-6 **P2 / P2b**）。

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
| **RV32I ISA 语义边界**（`feat-rv32i-isa-completeness`） | SB/SH 字节使能 · LB/LH/LBU/LHU 读字节道 · JAL/JALR 重定向 · SRA · **数据前推** + skid buffer / 取指双装 | ✅ 完成（**①–⑤ 全部归档**：① 2026-10-26 · ②③④ 2026-10-27 · ⑤ 2026-10-28 ⇒ feature `completed`；架构依据 ADR-046/047/048） |
| **riscv-mini RTL 缺陷收口**（`feat-riscv-mini-rtl-debt`） | cache wrapper 分支从不发射 · CLINT mtime 不自增 | ✅ 完成（① 2026-10-28 · ② 2026-10-30 归档 ⇒ feature `completed`；② 新建 capability `cpu-clint-timer`） |
| dual-backend 全量覆盖 | `feat-dual-backend-coverage` Phase 4 RV32I / Phase 5 全量（跨阶段残余） | ✅ 完成（**2026-11-03**：objective `objective-test-coverage-100pct` = `completed` · **parity 源文件 66 ≥ 60** ⚠️ 旧「`ctest -L verilator_e2e` ≥ 100」口径已于 **2026-10-30 修订**为「parity 源文件数 ≥ 60」⇒ 见 objective §2） |
| cache-stub 退役 | `include/cpu/cache/{i,d}_cache_stub.h` 物理删除 | ✅ 完成（2026-10-20，提前 6 个月，variant b） |
| 平台矩阵 | Windows-2022 CI 启用 | ⛔ **不做**（**2026-11-03 用户裁决**：CI 平台范围 = **仅 Ubuntu**；macOS / Windows 明确不做）⇒ feature `feat-platform-matrix` 据此 `completed` |

## 计划能力（change 序列 C1 → C2 → C3 → ISA 收口线）

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
4. **`feat-rv32i-isa-completeness`**（2026-10-26 审计新登记，**5 个独立 change**，顺序 ①→②→③→④→⑤）：
   ① `rv32i-store-byte-enable-2026-10`（DataTCM `strbe` + **wdata lane 对齐** + SB/SH 固件用例）
   —— ✅ 2026-10-26 归档；
   ② `rv32i-jump-sra-2026-10`（JAL/JALR 重定向 + JAL 写回 pc+4 + SRA 真算术右移）
   —— ✅ 2026-10-27 归档（连带修 U 型立即数 / I 型 SUB 误判 / 错误路径劫持 PC）；
   ③ `rv32i-forwarding-decision-2026-10`（**ADR-046 D5 裁决** + HazardUnit 5 个预留输入端口清理）
   —— ✅ 2026-10-27 归档（ADR-047 裁决「接」⇒ 新 capability `rv32i-data-forwarding`；
   顺带修掉**距离 3 读旧值**的既有缺陷 —— 两后端同构地错 ⇒ parity 抓不到）；
   ④ `rv32i-fetch-skid-buffer-2026-10`（取指 skid buffer + 取指填充期 `primed` 门控）
   —— ✅ 2026-10-27 归档（ADR-048）；
   ⑤ `rv32i-load-byte-lane-2026-10`（**load 侧**字节道：`LB`/`LH`/`LBU`/`LHU` 车道选择 +
   符号/零扩展）—— ✅ **2026-10-28 归档**（★ 原计划只有 ①–④ ⇒ ⑤ 是审计后再拆出的独立 change；
   实施中修掉 `LB` 的符号扩展掩码笔误 `0xFF000000`）。
   **目的：把「parity 通」升级为「符合 ISA」** —— C3 只证明两后端彼此一致，不证明实现正确。
   ⚠️ ①–⑤ 完成后**仍不覆盖**：CSR / ECALL / EBREAK / FENCE
   （`LB`/`LH`/`LBU`/`LHU` **已由 ⑤ 覆盖**）
   ⇒ 「ISA 语义边界」在本 phase 的口径 = **字节道两侧 + JAL/JALR + SRA + 前推 + 取指健壮性**
   —— 不是「RV32I 完整」（剩余三项由 phase-6 **P6** 独立立项）。
5. **`feat-riscv-mini-rtl-debt`**（P2）：`riscv-mini-cache-wrapper-live-logic-2026-10` +
   `clint-mtime-autoincrement-2026-10`（两个既有 improvement 草案的归属地）。
6. **（独立 backlog，非本 sprint、未立项）** `jit-mem-read-port-support` — JIT 对 `ch_mem` 读端口的 lowering，
   真三路 parity 的前提。**当前无真实需求**（JIT 回落是 `src/jit/jit_compiler.cpp:256-264` 的**有意设计**，
   且回落原因已可归因并被断言）⇒ 建档前置 = 出现「必须 JIT-native 跑 RV32I」的实际需求。

> ⚠️ **「三路 parity」定义已裁决**（2026-10-23 用户决定）：实测 `src/jit/jit_ir_gen_lnode_state.cpp:58`
> 对 `type_mem_read_port` 返回 `UNSUPPORTED_OP` ⇒ 含 imem 的 RV32I 设计 JIT 必然整体回落 interpreter。
> 故 `objective-rv32i-e2e` 从「interpreter + verilator + JIT 三路」**重定义为
> 「双路 parity（interpreter + verilator）+ JIT 回落行为显式断言」**；JIT-native 三路
> **降为 backlog（未立项，见上第 6 条）** —— 2026-10-26 订正：此前写「作为独立 objective 排期」，
> 但 `.rddf/roadmap/objectives/` 从未建过该 fragment（dangling 引用）。

## 依赖关系

```
C1 test-triage（绿色基线，无依赖）
    ↓
C2 canonical-convergence（前置 ADR-046 架构裁决；依赖 C1 基线）
    ↓
C3 soc-dual-parity（双路 parity + JIT 回落断言；依赖 C2 单源化）
    ↓
feat-rv32i-isa-completeness：
    ① store-byte-enable ──┐
    ② jump-sra ───────────┤ 全部依赖 C3 的 32 端口观测面 + 48 字固件金标
    ③ forwarding-decision ┘（需 ADR-046 D5 裁决）→ ④ fetch-skid-buffer（依赖 ③）
    ⑤ load-byte-lane（无依赖）
    ✅ ①–⑤ 全部已归档（2026-10-26 / 10-27 ×3 / **10-28**）

feat-riscv-mini-rtl-debt（独立，P2）：cache-wrapper-live-logic 依赖 C2 已迁移路径；
                                    clint-mtime 无依赖
```

并行：`feat-dual-backend-coverage` Phase 4 RV32I 残余（IfIdPipelineReg clk-spike + BranchPredictor
follow-up）与 `feat-riscv-mini-rtl-debt` 可与 ISA 线并行（互不触碰）。
⚠️ `feat-rv32i-isa-completeness` ① 会改变 store 写语义 ⇒ **MUST 先于**任何基于当前固件重算金标的 change。
✅ **已满足**（① 2026-10-26 归档 ⇒ 其后 ②③ 的金标重算均以新写语义为基线）。

## 架构文档锚点

| 文档 | 与本 phase 关联 |
|------|----------------|
| `docs/adr/ADR-046-rv32i-canonical-home.md`（✅ 2026-10-23 落盘，C2 实施依据 D1–D12） | 收敛方向裁决：`include/cpu/` 为唯一权威 + example 只留 SoC 顶层；**D5（前推是否接）已由 ADR-047 裁决** |
| `docs/adr/ADR-047-rv32i-data-forwarding.md`（✅ 2026-10-27 落盘） | 数据前推裁决：接（D2/D5/D6/D7/D8）+ **D11 寄存器堆读点旁路**（距离 3）+ 距离矩阵成为 capability 契约 |
| `docs/adr/ADR-038-forwarding-namespace-orphan.md` | forwarding 重复的既有裁决（C2 已收敛；`ForwardingMux` 与 `riscv::ForwardingUnit` 的合并仍是 backlog） |
| `docs/adr/ADR-039-cache-complete-rewrite-via-ch-mem.md` | cache 重写路径 |
| `docs/adr/ADR-041-cache-sync-async-variant.md` | cache Sync/Async 拆分 |
| `.rddf/improvements/riscv-mini-test-triage-2026-10.md` | C1 improvement 草稿 |
| `.rddf/improvements/rv32i-canonical-convergence-2026-10.md` | C2 improvement 草稿 |
| `.rddf/improvements/rv32i-soc-dual-parity-2026-10.md` | C3 improvement 草稿 |
| `.rddf/improvements/rv32i-forwarding-decision-2026-10.md` | ISA ③ improvement 草稿（✅ 已归档，2026-10-27） |
| `.rddf/improvements/riscv-mini-cache-wrapper-live-logic-2026-10.md` | RTL 缺陷收口 ① improvement 草稿（P2） |
| `.rddf/improvements/clint-mtime-autoincrement-2026-10.md` | RTL 缺陷收口 ② improvement 草稿（P2） |
| `.rddf/plans/2026-10-26-phase5-followup-execution-plan.md` | **阶段级执行总纲**（T1–T10 的步骤 / 验收 / 验证矩阵 / planner 审查记录） |
| `include/cpu/AGENTS.md` | STRUCTURE 表（C2 已同步；新增组件需继续维护） |
| root `AGENTS.md §KNOWN LIMITATIONS` | RV32I 的 6 处实现边界（ISA 3 + 流水线 3）+ Windows CI 延后 —— **ISA 线的事实依据** |

## 完成条件

- [x] C1 `riscv-mini-test-triage-2026-10` archive（19 target 清零 + 护栏 + CTEST_TOTAL 同步）— 2026-10-24
- [x] ADR-046 `rv32i-canonical-home` 落盘（2026-10-23，rdd-arch 职责，C2 前置）
- [x] C2 `rv32i-canonical-convergence-2026-10` archive（17 死头删除 + 10 组同名裁决 + 反向 include 消除 + 护栏）— 2026-10-25
- [x] **前置** `rv32i-pipeline-commit-path-2026-10` archive（SoC 观测面 **32 端口** + **17 个流水线缺陷**）— 2026-10-26
- [x] C3 `rv32i-soc-dual-parity-2026-10` archive（双路 parity + JIT 回落断言）— 2026-10-26
- [x] cache-stub 物理删除（ADR-037）— 2026-10-20 落地（variant b，change `cache-stub-physical-removal-2026-10` 已 archive）
- [x] dual-backend 全量覆盖（`feat-dual-backend-coverage` Phase 4 RV32I / Phase 5 全量）
      — **2026-11-03 达成**：objective `objective-test-coverage-100pct` 转 `completed`
      · **双后端 parity 源文件数 = 66 ≥ 60**（`scripts/verify_dual_backend_parity.sh` 实测）
      ⚠️ 完成判据已于 **2026-10-30 修订**（旧口径「`ctest -L verilator_e2e` ≥ 100」在现有语料内
      **不可达**：诚实上限 [71, 96]）⇒ 现口径见 objective §2
- [x] RV32I ISA 语义边界闭合（`feat-rv32i-isa-completeness` **①–⑤**）
      — ① 2026-10-26 · ②③④ 2026-10-27 · **⑤ 2026-10-28**（原写 ①–④ ⇒ ⑤ 是拆出的
      独立 change `rv32i-load-byte-lane-2026-10`）⇒ feature `completed`
      ⚠️ 本项口径**不含** CSR / `ECALL` / `EBREAK` / `FENCE`（⇒ phase-6 的 **P6** 独立立项）
- [x] riscv-mini RTL 缺陷收口（`feat-riscv-mini-rtl-debt` ①–②，P2）
      — ① `riscv-mini-cache-wrapper-live-logic-2026-10` 2026-10-28 归档 ·
      ② `clint-mtime-autoincrement-2026-10` 2026-10-30 归档（ADR-049）⇒ feature `completed`
- [x] 逻辑算子 `[[deprecated]]` **标记在位** + 0 新增调用点（8 个 overload 仍在
      `include/core/operators_logic.h` / `io_logical.h`）
      ⚠️ **其「移除」不是本 phase 的完成条件**：ADR-036 §8.1 规定 **2027-04-17 前拒绝任何移除 commit**
      （日历锁）⇒ 原先把「移除」列为 phase-5 完成条件会使 phase-5 最早 2027-04-17 才能收口。
      移除改归 **phase-6 的 calendar-gated 清单**（2026-10-26 修订）
- [x] ~~Windows-2022 CI 启用~~ ⇒ ⛔ **不做**（**2026-11-03 用户裁决**：CI 平台范围 = **仅 Ubuntu**）
      ⚠️ 原 **No-Go 判据（2026-10-26）** 的处置 = 「转 phase-6 并记录理由」⇒ 已执行：
      phase-6 **P2b** 只登记理由 + 关闭本条件，**不动 CI**（`.github/workflows/ci.yml` 全部
      `runs-on: ubuntu-22.04`，唯一矩阵 `${{ matrix.os }}` 的取值 = `['ubuntu-22.04']`）
      ⇒ 该项已跨 phase-3/4/5 三阶段挂 ⏳，**在此终止**
