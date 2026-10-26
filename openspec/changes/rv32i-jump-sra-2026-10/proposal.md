# 提案：RV32I 的 `JAL`/`JALR` MUST 重定向 PC 并写回返回地址；`SRA` MUST 是算术右移

## Why

2026-10-26 的 roadmap 对齐审计实测挖出 **两个互不耦合** 的语义缺口。

### 缺口 A：`JAL`/`JALR` 完全不跳，写回值也不是 `pc + 4`

事实链（逐条可核）：

1. `include/cpu/pipeline/rv32i_decoder.h:46-47,135,145` **已**解码 `OP_JAL` /
   `OP_JALR`（`imm_j` / `imm_i`）⇒ **缺口不在 decoder**；
2. `include/cpu/pipeline/stages/id_stage.h:201,265` **已**产出 `idex_is_jump`
   （`is_jal | is_jalr`）并一路传到 WB（`rv32i_pipeline.h:318,344,359`）
   ⇒ 「这是一条跳转指令」这个信息**是有的**；
3. ⚠️ `include/cpu/pipeline/stages/ex_stage.h:214`
   `branch_taken_sig = select(io().is_branch, branch_cond, false)` ——
   `is_branch` **只在 `OP_BRANCH` 时为真** ⇒ `ex_stage.io().branch_taken` 对
   `JAL`/`JALR` **恒为 false**；
4. `rv32i_pipeline.h:180` `if_stage.io().branch_valid <<= ex_stage.io().branch_taken;`
   ⇒ **PC 永不重定向** ⇒ **任何子程序调用都坏**；
5. ⚠️ 写回值也错：`wb_stage.h:81`
   `wb_data = select(io().is_alu | io().is_jump, io().alu_result, wb_data)`
   ⇒ `JAL`/`JALR` 写回 **ALU 的垃圾结果**（`rs1 + imm`）⇒ `ra`（x1）不是返回地址；
6. ⚠️ 即便补上 (3)，`JALR` 的目标地址算法也不对：`ex_stage.h:178`
   `branch_target_calc = io().pc + io().imm` —— `JALR` 应是 `(rs1 + imm) & ~1`。

⇒ 三处**缺一不可**：重定向信号 / 目标地址算法 / 写回值。

### 缺口 B：`SRA` 退化为 `SRL`

- `include/cpu/pipeline/stages/ex_stage.h:134` `ch_uint<32> sra_result = srl_result;`
  —— **pipeline 实际用的**内联 ALU；
- ⚠️ **第二处**：`include/cpu/pipeline/rv32i_alu.h:103`
  `auto sra_result = srl_result;  // TODO: 完善算术右移` —— 独立的 `Rv32iAlu` 组件
  ⇒ **两处都退化** ⇒ 只修一处会留下「组件层对、流水线层错」的分裂。

⚠️ 框架层**没有**算术右移原语：`include/core/operators_shift.h` 里
`operator>>>`（`sshr_op`）是**被注释掉**的 ⇒ MUST 在 ALU 层用
「逻辑右移 + 符号位填充」实现。

**为什么现在**：① 字节道写（`rv32i-store-byte-enable-2026-10`）**已归档**
⇒ capability `rv32i-isa-semantics` **已存在** ⇒ 本 change 的 delta **追加**到它；
② C3 的双后端逐拍 parity + 32 端口观测面已就位 ⇒ 控制流改动可被逐拍对照；
③ 现有固件**不含** `JAL`/`JALR`/`SRA` ⇒ **既有 golden 不会因「修好」而变**
（只会因「新增用例」而变）⇒ 回归面被既有断言钉住。

## What Changes

### 0. 两个**前置修复**（实施中实测挖出的既有缺陷；无它们则 JAL/JALR/SRA 的用例无法构造）

这两条**不在**原始立项范围内，是实施 `JAL`/`JALR`/`SRA` 时**被固件用例逼出来的**。
保留它们在本 change 而非另开 change 的理由：两者都是**一行级**修复，
且分别是「用 `AUIPC`+`ADDI` 把任意固件地址装进寄存器」（构造 `JALR` 目标的**标准做法**）
与「用 `ADDI` 造负数源」的**直接前提**。

> ⚠️ **如实声明（2026-10-27 第 1 轮 critic NIT #1）**：它们**不是**「没有就写不出用例」——
> `sub x21, x0, x2`（R 型 SUB，门控下本就正确）可以造负数源，`slli` 可以合成大基址。
> 保留在本 change 的真实理由是：二者都是**同 capability 内的一行级真缺陷**，
> 且本 change 的 spec delta 已为它们各写一条独立 Requirement ⇒ 捆版**不构成范围扩张**。

#### 0a. U 型立即数解码（`include/cpu/pipeline/stages/id_stage.h`）

- 原实现：`instr_masked = instruction & 0x000FFFFF`（**20 位**掩码）再 `<< 12`
  ⇒ 把 **`rd` / `opcode` 位也算进了立即数**（实测 `auipc x18, 0` ⇒ `pc + 0x917000`，
  其中 `0x917 = rd<<7 | opcode`）
- 正确：`imm[31:12] = instr[31:12]`（**低 12 位 MUST 清零**）
- ⚠️ 此前**无任何覆盖**：唯一消费者是仓外的 riscv-tests 二进制（CI 全 SKIP）
  ⇒ `LUI` / `AUIPC` **一直是错的**

#### 0b. `alu_op` 的 SUB/SRA 判定对 I 型的误判（`include/cpu/pipeline/rv32i_pipeline.h`）

- 原实现：`alu_is_sub_sra = funct7[5]`（= `instr[30]`），**对所有 opcode 都生效**
- ⚠️ 对 **I 型**（`ADDI` / `ANDI` / …）而言 `instr[30]` 是 **`imm[10]`** 的一部分
  ⇒ **任何 `imm >= 0x400` 或负数都被误判成 SUB**
  - 实测：`addi x21, x0, -8`（`0xFF800A93`）⇒ `0 - (-8)` = **8**
  - 实测：`addi x21, x0, 0x7FF`（`0x7FF00A93`）⇒ `0 - 0x7FF` = **0xFFFFF801**
- 正确：MUST 用「**R 型** 或 **`OP_OPIMM` 且 `funct3=101`（SRAI）**」门控
- ⚠️ 本固件原有 imm 全部 `< 0x400`（`0x100` / `0x122`）⇒ 侥幸未暴露；
  但**大偏移的 `lw`/`sw` / 负数立即数**一直受影响

### 1. `ex_stage` 的重定向语义（`include/cpu/pipeline/stages/ex_stage.h`）

- `branch_target` 语义从「条件分支目标」扩展为「**重定向目标**」：
  `JAL` = `pc + imm_j`；`JALR` = `(rs1 + imm_i) & ~1`；条件分支保持 `pc + imm_b`
- `branch_taken` 对 `JAL`/`JALR` **恒 true**（无条件跳）
- **新增输出端口** `link_value`（= `pc + 4`）
- ⚠️ `JALR` 的 `rs1` 取 `io().rs1_data` ⇒ **无数据前推**（ADR-046 **D5** 悬置）
  ⇒ `JALR` 的 `rs1` 若由前一条指令写入 ⇒ 读到**旧值** ⇒ **已知限制**（如实登记）

### 2. `wb_stage` 的写回选择（`include/cpu/pipeline/stages/wb_stage.h`）

- **新增输入端口** `link_value`
- 写回选择改为**三级**：`load` ⇒ `mem_data`；`jump` ⇒ `link_value`；
  `alu` ⇒ `alu_result`（原先 `is_alu | is_jump` 合并 ⇒ `jump` 拿到 ALU 结果）
- ⚠️ **新增输入端口 ⇒ 全部实例化点 MUST 显式驱动**（未驱动的输入端口读到的是
  **全 1** ⇒ 写回 `0xFFFFFFFF`）⇒ `examples/riscv-mini/tests/test_pipeline_debug.cpp`
  的 `WbStageTop` MUST 显式驱动

### 3. 流水线接线（`include/cpu/pipeline/rv32i_pipeline.h`）

- 新增 2 个流水线寄存器 `exmem_link_value` / `memwb_link_value` + 接线
- `hazard.io().ex_branch <<= id_stage.io().is_branch | id_stage.io().is_jump;`
  ⇒ `JAL`/`JALR` 也 **flush 错误路径**（否则 PC 跳了、错误路径的指令却继续执行）
- ⚠️ `if_stage` 的 PC 选择器**不新增第三路** —— **复用**现有 `branch_valid` /
  `branch_target` 两路 ⇒ `if_stage.h:88-96` 记录的隐式不变量**结构不变**；
  但「`ex_branch` 的成分」从「仅条件分支」扩展为「条件分支 + 跳转」
  ⇒ MUST **更新该不变量的文字**（plan 要求的「flush 与 jump 的互斥论证」）

### 4. `SRA` 真算术右移（**两处**）

- `ex_stage.h`（内联 ALU，pipeline 实际用）+ `rv32i_alu.h`（`Rv32iAlu` 组件）
- 算法：`sra = (a >> sh) | (sign ? (0xFFFFFFFF << (32 - sh)) : 0)`，`sh == 0` 时**不填充**
- ⚠️ `ch_uint` 无符号 ⇒ MUST **显式**取符号位（MUST NOT 直接 `>>`）

### 5. 测试（`examples/riscv-mini/tests/test_rv32i_soc_dual_parity.cpp`）

- 固件**尾部**追加（既有 word 索引一个都不移动）：`JAL` 跳到子程序 → 子程序写寄存器
  → 返回 → 主程序读回；`SRA` 用例（`-8 >> 1`）
- golden 重算：写回序列 / 停顿拍数 / 退休数 / `taken` 冲刷位置
- 变异验证：SRA 改回 SRL ⇒ 转红；`JAL` 目标接成 `pc + 4` ⇒ 转红；
  `link_value` 接成 `pc + imm` ⇒ 转红

## Acceptance

- [ ] `JAL` **真的跳**到子程序且能**返回**（子程序写的寄存器值被主程序读回）
- [ ] `ra`（x1）== 返回地址（`JAL` 指令的 `pc + 4`）
- [ ] `JALR` 跳到 `(rs1 + imm) & ~1`
- [ ] `SRA` 对负数正确（`-8 >> 3 == -1`，即 `0xFFFFFFF8 → 0xFFFFFFFF`）
- [ ] **前置修复 0a**：`LUI` / `AUIPC` 的立即数 = `instr[31:12] << 12`
      （`auipc x18, 0` ⇒ `pc + 0`）
- [ ] **前置修复 0b**：I 型的大立即数/负数 MUST NOT 被误判为 SUB
      （`addi x21, x0, -8` ⇒ `0xFFFFFFF8`；`addi x, x0, 0x7FF` ⇒ `2047`）
- [ ] **两处** `SRA` 实现（`ex_stage` 内联 + `Rv32iAlu` 组件）都正确
- [ ] 既有固件的 golden 除「新增用例导致的变化」外**不变**
- [ ] 双后端逐拍全等（无掩码）
- [ ] 变异验证 ≥ 3 条**全被抓**
- [ ] `wb_stage` 的全部实例化点显式驱动 `link_value`
- [ ] `ctest -LE perf` 零回归
- [ ] **对抗审稿**（critic）+ `openspec archive`

## Non-Goals

- 不接**数据前推**（ADR-046 **D5** 悬置 ⇒ `JALR` 的 `rs1` 无前推 ⇒ 如实登记）
- 不引入**分支预测**（`rv32i_branch_predict.h` 未接入流水线）
- 不改框架层 `ch_mem` / 不启用 `operator>>>`（算术右移原语仍是注释状态）
- 不改 ADR-046 **D10** 的 SoC 32 端口契约（本 change 不新增顶层端口）
- 不做非对齐 `JALR` 目标以外的异常处理（RISC-V 允许实现自定义）
