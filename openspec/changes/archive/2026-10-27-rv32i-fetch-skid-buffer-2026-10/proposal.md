# 提案：RV32I 取指 MUST 有 skid buffer（停顿不丢指令）+ 填充期 MUST NOT 双装

- **架构依据**：`docs/adr/ADR-048-rv32i-fetch-skid-buffer.md`（**Accepted** 2026-10-27；
  **2 轮 critic 对抗审稿** ⇒ 第 1 轮 REJECT（1 BLOCKER + 3 SF + 2 NIT）⇒ 整改 ⇒ 第 2 轮 **APPROVE**）
- **5 段草案**：`.rddf/improvements/rv32i-fetch-skid-buffer-2026-10.md`（P0；4 项决策门已由用户裁决）
- **roadmap 归属**：`feat-rv32i-isa-completeness` ④（ISA 语义边界收口的**最后一项**）
- **规格影响**：**有** spec 变更（`rv32i-commit-path` 的 **2** 个 Requirement MODIFIED）
  ⇒ 归档时 **禁** `--skip-specs`（`.openspec.yaml` 未声明 `skip_specs`）
- **前置**：ADR-048（已 Accepted）⇒ 本 change 是它的**实施**；③ 数据前推（ADR-047）已落地
- **规模 / 风险**：**高**（ADR-048 D13：结构性改动 + 时序金标全量重算）
  ⇒ **一个 change、两个 commit**（C1 缺陷 A / C2 缺陷 B；ADR-048 D13.1）⇒ 各自可回退

## Why

### 缺陷 A：load-use 停顿会**丢弃「消费者之后的第一条指令」**

imem 是 **1-cycle `sread`**（ADR-041 强制）⇒ 取指地址在第 t 拍提交、第 t+1 拍数据返回。
检测拍 T（EX 级是 load、ID 级是 W 的消费者）中，`pc_reg` 指向 `W+2`，而 `W+1` 的地址
在 **T−1** 拍已提交 ⇒ `W+1` 的数据**恰好在 T 拍返回** —— 正是 `if_id_update_en` 变 0 的
第一拍（`if_stage.h:192-200`）⇒ **无处可存** ⇒ 停顿解除后 IF/ID 锁存 `W+2`
⇒ **`W+1` 永久消失**（其地址不会被再次提交）。

**实测证据**：`openspec/changes/archive/2026-10-26-rv32i-pipeline-commit-path-2026-10/design.md`
§3.10（第 17 轮 critic 实测）—— ID 级序列在停顿 #1 处是 word 21（保持）→ **word 23**
（word 22 从未进 ID）；停顿 #2 处 word 44 → **word 46**。
⚠️ **两处被丢的都是 NOP** ⇒ 全部 golden 仍绿 ⇒ 典型的「**绿 ≠ 对**」。

**当前边界（如实登记）**：固件 MUST 保证「停顿点之后那条指令是 NOP」——
该例外**已写进主 spec**（`rv32i-commit-path` 的「停顿 MUST 只延迟退休」Scenario）
⇒ 本 change 消除它。

### 缺陷 B：取指填充期「同一条指令被装两次」

固件第一条指令被**写回两次**（连续两拍 `wb_write_en=1`，`rd`/`data` 相同）⇒ 退休数 **+1**
⇒ 测试的退休数金标**含**这 +1。

⚠️ **严重性已被 ADR-047 升级**：前推落地后，第二条会**读到第一条的前推值**
⇒ 若该指令**自依赖**（`rd == rs1`）⇒ **不再幂等** ⇒ **正确性**地雷。
现有防线是测试里的 compile-time 护栏 `fwd_first_instruction_not_self_dependent`
⇒ **固件级巧合在挡正确性缺陷** ⇒ 这是「缺陷 B 必须一并修」的**决定性理由**。

**根因（ADR-048 §1.3.3 已纠正既有文档的措辞）**：**不是**「建图期组合求值」——
`Simulator` 只把 proxy 槽位**分配为 0**；真正的预填是 **`load_firmware` 的加载拍**
（= 固件 word 数，每 word 一拍；`ext_rst=1` 下 sync 读端口照常工作
⇒ 解释器无 reset 意识、Verilog 的 reset 分支只对框架 `default_reset` 生效）
⇒ **两后端对称预填** ⇒ 幻影装载与下一拍的**真实取指**是**同一地址同一数据**。

### 为什么现在

- ③（数据前推，ADR-047）落地 ⇒ 固件**不再需要**为依赖间隔插 NOP ⇒ 残留的 NOP
  **只剩**「停顿点之后那一条」⇒ 缺陷 A 是它的唯一解法；
- 缺陷 B 在 ③ 之后从「计数偏差」变成「正确性地雷」⇒ 不能继续挂着；
- ①②③ 都已重算过金标 ⇒ ④ 的时序重算有**稳定基线**（179/20）而不是在混沌中重算。

## What Changes

### C1（commit 1）：取指 skid buffer —— 修缺陷 A

**文件**：`include/cpu/pipeline/stages/if_stage.h`

1. **新增 1 级 skid buffer**：`skid_instr` / `skid_pc` / `skid_valid`
   - **填充**：`stall && instr_ready && !skid_valid && !flush && !flush_d1 && !rst`
     ⇒ `skid_instr ← fetched_instr`、**`skid_pc ← fetch_pc_reg`**（**不是** `pc_reg`！
     否则指令与 pc 错 4 字节 ⇒ 与 `flush_d1` 缺陷同型）
   - **排空**：`!rst && !stall && skid_valid && !flush && !flush_d1`
     ⇒ **必须与 IF/ID 的实际装载同拍原子发生**（`if_id_update_en` 为真时）
   - **排空拍丢弃流上数据**（语义 MUST 写死）：该拍流上到达的数据被丢弃，但
     `pc_reg` 仍指向它 ⇒ 下一拍会被**重读** ⇒ 安全
   - **IF/ID 取数源**：`skid_valid ? skid : 流上数据`（pc 同理）
2. **flush 时清空 `skid_valid`**（含 `flush_d1`）—— 防御性契约
   （当前流水线级**不可达**：skid 非空 ⇒ 停顿窗口 ⇒ EX 是气泡 ⇒ 无分支 ⇒ 无 flush）
3. **⚠️ 不做 PC 回退**（ADR-048 D3）⇒ PC 选择器仍是两路 ⇒
   `if_stage.h:84-103` 的「flush 唯一来源 ⇒ `pc_update_en` 不看 flush」不变量**原样保留**
4. **顺带 ZERO-DEBT**：删掉 `if_stage.h:105-112` 与 `:113-119` 的**逐字重复**注释
   （复制粘贴事故）

### C2（commit 2）：`primed` 门控 —— 修缺陷 B

1. `IfStage` 内部新增 `primed` 标志（复位后首拍置 1）
2. IF/ID 的装载 MUST 以「`primed` 已置位」为前提
   - ⚠️ 判据 MUST 内生于 `IfStage`：`instr_ready` **恒为 1**（`rv32i_tcm.h:58`）
     ⇒ **不能**由它导出「已就绪」
   - **默认只门控 `valid`**（不门控另外两个装载）⇒ 幻影装载的 pc 是 **0x80000000**
     （与真值相同）⇒ `dbg_id_pc` 的观测序列**零变化**；若实施改选「门控全部三个装载」
     ⇒ MUST 在 change 里记录并改写 D9.3 的 M6 变异

### 测试

| 用例 | 宿主（**既有** target ⇒ 计数不变） | 内容 |
|---|---|---|
| **判别性固件** | `examples/riscv-mini/tests/test_rv32i_soc_dual_parity.cpp` | 停顿点之后放**非 NOP** ⇒ 未修前该指令**消失** ⇒ 修后必须出现；带反向期望 + 手工推导留痕 |
| **组件级负向对照** | `tests/test_rv32i_pipeline_behavior.cpp` | `ch_device<IfStage>` 同拍驱动 `stall` + `flush` ⇒ 断言「flush 时 skid 被清空 / 不被填充」⇒ **SoC 级变异抓不到**（不可达）⇒ 组件级是唯一判别力来源 |
| 退休数金标 | 同判别性固件 | 从 **179 / 20** 起算重算（⚠️ 不是 spec 里的 188）⇒ 钉住 `non_retired` 的**分段不变式**（fill / stall / flush） |

⚠️ **MUST NOT** 改 `tests/test_rv32i_pipeline.cpp`（既有 interpreter-only baseline）。

### 既有护栏与掩码的处置（ADR-048 D11）

| 对象 | 位置 | 处置 |
|---|---|---|
| `stall_drop_slots_are_nop` | `test_rv32i_soc_dual_parity.cpp:573-592` | 判别性固件要**故意**放非 NOP ⇒ 该 `static_assert` 会**编译失败** ⇒ **删除或反转**（反转为「该位置 MUST **不是** NOP 且必须退休」）⇒ 与判别性固件**同 commit** |
| `fwd_stall_drop_slots_are_nop` | 同上 `:1134-1147` | 同上 |
| `fwd_first_instruction_not_self_dependent` | 同上 `:1154-1166` | C2 修缺陷 B ⇒ **删除**（若最终不修 ⇒ MUST 升格为 spec 义务） |
| 「连续重复去重」掩码 | 同上 `:941-945` | 为缺陷 B 设计 ⇒ C2 **同 commit** MUST **移除**，否则它转而掩盖**真实**的双写回回归 |

### 规格与簿记

- **spec delta**：`openspec/changes/rv32i-fetch-skid-buffer-2026-10/specs/rv32i-commit-path/spec.md`
  —— **2** 个 Requirement MODIFIED：
  ① 「「退休指令数」MUST 由流水线的**有效链**定义」⇒ 新增 Scenario
  「取指填充期 MUST NOT 把同一条指令装进流水线两次」；
  ② 「停顿 / 冲刷期间流水线的**有效链** MUST 保持自洽」⇒ 追加条款
  「IF/ID MUST NOT 因停顿而丢弃已到达的指令」+ **消除**停顿丢指令的**例外**
  + 退休数 NOTE 的 **188 → 179** 基线订正
  ⇒ ⚠️ 既有 Scenario **一条不删**、标题**逐字保留**（消歧只写正文）
- ⚠️ **archive 后手工订正主 spec 的 `## Purpose`** 段（`:33-36` 的「⚠️ 已知边界」）：
  delta 的 `## Purpose` **不会**被 CLI 合并进**既有** capability ⇒ MUST 手工改
  （「PC 回退」表述已否决、根因表述已纠正 ⇒ 两条都要改）
- **文档**：根 `AGENTS.md §KNOWN LIMITATIONS` 两条残留消解 + §NOTES 记录；
  `include/cpu/AGENTS.md` 的 CONVENTIONS 增补取指侧契约
- **簿记**：`feat-rv32i-isa-completeness` ④ 勾选 + `roadmap.md` + 主题文档
  `docs/architecture/rv32i-microarchitecture.md` 的「已知残留」节同步

## Acceptance

### 功能性

- [ ] 判别性固件：停顿点之后的**非 NOP** 指令**不消失**（写回序列里出现它）
- [ ] 退休数 == 实际指令数（缺陷 B 消除 ⇒ 相对 **179** 的口径 −1；最终数字由**实测**回填）
- [ ] `non_retired` 的**分段**：停顿拍数与冲刷拍数**不变**（仍 3 / 3），只有 fill 段变
- [ ] 组件级：同拍 `stall` + `flush` ⇒ skid 被清空、不被填充

### 可判别性（零信号断言不合格）

- [ ] 每个用例带**反向期望**（`X_old != X_new`）+ 期望值**手工推导** + 注释留痕
- [ ] **变异验证**（每条先确认**变异后仍编译成功**，否则跑的是旧二进制 ⇒ 假绿）：
      M1 填充时锁 `pc_reg` 而非 `fetch_pc_reg` / M2 关掉排空 / M3 排空条件写反 /
      M4 排空不与 IF/ID 装载同拍原子 / M5 去掉 `primed` 门控 /
      M6 `primed` 门控全部三个装载（⚠️ 以默认方案为前提）
- [ ] 组件级 MUST 抓到「删掉 flush 清空 skid」的变异（SoC 级抓不到 ⇒ 组件级是唯一来源）

### 回归与工程

- [ ] `ctest -LE perf` 零回归（191/191）；`ctest -L perf` 全绿
- [ ] 双后端逐拍全等（无掩码）+ `run_all_ported_tests.sh` 28/28 + `run_all_samples.sh` 35/35
- [ ] `CTEST_TOTAL` **202 不变** / `EXAMPLES_TEST_SOURCES` **17 不变** / `ctest -L verilator_e2e` **37 不变**
      （用例进**既有** target）
- [ ] 7 个 `scripts/verify_*.sh` 全 exit 0 + `verify_test_counts.sh` 绿
- [ ] `openspec validate --specs --strict` 不因本 change **新增**红
      （⚠️ CLI 1.14.3 比仓库钉的 1.4.1 多两条 warning 规则 ⇒ 以「本 change 前后无新增」为准）

## Capabilities

- **`rv32i-commit-path`** —— **MODIFIED ×2**（见上）：停顿契约去例外 + 取指填充期不双装
  + 退休数基线订正
- `mem-read-timing` —— **不涉及**（ADR-048 D7 的框架层加 API 仅在局部修法不可行时启动
  ⇒ 本 change **不**动 `sread` 语义 ⇒ `aread`/`sread` 契约零改动）
- `verilator-rv32i-e2e` —— **不涉及**（skid **不加任何顶层端口**；ADR-046 D10 冻结 32 端口
  ⇒ ADR-048 D12）
- `rv32i-data-forwarding` —— **不涉及**（前推与取指无关）

## Impact

- **计数**：`CTEST_TOTAL` 202 / `EXAMPLES_TEST_SOURCES` 17 / `ctest -L verilator_e2e` 37
  —— **三者均不变**
- **文件面**：`include/cpu/pipeline/stages/if_stage.h`、
  `examples/riscv-mini/tests/test_rv32i_soc_dual_parity.cpp`、
  `tests/test_rv32i_pipeline_behavior.cpp`；**不动** `tests/test_rv32i_pipeline.cpp`
- **时序金标全量重算**：退休数、写回序列、`dbg_id_*` 逐拍序列 ⇒ 观测面 golden 重推
  ⇒ ⚠️ 变化来源是「**被救回的指令真实退休**」+「填充期重复退休消失」+ 200 拍窗口的
  尾部截断 ⇒ **不是**新增一级延迟（skid 是旁路缓冲）
- **风险**：**高** —— ① 结构性改动（新增 1 级缓冲）⇒ 对齐风险；
  ② 金标全量重算（手工推导，工作量大）；③ 主 spec 契约变更（去例外）
- **可回退**：两个 commit 粒度 ⇒ C2 若引发意外观测面扰动，可只回退 C2

## Non-Goals

- **不做 PC 回退**（ADR-048 D3：两种误实现形态均会引入新缺陷）
- **不动框架层**（`ch_mem::sread` 语义、读端口 API）⇒ ADR-048 D7 不启动
- **不加顶层观测端口**（ADR-046 D10 冻结 ⇒ 需先改 D10）
- 不改 `aread` 语义、不把 imem 换成 `aread`（ADR-041）
- 不做「档位 1 覆盖 load」（V2，独立立项）
- 不做预测取指 / 取指队列（会改变 ADR-048 D3/D4 的前提 ⇒ 将来引入 MUST 重审）
