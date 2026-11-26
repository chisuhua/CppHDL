# design · RV32I **`ECALL` / `EBREAK` / `WFI` + `mtvec`/`mepc` WARL**（P6-4）

> **Change**：`rv32i-ecall-ebreak-wfi-2026-11`
> **母 improvement**：`rv32i-privileged-csr-2026-11`（P6-4）
> **纪律**：TDD 5 步 · 期望值**手推**（禁抄仿真输出）· **零端口面变更**
> （核 IP 53 端口契约表 + C6 快照 51 **不动**）· 零新增 `HazardUnit` 端口

---

## 0. 三条**硬约束**

| # | 约束 | 出处 | 对本设计的含义 |
|---|---|---|---|
| **C1** | 父层 MUST 读「子组件**输出**端口」 | ADR-060 §2.1 · ADR-066 D1（P6-2/P6-3 实测） | ⇒ ⇒ `sys_trap_kind` MUST 从 `id_stage` 的**输出**取，MUST NOT 读 decoder 的输入 |
| **C2** | `CsrBank` 只有**一条**写口 | `rv32i_csr.h` | ⇒ ⇒ 本项**不新增写源**（仍只有 `CsrBank` 自身 + `ExceptionUnit` 两拍写 + P6-3 的仲裁） |
| **C3** | `HazardUnit` 端口面冻结（4 个直通端口） | 根 `AGENTS.md` 的 RV32I 红线 | ⇒ ⇒ 触发/原因码判据**全部落 `rv32i_pipeline.h`**，零新增 `HazardUnit` 端口 |

---

## 1. 裁决与规范依据

> 规范 = Privileged Spec **v20260120**（Machine-Level ISA **1.13**）。
> 裁决记录 = `.rddf/checks/decision-request-rv32i-privileged-d1-d2-2026-11.md`（**已裁决**）。

| 子问题 | 裁决 | 规范条文 | 实现位置 |
|---|---|---|---|
| **Q1.1** `mtvec.MODE` | **WARL = {0}**（只实现 Direct） | §2.1.1.7「`mtvec` 是 **WARL**」+ §1.1.3.3（读回任意**合法**值 ⇒ 合法集 = 硬件行为） | `rv32i_csr.h` 写路径掩码 |
| **Q1.2** `mtvec` 复位值 | **`0x8000_0100`**（I-TCM 区内） | §2.1.4 只要求「无 WARL 字段含非法值」⇒ 复位值**规范不规定** | `rv32i_csr.h` 初值 |
| **Q1.3** `mepc[1:0]` | **写入掩零** | §2.1.1.14「IALIGN=32 ⇒ the two low bits **are always zero**」⇒ **强制** | `rv32i_csr.h` 写路径掩码 |
| **Q2.1** `mcause[31]` | 异常 = **0**；中断 = **1**（P6-6） | §2.1.1.15「Interrupt bit **is set if … interrupt**」 | `rv32i_pipeline.h`（异常侧） |
| **Q2.1'** `mcause` coercion | **不做** | Exception Code 是 **WLRL**（§1.1.3.2）⇒ 任意读回**允许** | —（不新增逻辑） |
| **Q2.2** `ECALL` 原因码 | **11**（from **M**） | §2.1.3.1 + Table 6（`8` = from U ⇒ 本核不可达） | `rv32i_pipeline.h` |
| **Q2.3** `EBREAK` 原因码 | **3**（breakpoint） | §2.1.3.1 + Table 6 | `rv32i_pipeline.h` |
| **Q2.4** `WFI` 处置 | **合法 NOP** | §2.1.3.3「**a legal implementation is to simply implement the WFI instruction as a NOP**」 | `rv32i_decoder.h`（reason 0） |
| **Q2.5** 未定义 `imm` | `reason = 3` ⇒ 陷入 | `funct3=0` 的合法 `imm` 只有 `0x000/0x001/0x105/0x302` | `rv32i_decoder.h` |
| **Q2.5'** `SRET`（`0x102`） | `reason = 3` ⇒ 陷入（**不得** 1） | `misa.S = 0`（本核 M-only）⇒ 不可编码 ⇒ illegal-instruction | `rv32i_decoder.h` |

---

## 2. 实现

### 2.1 `funct3 = 0` 按 `imm[11:0]` 细分（`rv32i_decoder.h`）

| `imm[11:0]` | 指令 | `illegal_reason` | `sys_trap_kind` | 处置 |
|---|---|---|---|---|
| `0x000` | `ECALL` | **0**（合法） | **1** | 陷入（`mcause = 11`） |
| `0x001` | `EBREAK` | **0**（合法） | **2** | 陷入（`mcause = 3`） |
| `0x105` | `WFI` | **0**（合法） | 0 | **NOP**（正常退休） |
| `0x302` | `MRET` | **1**（已知未实现） | 0 | **冻结**（P6-5） |
| 其余（含 `0x102` SRET） | — | **3**（不可编码） | 0 | 陷入（`mcause = 2`） |

- **判定仅按 `imm[11:0]`**（`SYSTEM` 的 `rs1`/`rd` 在合法编码里全 0 ⇒ `instr[31:20]` 就是 imm）。
- 新输出 **`sys_trap_kind`**（2 bit · **具名**）**追加在 `illegal_reason` 之后** ⇒
  既有 `decoder_unnamed_output_N` / `id_stage_unnamed_output_N` 的**编号不位移**
  （端口按赋值顺序编号 · 实测踩过）。
- ⚠️ **`legal_opcode` MUST 含 ECALL/EBREAK/WFI** —— 否则它们会掉到 `reason = 2`
  （未知 opcode）⇒ ⇒ 触发条件被 `reason` 顺带命中，**掩盖** `sys_trap_kind` 的接线错误。

### 2.2 触发集与原因码（`rv32i_pipeline.h`）

```
trap_trigger = ((reason == 2) | (reason == 3) | (sys_trap_kind != 0))
               & if_stage.io().valid & !hazard.io().flush
exception_cause = ECALL ? 11 : (EBREAK ? 3 : 2)
```

- ⚠️ **MUST NOT** 把触发集放宽成 `reason != 0`：`MRET`（`reason = 1`）会随之 trap
  ⇒ ⇒ P6-5 前的「冻结」语义被静默替换。
- ⚠️ `!flush` **仍是承重条款**（P6-3 DA5）：错误路径上的 `ECALL`/`EBREAK` 不得写坏 `mepc`。
- 原因码是**组合**选出的 ⇒ `ExceptionUnit` 在 `start_trap` 拍**锁存**它（P6-3 既有机制）
  ⇒ ⇒ ECALL 的指令在下一拍被 `trap_flush` 杀掉也不影响原因码。

### 2.3 **合法但陷入**的指令 MUST 装气泡（**本项实测到的真缺陷形态**）

首版只做了 §2.2 ⇒ ⇒ SoC 级判据 `retire(20)` = **16**（期望 **13**）。

**根因**：`trap_flush` 只杀 IF/ID；`illegal_active`（`reason != 0`）才是「ID/EX 装气泡」的
承重信号 ⇒ ⇒ `ECALL` 的 `reason = 0` ⇒ 它在 trap 同拍**被锁进 ID/EX** ⇒
⇒ **越过 trap 照常退休**（`mepc`/`mcause` 也写了 ⇒ 架构自相矛盾）。

**修法**：`illegal_active` 的定义扩展为
`(reason != 0) | (sys_trap_kind != 0)` ⇒ ⇒ 与非法指令走**同一条**气泡路径
⇒ ⇒ 修复后 `retire(20)` = **13**（与 P6-3 的非法指令路径**逐字同形**）。

> 📌 判别力：这一格**只有 SoC 级退休计数**能抓到（核级 `mepc`/`mcause` 判据全绿 ——
> ECALL 既陷入、又退休）。⇒ ⇒ 变异 **M9** 钉的就是它（组件级/核级**结构性不可观测**）。

### 2.4 CSR 的 WARL coercion（`rv32i_csr.h`）

| 字段 | 改动 | 行为影响 |
|---|---|---|
| `mtvec[1:0]` | 写路径 `& 0xFFFF_FFFC` | 仅「写 MODE ≠ 0」这一种输入 ⇒ 读回不再与行为矛盾 |
| `mtvec` 初值 | `0x2000_0100` → `0x8000_0100` | **零**：两者都落到 I-TCM word **`0x040`**（`0x8000_0100` 在映射区内，不再依赖「取指丢高位」的偶然别名） |
| `mepc[1:0]` | 写路径 `& 0xFFFF_FFFC` | 仅「写奇数 PC」这一种输入 ⇒ `MRET`（P6-5）不可能跳非对齐 |

- **掩码值 MUST 是 `0xFFFF_FFFC`**（两位），**MUST NOT** 用 `~0x1`：IALIGN=32 约束的是 `[1:0]`。
- `mtvec` 的 `BASE` 字段本身**已经**隐含 4 字节对齐（`[1:0]` 不是 BASE）⇒ 掩码不改变合法写入。

### 2.5 注释订正（`rv32i_exception.h`，纯注释）

旧表把**异常码与中断码混写**（`3` = MSI · `8` = ECALL · `11` = MEI）⇒ 与异常侧
（`3` = breakpoint · `11` = ECALL-from-M）**直接冲突**。改为**两张表**并标注 `bit31` 的语义。

---

## 3. 判据设计

| 判据 | 文件 | AC | 关键期望值（**手推**） |
|---|---|---|---|
| `TRAP - ECALL 陷入：mcause = 11…` | `examples/riscv-mini/tests/test_rv32i_trap_entry.cpp` | AC1 | `mepc = 0x8000_0000` · `mcause = 11` · `reason` 恒 0 · ECALL 不退休 |
| `TRAP - EBREAK 陷入：mcause = 3` | 同上 | AC2 | `mcause = 3` |
| `TRAP - SRET（0x102）…` | 同上 | AC3 | `reason = 3` · `mcause = 2` |
| `TRAP - SYSTEM funct3=0 的未定义 imm…` | 同上 | AC5 | `reason = 3` · `mcause = 2` |
| `TRAP - WFI（0x105）⇒ 合法 NOP…` | 同上 | AC4 | `mepc` / `mcause` **仍 0**（无 trap）· 后续 `csrrs` 有写回 |
| `TRAP - MRET（0x302）P6-5 前仍 reason=1…` | 同上 | AC6 | **零写回** · `reason` 恒 1 · 退休恒 0 |
| `CSR WARL coercion: mtvec MODE and mepc IALIGN=32` | `examples/riscv-mini/tests/test_csr_unit.cpp` | AC7 · AC8 | 写 `…03`/`…01` ⇒ 读 `…00`；写 `…37` ⇒ `…34` · `…3F` ⇒ `…3C` |
| `CSR reset values`（订正） | 同上 | AC9 | `mtvec` 复位 = `0x8000_0100` |
| `Rv32iSoc - IllegalOpcodeObservableNotRetired + FenceIsNop`（订正） | `examples/riscv-mini/tests/test_rv32i_soc_dual_parity.cpp` | AC1 · AC9 | ECALL：`max_reason = 0` · `retire(20) = 13` · ID PC ≠ 探针；新增 MRET 行（`reason = 1` · `retire = 8` · 冻结） |
| `CSR - funct3=0（ECALL）⇒ reason 0…`（订正） | `examples/riscv-mini/tests/test_rv32i_csr_instructions.cpp` | AC1（译码面） | 无非零 reason · 不冻结 |

---

## 4. 爆炸半径

| 文件 | 改动 |
|---|---|
| `include/cpu/pipeline/rv32i_decoder.h` | `funct3=0` 细分 + `sys_trap_kind` 输出 |
| `include/cpu/pipeline/stages/id_stage.h` | `sys_trap_kind` 透传（`valid` 门控） |
| `include/cpu/pipeline/rv32i_pipeline.h` | 触发集扩展 + 原因码 mux + `illegal_active` 扩展 |
| `include/cpu/riscv/rv32i_csr.h` | `mtvec` 初值 / 两个写路径掩码 / 注释 |
| `include/cpu/riscv/rv32i_exception.h` | 原因码注释分表（纯注释） |
| `include/cpu/AGENTS.md` | 事实与计数同步 |
| 4 个测试文件 | 见 §3（3 处语义订正 + 6 个新用例） |

---

## 5. 边界（**明确不做**）

- **`MRET` 实现**（P6-5）⇒ 本项只保证它仍 `reason = 1`；
  ⚠️ `ECALL` 的 `mepc` 指向 **ECALL 自身**（规范 §2.1.3.1）⇒ handler **MUST** 自己 `+4`
  再 `MRET`，否则**无限 trapping**（P6-5 的判据必须覆盖这一点）。
- **中断**（P6-6）· **`mtval`**（P6-7）· **Vectored 模式**（本项 WARL = {0}）。
- **`mcause` 的 coercion**：WLRL ⇒ 不强制（软件写任意值可读回）。
- **trap 与 CSR 写同拍**：沿用 P6-3 的仲裁（trap 优先 · 被抑制的 CSR 写不补偿重试）。
- **`funct3 = 0` 的 `rs1` / `rd` 非零形态未校验**：ISA 要求这些位为 0；本实现只看
  `imm[11:0]` ⇒ 形如「`ECALL` + 非零 `rs1`」的编码会被当作同一条 `ECALL`（**未按 illegal 处理**）
  ⇒ ⇒ 如实登记为边界（不在本项 AC 内）。

---

## 6. 被排除的错法（每条都是**实测或可机械判定**的错误）

| 错法 | 后果 |
|---|---|
| `ECALL` 归 `reason = 1` | 冻结 ⇒ 固件挂死（旧行为） |
| 触发集放宽成 `reason != 0` | `MRET` 也 trap ⇒ P6-5 语义被静默替换 |
| 只加触发、不加气泡（§2.3） | ECALL 自己退休 ⇒ `retire(20) = 16 ≠ 13`（**实测**） |
| `ECALL` 用 `8` | 那是 U-mode 的原因码（本核不可达） |
| `WFI` 判非法 | 规范**不要求**（WFI 是合法指令）⇒ 轮询固件误 trap |
| `SRET` 归 `reason = 1` | 冻结 ⇒ hang（规范要求 illegal-instruction） |
| 去掉 `mtvec` 写掩码 | 读回 ≠ 行为（WARL 违规） |
| `mtvec` 复位值改成 `0` | 落到 word 0 ⇒ **重启程序**（行为变化） |
| `mepc` 掩码写成 `& ~0x1` | IALIGN=32 要求 `[1:0]` 均 0 |

---

## 7. 与既有 capability 的接缝

- `rv32i-isa-semantics` 的非法指令条款（`reason ∈ {2,3}` ⇒ 陷入）**不**由本项修订：
  `ECALL`/`EBREAK` 是 `reason = 0` 的**另一类触发源**（新契约写在本 capability）。
- `rv32i-privilege` 的 `## Purpose` 需在**归档后**手工补第三段（「`funct3=0` 的细分 +
  WARL」）—— `openspec archive` 的 MODIFIED 是**追加语义**，不删旧段。

---

## 8. 变异矩阵（每条须**仍能编译**；「转红」= 有判别力）

| # | 变异 | 目标判据 | 期望 | 实测 |
|---|---|---|---|---|

> **12/12 转红**，且每条变异都**编译通过**（`cmake --build` 成功后才跑判据）。

| **M1** | `ECALL` 原因码写 `2` | `TRAP - ECALL*` | 红 | **红**（编译通过） |
| **M2** | `ECALL` 归 `reason = 1` | `TRAP - ECALL*` | 红 | **红**（编译通过） |
| **M3** | `EBREAK` 原因码写 `2` | `TRAP - EBREAK*` | 红 | **红**（编译通过） |
| **M4** | `SRET` 归 `reason = 1` | `TRAP - SRET*` | 红 | **红**（编译通过） |
| **M5** | `WFI` 归 `reason = 1` | `TRAP - WFI*` | 红 | **红**（编译通过） |
| **M6** | 触发集去掉 `sys_trap_kind` | `[p6-4]`（6 用例） | 红 | **红**（编译通过） |
| **M7** | 去掉 `mtvec` 写掩码 | `CSR WARL coercion*` | 红 | **红**（编译通过） |
| **M8** | 去掉 `mepc` 写掩码 | `CSR WARL coercion*` | 红 | **红**（编译通过） |
| **M9** | `illegal_active` 不含 `sys_trap_kind` | SoC `IllegalOpcode…` | 红 | **红**（编译通过） |
| **M10** | `mtvec` 复位值回 `0x2000_0100` | `CSR reset values` | 红 | **红**（编译通过） |
| **M11** | 未定义 `imm` 不归 `reason = 3` | `TRAP - SYSTEM funct3=0*` | 红 | **红**（编译通过） |
| **M12** | `MRET` 不归 `reason = 1` | `TRAP - MRET*` | 红 | **红**（编译通过） |
