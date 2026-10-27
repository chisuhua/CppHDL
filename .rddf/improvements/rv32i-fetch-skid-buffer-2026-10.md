---
name: rv32i-fetch-skid-buffer-2026-10
priority: P0
source: 2026-10-27 roadmap 审查（feat-rv32i-isa-completeness ④ 的载体缺失；plan T5）
phase: phase-5
category: cpu
type: bugfix
created_at: 2026-10-27
---

# Improvement: RV32I 取指 MUST 有 skid buffer（停顿不丢指令）+ 填充期 MUST NOT 双装

- **Date drafted**: 2026-10-27
- **Drafted by**: build-agent (planner role)
- **Status**: 📋 5-segment draft；change 目录**待建**
  （本草案是 `feat-rv32i-isa-completeness` ④ 的**载体** —— 此前只有
  feature ④ 的条目 + plan T5 的范围定义，**无 improvement** ⇒ 与
  `dual-backend-rv32i-integration` 同型的「载体缺失」）
- **依据**（全部来自**已归档**的实测证据链，不重新臆测）：
  - `openspec/changes/archive/2026-10-26-rv32i-pipeline-commit-path-2026-10/design.md`
    **§3.6**（填充期双装）+ **§3.10**（停顿丢指令）—— 第 16/17 轮 critic 的实测记录
  - `.rddf/plans/2026-10-26-phase5-followup-execution-plan.md` **T5**（范围 / 步骤 / 验收 / 风险）
  - `openspec/specs/rv32i-commit-path/spec.md`（**§已知边界 :33-36** + Scenario
    「停顿 MUST 只**延迟**退休 —— ⚠️ 当前有一处例外」:259-268 + 「退休指令数的推导」:270-278）
  - 根 `AGENTS.md §KNOWN LIMITATIONS` 的两条对应残留
- **roadmap 归属**：`feat-rv32i-isa-completeness` ④（ISA 语义收口的**最后一项**）

## 1. Why (背景与动机)

### 缺陷 A：load-use 停顿会**丢弃「消费者之后的第一条指令」**

**机制**（design.md §3.10，第 17 轮 critic 实测 + 逐拍 trace 双向确认）：
imem 是 **1-cycle `sread`** ⇒ ID 级译码 word `W` 的那一拍（**检测拍 T**），
`pc_reg` 已指向 `W+2`；而 word **`W+1`** 的地址在 **T−1** 就已提交 ⇒ 数据
**恰好在 T 拍返回** —— 正是 `if_id_update_en` 变 0 的第一拍 ⇒ **无处可存**。
⇒ 停顿解除后 IF/ID 锁存的是 `W+2` ⇒ **word `W+1` 从指令流里消失**。

**实测证据**（该 change 自己的 trace）：ID 级序列在停顿 #1 处是
word 21（保持）→ **word 23**（word 22 从未进 ID）；停顿 #2 处 word 44 → **word 46**。
⚠️ **两处被丢的都是 NOP** ⇒ 全部 golden 仍绿 ⇒ 典型的「**绿 ≠ 对**」。

**当前边界（如实登记）**：`固件 MUST 保证「load-use 停顿点之后的那条指令」是 NOP`。
已登记在 `AGENTS.md` 的 KNOWN LIMITATIONS 与 `rv32i-commit-path` capability
（Scenario :259-268 明确写出这个**例外**）⇒ 不会把一条当前不满足的 MUST 冻进主 spec。

**正解**（ADR-048，**已 Accepted** 2026-10-27）：**取指 skid buffer（1 级缓冲）**
—— 停顿期间「已提交但未锁存」的那条指令要有个地方暂存。
⚠️ **2026-10-27 订正**（ADR-048 D3 经 oracle 独立复核后**否决**「PC 回退」）：
「skid buffer + **PC 回退**」是 design.md §3.10 / plan T5 的表述，**未经论证** ⇒
完整论证见 ADR-048 D3：停顿 ⇒ `pc_reg` 冻结 ⇒ 取指地址流**自我重复**
⇒ 落在 PC 之后的在途数据**至多一条** ⇒ skid 接住它即可 ⇒ 序列连续 ⇒ **无丢失无重复**
⇒ **PC 回退不需要**（且 `PC ← skid_pc` 会**制造**双装、`PC ← skid_pc + 4` 是**空操作**）。
⇒ 仍是**结构性改动**（IF/ID 旁多一级缓冲 ⇒ **全部时序/金标都要重算**）
⇒ 独立立项 ⇒ change `rv32i-fetch-skid-buffer-2026-10`（**已立项**）。

### 缺陷 B：取指填充期「同一条指令被装两次」

**现象**（design.md §3.6）：固件 word 0 **被写回两次**（连续两拍 `wb_write_en=1`，
`rd=x1`、`data` 相同）⇒ 幂等写回，但 ⚠️ **退休数会 +1**
（第 16 轮 critic 实测：`perf_instructions` 在 cyc 3 与 cyc 4 各 +1）
⇒ **「退休数不受影响」是错的** ⇒ 现有测试的退休数金标**含**这 +1。

⚠️ **2026-10-27 订正**（ADR-048 §1.3.2）：接上**数据前推**（ADR-047）后，
**第二条** word 0 会**读到第一条的前推值** ⇒ 若 word 0 **自依赖**（`rd == rs1`）
⇒ **不再幂等** ⇒ 这是**正确性**地雷，不再只是计数偏差
⇒ 现有防线 `fwd_first_instruction_not_self_dependent` 是「**固件级巧合在挡正确性缺陷**」
⇒ 「缺陷 B 必须一并修」的**决定性理由**。

**根因链**（⚠️ **2026-10-27 订正**：design.md §3.6 的「**建图期组合求值**」措辞**不准**）：
`Simulator` 只把 proxy 槽位**分配为 0**（`src/simulator_init.cpp`）⇒ **没有**建图期求值；
真正的预填是 **`load_firmware` 的加载拍**（= 固件 word 数，每 word 一拍；`ext_rst=1`
下逐拍 tick ⇒ sync 读端口照常工作：解释器**无 reset 意识**，Verilog 的 reset 分支
只对框架 `default_reset` 生效而 `ext_rst` 是 fabric 端口）
⇒ ⇒ **两后端对称预填** proxy ⇒ 幻影装载与下一拍的**真实取指**是**同一地址同一数据**
⇒ 同一条指令进流水线两次。（PC 序列看起来「重复」是**巧合**：`pc_reg` 初值 = 第一次取指
地址，而 `fetch_pc_reg` 让 IF/ID 的 pc 延迟 2 拍 ⇒ 「拍 −2」与「拍 −1」取到的地址相同。）

**修法**（ADR-048 D6）：**局部**可修 —— `IfStage` 内部加 `primed` 标志（复位后首拍置 1），
IF/ID 装载以「已就绪」为前提 ⇒ 幻影装载**不进流水线** ⇒ 退休数 **−1**；
⚠️ 判据 **MUST NOT** 由 `instr_ready` 导出（它恒为 1，不携带信息）
⇒ 框架层「加 API」（D7）仅在局部修法不可行时才需要 ⇒ **本 change 不动框架层**。

### Why now

- ③（数据前推，ADR-047）已落地 ⇒ 固件**不再需要**为「依赖间隔」插 NOP ⇒ 固件里的 NOP
  **只剩**「load-use 停顿点之后那一条」这一类残留 ⇒ ④ 是这条残留的**唯一**解法；
- ①②③ 都已重算过金标 ⇒ ④ 的时序重算有**稳定基线**（而不是在混沌中重算）；
- `rv32i-commit-path` capability 里那条**例外**（:259-268）是**写进主 spec 的债** ⇒
  ④ 落地后 MUST 用 MODIFIED 把它去掉（主 spec 契约变更 ⇒ MUST 走 change + archive 合并）。

## 2. What Changes (变更范围)

### 2.1 取指 skid buffer（缺陷 A 的正解，主体）

**文件**：`include/cpu/pipeline/stages/if_stage.h`（`IfStage::describe()`）

1. **新增 1 级 skid buffer**（3 个寄存器 + 1 个 valid）：
   `skid_instr` / `skid_pc` / `skid_valid`
   - **填充条件**：停顿期间「取指数据已回、但 IF/ID 装载被停顿门控」
     ⇒ `stall && instr_ready && !skid_valid` ⇒ 把 `fetched_instr` + `fetch_pc_reg` 存进去
   - **排空条件**：停顿解除且 skid 有货 ⇒ **IF/ID 的装载源改为 skid**
     （pc 取 `skid_pc`，指令取 `skid_instr`）⇒ 被丢的那条指令**回到指令流**
   - ⚠️ 1 级**够用**的理由：停顿期间 `pc_reg` 被冻结 ⇒ `instr_addr` 不变 ⇒
     imem 反复返回**同一个** word ⇒ 唯一「会消失」的指令只有**停顿首拍已在途的那一条**
     ⇒ 深度 1 即可覆盖（深度 2+ 是无谓的复杂度）
2. **⚠️ PC 回退：已否决**（ADR-048 D3）—— 停顿期间 `pc_reg` 被冻结 ⇒ 取指地址流**自我重复**
   ⇒ 落在 PC 之后的在途数据**至多一条** ⇒ skid 接住它即可 ⇒ 停顿解除后 IF/ID 先取 skid
   （= `W+1`）、下一拍从正常流取（= `W+2`，地址仍冻结 ⇒ 数据被重读）⇒ 序列连续
   ⇒ **无丢失、无重复** ⇒ **不需要**回退
   - ⚠️ 两种误实现形态（**勿犯**）：`PC ← skid_pc` ⇒ 把已排空的 `W+1` **再取一次**
     ⇒ **制造**双装；`PC ← skid_pc + 4` ⇒ 恒等于冻结的 `pc_reg` ⇒ **空操作**
   - ⇒ 附带收益：PC 选择器仍是**两路** ⇒ `if_stage.h:84-103` 的
     「flush 唯一来源 ⇒ `pc_update_en` 不看 flush」不变量**原样保留**
3. **flush 交互**：flush 拍 MUST **同步清空 skid**（否则错误路径的指令会被塞进 ID）
   - ⚠️ flush 与 stall **不同拍**（既有互斥论证见 `if_stage.h:105-119`：flush 来自
     EX 级的 `branch_taken` ⇒ 那一拍 EX 级只有分支指令 ⇒ 不是 load ⇒ 不可能同拍成立）
     ⇒ 但「flush 发生时 skid 里可能正好有货」这一情形**仍需明确处置**
     （分支在 EX 级被解析时，IF 级早已取过指 ⇒ skid 可能有货）
   - ⚠️ **2026-10-27 补强**（ADR-048 D5）：「flush ∥ skid_valid」在当前微架构下
     **不可达**（skid 非空 ⇒ 停顿窗口 ⇒ EX 是气泡 ⇒ 无分支）⇒ 上述是**防御性契约**
     ⇒ 验收 MUST 落**组件级**（SoC 级变异抓不到）⇒ 理由：可达性论证依赖多重耦合前提，
     而 `if_stage.h:100-103` 已预警「引入第三个 flush 来源前 MUST 重审」

### 2.2 填充期「未就绪」判据（缺陷 B 的**局部**修法 —— 决策门 1 已裁决：**可局部修**）

**方案**（ADR-048 D6）：`IfStage` 内部新增 `primed` 标志（复位后首拍置 1），
IF/ID 装载 MUST 以「`primed` 已置位」为前提 ⇒ 幻影装载**不进流水线** ⇒ 退休数 **−1**。
- ✅ **决策门 1 已裁决**（2026-10-27 用户）：**按判据执行** ⇒ 判据成立（可局部修）
  ⇒ **本 change 不动框架层** ⇒ ADR-048 D7（框架层加 API）**不启动**
- ⚠️ 判据 **MUST 内生于 `IfStage`**：`instr_ready` **恒为 1**（`rv32i_tcm.h:58`）
  ⇒ **不能**由它导出「已就绪」（它不携带信息）
- ⚠️ **两种门控面**（ADR-048 D6）：默认「**只门控 `valid`**」⇒ 幻影装载的 pc 是
  **0x80000000**（与真值相同）⇒ `dbg_id_pc` 观测序列**零变化**；
  改选「门控全部三个装载」⇒ cycle 1 显示复位值 0 ⇒ 观测面 golden 扰动更大
  ⇒ 改选须在 change 里记录
- ⚠️ **填充期两后端是否对称**（2026-10-27 已澄清）：**当前流程下对称** ——
  预填发生在 `load_firmware` 的加载拍（`ext_rst=1`）⇒ 解释器**无 reset 意识** ⇒ 照常写；
  Verilog 的 reset 分支只对框架 `default_reset` 生效，而 `ext_rst` 是 fabric 端口
  ⇒ 两后端**都**预填 word 0 ⇒ **不产生分叉**
  ⇒ 但「解释器的 sync 读端口**没有** reset 输入」这一**潜在**分歧仍在
  （ADR-048 D3.1 如实登记）⇒ 对 skid 无影响（skid 只在停顿窗口填充）

### 2.3 框架层评估（**仅当**决策门 1 判定需要）

**目标**：`ch_mem::sread` 输出端口的「未初始化」语义 —— 是否提供
**「读端口尚未取指」的可观测判据**（哨兵值 or 独立的 valid 信号）。
- 影响面清单 MUST **逐个列出**所有 `sread` 用户（`ch_mem::sread` 的消费者）：
  imem（`rv32i_tcm.h` 的 ITCM）、`chlib::sync_fifo`、cache Sync 变体
  （`i_cache_sync.h` / `d_cache_sync.h`）、AXI 互连相关读口、perf 基准路径
- ⚠️ **MUST NOT** 改 `aread` 语义（0-cycle 组合读已与 Verilog `assign` 对齐；
  改它会波及 FIFO 的 `dout` 时序契约 —— 见 `rv32i-commit-path` 与
  `mem-read-timing` capability）
- ⚠️ 若提供**新 API**（如 `sread_valid()`）而非改语义 ⇒ 影响面更小（增量而非替换）
  ⇒ 决策门 2（见 §4）

### 2.4 测试（既有 target ⇒ **计数不变**）

**文件**：`examples/riscv-mini/tests/test_rv32i_soc_dual_parity.cpp`（既有）
- **判别性固件**：load-use 停顿点之后放一条**非 NOP**（例如 `addi`）
  ⇒ **未修前**该指令**消失**（写回序列少一条 / 后续 pc 错位）⇒ **修后必须出现**
  - ⚠️ 反向期望 MUST 写进代码注释（`X_old != X_new`）
  - ⚠️ 期望值 MUST **手工推导**（MUST NOT 从仿真输出抄值 —— 循环论证）
- **退休数金标**：若缺陷 B 消除 ⇒ 退休数 **−1**
  ⇒ MUST 手工推导 + 留痕；**基线从 179 起算**（⚠️ **不是 188** —— 见下方「与既有文档的
  差异」：spec 写的 188 自 2026-10-26 起就已与测试脱节 ⇒ 本 change 的 MODIFIED
  MUST 先修基线）⇒ 缺陷 A 另加 `+k`（被救回的指令真实退休）⇒ 最终数字 MUST 由**实测**回填
  ⇒ 更稳的锚点 = `non_retired` 的**分段分解**（fill / stall / flush 各几拍）
- **flush × skid 交互**：有断言（错误路径的指令不进 ID）
- **双后端**：逐拍全等（无掩码）

### 2.5 spec 与簿记

- `rv32i-commit-path` **MODIFIED**：
  - Scenario「停顿 MUST 只**延迟**退休（MUST NOT 丢弃指令）—— ⚠️ 当前有一处例外」(:259-268)
    ⇒ **去掉例外**，改为「停顿 MUST NOT 丢弃任何指令」+ skid 的契约
  - `## Purpose` 段的「⚠️ 已知边界」(:33-36) ⇒ 改写
  - Scenario「退休指令数的推导 MUST 与测试断言同义」(:270-278) ⇒ 若缺陷 B 消除则数字改
  - ⚠️ **MODIFIED 会整体替换该 Requirement** ⇒ MUST 保留**原标题**与全部条目
    （既有 `openspec validate` 规则：删改既有 Scenario 标题会被拒）
- `mem-read-timing`：**可能** MODIFIED（仅当 §2.3 动了 `sread` 语义）
- `rv32i-isa-semantics`：**可能** MODIFIED（固件覆盖扩大 → 能力面描述）
- `verilator-rv32i-e2e`：**仅当** SoC 观测端口变化 ⇒ ⚠️ **ADR-046 D10 冻结了 32 端口**
  ⇒ 需**先**改 D10（rdd-arch 职责）再实施
- 根 `AGENTS.md §KNOWN LIMITATIONS` 的**两条**残留消解 + 「固件 MUST 用 NOP 填充该位置
  （已有 compile-time 静态护栏）」—— ⚠️ **该 compile-time 护栏**（若存在）落地后
  MUST 同步处理（删除或改为「允许非 NOP」）⇒ 实施时 MUST 先定位它
- `feat-rv32i-isa-completeness` ④ 勾选 + roadmap 状态

## 3. Acceptance (验收标准)

- [ ] **判别性固件**：load-use 停顿点之后的**非 NOP** 指令**不消失**
      （写回序列里出现它；未修前的行为作为反向期望写进注释）
- [ ] **退休数** == 实际指令数 **或** 明确登记为残留并写进 ADR
      （二选一，取决于决策门 1；MUST 在 change 里写明选了哪个及理由）
- [ ] **填充期双装**要么消除（退休数金标 −1 且首条指令只写回一次）、
      要么**保留登记**且**两后端行为一致**（MUST 实测两后端的填充期序列并记录）
- [ ] **flush × skid** 行为自洽（有断言：错误路径的指令不进 ID；skid 不泄漏错误路径）
- [ ] **PC 回退**不产生重复取指（`W+2` 不被取两次 ⇒ 退休数不多算）
- [ ] 全部时序金标**重算**并**手工推导留痕**（停顿拍数 / 冲刷拍数 / 写回序列 / 退休数）
- [ ] 双后端**逐拍全等**（无掩码）+ `ctest -LE perf` 零回归
- [ ] 若动框架层 ⇒ `sread` 用户影响面清单**逐项**回归通过（清单见 §2.3）
- [ ] **变异验证**（每条先确认编译成功）：
      - 关掉 skid 排空 ⇒ 应复现「丢指令」⇒ 转红
      - 把排空条件写反（`stall` 时排空）⇒ 转红
      - PC 回退少加 4 ⇒ 重复取指 ⇒ 退休数断言转红
- [ ] `CTEST_TOTAL` **202 不变** / `EXAMPLES_TEST_SOURCES` **17 不变**（用例进**既有** target）

## 4. Decisions Needed (决策门 —— planner 裁决点)

> ✅ **2026-10-27 用户已接受草案默认裁决**（记录如下；本 change 的 proposal MUST 引用本节）：

| # | 决策 | 裁决（2026-10-27） |
|---|------|-------------------|
| 1 | **缺陷 B 是否随本 change 一并消除？** | **按判据执行**：先判定「能否在**不**改框架层的前提下表达『首个取指数据已就绪』」⇒ **能** ⇒ 一并消除（退休数 −1，固件第一条指令只写回一次）；**不能** ⇒ 缺陷 B 继续「如实登记」，本 change 只修缺陷 A。⚠️ 无论哪个分支，MUST 在 change 里**实测**两后端的填充期序列并记录（决策门 2 的分歧点） |
| 2 | 若需改框架层：**改语义** 还是 **加 API**？ | **加 API**（增量、影响面小）⇒ 不改 `sread` 的现有初值语义，改为提供「读端口未就绪」的可观测判据；⚠️ MUST 复核 ADR-041 与 `mem-read-timing` capability |
| 3 | 是否新开 **ADR**？ | **要** ⇒ 新开 **ADR-048**（取指 skid buffer + 填充期语义）⇒ **架构依据先于 change** ⇒ 执行顺序：**architect 写 ADR-048 → planner 起草本 change 的 proposal → builder 实施** |
| 4 | 「固件 MUST 用 NOP 填充该位置（compile-time 静态护栏）」如何处置？ | ④ 落地后该护栏失效 ⇒ **删除**，或改为「允许非 NOP」的等价护栏 ⇒ **实施时 MUST 先定位该护栏**再决定；⚠️ 定位结果 MUST 写进 change 的 tasks |

| # | 判据 / 选项（裁决前的原始表述，保留供追溯） |
|---|------|
| 1 | 判据 = 「能否在**不**改框架层的前提下表达『首个取指数据已就绪』」⇒ 能 ⇒ 一并消除（退休数 −1）；不能 ⇒ 继续「如实登记」 |
| 2 | 改 `sread` 的初值语义 = 影响所有 `sread` 用户的填充期行为；加「未就绪」判据 API = 增量、影响面小 ⇒ **默认选加 API** |
| 3 | 结构性改动 + 可能动框架层 + **改主 spec 契约** ⇒ 与 ADR-046/047 同级 ⇒ **建议要** |
| 4 | ④ 落地后该护栏失效 ⇒ 删除，或改为「允许非 NOP」的等价护栏 |

## 5. Capabilities (涉及的能力维度)

- **`rv32i-commit-path`** —— **MODIFIED**（停顿契约去例外 + skid 契约 + 退休数推导）
- `mem-read-timing` —— **可能 MODIFIED**（仅当动 `sread` 语义）
- `rv32i-isa-semantics` —— **可能 MODIFIED**（固件覆盖扩大）
- `verilator-rv32i-e2e` —— **仅当** SoC 观测端口变化（⇒ 先改 ADR-046 D10）
- `rv32i-data-forwarding` —— 不涉及（前推与取指无关；但二者共同决定「固件还需不需要 NOP」）

## 6. Impact (影响面分析)

- **时序/金标全量重算**：IF/ID 与取指之间多一级 ⇒ 停顿拍数、冲刷拍数、写回序列、
  退休数**全部**变 ⇒ 预期影响 `examples/riscv-mini/tests/test_rv32i_soc_dual_parity.cpp`
  的 golden、`tests/test_rv32i_pipeline.cpp` 的用例、可能的 perf 基准指令数
- **计数**：`CTEST_TOTAL` **202 不变** / `EXAMPLES_TEST_SOURCES` **17 不变**
  （判别性固件进**既有** target ⇒ 无新测试文件）
- **风险**：**最高的一项**（plan T5 原话）—— ① 结构性改动（多一级缓冲 ⇒ 对齐风险）；
  ② 框架层 `sread` 语义影响所有用户；③ 改**主 spec** 契约（`rv32i-commit-path` 的例外）
- **建议执行顺序**（plan T5 的判断，本草案沿用）：
  ① **先做 skid buffer**（局部、不动框架层）⇒ ② 量化填充期两后端行为 ⇒
  ③ 再决定缺陷 B 是修还是登记 ⇒ ④ 框架层改动（若需要）单独评估 + 影响面清单回归
- **不做会怎样**：固件 MUST 继续在每个 load-use 停顿点之后放一条 NOP
  ⇒ 固件作者 MUST 知道「哪条会被丢」（当前靠「都是 NOP，丢了也无所谓」的巧合掩盖）
  ⇒ 且 `rv32i-commit-path` 主 spec 里那条**例外**长期挂着

## 7. 与既有文档的关系（实施时 MUST 同步）

| 文档 | 现写法 | 本 change 落地后 |
|---|---|---|
| `rv32i-commit-path` Scenario :259-268 | 「停顿 MUST 只延迟退休 —— ⚠️ 当前有一处例外」 | **去掉例外**（MODIFIED） |
| `rv32i-commit-path` Scenario :270-278 | 退休数 199 拍间隔 − 11 不增长 = **188** | ⚠️ **该数字自 2026-10-26 起就已与测试脱节**（测试断言 **179** = 199 − 20）⇒ 本 change 的 MODIFIED **先修基线**（188 → 179），再叠加 `+k`（缺陷 A）与 `−1`（缺陷 B）⇒ 最终值由**实测**回填 |
| `rv32i-commit-path` §已知边界 :33-36 | 「固件 MUST 用 NOP 填充该位置（已有 compile-time 静态护栏）」 | 改写；护栏同步处理（见决策门 4） |
| 根 `AGENTS.md §KNOWN LIMITATIONS` | 两条残留（停顿丢指令 / 填充期双装） | 消解或改写（若缺陷 B 保留 ⇒ 保留但补「两后端一致」的实测结论） |
| `plan` T5 | 「先做 skid buffer，双装单独评估」 | 沿用（本草案的决策门 1/2 即 T5 的「评估」环节） |
| `feat-rv32i-isa-completeness` ④ | 「取指 skid buffer + 取指填充期双装」 | 勾选 + 写实施结果 |
