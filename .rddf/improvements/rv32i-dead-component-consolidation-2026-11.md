---
name: rv32i-dead-component-consolidation-2026-11
priority: P2
phase: phase-6
category: cpu
type: chore
状态: draft
主题: riscv-mini 技术债清零
依赖: public-api-remove-visibility-2026-11（公开面移除/改名登记机制）· rv32i-commit-path（时序基线）
来源: 用户 2026-11-10 拍板「#6 按你建议」+ planner 实测消费点普查
生成时间: 2026-11-10
evidence: probe:grep 消费点普查（Rv32iDecoder / Rv32iAlu / riscv::ForwardingUnit / HazardDetectionUnit / rv32i_hazard_complete / BranchPredictor*）
---

# Improvement: RV32I **重复实现**收口（组件接入 + 冗余实现退役）

- **Date drafted**: 2026-11-10
- **Drafted by**: planner 角色（build-agent）
- **Status**: 📝 **draft**（待用户拍板 ⇒ 拍板后转 builder 流程）
- **用户裁决（2026-11-10）**: 「#6 按你建议」⇒ ⇒ **Decoder/Alu 接入**（先差分测试）+ **ForwardingUnit 删除** + **分支预测器单独立项**
- **⚠️ 事实订正（相对 planner 上一轮的口头表述）**: 这批组件**不是「零消费者」** —— 它们**都有测试消费者** ⇒ ⇒ 问题不是「死代码」，而是「**被测但未被交付**」⇒ ⇒ 详见 §架构依据 A.1

---

**主题**: riscv-mini 技术债清零

## 架构依据

### A.1 消费点普查（2026-11-10 实测）

| 组件 | 头文件行数 | **真实流水线**消费者 | **测试**消费者 | 判定 |
|---|---|---|---|---|
| `Rv32iDecoder` | 161 | **0** | `rv32i_phase{1,2,3}_test.cpp` · `test_pipeline_exec.cpp` · `tests/test_rv32i_arch_dual.cpp` · `tests/test_rv32i_stage_dual.cpp`<br>⚠️ **metis 订正**：`rv32i_phase{1,2,3}_test` 是 `riscv_mini_unit_test`（`examples/riscv-mini/CMakeLists.txt:8-15`）⇒ ⇒ **无 `add_test`** ⇒ ⇒ **不在 CTest 的 257 里**；⚠️ **critic 第 2 轮订正**：但它们**在** `CH_TRACKED_EXAMPLE_TARGETS`（`examples/CMakeLists.txt:34-76`）里 ⇒ ⇒ **`run_all_ported_tests.sh` 会跑**（`main()` 退出码即判据，无断言）⇒ ⇒ 准确表述 = 「**进 CTest 之外、有运行无断言**」⇒ 仍**不是**回归证据 |
| `Rv32iAlu` | 172 | **0** | `rv32i_phase1_test.cpp` · `rv32i_phase2_test.cpp` · `test_pipeline_exec.cpp`（⚠️ 同上：phase 测试**从不执行**）<br>⚠️ **metis 追加**：交付路径**已有独立 oracle** —— `tests/test_rv32i_stage_dual.cpp:320-362` 用 **27 条手推期望**逐项断言 `ExStage` 的 `alu_result`/`branch_taken`/`branch_target`/`link_value`/`zero`/`less_than`/`less_than_u`/`is_store_alu` ⇒ ⇒ 「交付路径对 ALU 零覆盖」**不成立** ⇒ ⇒ 真正的缺口是「**组件版**未被交付」+「**组件版**有 SLTU 缺陷」 |
| `riscv::ForwardingUnit`<br>(`rv32i_forwarding.h`) | 174 | **0** | `tests/test_rv32i_pipeline.cpp` · `tests/test_rv32i_pipeline_behavior.cpp`（⚠️ **metis 订正**：`tests/chlib/test_forwarding.cpp` **不**消费它 —— 该文件只 include `chlib/forwarding.h`，测 `forward_detect` / `ForwardingMux`）| ⚠️ **被测但未被交付**（与已交付的 `chlib::ForwardingMux` **时序口径不同**，见 A.2 ③）<br>⚠️ **metis 追加**：该组件「独有且被覆盖」的语义 = **空集**（WB 档**无任何用例覆盖** · `id_reg_read` 死输入）⇒ ⇒ 退役时**删除**而非迁移 |
| `HazardDetectionUnit`<br>(`hazard_detection_unit.h`) | 65 | **0** | `tests/test_rv32i_pipeline.cpp` · `tests/test_rv32i_pipeline_behavior.cpp` | ⚠️ 同上 |
| `rv32i_hazard_complete.h` | 195 | **0** | `tests/chlib/test_hazard_complete.cpp` · `test_hazard_complete_dual.cpp` · `test_phase4_integration.cpp` | ⚠️ 同上 |
| `rv32i_branch_predict.h` + `branch_predictor_v2.h` | 475 + 151 | **0** | `tests/chlib/test_branch_predictor_v2.cpp` · `tests/test_rv32i_pipeline*.cpp` | ❌ **不是**重复实现 ⇒ 是**功能缺口**（分支预测未接）⇒ ⇒ **本项不做** |

### A.2 为什么「被测但未被交付」比「零消费者」更危险

| # | 事实 | 证据 |
|---|---|---|
| ① | 仓内**零测试**断言「真实流水线用的是哪个实现」⇒ ⇒ 换成内联副本**不会有任何测试转红** | ID/EX 级的内联实现与组件实现**无任何交叉断言** |
| ② | ⚠️ **两份 ALU 的操作码编码不同** ⇒ ⇒ 直接换会**静默算错** | `rv32i_alu.h:25-34`（ADD=0 · SLL=1 · SLT=2 · SLTU=3 · XOR=4 · SRL=5 · OR=6 · AND=7 · **SUB=10** · **SRA=11**）vs `ex_stage.h:34-43`（ADD=0 · **SUB=1** · AND=2 · OR=3 · XOR=4 · SLL=5 · SRL=6 · **SRA=7** · SLT=8 · SLTU=9） |
| ③ | ⚠️ `riscv::ForwardingUnit` 是 **3 档**（含 WB 档）而**已交付**的 `chlib::ForwardingMux` 走 **2 档 + WB→ID 组合旁路** ⇒ ⇒ 两者**时序口径不同** ⇒ ⇒ 换过去会**改行为** | `include/cpu/pipeline/rv32i_forwarding.h:149-170` vs `include/chlib/forwarding.h:184` + `rv32i_pipeline.h:536-574`（ADR-047 D5 的「档位描述的是 T+1 消费者进 EX 时选哪一级」） |
| ④ | ⚠️ ID 级的**立即数选择**在 `rv32i_decoder.h`（按 `type` 选）与 `id_stage.h:181-189`（按 **opcode** 选）**各写了一份** ⇒ ⇒ 逻辑**不等价**（未知 opcode 时 decoder 的 `type` 默认 `INSTR_R` 而 `imm` = 0） | 两处并存 |
| ⑤ | ⚠️ **metis 订正（原稿事实错）**：`funct7[5]` 门控**不在** `ex_stage.h`，而在 `rv32i_pipeline.h:292-296`（`alu_is_sub_sra`）⇒ ⇒ `ex_stage.h` 的 `funct7` 端口**零消费者**（`rv32i_pipeline.h:273` 驱动它，但 EX 级不用）⇒ ⇒ 「接进去要差分 `funct7` 维度」是**错的方向**（双方都不消费）⇒ ⇒ 真正要差分的是 **SLTU / ADD 宽度 / `less_than_u` 内部矛盾**（见下）<br>⚠️ **metis 追加（SLTU 是真缺陷，非「编码差异」）**：`rv32i_alu.h:93` 的 `lt_unsigned_sig = bits<31,31>(sub_result)` ⇒ ⇒ `0 < 0xFFFFFFFF` 得 **0**（期望 **1**）⇒ ⇒ 且组件内部 `result`(SLTU) 与 `less_than_u` **自相矛盾** ⇒ ⇒ 「差异清单」若不含 SLTU ⇒ **清单不合格** | `include/cpu/pipeline/rv32i_pipeline.h:292-296` · `include/cpu/pipeline/rv32i_alu.h:93,148` |
| ⑥ | ⚠️ 「不接」曾经造成过**真实事故**：前推数据 mux 曾留在 `HazardUnit` 里，接进真实装配 ⇒ ⇒ 组合环（`FATAL: Cycle detected in IR graph during topological sort`）⇒ ⇒ 接组件时 MUST 避开组合环 | `include/cpu/AGENTS.md` ANTI-PATTERNS「把数据 mux 放进 `HazardUnit`」· 事故记录 `bf78367` |
| ⑦ | 分支预测**未接** ⇒ ⇒ 每个 taken 分支 / JAL / JALR 付 **3 拍冲刷** ⇒ ⇒ 这是**功能缺口**，不是重复实现 | `include/cpu/pipeline/hazard_unit.h`（flush 唯一来源 = `ex_branch & ex_branch_taken`）· 本轮实测 `control_stalls = 4` |

### A.3 「被测但未被交付」的根本问题

仓内既有纪律：**「两后端一致」不能当作正确性证据**（同源生成 ⇒ 同构缺陷一致地错）。⇒ ⇒ **同一逻辑适用于「组件测试 vs 交付实现」**：
`Rv32iAlu` 的测试全绿 ⇒ ⇒ 但**交付路径根本没跑过它** ⇒ ⇒ 那些测试**不构成**对交付代码的覆盖 ⇒ ⇒ **虚假信心**。

⇒ ⇒ 本项的判据 MUST 是**「交付路径确实用了组件」+「组件行为与被替换的内联行为逐条等价」**，而不是「组件自己的测试还绿」。

---

## 范围

### ✅ 在内

| 编号 | 内容 |
|---|---|
| **C1** | **差分测试先行**（**先于任何接入**）：⚠️ **metis 订正**：① 仓内**已有**差分基础设施 —— `tests/test_rv32i_stage_dual.cpp` 的 `kExCases`(27 条手推) / `kDecCases`(17 条) / `ImmConsistencyDut`（ADR-063 D4 的两份 imm 等价性）⇒ ⇒ 本项 MUST **复用**它们，**MUST NOT** 重造第二套 ② 「等价物」MUST 规定为「**逐行照抄** `ex_stage.h:136-208`（不许重构 / 不许简化）」，否则差分退化自证 ③ 差异清单 MUST 至少含：**SLTU 借位（真缺陷）** · **ADD 33 位 vs 32 位**（`ch_uint` + `ch_uint` 结果宽 `max+1`，交付路径用 `chlib::add<32>`）· **`less_than_u` 与 `result` 内部矛盾** · `mem_size`/`mem_signed` 死输出 · `funct7` 双方都不消费 |
| **C2** | **统一 ALU 操作码编码**：以**已交付**的 `ex_stage.h::ALU_OP_*` 为唯一权威（或反之 —— **MUST 由 architect/builder 裁决并写进契约**）⇒ ⇒ 两份常量 MUST **归一**为一份（放公共头）⇒ ⇒ 删除另一份 |
| **C3** | **`Rv32iAlu` 接入 EX 级**：EX 级改为实例化 `Rv32iAlu`（**只搬算术核心**；**操作数选择**（I 型取 imm / R 型取 rs2）**留在 EX 级** ⇒ ⇒ 见 `rv32i_alu.h:41` 的 `operand_a`/`operand_b`/`alu_op` 端口）⇒ ⇒ 接入后 MUST 确认**无组合环**（⑥） |
| **C4** | **`Rv32iDecoder` 接入 ID 级**：ID 级改为实例化 `Rv32iDecoder`（opcode/rd/rs1/rs2/funct3/funct7/imm/type）⇒ ⇒ 消除 ④ 的双份立即数逻辑 ⇒ ⇒ ⚠️ **风险**：ID 级现有的 `is_op`/`is_load`/… 标志由 **opcode 直接比较**得出（`id_stage.h:123-131`）⇒ ⇒ 接入后 MUST 决定「标志由 `instr_type` 派生」还是「仍由 opcode 比较」⇒ ⇒ **不得**让两者并存 |
| **C5** | 🔴 **B3 修复（critic 第 1 轮 BLOCKER）：C5 MUST 与 C6 同批** —— 因为 `include/cpu/pipeline/hazard_detection_unit.h:19` **include 了** `rv32i_forwarding.h`（另有 2 个测试 TU 经它间接受影响：`tests/test_rv32i_pipeline.cpp:28`、`tests/test_rv32i_pipeline_behavior.cpp:69`）⇒ ⇒ **单独删头文件 ⇒ `cmake --build` 必红**。<br>⇒ ⇒ **批次写成「C5 + C6 同一 commit」**（退役 + 同族处置 + 死 include 清理），并**在 C6 里先断 include 再删头**。<br>原 C5 内容：**`riscv::ForwardingUnit` 退役**
| **C6** | **同族处置**：⚠️ **metis 要求给出可判定准则**（三级）：① 有交付路径消费者 ⇒ **接入**；② 无消费者 **且** 其独有语义**未被任何 capability 明文要求** ⇒ **退役**；③ 无消费者 **但**独有语义被 capability 要求 ⇒ **保留** + `include/cpu/AGENTS.md` 写「⚠️ 非流水线路径」+ 必须有**非测试**的第二个消费者，否则仍退役。<br>⇒ 按此表：`HazardDetectionUnit` ⇒ 退役（其 MEM 级 load-use 检测点在 `hazard_unit.h:229-233` 已注明「流水线里永不触发」）· `rv32i_hazard_complete.h` ⇒ 退役（2 个死输入 + 3 档语义 + 唯一存在理由是 parity 夹具，可换 `HazardUnit` 做 DUT）<br>⚠️ **顺序约束**：`hazard_detection_unit.h` **include 了** forwarding + branch_predict + regfile + alu + decoder + pcu + pipeline_regs ⇒ ⇒ **C5（删 forwarding）MUST 先于/伴随 C6**，否则中间态编译不过 |
| **C7** | **交接登记**：若某组件**保留**但**不接入**流水线，MUST 在 `include/cpu/AGENTS.md` 的「WHERE TO LOOK」里**明写**「⚠️ 非流水线路径 · 仅独立使用场景」⇒ ⇒ 消除「看起来是流水线的一部分」的误导 |

### ❌ 不在本项

- **分支预测器接入** ⇒ ⇒ 用户已裁决**单独立项**（功能增强 ≠ 重复实现）
- **ISA 缺口**（非法指令 trap / FENCE / CSR）⇒ 归 `rv32i-isa-gap-closure-2026-11`
- **时序契约**（D-TCM 寄存读）⇒ 归 `memory-timing-contract-2026-11` ⇒ ⇒ ⚠️ 建议本项在它**之后**做（否则时序金标算两遍）
- **任何「为了让测试变绿」而弱化判据** ⇒ ⇒ 判据只可**更强**

---

## Why

### W.1 现状给的是**虚假信心**

`Rv32iAlu` 有测试、有双后端测试、`test_pipeline_exec.cpp` 也覆盖它 ⇒ ⇒ 看覆盖率会觉得"ALU 有测试" ⇒ ⇒ 但**交付路径一行都没跑过它** ⇒ ⇒ 内联副本里的任何 bug（哪怕与组件版不同）**没有任何测试会发现**。

⇒ ⇒ 仓内已有同型事故：`funct7[5]` 对 I 型误判（`addi x, x0, -8` 算成 SUB）—— 那次修复发生在**内联副本**里 ⇒ ⇒ 组件版很可能仍有该缺陷 ⇒ ⇒ 而组件版的测试**测不出**它（因为测试的是组件自己）。

### W.2 直接换会静默算错

② 两份 ALU 编码不同（SUB=1 vs SUB=10）⇒ ⇒ 若把 `Rv32iAlu` 接到 `ex_stage` 的 `alu_op` 上而不改编码 ⇒ ⇒ **所有 SUB/SRA 静默算错** ⇒ ⇒ 而 SUB/SRA 恰好是**有测试的**（既有 golden 含 `SRA`）⇒ ⇒ 会被抓到 —— 但**前提是先做差分**，否则抓到的可能是"差分用例自己写错了"。

### W.3 ForwardingUnit 换过去会改行为

③ 3 档 vs 2 档 + WB→ID 旁路 ⇒ ⇒ 档位语义不同（ADR-047 D5）⇒ ⇒ 换过去 ⇒ ⇒ 前推时序变 ⇒ ⇒ load-use 停顿拍数变 ⇒ ⇒ **不是重命名，是换实现** ⇒ ⇒ 用户已裁决**退役**（正确：`chlib::ForwardingMux` 是 ADR-047 指定的那一份）。

### W.4 「保留但不接入」必须**说清楚**

`rv32i_branch_predict.h`（475 行）· `rv32i_hazard_complete.h`（195 行）· `rv32i_forwarding.h`（174 行）· `rv32i_decoder.h`（161 行）· `rv32i_alu.h`（172 行）· `hazard_detection_unit.h`（65 行）⇒ ⇒ 合计 **1242 行**，占 `include/cpu/` 的 **~19%**（6473 行）⇒ ⇒ 在一个"5 级流水线 CPU"的头目录里，这个比例**会误导读者**（以为流水线由这些组件组成）⇒ ⇒ C7 的登记是**低成本、高收益**的诚实化动作。

---

## What Changes

### C1 — 差分测试（先于接入）

- 新建差分用例（**组件级**，不进流水线）：同一组指令字（覆盖 **LUI/AUIPC/JAL/JALR/6 分支/5 load/3 store/9 OP-IMM/10 OP + 未知 opcode**）⇒ ⇒ 分别喂给「组件」与「当前内联路径的等价物」⇒ ⇒ 比对**全部输出**（`alu_result` / `zero` / `imm` / `type` / `rd` / `rs1` / `rs2` / `funct3` / `funct7`）。
- ⚠️ **差分前 MUST 先有一份「差异清单」**（人工读码列出两者的已知差异）⇒ ⇒ 差分结果 MUST 与清单**逐条对应** ⇒ ⇒ 清单外的差异 = **未预期差异** ⇒ ⇒ **判别力来源**。
- ⚠️ **差分测试本身**是**判别力**的来源：⇒ ⇒ 必须有**变异自证**（故意改内联一处 ⇒ 差分必红）。

### C2 — ALU 编码归一

- 公共头（建议 `include/cpu/pipeline/rv32i_alu_ops.h` 或并入 `rv32i_alu.h`）持有**唯一**一份 `ALU_OP_*` ⇒ ⇒ `ex_stage.h` 与 `rv32i_alu.h` **同时**改为引用 ⇒ ⇒ 删除重复常量。
- ⚠️ **裁决点**：以哪一份为准？⇒ ⇒ **建议以已交付路径（`ex_stage.h`）为准**（它的编码已被既有 golden 钉住）⇒ ⇒ 组件版改适配（`rv32i_alu.h` 的 `ALU_SUB=10` ⇒ 变 1）⇒ ⇒ ⚠️ 这会**破坏** `rv32i_phase{1,2}_test.cpp` 里的手写 `alu_op` 常量 ⇒ ⇒ MUST 同步更新那些测试 ⇒ ⇒ **登记**。

### C3 — `Rv32iAlu` 接入 EX 级

- `ex_stage.h` 的 `describe()`：实例化 `ch::ch_module<Rv32iAlu> alu{"alu"}` ⇒ ⇒ 接 `operand_a` / `operand_b` / `alu_op` ⇒ ⇒ 取 `res` ⇒ ⇒ **删掉**内联的 10 个算术分支。
- ⚠️ **`zero` 标志**：组件有 `zero` 输出（`rv32i_alu.h:139`）⇒ ⇒ EX 级**当前不用**它 ⇒ ⇒ 接入后 MUST 决定「接上」还是「显式不用」⇒ ⇒ **不得**留悬空输出。
- ⚠️ **组合环检查**：`rv32i_alu.h` 内部**不**含寄存器 ⇒ ⇒ 纯组合 ⇒ ⇒ 无环风险 ⇒ ⇒ 但接入后 MUST 跑拓扑排序验证（仓内既有 `FATAL: Cycle detected` 防护）。

### C4 — `Rv32iDecoder` 接入 ID 级

- `id_stage.h` 的 `describe()`：实例化 `ch::ch_module<Rv32iDecoder> decoder{"decoder"}` ⇒ ⇒ 接 `instruction` ⇒ ⇒ 取 `opcode` / `rd` / `rs1` / `rs2` / `funct3` / `funct7` / `imm` / `instr_type`。
- ⚠️ **立即数单一来源**：删除 `id_stage.h:181-189` 的 `selected_imm` 链（改用 `decoder.imm`）⇒ ⇒ **前提**：decoder 的 `imm` 选择必须与现状**逐条等价**（C1 的差分 MUST 覆盖）。
- ⚠️ **标志的单一来源**：现状 `is_op`/`is_load`/… 由 **opcode 比较**得出（`id_stage.h:123-131`）⇒ ⇒ decoder 的 `instr_type` 是**另一套**枚举 ⇒ ⇒ ⚠️ `OP_SYSTEM`（0x73）在 decoder 里 `type` 默认 `INSTR_R` ⇒ ⇒ 若改用 `instr_type` 派生标志 ⇒ ⇒ **未知指令会被当成 R 型** ⇒ ⇒ 行为变化 ⇒ ⇒ ⇒ **裁决**：标志**继续由 opcode 比较**（保持现状语义），decoder 只提供**字段**与 `imm` ⇒ ⇒ `instr_type` 输出**显式标注为不使用**（或推动 ISA 项统一）。
- ⚠️ **寄存器堆读地址**：`id_stage.h` 用 `rs1_addr_mux`（`select(is_op|..., rs1, 0)`）⇒ ⇒ 接 decoder 后，`rs1` 来自 decoder、`mux` 仍在 ID 级 ⇒ ⇒ 保持。

### C5 — `riscv::ForwardingUnit` 退役

- 删除 `include/cpu/pipeline/rv32i_forwarding.h` ⇒ ⇒ 更新 `include/cpu/AGENTS.md`（NAMESPACE MAP 里 `riscv` 命名空间的条目）⇒ ⇒ 更新 `include/cpu/pipeline/rv32i_forwarding.h` 的引用方（测试）⇒ ⇒ 走 `public-api-remove-visibility-2026-11` 的**移除登记**（`.rddf/checks/public-api-surface.md` 的「已移除 / 已改名的公开符号」节 + `docs/CHANGELOG.md` 一行）⇒ ⇒ 护栏 `scripts/verify_public_api_visibility.sh` MUST 全绿。
- ⚠️ **测试迁移**：`tests/test_rv32i_pipeline.cpp` / `test_rv32i_pipeline_behavior.cpp` 中直接用 `riscv::ForwardingUnit` 的用例 ⇒ ⇒ MUST 迁到 `chlib::ForwardingMux` 口径（**注意**：档位语义不同 ⇒ ⇒ 用例的期望值 MUST **重推**，禁抄）⇒ ⇒ 或删（若其职责已被 `HazardUnit` + `ForwardingMux` 覆盖）⇒ ⇒ **逐条裁决并登记**。

### C6 / C7 — 同族处置与交接登记

- `HazardDetectionUnit`（65 行）：职责与 `HazardUnit` 重叠（load-use 检测）⇒ ⇒ **裁决**：退役（`HazardUnit` 是已交付路径）⇒ ⇒ 其测试迁/删。
- `rv32i_hazard_complete.h`（195 行）：组合式 hazard 单元 ⇒ ⇒ **裁决**：退役或明确降级为「独立示例组件」并在 AGENTS.md 标注。
- `rv32i_branch_predict.h` / `branch_predictor_v2.h`：**不属本项** ⇒ ⇒ 但 MUST 在 AGENTS.md **标注「未接入流水线」**（否则读者会以为有预测）⇒ ⇒ **接入**属独立立项。

---

## Acceptance

### 差分（前置于接入）

- [ ] 🔴 **SLTU 修复 MUST 自带判据**（critic 第 1 轮：原稿「修 SLTU」是**无判据变更**）⇒ 新增 ctest 断言 `execute_alu(0, 0xFFFFFFFF, SLTU) == 1`（现 `grep SLTU test_pipeline_exec.cpp` **只命中常量定义**，零断言）
- [ ] 🔴 **静默地雷 MUST 同批修**：`examples/riscv-mini/tests/test_pipeline_exec.cpp:124` 以**字面量 `SLTU = 0x3`** 驱动 `Rv32iAlu`（老编码）⇒ ⇒ 归一后 `3` 变 **XOR** ⇒ ⇒ MUST 改为**符号常量**，并全仓扫「以字面量驱动 `alu_op`」的调用点
- [ ] ⚠️ **metis 订正 —— 主判据换掉**
- [ ] **差异清单**（人工读码产出）与差分结果**逐条对应**；清单外差异 = 0；清单 MUST 至少含 **SLTU 借位**（真缺陷）· **ADD 33 位宽度** · **`less_than_u` 与 `result` 内部矛盾** · `mem_size`/`mem_signed` 死输出 · `funct7` 双方都不消费 —— **缺 SLTU 视为清单不合格**
- [ ] 差分用例有**变异自证**（改内联一处 ⇒ 差分必红）

### 接入

- [ ] `ex_stage.h` 的算术核心由 `Rv32iAlu` 组件承担（内联的 10 个分支**已删**）
- [ ] `id_stage.h` 的字段与立即数由 `Rv32iDecoder` 组件承担（`selected_imm` 链**已删**，立即数**单一来源**）
- [ ] **ALU 操作码常量全仓唯一**（`ALU_OP_*` 只剩一份定义）
- [ ] ⚠️ 未知 opcode 的行为**零变化**（接入前后一致 ⇒ ⇒ 显式断言）
- [ ] 接入后**无组合环**（拓扑排序不报 `Cycle detected`）
- [ ] `Rv32iAlu::zero` 输出**不是悬空**（接上或显式标注不用）

### 退役

- [ ] `include/cpu/pipeline/rv32i_forwarding.h` **已删除**；全仓 grep **零引用**
- [ ] 其测试用例**逐条**处置（迁移 / 删除）并**登记**；迁移用例的期望值**手推**（禁抄仿真输出）
- [ ] `.rddf/checks/public-api-surface.md` 的「已移除」节 + `docs/CHANGELOG.md` 有本项的一行；`scripts/verify_public_api_visibility.sh` 全绿
- [ ] `HazardDetectionUnit` / `rv32i_hazard_complete.h` **逐个**有裁决记录（退役 / 保留 + 定位说明）

### 诚实化

- [ ] `include/cpu/AGENTS.md` 的「WHERE TO LOOK」对每个**保留但不接入**的组件**明写**「⚠️ 非流水线路径」；分支预测器**明写**「未接入 ⇒ 每个 taken 分支付 3 拍冲刷」
- [ ] 根 `AGENTS.md` 的 KNOWN LIMITATIONS **MUST NOT** 声称流水线由这些组件构成

### 不回归

- [ ] 既有 ctest 全绿（受影响用例的期望值 MUST **手推**并留痕）—— ⚠️ 数值 MUST 引用 **`CTEST_TOTAL` 锚点**，**禁**写死「257」（退役会**减少**用例数 ⇒ ⇒ 必须写成「`CTEST_TOTAL` + 新增 − 删除 == `ctest -N`」，且**缩面须 reviewer 显式签字**）
- [ ] `test_rv32i_soc_dual_parity` 的 **20 项 golden** 全绿（退休数 178 / load_use_stalls 6 / control_stalls 4 / store 5 全部不变 ⇒ ⇒ **证明接入是行为等价的**）
- [ ] 计数锚点同步（`CTEST_TOTAL` / `DUAL_BACKEND_PARITY_TOTAL` / `DUAL_BACKEND_PARITY_PAIRS` / `VERILATOR_E2E_TOTAL`）

---

## Capabilities

| capability | 动作 | 说明 |
|---|---|---|
| **`rv32i-component-unification`**（**新**） | ADDED | ⚠️ **metis 收窄为两条**（原稿四条里有一条**永远为红**）：① 有交付消费者的实现 MUST 有**独立 oracle**（手推表）断言 ② 无消费者的组件 MUST 在文档**明写**定位 + 退役 MUST 走公开面移除登记。<br>**删除**「同一语义 MUST NOT 有两份实现」（Simulator/codegen 同源、`chlib::ForwardingUnit` 与 `riscv::ForwardingUnit` 双份、立即数三处 ⇒ ⇒ 该条**恒红** ⇒ ⇒ 立一条不可满足的契约 = 制造永久技术债） |
| `rv32i-isa-semantics` | MODIFIED | 追加「未知 opcode 的行为 MUST 在组件重构前后**保持不变**」（⇒ ⇒ 接入不是行为变更） |
| `rv32i-data-forwarding` | MODIFIED | 追加「`riscv::ForwardingUnit` 退役 ⇒ 档位口径唯一 = `chlib::ForwardingMux` + ADR-047 D5」⇒ ⇒ 既有档位语义条款**不删** |
| `rv32i-canonical-home` | **不动**（引用） | 本项**不动** ADR-046 的「单源化」决策 ⇒ ⇒ 只做组件内部收口 |

---

## Impact

### 爆炸半径

| 面 | 影响 |
|---|---|
| `include/cpu/pipeline/stages/ex_stage.h` | 算术核心改由组件承担 ⇒ ⇒ **EX 级代码大幅缩短** ⇒ ⇒ 但**行为必须零变化**（C1 差分 + 20 项 golden 双重看守） |
| `include/cpu/pipeline/stages/id_stage.h` | 字段/立即数改由组件承担 ⇒ ⇒ 同上 |
| `include/cpu/pipeline/rv32i_alu.h` | ALU 编码常量**改值**（SUB 10→1 · SRA 11→7）⇒ ⇒ **破坏**既有手写常量的测试 ⇒ ⇒ 同步更新 |
| `include/cpu/pipeline/rv32i_decoder.h` | `instr_type` 的定位需澄清（是否使用）⇒ ⇒ 可能变成"输出但不用" ⇒ ⇒ 需处置 |
| `include/cpu/pipeline/rv32i_forwarding.h` | **删除** ⇒ ⇒ 公开面移除 ⇒ ⇒ 跨仓下游可能受影响 ⇒ ⇒ 登记 + CHANGELOG |
| 流水线**拓扑** | 组件接入 ⇒ ⇒ ctx 节点数变化 ⇒ ⇒ **产物**（.v）变化 ⇒ ⇒ 既有逐字节 golden 更新 |
| 组合环 | 接入组件 ⇒ ⇒ 拓扑变化 ⇒ ⇒ 必须验证无环（既有 `FATAL: Cycle detected` 防护）⇒ ⇒ 这是 ⑥ 的历史事故面 |
| 计数锚点 | `CTEST_TOTAL` 可能变（差分用例新增）⇒ ⇒ 同步 |

### 已知风险

| 风险 | 缓解 |
|---|---|
| **ALU 编码归一引入静默错** | C1 差分（先于归一）⇒ ⇒ 归一后 MUST 重跑差分 + 20 项 golden |
| **接入改变了「未知 opcode」的行为** | Acceptance 显式断言「未知 opcode 行为零变化」⇒ ⇒ C4 的裁决（标志仍由 opcode 比较）已规避 |
| **组合环**（历史事故面） | 纯组合组件 ⇒ ⇒ 接入后 MUST 跑拓扑排序 + 既有 `Cycle detected` 防护 |
| **退役 `ForwardingUnit` 破坏跨仓下游** | 走公开面移除登记 + CHANGELOG + 护栏 ⇒ ⇒ **MUST NOT** 静默删 |
| **差分测试变成"自证"**（两边都错得一样） | ⚠️ 差分只能证明「两者一致」，**不能**证明「两者都对」⇒ ⇒ **必须**与既有的 20 项**手推** golden 合看 ⇒ ⇒ Acceptance 明写这一点 |

### 与其他 improvement 的关系

| 项 | 关系 |
|---|---|
| `memory-timing-contract-2026-11` | ⚠️ **顺序**：本项在它**之后**做 ⇒ ⇒ 否则时序金标算两遍 |
| `port-naming-ratchet-2026-11` | ⚠️ **耦合**：本项改动的组件（`ex_stage.h` / `id_stage.h`）**被改动** ⇒ ⇒ 触发命名棘轮 P1 ⇒ ⇒ **顺带带名**（成本低）⇒ ⇒ 建议**首批消化**与本项合并 |
| `rtl-emit-product-tooling-2026-11` | 产物节点数变化 ⇒ ⇒ manifest 快照同步 |
| `rv32i-isa-gap-closure-2026-11` | 无耦合 ⇒ ⇒ 可并行；但 ISA 项若改了 ID 级 ⇒ ⇒ 建议 ISA 项在本项**之后**（避免 ID 级改两遍） |

## metis 第 1 轮 gap 分析与处置（2026-11-10）

> **轮次**：1（该 improvement 总计 1 轮）· **审查者**：metis（预规划 gap 分析）

### ✅ 已订正

| # | metis finding | 处置 |
|---|---|---|
| 1 | 🔴 **原稿事实错**：`funct7[5]` 门控在 `rv32i_pipeline.h:292-296`，**不在** `ex_stage.h`；且 `ex_stage.h` 的 `funct7` 端口**零消费者** ⇒ ⇒ 「差分 `funct7` 维度」是错方向 | A.2⑤ 重写 |
| 2 | 🔴 **`Rv32iAlu` 的 SLTU 是真缺陷**（不是「编码差异」）：`rv32i_alu.h:93` 的 `lt_unsigned_sig = bits<31,31>(sub_result)` ⇒ ⇒ `0 < 0xFFFFFFFF` 得 **0**（期望 **1**）⇒ ⇒ 且组件内部 `result`(SLTU) 与 `less_than_u` **自相矛盾** | A.2⑤ 追加；C1 的差异清单**必须含 SLTU**（缺 ⇒ 清单不合格） |
| 3 | 🔴 **原稿事实错**：`tests/chlib/test_forwarding.cpp` **不**消费 `riscv::ForwardingUnit` | A.1 表订正；C5 的「迁移该文件测试」**作废**（是空操作） |
| 4 | `rv32i_phase{1,2,3}_test` 是 `riscv_mini_unit_test` ⇒ **从不执行** | A.1 表订正 |
| 5 | 🔴 **「交付路径对 ALU 零覆盖」不成立**：`tests/test_rv32i_stage_dual.cpp:320-362` 用 **27 条手推期望**逐项断言 `ExStage` | A.1 表追加；W.1 相应收窄 |
| 6 | 仓内**已有**差分基础设施（`kExCases`(27) / `kDecCases`(17) / `ImmConsistencyDut`）⇒ ⇒ 本项若重造 = **第二份同型实现**（恰违反它想立的契约） | C1 改为「**复用**既有表 + 「等价物 MUST 逐行照抄 `ex_stage.h:136-208`」+ 差异清单必含 SLTU / ADD 33 位宽度 / `less_than_u` 内部矛盾 / 死输出 / `funct7` 双方都不消费」 |
| 7 | 新 capability 的「同一语义 MUST NOT 有两份实现」**永远为红**（Simulator/codegen 同源 · `chlib::ForwardingUnit` 与 `riscv::ForwardingUnit` 双份 · 立即数三处）⇒ ⇒ 立一条不可满足的契约 = 制造永久技术债 | Capabilities **收窄为两条**（独立 oracle 断言 / 无消费者组件须标明定位） |
| 8 | `riscv::ForwardingUnit` 「独有且被覆盖」的语义 = **空集**（WB 档无覆盖 · `id_reg_read` 死输入）⇒ ⇒ 迁移会把「EX 档前推 load 的访存地址」这一**已知错误模型**固化进新断言 | C5 改为**删除**（不是迁移）；组合时序覆盖改由「`HazardUnit` + `ForwardingMux` 组成 DUT」承担（该形态已有先例） |
| 9 | 退役会**减少** `DUAL_BACKEND_PARITY_PAIRS`(290) / `VERILATOR_E2E_TOTAL`(77) ⇒ ⇒ 缩面须**显式批准** | C5 加**缩面条款**（change 里明写「这是缩面」+ reviewer 签字）；验收从「257 全绿」改为「`CTEST_TOTAL` + 新增 − 删除 == `ctest -N`」 |
| 10 | `hazard_detection_unit.h` **include 了** forwarding ⇒ ⇒ C5 必须先于/伴随 C6，否则中间态编译不过 | C6 加**顺序约束** |
| 11 | C6「逐个裁决」是**无法验收**的口号 | C6 改为**三级可判定准则** + 按准则给出逐个结论 |
| 12 | 差分「一致」**不能**当主判据 | Acceptance 第一条改为「**既有手推表逐条保持全绿**」，差分**降级**为「差异清单的可执行化」且**不得**作为通过条件 |

### ⚠️ 待用户裁决

1. **C3/C4（ALU / Decoder 接入）是否本轮做**：metis 建议**拆成独立 change**（本轮只做 C5 + C7）—— 理由：零行为风险、唯一消费者是测试；且「接入」与 `memory-timing-contract` / `port-naming-ratchet` 有顺序耦合。⇒ ⇒ **建议**：本轮只做 C5（退役）+ C7（文档标记），C1–C4 另立项。
2. **C4 的 `instr_type` 二选一**：删端口（改 2 处测试）or 并入 ISA 项由 `instr_type` 派生 flags（⚠️ 后者会让 `OP_SYSTEM` 变成「像 R 型」⇒ ⇒ **会写 rd** ⇒ ⇒ 行为变更 ⇒ 须显式断言「`0x00000073` / `0x00001073` 在接入前后寄存器堆写使能逐拍相同」）。⇒ ⇒ **禁**「显式标注不使用」（`instr_type` 不是文档能救的死代码）。

## critic 第 1 轮（plan, build）—— **REJECT**（随整体）· mergeable: no · scope_exceeded: false

> **轮次**：1/2 · 审查者：critic

### 🔴 BLOCKER（**plan 可修**）：B0 的「零耦合 / 可全程并行」前提**不成立**

本文档 `:72` 自陈「`hazard_detection_unit.h` **include 了** forwarding ⇒ C5 MUST 先于/伴随 C6」。critic 实测确认：
`include/cpu/pipeline/hazard_detection_unit.h:19` `#include "rv32i_forwarding.h"`；两个测试 TU 间接受影响（`tests/test_rv32i_pipeline.cpp:28`、`tests/test_rv32i_pipeline_behavior.cpp:69`）。

⇒ ⇒ oracle 的 **B0 列表不含 C6** ⇒ 删掉 `rv32i_forwarding.h` 后 `cmake --build` **必红**。
**处置**：B0 的 ⑧ 项 MUST 写成「**C5 + C6 同批**（退役 + 同族处置/死 include 清理）」，否则并入 B1。

### ⚠️ SHOULD_FIX（已采纳）：C5 引 `ADR-047 D7` 是**错误类比**

| 事实 | 证据 |
|---|---|
| `ADR-047 D7` 的前提是「**零内部消费者**的预留端口」 | `docs/adr/ADR-047-*.md:223,245-246` |
| 而 `HazardUnit::mem_is_load` **有**内部消费者 | `hazard_unit.h:241`（`load_use_hazard = ex_load_use \| mem_load_use`） |
| 且被 2 个测试**直接驱动** | `tests/test_rv32i_pipeline.cpp:84,192` · `examples/riscv-mini/tests/test_pipeline_exec.cpp:607,632` |
| 且注释明文写「保留给独立使用场景」 | `hazard_unit.h:229-240,266-267` |

⇒ ⇒ oracle ⑦ 的「tie-off **或删端口**」把**爆炸半径差一个量级**的两选项并列 ⇒ **MUST 二选一收口**：
- **推荐 tie-off**（装配点 `hazard.io().mem_is_load <<= ch_bool(false)`）⇒ 等价现状「保持 0」，且**顺手消除 4 态 X 传播**；零公开面变更。
- 删端口 ⇒ **必须**走公开面移除登记 + 跨仓下游通知。

### ⚠️ SHOULD_FIX（已采纳）：⑧「共用 ALU 操作码常量（零行为变更）」对 `Rv32iAlu` **不是零变更**，且 SLTU 修复**无断言覆盖**

- `rv32i_alu.h:25-34`（SUB=10、SRA=11）vs `ex_stage.h:34-43`（SUB=1、SRA=7）⇒ 归一后组件版改值 ⇒ **破坏** `rv32i_phase{1,2}_test` 的手写常量 ⇒ 「零行为变更」仅对**交付路径**成立。
- 🔴 **静默地雷**：`examples/riscv-mini/tests/test_pipeline_exec.cpp:124` 以**字面量 `SLTU = 0x3`** 驱动 `Rv32iAlu`（老编码）⇒ 归一后 `3` 变 **XOR**。
- SLTU 修复（`rv32i_alu.h:93`）**无任何 ctest 断言 SLTU 结果**（`grep SLTU test_pipeline_exec.cpp` 只命中常量定义）⇒ 属「**无判据变更**」。
**处置**：MUST 加 `ctest` 断言 `execute_alu(0, 0xFFFFFFFF, SLTU) == 1`；并把 `test_pipeline_exec.cpp:124` 的字面量改为符号常量。
