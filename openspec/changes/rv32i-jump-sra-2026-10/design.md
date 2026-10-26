# Design: RV32I 跳转重定向与算术右移

## 1. 数据通路（改动后）

```
                    ID 级                          EX 级                    MEM/WB → WB
┌────────────────────────────────────────────────────────────────────────────────────────────┐
│ decode: opcode ✓  imm ✓  rd ✓                          opcode ✓  pc ✓  rs1_data ✓         │
│         is_jump ✓                                       link_value = pc + 4  (新增输出)     │
│                                                            │                                  │
│                                          ┌─────────────────┴──────────────────┐            │
│                                          │ 重定向目标（branch_target 语义扩展）│            │
│                                          │  OP_BRANCH ⇒ pc + imm_b            │            │
│                                          │  OP_JAL    ⇒ pc + imm_j            │            │
│                                          │  OP_JALR   ⇒ (rs1 + imm_i) & ~1   │            │
│                                          └─────────────────┬──────────────────┘            │
│                                                            │                                  │
│                                          重定向成立（branch_taken 语义扩展）              │
│                                          条件分支 ⇒ branch_cond                            │
│                                          OP_JAL / OP_JALR ⇒ **恒 true**                    │
│                                                            │                                  │
│                        ┌───────────────────────────────────┼──────────────────────────┐       │
│                        │                                   │                          │       │
│                        ▼                                   ▼                          │       │
│   if_stage.branch_valid ◄────────────────────────────────┘                          │       │
│   if_stage.branch_target ◄────────────────────────────────┘                          │       │
│        └─► if_stage 的 PC 选择器（**仍是两路**：branch_valid ? branch_target : pc+4)   │
│                                                                                       │
│   hazard.ex_branch ◄── id_stage.is_branch | id_stage.is_jump  （**扩展** ⇒ JAL/JALR    │
│                                                     也 flush 错误路径）               │
│   hazard.ex_branch_taken ◄── ex_stage.branch_taken                                    │
│        └─► hazard.flush = ex_branch & ex_branch_taken                                  │
│                                                                                       │
│   ex_stage.link_value ──► exmem_link_value ──► memwb_link_value ──► wb_stage.link_value │
│                                                    （**新增** 2 个流水线寄存器）        │
│                                        └─► wb_data = is_jump ? link_value : ...        │
│                                             （= pc + 4 = 返回地址）                  │
└────────────────────────────────────────────────────────────────────────────────────────────┘
```

### SRA（两处，同一算法）

```
  sh   = operand_b[4:0]                       // 5 bit
  srl  = a >> sh                              // 逻辑右移（ch_uint 无符号 ⇒ 天然逻辑）
  sign = a[31]                                // 显式取符号位（MUST NOT 直接 >>）
  fill = sign & (sh != 0) ? (0xFFFFFFFF << (32 - sh)) : 0
  sra  = srl | fill
```

⚠️ `sh == 0` 时**不填充**（否则 `32 - 0 = 32` ⇒ 移位越界）。
⚠️ `sh ∈ [1,31]` ⇒ `32 - sh ∈ [1,31]` ⇒ **不触越界**。

## 2. 关键决策

### D1：`if_stage` **不新增第三路** PC 源 —— **复用**已有的条件分支通路

plan 的实现要点写的是「`if_stage` 增加第三路 PC 源（`jump_valid`/`jump_target`），
选择器扩为三路」。**本设计偏离该建议**，理由：

- `if_stage.h:88-96` 记录了一条**隐式不变量**：
  「flush 的**唯一**来源是 `hazard.io().flush = ex_branch & ex_branch_taken`，
  而 `next_pc_target` 的选择条件 `branch_valid` 接的**正是同一个**
  `ex_stage.io().branch_taken` ⇒ flush 为真时 `next_pc_target` **已经是**分支目标
  ⇒ PC 无需额外覆盖。」
- 新增第三路 ⇒ 引入**第二个**无条件跳转来源 ⇒ 该不变量必须**重新论证**
  （`jump_valid` 为真但 `branch_valid` 为假时，flush 与 PC 重定向**不同源**
  ⇒ `pc_update_en` 不看 `flush` 的推理链断裂）。
- **复用** ⇒ 不变量的**结构完全不变**（flush 仍只来自 `ex_branch & ex_branch_taken`，
  `branch_valid` 仍接同一个 `ex_stage.branch_taken`）⇒ 只需把「`ex_branch` 的成分」
  从「仅条件分支」更新为「条件分支 + 跳转」。

⇒ **flush 与 jump 的互斥论证（更新后的不变量）**：

> flush 的唯一来源仍是 `hazard.io().flush = ex_branch & ex_branch_taken`，
> 其中 `ex_branch = id_stage.is_branch | id_stage.is_jump`，
> `ex_branch_taken = ex_stage.branch_taken`（含 `JAL`/`JALR` 的恒 true）。
> `if_stage.branch_valid` 接的**正是** `ex_stage.branch_taken` ⇒
> **flush 为真 ⇒ `branch_valid` 为真 ⇒ `next_pc_target` 已是重定向目标**
> ⇒ `pc_update_en` 无需看 `flush` 仍然安全。
> 且 `ex_branch` 的两个成分**互斥**（一条指令不可能同时是 `OP_BRANCH` 与
> `OP_JAL`/`OP_JALR`）⇒ 不存在「两个来源同时成立」的情形。
> ⚠️ **flush 与 stall 仍不会同拍**（沿用既有论证）：flush 由 **EX 级**的
> `branch_taken` 产生 ⇒ 触发那一拍 EX 级只有那一条指令 ⇒ 它不是 load
> ⇒ `ex_is_load` 为假 ⇒ load-use 检测不可能同拍成立。

### D2：`JALR` 的目标 = `(rs1 + imm) & ~1`

- RISC-V 规定 `JALR` 目标 = `(rs1 + imm[11:0])`，且**最低位清零**（`& ~1`）
  ⇒ 目标必为 **2 字节**对齐（`imm` 是偶数 ⇒ 实际是 4 字节对齐，因为 `imm` 的
  bit0 恒为 0）。
- ⚠️ `rs1` 取 `ex_stage.io().rs1_data` ⇒ **无数据前推**（ADR-046 **D5** 悬置）
  ⇒ `JALR` 的 `rs1` 若由**前一条**指令写入 ⇒ 读到**旧值**
  ⇒ **已知限制**：固件 MUST NOT 依赖「`JALR` 的 `rs1` 刚被写」

### D3：返回地址走**新增**的 `link_value` 通路（不复用 `alu_result`）

- 现有 `wb_stage` 的写回只有三个来源（`mem_data` / `alu_result` / 隐含 0）
  ⇒ `pc + 4` **无处可来** ⇒ 必须新增通路
- 选择：**EX 级算 `link_value = pc + 4`** → 过 `exmem_link_value` /
  `memwb_link_value` 两个流水线寄存器 → 进 `wb_stage.link_value`
  - ⚠️ 为什么不在 MEM/WB 级算？MEM/WB 没有 `pc` 通路（`pc` 只到 EX 级）
  - ⚠️ 为什么不在 WB 级算？WB 也没有 `pc`
  - ⇒ EX 级是**唯一**同时持有「该指令的 `pc`」与「写回通路起点」的位置
- `ex_stage.link_value` 是**输出**端口（由组件自己驱动）⇒ 无「未驱动」问题
- `wb_stage.link_value` 是**输入**端口 ⇒ 全部实例化点 MUST 显式驱动

### D4：`SRA` 在 ALU 层用「逻辑右移 + 符号位填充」实现

- 框架层**没有**算术右移原语：`include/core/operators_shift.h` 里
  `operator>>>`（`sshr_op`）是**被注释掉**的 ⇒ 启用它会动框架层
  （影响所有用户）⇒ 本 change **不启用** ⇒ 在 ALU 层合成
- `ch_uint` 无符号 ⇒ `operator>>`（`shr_op`）天然是**逻辑**右移 ✓
  ⇒ MUST **显式**取符号位并做高位填充

### D5：两处 `SRA` 实现**都**改

- `ex_stage.h`（内联 ALU，**pipeline 实际用的**）+ `rv32i_alu.h`（`Rv32iAlu` 组件）
- 只改一处 ⇒ 留下「组件层对、流水线层错」或反之的**分裂**
  ⇒ 变异验证 MUST 覆盖**两处**（各断一次）

### D6：SoC 的 32 端口契约**不变**

- 本 change 不新增 SoC 顶层端口（`JAL`/`JALR`/`SRA` 全在流水线内部）
- 验收**不依赖**新增观测端口：`JAL` 的判据是**固件行为**
  （子程序写的寄存器值被主程序读回）⇒ 这是比「看 PC 跳了」更 robust 的判据
  （它同时覆盖了「跳对了」+「写回对了」+「返回对了」三件事）
- ⇒ **无需**改 ADR-046 **D10**

### D7：**不拆成两个 change**（偏离 plan 的建议）

plan 写「写回通路可能缺失 ⇒ 若需扩流水线写回接口，本 change 体积会显著上升 ⇒
**宁可拆成两个 change**，也不要在一次里改两件事」。

**本设计不拆**，理由：

- 写回接口扩展（`link_value` 通路）**只服务 `JAL`/`JALR`**；`SRA` 与之**零耦合**
  ⇒ 「两件事」在**代码面**上并不纠缠
- 拆分会把「两次 golden 重算 + 两次审稿 + 两次归档」的开销**翻倍**
- 拆分唯一的收益是「归因」⇒ 归因改由**分离的断言 + 分离的变异**保证：
  `SRA` 的断言只看 `SRA` 用例的寄存器值，`JAL` 的断言只看跳转用例 ⇒
  **任一处变红都不会被另一处的断言掩盖**

### D8：**必须**连带修 U 型立即数（否则 JALR 的用例无法构造）

- `JALR` 的目标 = `(rs1 + imm) & ~1` ⇒ 要把「任意固件地址」装进 `rs1`
  ⇒ 标准做法是 `auipc x18, 0` + `addi x18, x18, off`（`auipc` 的 imm 只能是 4 KiB 倍数）
- ⚠️ 实测 `auipc x18, 0` 得到 `pc + 0x917000`：`id_stage` 用 **20 位掩码**
  （`instruction & 0x000FFFFF`）再左移 12 ⇒ 把 `rd`/`opcode` 位算进了立即数
- ⇒ 修复为 `imm[31:12] = instr[31:12]`（一行）；`LUI` 同时被修好
- ⚠️ 此前**零覆盖**（唯一消费者是仓外的 riscv-tests，CI 全 SKIP）

### D9：**必须**连带修 `alu_op` 对 I 型的 SUB/SRA 误判（否则负数立即数不可用）

- `ID` 级的 `alu_op` 用 `funct7[5]`（= `instr[30]`）区分 SUB / SRA
  —— 这只对 **R 型**（和 `SRAI`）成立
- ⚠️ 对 **I 型的 `ADDI`** 而言 `instr[30]` 是 **`imm[10]`** ⇒ **任何
  `imm >= 0x400` 或负数都被判成 SUB**
  - `addi x21, x0, -8`（`0xFF800A93`）⇒ `0 - (-8)` = **8**（实测）
  - `addi x21, x0, 0x7FF`（`0x7FF00A93`）⇒ `0 - 0x7FF` = **0xFFFFF801**（实测）
- ⇒ 修复：`alu_is_sub_sra = funct7[5] & (is_op | (opcode == OP_OPIMM & funct3 == 5))`
- ⚠️ 本固件原有 imm 全部 `< 0x400` ⇒ 侥幸未暴露（但**大偏移访存 / 负数立即数**
  一直受影响 —— 这是本 change 挖出的**独立价值**）

## 3. 时序

### `JAL` 的执行（5 级流水线，EX 级解析 ⇒ 2 拍跳转延迟）

| 拍 | IF | ID | EX | MEM | WB |
|---|---|---|---|---|---|
| T | 取 `JAL` | | | | |
| T+1 | 取**错误路径**指令 | 解码 `JAL`（`is_jump`=1） | | | |
| T+2 | 取**错误路径**指令 | | **EX：`branch_taken`=true、`branch_target`=目标、`link_value`=pc+4 | | |
| T+3 | **取目标指令** | | | MEM | |
| T+4 | | | | | **WB：写回 `pc + 4` ⇒ `ra`** |

- T+2 那拍：`hazard.flush` = true ⇒ ID/EX 清空（错误路径那条不进 EX）
  + `if_stage` 的 IF/ID 在 T+3 清空 ⇒ 错误路径的指令**不产生任何效果**
- T+2 那拍：`pc_reg` 在边沿装入**目标地址**（`next_pc_target` 已是目标）
  ⇒ T+3 取到的是**目标指令** ✓
- ⚠️ 与既有 `BEQ` 的时序**完全同构**（`BEQ` 也是 EX 级解析）⇒ 复用已验证的通路

### `JALR`

- 目标 = `(rs1 + imm) & ~1` ⇒ `rs1` 取 EX 级的 `rs1_data`
- ⚠️ **无数据前推** ⇒ `rs1` 刚被前一条指令写 ⇒ 读到旧值（已知限制，见 D2）

## 4. 验证

| 层 | 手段 |
|---|---|
| 编码 | 编码器自校验（`JAL`/`JALR`/`SRA` 的机器码字面量） |
| 行为 | 固件尾部追加：`JAL` → 子程序写 `x12` → 返回 → 主程序读 `x12`；`JALR` → 同构；`SRA` → `-8 >> 1` |
| 写回 | 断言 `ra`（x1）== 返回地址 |
| 双后端 | interpreter + verilator **逐拍全等**（复用 C3 的 32 端口观测面） |
| 变异 | ① SRA 改回 SRL（**两处各一次**）② `JAL` 目标接成 `pc + 4` ③ `link_value` 接成 `pc + imm` ④ `JALR` 的 `& ~1` 去掉 |
| 回归 | `ctest -LE perf`（**191/191**）+ 7 个护栏 |

## 5. 已知边界

- **`JALR` 的 `rs1` 无数据前推**（ADR-046 **D5** 悬置）⇒ 固件 MUST NOT 依赖
  「`JALR` 的 `rs1` 刚被写」⇒ 归 `rv32i-forwarding-decision-2026-10` 裁决
- **跳转延迟 = 2 拍**（EX 级解析）⇒ 与 `BEQ` 同构 ⇒ 固件 MUST NOT 假设
  「跳转是 1 拍」
- **不实现分支预测** ⇒ 每条 taken 分支都付 2 拍（`rv32i_branch_predict.h`
  **未接入**流水线）
- **`JAL`/`JALR` 的冲刷代价 = 3 拍**（与条件分支**同构**，实测 `non_retired` 11 → 17）
- **`LUI` / `AUIPC` 的 imm 修复**改变了它们的行为（此前是错的）——
  当前**无固件/测试依赖旧行为**（唯一消费者是仓外 riscv-tests 二进制）
- **`alu_op` 修复的影响面**：只对 `imm >= 0x400` 的 I 型指令生效
  ⇒ 既有固件（imm 全部 `< 0x400`）零行为变化（实测：既有 13 条写回 + 全部计数不变）
- **`SRA` 只改 32 位**（`ch_uint<32>`）⇒ 不涉及 `ch_uint<N>` 的通用算术右移
- **非对齐 `JALR` 目标**：`(rs1 + imm)` 可以是奇数（`& ~1` 只清 bit0）
  ⇒ 目标必为 2 字节对齐；取指按字 ⇒ 不实现地址不对齐异常（RISC-V 允许实现自定义）
