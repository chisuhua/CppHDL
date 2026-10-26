---
name: rv32i-forwarding-decision-2026-10
priority: P0
source: ADR-046 §D5 悬置项的落地裁决（ADR-047，2026-10-27，2 轮 critic 审稿通过）
phase: phase-5
category: cpu
type: feature
created_at: 2026-10-27
---

# Improvement: RV32I 流水线 MUST 接数据前推（ADR-046 §D5 的落地裁决）

- **Date drafted**: 2026-10-27
- **Drafted by**: build-agent (planner role)
- **Status**: 📋 5-segment draft；change `rv32i-forwarding-decision-2026-10` 待实施
- **架构依据**: `docs/adr/ADR-047-rv32i-data-forwarding.md`（Accepted，**2 轮 critic 对抗审稿**
  ⇒ 第 1 轮 APPROVE_WITH_CONDITIONS（7 SHOULD_FIX + 3 NITS，**全部 CLOSED**）
  ⇒ 第 2 轮 **APPROVE**（3 NIT 已处置））
- **roadmap 归属**: `.rddf/roadmap/features/feat-rv32i-isa-completeness.md` ③
  （**roadmap 已把它登记为「需 ADR（行为变更）」⇒ ADR 已出 ⇒ 本草案解锁**）

## 1. Why (背景与动机)

### 缺口：lib 流水线**不接**数据前推，固件被迫手工插 NOP

`include/cpu/pipeline/hazard_unit.h:198`：

```cpp
io().rs1_data = io().rs1_data_raw;   // 直通，无前推
```

⇒ 根 `AGENTS.md §KNOWN LIMITATIONS` 登记了 **3 条**由此派生的限制：

| # | 限制（根 `AGENTS.md` 的原文要点） | 出处（行号） |
|---|---|---|
| ① | 相邻 ALU→ALU 依赖读到**旧值** ⇒ **固件 MUST 自行插 NOP/间隔** | `AGENTS.md:168`（「RV32I 流水线的『无数据前推』约束」） |
| ② | 距离 2 的 load→use（`lw` → `nop` → `add`）**检测不到** ⇒ 读**旧值** | `AGENTS.md:170`（「自保只覆盖『紧邻』（距离 1）的依赖」） |
| ③ | `JALR` 的 `rs1` **无数据前推** ⇒ 固件 MUST NOT 依赖「`rs1` 刚被前一条指令写」 | `AGENTS.md:178` + `rv32i-isa-semantics` spec 的明文条款 |

⚠️ 上述 3 条**都**含「ADR-046 **D5** 仍悬置」的措辞 ⇒ 本 change 落地后**同时**过时
⇒ 文档同步 MUST 覆盖这 **3** 处（不只 2 处）。

⚠️ 这 3 条限制**不是「5 级流水线的固有属性」，而是「没接前推」的实现后果**
⇒ 把它们留在「ISA 语义」层 = 把实现缺陷固化成规范。

### 为什么是**现在**（而不是继续挂着）

1. **ADR-046 §D5 已裁决**（2026-10-27）：`docs/adr/ADR-047-rv32i-data-forwarding.md`
   ⇒ D5 要求的「独立 ADR + **回归证据**」已满足 ⇒ 本草案的**悬置项已清零**；
2. **T5（取指 skid buffer）依赖本裁决**：roadmap 的 feat-rv32i-isa-completeness ④
   明确「前置：③（无前推时的停顿语义要先定）」⇒ 本 change 是 T5 的**强前置**；
3. ⚠️ **现有固件刻意无冒险** ⇒ 前推**接错 / 接反 / 没接，全仓照绿**
   ⇒ 「先接后补判别性固件」会**永久丢失**发现接错的机会
   ⇒ **判别性固件 MUST 与接线同批落地**（ADR-047 D9）⇒ 现在做是成本最低的时刻；
4. capability `rv32i-isa-semantics` **已存在**（T2 + T3 归档产物）⇒ 本 change 的
   delta **追加**到同一 capability（不新建 capability 骨架）。

### 事故史（决定「怎么接」）

`git show bf78367`（2026-04-15）—— 一次**组合环事故**：

```
ex_stage.alu_result → hazard.ex_alu_result → hazard.rs1_data(mux) → ex_stage.rs1_data → ex_stage.alu_result
```

全链组合、无寄存器节点 ⇒ 拓扑排序 `CHFATAL: Cycle detected in IR graph`。
该事故的处置是**移除 mux** ⇒ 今天的「不接前推」状态。

⇒ ADR-047 D3 定位了**断环的充分条件**：`src/core/context.cpp` 的拓扑排序对
`type_reg` **只遍历 `init_val`** ⇒ **寄存器是断路器** ⇒ 数据源取自
**流水线寄存器**（`ch_reg`）⇒ 环断开。

## 2. What Changes (变更范围)

### A. 前推接线（**零新增组件**）

**mux = 既有 `chlib::ForwardingMux<32>`**（`include/chlib/forwarding.h:184-243`）
—— ADR-047 D2 指定的**唯一**数据 mux（档位语义与当年被移出的 mux **逐档等价**：
0→REG / 1→EX / 2→MEM）⇒ **MUST NOT** 把 mux 放回 `HazardUnit`
（事故史 + `rv32i-canonical-home` spec 已禁止）。

**三个数据源**（全部取自**流水线寄存器** ⇒ 不成环）：

| 档位 | mux 端口 | 源 | 说明 |
|---|---|---|---|
| 0 = REG | `rs1_data_reg` / `rs2_data_reg` | `id_stage.io().rs1_data` / `rs2_data` | 寄存器堆**组合读**（`aread`）⇒ 与今天的直通值**同源** ⇒ 行为零变化 |
| 1 = EX/MEM | `ex_mem_alu_result` | `select(exmem_is_jump, exmem_link_value, exmem_alu_result)` | ⚠️ **MUST 含 `link_value`**（`JAL`/`JALR` 的写回值是 `pc + 4` 不是 ALU 结果）<br>⚠️ 三个源同受 `exmem_en` 守卫 ⇒ **对齐**（已实测：`rv32i_pipeline.h:315/318/343`） |
| 2 = MEM/WB | `mem_wb_result` | `wb_stage.io().write_data` | **已合并** load / jump / ALU 三路 ⇒ **免费覆盖「相隔两级的 load」** |

**档位 select MUST 打一拍**（ADR-047 D5）：pipeline 级加两个 `ch_reg`
（`fwd_a_d1` / `fwd_b_d1`）。

⇒ **根因**：`hazard` 的前推比较点是「**EX 级** rd vs **ID 级** rs」（那是 load-use
检测点），而 mux 喂的是 **EX 级**操作数 ⇒ 拍 T 的档位描述的是「T+1 消费者进 EX」，
mux 在拍 T 就用了 ⇒ **错一级** ⇒ 后果**不是「不前推」，而是把错误数据塞进当前
EX 指令** ⇒ 静默错值路径。

⇒ 打拍 ⇒ `hazard` 的组合逻辑**一字不改** ⇒ 既有真值表测试
（`tests/chlib/test_forwarding.cpp` + `examples/riscv-mini/tests/test_rv32i_pipeline.cpp`）
**因打拍而全部保留**。

### B. `HazardUnit` 的清理（ADR-047 D7 / D8）

| 动作 | 对象 | 理由 |
|---|---|---|
| **删 5 个预留输入端口** | `wb_reg_write` / `wb_rd_addr` / `ex_alu_result` / `mem_alu_result` / `wb_write_data` | 无内部消费者；`ForwardingMux` 只有 3 档 ⇒ WB 档位用不上；⚠️ `ex_alu_result` 已接线到 EX 级**组合输出** ⇒ 它是**环饵**（留着就诱使后人复现 `bf78367`）⇒ **删掉让 ADR 自执行** |
| **保留 4 个直通端口** | `rs1_data_raw` / `rs2_data_raw`（输入）+ `rs1_data` / `rs2_data`（输出） | `HazardUnit` 作为**独立组件**仍有**真实消费者**：`forwarding_dut_fixture.h:105-106` 喂前两个、`test_pipeline_exec.cpp:531/560/…` **读**后两个 ⇒ 删了会断既有断言 |
| ⚠️ **pipeline 装配内删 2 处驱动** | `rv32i_pipeline.h:484` / `:485`（`rs1_data_raw` / `rs2_data_raw`） | 接 mux 后 `hazard.rs1_data` 在 pipeline 内**无消费者** ⇒ 继续驱动就是「喂了没人读」⇒ ZERO-DEBT 违规 |
| **前推判定加门控** | `forward_ex_to_a` / `forward_ex_to_b` 加 `& !ex_is_load` | `ex_reg_write` **包含 load** ⇒ 保守形态下档位 1 不覆盖 load ⇒ 判定侧 MUST 排除 |
| **收敛双驱动** | 删 `rv32i_pipeline.h:200` / `:201`（`ex_stage.io().rs1_data` 的旧驱动） | `inputimpl::set_driver` 替换 `driver_` 但 `add_src` **累积** ⇒ IR 图残留 2 条 src 边 ⇒ **ZERO-DEBT / 拓扑排序 hygiene**（⚠️ **不是**「今天已双后端分叉」—— `codegen_verilog.cpp:613-625` 只取单数 `driver()`） |

### C. 测试（**判别性固件是必需项**，不是可选项）

⚠️ 现有固件**刻意无冒险** ⇒ 前推接错 / 接反 / 没接，**全仓照绿**
⇒ **判别性固件 MUST 与接线同批落地**（ADR-047 D9）。

**6 类判别性固件**：

1. 相邻 ALU→ALU，**无 NOP**（前推的核心收益）
2. 距离 2 的 load→use（覆盖**档位 2**）
3. `JALR` 的 `rs1` 紧邻被写（消除限制 ③）
4. 分支条件用**紧邻**新值
5. 双生产者同 rd（`hazard_unit.h:116-126` 注释里的陷阱形态）
6. **jump 生产者的前推**（覆盖**档位 2** 对 jump 生产者）
   ⇒ ⚠️ **具体距离 MUST 由实施期实测确定，MUST NOT 预先写死**

**变异验证**（每条都先确认编译成功 —— 编译失败的变异会跑旧二进制 ⇒ 假绿）：

- 去掉打拍 ⇒ 红（档位错一级）
- 交换档位 1 / 2 的数据源 ⇒ 红
- 前推判定漏 `& !ex_is_load` ⇒ 红
- ⚠️ 「档位 1 忘接 `link_value`」⇒ **不得**用固件验收
  （`JAL`/`JALR` 是**无条件**跳 ⇒ 距离-1 消费者必在错误路径上 ⇒ 该失效模式
  **固件级不可达**）⇒ **MUST** 用**定向接线级单测**
  （直接驱动 `exmem_is_jump = 1` + 档位 1，断言 mux 输出 == `link_value`）

**负向对照**（实施期第一屏）：先跑 **`bf78367` 形态**的最小复现
⇒ 确认环检测**仍然 FATAL**（证明护栏有效），再接新形态。

### D. 规格与文档同步（ADR-047 D10）

**4 个 spec 走 MODIFIED delta**（MUST NOT 直接改主 spec）：

| spec | 过时之处 |
|---|---|
| `rv32i-isa-semantics` | 「`JALR` 的 `rs1` MUST 取 EX 级的寄存器值（**MUST NOT 依赖数据前推** —— 前推未裁决）」⇒ 必须改；另**新增**「数据前推 MUST 为消费者提供最新值」的 ISA 级契约 |
| `rv32i-commit-path` | ① 「停顿 MUST NOT 是单拍脉冲式」的「**无数据前推**」前提；② 「load-use 自保 MUST 只覆盖**紧邻**（距离 1）」的「读**旧值** ⇒ 固件 MUST 自行保证间隔」 |
| `verilator-rv32i-e2e` | trace parity fixture 里「NOP 是为『**无数据前推**』留的间隔」⇒ 间隔**不再是必需** ⇒ 且 MUST NOT 把 NOP 间隔当作前推正确性的证据 |
| `rv32i-canonical-home` | 「前推数据 mux MUST 归属独立的 mux 组件」⇒ 从「**MUST** 归属」变为「**已**归属并**已接入**」⇒ 登记新的验收证据 |

**文档同步**（不在 spec 层，但同样 MUST 做）：

- 根 `AGENTS.md §KNOWN LIMITATIONS`：「无数据前推」约束（:168）、
  「距离 2 检测不到」条目（:170）、「`JALR` 的 `rs1` 无数据前推」条目（:178）
  ⇒ **3 条**都会过时
- `include/cpu/AGENTS.md`：「`HazardUnit` 的『预留输入』端口无内部消费者」条目
  ⇒ D7 删端口 ⇒ 该条目消失

## 3. Acceptance (验收标准)

### 功能性

- [ ] 相邻 ALU→ALU（**无 NOP**）的消费者 MUST 拿到**新值**
- [ ] 距离 2 的 load→use 的消费者 MUST 拿到**新值**（档位 2 覆盖 load）
- [ ] `JALR` 的 `rs1` 紧邻被写时 MUST 拿到**新值**（限制 ③ 消除）
- [ ] 分支条件用**紧邻**新值时 MUST 走**正确**的分支
- [ ] jump 生产者（`JAL`/`JALR`）的写回值 MUST 被**档位 2** 正确前推
      （`wb_stage.write_data` 已含 `link_value`）
- [ ] 双生产者同 rd 的形态 MUST NOT 前推**错误的那一个**的值

### 结构性（ADR-047 的机制约束）

- [ ] 数据源 MUST 全部取自**流水线寄存器** ⇒ 拓扑排序 MUST NOT 报组合环
- [ ] 档位 select MUST **打一拍** ⇒ 拍 T 的档位 MUST 在 T+1 生效
- [ ] `HazardUnit` 的 5 个预留输入端口 MUST 已删除
- [ ] `HazardUnit` 的 4 个直通端口 MUST 保留（独立组件仍可用）
- [ ] pipeline 装配内 MUST NOT 有「喂了没人读」的驱动
- [ ] `ex_stage.io().rs1_data` / `rs2_data` MUST 只有**单一**驱动
- [ ] `ex_is_load` MUST 被**全部**实例化点显式驱动
      （⚠️ 现状：**全仓 6 个独立实例化点全部未驱动** ⇒ 读到**未定义值**）
- [ ] 负向对照：`bf78367` 形态的最小复现 MUST **仍然** `CHFATAL`（环检测护栏有效）

### 可判别性（**零信号断言是不合格**）

- [ ] 6 类判别性固件 MUST 全部存在，且**每类都有会转红的断言**
- [ ] 变异验证 ≥ 4 条**全被抓**，且每条都**先确认编译成功**
- [ ] 「档位 1 忘接 `link_value`」MUST 由**定向接线级单测**验收（不得靠固件）
- [ ] 双后端（interpreter + verilator）**逐拍全等**（无掩码）

### 回归与工程

- [ ] `ctest -LE perf` 零回归
- [ ] 全部护栏脚本 exit 0（含 `verify_example_assertion_quality.sh` —— 它钉住断言计数）
- [ ] 既有 golden 的变化 MUST **全部可归因**（新增用例导致的变化 vs 语义变化）
- [ ] `openspec validate --specs --strict` 通过（archive 前）

## 4. Capabilities (涉及的能力维度)

- **`rv32i-isa-semantics`（已存在）** —— MODIFIED ×1（`JAL`/`JALR` 的 `rs1` 条款）
  + ADDED ×1（「数据前推 MUST 为消费者提供最新值」）
- **`rv32i-commit-path`（已存在）** —— MODIFIED ×2（load-use 停顿的「无数据前推」前提
  + 「距离 1」自保限制）
- **`verilator-rv32i-e2e`（已存在）** —— MODIFIED ×1（trace parity fixture 的 NOP 间隔理由）
- **`rv32i-canonical-home`（已存在）** —— MODIFIED ×1（mux 归属 + 「已接入」的验收证据）

⇒ **不新建 capability**（4 个既有 capability 已覆盖全部契约面）⇒ delta **追加**。

## 5. Impact (影响面分析)

### 改动文件（ADR-047 §5.1）

| 文件 | 改动 |
|---|---|
| `include/cpu/pipeline/rv32i_pipeline.h` | 加 `ForwardingMux` 实例 + `fwd_a_d1`/`fwd_b_d1` 两个 `ch_reg` + 3 个前推源接线；**删** `:200`/`:201` 旧驱动（双驱动收敛）+ `:484`/`:485` 的 `rs1_data_raw`/`rs2_data_raw` 驱动（喂了没人读）；**删** 5 个预留端口的驱动 |
| `include/cpu/pipeline/hazard_unit.h` | **删** 5 个预留输入端口；前推判定加 `& !ex_is_load` |
| `examples/riscv-mini/tests/forwarding_dut_fixture.h` | 补 `ex_is_load` 驱动；删 5 处预留端口驱动 |
| `examples/riscv-mini/tests/test_pipeline_debug.cpp` | 同上 |
| `examples/riscv-mini/tests/test_wrapper_simple.cpp` | 同上 |
| `examples/riscv-mini/tests/test_pipeline_exec.cpp` | 删 **8 区块 × 5 端口 = 40 处** `set_input_value`；补 `ex_is_load` 驱动 |
| `examples/riscv-mini/tests/test_rv32i_pipeline.cpp` | 删 **4 区块 × 5 端口 = 20 处** `set_input_value`；补 `ex_is_load` 驱动 |
| `examples/riscv-mini/tests/test_rv32i_soc_dual_parity.cpp` | 6 类判别性固件 + golden 全量重算 |
| 根 `AGENTS.md` / `include/cpu/AGENTS.md` | 3 条过时限制 + 「预留输入」条目 |

### 爆炸半径（**实测**，ADR-047 §5.3）

- `HazardUnit` 共 **7 个实例化点**（分布 **6 个文件**）⇒ **7 个全部驱动**了那 5 个
  预留端口 ⇒ **删端口 ⇒ 7 个文件全部编译失败** ⇒ 爆炸半径**封闭可枚举**
- ⚠️ `forwarding_dut_fixture.h:90` 写的是 `ch::ch_module<riscv::HazardUnit>`
  （**带 namespace 前缀**）⇒ **不带前缀的 grep 会漏掉它**
  ⇒ 本 ADR 曾连续两轮误数 ⇒ **实施期 MUST 按 7 个实例逐个核对**
- 根 `tests/test_rv32i_pipeline.cpp` 实例化的是 `riscv::ForwardingUnit`（3 处）
  / `ControlHazardUnit`（1 处）⇒ **不受**影响
- 仓外无契约面

### API 变更

- ⚠️ **破坏性**：`HazardUnit` 删除 5 个**公开输入端口** ⇒ 公共 API 面变化
- ⚠️ `HazardUnit` 的**前推判定语义**变化（加 `& !ex_is_load`）⇒
  独立消费者（`test_pipeline_exec.cpp` / `examples/.../test_rv32i_pipeline.cpp`）的
  既有断言**可能需要复核**（它们当前读到的 `ex_is_load` 是**未定义值**）

### 风险：**中-高**（改的是数据通路的时序语义）

| 缓解 | 说明 |
|---|---|
| 负向对照先行 | 第一屏跑 `bf78367` 形态 ⇒ 确认环检测仍 FATAL |
| 判别性固件**同批**落地 | ⚠️ 不允许「先接后补」⇒ 现有固件无冒险 ⇒ 接错会**永久**失去发现机会 |
| 变异验证 4+ 条 | 每条先确认编译成功（否则跑旧二进制 ⇒ 假绿） |
| 双后端逐拍 parity | 无掩码 |
| 打拍**不改正 hazard 语义** | ⇒ 既有真值表测试全部保留 |
| 保守边界 | 档位 1 **不**覆盖 load ⇒ load-use 停顿**行为不变** ⇒ 停顿契约零改动 |

### 明确非目标

- **load-use 停顿仍在**（3 拍 + 「丢弃消费者之后的第一条指令」）
  ⇒ 取指 skid buffer = **T5 `rv32i-fetch-skid-buffer-2026-10`**（本 change 的下游）
- **档位 1 覆盖 load**（V2）⇒ 独立立项
- **`riscv::ForwardingUnit` 与 `chlib::ForwardingMux` 的合并** ⇒ ADR-038 §2.3 留未来
  （⚠️ `riscv::ForwardingUnit` 的 `ex_alu_result` 输入 = 「活的 EX 输出」⇒ **环饵**）
- **分支预测**（`rv32i_branch_predict.h` 未接入流水线）
- **框架层改动**（`operator>>>` 仍是注释状态；`ch_mem` 语义）
