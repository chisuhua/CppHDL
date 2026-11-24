# Change: 提交事件端口脉冲化（`obs_data_write_en` / `dbg_wb_write_en`）

> **Change 名**：`rv32i-commit-event-pulse-2026-11`
> **母 improvement**：`rv32i-soc-platform-2026-11`（**S4a 的硬前置** · 状态 `in-progress`）
> **决策依据**：
> - **G-9 已裁决**（2026-11-24 · 用户）⇒ ⇒ S4a 的核内 pre-cache 分流会把
>   **MMIO 段**接进数据通路 ⇒ ⇒ MMIO 事务**必然多拍** ⇒ ⇒ `mem_stall` 必然出现
>   ⇒ ⇒ `.rddf/checks/decision-request-g9-core-port-face-v2-2026-11.md`
> - **已签批 spec `rv32i-commit-path`** ⇒ 「该指令的**写回** MUST 只发生**一次**」
>   + 「观测面 MUST 让『指令退休』可被外部验证」
>   ⇒ ⇒ 本 change 是**实现既有 Requirement**（不是新增能力）
> **日期**：2026-11-24 · builder
> **性质**：**观测面语义订正**（端口名 / 方向 / 位宽**一律不变** ⇒ **核端口面零变更**）
> ⇒ ⇒ **零公开面新增**（语义变更 ⇒ 登记 `public-api-surface.md` 的「语义订正」节）
> ⇒ ⇒ **零 spec delta 的 ADDED**（只 MODIFIED 既有 Requirement 的**观测面**条款）

---

## ⚠️ 订正块（2026-11-24 · 实施期实测 · **本块优先于下文任何相冲突的表述**）

> **触发**：实施期「先见红」阶段的**实测**推翻了初稿的**三处前提**。
> ⇒ ⇒ **下文 AC / What Changes 已按实测结论重写**；本块保留**被推翻的原文与证据**，
> ⇒ ⇒ 供审稿人复核「订正是否成立」。

| # | 初稿表述（**已被实测推翻**） | 实测证据 | 订正后 |
|---|---|---|---|
| **C1** | **AC1 的程序**（`addi x1` · `lw x2,0` · `addi x3` · `jal x0,0`）在旧实现下**必红** | 该程序在旧实现下 `wb_high = 3` = **写回条数** ⇒ ⇒ **零灌水** ⇒ ⇒ AC1 **恒绿** | AC1 的程序改为 **3 条 `rd≠0` 的写回类指令 + 1 条冷 `lw`**（见下文 AC1）⇒ 旧实现 `wb_high = 12` ⇒ **必红** |
| **C2** | **修法** = 两根端口都加 `& !mem_stall`（`design` §3） | `io().data_write_en & !mem_stall` ⇒ ⇒ **组合环** ⇒ ⇒ 仿真直接 **`FATAL: Cycle detected in IR graph during topological sort`**（`src/core/context.cpp:376`）⇒ ⇒ **该修法跑不起来** | **写回事件端口**（`dbg_wb_write_en`，纯观测面）门控 `& !mem_stall`；**store 事件端口**（`obs_data_write_en`）**不在流水线里门控** ⇒ ⇒ 改在**核顶层总线侧**派生（`data_write_en & data_ready_calc`）⇒ ⇒ **零功能变化** |
| **C3** | **AC1 的判据形式** = 「停顿期间事件端口**逐拍为 0**」 | `mem_stall` **不是顶层端口** ⇒ ⇒ 「停顿拍」在核外**不可直接观测** ⇒ ⇒ 该形式**无法实现为顶层判据** | AC1 改为**可观测的等价形式**：「**事件次数** == 程序里写回类且 `rd≠0` 的指令条数」+「**每个 rd 恰好一次**」+「**相邻重复 == 0**」 |

### 补：灌水的**充要条件**（实测确立 · 初稿缺失 ⇒ 这是 C1 的根因）

```
EX/MEM ──▶ M2 ──▶ MEM/WB ──▶ WB      （级序）
   ▲                   ▲
   │                   └── 装的是「EX/MEM 那条指令的**前 2 条**」
   └── 冷 load 在这里 ⇒ ⇒ 反压时 MEM/WB 被冻结 ⇒ ⇒ 里面的指令**停留多拍**
```

- `wb_stage.write_en = needs_writeback & rd_not_zero & mem_valid`（`stages/wb_stage.h:110`）
- `mem_valid` ← MEM/WB 寄存器（`rv32i_pipeline.h:662`）⇒ ⇒ 反压时**冻结**（`:637-652`）
- ⇒ ⇒ **灌水的充要条件** = 「反压发生时，MEM/WB 里恰有一条 `rd≠0` 的写回类指令」
- ⇒ ⇒ ⇒ 故冷 load 前 MUST 有 **≥ 2 条** `rd≠0` 的写回类指令
  ⇒ ⇒ ⇒ 「**load 紧跟 load**」**不够**（实测：旧实现 `wb_high = 2` ⇒ 零灌水 ⇒ **恒绿**）

---

## Why

1. 🔴 **两根事件端口是「电平」⇒ 停顿期间持续 ⇒ 任何按「计数」消费它们的观测面被灌水**：
   - `data_write_en` = `exmem_is_store`（`include/cpu/pipeline/rv32i_pipeline.h:693`），
     而 `exmem_is_store` 在 `mem_stall` 时**冻结保持**（`:568`）
     ⇒ ⇒ **停顿 N 拍 ⇒ 该端口高 N 拍**；
   - `dbg_wb_write_en` = `wb_stage.io().write_en`（`rv32i_pipeline.h:891`），
     而 `write_en = needs_writeback & rd_not_zero & mem_valid`
     （`include/cpu/pipeline/stages/wb_stage.h:110`）⇒ ⇒ `mem_valid` 来自 MEM/WB 寄存器，
     该寄存器在 `mem_stall` 时**冻结**（`rv32i_pipeline.h:637-652`）
     ⇒ ⇒ **停顿 N 拍 ⇒ 该端口高 N 拍**。
   ⇒ ⇒ **「事件次数」被污染成「事件时长之和」** ⇒ ⇒ **手推期望值不可得**
   ⇒ ⇒ 直接违反本仓纪律「**期望值不得依赖实现时序**」。

2. 🔴 **这在 S4a 之前是「潜伏」的**：核级/SoC 级现有程序**只有 `sw`（无 `lw`）**
   ⇒ ⇒ `sw` 走 D-L1 的 **write-through 单拍**路径 ⇒ ⇒ 无 fill ⇒ ⇒ **无 `mem_stall`**
   ⇒ ⇒ 今日实测 `store_count == 1` / `writeback_count == 3` 仍绿。
   ⇒ ⇒ 但 **S4a 的 SoC 级程序必然含 MMIO 访问** ⇒ ⇒ MMIO 事务**必然多拍**
   ⇒ ⇒ **`mem_stall` 必然出现** ⇒ ⇒ 灌水必然发生 ⇒ ⇒ **AC 的手推终值当场失效**。

3. 🔴 **仓内已有「已知违规」登记，且现有 workaround 掩盖了它**：
   - `openspec/specs/rv32i-commit-path/spec.md:96-99` 的 NOTE 明写
     「固件的第一条指令被写回两次（连续两拍 `wb_write_en=1`）」⇒ ⇒ **同一形态**；
   - `examples/riscv-mini/tests/test_rv32i_soc_dual_parity.cpp:958-974` 的注释明写
     「`wb_write_en` 是**电平**，同一条指令会在 WB **停留多拍** ⇒ ⇒ 逐拍收集会重复计数
     ⇒ ⇒ 折掉**连续相同** `(rd, data)`」⇒ ⇒ 且**自陈**
     「该折算会**掩盖**『同一条指令连续两拍写回**同一个** (rd,data)』这一形态」
     ⇒ ⇒ **已登记的判别力缺口**。

4. ✅ **核内已有正确口径可对齐**：`commit_count` 的推进条件是
   `commit_valid = mem_stage.io().mem_valid_out & !mem_stall`
   （`rv32i_pipeline.h:833-834`）⇒ ⇒ **退休计数已经正确排除了停顿**
   ⇒ ⇒ 两根事件端口与 `commit_count` **口径不一致** ⇒ ⇒ 这是**一致性缺陷**，不是新需求。

5. ⇒ ⇒ **结论**：S4a 之前 MUST 先把这两根**事件**端口改成与 `commit_count` 同口径的**事件脉冲**。

## What Changes

| 面 | 内容 |
|---|---|
| **库层** `include/cpu/pipeline/rv32i_pipeline.h` | ① `io().dbg_wb_write_en <<= wb_stage.io().write_en & !mem_stall;`（**纯观测面** ⇒ 门控**不成环**）<br>② `io().data_write_en <<= exmem_is_store;` **零改动**（**功能信号** ⇒ 门控会**成环**，见订正块 C2）<br>⇒ ⇒ 附带**口径注释**（说明「事件端口 vs 级属性端口」的判据） |
| **核 IP 顶层** `examples/riscv-mini/src/rv32i_core_top.h` | `io().obs_data_write_en <<= pipeline.io().data_write_en & data_ready_calc;`（`data_ready_calc` = `(!data_req) \| dcache.ready` ⇒ ⇒ 对 store 而言 = `aw_fire` ⇒ ⇒ **每次 store 恰好一拍**）⇒ ⇒ **端口名 / 方向 / 位宽不变** ⇒ ⇒ **核端口面仍是 34**（C6 快照不动） |
| **判据**（**追加**到既有）`examples/riscv-mini/tests/test_rv32i_soc_dual_parity.cpp` | 新增 `DUAL_BACKEND_TEST_PAIR(Rv32iCoreTop, "Rv32iSoc - CommitEventPulse", …)`：固件 = `addi x2,x0,7` · `addi x3,x0,9` · `addi x4,x0,11` · `lw x5,0x100(x0)` · `jal x0,0` ⇒ ⇒ 断言 ① **写回事件次数 == 4** ② **每个 rd 恰好一次** ③ **相邻重复 == 0** ④ **x5 的写回数据 == 0**<br>⇒ ⇒ **零新增 target** ⇒ ⇒ `CTEST_TOTAL` / `DUAL_BACKEND_PARITY_*` / `VERILATOR_E2E_TOTAL` 锚点**零变更** |
| **登记面** | `.rddf/checks/public-api-surface.md`（**语义订正**节）· `docs/CHANGELOG.md`（一行）· `include/cpu/AGENTS.md`（流水线观测面约定补一节）· `examples/riscv-mini/AGENTS.md`（核端口契约表的**语义**脚注 —— 表本身不动） |

## spec delta（**MODIFIED** · 不新增 capability）

- **MODIFIED** `rv32i-commit-path` 的「**RV32I SoC 顶层 MUST 有可观测的端口契约**」Requirement
  ⇒ ⇒ **既有 3 个 Scenario 逐字保留** ⇒ ⇒ 只**追加** 2 个 Scenario：
  1. 「**事件端口** MUST 是**单拍脉冲**（与事件时长无关）」；
  2. 「**事件端口** MUST 与 `commit_count` **同口径**（停顿期间两者 MUST 同时不增长）」。
- ⚠️ **不改** `rv32i-memory-timing`（它约束 MEM/WB 的**冻结**语义 —— 本 change **正是**依赖它）
- ⚠️ **不改** `cpu-cache-subsystem`（不改 cache 的任何行为）
- ⚠️ **不改** `rv32i-data-forwarding`（不碰前推）
- ⚠️ **不改** `example-component-boundary` / `rtl-soc-output` / `rtl-hierarchical-output`
  （产物端口面**零变更** ⇒ 那三个 capability 的条款**自动成立**）

## 用户可见变更

1. **观测面语义订正**（**端口名 / 方向 / 位宽全部不变** ⇒ ⇒ 核端口面仍 **34** ⇒ ⇒ C6 快照不动）：
   - `Rv32iCoreTop::obs_data_write_en` —— **store 提交事件**，由「**电平**」改为「**单拍脉冲**」
     （= 写使能 **且** 总线就绪 ⇒ ⇒ 对 store 而言 = AXI 写地址握手那一拍 ⇒ ⇒ 每次 store **恰好一次**）；
   - `Rv32iCoreTop::dbg_wb_write_en` —— **写回事件**，由「**电平**」改为「**单拍脉冲**」
     （= `write_en & !mem_stall` ⇒ ⇒ 与 `obs_perf_instructions` **同口径**）。
2. **功能面零变化**：`Rv32iPipeline::data_write_en`（→ D-L1 的 `we`）**保持电平** ⇒ ⇒
   D-L1 的写握手次数**恒为 1** ⇒ ⇒ 核级 `tb_rv32i_soc` 的 **24** 条判据与 SoC 级
   `tb_riscv_soc` 的 **7** 条判据**逐条不变**。
3. **下游可手推**：含数据反压的固件下，两根事件端口的「事件次数」不再被**停顿拍数**污染
   ⇒ ⇒ 下游（如 S4a 的 MMIO 通道）的手推计数终值**可由程序结构推出**。
4. **迁移指引**：把这两根端口当「**电平**」用的下游判据（如「高电平持续 ⇒ 事务未完成」）
   ⇒ ⇒ MUST 改判「**该拍发生了事件**」⇒ ⇒ **今日零可观测变化**（既有 golden 逐项不变）。

---

## 验收标准

> 期望值 MUST **手推**（禁抄仿真输出）· 事件次数 MUST 由**程序结构**推出。
> ⚠️ **核级现有 24 条判据 + `.sv` TB + golden 写回序列** MUST **逐项不变**（本 change 是**观测面**改，不是**行为**改）。

- [ ] **AC1** 🔴 **写回事件次数 == 手推条数**：程序 = `addi x2,x0,7` · `addi x3,x0,9`
  · `addi x4,x0,11` · `lw x5,0x100(x0)`（**冷行** ⇒ D-L1 miss ⇒ 4-beat burst fill ⇒
  实测 **8 拍**反压）· `jal x0,0`
  ⇒ ⇒ **写回事件 = 4**（x2 / x3 / x4 / x5；`jal x0,0` 的 `rd=x0` ⇒ **不写回**）
  ⇒ ⇒ ⚠️ **旧实现（电平）= 4 + 8 = 12**（实测）⇒ ⇒ **必红**
  ⇒ ⇒ ⇒ 这是「事件次数」与「事件时长之和」的**分离判据**
- [ ] **AC2** 🔴 **每个 rd 恰好写回一次**（x2 / x3 / x4 / x5 各 1 次）
  ⇒ ⇒ ⚠️ **旧实现下 x3 写回 9 次**（实测）⇒ ⇒ **必红**
  ⇒ ⇒ ⇒ 正面覆盖「同一条指令被重复写回」（既有 golden 的「折连续相同」workaround
  **掩盖**了这一形态 ⇒ 见 `test_rv32i_soc_dual_parity.cpp:962-964` 的自陈）
- [ ] **AC3** 🔴 **相邻重复 == 0**：相邻两拍 `dbg_wb_write_en` 都为高 **且 `(rd, data)` 相同**
  ⇒ ⇒ MUST 为 0 ⇒ ⇒ ⚠️ **旧实现下 = 8**（实测）⇒ ⇒ **必红**
- [ ] **AC4** 🔴 **写回数据未被脉冲化改变**：`x5` 的写回数据 MUST == 0
  （冷读 D-TCM ⇒ 零初始化）⇒ ⇒ 证明「脉冲化没有改变『哪条指令被写回、写回什么』」
- [ ] **AC5** 🔴 **核级既有判据零变化**：`examples/riscv-mini/tb_rv32i_soc` 的
  `RESULT: checks=24 fails=0` **不变** · `tb_rv32i_soc.sv` 的 24 条**不变**
  ⇒ ⇒ `test_rv32i_soc_dual_parity.cpp` 的**写回序列 golden 逐项不变**
  ⇒ ⇒ 「折连续相同」workaround 保留（脉冲化后**幂等**）⇒ ⇒ golden **零改动**
- [ ] **AC6** 🔴 **零回归**：`ctest` 全绿（**275** · 锚点零变更）· `run_all_ported_tests` PASS
  ⇒ ⇒ **全部 `scripts/verify_*.sh` 全绿**（含 C6 快照 —— 端口面**零变更** ⇒ C6 **不动**）
- [ ] **AC7** ⚪ **store 事件端口的判别力 = 归 S4a**（**如实登记 · 不是遗漏**）：
  今日 store 路径**不反压**（`aw_ready` 在 store 到达时恒 1 ⇒ 无 fill ⇒ 无 `mem_stall`）
  ⇒ ⇒ 今日 `obs_data_write_en` 的脉冲化是**零可观测变化**（既有固件 store 事件 = 5
  = store 条数）⇒ ⇒ 「事件次数 == store 条数」在今日**恒绿** ⇒ ⇒ **恒绿判据必须撤掉**
  ⇒ ⇒ ⇒ 判别力归 **S4a**（MMIO 事务必然多拍 ⇒ 那时才可判别）

## Out of Scope（本 change 明确不做）

- **`reg_file.write` 的「重复写入」** ⇒ ⇒ 停顿期间 WB 级仍会**每拍**写同一 `(rd, data)`
  （**幂等** ⇒ 寄存器堆内容无可观测差异）⇒ ⇒ **本 change 不动它**
  ⇒ ⇒ 如实登记为**残留**（见 design §4）
- **`dbg_exmem_is_store` / `dbg_id_is_store`** ⇒ ⇒ 它们是**级属性**（EX/MEM / ID 寄存器的内容）
  ⇒ ⇒ **电平是正确语义** ⇒ ⇒ **不改**（见 design §2 的「事件端口 vs 级属性端口」判据）
- **store 事件端口的脉冲化判据** ⇒ ⇒ 见 **AC7**（归 S4a）
- **MMIO 通道 / pre-cache 分流 / SoC 装配** ⇒ ⇒ **S4a**
- **`irq` 口** ⇒ ⇒ **S5**
- **D-TCM 窗口收紧**（今日越界访问静默别名）⇒ ⇒ **S4a 的 Out of Scope**（同批登记）
