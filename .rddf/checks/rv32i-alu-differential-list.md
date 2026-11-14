---
evidence: test:tests/test_rv32i_alu_differential.cpp
---

# RV32I ALU 差分清单（`Rv32iAlu` vs `ExStage` 内联 ALU）—— B4 · C1

> **本文件是「差异清单」的**唯一登记处** ⇒ ⇒ 判据 = `tests/test_rv32i_alu_differential.cpp`
> （机器可执行）⇒ ⇒ **清单外的值差异 MUST = 0**。
> **架构依据** = `.rddf/improvements/rv32i-dead-component-consolidation-2026-11.md` 的
> 「差分（前置于接入）」段（**C1**）· **批次** = phase-6 **B4**（C1 → C2 → C3/C4）。

## 0. 差分对象与口径

| 项 | 内容 |
|---|---|
| **库组件侧** | `riscv::Rv32iAlu`（`include/cpu/pipeline/rv32i_alu.h`）—— **尚未**接入流水线 |
| **内联侧** | `riscv::ExStage` 的内联 ALU（`include/cpu/pipeline/stages/ex_stage.h:136-208`）—— **生产代码本体** |
| **可比输出** | `result`↔`alu_result` · `zero`↔`zero` · `less_than`↔`less_than` · `less_than_u`↔`less_than_u`（**4 项 × 21 条用例 = 84 组比较**） |
| **输入口径** | 只驱动 **R 型**（`opcode = OP_OP`）⇒ ⇒ 内联的「第二操作数 = `rs2_data`」⇒ ⇒ 与库组件的 `operand_b` **同口径** |
| ⚠️ **为什么用真实 `ExStage` 而不是「逐行照抄一份」** | 改进的 AC 写的是「等价物 MUST 逐行照抄」（目的是**防自证**）⇒ ⇒ 本文件用**更强**的做法：差分另一侧**就是生产代码本身** ⇒ ⇒ 「复制漂移」风险从根上不存在 ⇒ ⇒ AC 的本意被更强地满足（**偏离已登记**） |
| ⚠️ **期望值来源** | **全部手推**（禁抄仿真输出）⇒ ⇒ 每条用例两侧**各自**对手推值断言 ⇒ ⇒ 「两者相等」只是**附加**判据 |

## 1. 差异清单（逐条 + 处置）

| # | 差异 | 现状（**实测**） | 处置 | 判据 |
|---|---|---|---|---|
| **Δ1** | ✅ **已修（2026-11-14 · B4 · C2）**：原为 🔴 **ALU 操作码编码完全不同** | 内联 `ALU_OP_*`：ADD 0 · SUB **1** · AND **2** · OR **3** · XOR 4 · SLL **5** · SRL **6** · SRA **7** · SLT **8** · SLTU **9**<br>库 `ALU_*`：ADD 0 · SLL **1** · SLT **2** · SLTU **3** · XOR 4 · SRL **5** · OR **6** · AND **7** · SUB **10** · SRA **11**<br>⇒ ⇒ **8 / 10 项不同** ⇒ ⇒ `alu_op = 3` 内联 = **OR**、库 = **SLTU** | ✅ **已修 = C2（编码归一）**：`ex_stage.h` 的 `ALU_OP_*` **已删除** ⇒ ⇒ 唯一权威 = `riscv::ALU_*` ⇒ ⇒ 差分测试的编码断言**已翻转**为「两侧**必须相同**」（棘轮）⇒ ⇒ 公开面移除登记 = `public-api-surface.md` + `public-api-removals.tsv`（`re` 探针）+ `CHANGELOG.md` |
| **Δ2** | **ADD 中间值宽度** | 内联 `chlib::add<32>`（**32** 位）· 库 `a + b`（`ch_uint` + `ch_uint` ⇒ **33** 位）⇒ ⇒ 赋值进 32 位 `res` 时**隐式截断** ⇒ ⇒ **值等价**（`0xFFFFFFFF + 1 = 0`） | **不修**（值等价）⇒ ⇒ ⚠️ 接入时**必须**复核截断语义（库的写法是**隐式**截断） | 差分用例 `ADD(0xFFFFFFFF, 1)` ⇒ 两侧皆 0 |
| **Δ3** | **SUB 表达式** | 内联 `chlib::subtract<32>` · 库 `a - b` ⇒ ⇒ 含环绕 `0 - 1 = 0xFFFFFFFF` | **不修**（实测值等价） | 差分用例 `SUB(0, 1)` |
| **Δ4** | **SLT 的 `select` 分支类型** | 内联 `rs1_sign`（**1 bit**）· 库 `a_neg`（**bool**）⇒ ⇒ 值等价（都是「rs1 为负」） | **不修**（值等价）⇒ ⇒ ⚠️ **类型差异可能在 Verilog 发射宽度上可见** ⇒ ⇒ 接入后复查产物 | 差分用例 `SLT(0xFFFFFFFF, 1)` |
| **Δ5** | **SLTU 无符号比较** | 两侧**都**用无符号 `<` ⇒ ⇒ **修复前**库的 `bits<31,31>(sub_result)`（取**结果位**而非**借位位**）已由 **B0 ⑧** 修 | **已修**（B0 ⑧）⇒ ⇒ 该条**不在**当前差分结果里（修前才有） | 差分用例 `SLTU(0, 0xFFFFFFFF)` = 1（旧实现给 0） |
| **Δ6** | **第二操作数来源** | 内联按 `opcode` 选（`imm` vs `rs2_data`）· 库只接受 `operand_b` | **不修**（**结构性**）⇒ ⇒ 它**不是** ALU 语义差异，而是**接入方案**的责任：EX 级算好 `alu_second_operand` 再喂 `operand_b` | 差分只在 R 型口径下跑 ⇒ ⇒ 本条**不进**差分表 |
| **Δ7** | **`equal` 输出** | 库组件**独有**（EX 级无对应；BEQ 走 `branch_type` 判定）⇒ ⇒ **有消费者**（`examples/riscv-mini/rv32i_phase2_test.cpp` 读 `io().equal`） | **不修**（组件自有能力 + 有测试消费者）⇒ ⇒ 接入时**不接**即可（悬空输出须显式处理） | — |
| **Δ8** | **`mem_size` / `mem_signed` 输出** | 库组件**独有** + **零消费者**（全仓 grep 命中 0） | **待修 = ZERO-DEBT**（接入前**删除**）⇒ ⇒ ⚠️ 属**公开面移除** ⇒ ⇒ MUST 走 `public-api-removals.tsv` + `public-api-surface.md` + `CHANGELOG.md` | 全仓 grep（零命中） |
| **Δ9** | **`funct7`** | 两侧**都不消费** | **不修**（无差异） | — |
| **Δ10** | **标志来源** | `zero` / `less_than` / `less_than_u` 两侧都是同口径的组合量 | **不修**（实测值等价） | 差分的 84 组比较 |

## 2. 差分结论（**实测**）

| 指标 | 值 |
|---|---|
| 差分用例数 | **21**（覆盖全部 **10** 种操作） |
| 比较组数 | **84**（21 × 4 输出） |
| **清单外的值差异** | **0** ⇒ ⇒ 两套实现**当前值等价** |
| 手推期望值自洽性 | ✅（② 用例：比较标志 MUST 与「手算的有符号 / 无符号比较」一致；零标志 MUST 与结果一致） |
| **变异自证** | ✅ 内联 `and_result` 改 `or_gate` ⇒ ⇒ 差分**红**（`REQUIRE(inl.result == c.exp_result)` 失败）⇒ ⇒ 还原 + **重建**后复绿 |

⚠️ **本轮手推期望值被抓出 3 处错**（AND/OR/XOR 的比较标志、XOR 的结果值）⇒ ⇒
**自洽性用例（②）** 与 **① 各自对值** 两条合起来把它们**逐条抓住** ⇒ ⇒
这正是「期望值 MUST 手推 + 禁抄仿真输出」这条纪律的**实际收益**。

## 3. C2 归一落地时**必须**同步的三处 —— ✅ **2026-11-14 全部已同步**

| # | 位置 | 动作 | 状态 |
|---|---|---|---|
| 1 | `tests/test_rv32i_alu_differential.cpp` 的编码用例 | **翻转**（断言两套编码**逐项相同**）+ 锁死归一后的 10 个数值 | ✅ |
| 2 | 本文件 Δ1 | 状态由「待修」改为「**已修**」+ 记录归一后的**唯一**编码表 | ✅ |
| 3 | `.rddf/improvements/rv32i-dead-component-consolidation-2026-11.md` 的 AC 勾选 | 由 builder 在收口时回填（**planner 域** ⇒ ⇒ 未代改） | ⬜ |

### C2 的**配套改动**（归一不可能只改常量定义）

| # | 位置 | 为什么必须改 |
|---|---|---|
| 1 | `include/cpu/pipeline/stages/ex_stage.h` | 删 `ALU_OP_*`（10）+ 10 处 select 链改引用 + include `rv32i_alu.h` |
| 2 | `include/cpu/pipeline/rv32i_pipeline.h` | `alu_op_calc` 的 **12** 处引用 ⇒ ⇒ **数值变了** ⇒ ⇒ 但两侧 select 链引用**同一套常量** ⇒ ⇒ **行为不变** |
| 3 | `tests/test_rv32i_stage_dual.cpp` 的 `kExCases` | **28 行**的 `alu_op` **输入列**按新编码重映射（9 行实际变化）⇒ ⇒ **期望值一字未动** ⇒ ⇒ ⇒ ⚠️ **这就是「归一正确性」的判据**：映射错一条 ⇒ ⇒ 那条**必红** |
| 4 | `examples/riscv-mini/tests/test_pipeline_exec.cpp` | `ALU_OP_ADD_ = 0u`（**本地字面量副本**）⇒ ⇒ 改 `riscv::ALU_ADD` |
| 5 | `examples/riscv-mini/tests/test_pipeline_debug.cpp` | `ch_uint<4>(0_d)` + 注释 `// ALU_OP_ADD` ⇒ ⇒ 改 `riscv::ALU_ADD` |
| 6 | 公开面登记 | `ALU_OP_*` 一族**移除** ⇒ ⇒ `public-api-surface.md` + `public-api-removals.tsv`（`re` 探针）+ `CHANGELOG.md` |

⚠️ **AC 的一处事实订正**：AC 写「`test_pipeline_exec.cpp:124` 仍以**字面量 `SLTU = 0x3`** 驱动 `Rv32iAlu`」
⇒ ⇒ **不成立**：该行是 `namespace riscv_funct3 { SLTU = 0x3 }` ⇒ ⇒ 那是 **funct3** 编码
（ISA 层）⇒ ⇒ **既不驱动 ALU 也不该改** ⇒ ⇒ 该文件里真正驱动 `Rv32iAlu` 的是
`ALU_ADD` / `ALU_SUB` / `ALU_SRA` ⇒ ⇒ 它们**本来就来自 `rv32i_alu.h` 的符号常量** ✓
⇒ ⇒ 真正的「字面量副本」是 `ALU_OP_ADD_ = 0u`（已改）⇒ ⇒ **AC 的措辞已按实测订正**。

## 4. 未纳入本差分的部分（**如实登记**）

| 项 | 为什么不在差分表里 |
|---|---|
| **I / U / J 型的第二操作数选择**（`imm` vs `rs2_data`） | 属 **Δ6**（结构性）⇒ ⇒ 差分只在 R 型口径下跑 ⇒ ⇒ 接入时由 EX 级负责 ⇒ ⇒ 接入后 MUST 另补一条「I 型口径」的判据 |
| **`mem_size` / `mem_signed` / `equal`** | 库组件**独有** ⇒ ⇒ 内联侧**无对应** ⇒ ⇒ 不可比较 ⇒ ⇒ 见 Δ7 / Δ8 |
| **分支判定 / 重定向目标** | 属 `ExStage` 的**分支通路**，不是 ALU ⇒ ⇒ 不在 C1 范围（`Rv32iDecoder` 接入 = **C4**，另立差分） |
| **两后端一致性** | 差分对象是**两套 C++ 实现**，不是同一实现的两后端 ⇒ ⇒ 后端一致性由 `test_rv32i_stage_dual`（`ExStage` 双后端）+ `test_rv32i_alu`（库组件）承担 ⇒ ⇒ 本测试**单后端** ⇒ ⇒ **不动** parity 计数锚点 |
