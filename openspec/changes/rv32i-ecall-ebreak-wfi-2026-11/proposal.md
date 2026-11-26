# proposal · RV32I **`ECALL` / `EBREAK` / `WFI` + `mtvec`/`mepc` WARL**（P6-4）

> **Change**：`rv32i-ecall-ebreak-wfi-2026-11`
> **母 improvement**：`rv32i-privileged-csr-2026-11`（**P6-4**）
> **前置**：① P6-1（`mcause` 可写 · 已闭）② P6-2（CSR 指令 · 已闭）③ P6-3（trap 入口 · 已闭）
> **裁决依据**：`.rddf/checks/decision-request-rv32i-privileged-d1-d2-2026-11.md`
> ⇒ ⇒ **D-1** = A（`mtvec.MODE` WARL = {0} + 复位值 `0x8000_0100`）
> ⇒ ⇒ **D-2** = 全部按推荐（`ECALL` = 11 · `EBREAK` = 3 · `WFI` = 合法 NOP ·
> 未定义 `imm` / `SRET` ⇒ `reason = 3` · `mcause` 不加 coercion）
> **规范依据**：Privileged Spec **v20260120**（Machine-Level ISA 1.13）·
> `§2.1.1.7`（`mtvec`）· `§2.1.1.14`（`mepc`）· `§2.1.1.15`（`mcause`）·
> `§2.1.3.1`（`ECALL`/`EBREAK`）· `§2.1.3.3`（`WFI`）

---

## Why（为什么现在做）

P6-3 把**非法指令**接入了 trap（`reason ∈ {2,3}` ⇒ 陷入 · `mcause = 2`），但
`OP_SYSTEM` 的 `funct3 = 0` **仍是整桶冻结**：

- `ECALL` / `EBREAK` **冻结** ⇒ ⇒ 固件一旦执行「系统调用 / 断点」就**永久挂死**
  （既无 `mepc` / `mcause`，也没有 handler 可跑）；
- `WFI` **冻结** ⇒ ⇒ 轮询式固件（`wfi` 等待）**挂死**，而规范**明确允许**把它实现成 **NOP**；
- `SRET`（`0x102`）与**未定义 `imm`** 也落进同一个「冻结」桶 ⇒ ⇒ 它们是
  **不可编码**（本核 `misa.S = 0` / ISA 未定义）⇒ ⇒ 规范要求 **illegal-instruction 陷入**，
  而不是挂死。

同时**两个 WARL 字段**与规范不一致（D-1 实测）：

- `mtvec` 写路径**无 coercion**，而 `ExceptionUnit` **恒取 Direct** ⇒ ⇒ 写入 `MODE = 1`
  后**读回 1、行为仍 Direct** ⇒ ⇒ 「读回 ≠ 硬件行为」；
- `mepc` 写路径**无对齐 coercion**，而 IALIGN=32 下 `mepc[1:0]` **必须恒 0**
  （Priv §2.1.1.14）⇒ ⇒ 写奇数 PC 会原样读回。

⇒ ⇒ 本项把 `funct3 = 0` 按 `imm[11:0]` **细分**、把 `ECALL`/`EBREAK` 接进 trap、
把 `WFI` 定为合法 NOP、把两个 WARL 字段补齐 —— 这也是 **P6-5（`MRET`）/ P6-6（中断）**
的期望值前提（否则只能「先写实现、再抄期望」）。

---

## What Changes

### ✅ IN

| # | 内容 | 载体 |
|---|---|---|
| **S1** | `OP_SYSTEM` / `funct3 = 0` 按 **`imm[11:0]`** 细分：`0x000` ECALL · `0x001` EBREAK · `0x105` WFI ⇒ **合法**（`reason = 0`）· `0x302` MRET ⇒ `reason = 1`（**P6-5 前仍冻结**）· 其余（含 `0x102` SRET）⇒ `reason = 3` | `pipeline/rv32i_decoder.h` |
| **S2** | 新输出 **`sys_trap_kind`**（2 bit · 0 = 非 ECALL/EBREAK · 1 = ECALL · 2 = EBREAK）⇒ ⇒ 具名端口，**追加在 `illegal_reason` 之后** ⇒ 既有 unnamed 端口编号**不位移** | `pipeline/rv32i_decoder.h` + `stages/id_stage.h`（`valid` 门控透传） |
| **S3** | 触发集扩展：`trap_trigger = (reason ∈ {2,3}) \| (sys_trap_kind ≠ 0)`，仍 `& id_valid & !flush` | `pipeline/rv32i_pipeline.h` |
| **S4** | `mcause` 动态：`ECALL` ⇒ **11**（from M）· `EBREAK` ⇒ **3** · 其余 ⇒ **2** | `pipeline/rv32i_pipeline.h` |
| **S5** | **合法但陷入**的指令 MUST 与非法指令走同一条「ID 装气泡」路径（`illegal_active` 纳入 `sys_trap_kind`）⇒ 否则会越过 trap 照常退休 | `pipeline/rv32i_pipeline.h` |
| **S6** | `mtvec.MODE` **WARL = {0}**：写路径把 `[1:0]` 掩零 ⇒ 读回恒 Direct | `riscv/rv32i_csr.h` |
| **S7** | `mtvec` 复位值 `0x2000_0100` → **`0x8000_0100`**（I-TCM 区内；与旧值**同一 word `0x040`** ⇒ 行为零变化） | `riscv/rv32i_csr.h` |
| **S8** | `mepc[1:0]` 写入掩零（IALIGN=32 ⇒ 恒 0） | `riscv/rv32i_csr.h` |
| **S9** | `rv32i_exception.h` 的原因码注释**分表**（异常 / 中断）—— 旧表把两侧混写（`3` = MSI / `8` = ECALL / `11` = MEI） | `riscv/rv32i_exception.h`（纯注释） |

### ❌ OUT（**明确不做**）

- **`MRET` 的实现**（`pc ← mepc` + `MIE/MPIE/MPP` 弹栈）⇒ **P6-5**；本项只保证它**仍**
  `reason = 1`（冻结），**MUST NOT** 提前当 NOP / trap。
- **中断**（`mcause[31] = 1` · `mtip` / `meip`）⇒ **P6-6**；本项只登记数值规则。
- **`mtval` 真值** ⇒ **P6-7**（本项维持 0）。
- **`mtvec` 的 Vectored 模式** ⇒ 本项选 **WARL = {0}**（§D-1 裁决 A）；
  未来若要实现 Vectored ⇒ 以 **MODIFIED delta** 扩 `MODE` 合法集。
- **`mcause` 的 WARL/WLRL coercion** ⇒ 不做：Exception Code 是 **WLRL**，
  规范**允许**任意读回（Priv §1.1.3.2）⇒ ⇒ 软件可写任意值可读回**合规**。

---

## Capabilities

- `rv32i-privilege`：**MODIFIED ×2**（触发集 + `funct3=0` 细分）· **ADDED ×1**
  （`mtvec` / `mepc` 的 WARL 契约）。

---

## 验收标准

- [ ] **AC1** `ECALL`（`imm = 0x000`）⇒ 核级陷入：`mepc` = **ECALL 自身 PC** ·
      `mcause` = **11**；`illegal_reason` 恒 **0**（合法指令）· ECALL **不退休**。
- [ ] **AC2** `EBREAK`（`imm = 0x001`）⇒ 核级陷入：`mepc` = 自身 PC · `mcause` = **3**。
- [ ] **AC3** `SRET`（`imm = 0x102`）⇒ `illegal_reason = 3` ⇒ 陷入 · `mcause = 2`
      ⇒ ⇒ **MUST NOT** 为 `reason = 1`（冻结 ⇒ hang）。
- [ ] **AC4** `WFI`（`imm = 0x105`）⇒ **合法 NOP**：`reason = 0` · 不 trap · 指令流继续。
- [ ] **AC5** `funct3 = 0` 的**未定义 `imm`**（非 `0x000/0x001/0x105/0x302`）⇒ `reason = 3`
      ⇒ 陷入 · `mcause = 2`。
- [ ] **AC6** `MRET`（`imm = 0x302`）⇒ **仍** `reason = 1`（冻结）· 不 trap · 无写回
      （P6-5 前不得提前兑现）。
- [ ] **AC7** `mtvec` 的 `MODE` 是 **WARL = {0}**：写 `…01` / `…11` ⇒ 读回 `…00`
      （读回 = 硬件行为）。
- [ ] **AC8** `mepc[1:0]` 恒 0（IALIGN=32）：写 `0x…37` ⇒ 读回 `0x…34`；写 `0x…3F` ⇒ 读回 `0x…3C`。
- [ ] **AC9** `mtvec` 复位值 = **`0x8000_0100`**（I-TCM 区内）；trap 落点字索引仍为 **`0x040`**
      （现有固件级判据**值不变**）。
- [ ] **AC10** **零端口面变更**：核 IP **53** 端口契约表 / **C6 快照 51** / 独立 TB / tsv /
      层次化 module 数（8）全部不动；**零新增 `HazardUnit` 端口**。
- [ ] **AC11** 全量回归 `ctest -LE perf` 全绿（0 失败）；锚点无变化
      （`CTEST_TOTAL` / `VERILATOR_E2E_TOTAL` / `DUAL_BACKEND_PARITY_*` / `EXAMPLES_TEST_SOURCES`）。
- [ ] **AC12** 每条新判据配**一条变异**（须仍能编译），逐条登记在 `design.md` §8。

---

## Impact

| 面 | 影响 |
|---|---|
| **公开 API** | 无（`CsrBank` / `ExceptionUnit` 的 `__io` 未动）。 |
| **内部端口** | +1 个具名输出 ×2（decoder / id_stage）· pipeline 读一个（**被消费 ⇒ 不冒泡**）。 |
| **核 IP 顶层** | **零变更**（53 端口不动）。 |
| **行为** | ECALL/EBREAK 从「冻结」→「陷入」；WFI 从「冻结」→「NOP」；`SRET`/未定义 `imm` 从「冻结」→「陷入」；`mtvec`/`mepc` 写入被掩码；`mtvec` 复位值语义化（同 word）。 |
| **既有判据** | 3 处语义订正（`test_csr_unit` 的复位值 · `test_rv32i_csr_instructions` 的 AC8 · `test_rv32i_soc_dual_parity` 的 ECALL 场景）+ `test_rv32i_trap_entry` 的 AC12 改写。 |

---

## 用户可见变更

> 本段按 `.rddf/checks/public-api-surface.md` 的载体机制填写（机制生效日 = 2026-11-09）。

- **库代码 / 公开 API：无变更** —— `CsrBank` / `ExceptionUnit` 的 `__io` 签名未动；
  核 IP 顶层 **53 端口**逐字不变（C6 快照 51 不变）。
- **ISA 行为（对下游可见，**值得知道**）**：
  - `ECALL` / `EBREAK` 从「流水线级冻结」改为**合法的 trap 指令**（`mcause` = **11** / **3**）；
  - `WFI`（`imm = 0x105`）从「冻结」改为**合法 NOP**（规范明文允许）；
  - `SRET`（`0x102`）与 `funct3 = 0` 的未定义 `imm` 从「冻结」改为 **illegal-instruction 陷入**（`mcause = 2`）；
  - `mtvec.MODE` 成为 **WARL = {0}**（写入被掩码 · 读回恒 Direct）；复位值
    `0x2000_0100` → **`0x8000_0100`**（**同一字索引 `0x040`** ⇒ 取指行为零变化，**读回值变化**）；
  - `mepc[1:0]` 写入被规范化（IALIGN = 32 ⇒ 恒 0）。
  ⇒ ⇒ **下游若把上述指令当作「已知未实现 / 冻结」处理，MUST 更新其期望**（新行为已由规范锁定）。
- **公开 API 移除 / 改名：无** ⇒ ⇒ 不走移除登记。
