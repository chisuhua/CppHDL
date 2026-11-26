# proposal · RV32I **trap 入口接线**（P6-3）

> **Change**：`rv32i-trap-entry-2026-11`
> **母 improvement**：`rv32i-privileged-csr-2026-11`（**P6-3**）
> **前置**：① **P6-1**（`mcause` 可写 · 已闭）② **P6-2**（CSR 指令 · 已闭）
> ⇒ ⇒ **D-1 / D-2 不阻塞本项**（它们分别阻塞 P6-4/P6-6 的**期望值**，见 §What Changes 的边界段）
> ⚠️ **开工前置（架构面）**：**ADR-048 的 dated addendum**（第二路 PC 重定向打破
> 「flush 唯一来源」）⇒ ⇒ **architect 域** ⇒ ⇒ 见 `design.md` §7 与 `tasks.md` T2。

---

## Why（为什么现在做）

P6-2 落地后，**非法指令**（未知 opcode / `OP_SYSTEM` 的 `funct3=0` / `funct3=4`）的处置
仍是**流水线级冻结**：`illegal_reason != 0` ⇒ IF 停 + ID/EX 装气泡 ⇒ **该指令永不退休**。

⇒ ⇒ **软件侧看不到任何错误信号**：既没有 PC 重定向，也没有 `mcause` ⇒ ⇒ 固件一旦跑到
一条非法指令就**永久挂死**（而不是跳进 handler）⇒ ⇒ 这与 P6-2 之前的「静默当 NOP 退休」
相比是**改善**（不再静默跳过），但**仍不是**机器级语义应有的 trap。

⇒ ⇒ **trap 入口的「机制」是缺失的那一环**：RISC-V 要求非法指令 ⇒ **陷入**（`mcause` = 2）
⇒ ⇒ 写 `mepc` / `mcause` ⇒ ⇒ PC 跳到 `mtvec` ⇒ ⇒ handler 处理后 `MRET` 返回。
⇒ ⇒ 本项只做**机制**（接线 + 上下文写入 + PC 重定向 + kill 语义），
**不**做触发源的完整集合（`ECALL`/`EBREAK` = P6-4 · 中断 = P6-6）。

---

## What Changes

### ✅ IN

| # | 内容 | 载体 |
|---|---|---|
| **S1** | **`ExceptionUnit` 首次接入流水线**（此前**零生产消费者**）：作为 pipeline 的 **inline** 子组件（`ch_module`）⇒ ⇒ 与 pipeline 内的 `CsrBank` **共享同一实例**（父层连线 ⇒ **零端口面变更**） | `rv32i_pipeline.h` |
| **S2** | **trap 触发点 = ID 级**：触发源 = **非法指令**（`illegal_reason != 0`）⇒ ⇒ `exception_valid = illegal_active & id_valid & !flush`（**排除 flush** ⇒ ⇒ 错误路径上的非法字**不得**写坏 `mepc`） | `rv32i_pipeline.h` |
| **S3** | **`mepc` 保现场** = 触发指令的 PC（**ID 级**的 IF/ID PC ⇒ ⇒ `mepc` 指向**该指令**本身 ⇒ ⇒ 该指令**不前进**、**不退休**） | `rv32i_pipeline.h` |
| **S4** | **PC 重定向的第二路**：`if_stage` 新增 `trap_target` / `trap_flush` 输入 ⇒ ⇒ `next_pc_target` 变成「**trap > branch > PC+4**」三路（**内部** mux；`if_stage.__io` +2 端口 ⇒ **被顶层消费 ⇒ 不冒泡** ⇒ ⇒ 顶层 53 端口**不变**） | `stages/if_stage.h` |
| **S5** | **`trap_flush_d1`**（多杀一拍：imem 是 1 拍 `sread` ⇒ ⇒ 与既有 `flush_d1` **同构**）⇒ ⇒ `if_id_update_en` 与 `skid_valid` 的清空条件都 MUST 含 `trap_flush \| trap_flush_d1`（**skid MUST 清空** ⇒ ADR-048 D5 的同构要求） | `stages/if_stage.h` |
| **S6** | **kill 语义**：trap ⇒ ⇒ **IF/ID 被杀**（`if_id_valid_reg` 清零）⇒ ⇒ ID 级下一拍为空 ⇒ ⇒ **ID/EX 装气泡** ⇒ ⇒ 触发指令**不退休**；**EX/MEM/WB 不受影响**（在途的**更老**指令继续退休） | `stages/if_stage.h` + `rv32i_pipeline.h` |
| **S7** | **`CsrBank` 单写口的仲裁**：`CsrBank` 只有**一条**写口 ⇒ ⇒ 本项引入**第二个写源**（`ExceptionUnit` 的两拍写：`mepc` 拍 1 / `mcause` 拍 2）⇒ ⇒ MUST 写死优先级 ⇒ ⇒ **本项选「`ExceptionUnit` 优先」**（trap 上下文是**一次异常**的成对写入 ⇒ 丢它更严重 ⇒ ⇒ CSR 指令的写在该拍被抑制 ⇒ ⇒ 记为**已知边界**） | `rv32i_pipeline.h` |
| **S8** | **`mret` 通道留空但接通**：`ExceptionUnit` 的 `mret_instruction` 接 **0**（`MRET` = **P6-5**）⇒ ⇒ 接口面**完整** ⇒ ⇒ P6-5 只差「谁来拉高它」 | `rv32i_pipeline.h` |

### ❌ OUT（本项**不做** ⇒ ⇒ 理由逐条登记）

| # | 不做的事 | 理由（**归属 step**） |
|---|---|---|
| **O1** | `ECALL` / `EBREAK` 的 cause 值（**11** / **3**）与它们的 trap 触发 | **P6-4** · 且 **D-2（`mcause` 编码表 / `bit31` 中断位）未裁决** ⇒ ⇒ 期望值**无唯一解** ⇒ ⇒ 本项只做 **illegal（cause 2）** |
| **O2** | `MRET` 的 PC 恢复 + `mstatus.MIE↔MPIE` | **P6-5**（`CsrBank` 的 `mstatus` **只有写口** ⇒ ⇒ 恢复路径**当前不存在**） |
| **O3** | **中断**（CLINT ⇒ `mtip` · `mie`/`mip`/`mstatus` 陷阱侧） | **P6-6**（受理条件 + 采样点 + `mcause[31]=1` 全部待定） |
| **O4** | `mtval` 真值 | **P6-7**（cause 相关 ⇒ ⇒ 需 cause × 手推表） |
| **O5** | **`mtvec` 取值语义**（base / 模式位 / 复位值是否合理） | **D-1 未裁决** ⇒ ⇒ 本项只把 `mtvec` 的**当前值**当**输入**用（`ExceptionUnit` 已实现 `base = mtvec & ~3`） |
| **O6** | 未实现 / 只读 CSR 地址的**可写性语义**（`misa`/`mip` 写入被静默丢弃 ⇒ ⇒ 规范要求 illegal） | 母 improvement 的**第 6 项排除**（本版不 trap） |

---

## 验收标准（AC · 可机械判定）

> ⚠️ **期望值 MUST 手推**（禁抄仿真输出 —— 解释器与 codegen 同源 ⇒ ⇒ 双后端一致地错
> ⇒ ⇒ parity 对该族缺陷**结构性失明** ⇒ ⇒ parity 只能当**辅助**证据）。
> ⚠️ **观测面 MUST 零新增端口** ⇒ ⇒ `mepc` / `mcause` 的**寄存器值**经
> **pipeline 内的 `csr_bank` 实例**读取（`csr_bank.instance().io().mepc_value`）⇒ ⇒
> **不碰**核顶层 53 端口契约。

- [ ] **AC1**：一条**非法指令**（如 `OP_SYSTEM` 的 `funct3=4`）⇒ ⇒ 触发 trap ⇒ ⇒
      `mepc` == 该指令的 PC（**手推**）· `mcause` == **2**（illegal instruction）
- [ ] **AC2**：trap ⇒ ⇒ 触发指令**不退休**（退休计数**不递增**）⇒ ⇒ **IF/ID 被杀**
      （下一拍 ID 级 `valid == 0`）
- [ ] **AC3**：trap ⇒ ⇒ PC **重定向**到 `mtvec` 的 base（`mtvec & ~3`）⇒ ⇒ **手推**
- [ ] **AC4**：**在途更老指令照常退休**（trap **不**杀 EX/MEM/WB）⇒ ⇒ 判据 = trap 前
      排入一条 ALU 指令 ⇒ ⇒ trap 后它**仍**退休
- [ ] **AC5**：`flush` 优先 ⇒ ⇒ **taken 分支的 fall-through 槽**放非法字 ⇒ ⇒
      **不**触发 trap（`mepc` / `mcause` **不变**）⇒ ⇒ **错误路径的 PC 不得写坏 `mepc`**
- [ ] **AC6**：`trap_flush` **多杀一拍**（`trap_flush_d1`）⇒ ⇒ 判据 = trap 后**连续两拍**
      ID 级 `valid == 0`（第二拍是 `sread` 的在途指令）⇒ ⇒ **skid MUST 被清空**
- [ ] **AC7**：**单写口仲裁** ⇒ ⇒ trap 上下文（`mepc` + `mcause`）**成对**写入
      （`mcause` 晚 1 拍）⇒ ⇒ 且与 `mepc` 是**同一次**异常
- [ ] **AC8**：**零端口面变更** ⇒ ⇒ 核 IP 顶层 **53** 端口契约表**逐字不变** +
      C6 快照不变 + 层次化 module 数不变（`ExceptionUnit` 是 **inline** ⇒ **不新增层**）
- [ ] **AC9**：**每条新判据附一条变异**（还原错误实现 ⇒ 转红）⇒ ⇒ 且**变异 MUST 仍能编译**
- [ ] **AC10**：**spec delta** 进 `rv32i-privilege`（`## ADDED Requirements`）⇒ ⇒
      **禁** `--skip-specs` ⇒ ⇒ **既有 Requirement 不动** ⇒ ⇒ ⚠️ 同时**逐项**修订
      capability 的「**不**覆盖」清单（**D-3 的接缝兑现**：本项关闭
      「`#2 mtvec 取值语义`」中的**机制部分**，`#1 mtval` / `#3 mstatus 陷阱侧` /
      `#4 MRET 恢复` / `#5 并发异常排队` 仍归 P6-5/P6-6/P6-7）
- [ ] **AC11**：**全量回归零失败**（`ctest -LE perf`）· 全部 `scripts/verify_*.sh` PASS

---

## Capabilities

- **`rv32i-privilege`** —— **ADDED**（一条新 Requirement：「trap 入口 MUST 保存上下文
  并重定向 PC」）⇒ ⇒ **既有 Requirement 不动**（MODIFIED 会整体替换既有条款 ⇒ ⇒ 风险高）
- ⚠️ **不新增 capability**（trap 入口是**既有** `rv32i-privilege` 的自然延伸 ⇒ ⇒
  新 capability = vapor capability）

---

## 用户可见变更

- **非法指令不再「永久挂死」**：改为 **陷入**（`mcause` = 2 · `mepc` = 触发指令 PC · PC → `mtvec`）
- 顶层端口面**零变化**（53 端口逐字不变）
- `mstatus` / `mie` / `mip` / `misa` 的语义**零变化**（本项不碰陷阱侧更新）
