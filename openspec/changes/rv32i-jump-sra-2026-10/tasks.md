# Tasks: RV32I 跳转重定向与算术右移（`JAL` / `JALR` / `SRA`）

## Phase 1: 失败断言先行

- [ ] T1.1 固件尾部追加 `JAL` → 子程序 → `JALR` 返回的用例（既有 word 索引一个都不移动）
- [ ] T1.2 固件尾部追加 `SRA` 用例（`-8 >> 1` ⇒ `0xFFFFFFFC`）
- [ ] T1.3 编码器自校验：`JAL` / `JALR` / `SRA` 的机器码字面量
- [ ] T1.4 **确认失败**：在实现之前，上述断言 MUST 是红的（否则断言无判别力）

## Phase 2: 实现

- [ ] T2.1 `ex_stage.h`：`branch_target` 语义扩展（`JAL` = `pc + imm_j`、
      `JALR` = `(rs1 + imm_i) & ~1`）+ `branch_taken` 对 `JAL`/`JALR` 恒 true
- [ ] T2.2 `ex_stage.h`：**新增输出** `link_value`（= `pc + 4`）
- [ ] T2.3 `wb_stage.h`：**新增输入** `link_value` + 写回选择改三级
      （`load` ⇒ `mem_data`；`jump` ⇒ `link_value`；`alu` ⇒ `alu_result`）
- [ ] T2.4 `rv32i_pipeline.h`：新增 `exmem_link_value` / `memwb_link_value`
      两个流水线寄存器 + 接线
- [ ] T2.5 `rv32i_pipeline.h`：`hazard.io().ex_branch <<= id_stage.io().is_branch | id_stage.io().is_jump;`
      ⇒ `JAL`/`JALR` 也 flush 错误路径
- [ ] T2.6 `if_stage.h`：更新隐式不变量的文字（「`ex_branch` 的成分」从
      「仅条件分支」扩展为「条件分支 + 跳转」；**不新增第三路 PC 源** ⇒ 结构不变）
- [ ] T2.7 `ex_stage.h`（内联 ALU）+ `rv32i_alu.h`（`Rv32iAlu` 组件）：
      `SRA` 改**真算术右移**（逻辑右移 + 符号位填充；`sh == 0` 不填充）
- [ ] T2.8 `examples/riscv-mini/tests/test_pipeline_debug.cpp`：`WbStageTop`
      **显式驱动**新增的 `link_value` 输入端口（未驱动 = 全 1 ⇒ 写回 `0xFFFFFFFF`）

## Phase 3: golden 重算

- [ ] T3.1 写回序列 golden（含 `ra` == 返回地址这条新断言）
- [ ] T3.2 停顿拍数 / 退休数 / `taken` 冲刷位置的 golden 重算
- [ ] T3.3 反向断言：SRL 的结果（`0x7FFFFFFC`）MUST NOT 出现

## Phase 4: 验收

- [ ] T4.1 `ctest -LE perf` 零回归（191/191）
- [ ] T4.2 `scripts/verify_test_counts.sh` 绿（测试源数**未变** ⇒ `CTEST_TOTAL` 不变）
- [ ] T4.3 `openspec validate --strict` 通过
- [ ] T4.4 变异验证 ≥ 3 条**全被抓**：
      ① `ex_stage` 的 `SRA` 改回 `SRL` ② `rv32i_alu.h` 的 `SRA` 改回 `SRL`
      ③ `JAL` 目标接成 `pc + 4` ④ `link_value` 接成 `pc + imm`
      ⑤ `JALR` 的 `& ~1` 去掉
- [ ] T4.5 双后端逐拍全等（interpreter + verilator，无掩码）
- [ ] T4.6 **对抗审稿**（critic，第 1 轮）
- [ ] T4.7 **对抗审稿第 2 轮**（复审：只提交「第 1 轮 finding 闭合证据 + 新增 diff」）
- [ ] T4.8 `openspec archive`

## Out of Scope

- 不接**数据前推**（ADR-046 **D5** 悬置 ⇒ `JALR` 的 `rs1` 无前推 ⇒ 如实登记）
- 不引入**分支预测**（`rv32i_branch_predict.h` 未接入流水线）
- 不改框架层 `ch_mem` / 不启用 `operator>>>`（算术右移原语仍是注释状态）
- 不改 ADR-046 **D10** 的 SoC 32 端口契约（本 change 不新增顶层端口）
- 不实现地址不对齐异常（RISC-V 允许实现自定义非对齐行为）
