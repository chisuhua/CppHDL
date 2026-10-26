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
- ⚠️ **起草后期的缺口分析（metis）找出 ADR-047 的 3 处缺陷** ⇒ 见 §6
  ⇒ **实施 MUST 等 architect 修订 ADR-047**

## 1. Why (背景与动机)

### 缺口 A：lib 流水线**不接**数据前推 ⇒ 3 条限制被固化成「ISA 语义」

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

⚠️ 这 3 条**不是「5 级流水线的固有属性」，而是「没接前推」的实现后果**
⇒ 把它们留在「ISA 语义」层 = 把实现缺陷固化成规范。

### 缺口 B：spec 层挂着**未决决策**的指针

`rv32i-isa-semantics` 与 `rv32i-commit-path` 里各有一处引用「**前推未裁决**
—— ADR-046 D5」⇒ spec 引用 suspended decision 是一种 **spec 债**
⇒ ADR-047 D10 只列了「冲突点」，没意识到「指针闭环」也是要消除的对象。

### 缺口 C：根 `AGENTS.md` 的相关条目已叠了 **4 层**修订

（「紧邻能自保」/「距离 2 检测不到」/「仍无前推」/「丢指令约束并存」）
⇒ 可维护性已崩 ⇒ 本 change 是把它**重写**成干净**距离矩阵**的机会
⇒ 而 D10 只列了「2 条会过时」，**低估**了这个收益。

### 缺口 D（起草期新发现）：**距离 3** 的同沿竞态 —— **今天就是错的**，且**从未被文档化**

- `idex_rs1_data` **锁存**了 regfile 的组合读（`id_stage.h:255`）
  ⇒ ID→EX 锁存沿与 WB 写回沿**同沿** ⇒ 采样到**写回前**的值；
- regfile 读点（`rv32i_pipeline.h:172`）**无 WB 旁路**；
- ⇒ 距离 3（`lw` → `nop` → `nop` → 消费者）读**旧值**
  ⇒ 现有固件用 **3 条 NOP（距离 4）恰好躲开** ⇒ 从未被记录。

⇒ 这是**前推之外**的独立缺陷，但它决定了「固件到底还需不需要插 NOP」的答案
⇒ 必须与前推**一起**收口（见 §2.B 与 §6 第 1 条）。

### 为什么是**现在**

1. **ADR-046 §D5 已裁决**（2026-10-27）⇒ D5 要求的「独立 ADR + **回归证据**」
   已满足 ⇒ 悬置项清零；
2. **T5（取指 skid buffer）依赖本裁决**（roadmap feat ④ 明确「前置：③」）
   ⇒ 本 change 是 T5 的**强前置**；
3. ⚠️ **现有固件刻意无冒险** ⇒ 前推**接错 / 接反 / 没接，全仓照绿**
   ⇒ **判别性固件 MUST 与接线同批落地**；
4. capability `rv32i-isa-semantics` **已存在** ⇒ delta **追加**，不新建骨架。

### 事故史（决定「怎么接」）

`git show bf78367`（2026-04-15）—— 一次**组合环事故**：

```
ex_stage.alu_result → hazard.ex_alu_result → hazard.rs1_data(mux) → ex_stage.rs1_data → ex_stage.alu_result
```

全链组合、无寄存器节点 ⇒ 拓扑排序 `FATAL: Cycle detected in IR graph`。
该事故的处置是**移除 mux** ⇒ 今天的「不接前推」状态。

⇒ ADR-047 D3 定位了**断环的充分条件**：`src/core/context.cpp` 的拓扑排序对
`type_reg` **只遍历 `init_val`** ⇒ **寄存器是断路器** ⇒ 数据源取自
**流水线寄存器** ⇒ 环断开。

## 2. What Changes (变更范围)

### A. 前推接线（**零新增组件**）

**mux = 既有 `chlib::ForwardingMux<32>`**（`include/chlib/forwarding.h:184-243`）
—— ADR-047 D2 指定的**唯一**数据 mux（档位语义与当年被移出的 mux **逐档等价**）
⇒ **MUST NOT** 把 mux 放回 `HazardUnit`（事故史 + spec 已禁止）。

| 档位 | mux 端口 | 源 | 备注 |
|---|---|---|---|
| 0 = REG | `rs1_data_reg` / `rs2_data_reg` | `id_stage.io().rs1_data` / `rs2_data` | ⚠️ 是**锁存值** ⇒ 见 §B 的「距离 3」 |
| 1 = EX/MEM | `ex_mem_alu_result` | `select(exmem_is_jump, exmem_link_value, exmem_alu_result)` | ⚠️ **MUST 含 `link_value`**（防御性）<br>⚠️ 三个源同受 `exmem_en` 守卫 ⇒ **对齐**（`rv32i_pipeline.h:315`/`:318`/`:343`） |
| 2 = MEM/WB | `mem_wb_result` | `wb_stage.io().write_data` | **已合并** load / jump / ALU 三路 |

**档位 select MUST 打一拍**（ADR-047 D5）：pipeline 级加两个 `ch_reg`
（`fwd_a_d1` / `fwd_b_d1`）。

⇒ **根因**：`hazard` 的比较点是「**EX 级** rd vs **ID 级** rs」（load-use 检测点），
mux 喂的却是 **EX 级**操作数 ⇒ 拍 T 的档位描述的是「T+1 消费者进 EX」，
mux 在拍 T 就用了 ⇒ **错一级** ⇒ 后果**不是「不前推」，而是把错误数据塞进当前
EX 指令** ⇒ 静默错值路径。

⇒ 打拍 ⇒ `hazard` 的组合逻辑**一字不改** ⇒ 既有真值表测试**因打拍而全部保留**。

### B. ⚠️ **距离矩阵**（本 change 的核心契约 ⇒ 单一权威 = 新 capability）

| 距离 | 取值来源 | 状态 |
|---|---|---|
| 1 · ALU→ALU | 档位 `1`（EX/MEM 级） | ✅ |
| 1 · load→use | **load-use 停顿**（3 拍） | ⚠️ 停顿**保守地**不覆盖 load ⇒ 停顿仍在 |
| 2 | 档位 `2`（MEM/WB 级合并写回值） | ✅ |
| **3** | **寄存器堆路径 + 读点旁路** | ⚠️⚠️ **同沿竞态 ⇒ MUST 加旁路**（§6 第 1 条） |
| ≥ 4 | 寄存器堆路径 | ✅ |

⇒ **距离 3** 不经前推档位（生产者此时已离开可命中的级）
⇒ MUST 在 regfile 读点加 **WB→ID 组合旁路**（约 10 行；数据叶是 `memwb_*`
寄存器 ⇒ **不成环**；**不需要** hazard 的 `wb_*` 端口 ⇒ 与 D7 正交）。

### C. `HazardUnit` 的清理（ADR-047 D7 / D8）

| 动作 | 对象 | 理由 |
|---|---|---|
| **删 5 个预留输入端口** | `wb_reg_write` / `wb_rd_addr` / `ex_alu_result` / `mem_alu_result` / `wb_write_data` | 无内部消费者；⚠️ `ex_alu_result` 已接线到 EX 级**组合输出** ⇒ 它是**环饵** ⇒ **删掉让 ADR 自执行** |
| **保留 4 个直通端口** | `rs1_data_raw` / `rs2_data_raw` + `rs1_data` / `rs2_data` | `HazardUnit` 作为**独立组件**仍有**真实消费者**（`forwarding_dut_fixture.h:105-106` 喂；`test_pipeline_exec.cpp:531`/`:560` **读**） |
| ⚠️ **pipeline 内的 `:484`/`:485` 驱动** | `rs1_data_raw` / `rs2_data_raw` | **保留**（与仓库反模式「输入端口 MUST 全部显式驱动」相撞 ⇒ **正确性优先于整洁**；驱动来源合法且免费）⇒ 但 MUST 在注释里**如实说明**消费者已迁到 mux |
| **前推判定加门控** | `forward_ex_to_a` / `forward_ex_to_b` 加 `& !ex_is_load` | `ex_reg_write` **包含 load** ⇒ 保守形态下档位 1 不覆盖 load |
| **收敛双驱动** | 删 `rv32i_pipeline.h:200` / `:201` | `set_driver` 替换 `driver_` 但 `add_src` **累积** ⇒ IR 图残留 2 条 src 边 ⇒ **ZERO-DEBT / 拓扑排序 hygiene**（⚠️ **不是**「今天已双后端分叉」—— `codegen_verilog.cpp:613-625` 只取单数 `driver()`） |

### D. 测试（**判别性固件是必需项**）

⚠️ 现有固件**刻意无冒险** ⇒ 前推接错 / 接反 / 没接，**全仓照绿**
⇒ **判别性固件 MUST 与接线同批落地**。

**8 类判别性固件**：① 相邻 ALU→ALU（无 NOP）② 距离 2 的 load→use
③ **距离 3** 的 load→use ④ `JALR` 的 `rs1` 紧邻被写 ⑤ 分支条件用紧邻新值
⑥ **store 的写数据（rs2）** 紧邻被写 ⑦ 双生产者同 rd ⑧ `x0` 负向用例

⚠️ **「jump 的写回值」NOT 在此列** —— 跳转的返回地址**不经前推档位**到达消费者
⇒ 用它当判别性证据是**零信号** ⇒ 只作**回归**用例（§6 第 3 条）。

**变异验证**（每条都先确认**编译成功** —— 编译失败的变异会跑旧二进制 ⇒ **假绿**）：

| 变异 | 验收形态 |
|---|---|
| 去掉打拍 | **固件级**可判别 |
| 交换档位 1 / 2 的数据源 | **固件级**可判别 |
| 前推判定漏 `& !ex_is_load` | ⚠️ **固件级不可达** ⇒ **组件级真值表** |
| 档位 1 忘接 `link_value` | ⚠️ **固件级不可达** ⇒ **组件级单测**（§6 第 2 条） |

**负向对照**：`bf78367` 形态的最小复现 MUST **仍然** `CHFATAL`，
且 MUST **固化为常驻测试**。

**金标纪律**：所有重算的期望值 MUST **手工推导** + **代码注释留痕**
（MUST NOT 从 sim 输出抄值）；每个判别性用例 MUST 带**反向期望**
（「未接前推时读到 `X_old`」且 `X_old != X_new`）⇒ 否则该用例**可能已退化**。

### E. 规格与文档同步

**5 个 capability 走 delta**：

| capability | 动作 |
|---|---|
| **`rv32i-data-forwarding`（新建）** | ADDED ×4：数据前推 MUST 提供最新值（含**距离矩阵**）· 档位/数据源/时间对齐 · MUST 不成环 + 负向对照 · MUST 有**判别性**证据 |
| `rv32i-isa-semantics` | MODIFIED ×1（`JALR` 的 `rs1` 约束） |
| `rv32i-commit-path` | MODIFIED ×2（停顿前提 + 「距离 1」取值结论） |
| `verilator-rv32i-e2e` | MODIFIED ×1（NOP 间隔理由 + 「无掩码 MUST 有端口清单」） |
| `rv32i-canonical-home` | MODIFIED ×1（mux 「已归属且已接入」+ 负向对照 + 判别性证据） |

⇒ **新建 capability** 的理由：前推是**微架构**契约（档位 / 数据源 / 打拍 /
距离矩阵 / 装配），与**纯 ISA 语义**、与**退休/有效链/停顿**是**三种不同的
验收证据** ⇒ 混在一起会互相掩盖 ⇒ **距离矩阵 MUST 只有一个出处**。

**文档同步**：根 `AGENTS.md`（3 条过时 + **补写「距离 3」**）·
`include/cpu/AGENTS.md`（「预留输入」条目消失）·
`ex_stage.h:200-202`（`JALR` 的 `rs1` 注释）·
`hazard_unit.h:83-86`（「经过前推MUX 后的数据」—— **今天就是假的**）·
`forwarding_dut_fixture.h` 头注释（该 fixture **不打拍**、**不含 link-select**
⇒ **不是**装配级证据）。

**簿记**：`CTEST_TOTAL` / `EXAMPLES_TEST_SOURCES` / NOTES 算术链 ·
`verify_example_assertion_quality.sh` · `verify_example_test_wiring.sh` ·
`verify_openspec_delta.sh` + `openspec validate --strict`（CLI 钉 1.4.1）。
⇒ `tests/benchmark/` **已核实零 rv32i 引用** ⇒ perf 基准**不受影响**。

## 3. Acceptance (验收标准)

### 功能性

- [ ] 相邻 ALU→ALU（**无 NOP**）的消费者 MUST 拿到**新值**
- [ ] 距离 2 的 load→use 的消费者 MUST 拿到**新值**
- [ ] **距离 3** 的 load→use 的消费者 MUST 拿到**新值**
- [ ] store 的**写数据（rs2）**紧邻被写时 MUST 拿到**新值**
- [ ] `JALR` 的 `rs1` 紧邻被写时 MUST 拿到**新值**
- [ ] 分支条件用**紧邻**新值时 MUST 走**正确**的分支
- [ ] 双生产者同 rd 的形态 MUST NOT 前推**错误的那一个**的值
- [ ] `x0` MUST NOT 被误命中

### 结构性

- [ ] 数据源的**叶子** MUST 是流水线寄存器 ⇒ 拓扑排序 MUST NOT 报组合环
- [ ] 档位 select MUST **打一拍**
- [ ] 档位 `1` 的源 MUST 含 `link_value`
- [ ] `HazardUnit` 的 5 个预留输入端口 MUST 已删除；4 个直通端口 MUST 保留
- [ ] `ex_stage.io().rs1_data` / `rs2_data` MUST 只有**单一**驱动
- [ ] `ex_is_load` MUST 被**全部**实例化点显式驱动
      （⚠️ 现状：**全仓 6 个独立实例化点全部未驱动**）
- [ ] 负向对照 MUST **仍然** `CHFATAL`，且 MUST 是**常驻测试**

### 可判别性（**零信号断言是不合格**）

- [ ] 8 类判别性固件 MUST 全部存在，且每类都有会转红的断言
- [ ] 每个判别性用例 MUST 带**反向期望**且 `X_old != X_new`
- [ ] 变异验证 ≥ 4 条**全被抓**，每条都**先确认编译成功**
- [ ] 2 条**固件级不可达**的变异 MUST 由**组件级**测试验收
- [ ] 所有重算的期望值 MUST **手工推导** + **注释留痕**
- [ ] 双后端逐拍全等，且比对端口 MUST 有**显式清单**
- [ ] 「首条指令 MUST NOT 自依赖」（填充期双装的**新不变量**）MUST 被断言或规避

### 回归与工程

- [ ] `ctest -LE perf` 零回归（**已核实** perf 基准不含 rv32i）
- [ ] 全部护栏脚本 exit 0
- [ ] 既有 golden 的变化 MUST **全部可归因**
- [ ] `openspec validate --specs --strict` 通过
- [ ] **对抗审稿**（critic，≤ 2 轮）+ `openspec archive`

## 4. Capabilities (涉及的能力维度)

- **`rv32i-data-forwarding`（新建）** —— ADDED ×4（**微架构契约的唯一权威**：
  最新值 / 距离矩阵 · 档位与时间对齐 · 不成环 + 负向对照 · 判别性证据）
- `rv32i-isa-semantics`（已存在）—— MODIFIED ×1
- `rv32i-commit-path`（已存在）—— MODIFIED ×2
- `verilator-rv32i-e2e`（已存在）—— MODIFIED ×1
- `rv32i-canonical-home`（已存在）—— MODIFIED ×1

## 5. Impact (影响面分析)

### 改动文件

| 文件 | 改动 |
|---|---|
| `include/cpu/pipeline/rv32i_pipeline.h` | 加 `ForwardingMux` 实例 + `fwd_a_d1`/`fwd_b_d1` 两个 `ch_reg` + 3 个前推源接线；**删** `:200`/`:201` 旧驱动；**删** 5 个预留端口的驱动；**加** regfile 读点的 WB 旁路（§B） |
| `include/cpu/pipeline/hazard_unit.h` | **删** 5 个预留输入端口；前推判定加 `& !ex_is_load`；修 `:83-86` 的假注释 |
| `include/cpu/pipeline/stages/ex_stage.h` | 修 `:200-202` 的过时注释（`JALR` 的 `rs1` 路径） |
| `include/chlib/forwarding.h` | ⚠️ 若按 §6 第 2 条裁决：`ForwardingMux` 加 `is_jump` / `jump_result` 输入（**端口面**变化，档位**语义**不变） |
| `examples/riscv-mini/tests/forwarding_dut_fixture.h` | 补 `ex_is_load` 驱动；删 5 处预留端口驱动；头注释补「非装配镜像」 |
| `examples/riscv-mini/tests/test_pipeline_debug.cpp` / `test_wrapper_simple.cpp` | 各删 5 处驱动；补 `ex_is_load` 驱动 |
| `examples/riscv-mini/tests/test_pipeline_exec.cpp` | 删 **8 区块 × 5 端口 = 40 处** `set_input_value`；补 `ex_is_load` 驱动 |
| `examples/riscv-mini/tests/test_rv32i_pipeline.cpp` | 删 **4 区块 × 5 端口 = 20 处** `set_input_value`；补 `ex_is_load` 驱动 |
| `examples/riscv-mini/tests/test_rv32i_soc_dual_parity.cpp` | 8 类判别性固件 + golden 全量重算 |
| 根 `AGENTS.md` / `include/cpu/AGENTS.md` | 3 条过时限制 + **补写「距离 3」** + 「预留输入」条目 |

### 爆炸半径（**实测**）

- `HazardUnit` 共 **7 个实例化点**（**6 个文件**）⇒ **7 个全部驱动**了那 5 个
  预留端口 ⇒ **删端口 ⇒ 7 个文件全部编译失败** ⇒ 爆炸半径**封闭可枚举**
- ⚠️ `forwarding_dut_fixture.h:90` 写的是 `ch::ch_module<riscv::HazardUnit>`
  （**带 namespace 前缀**）⇒ **不带前缀的 grep 会漏掉它**
- `ForwardingMux` 的实例化点 = `forwarding_dut_fixture.h:91` +
  `tests/chlib/test_forwarding.cpp`（2 个用例）⇒ **很小**
- 根 `tests/test_rv32i_pipeline.cpp` 实例化的是 `riscv::ForwardingUnit`（3 处）
  / `ControlHazardUnit`（1 处）⇒ **不受**影响
- 仓外无契约面

### API 变更

- ⚠️ **破坏性**：`HazardUnit` 删除 5 个**公开输入端口**
- ⚠️（若采纳 §6 第 2 条）`chlib::ForwardingMux` **新增 2 个输入端口**
- ⚠️ `HazardUnit` 的**前推判定语义**变化（加 `& !ex_is_load`）⇒ 独立消费者的
  既有断言**可能需要复核**（它们当前读到的 `ex_is_load` 是**未定义值**）

### 风险：**中-高**（改的是数据通路的时序语义）

| 缓解 | 说明 |
|---|---|
| 负向对照先行 | 第一屏跑 `bf78367` 形态 ⇒ 确认环检测仍 FATAL；且**固化为常驻测试** |
| 判别性固件**同批**落地 | ⚠️ 不允许「先接后补」 |
| 变异验证 4 条（2 条走组件级） | 每条先确认编译成功 |
| **反向期望 + 手工推导金标** | ⇒ 杜绝「跑一遍 sim 抄期望值」的循环论证 |
| 双后端逐拍 parity + **显式端口清单** | ⇒ 判据可执行 |
| 保守边界 | 档位 1 **不**覆盖 load ⇒ load-use 停顿**行为不变** ⇒ 停顿契约零改动 |

### 明确非目标

- **load-use 停顿仍在** ⇒ 取指 skid buffer = **T5**（本 change 的下游）
- **档位 1 覆盖 load**（V2）⇒ 独立立项
- **`riscv::ForwardingUnit` 与 `chlib::ForwardingMux` 的合并** ⇒ ADR-038 §2.3 留未来
  （⚠️ 前者的 `ex_alu_result` 输入 = 「活的 EX 输出」⇒ **环饵**）
- **分支预测**（`rv32i_branch_predict.h` 未接入流水线）
- **框架层改动**（`operator>>>` 仍是注释状态；`ch_mem` 语义）

## 6. ⚠️ 对 ADR-047 的修订请求（**实施 MUST 等这 3 条**）

起草期的缺口分析（metis）找出 ADR-047 的 **3 处缺陷**。第 1 条直接改变本
change 的**范围**，第 2 条改变**验收形态** ⇒ ⇒ **实施前 MUST 由 architect
修订 ADR-047**。

### 第 1 条（BLOCKER）：**距离 3** 的同沿竞态 —— ADR-047 的形态**修不了它**

- **事实**：`idex_rs1_data` **锁存**了 regfile 的组合读（`id_stage.h:255`）
  ⇒ ID→EX 锁存沿与 WB 写回沿**同沿** ⇒ 采样到**写回前**的值；
  regfile 读点（`rv32i_pipeline.h:172`）**无 WB 旁路**；
  hazard 只比较 EX / MEM **两级** ⇒ 距离 3 时生产者**已离开可命中的级**
  ⇒ **档位 2 的数据源也无处可取**；
- **后果**：ADR-047 §3「固件不再需要插 NOP」**不成立**
  ⇒ 距离矩阵 = 「1 ✓ / 2 ✓ / **3 ✗** / ≥4 ✓」
  ⇒ **「隔 2 条不行、隔 1 或 3 条才行」无法向固件作者交代**；
- **推荐裁决**：regfile 读点加 **WB→ID 组合旁路**
  （`wb_write_en & wb_rd == rs & rs != 0 ? wb_write_data : aread(...)`）
  ⇒ 约 10 行；数据叶是 `memwb_*` 寄存器 ⇒ **不成环**；
  ⇒ **不需要** hazard 的 `wb_*` 端口（D7 要删的那两个）⇒ 两者**正交**；
- **次选**：登记为残留限制 ⇒ 但必须同时**削弱** ADR-047 §3 的承诺。

### 第 2 条（BLOCKER）：档位 1 的 `link_value` 变异**没有可落地的 harness**

- **事实**：`ForwardingMux` 的端口面只有 `forward_a/b` + 3 路数据
  ⇒ **没有** `is_jump` / `link` 输入；`exmem_is_jump` 是 pipeline 的**成员**
  `ch_reg` ⇒ TB **无法驱动** ⇒ ADR-047 D9 要求的「定向接线级单测」
  **照现文本无法落地**；
- **危险**：实施者很可能交一个**复刻 DUT** ⇒ 测的是**副本**
  ⇒ 对**真实接线零信号** ⇒ **假绿**；
- **推荐裁决**：把档位 1 的源选择**收进 `ForwardingMux`**
  （新增 `is_jump` + `jump_result` 两个输入）⇒ 仍是**既有组件**
  （零新增组件）⇒ 档位**语义**不变 ⇒ 不违反 `rv32i-canonical-home` 的
  「逐档等价」（该条约束**档位语义**，不是端口面）⇒ 该变异即可由
  **组件级单测**判别。

### 第 3 条（BLOCKER）：D4 与 D9 第 6 类的**机制归因错误**

- **事实**：跳转在 **EX 级**就重定向 PC ⇒ 目标指令最早在**若干拍之后**才到 ID 级
  ⇒ 届时生产者**已离开可命中的级** ⇒ **没有任何前推档位能命中 jump 生产者**
  ⇒ ADR-047 D4「真正覆盖 jump 生产者的是**档位 2**」**不成立**；
- **后果**：① D9 第 6 类用例**不判别**；② 本草案初稿的 spec delta 里那条
  「jump 生产者的写回值 MUST 被前推」是**零信号断言** ⇒ **已删除**
  （改为「经寄存器堆路径」的 Scenario + 明确「MUST NOT 被当作前推的判别性证据」）；
- **推荐裁决**：D4 / D9 第 6 条 MUST 改为「跳转的返回地址经**寄存器堆写回
  路径**到达消费者 ⇒ **不属**前推档位的覆盖范围」；档位 1 的 `link_value`
  分支降级为**防御性**要求（验收走第 2 条的组件级单测）。

### 附：另两条 SHOULD_FIX（已在本草案内落实，不需 ADR 修订）

- **`& !ex_is_load` 门控变异在固件级不可达** ⇒ 已改为**组件级真值表**验收；
- **D4 的术语漂移**（三套档位命名）⇒ 已在 `rv32i-data-forwarding` 的
  Requirement 里钉死「档位编号 = **打拍之后**的语义」；
- ⚠️ **`wb_stage.write_data` 不是字面的 `ch_reg`**（它是 `wb_stage.h:84-87` 对
  `memwb_*` 寄存器的**组合 select**）⇒ 「数据源取自流水线寄存器」是**传递性**
  成立 ⇒ ADR 的表述 MUST 改为「数据源的**叶子** MUST 是流水线寄存器」。
