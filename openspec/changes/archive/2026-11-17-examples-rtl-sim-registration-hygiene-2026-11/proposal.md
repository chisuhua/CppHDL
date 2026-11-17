# Proposal: examples 的 **RTL 仿真覆盖登记面**收口（登记完备 + 机器可消费）

> **来源**：2026-11-17「评估 CppHDL 能否生成功能完整的 RISC-V CPU 核 RTL，并把它推进为
> 「RTL 生成能力产品化」」的评估结论 —— 7 项产品化面里 **6 项已闭**，唯一**未闭**的是
> **③ examples 全覆盖**。
> **前序**：`rtl-productization-surface-2026-11`（已归档）· `soc-rtl-product-and-tb-2026-11`（已归档）
> ⇒ ⇒ 本 change **不改任何 capability 契约** ⇒ ⇒ **无 spec delta** ⇒ ⇒ 归档时无 spec 合并。

---

## Why

### 缺陷（实测 · 可复算 · 2026-11-17）

产品化的「**四件套**」判据（ADR-072 D2）已落地：manifest + Verilog 产物 + 独立可跑 TB +
覆盖登记。但**覆盖登记这一侧**有两处**静默失效**：

| # | 缺陷 | 证据 | 后果 |
|---|---|---|---|
| **D1** | **落选登记漏了 9 项** —— 被跟踪的 example 从 **19 涨到 28**，而落选名单只按 19 登记 | `examples/CMakeLists.txt` 的 `CH_TRACKED_EXAMPLE_TARGETS` = **28** 项；`.rddf/checks/examples-rtl-sim-exclusions.md:23` 仍写「共 **19** 项 ⇒ 落选 **10**」⇒ ⇒ 且它自己的表**只有 9 行** | ⇒ ⇒ **9 个被跟踪 example 既不在覆盖表、也不在落选表** ⇒ ⇒ 「**新增即漏**被堵住」这句承诺**已失效** ⇒ ⇒ 下次新增 example 会**继续**静默漏登记 |
| **D2** | **`spec_checks` 列被塞进散文** —— 4 行的该列不是裸整数 | `.rddf/checks/examples-rtl-sim-targets.tsv` 的 `fifo_stream` / `uart_tx` / `uart_rx` / `axi4_lite` 四行：`spec_checks` = `46 · ⚠️ **ADR-072 D1-f**：…` | ⇒ ⇒ 护栏 `scripts/verify_examples_rtl_sim.sh:222` 的 ②'' 软检查 `[ "$spec" -lt 3 ]` **报 `integer expression expected`** ⇒ ⇒ **该判据对 4 行永远不生效**（对 `gpio`（`spec_checks=0`）生效 ⇒ ⇒ 它的告警能力被削成 1/11） |

### 为什么现在做

1. **产品化契约要求登记面「机器可消费」** —— manifest 侧已做到（schema v2 的 `port_table[]`），
   覆盖登记侧还停在**散文** ⇒ ⇒ 两端不对称。
2. **D1 已造成一次实际漏登记**（9 项）⇒ ⇒ 不是「将来可能漏」，是「**已经漏了**」。
3. **D2 让一条判据静默失效** ⇒ ⇒ 与本仓反复治的「**看起来在守、其实恒绿**」同型
   （护栏进程 rc 仍为 0 ⇒ ⇒ **CI 绿** ⇒ ⇒ 缺陷不可见）。
4. **前置已具备**：落选理由与归位条件**本来就写在散文里** ⇒ ⇒ 本 change 只是把它们
   **转成机器可读** + **补齐漏项** + **加守恒判据** ⇒ ⇒ 不需要新的期望源推导。

---

## What Changes

### A. **落选表转机器可读**（单一真相）

- 新增 `.rddf/checks/examples-rtl-sim-exclusions.tsv`：**18 行**（= 28 被跟踪 − 10 已覆盖）
  ⇒ ⇒ 列：`target` · `category` · `reason` · `return_condition`
- `.rddf/checks/examples-rtl-sim-exclusions.md` 的「落选名单」表 ⇒ ⇒ **改为指向 TSV 的指针**
  ⇒ ⇒ **散文里的逐条理由迁入 TSV**（不保留双份 ⇒ ⇒ 杜绝漂移）
  ⇒ ⇒ 该文档保留：入选准则 / 首批的未覆盖项 / 墙钟实测

### B. **补登 9 个漏项**（D1）

`phase1c_test` · `stream_mux_demo` · `stream_demux_demo` · `stream_arbiter_demo` ·
`stream_fork_demo` · `phase3a_complete` · `rv32i_phase1_test` · `rv32i_phase2_test` ·
`rv32i_phase3_test`

⇒ ⇒ 每个都带**理由 + 归位条件**（与既有 9 行的口径一致）。

### C. **`spec_checks` 列恢复为裸整数**（D2）

4 行的散文 ⇒ ⇒ 迁到该行的 `note` 列（**信息不丢**）⇒ ⇒ `spec_checks` 变回整数
⇒ ⇒ 护栏 ②'' 判据**对 11 行全部生效**。

### D. **新护栏** `scripts/verify_examples_rtl_sim_coverage.sh`（静态 · fail-closed）

| # | 判据 | 判别力（负向对照） |
|---|---|---|
| **① 守恒** | 被跟踪 target 集合 == （覆盖表里 `kind ∈ {target, soc}` 且属于被跟踪集合的 target）∪（落选 TSV 的 target）⇒ ⇒ 且三者**互斥** | 删一条落选登记 ⇒ 必红 |
| **② 落选登记完备** | 每行 MUST 有**非空理由** + **非空归位条件** ⇒ ⇒ 且 target MUST 真在被跟踪集合里 | 删理由 ⇒ 必红 · 登记一个不存在的 target ⇒ 必红 |
| **③ 列纯度** | 覆盖表的 `spec_checks` 列 MUST 是**裸整数** ⇒ ⇒ 护栏 ②'' 判据可判 | 给 `spec_checks` 加散文尾巴 ⇒ 必红 |
| **④ `--self-test`** | 3 条投毒（① 删落选行 · ② 删理由 · ③ 污染 `spec_checks`）各自**转红** ⇒ ⇒ 还原复绿 | —— |

### E. **CI 接线 + 护栏可达性登记**

- `.github/workflows/ci.yml` 的 **`build-and-test`** job 加一步（**静态判据 ⇒ 不需要 verilator**
  ⇒ ⇒ 与 `pr-verilator-fast` 无关 ⇒ ⇒ 增量成本 ≈ 0）
- `.rddf/checks/guard-reachability.md` 的登记表**新增一行**（双向 1:1 棘轮）

---

## 验收标准

> ⚠️ **期望值 MUST 手推 / 可复算**（禁抄工具输出）。

- [ ] **AC1** 🔴 **守恒**：`CH_TRACKED_EXAMPLE_TARGETS`（**28**）== 覆盖（**10**：`kind ∈ {target, soc}`）
      ∪ 落选（**18**）⇒ ⇒ 且交集为空 ⇒ ⇒ 判据 = 新护栏 ①
- [ ] **AC2** 🔴 **变异自证**：删掉落选表里**一行** ⇒ ⇒ AC1 必红 ⇒ ⇒ 判据 = 新护栏 `--self-test` ①
- [ ] **AC3** 🔴 **落选登记完备**：18 行**逐行**有非空 `reason` + 非空 `return_condition`
      ⇒ ⇒ 且每个 target 真在被跟踪集合里 ⇒ ⇒ 判据 = 新护栏 ②
- [ ] **AC4** 🔴 **变异自证**：把某行的 `reason` 清空 ⇒ ⇒ AC3 必红 ⇒ ⇒ 判据 = 新护栏 `--self-test` ②
- [ ] **AC5** 🔴 **`spec_checks` 列纯度**：覆盖表 11 行的 `spec_checks` 列 MUST **逐行**是裸整数
      ⇒ ⇒ 判据 = 新护栏 ③
- [ ] **AC6** 🔴 **变异自证**：给某行的 `spec_checks` 追加散文 ⇒ ⇒ AC5 必红
      ⇒ ⇒ 判据 = 新护栏 `--self-test` ③
- [ ] **AC7** 🔴 **护栏②'' 恢复可判**：`scripts/verify_examples_rtl_sim.sh` 跑起来
      **零** `integer expression expected` ⇒ ⇒ 且 `gpio_example`（`spec_checks = 0`）**仍**告警
- [ ] **AC8** 🔴 **既有护栏不回归**：`verify_examples_rtl_sim.sh` **11 项 PASS** + `--self-test` 绿
      + `verify_example_test_wiring.sh` / `verify_example_assertion_quality.sh` 绿
- [ ] **AC9** 🔴 **回归**：`ctest --test-dir build -LE perf` = **255/255**
- [ ] **AC10** 🔴 **CI 接线 + 可达性登记**：新护栏在 `ci.yml` 里有引用 + 登记表有行
      ⇒ ⇒ `scripts/verify_guard_reachability.sh` 绿

### AC ↔ task 承载矩阵（**每个 AC MUST 有 task 承载**）

| AC | task |
|---|---|
| AC1 · AC2 | **T4**（新护栏 + `--self-test` 投毒 ①） |
| AC3 · AC4 | **T1**（落选 TSV 逐行登记）· **T4**（投毒 ②） |
| AC5 · AC6 | **T2**（`spec_checks` 纯化）· **T4**（投毒 ③） |
| AC7 | **T2** |
| AC8 | **T5** |
| AC9 | **T6** |
| AC10 | **T3** |

---

## Capabilities

| capability | 动作 | 理由 |
|---|---|---|
| （**无**） | — | ⚠️ 本 change 改的是**覆盖登记面**（仓内登记文件 + 护栏 + CI）⇒ ⇒ **不改任何 capability 的产品契约**（端口面 / manifest schema / 层级语义 / ISA 边界 **零改动**）⇒ ⇒ **无 spec delta** ⇒ ⇒ 归档时**无** spec 合并 |

---

## 用户可见变更

| 面 | 变更 |
|---|---|
| **覆盖登记** | 「哪些 example 已做 RTL 仿真 / 哪些没做」从此**机器可读**（TSV）⇒ ⇒ 且与被跟踪集合**守恒**（漏登记即 CI 红） |
| **护栏** | 新增 `scripts/verify_examples_rtl_sim_coverage.sh`（静态 · fail-closed） |
| **既有护栏** | `verify_examples_rtl_sim.sh` 的 ②'' 判据**恢复生效**（此前 11 行里 4 行失效） |

---

## Out of Scope

| 项 | 理由 | 归属 |
|---|---|---|
| **给未覆盖的 18 项补 RTL 产物 + 独立 TB** | 期望值 MUST **手推**（禁抄仿真输出）⇒ ⇒ **人工工作** ⇒ ⇒ 不可自动化 | 独立排期（每项按「新增一个产品单元」的流程走） |
| **把 `CH_TRACKED_EXAMPLE_TARGETS` 缩到 10** | 「不收窄目标，收窄声明」是 **2026-11-10 用户裁决** ⇒ ⇒ 本 change **只补登记**，不动目标集 | —— |
| **`rv32i-phase1/2/3` 作为独立产品单元** | 它们是**流水线阶段测试**（非产品）⇒ ⇒ 产品面由 `riscv-mini-rv32i-soc`（SoC 顶层）承载 | —— |
| **改 `verify_examples_rtl_sim.sh` 的既有判据** | 本 change **不碰**该脚本（并发会话正在改它 ⇒ ⇒ 写点零交集） | —— |

---

## 🚪 门（**开工前 MUST 闭**）

| # | 事项 | 状态 |
|---|---|---|
| **P-1** | 用户 2026-11-17 指令：「推进 RTL 生成能力产品化」⇒ ⇒ 覆盖登记面在其清单内（③ examples 全覆盖） | ✅ 已闭 |
| **P-2** | **写点零交集**：不碰 `verilator-pch-toolchain-fix-2026-11` 的任何写集（`CMakeLists.txt` · `scripts/verify_verilator_pch.sh` · `scripts/verify_examples_rtl_sim.sh` · `docs/developer_guide/verilator-integration.md` · `docs/CHANGELOG.md` · `.github/workflows/ci.yml` 的 `pr-verilator-fast` 段） | ⬜ 开工时核对 |
| **P-3** | **不新增 capability** ⇒ ⇒ **无 spec delta** ⇒ ⇒ 归档无 spec 合并 | ✅ 裁决 |
