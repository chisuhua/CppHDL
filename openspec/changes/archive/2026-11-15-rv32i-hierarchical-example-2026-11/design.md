# Design: 双类消费者的发射路径 + RV32I 的成层与验收

> 决策层归 **ADR-070**（D1 = 发射器判据修正 · D2 = 实例级 `ch_layer` 7 处 · D3 = 契约名）。
> 本文只记**实现取舍**与**实施期实测订正**（后者是必须交回 architect 的部分）。

## 1. H-A：冲突判据**方向感知**

`add_conn` 的冲突循环遍历分析层为**该实例**记的连接行。其中：

| `into_child` | 语义 | `parent_side` 是什么 |
|---|---|---|
| `true` | **父驱动子 input** | 实参（承载值的网 / 端口） |
| `false` | **子 output → 父侧消费点** | **消费点**的 ctx 名（**不是**实参） |

⇒ ⇒ 只有 `into_child == true` 的行能构成「该 pin 的实参冲突」。`false` 的行表示
「这个子输出被父体某处读」，多个消费者是**合法扇出** ⇒ MUST NOT 判冲突。

**反向不变式（承重 · 已写进源码注释）**：`into_child == true` 的双驱动**永远** fail-closed
—— IR 的 `set_src` 是 last-wins ⇒ 放行 = 静默丢掉一个驱动。

## 2. H-B：兄弟边也登记 `parent_landing_wire`

`parent_landing_wire`（键 = `<module>\x1f<instance>\x1f<child_port>`）决定发射期是否
**丢弃**分析层的旧行（并在旧行连的是父 `output` 端口时发决策⑤ 的 `assign`）。
兄弟边分支此前**只**建中转网、不登记 ⇒ 旧行与新行并存 ⇒ Verilator `Duplicate pin connection`。

⇒ ⇒ 驱动侧 pin（`into_child=false` 的那次 `add_conn`）MUST 登记 ⇒ ⇒ 本次补齐。

**实测**：补齐后 20 条边归零 **且** 产物 lint 零 Error **且** 零 Warning
（说明 21–23 那条「父 `output` 端口」路径的 `assign` 也正确发出）。

## 3. `rewired_pins`：结构化可观测面

`HierarchyEmitResult::rewired_pins` = `parent_landing_wire` 的可读投影，格式
`<父 module>:<实例名>.<子侧端口> -> <落点网名>`，顺序 = 登记序（确定性）。

**为什么不是 `CHWARN`**（ADR-070 D1 明文）：该路径对**每个**「子 output 被父体消费」的 pin
都会发生 ⇒ 发 CHWARN 会让**所有**层次化发射常态告警，并牵连断言 `warnings.empty()` 的既有测试。

**只填成功路径**（失败时为空）⇒ 与 `ok` 一起读。

## 4. D2 的实施：7 处 `ch_layer`（**不含 `load_lane`**）

ADR-070 §2.2 列出 7 个实例名与模块类型名（`if/id/ex/mem/wb/hazard/forwarding_mux`）。
B3 之后 `Rv32iPipeline` 新增了第 8 个成员 `load_lane`（`LoadLaneSelector`）。

⇒ ⇒ **按 ADR 的 7 处字面实施**，`load_lane` 保持 `ch_module`（inline）。理由：

1. D3 的**契约名集合是精确的 7 个** ⇒ 把 `load_lane` 也成层 = 擅自新增第 8 个**产物级契约名**；
2. 集合变更属**决策**（ADR-070 的 D2）⇒ 归 architect，builder 不自决。

**实测后果**：`load_lane` inline 进 `top` ⇒ 它的 `ch_out` 成为 `top` 的 output 端口
（`top_pipeline_load_lane_selector_data` 等），且它的输入由 `mem_stage`（成层子）驱动 ⇒
走**行 2** 机制（子 output 端口 + 父侧落点网）—— 与 D1 修的兄弟边分支**互不干扰** ✓。

## 5. 判据口径订正（⚠️ 实测 · 交回 architect）

ADR-060 §2.2 G3 的**依据句**写「顶层端口表 MUST 与扁平产物**逐字逐序**一致」。
实测（RV32I 7 处 `ch_layer` + `load_lane` inline）**只成立一半**：

| 维度 | 实测 |
|---|---|
| **逐字** | ✅ 层次化顶层的每个端口条目都在扁平侧**逐字**存在（集合 ⊆） |
| **逐序** | ⚠️ **只在根 `io()` 端口段成立**；其后「inline 子组件的 `ch_out`」与「成层子组件的 `ch_out`」的相对序两侧不同（扁平按 eval 序，层次化按 owner 序） |
| **集合** | ⚠️ 扁平侧**多出 61 个**端口 = 成层子组件的 `ch_out`（= `hierarchy_equivalence.h` 明写的「观测盲区」/ 台账 id=23） |

⇒ ⇒ 本 change 的判据按**可判定形态**写：**根端口段逐字逐序** + **其余逐字存在（⊆）**。
⇒ ⇒ ADR-060 的**依据句**是否要改写 = **architect 面**（本 change 只交回实测事实，
**不改 ADR 正文**）。

## 6. 负向对照 (a) **结构性不可达**（如实登记）

「两个**合成**驱动争同一个子 input pin」：`ch_in` 的 `set_driver` 是 **last-wins**
（后写覆盖前写）⇒ ⇒ IR 层一个输入端口**只可能有一个**驱动 ⇒ ⇒ 分析层不可能同时产出两条
`into_child == true` 的边 ⇒ ⇒ **无法用真实 DUT 构造**该负向对照。

⇒ ⇒ **不作假绿**：不写「已覆盖」；该不变式的载体 = 源码注释 + capability 的反向 Requirement。

## 7. 手推期望值（★ 禁抄仿真输出）

固件（6 条 · 每条按 RV32I 语义独立推导）：

| word | 指令 | 语义推导 | 期望 |
|---|---|---|---|
| 0 | `addi x1, x0, 10` | `x1 = 0 + 10` | **10** |
| 1 | `addi x2, x0, 3` | `x2 = 0 + 3` | **3** |
| 2 | `add  x3, x1, x2` | `x3 = 10 + 3` | **13** |
| 3 | `sw   x3, 0x100(x0)` | `mem[0x100] = 13` | 地址 **0x100** · 数据 **13** |
| 4 | `lw   x4, 0x100(x0)` | `x4 = mem[0x100]`（★ 只有 store 真生效才是 13） | **13** |
| 5 | `add  x5, x4, x1` | `x5 = 13 + 10`（★ 只有 load 真读到 13 才是 23） | **23** |

⇒ ⇒ **判别值** = `(rd=5, data=23)`：它同时要求 ALU、store、load、寄存器堆**全部**正确
⇒ ⇒ 恒 NOP 的流水线 / 未装载的 imem 都**不可能**产出它。

编码核对（手算，实测曾因一处编码笔误把 `lw` 的 `rd` 写成 6 而暴露）：
`lw x4, 0x100(x0)` = `(0x100<<20) | (0<<15) | (2<<12) | (4<<7) | 0x03` = **`0x10002203`**。

## 8. 基线订正：**20** 条（不是 23）

improvement §A.2 的 23 条清单写于 **2026-11-13**（B3 之前）。B3 改了流水线（D-TCM `sread` ·
M2 寄存器组 · 档位 2 源改指 · 车道 mux 迁移 · 停顿链加一级）⇒ ⇒ 边的集合随之重算
⇒ ⇒ **2026-11-15 实测基线 = 20 条**（逐条点名见探针输出）。

⇒ ⇒ spike 判据按「**当前基线 → 0**」判，并在此**显式登记**与 improvement 的差异
（MUST NOT 让「23→0」成为不可对账的数字）。

## 9. 判别性自证：两次「假绿」的教训（如实记录）

| 尝试 | 篡改对象 | 结果 | 原因 |
|---|---|---|---|
| ① | `IfStage` 内的 `assign <uint_lit> = 32'h80000000;` | **仍 equal** | 该常量只作为 `ch_reg` 的 init（被内联进 `always_ff`）⇒ 改它**不改变行为** |
| ② | `top` 内的实例端口映射 `.pin(net)` ⇒ `.pin('0)` | **仍 equal** | 该 pin 的下游在 198 拍内**不被观测** |
| ③ | `top` 内**驱动被观测信号**的 `assign` ⇒ `'0` | ✅ **转红** | 与扁平侧的真实值必然分叉 |

⇒ ⇒ 结论：判别性自证 MUST 篡改**能保证影响观测**的线（否则判据假绿）。
「内部通路正确」由 198 拍逐拍等价 + 手推写回序列承担 ⇒ ⇒ 三者互补，互不替代。

## 10. 与既有判据的关系

- **扁平产物零扰动**：`ch_layer` 只写 `is_layer` / `module_type_name` 两个标志，扁平路径不读
  ⇒ ⇒ **结构性恒真** ⇒ 只作跳线，**不作承重证据**（ADR-070 §2.2 已明文）。
- **`verify_hierarchical_rtl.sh` 的语料**不含 RV32I ⇒ 本 change 的 lint 门在**测试内**
  （`test_rv32i_hier_rtl` 的 `[product]` 用例）⇒ ⇒ 复用已生成的产物，不新增 CI 步骤。
- **`CTEST_TOTAL` +1 · `VERILATOR_E2E_TOTAL` +1**（新 target 带 `verilator_e2e` 标签）
  ⇒ 锚点同批同步（`scripts/update_test_count_anchors.sh`）。

## 11. 实施期发现：**parity 护栏的覆盖缺口**（已修）

`scripts/lib/dual_backend_parity.py` 的 `locked_set` 解析用的是 `` `tests/...cpp` `` 前缀
⇒ ⇒ **example 侧**的单后端条目（DUT 在 `examples/**` ⇒ 根 `tests/**` 不许 include
⇒ 只能在 example 侧）**无法**进入契约锁定清单 ⇒ ⇒ 新增条目一律被判
`unclassified-labeled-entry`（实测：`examples/riscv-mini/tests/test_rv32i_hier_rtl.cpp`）。

**修法**：正则同时认 `tests/...` 与 `examples/**/tests/...`。
⚠️ **这不是放宽**：「**不在**清单里」仍判违规（对象与严格度不变，只是让合法登记**可被解析**）。
⚠️ 自检 `--self-test` **12/12 fixture** 全绿（含 F11 = 「标签 + 无 parity + 未登记 ⇒ 违规」）。

## 12. 其他实施期订正

| # | 订正 | 证据 |
|---|---|---|
| ① | 基线 **20** 条（improvement 的 23 条写于 B3 之前） | `probe_r21_hier_granularity.sh` |
| ② | ADR-070 §2.2 的「7 处」不含 B3 新增的 `load_lane` ⇒ 按字面实施 | `rv32i_pipeline.h` 的成员数 = 8 |
| ③ | 顶层端口表「逐序」只在**根端口段**成立 | §5 的实测表 |
| ④ | 负向对照 (a) **结构性不可达** | §6 |
| ⑤ | 判别性自证两次假绿 | §9 |
| ⑥ | Catch2 的 `||` 重载会把 `a != npos || b != npos` 判成**链式比较** ⇒ 断言内 MUST 先算成 `bool` | 编译期 static_assert 实测 |
