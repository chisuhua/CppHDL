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

## 审稿记录（2026-10-27 · critic 第 1 轮）

**结论**：**REJECT**（`mergeable: no` / `scope_exceeded: false`）——
**1 个 BLOCKER** + 2 个 SHOULD_FIX + 2 个 NITS。

| # | finding | 处置 |
|---|---|---|
| **BLOCKER 1** | `ex_stage` 用**裸 `opcode`** 判 `jump_unconditional`，而 `idex_opcode`/`idex_pc`/`idex_imm` 在**气泡拍不清零** ⇒ **错误路径上的 `JAL`/`JALR`** 被锁进 ID/EX 后，下一拍气泡仍拉高 `branch_taken`，而此时 `ex_branch = is_branch \| is_jump = 0` ⇒ **`flush = 0` ⇒ 无人拦** ⇒ **静默劫持 PC** | ✅ `ex_stage` **新增输入 `is_jump`**（接 `idex_is_jump`，**已气泡清零**）⇒ `jump_unconditional = io().is_jump`；`is_op_jalr` 也与 `is_jump` 相与。**结构收益**：重定向判定与 `hazard` 的 flush 判定**同源** ⇒ 「`branch_taken=1` 但 `ex_branch=0`」**不再可达**。3 个实例化点（pipeline / `ExStageTop` / `test_pipeline_exec`）全部显式驱动 |
| SHOULD_FIX 1 | `x19`/`x23` 的行为级断言排在**派生计数器之后** ⇒ `control_stalls` 抢先失败（Catch2 `REQUIRE` 中止 TEST_CASE）⇒ 它们**从未被执行** ⇒ 丧失独立判别力 | ✅ 整块**移到** `store_cycles` 之前，并在注释里写明这条**位置纪律** |
| SHOULD_FIX 2 | `control_stalls` 注释称「停顿**拍数**」，实现是**事件数**（每次 +1，实际代价 3 拍） | ✅ 注释订正为「**重定向次数**」+ spec delta 加 Scenario 写明口径 |
| NIT 1 | `design.md` D8/D9 与 `proposal.md` §0 的「缺一不可 / 根本写不出来」**言过其实**（`sub` 可造负数源、`slli` 可合成基址） | ✅ 措辞降级为「**标准做法**的前提」+ 如实声明「不是没有就写不出用例」；保留理由是「同 capability 内的一行级真缺陷 + 各有独立 Requirement」 |
| NIT 2 | `branch_count` 只统计 `is_branch` ⇒ spec 未写明口径 | ✅ spec delta 加 Scenario 写明三个计数器的口径；测试侧加注释 |

### BLOCKER 的最小复现与「先红后绿」证据

⚠️ **初版固件抓不到它**（这是本 change 最大的教训）：
- 错误路径放 `JAL(27, 8)` ⇒ 残留的 `pc + imm` **恰好**给出正确目标 ⇒ **巧合通过**
- 改成 `JAL(27, 20)` ⇒ 仍落在正确路径附近 ⇒ 仍绿
- 最终用 **`JALR(27, 100(x0))`**（目标 = **低地址 `100`**）⇒ 缺陷暴露成「**程序重启**」
  ⇒ `store_cycles == 5` 断言转红（**修复前红 / 修复后绿**，实测）

⇒ 该用例已作为**回归用例**永久保留在固件里（word 100-102）。

### 变异验证（整改后 6/6 + **M7 的判别力边界如实记录**）

原有 6 条（M1–M6）**全部仍被抓**（SRA×2 / `JAL` 目标 / `ex_branch` 去 `is_jump` /
`link_value` / `alu_op` 门控）。

**M7（BLOCKER 的回归护栏）—— 判别力有边界，如实记录**：

| 变异形态 | 结果 | 解释 |
|---|---|---|
| **M7a**：只把 `jump_unconditional` 改回裸 opcode（保留 `is_op_jalr & io().is_jump`） | ❌ **不红** | ⚠️ **真实机理未查明**（见下） |
| **M7b**：**两处都还原**（= 修复前的实现） | ✅ **红**（`store_cycles == 11`，期望 5 ⇒ 程序被劫持重启） | 这才是 BLOCKER 的真实形态 |

⇒ **结论**：修复的判别力由 **M7b** 承担（= 修复前的实现，实测红）；
M7a 不红 ⇒ **「单改一处不足以触发」**。

⚠️ **如实声明（2026-10-27 第 2 轮 critic NIT #1）**：M7a 不红的**机理未查明**。
曾据「`is_op_jalr` 门控把 `branch_target` 退回 `pc + imm`」推测它「落在正确目标附近」——
但实测 dump 显示 **M7a 的 trace 与修复版逐拍完全一致**（word 101 在 ID 三拍、
`branch_taken` 恒 1），而这个推测**没有被任何实验证明**。
（`pc + imm` = `0x80000194 + 100` = word **126**，已越过 108 字固件末端，
并非「正确目标附近」⇒ 该推测本身就不自洽。）
⇒ 记录到此精度：**M7a 不红是实测事实，机理未定**；不放宽为「冗余保险成立」的结论。

⚠️ 另两条教训：
① **变异脚本「只还原一处」会低估修复的必要性** ⇒ 报告变异结果时 MUST 说清「还原了几处」；
② 变异脚本 MUST **校验 python 片段真的替换成功**（本 change 的 `mutate()` 用 `assert` 保证，
   但更稳的做法是替换后 grep 一次）。
