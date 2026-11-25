# Change: RV32I CSR 指令（`csrrw` / `csrrs` / `csrrc` / `csrrwi` / `csrrsi` / `csrrci`）

> **Change 名**：`rv32i-csr-instructions-2026-11`
> **母 improvement**：`rv32i-privileged-csr-2026-11`（**P6-2** · 步骤）
> **日期**：2026-11-25 · builder
> **性质**：
> - 🔴 **补 ISA 能力缺口**（CSR 指令当前 = `illegal_reason = 1` ⇒ 冻结 ⇒ 完全不可用）
> - 🟢 **首次把 `CsrBank` 接进流水线**（⇒ **生产消费者 0 → 1** ⇒ 消除「有硬件、无指令」倒挂）
> ⇒ ⇒ **零端口面变更**（核 IP 顶层 **53** 端口契约表 + C6 快照**不动**）
> ⇒ ⇒ **零公开 API 变更**（`CsrBank` / `ExceptionUnit` 的 `__io` **一字未动**）

---

## Why

### 1. CSR 指令当前**完全不可用**（不是「未覆盖」，是「冻结」）

`include/cpu/pipeline/rv32i_decoder.h` 的非法判定把**整个** `OP_SYSTEM`（`0x73`）归为
「**已知未实现**」⇒ ⇒ `illegal_reason = 1` ⇒ ⇒ `rv32i_pipeline.h` 的
`if_stage.stall` / `id_stage.stall` **均 OR 上 `illegal_reason != 0`**
⇒ ⇒ **任何 `csrrw` / `csrrs` / `csrrc` 一进 ID 级就把流水线冻死**
⇒ ⇒ **PC 不前进 · ID/EX 装气泡 · 该指令永不退休**。

⇒ ⇒ 现状是：**CSR 指令不是「跑错」，而是「跑不动」** ⇒ ⇒ 软件侧看不到任何错误信号
⇒ ⇒ 与仓内既有的「未知 opcode 静默当 NOP」是**同类缺陷**（ADR-071 的 G1 家族）
⇒ ⇒ 本 change 把它从「已知未实现」里**摘出来并真正实现**。

### 2. `CsrBank` 已实现，但**生产消费者 = 0** ⇒ 「有硬件、无指令」倒挂

`include/cpu/riscv/rv32i_csr.h` 的 `riscv::CsrBank` **已具备**全部所需能力：

| 能力 | 现状 |
|---|---|
| **9 个 CSR**（`mstatus` / `misa` / `mie` / `mtvec` / `mscratch` / `mepc` / `mcause` / `mtval` / `mip`） | ✅ 已实现（`misa` / `mip` 只读 · 写入被忽略） |
| **组合读口** `csr_addr` → `csr_rdata` | ✅ **组合**（无寄存器）⇒ ⇒ **ID 级可组合读** |
| **沿写口** `csr_addr` / `csr_wdata` / `csr_write_en` | ✅ 单写口（`ch_reg` 的 `->next`） |
| 中断链（`mtip` / `meip` → `interrupt_req` · `mstatus` 字段访问器） | ✅ 已实现（归 P6-6 接线） |

⇒ ⇒ 但它的**生产引用数 = 0**（`include/**` + `src/**` + `examples/**` 中排除自身头与
`tests/**` 后 ⇒ **零命中**）⇒ ⇒ 消费者**只有 23 处测试实例化点**
（`examples/riscv-mini/tests/test_csr_unit.cpp` 11 + `test_interrupt_flow.cpp` 12）
⇒ ⇒ ⇒ **组件有测试、流水线无接线** ⇒ ⇒ 「实现了但没人用」⇒ ⇒ **交付路径上等于不存在**。

### 3. 它是**三条线**的共同前置

| 线 | 阻塞点 |
|---|---|
| **`rv32i-soc-platform-2026-11` 的 S5（IRQ）** | 已登记「被 `rv32i-privileged-csr-2026-11` **双重阻塞**」⇒ ⇒ CSR 指令是其硬前置 |
| **同上的 S7（MMU Sv32）** | 依赖 S5 ⇒ ⇒ 依赖本项 |
| **`rv32i-arch-test-certification-2026-11` 的特权面** | `Zicsr` / Exceptions 测试 ⇒ ⇒ 依赖本项 |

⇒ ⇒ ⇒ **本项是「一次投入解锁三条线」的节点** ⇒ ⇒ 而它**零外部依赖**（不需新工具链）。

### 4. 「CSR 读旧值」**不合规** ⇒ ⇒ RAW 语义**必须**裁决

RISC-V 规范要求 **CSR 访问之间保持顺序** ⇒ ⇒ 「后一条 CSR 指令读到前一条写入前的旧值」
**违反该要求** ⇒ ⇒ ⇒ **不能**用「读旧值」图省事 ⇒ ⇒ 必须**停顿**或**等价机制**。

⚠️ 而**停顿判据若放进 `HazardUnit`** ⇒ ⇒ 触碰根 `AGENTS.md:256` 的明文红线
（「**`HazardUnit` 端口面冻结在 4 个直通端口** ⇒ 不得再接 `ex_alu_result`（环饵）」）
⇒ ⇒ ⇒ **本项的判据落 `rv32i_pipeline.h` 顶层**（复用现成的 `if_stage.stall` /
`id_stage.stall` 机制）⇒ ⇒ ⇒ **零新增 `HazardUnit` 端口** ⇒ ⇒ **红线不受影响**。
⚠️ **该走法已获用户确认（2026-11-25）**。

---

## What Changes

### ✅ IN

| # | 内容 |
|---|---|
| **S1** | **译码**：CSR 指令的 **6 种 `funct3`**（`001` `CSRRW` / `010` `CSRRS` / `011` `CSRRC` / `101` `CSRRWI` / `110` `CSRRSI` / `111` `CSRRCI`）⇒ ⇒ **零新增 decoder 端口**（从已有 `opcode` + `funct3` 推导） |
| **S2** | **`illegal_reason` 细化**：合法 `funct3` ⇒ `reason = 0`；`funct3 = 4`（**保留**）⇒ `reason = 3`；`funct3 = 0`（`ECALL`/`EBREAK`/`MRET`/`WFI`）⇒ **仍 `reason = 1`**（归 P6-4 / P6-5）⇒ ⇒ ⚠️ **必须与 S1–S6 同批**（单独改 ⇒ 「合法但无功能」⇒ 比「冻结」更糟 ⇒ 会当 NOP 退休） |
| **S3** | **读路径**：ID 级**组合读** `CsrBank.csr_rdata` ⇒ ⇒ 读值随指令下行 ⇒ ⇒ **WB 级写回 `rd`**（进现有 mux） |
| **S4** | **写路径**：EX 级算 `csr_wdata`（`CSRRW` = `rs1` 值；`CSRRS`/`CSRRC` = `rs1` \| `rs2`（或 `zimm`））⇒ ⇒ 沿写口写入 `CsrBank` |
| **S5** | **立即数型**（`CSRRWI`/`CSRRSI`/`CSRRCI`）：源操作数 = **`zimm`(5 位 · 零扩展)**，**不是** `rs1` 值 |
| **S6** | **特殊语义**：`CSRRW` 且 `rd = 0` ⇒ **不读**旧值（仍写）；`CSRRS`/`CSRRC` 且 `rs1 = 0` ⇒ **不写**（仍读 + 写 `rd`） |
| **S7** | **RAW 停顿**：CSR→CSR 依赖 ⇒ **停顿 1 拍**（判据落 pipeline 顶层 · **零新增 `HazardUnit` 端口**） |
| **S8** | **`CsrBank` 首次接入流水线**（生产消费者 0 → 1）⇒ ⇒ 其 `mtip` / `meip` 输入**显式 tie 0**（P6-2 阶段无中断源 ⇒ ⇒ 未驱动 = 未定义值 ⇒ MUST 显式） |
| **S9** | **判据**：逐条**手推期望值**（禁抄仿真输出）+ **每条一条变异**（还原错误实现 ⇒ 转红） |

### ❌ OUT（不做 · 已登记归属）

| 项 | 归属 / 理由 |
|---|---|
| `ECALL` / `EBREAK`（`funct3 = 0`） | ⇒ ⇒ **P6-4**（归 `rv32i-privileged-csr-2026-11`）⇒ ⇒ 本项**保持** `reason = 1`（冻结） |
| `MRET`（`funct3 = 0` · `imm = 0x302`） | ⇒ ⇒ **P6-5** |
| `WFI`（`funct3 = 0` · `imm = 0x105`） | ⇒ ⇒ **归属未定**（见母 improvement 的 D-2）⇒ ⇒ 本项**不碰** |
| trap 入口接线（`mepc` / `mcause` 写） | ⇒ ⇒ **P6-3** ⇒ ⇒ 本项**不接** `ExceptionUnit` ⇒ ⇒ ⇒ **故 `CsrBank` 的写口在本项阶段只有 CSR 指令一个写源 ⇒ ⇒ 无仲裁冲突** |
| 中断（`mtip` / `mie` / `mip` / `mstatus` 陷阱侧更新） | ⇒ ⇒ **P6-6** |
| `mtval` 真值（cause 相关） | ⇒ ⇒ **P6-7** |
| `misa` / `mip` 的**可写性语义** | ⇒ ⇒ 母 improvement 的**第 6 项排除**（本版**不 trap** ⇒ ⇒ 写入被忽略 · 读返回 0）⇒ ⇒ **本项保持现状** |
| 扩 TCM 容量 / 动 `HazardUnit` 端口面 | ⇒ ⇒ **红线**（`AGENTS.md:256`）⇒ ⇒ 本项**零**触碰 |

---

## 验收标准（AC · 可机械判定）

- [ ] **AC1**：`csrrw` / `csrrs` / `csrrc` / `csrrwi` / `csrrsi` / `csrrci` **6 种**指令
      **各至少 1 条**流水线级用例 ⇒ ⇒ 全部**正常退休**（`retired` 计数递增）
      ⇒ ⇒ `illegal_reason` 对它们**恒为 0** ⇒ ⇒ 且**不冻结**（PC 继续前进）
- [ ] **AC2**：`csrrs rd, csr, x0` 读回 `csr` 的**当前值** ⇒ ⇒ **期望值逐条手推**
      （禁抄仿真输出 —— 解释器与 codegen 同源 ⇒ ⇒ parity 对该族缺陷**结构性失明**）
- [ ] **AC3**：`csrrw` 写入后 `csrrs` **读到新值** ⇒ ⇒ 且**相邻两条**（距离 1）
      ⇒ ⇒ **必须读新值**（**不是旧值**）⇒ ⇒ ⇒ **判别力 = 去掉 RAW 停顿 ⇒ 转红**
- [ ] **AC4**：`csrrs` / `csrrc` 的**读-改-写**语义逐条手推
      （`set` / `clear` 掩码 · 与 `rs1` 值 OR / AND-NOT）⇒ ⇒ `csrrc` 的掩码
      MUST 是**按位取反后 AND**（不是 XOR）
- [ ] **AC5**：立即数型（`csrrwi` / `csrrsi` / `csrrci`）用 **`zimm`(5 位零扩展)**
      ⇒ ⇒ **不是** `rs1` 值 ⇒ ⇒ 判据 MUST 用一个 **`rs1 ≠ zimm` 的指令**（否则恒绿）
- [ ] **AC6**：`CSRRW` 且 `rd = 0` ⇒ ⇒ **不读**旧值（写仍发生）⇒ ⇒
      `CSRRS`/`CSRRC` 且 `rs1 = 0` ⇒ ⇒ **不写**（读 + 写 `rd` 仍发生）⇒ ⇒
      逐条用例 + 变异
- [ ] **AC7**：`funct3 = 4`（`OP_SYSTEM` 的保留组合）⇒ ⇒ `illegal_reason == 3`
      （**不是** 1 · **不是** 0）⇒ ⇒ 且**冻结**（与既有「保留组合」行为一致）
- [ ] **AC8**：`funct3 = 0`（`ECALL` / `EBREAK` / `MRET` / `WFI`）⇒ ⇒
      `illegal_reason == 1`（**仍是「已知未实现」**⇒ ⇒ 冻结）⇒ ⇒
      ⇒ ⇒ **不得**因本项而变成「合法但无功能」
- [ ] **AC9**：`CsrBank` 的 `mtip` / `meip` 输入**显式 tie 0** ⇒ ⇒
      未驱动端口读到**未定义值** ⇒ ⇒ 判据 MUST 断言「本项阶段无中断源」
- [ ] **AC10**：**零端口面变更** ⇒ ⇒ 核 IP 顶层 **53** 端口契约表**逐字不变** +
      C6 快照**逐字不变** + **产物顶层端口表逐字不变**（新增端口若**冒泡**到产物顶层
      ⇒ ⇒ 独立 Verilog TB 的端口表会**整体错位** ⇒ ⇒ ⇒ **MUST 实测比对**）
- [ ] **AC11**：**每条新判据附一条变异**（还原错误实现 ⇒ 转红）⇒ ⇒ 且
      变异 MUST **仍能编译**（编译失败 ⇒ 跑的是旧二进制 ⇒ 假绿）
- [ ] **AC12**：**spec delta** 进 `rv32i-privilege`（`## ADDED Requirements`）⇒ ⇒
      **禁** `--skip-specs` ⇒ ⇒ 归档后**手工**更新该 capability 的 `## Purpose`
      （⚠️ delta 里的 `## Purpose` **不被合并** ⇒ ⇒ 见
      `docs/developer_guide/openspec-specs-guide.md` §「Purpose 段 MUST 在 archive 之后手工写入」）
- [ ] **AC13**：**计数锚点同步** ⇒ ⇒ `CTEST_TOTAL`（新增 ctest target ⇒ +K）⇒ ⇒
      ⇒ ⇒ `EXAMPLES_TEST_SOURCES`（若新增文件落在 `examples/**/tests/` 下 ⇒ ⇒ +1）
      ⇒ ⇒ ⇒ **与 phase-6 的收口批次串行**（锚点编辑权 = 最后合入者）

---

## Capabilities

| capability | 变化 |
|---|---|
| **`rv32i-privilege`** | **ADDED** —— 新增一条 Requirement：「**CSR 指令（6 种 `funct3`）的语义与顺序保证**」<br>⚠️ **既有 Requirement 不动**（MODIFIED 会**整体替换**既有条款 ⇒ ⇒ 风险高）⇒ ⇒ ⇒ 只 ADDED<br>⚠️ 该 capability 的 `## Purpose` 里「**不**覆盖 CSR 指令语义（`csrrw`/`csrrs`/`csrrc`）」一句**将在归档后手工更新**（AC12） |
| `rv32i-isa-semantics` | ⚠️ **零 MODIFIED** ⇒ ⇒ 「未知 / 未实现 opcode ⇒ 流水线级冻结」那条**仍然成立**（只是 `OP_SYSTEM` 的**一部分** `funct3` 不再属于「未实现」）⇒ ⇒ ⇒ **不产生该 capability 的 delta** |
