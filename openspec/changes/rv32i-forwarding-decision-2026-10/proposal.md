# 提案：RV32I 流水线 MUST 接数据前推（ADR-046 §D5 的落地裁决）

- **架构依据**：`docs/adr/ADR-047-rv32i-data-forwarding.md`（Accepted 2026-10-27，
  **2 轮 critic 对抗审稿** ⇒ 第 1 轮 APPROVE_WITH_CONDITIONS ⇒ 第 2 轮 **APPROVE**）
- **5 段草案**：`.rddf/improvements/rv32i-forwarding-decision-2026-10.md`
- **roadmap**：`.rddf/roadmap/features/feat-rv32i-isa-completeness.md` ③
- ⚠️ **本提案在起草后期（metis 缺口分析）发现 ADR-047 有 3 处缺陷** ⇒ 见
  §「对 ADR-047 的修订请求」⇒ **实施 MUST 等那 3 条裁决落地**（否则会按错误的
  机制归因实施 ⇒ 静默错值）

## Why

### 缺口：lib 流水线**不接**数据前推 ⇒ 3 条限制被固化成了「ISA 语义」

`include/cpu/pipeline/hazard_unit.h:198`：

```cpp
io().rs1_data = io().rs1_data_raw;   // 直通，无前推
```

⇒ 根 `AGENTS.md §KNOWN LIMITATIONS` 登记了 **3 条**由此派生的限制：

| # | 限制 | 出处 |
|---|---|---|
| ① | 相邻 ALU→ALU 依赖读到**旧值** ⇒ **固件 MUST 自行插 NOP / 间隔** | `AGENTS.md:168` |
| ② | 距离 2 的 load→use（`lw` → `nop` → `add`）**检测不到** ⇒ 读**旧值** | `AGENTS.md:170` |
| ③ | `JALR` 的 `rs1` **无数据前推** ⇒ 固件 MUST NOT 依赖「`rs1` 刚被前一条指令写」 | `AGENTS.md:178` + `rv32i-isa-semantics` spec 的明文条款 |

⚠️ 这 3 条**不是「5 级流水线的固有属性」，而是「没接前推」的实现后果**
⇒ 继续留在「ISA 语义」层 = 把实现缺陷固化成规范。

### 事故史：为什么之前「接不上」

`git show bf78367`（2026-04-15）—— 一次**组合环事故**：

```
ex_stage.alu_result → hazard.ex_alu_result → hazard.rs1_data(mux) → ex_stage.rs1_data → ex_stage.alu_result
```

全链组合、无寄存器节点 ⇒ 拓扑排序 `FATAL: Cycle detected in IR graph`。
该事故的处置是**移除 mux** ⇒ 这就是今天的状态。

⇒ **ADR-047 D3 定位了断环的充分条件**：`context.cpp` 的拓扑排序对 `type_reg`
**只遍历 `init_val`** ⇒ **寄存器是断路器** ⇒ 数据源取自**流水线寄存器** ⇒ 环断开。

### 为什么现在

1. **ADR-046 §D5 已裁决** ⇒ 悬置项清零；
2. **T5（取指 skid buffer）依赖本裁决** ⇒ 本 change 是 T5 的**强前置**；
3. ⚠️ **现有固件刻意无冒险** ⇒ 前推**接错 / 接反 / 没接，全仓照绿**
   ⇒ **判别性固件 MUST 与接线同批落地**；
4. capability `rv32i-isa-semantics` **已存在** ⇒ delta **追加**；
5. **「前推未裁决」是 spec 层的悬置指针**（`rv32i-isa-semantics` 与
   `rv32i-commit-path` 各有一处引用未决决策）⇒ 本 change 消除该指针。

## What Changes

### 0. 前置（第一屏）：**负向对照** —— 证明环检测护栏仍然有效

先跑 `bf78367` 形态的**最小复现**（数据源取自 EX/MEM 级的**组合输出**）
⇒ 确认拓扑排序**仍然** `CHFATAL` ⇒ **然后**才接新形态。

⇒ 若护栏已失效 ⇒「新形态不成环」这个结论**没有意义**（它可能只是**环检测坏了**）。

### 1. 前推接线（**零新增组件**）

**mux = 既有 `chlib::ForwardingMux<32>`**（`include/chlib/forwarding.h:184-243`）
—— ADR-047 D2 指定的**唯一**数据 mux（档位语义与当年被移出的 mux **逐档等价**：
0→REG / 1→EX / 2→MEM）⇒ **MUST NOT** 把 mux 放回 `HazardUnit`。

**三个数据源**（全部取自**流水线寄存器** ⇒ 不成环）：

| 档位 | mux 端口 | 源 | 备注 |
|---|---|---|---|
| 0 = REG | `rs1_data_reg` / `rs2_data_reg` | `id_stage.io().rs1_data` / `rs2_data` | ⚠️ 该值是**ID/EX 锁存值**（`id_stage.h:255`）⇒ 见 §「距离矩阵」的**距离 3** 行 |
| 1 = EX/MEM | `ex_mem_alu_result` | `select(exmem_is_jump, exmem_link_value, exmem_alu_result)` | ⚠️ **MUST 含 `link_value`**（防御性，见下）<br>⚠️ 三个源同受 `exmem_en` 守卫 ⇒ **对齐**（`rv32i_pipeline.h:315` / `:318` / `:343`） |
| 2 = MEM/WB | `mem_wb_result` | `wb_stage.io().write_data` | **已合并** load / jump / ALU 三路 ⇒ **覆盖「相隔两级的 load」** |

**档位 select MUST 打一拍**（ADR-047 D5）：pipeline 级加两个 `ch_reg`
（`fwd_a_d1` / `fwd_b_d1`）。

- **根因**：`hazard` 的前推比较点是「**EX 级** rd vs **ID 级** rs」（load-use
  检测点），而 mux 喂的是 **EX 级**操作数 ⇒ 拍 T 的档位描述的是「T+1 消费者进 EX」，
  mux 在拍 T 就用了 ⇒ **错一级**；
- ⚠️ 后果**不是「不前推」，而是把错误数据塞进当前 EX 指令** ⇒ 静默错值；
- ⇒ 打拍 ⇒ `hazard` 的组合逻辑**一字不改** ⇒ 既有真值表测试**因打拍而全部保留**。

### 2. ⚠️ **距离矩阵**（本 change 的核心契约；**单一权威** = `rv32i-data-forwarding`）

| 距离 | 取值来源 | 状态 |
|---|---|---|
| 1（紧邻）· ALU→ALU | 档位 `1`（EX/MEM 级） | ✅ 前推覆盖 |
| 1（紧邻）· load→use | **load-use 停顿**（3 拍） | ⚠️ 停顿**保守地**不覆盖 load（档位 1 排除）⇒ 停顿仍在 |
| 2 | 档位 `2`（MEM/WB 级合并写回值） | ✅ 前推覆盖 |
| **3** | **寄存器堆路径** | ⚠️⚠️ **同沿竞态 —— 见 §「对 ADR-047 的修订请求」第 1 条** |
| ≥ 4 | 寄存器堆路径 | ✅ 写回早已落地 |

⇒ **距离 3 是唯一需要额外处理的距离**：ID/EX 在 ID→EX 锁存沿上采样 regfile 的
**组合读**，而 WB 写回落在**同一条沿** ⇒ 采样到的是**写回前**的值
⇒ 该距离**不经前推档位**（生产者此时已离开可命中的级）⇒ **必须靠读点旁路**。

⚠️ 这是**今天就存在的**缺陷（无前推时同样读旧值）⇒ 现有固件用 **3 条 NOP
（距离 4）恰好躲开** ⇒ 文档里**从未被记录**。

### 3. `HazardUnit` 的清理（ADR-047 D7 / D8）

| 动作 | 对象 | 理由 |
|---|---|---|
| **删 5 个预留输入端口** | `wb_reg_write` / `wb_rd_addr` / `ex_alu_result` / `mem_alu_result` / `wb_write_data` | 无内部消费者；⚠️ `ex_alu_result` 已接线到 EX 级**组合输出**（`rv32i_pipeline.h:469`）⇒ 它是**环饵** ⇒ **删掉让 ADR 自执行** |
| **保留 4 个直通端口** | `rs1_data_raw` / `rs2_data_raw`（输入）+ `rs1_data` / `rs2_data`（输出） | `HazardUnit` 作为**独立组件**仍有**真实消费者**：`forwarding_dut_fixture.h:105-106` 喂前两个；`test_pipeline_exec.cpp:531` / `:560` **读**后两个 |
| ⚠️ **pipeline 装配内的驱动** | `rv32i_pipeline.h:484` / `:485` | 接 mux 后 `hazard.rs1_data` 在 pipeline 内**无消费者** ⇒ 继续驱动 = 「喂了没人读」⇒ **ZERO-DEBT 违规**。<br>⚠️ **但这与仓库反模式「新增输入端口 ⇒ 全部实例化点 MUST 显式驱动」正面相撞**（未驱动端口读**未定义值**）⇒ **裁决见 §「两例律冲突的裁决」** |
| **前推判定加门控** | `forward_ex_to_a` / `forward_ex_to_b` 加 `& !ex_is_load` | `ex_reg_write` **包含 load** ⇒ 保守形态下档位 1 不覆盖 load ⇒ 判定侧 MUST 排除 |
| **收敛双驱动** | 删 `rv32i_pipeline.h:200` / `:201` | `inputimpl::set_driver` 替换 `driver_` 但 `add_src` **累积** ⇒ IR 图残留 2 条 src 边 ⇒ **ZERO-DEBT / 拓扑排序 hygiene**（⚠️ **不是**「今天已双后端分叉」—— `codegen_verilog.cpp:613-625` 只取单数 `driver()`） |

### 4. 测试（**判别性固件是必需项**）

⚠️ 现有固件**刻意无冒险** ⇒ 前推接错 / 接反 / 没接，**全仓照绿**
⇒ **判别性固件 MUST 与接线同批落地**（MUST NOT「先合后补」）。

**判别性固件**（`examples/riscv-mini/tests/test_rv32i_soc_dual_parity.cpp`）：

| # | 用例 | 覆盖 |
|---|---|---|
| 1 | 相邻 ALU→ALU，**无 NOP** | 档位 `1` |
| 2 | 距离 2 的 load→use | 档位 `2` |
| 3 | **距离 3** 的 load→use | 读点旁路（§2） |
| 4 | `JALR` 的 `rs1` 紧邻被写 | 档位 `1` |
| 5 | 分支条件用**紧邻**新值 | 档位 `1` |
| 6 | **store 的写数据（rs2）**紧邻被写 | mux 的 **b 路** |
| 7 | 双生产者同 rd | 档位优先级 |
| 8 | `x0` 的负向用例 | `rd_nz` 门控 |

⚠️ **用例 8（jump 的写回值）NOT 在此列** —— 跳转的返回地址**不经前推档位**到达
消费者（跳转在 EX 级就重定向 PC，目标指令若干拍后才到 ID 级，此时生产者已
离开可命中的级）⇒ 该值经**寄存器堆写回路径**到达 ⇒ 用它当判别性证据是
**零信号** ⇒ 只作**回归**用例。

**变异验证**（每条都先确认**编译成功** —— 编译失败的变异会跑旧二进制 ⇒ **假绿**）：

| 变异 | 期望 | 验收形态 |
|---|---|---|
| 去掉打拍 | 红 | **固件级**可判别 |
| 交换档位 1 / 2 的数据源 | 红 | **固件级**可判别 |
| 前推判定漏 `& !ex_is_load` | 红 | ⚠️ **固件级不可达**（误选只打进气泡 ⇒ 结果被丢弃）⇒ MUST 用**组件级真值表**（独立驱动 `ex_is_load=1` + `ex_reg_write=1` + rd 匹配 ⇒ 断言档位 ≠ `1`） |
| 档位 1 忘接 `link_value` | 红 | ⚠️ **固件级不可达** ⇒ MUST 用**组件级单测**（见 §「对 ADR-047 的修订请求」第 2 条） |

**负向对照**（第一屏）：`bf78367` 形态的最小复现 MUST **仍然** `CHFATAL`
⇒ 且 MUST **固化为常驻测试**（一次性人工检查 ⇒ 护栏失效无人守）。

### 5. 规格与文档同步

**5 个 capability 走 delta**（MUST NOT 直接改主 spec）：

| capability | 动作 | 内容 |
|---|---|---|
| **`rv32i-data-forwarding`（新建）** | ADDED ×4 | 数据前推 MUST 提供最新值（含**距离矩阵**）· 档位/数据源/时间对齐 · MUST 不成环 + 负向对照 · MUST 有**判别性**证据 |
| `rv32i-isa-semantics` | MODIFIED ×1 | `JALR` 的 `rs1` 从「MUST NOT 依赖数据前推 —— 前推未裁决」→「MUST 取最新值」 |
| `rv32i-commit-path` | MODIFIED ×2 | 停顿的「无数据前推」前提（改为「**保守地**不覆盖 load」）；「距离 1」自保的取值结论 |
| `verilator-rv32i-e2e` | MODIFIED ×1 | NOP 间隔的**理由**过时 + 新增「该 fixture MUST NOT 被当作前推正确性的证据」+「无掩码 MUST 有显式端口清单」 |
| `rv32i-canonical-home` | MODIFIED ×1 | mux 从「MUST 归属」→「**已**归属且**已**接入」+ 负向对照 + 判别性证据 |

⇒ **新建 capability** 的理由：前推是**微架构**契约（档位/数据源/打拍/距离矩阵/
装配），与 `rv32i-isa-semantics` 的**纯 ISA 语义**、与 `rv32i-commit-path` 的
**退休/有效链/停顿**是**三种不同的验收证据** ⇒ 混在一起会让「ISA 正确性」与
「微架构时序」互相掩盖 ⇒ **单一权威**（距离矩阵尤其 MUST 只有一个出处）。

**文档同步**（不在 spec 层，但同样 MUST 做）：

- 根 `AGENTS.md §KNOWN LIMITATIONS`：**3 条**过时（:168 / :170 / :178）
  ⇒ 且 **距离 3** 的限制**当前根本没写** ⇒ MUST 补写（或随 §2 的旁路一并消除）；
- `include/cpu/AGENTS.md`：「`HazardUnit` 的『预留输入』端口无内部消费者」条目
  ⇒ D7 删端口 ⇒ 该条目消失；
- ⚠️ `include/cpu/pipeline/stages/ex_stage.h:200-202`：「`JALR` 的 `rs1` 取
  `io().rs1_data` ⇒ **无数据前推**（ADR-046 D5 悬置）」⇒ **过时**
  （且 `JALR` 的 PC 重定向路径从此**经过 mux** ⇒ 组合深度 +1）；
- ⚠️ `include/cpu/pipeline/hazard_unit.h:83-86`：「前推数据输出（**经过前推MUX
  后的数据**）」⇒ **今天就是假的**（`:198-199` 是直通）⇒ MUST 改为如实描述；
- `examples/riscv-mini/tests/forwarding_dut_fixture.h` 的头注释：该 fixture
  **不打拍**、**不含 link-select** ⇒ 落地后它**不再镜像**真实流水线接法
  ⇒ MUST **显式声明**它**不是**装配级证据（否则「装配错了」对它零信号）。

### 6. 簿记（MUST 与代码同批更新）

- `CTEST_TOTAL`（202）/ `EXAMPLES_TEST_SOURCES`（17）/ 根 `NOTES` 的算术链
  （新增测试文件或用例时）
- `scripts/verify_example_assertion_quality.sh`（钉住断言计数）⇒ **MUST 重跑**
- `scripts/verify_example_test_wiring.sh` ⇒ **MUST 重跑**
- `scripts/verify_openspec_delta.sh` + `openspec validate --strict`
  （CLI 钉 **1.4.1**，本机 1.14.3 的额外 warning 属版本漂移）
- `tests/benchmark/`：**已核实零 rv32i 引用** ⇒ perf 基准（寄存器链 / 异或链）
  **不受前通影响** ⇒ perf 套件**预期**零变化（仍 MUST 实测）

## Acceptance

### 功能性

- [ ] 相邻 ALU→ALU（**无 NOP**）的消费者 MUST 拿到**新值**
- [ ] 距离 2 的 load→use 的消费者 MUST 拿到**新值**
- [ ] **距离 3** 的 load→use 的消费者 MUST 拿到**新值**
- [ ] store 的**写数据（rs2）**紧邻被写时 MUST 拿到**新值**
- [ ] `JALR` 的 `rs1` 紧邻被写时 MUST 拿到**新值**
- [ ] 分支条件用**紧邻**新值时 MUST 走**正确**的分支
- [ ] 双生产者同 rd 的形态 MUST NOT 前推**错误的那一个**的值
- [ ] `x0` MUST NOT 被误命中（生产者写 x0 / 消费者读 x0）

### 结构性（ADR-047 的机制约束）

- [ ] 数据源 MUST 全部取自**流水线寄存器** ⇒ 拓扑排序 MUST NOT 报组合环
- [ ] 档位 select MUST **打一拍** ⇒ 拍 T 的档位 MUST 在 T+1 生效
- [ ] 档位 `1` 的源 MUST 含 `link_value`
- [ ] `HazardUnit` 的 5 个预留输入端口 MUST 已删除
- [ ] `HazardUnit` 的 4 个直通端口 MUST 保留（独立组件仍可用）
- [ ] `ex_stage.io().rs1_data` / `rs2_data` MUST 只有**单一**驱动
- [ ] `ex_is_load` MUST 被**全部**实例化点显式驱动
      （⚠️ 现状：**全仓 6 个独立实例化点全部未驱动** ⇒ 读到**未定义值**）
- [ ] 负向对照：`bf78367` 形态 MUST **仍然** `CHFATAL`，且 MUST 是**常驻测试**

### 可判别性（**零信号断言是不合格**）

- [ ] 8 类判别性固件 MUST 全部存在，且**每类都有会转红的断言**
- [ ] 每个判别性用例 MUST 附带**反向期望**（「未接前推时读到 `X_old`」）
      且 `X_old != X_new` ⇒ 否则该用例 MUST 被视为**无效**并重写
- [ ] 变异验证 ≥ 4 条**全被抓**，且每条都**先确认编译成功**
- [ ] 2 条**固件级不可达**的变异 MUST 由**组件级**测试验收
      （`& !ex_is_load` 门控 / 档位 1 的 `link_value`）
- [ ] 所有重算的期望值 MUST **手工推导**并在**代码注释**里留痕
      ⇒ MUST NOT 从仿真输出抄值
- [ ] 双后端（interpreter + verilator）**逐拍全等**，且比对端口 MUST 有
      **显式清单**
- [ ] 填充期双装的**新不变量**：「首条指令 MUST NOT 自依赖」
      （否则第二次执行会读到第一次的结果 ⇒ 非幂等）MUST 被显式断言或规避

### 回归与工程

- [ ] `ctest -LE perf` 零回归（**已核实** perf 基准不含 rv32i ⇒ 预期零变化）
- [ ] 全部护栏脚本 exit 0（含 `verify_example_assertion_quality.sh` /
      `verify_example_test_wiring.sh`）
- [ ] 既有 golden 的变化 MUST **全部可归因**（新增用例导致的变化 vs 语义变化）
- [ ] `openspec validate --specs --strict` 通过（archive 前）
- [ ] **对抗审稿**（critic，≤ 2 轮）+ `openspec archive`

## 对 ADR-047 的修订请求（⚠️ 实施 MUST 等这 3 条）

起草期的缺口分析（metis）找出 ADR-047 的 **3 处缺陷**。其中第 1 条会直接
改变本 change 的**范围**，第 2 条会改变**验收形态** ⇒ ⇒ **实施前 MUST 由
architect 修订 ADR-047**。

### 第 1 条（BLOCKER）：**距离 3** 的同沿竞态 —— ADR-047 的形态**修不了它**

- **事实**：`idex_rs1_data` **锁存**了 regfile 的组合读（`id_stage.h:255`）
  ⇒ ID→EX 锁存沿与 WB 写回沿**同沿** ⇒ 采样到**写回前**的值；
  regfile 读点（`rv32i_pipeline.h:172`）**无 WB 旁路**；
  hazard 只比较 EX / MEM **两级** ⇒ 生产者在距离 3 时**已离开可命中的级**
  ⇒ **档位 2 的数据源也无处可取**；
- **后果**：ADR-047 §3「固件不再需要插 NOP」**不成立** ⇒ 距离矩阵变成
  「1 ✓ / 2 ✓ / **3 ✗** / ≥4 ✓」⇒ **「隔 2 条不行、隔 1 或 3 条才行」是无法向
  固件作者交代的约束**；
- **推荐裁决**：在 regfile 读点加 **WB→ID 组合旁路**
  （`wb_write_en & wb_rd == rs & rs != 0 ? wb_write_data : aread(...)`）
  ⇒ 约 10 行；数据叶是 `memwb_*` 寄存器 ⇒ **不成环**；
  ⇒ **不需要** hazard 的 `wb_*` 端口（D7 要删的那两个）⇒ 两者**正交**；
- **次选**：登记为残留限制 ⇒ 但必须同时**削弱** ADR-047 §3 的承诺
  ⇒ 且固件约束将不可解释。

### 第 2 条（BLOCKER）：档位 1 的 `link_value` 变异**没有可落地的 harness**

- **事实**：`ForwardingMux` 的端口面只有 `forward_a/b` + 3 路数据
  （`forwarding.h:188-206`）⇒ **没有** `is_jump` / `link` 输入；
  `exmem_is_jump` 是 pipeline 的**成员** `ch_reg`（`rv32i_pipeline.h:135`）
  ⇒ TB **无法驱动** ⇒ ADR-047 D9 要求的「定向接线级单测」**照现文本无法落地**；
- **危险**：实施者很可能交一个**复刻 DUT**（自己重建 select）⇒ 测的是**副本**
  ⇒ 对**真实接线零信号** ⇒ **假绿**；
- **推荐裁决**：把档位 1 的源选择**收进 `ForwardingMux`**（新增 `is_jump` +
  `jump_result` 两个输入）⇒ 仍是**既有组件**（零新增组件）⇒ 档位**语义**不变
  （`1`→EX/MEM）⇒ 不违反 `rv32i-canonical-home` 的「逐档等价」条款
  （该条约束的是**档位语义**，不是端口面）⇒ 该变异即可由**组件级单测**判别；
- **爆炸半径**：`ForwardingMux` 的实例化点 = `forwarding_dut_fixture.h:91`
  + `tests/chlib/test_forwarding.cpp`（2 个用例）⇒ 很小。

### 第 3 条（BLOCKER）：D4 与 D9 第 6 类的**机制归因错误**

- **事实**：跳转在 **EX 级**就重定向 PC ⇒ 目标指令最早在**若干拍之后**才到 ID 级
  ⇒ 届时生产者**已离开可命中的级** ⇒ **没有任何前推档位能命中 jump 生产者**
  ⇒ ADR-047 D4「真正覆盖 jump 生产者的是**档位 2**」**不成立**；
- **后果**：① D9 第 6 类用例**不判别**（它实际是「`link_value` 经 regfile
  通路」的回归测试）；② 本提案初稿的 spec delta 里那条
  「jump 生产者的写回值 MUST 被前推」是**零信号断言** ⇒ **已在本稿删除**
  （改为 `rv32i-data-forwarding` 里的「经寄存器堆路径」Scenario + 明确
  「MUST NOT 被当作前推的判别性证据」）；
- **推荐裁决**：ADR-047 D4 / D9 第 6 条 MUST 改为「跳转的返回地址经**寄存器堆
  写回路径**到达消费者 ⇒ **不属**前推档位的覆盖范围」；档位 1 的
  `link_value` 分支降级为**防御性**要求（其验收走第 2 条的组件级单测）。

### 附：另两条 SHOULD_FIX（已在本提案内直接落实，不需 ADR 修订）

- **`& !ex_is_load` 门控变异在固件级不可达** ⇒ 已改为**组件级真值表**验收
  （与第 2 条同型）；
- **D4 的术语漂移**（`hazard_unit.h:78` 的「1=来自 EX，2=来自 MEM」/
  ADR 的「1=EX/MEM，2=MEM/WB」/ mux 的端口名 `ex_mem_alu_result`/`mem_wb_result`
  三套命名）⇒ 已在 `rv32i-data-forwarding` 的 Requirement 里钉死
  「档位编号 = **打拍之后**的语义」；
- ⚠️ **`wb_stage.write_data` 不是字面的 `ch_reg`**（它是 `wb_stage.h:84-87` 对
  `memwb_*` 寄存器的**组合 select**）⇒ 「数据源取自流水线寄存器」是**传递性**
  成立（叶子是 reg ⇒ 断环论证不受影响）⇒ ADR 的表述 MUST 改为「数据源的
  **叶子** MUST 是流水线寄存器」。

## 两例律冲突的裁决（planner 侧，已写入 proposal §3）

删除 pipeline 内的 `rs1_data_raw` / `rs2_data_raw` 驱动，与仓库反模式
「新增组件输入端口 ⇒ 全部实例化点 MUST 显式驱动」**正面相撞**：

- 例律 A（ZERO-DEBT）：「喂了没人读」⇒ **删驱动**；
- 例律 B（未驱动端口读**未定义值**）：⇒ **必须驱动**；

⇒ **裁决：保留驱动**。理由：① 例律 B 保护的是**正确性**（未定义值 ⇒ 静默
错值），例律 A 保护的是**整洁** ⇒ **正确性优先**；② 保留驱动是**免费**的
（2 行 `<<=`，且驱动的是 `id_stage.io().rs1_data` 这个**合法来源**）；
③ 删端口（`rs1_data_raw` / `rs2_data_raw` 本身）**不在**本 change 的范围
（它们是**独立消费者**在用的真端口）⇒ 本 change 只**删 5 个预留输入端口**。

⇒ ⇒ 修正 ADR-047 D7：「pipeline 装配内 MUST 删掉 `:484`/`:485` 两处驱动」
⇒ **改为**「MUST **保留**驱动，但 MUST 在注释里如实说明
『该驱动的消费者已迁到 mux，本端口保留是因为它是**合法输入**』」。

## Non-Goals

- **load-use 停顿仍在**（3 拍 + 「丢弃消费者之后的第一条指令」）
  ⇒ 取指 skid buffer = **T5 `rv32i-fetch-skid-buffer-2026-10`**（本 change 的下游）
- **档位 1 覆盖 load**（V2）⇒ 独立立项
- **`riscv::ForwardingUnit` 与 `chlib::ForwardingMux` 的合并**
  ⇒ ADR-038 §2.3 裁决留未来
  （⚠️ `riscv::ForwardingUnit` 的 `ex_alu_result` 输入 = 「活的 EX 输出」⇒ **环饵**）
- **分支预测**（`rv32i_branch_predict.h` 未接入流水线）
- **框架层改动**（`operator>>>` 仍是注释状态；`ch_mem` 语义）
- **不新建第二个 capability** ⇒ 5 个（1 新建 + 4 MODIFIED）已覆盖全部契约面
