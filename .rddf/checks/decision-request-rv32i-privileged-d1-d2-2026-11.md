---
id: decision-request-rv32i-privileged-d1-d2-2026-11
日期: 2026-11-26
提出者: builder（P6-3 收口后 · D-1 / D-2 的**裁决推进**）
状态: ✅ **已裁决（2026-11-26）**
关联:
  - .rddf/improvements/rv32i-privileged-csr-2026-11.md（**D-1** / **D-2** / R7）
  - openspec/changes/archive/2026-11-26-rv32i-trap-entry-2026-11/（**P6-3 已闭**）
  - openspec/specs/rv32i-privilege/spec.md（§「⚠️ 边界」段已登记这两项为**排除**）
evidence: probe:include/cpu/riscv/rv32i_csr.h
---

# 决策请求单：RV32I 特权级 **D-1**（`mtvec` 取值语义）· **D-2**（`mcause` 编码表 / `WFI` 归属）

## 0. 一句话

两项各自**阻塞下一批的期望值**：不裁 ⇒ **P6-4 / P6-5 / P6-6** 的判据只能「先写实现、
再按实现抄期望」⇒ 那是**拿实现当规范**（本仓明文禁止：期望值 MUST **手推**）。

| 项 | 阻塞谁 | 阻塞什么 |
|---|---|---|
| **D-1**（`mtvec` / `mepc`） | P6-5（`MRET` 返回）· P6-6（中断向量） | 「trap 落点是否**可达**」与「`MRET` 回到哪」的**期望值** |
| **D-2**（`mcause` / `WFI`） | P6-4（`ECALL`/`EBREAK`）· P6-6（中断）· P6-7（`mtval`） | `mcause` 的**数值**与 `WFI` 的**处置** |

⇒ ⇒ **推荐一次裁完**（§4 的打包），因为 D-1/D-2 的选项**互相独立**、且都在 `CsrBank` /
`ExceptionUnit` / 解码器这**同三个文件**上落地。

---

## 1. 事实（代码级 · 逐条可复现）

| # | 事实 | 位置 | 判定 |
|---|---|---|---|
| **F1** | `mtvec` 复位值 = `0x2000_0100`，**注释写 `MODE(1:0)=01`** | `include/cpu/riscv/rv32i_csr.h:94-96` | ⚠️ **注释错**：`0x100 & 0x3 == 0` ⇒ MODE 实际是 **00**（Direct）（母 improvement 的 **R7** 已登记） |
| **F2** | `mtvec` 写路径**无 WARL coercion**：`mtvec->next = select(we & sel_mtvec, csr_wdata, mtvec)` | `rv32i_csr.h:175-179` | 可写入 MODE ∈ {2,3}（**保留值**）· 也可写入 MODE=1 |
| **F3** | `ExceptionUnit` **恒取 Direct**：`mtvec_base = csr_mtvec & ~0x3`，`trap_pc = mtvec_base`（**异常与中断同路**） | `include/cpu/riscv/rv32i_exception.h` | **Vectored 未实现** ⇒ 与 F2 叠加后 **读回 ≠ 行为**（WARL 违规形态） |
| **F4** | `0x2000_0100` **不在任何映射区**：`kItcm = {0x8000_0000, 16 KB}` · `kDtcm = {0x8001_0000, 16 KB}` · MMIO 自 `0x8010_0000` | `include/cpu/riscv/rv32i_mem_map.h:62-97` | ⚠️ trap 落到 I-TCM **word `0x040`** 只因为取指做 `bits<13,2>`（**高位被丢弃**）⇒ **偶然别名**（P6-3 实测 · 不是有意的地址映射） |
| **F5** | `mepc` 写路径**无对齐 coercion** | `rv32i_csr.h:187-191` | IALIGN=32 下 `mepc[1:0]` 应恒 0 ⇒ 写入奇数 PC 会**原样读回** |
| **F6** | `mcause` 写路径**无 coercion** | `rv32i_csr.h:193-197` | 软件可写任意 32 位值（含 `bit31=1`）⇒ 规范上**合规**（Exception Code 是 **WLRL**，见 §1.5.2） |
| **F7** | 流水线**只写一种 cause**：`exception.io().exception_cause <<= ch_uint<32>(2)` | `include/cpu/pipeline/rv32i_pipeline.h:586` | 当前 `mcause` 恒 **2**（illegal） |
| **F8** | `rv32i_exception.h` 头注释的「异常原因码」表**混用了中断码与异常码**：写 `3 = MSI` / `11 = MEI` / `8 = ECALL` | `rv32i_exception.h:9-14` | ⚠️ **三处错**（见 §3.1）：`3` 作**异常码** = **breakpoint**；`11` 作**异常码** = **ECALL from M**；`8` = ECALL from **U**（本核**不可达**） |
| **F9** | 解码器**只按 `funct3`** 分类 `OP_SYSTEM` ⇒ `funct3 = 0` **全桶**（`ECALL`/`EBREAK`/`MRET`/`WFI`/**未定义 `imm`**）⇒ `reason = 1` 一处 | `include/cpu/pipeline/rv32i_decoder.h:215-249` | ⇒ **P6-4 MUST 解 `imm[11:0]`**（见 Q2.5） |
| **F10** | **唯一钉住复位值**的断言：`REQUIRE(mtvec_val == 0x20000100)`；另有 `mtvec_out` 写-读回断言（`0x20000100` 低 2 位 = 0 ⇒ 掩码后仍相等） | `examples/riscv-mini/tests/test_csr_unit.cpp:98` · `:734/:759` | 改复位值 ⇒ **1 处断言**；`& ~0x3` 掩码对 `:734/:759` **无影响** |
| **F11** | `WFI` 的既有「判据」是**空断言**（`test_csr_unit.cpp:483` 标题写「无副作用」，正文只验证「CSR 写入照常」） | `test_csr_unit.cpp:483-510` | 现状**没有**任何判据覆盖 `WFI` 的**解码处置** |

---

## 1.5 规范原文核对（Privileged Spec **v20260120** · Machine-Level ISA **1.13**）

> 来源：`docs.riscv.org/reference/isa/v20260120/priv/`（在线发布版）与
> `riscv/riscv-isa-manual` 的 `main` 分支 asciidoc 源，逐字一致。只摘**与 Q1/Q2 有判别力**的条文。

### 1.5.1 `mtvec`（§2.1.1.7）

> "The `mtvec` register is an MXLEN-bit **WARL** read/write register that holds trap vector
> configuration, consisting of a vector base address (BASE) and a vector mode (MODE)."

MODE：`0 = Direct`（**all traps** 置 `pc = BASE`）· `1 = Vectored`（**仅异步中断** 置
`pc = BASE + 4×cause`，**同步异常仍为 BASE**）· `≥2 = Reserved`。

- **WARL 语义**（§1.1.3.3）：写不支持值**不抛异常**，读回**任意合法值**，但须**确定性地**
  依赖写入值 ⇒ 「只支持 Direct 时写 `MODE=1` 读回 0」是**推导结论**，规范**未逐字写**。
- **复位值：规范不给**。§2.1.4 只要求「**No WARL field contains an illegal value**」+
  「All other hart state is UNSPECIFIED」⇒ `0x2000_0100`（MODE=0）**合法**。
- BASE MUST **4 字节对齐**。

### 1.5.2 `mcause`（§2.1.1.15）

- 位域：`mcause[MXLEN-1]` = **Interrupt**（1 bit）· `mcause[MXLEN-2:0]` = **Exception Code**，
  后者是 **WLRL**（**不是** WARL）。
- RV32 ⇒ 中断位 = **bit 31**；"The Interrupt bit ... is set if the trap was caused by an
  interrupt" ⇒ 异常 ⇒ 该位 **0**（NOTE：可用「单条符号位分支」区分中断 / 异常）。
- Table 6：`2` illegal · **`3` breakpoint** · `8` ECALL from U · `9` ECALL from S ·
  **`11` ECALL from M**（`10` Reserved）。
- **WLRL 语义**（§1.1.3.2）：写非法值「**permitted but not required**」抛 illegal；
  读回**可为任意位型**，须确定性依赖写入值与前值。

### 1.5.3 `mepc`（§2.1.1.14）

> "The low bit of `mepc` (`mepc[0]`) is always zero. **On implementations that support only
> IALIGN=32, the two low bits (`mepc[1:0]`) are always zero.**"

本核 IALIGN=32 ⇒ `mepc[1:0]` **恒 0**（**规范强制**）。「masked on reads / 写仍可写」那条
**只**适用于 IALIGN ∈ {16, 32} 的实现 ⇒ **不适用**本核。

### 1.5.4 `WFI`（§2.1.3.3）

> "**WFI is available in all privileged modes**" · "**a legal implementation is to simply
> implement the WFI instruction as a NOP.**"

- 唯一非法路径 = `mstatus.TW=1` 时的**低特权模式**执行；而 "**TW is read-only 0 when there
  are no modes less privileged than M**" ⇒ **纯 M-mode 实现里 WFI 永不非法**。
- 编码：`funct7=0001000` · `rs2=00101` ⇒ `imm[11:0] = 0x105`。

### 1.5.5 `ECALL` / `EBREAK` / `MRET` / `SRET`

| 指令 | 编码（`opcode=1110011` · `funct3=000`） | 语义 |
|---|---|---|
| `ECALL` | `imm = 0x000` | 异常码**取当前特权级**：U→8 · S→9 · **M→11**；`mepc` = **指令自身**地址 |
| `EBREAK` | `imm = 0x001` | **breakpoint（3）**；`mepc` = **指令自身**地址 |
| `MRET` | `imm = 0x302` | "**MRET is always provided**"；`pc ← mepc`；弹 `MIE←MPIE` · `MPIE←1` · `MPP←最低支持模式`（M-only 即 M） |
| `SRET` | `imm = 0x102` | S-mode 指令；本核 `misa = 0x4010_1101`（MXL=01 · A/I/M/F，**无 S**）⇒ **M-only 下 illegal** |

### 1.5.6 规范判定汇总（**强制** vs **选择**）

| 子问题 | 规范怎么说 | 对选项的约束 |
|---|---|---|
| Q1.1 | MODE 是 WARL；不实现 Vectored ⇒ 合法集 {0} | **A 合规**；B 也合规（但 MUST 真实现 `BASE+4×cause`）；**A/B 都必须把 `≥2` 的保留值 coerce 掉** |
| Q1.2 | **规范不给 `mtvec` 复位值** | A / B **都合规** ⇒ 纯工程语义选择 |
| Q1.3 | IALIGN=32 ⇒ `mepc[1:0]` **恒 0** | **不是选项**：不 coerce 直接违反规范 |
| Q2.1 | Exception Code 是 **WLRL**（允许任意读回） | 「不加 coercion」**明确合规** |
| Q2.2 | ECALL from M = **11** | 确认（`8` 是 U-mode ⇒ 本核不可达） |
| Q2.3 | EBREAK = **3** | 确认 |
| Q2.4 | WFI 合法；NOP 是规范明文允许的实现 | **A 合规**；B（判非法）**不合规** |
| Q2.5 | 合法 imm = `0x000`/`0x001`/`0x105`/`0x302`；`SRET(0x102)` 在 M-only 下非法 | 确认 + **SRET MUST trap**（`reason` **不得**取 1，那在本核语义里是「冻结」⇒ 会 hang） |

---

## 2. D-1：`mtvec` 的取值语义（3 个子问题）

### Q1.1 `mtvec.MODE`：WARL = `{0}`（Direct only）还是 `{0,1}`（实现 Vectored）？

**现状**：行为 = Direct（F3），读回 = 任意（F2）⇒ **读回与行为不一致**。

**选项**

| 选项 | 内容 | 代价 / 收益 |
|---|---|---|
| **A（推荐）** | **WARL = {0}**：写入时掩码 `mtvec[1:0] = 0` ⇒ 读回恒为 Direct | 1 个 AND + **1 条判据**（写 `…01` 读回 `…00`）；**P6-6 的中断也走 BASE** ⇒ 软件在 handler 内按 `mcause` 分发 ⇒ **完全合规**（规范允许不实现 Vectored） |
| **B** | **WARL = {0,1}**：`trap_pc = select(是中断 & MODE[0], BASE + 4×cause, BASE)` | +1 mux + `cause` 位宽/边界判据；收益 = 中断延迟更低（仅 P6-6 有意义） |

**推荐 A**。理由：本核的定位是「可替换 IP + 教学」⇒ 中断数量级小、handler 分发成本可忽略；
A **不需要**在 P6-6 里额外处理「mode=1 且 cause 越界」的边界（那会引入一个新的**未定义行为面**）。
⇒ ⇒ 若选 B，`4×cause` 的 `cause` MUST 取 `mcause[30:0]`（**中断码**，不含 `bit31`），
且 MODE=2/3 仍 MUST 被 coerced（WARL）。

> 📌 **WARL 的硬要求（与选项无关）**：读回 MUST 是**合法值**，而「合法」由硬件实际行为定义
> （§1.5.1）⇒ 现状（F2+F3）违反这一条 ⇒ ⇒ 无论选 A 还是 B，**都必须给 `mtvec` 的写路径加掩码**。

### Q1.2 `mtvec` 的**复位值**：`0x2000_0100`（未映射）保留，还是改成映射地址？

**现状**：`0x2000_0100` **不在任何映射区**（F4）；它能「工作」（P6-3 的 handler 在 word `0x040`）
**只因为**取指把高位丢了 ⇒ 属于**偶然别名**，不是设计。

| 选项 | 内容 | 行为变化 | 代价 |
|---|---|---|---|
| **A** | 保持 `0x2000_0100`，文档明说「未映射 ⇒ 复位后软件 **MUST** 设 `mtvec`」 | 无 | 0；但「reset + trap」仍落到一个**语义上无意义**的地址 |
| **B（推荐）** | 改成 **`0x8000_0100`**（I-TCM 区内 · **落在同一个 word `0x040`**） | **零**（同一 word） | 1 行初值 + **1 处断言**（F10）+ 3 处注释（含 P6-3 的「别名」措辞） |
| **C** | 改成 `0` | **变**（落到 word 0 ⇒ **重启程序**） | 同 B，且需要重新定义「trap 后行为」 |

**推荐 B**。理由：**零行为变化**，但把「偶然别名」变成「**明确的向量地址**」——
`mtvec` 的读回从此是一个**合法代码地址**（对调试器 / 用户可见）。

### Q1.3 `mepc[1:0]`：写入是否 coerce 为 0？（**规范强制 · 无选项**）

**现状**：不 coerce（F5）⇒ 写奇数 PC 会**原样读回**。

> 🚨 **这条不需要裁决**（§1.5.3）：IALIGN=32 的实现 "the two low bits (`mepc[1:0]`) are
> **always zero**" ⇒ 现状**违反规范**，MUST 修。下表只说明**怎么修**：

| 做法 | 内容 | 后果 |
|---|---|---|
| **A（唯一合规）** | 写入时 `mepc[1:0] = 0`（`mepc->next = select(we & sel_mepc, wdata & ~0x3, mepc)`） | 1 个 AND + **1 条判据**（写 `0x…7` 读回 `0x…4`）；`MRET`（P6-5）**不可能**跳到非对齐 PC |
| ~~B~~ | 不 coerce，由软件保证 | **规范违反**（读回 ≠ "always zero"）；`MRET` 可跳非对齐 ⇒ 落到本核**未实现**的 misaligned 面 |

**结论**：A 无替代。P6-5 的判据（「`MRET` 回 `mepc`」）因此可**手推** —— 任何写入都被规范化。

---

## 3. D-2：`mcause` 编码表 + `WFI` 归属（5 个子问题）

### 3.1 先钉**语义拆分**（这是 F8 那个错误注释的根源）

`mcause` 的低 31 位是**编码**，其含义**取决于 `bit31`**：

| `bit31` | 含义 | code 例子（本核相关） |
|---|---|---|
| **0** | **异常**（exception） | `2` = illegal instruction · **`3` = breakpoint（EBREAK）** · **`11` = ECALL from M-mode** |
| **1** | **中断**（interrupt） | `3` = machine software · `7` = machine timer · `11` = machine external ⇒ 写作 `0x8000_0003` / `0x8000_0007` / `0x8000_000B` |

⇒ ⇒ **同一数字在两侧含义完全不同**（`3`：异常侧 = breakpoint · 中断侧 = MSI）——
`rv32i_exception.h:9-14` 的表**正是**把两侧混在一张表里（F8）⇒ 必须重写。

> 原文核对（§1.5.2）：`mcause[MXLEN-1]` = **Interrupt**（1 bit）· `mcause[MXLEN-2:0]` =
> **Exception Code（WLRL）**；Table 6 的异常码 = `2` illegal / **`3` breakpoint** /
> `8` ECALL-U / `9` ECALL-S / **`11` ECALL-M**。

### Q2.1 `mcause[31]` 的写法

| 处置 | 推荐 |
|---|---|
| **异常** ⇒ `bit31 = 0`（`mcause` = 2 / 3 / 11） | ✅ 确认（现状 `2` 已合规，F7） |
| **中断** ⇒ `bit31 = 1`（P6-6 写 `0x8000_0000 \| code`） | ✅ 确认（**本版不实现**，只登记数值规则） |
| **是否给 `mcause` 加 coercion**（只接受已实现的 code） | **不做**。Exception Code 是 **WLRL**（§1.5.2）⇒ 规范 **permitted but not required** 抛异常、且**允许任意读回** ⇒ 现状（F6）**合规**；判据只钉「**流水线侧**写入的 code 属于已定义集」 |

### Q2.2 `ECALL` 的 cause = **11**（M-mode）

本核**只有 M-mode** ⇒ `ECALL from M` ⇒ **11**（不是 8 = U-mode ⇒ **不可达**）。✅ 确认。
⚠️ 副作用：`ECALL` 的 `mepc` 指向 `ECALL` **自身** ⇒ handler **MUST** 自己 `+4` 再 `MRET`，
否则 **无限 trapping**（母 improvement 的 P6-4 行已登记）。

### Q2.3 `EBREAK` 的 cause = **3**

✅ 确认（与「中断侧 `3` = MSI」**不冲突**——`bit31` 区分，见 §3.1）。

### Q2.4 `WFI` 的处置

**现状**：`funct3 = 0` 全桶 ⇒ `reason = 1` ⇒ **冻结**（固件挂死）。

| 选项 | 内容 | 合规性 | 推荐 |
|---|---|---|---|
| **A（推荐）** | **合法 NOP**（正常退休 · 零副作用） | ✅ 合规：§1.5.4 明文「**a legal implementation is to simply implement the WFI instruction as a NOP**」 | ★ |
| **B** | 当作 **illegal**（`mcause = 2`） | ⚠️ 规范**不要求**（WFI 是合法指令；M-only 下 `TW` 恒 0 ⇒ 无非法路径）⇒ 会让「WFI 轮询固件」误 trap | ✗ |
| **C** | 维持 `reason = 1`（冻结） | ✗ 既不合规也不可用（hang） | ✗ |

**推荐 A**。低功耗语义（真睡到中断）**不在**本核范围 ⇒ 用 NOP 表达「已实现、无副作用」。

### Q2.5 `funct3 = 0` 的**未定义 `imm`**（P6-4 的设计输入）

`SYSTEM` / `funct3 = 0` 的合法 `imm[11:0]`（§1.5.5 编码核对）：
`0x000` = `ECALL` · `0x001` = `EBREAK` · `0x105` = `WFI` · `0x302` = `MRET`
⇒ ⇒ **其余取值 ⇒ illegal**，其中 **`0x102`（`SRET`）是「已定义但本实现不可用」**
（`misa` 无 S 位）⇒ **MUST trap**，见下表。

| 子问题 | 推荐 |
|---|---|
| 未定义 `imm` 的 `reason` 码用哪个？ | **`3`（保留组合）**——现有分类学里 `3` 的定义是「已知 opcode 但 `funct3`/`funct7` 是保留组合」⇒ 把 `imm` 纳入同一语义（**不新增第 5 态**） |
| `SRET`（`imm = 0x102`）归到哪？ | **`reason = 3`**（trap）。**不得用 `reason = 1`** —— 那个桶在本核语义里是**冻结**（`reason=1` ⇒ 不 trap、PC 不前进）⇒ SRET 会 **hang**，而规范要求 **illegal-instruction exception** ⇒ 需把 `reason = 3` 的语义从「保留组合」**扩为「本实现不可编码」**（扩定义，不新增状态） |
| `reason = 1` 是否因此**清空**？ | **是**：P6-4 落地后 `funct3 = 0` **不再有**「已知未实现」桶 ⇒ `reason = 1` 只剩**未落地的 `imm`**（`0x302` MRET 在 P6-5 前）⇒ ⇒ 建议 **P6-4 一并处理**（ECALL/EBREAK/WFI 落地，MRET 留 `1` 到 P6-5） |

---

## 4. 推荐打包（**一次裁完**）

| # | 子问题 | 推荐 | 实现成本 | 落到哪一步 |
|---|---|---|---|---|
| Q1.1 | `mtvec.MODE` | **WARL = {0}**（Direct only · 写入掩码） | 1 个 AND + 1 条判据 | **P6-4 的 change**（与 Q1.2/Q1.3 同批 ⇒ 满足「golden 变更 ≤ 2」） |
| Q1.2 | `mtvec` 复位值 | **`0x8000_0100`**（零行为变化） | 1 行 + 1 处断言 + 注释 | 同上 |
| Q1.3 | `mepc[1:0]` | **写入 coerce 为 0**（⚠️ **规范强制**，非选项） | 1 个 AND + 1 条判据 | 同上 |
| Q2.1 | `mcause[31]` | 异常 `0` / 中断 `1`；**不加 coercion**（WLRL 允许） | 0（只登记规则） | P6-4（异常）· P6-6（中断） |
| Q2.2 | `ECALL` | **11** | 常量 | P6-4 |
| Q2.3 | `EBREAK` | **3** | 常量 | P6-4 |
| Q2.4 | `WFI` | **合法 NOP** | 解码器 1 个分支 | P6-4 |
| Q2.5 | 未定义 `imm`（含 `SRET 0x102`） | `reason = 3`（语义扩为「本实现不可编码」） | 解码器细分 | P6-4 |

⇒ ⇒ **代码面总成本**：`rv32i_csr.h` 3 处（`mtvec` 掩码 / 初值 / `mepc` 掩码）+ 解码器 1 处细分
+ **3 条新判据**；**零端口面变更**；**零公开 API 变更**。

---

## 5. 爆炸半径（若采纳 §4）

| 文件 | 改动 | 影响面 |
|---|---|---|
| `include/cpu/riscv/rv32i_csr.h` | `mtvec` 写路径掩码 `& ~0x3` · 初值 `0x8000_0100` · `mepc` 写路径掩码 `& ~0x3` · **订正 F1 的错注释** | 行为变化**仅**在「写 MODE≠0」「写 `mtvec` 后读回」「写奇数 `mepc`」三种输入上 |
| `include/cpu/riscv/rv32i_decoder.h` | `funct3 = 0` 按 `imm[11:0]` 细分（`0x000`/`0x001`/`0x105`/`0x302`/其余 ⇒ `3`） | **P6-4 的核心**（F9） |
| `include/cpu/riscv/rv32i_exception.h` | **重写** F8 的「原因码」注释（拆异常 / 中断两张表） | 纯注释（无行为） |
| `examples/riscv-mini/tests/test_csr_unit.cpp` | `mtvec` 复位值断言（F10）· **+3 条判据**（MODE 掩码 / `mepc` 对齐 / `WFI` 解码） | 判据增量 |
| `test_rv32i_trap_entry.cpp` · `test_rv32i_csr_instructions.cpp` · `test_rv32i_soc_dual_parity.cpp:2536` · `include/cpu/AGENTS.md:297` · 验收报告 | 「别名 word `0x040`」措辞 ⇒ 「映射地址 `0x8000_0100`（word `0x040`）」 | **判据值不变**（同一 word）；纯注释 |
| `openspec/specs/rv32i-privilege/spec.md` | 「⚠️ 边界」段：`mtvec` 取值语义 / `mepc` 对齐**从排除清单移出**（由 P6-4 的 change 以 MODIFIED delta 兑现） | 文档 |

---

## 6. 裁决记录

> **待用户填**（每项写选项字母；可只写「全部按推荐」）。
> ⚠️ **Q1.3 无需裁决** —— 规范强制（IALIGN=32 ⇒ `mepc[1:0]` 恒 0），照做即可。

| # | 子问题 | 推荐 | 你的裁决 |
|---|---|---|---|
| Q1.1 | `mtvec.MODE` | **A**（WARL = {0}） | ✅ **A** |
| Q1.2 | `mtvec` 复位值 | **B**（`0x8000_0100` · 零行为变化） | ✅ **B** |
| Q1.3 | `mepc[1:0]` | **A**（**规范强制** ⇒ 无替代） | ✅ A（无需裁决） |
| Q2.1 | `mcause[31]` | 异常 `0` / 中断 `1`（**不加 coercion**） | ✅ 按推荐 |
| Q2.2 | `ECALL` | **11** | ✅ 按推荐 |
| Q2.3 | `EBREAK` | **3** | ✅ 按推荐 |
| Q2.4 | `WFI` | **A**（合法 NOP） | ✅ **A** |
| Q2.5 | 未定义 `imm` / `SRET` | `reason = 3` | ✅ 按推荐 |

**裁决结论**：D-1 = **A**（`MODE` WARL={0} + 复位值 `0x8000_0100`）· D-2 = **全部按推荐**。
**落地**：change `openspec/changes/archive/2026-11-26-rv32i-ecall-ebreak-wfi-2026-11/`（P6-4 · **已归档** 2026-11-26）。

**裁决后由谁落地**：Q1.x + Q2.2–Q2.5 ⇒ **P6-4 的 change**（builder）· Q2.1 的中断半 ⇒ **P6-6**；
若某项需要 ADR（例如选 Q1.1 = B，则「Vectored 模式的 cause 边界」属架构决策）⇒ **architect**。
