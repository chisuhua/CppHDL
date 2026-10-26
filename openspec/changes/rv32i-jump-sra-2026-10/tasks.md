# Tasks: RV32I 跳转重定向与算术右移（`JAL` / `JALR` / `SRA`）

## Phase 1: 断言（判别力由变异验证承担）

- [x] T1.1 固件尾部追加 `JAL` → 子程序 → `JALR` 跳转的用例（既有 word 索引**一个都不移动**，64 → 100 字）
- [x] T1.2 固件尾部追加 `SRA` 用例（`-8 >> 3` ⇒ `0xFFFFFFFF`；移位 0 ⇒ 不变）
- [x] T1.3 编码器自校验：`JAL` / `JALR` / `AUIPC` / `SRA` 的机器码字面量
      （`0x00C0076F` / `0x000908E7` / `0x00000917` / `0x402ADB33`）
- [x] T1.4 ⚠️ **如实记录**：本 change **没有**严格走「先写断言 → 确认红 → 再实现」
      （断言与实现同批；`SRA` 的 ALU 级断言是「实现后立即验证」）。
      判别力改由 **6 条变异验证**承担（见 T4.4）—— 每条变异都**先有断言**才被抓到，
      其中 M3/M4/M5/M6 的断言正是在变异前写好的。

## Phase 2: 实现

- [x] T2.0a **前置修复（既有缺陷，实施中被用例逼出）**：U 型立即数
      （`id_stage.h` 的 20 位掩码 ⇒ `instr[31:12]`）⇒ `LUI`/`AUIPC` 修好
- [x] T2.0b **前置修复（既有缺陷）**：`alu_op` 的 SUB/SRA 判定 MUST 只对
      **R 型 与 SRAI** 生效（`funct7[5]` 对 I 型是 `imm[10]`）⇒ 负数/大立即数修好
- [x] T2.1 `ex_stage.h`：`branch_target` 语义扩展（`JAL` = `pc + imm_j`、
      `JALR` = `(rs1 + imm_i) & ~1`）+ `branch_taken` 对 `JAL`/`JALR` 恒 true
- [x] T2.2 `ex_stage.h`：**新增输出** `link_value`（= `pc + 4`）
- [x] T2.3 `wb_stage.h`：**新增输入** `link_value` + 写回选择改三级
      （`load` ⇒ `mem_data`；`jump` ⇒ `link_value`；`alu` ⇒ `alu_result`）
- [x] T2.4 `rv32i_pipeline.h`：新增 `exmem_link_value` / `memwb_link_value`
      两个流水线寄存器 + 接线
- [x] T2.5 `rv32i_pipeline.h`：`hazard.io().ex_branch <<= id_stage.io().is_branch | id_stage.io().is_jump;`
      ⇒ `JAL`/`JALR` 也 flush 错误路径
- [x] T2.6 `if_stage.h`：更新隐式不变量的文字（「`ex_branch` 的成分」从
      「仅条件分支」扩展为「条件分支 + 跳转」；**不新增第三路 PC 源** ⇒ 结构不变）
- [x] T2.7 `ex_stage.h`（内联 ALU）+ `rv32i_alu.h`（`Rv32iAlu` 组件）：
      `SRA` 改**真算术右移**（逻辑右移 + 符号位填充；`sh == 0` 不填充）
- [x] T2.8 `examples/riscv-mini/tests/test_pipeline_debug.cpp`：`WbStageTop`
      **显式驱动**新增的 `link_value` 输入端口（未驱动 = 全 1 ⇒ 写回 `0xFFFFFFFF`）
- [x] T2.9 `rv32i_pipeline.h`：**性能计数器语义分离** —— `branch_mispredict`
      只统计**条件分支**（加 `is_branch` 门控），`control_stalls` 统计**全部重定向**
      （含 `JAL`/`JALR`）⇒ 两者从此分离（此前同源只因固件只有条件分支）

## Phase 3: golden 重算

- [x] T3.1 写回序列 golden 13 → **22** 条（含 `x14`/`x17` == 返回地址、`x16`/`x20` == 子程序体）
- [x] T3.2 停顿拍数 / 退休数 golden 重算：`non_retired` **11 → 17**（两个跳转各 +3 拍，
      与条件分支冲刷同构）、`instructions` `188 → 182`、`control_stalls` **1 → 3**
      （`branch_mispredict` 仍为 **1** ⇒ 跳转不污染「误预测」）
- [x] T3.3 反向断言：`x19` / `x23`（`JAL`/`JALR` 之后的**错误路径**）MUST NOT 被写回
- [x] T3.4 既有 golden **零回归**（前 13 条写回、`store_cycles == 5`、`hazards == 2`、
      `x8_writes == 1`、pc↔指令对齐全部不变）

## Phase 4: 验收

- [x] T4.1 `ctest -LE perf` 零回归（191/191）
- [ ] T4.2 `scripts/verify_test_counts.sh` 绿（测试源数**未变** ⇒ `CTEST_TOTAL` 不变）
- [ ] T4.3 `openspec validate --strict` 通过
- [x] T4.4 变异验证 **6/6 全被抓**（每条都**先编译成功**再跑，避免「跑了旧二进制」的假绿）：
      ① `ex_stage` 的 `SRA` 改回 `SRL` ② `rv32i_alu.h` 的 `SRA` 改回 `SRL`
      ③ `JAL` 目标接成 `pc + 4` ④ `ex_branch` 去掉 `is_jump`（不 flush）
      ⑤ `link_value` 接成 `pc + imm` ⑥ `alu_op` 的 `funct7[5]` 门控去掉
      ⚠️ 原计划的「`JALR` 的 `& ~1` 去掉」**未做** —— 固件里的 `JALR` 目标
      （`0x8000014C`）本来就是偶数 ⇒ 该变异**无判别力**（去掉也不变）⇒ 如实跳过。
- [x] T4.5 双后端逐拍全等（interpreter + verilator，27 个观测值**无掩码**）
- [ ] T4.6 **对抗审稿**（critic，第 1 轮）
- [ ] T4.7 **对抗审稿第 2 轮**（复审：只提交「第 1 轮 finding 闭合证据 + 新增 diff」）
- [ ] T4.8 `openspec archive`

## Out of Scope

- 不接**数据前推**（ADR-046 **D5** 悬置 ⇒ `JALR` 的 `rs1` 无前推 ⇒ 如实登记）
- 不引入**分支预测**（`rv32i_branch_predict.h` 未接入流水线）
- 不改框架层 `ch_mem` / 不启用 `operator>>>`（算术右移原语仍是注释状态）
- 不改 ADR-046 **D10** 的 SoC 32 端口契约（本 change 不新增顶层端口）
- 不实现地址不对齐异常（RISC-V 允许实现自定义非对齐行为）
