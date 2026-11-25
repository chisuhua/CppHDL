# design · RV32I CSR 指令（6 种 `funct3`）

> **Change**：`rv32i-csr-instructions-2026-11`
> **母 improvement**：`rv32i-privileged-csr-2026-11`（P6-2）
> **纪律**：TDD 5 步（**先见红**）· 每步小迭代 · 每步跑验证 · 提交**单独成段**
> ⇒ ⇒ **零端口面变更**（核 IP 顶层 53 端口契约表 + C6 快照**不动**）

---

## 0. 三条**硬约束**（先读这三条，否则设计会踩坑）

| # | 约束 | 出处 | 对本设计的含义 |
|---|---|---|---|
| **C1** | ⚠️ **decoder 的端口按**赋值顺序**编号** ⇒ ⇒ 插在中间会让既有 `decoder_unnamed_output_N` **整体位移** ⇒ ⇒ 独立 Verilog TB 的端口表**全部错位**（实测踩过） | `include/cpu/pipeline/rv32i_decoder.h` 的输出段注释 | ⇒ ⇒ **零新增 decoder 端口** ⇒ ⇒ CSR 指令的识别 MUST 从**已有**的 `opcode` + `funct3` **推导** |
| **C2** | ⚠️ **子组件的端口会冒泡到产物顶层**（未驱动 / 未消费的输出）⇒ ⇒ decoder 曾使产物端口 118 → 126 | `include/cpu/AGENTS.md`（B4 · C4 记录） | ⇒ ⇒ 新增的 stage 端口 MUST **被顶层消费**（被消费 ⇒ **不冒泡**）⇒ ⇒ ⇒ **产物端口表零变化**（⇒ ⇒ AC10 MUST **实测比对**） |
| **C3** | ⚠️ **`HazardUnit` 端口面冻结在 4 个直通端口** ⇒ 不得再接 `ex_alu_result`（环饵） | 根 `AGENTS.md:256` | ⇒ ⇒ **RAW 判据 MUST NOT 放进 `HazardUnit`** ⇒ ⇒ 落 `rv32i_pipeline.h` 顶层 ⇒ ⇒ **零新增 `HazardUnit` 端口** ⇒ ⇒ ⇒ **红线不受影响**（用户 2026-11-25 已确认该走法） |

---

## 1. 译码：`illegal_reason` 的细化（C1 ⇒ 零新增端口）

### 1.1 `OP_SYSTEM` 的 `funct3` 分类表

`OP_SYSTEM = 0b1110011`（`0x73`）的 `funct3`（bits 14:12）：

| `funct3` | 指令 | 本项处置 | `illegal_reason` |
|---|---|---|---|
| `000` | `ECALL` / `EBREAK` / `MRET` / `WFI` | **OUT**（P6-4 / P6-5 · `WFI` 归属未定） | **1**（已知未实现）**不变** |
| `001` | `CSRRW` | **IN** | **0**（合法） |
| `010` | `CSRRS` | **IN** | **0** |
| `011` | `CSRRC` | **IN** | **0** |
| `100` | **保留** | — | **3**（保留组合） |
| `101` | `CSRRWI` | **IN** | **0** |
| `110` | `CSRRSI` | **IN** | **0** |
| `111` | `CSRRCI` | **IN** | **0** |

### 1.2 判据的**优先级**（承接 ADR-071 v1.2 的 `1 > 3 > 2 > 0`）

原判定链（`rv32i_decoder.h` 末段）：

```
io().illegal_reason =
    select(known_unimplemented, 1,
    select(reserved,          3,
    select(known_opcode,      0, 2)));
```

⚠️ 原表把**整个** `OP_SYSTEM` 判为 `known_unimplemented` ⇒ ⇒ 故 `funct3 = 4`
也拿到 `1`（**不是** `3`）⇒ ⇒ 本项必须**拆开**：

```
auto is_op_system  = (op == OP_SYSTEM);
auto csr_legal_f3  = (f3 == 1) | (f3 == 2) | (f3 == 3) | (f3 == 5) | (f3 == 6) | (f3 == 7);
auto sys_known_unimpl = is_op_system & !csr_legal_f3 & (f3 != 4);   // funct3 == 0
auto sys_reserved     = is_op_system & (f3 == 4);

io().illegal_reason =
    select(sys_known_unimpl, 1,
    select(sys_reserved,     3,
    select(reserved,         3,
    select(known_opcode,     0, 2))));
```

⇒ ⇒ **优先级仍为 `1 > 3 > 2 > 0`** ⇒ ⇒ `OP_SYSTEM` 的判定插在**最前**（它原本就在最前）✔
⇒ ⇒ ⚠️ **`known_opcode` MUST NOT 加入 `OP_SYSTEM`**（否则 `funct3 = 0` 会掉到 `0` ⇒ ⇒
「合法但无功能」⇒ ⇒ 比「冻结」更糟 ⇒ ⇒ **AC8 的判别力就在这里**）。

⚠️ **`io().illegal_reason` MUST 仍是最后一个 `io()` 赋值**（C1 的同型要求）⇒ ⇒
本次**不新增任何 `io()` 赋值** ⇒ ⇒ 该约束自动满足 ✔

### 1.3 CSR 指令的**识别**（供下游用 · 零新增端口）

```
is_csr        = (opcode == OP_SYSTEM) & csr_legal_f3
csr_is_imm    = (funct3 == 5) | (funct3 == 6) | (funct3 == 7)   // CSRRWI/CSRRSI/CSRRCI
csr_is_write  = (funct3 == 1) | (funct3 == 2) | (funct3 == 3)   // CSRRW/CSRRS/CSRRC（会写 CSR）
csr_is_set    = (funct3 == 2) | (funct3 == 6)                     // CSRRS/CSRRSI（set 语义）
csr_is_clear  = (funct3 == 3) | (funct3 == 7)                     // CSRRC/CSRRCI（clear 语义）
```

⚠️ 这五个信号 MUST 在**同一处**推导（**单一来源**）⇒ ⇒ 否则「两份判据 = 静默分叉」
（`include/cpu/AGENTS.md` 的「非法指令判定唯一权威 = `Rv32iDecoder`」纪律的同型要求）。

---

## 2. 数据通路：读 → 写回

### 2.1 通路概览（**零新增端口**的落点）

```
ID 级（组合读 CsrBank）
  ├─ csr_read_value = CsrBank.csr_rdata      ← CsrBank 读口是组合（无寄存器）
  ├─ csr_addr       = instruction[31:20]     ← 12 位 CSR 地址
  ├─ csr_zimm       = instruction[19:15]     ← 5 位立即数（零扩展）
  └─ 判定信号（is_csr / csr_is_imm / csr_is_write / csr_is_set / csr_is_clear）
        ↓
IdExRegs（结构体字段 · 零端口代价）
        ↓
EX 级：算 csr_wdata（见 §3）→ ExMemRegs
        ↓
MEM 级：透传 → MemWbRegs（携带 csr_read_value + is_csr）
        ↓
WB 级：写回 mux 加「CSR 读值」一段（优先级最低 —— 与 load/alu/jump **互斥**）
```

### 2.2 为什么**零新增端口**是可能的

> ⚠️ **2026-11-25 · builder 实施期订正（下方 §8 有完整说明）**：本节的「载体」写错了 ——
> `rv32i_pipeline_regs.h` 的 `IdExRegs` / `ExMemRegs` / `MemWbRegs` **在流水线实现里零使用**
> （实测 grep：仅 `rv32i_branch_predict.h` 与 3 个测试 include 它 ⇒ ⇒ 实际实现是
> `rv32i_pipeline.h` 里的 `ch_reg`）⇒ ⇒ **零新增端口的结论仍成立，但载体是
> `rv32i_pipeline.h` 顶层的 `ch_reg`（`exmem_*` / `memwb_*`）** ⇒ ⇒ ⇒
> **且 `id_stage` / `ex_stage` / `mem_stage` 可零改动** ⇒ ⇒ **改动面 = 3 个文件**。

| 需求 | 常规做法 | 本设计的做法 | 代价 |
|---|---|---|---|
| CSR 信息从 ID 到 EX | `id_stage` 加输出端口 | **塞进 `IdExRegs` 结构体**（`rv32i_pipeline_regs.h`）⇒ ⇒ 顶层从结构体取 | **0 端口** |
| CSR 信息从 MEM 到 WB | `mem_stage` 加端口 | **塞进 `MemWbRegs`** | **0 端口** |
| `CsrBank` 的接入 | — | **在 `rv32i_pipeline.h` 顶层实例化**（顶层内部连接 ⇒ 不是端口） | **0 端口** |

⇒ ⇒ ⇒ **产物顶层端口表零变化**（C2：新端口若被消费则不冒泡；本设计**根本不新增端口**）
⇒ ⇒ ⇒ **53 端口契约表 + C6 快照 + 独立 Verilog TB 的端口表 + tsv + 期望值表 ⇒ ⇒ 全部不动** ✔

⚠️ **但 MUST 实测比对**（AC10）⇒ ⇒ 「推断不冒泡」是**推断**，不是事实 ⇒ ⇒
判据 = **产物顶层端口表逐字比对**（`scripts/verify_riscv_soc_core_ports.sh` V1/V2/V3 +
独立 TB 的端口映射逐字比对）⇒ ⇒ **实测红 ⇒ 即端口冒泡了** ⇒ ⇒ 届时 MUST 同批改 TB + tsv。

---

## 3. 写路径：`csr_wdata` 的逐条语义

### 3.1 源操作数（`rs1` 值 vs `zimm`）

| 指令 | 源操作数 | `csr_wdata` |
|---|---|---|
| `CSRRW` | `rs1` 的**寄存器值** | `rs1_value`（**原值**，不做 set/clear） |
| `CSRRS` | `rs1` 的**寄存器值** | `csr_old \| rs1_value` |
| `CSRRC` | `rs1` 的**寄存器值** | `csr_old & ~rs1_value` |
| `CSRRWI` | **`zimm`(5 位 · 零扩展)** | `zimm`（**原值**） |
| `CSRRSI` | **`zimm`** | `csr_old \| zimm` |
| `CSRRCI` | **`zimm`** | `csr_old & ~zimm` |

⚠️ **`CSRRC` 的掩码 MUST 是「按位取反后 AND」**（**不是** XOR）⇒ ⇒
`csr_old & ~rs1_value` ≠ `csr_old ^ rs1_value`（当 `rs1_value` 有 1 位以上为 0 时两者不同）
⇒ ⇒ ⇒ **AC4 的判别力就在这里**（变异：把 `& ~x` 改成 `^ x` ⇒ 转红）。

⚠️ **立即数型 MUST 用 `zimm`（5 位零扩展）** ⇒ ⇒ **不是** `rs1` 值 ⇒ ⇒
**AC5 的判据 MUST 用一条 `rs1 ≠ zimm` 的指令**（否则两种实现**恒绿**）。

### 3.2 `csr_old` 从哪来

`csr_old` = **`CsrBank.csr_rdata`**（**同一次组合读的旧值**）⇒ ⇒
⇒ ⇒ **写与读 MUST 用同一个 `csr_old`** ⇒ ⇒ 否则 `CSRRS` 会把「写后新值」OR 进去 ⇒ ⇒
⇒ ⇒ **判别力**：变异「写路径改用写后新值」⇒ ⇒ 转红。

### 3.3 **不写** CSR 的两种情形（AC6）

| 情形 | `csr_write_en` | 理由 |
|---|---|---|
| `CSRRW` 且 `rd = 0` | **仍为 1**（写发生） | RISC-V：`CSRRW` **总是**写（`rd = 0` 只是**不读**旧值） |
| `CSRRS` / `CSRRC` 且 `rs1 = 0` | **为 0**（不写） | RISC-V：`rs1 = 0` ⇒ 掩码全 0 ⇒ 写回同值 ⇒ **规范允许不写**；本项选**不写**（⇒ ⇒ 零副作用 ⇒ ⇒ 判据可判） |
| `CSRRWI`（`zimm` 恒非 0？⇒ 否，`zimm = 0` 合法） | **为 1** | `CSRRWI` 总是写（`rd = 0` 时不读） |
| `CSRRSI` / `CSRRCI` 且 `zimm = 0` | **为 0** | 同 `rs1 = 0` 的理由 |

⚠️ **「不读」与「不写」是两个独立维度** ⇒ ⇒ 判据 MUST 覆盖**两者都发生**的组合
（`CSRRW rd=0` ⇒ 不读但写）。

### 3.4 写使能的**发出级**（**沿级**语义）

`CsrBank` 的写口是**时钟沿**写（`ch_reg` 的 `->next`）⇒ ⇒ 写使能 MUST 在**某一级**拉高
⇒ ⇒ 本项选 **EX 级**：

- EX 级发 `csr_write_en` ⇒ ⇒ 沿上 `CsrBank` 写入 ⇒ ⇒ **下一拍**生效
- ⇒ ⇒ 与 §4 的 RAW 停顿**正好配套**（停顿 1 拍 ⇒ 后一条 CSR 指令读到的已是新值）

⚠️ **本项阶段 `CsrBank` 的写口只有 CSR 指令一个写源** ⇒ ⇒ **无仲裁冲突**
⇒ ⇒ ⇒ `ExceptionUnit`（P6-3）接入时 MUST 补仲裁规则（**母 improvement 的 R1 已登记**）。

---

## 4. RAW：CSR→CSR 的**停顿 1 拍**（C3 ⇒ 判据落顶层）

### 4.1 为什么「读旧值」**不可选**

RISC-V 规范要求 **CSR 访问之间保持顺序** ⇒ ⇒ 后一条 CSR 指令读到前一条写入**前**的旧值
**违反**该要求 ⇒ ⇒ ⇒ **不能**用「读旧值」图省事 ⇒ ⇒ 必须停顿或等价机制。

### 4.2 逐拍推导（**停 1 拍为什么恰好够**）

设：`C1` = `csrrw x1, mscratch, x2`（写）· `C2` = `csrrs x3, mscratch, x0`（读）· 相邻。

| 拍 | ID 级 | EX 级 | `CsrBank` 的 CSR 值 | 说明 |
|---|---|---|---|---|
| t0 | `C1` | — | `V0` | `C1` 进 ID |
| t1 | `C2` | `C1` | `V0` | `C1` 进 EX（EX 发写使能）⇒ ⇒ **C2 若此刻读 ⇒ 读到 `V0`（旧值）⇒ ⇒ 违规** |
| t2 | （`C2` 被**停顿**在 ID） | `C1`（**仍在 EX** ⇒ 写使能持续） | `V0` | 停顿拍 ⇒ `C1` 的写**尚未落在沿上** |
| t3 | `C2` | `C1`（终于前进） | **`V1`** | **t2 的沿上** `CsrBank` 写入 ⇒ ⇒ `C2` 此刻进 ID ⇒ ⇒ 组合读到 **`V1`（新值）** ✔ |

⇒ ⇒ ⇒ **停顿 1 拍恰好够**（不多不少）⇒ ⇒ 期望值 = **`V1`（新值）**（AC3）

⚠️ **停顿期间 `C1` 持续占用 EX** ⇒ ⇒ 它的写使能**持续拉高** ⇒ ⇒
`CsrBank` 在**每个沿**都写**同一个值** ⇒ ⇒ **幂等** ⇒ ⇒ 值不变 ✔（⇒ ⇒ **无副作用**）

### 4.3 判据的落点（C3 ⇒ 零新增 `HazardUnit` 端口）

```
// 在 rv32i_pipeline.h 顶层（不是 HazardUnit）：
auto csr_in_id   = <ID 级是 CSR 指令>;      // 来自 IfIdRegs / ID 级判定
auto csr_in_ex   = <EX 级是 CSR 指令>;      // 来自 IdExRegs
auto csr_raw     = csr_in_id & csr_in_ex;   // 前一条写 CSR + 当前这条读 CSR
auto stall_csr   = csr_raw & !flush;       // flush 优先（与既有 flush/stall 仲裁一致）
```

⇒ ⇒ **复用现成的 `if_stage.stall` / `id_stage.stall` 机制**（与 `illegal_active` 同一入口）
⇒ ⇒ ⇒ **零新增 `HazardUnit` 端口** ⇒ ⇒ ⇒ **红线不受影响** ✔

⚠️ **flush 优先**：既有 `if_stage.h` 的 PC 更新门是「`flush` 优先于冻结 stall」
（ADR-071 审稿修正）⇒ ⇒ 本项的 `stall_csr` MUST 同样**让位**给 `flush`
⇒ ⇒ ⇒ 否则「flush ∥ CSR 停顿同拍 ⇒ redirect 被丢掉」⇒ ⇒ **判别力 MUST 覆盖**。

---

## 5. 写回（WB 级）

现有 `wb_stage.h` 的 mux 优先级 = **load > alu > jump**（三段，`wb_data` 逐字对齐）。

⇒ ⇒ **CSR 读值段加在最后（最低优先级）** ⇒ ⇒ 理由：**CSR 与 load/alu/jump 互斥**
（一条指令只是一种）⇒ ⇒ ⇒ 优先级**任意** ⇒ ⇒ ⇒ 放最低 ⇒ ⇒ **零行为风险**。

⚠️ **`rd = 0` 的处理**：既有 `wb_stage` 已有 `rd_not_zero` ⇒ ⇒ CSR 段**自动**被它覆盖
⇒ ⇒ ⇒ **零新增逻辑** ✔

---

## 6. `CsrBank` 的接入（生产消费者 0 → 1）

| 端口 | 本项阶段的驱动 | 理由 |
|---|---|---|
| `csr_addr` | ID 级的 `csr_addr` | — |
| `csr_rdata` | ID 级组合读 | 读口是组合（`rv32i_csr.h` 末段 `io().csr_rdata = rdata;`） |
| `csr_wdata` / `csr_write_en` | EX 级 | §3.4 |
| **`mtip` / `meip`** | ⚠️ **显式 tie 0** | 本项阶段**无中断源**（P6-6 才有）⇒ ⇒ **未驱动端口读到未定义值** ⇒ ⇒ **MUST 显式 tie 0**（AC9） |

⚠️ `CsrBank` 的 `__io` **一字未动** ⇒ ⇒ ⇒ **零新增端口 ⇒ 23 处既有实例化点零改动** ✔
⇒ ⇒ ⇒ **既有组件级测试全部不受影响** ✔

---

## 7. 待定项与**已排除**的形态（不要重提）

| 形态 | 处置 |
|---|---|
| **读旧值**（不停顿） | ❌ **违反 RISC-V 的 CSR 顺序要求** ⇒ ⇒ 不可选 |
| **RAW 判据放进 `HazardUnit`** | ❌ 触碰 `AGENTS.md:256` 红线 ⇒ ⇒ **已排除**（用户 2026-11-25 确认） |
| **新增 decoder 端口**（如 `is_csr`） | ❌ C1 ⇒ ⇒ **端口位移** ⇒ ⇒ 产物端口表错位 ⇒ ⇒ **已排除** |
| **新增 stage 端口**（CSR 信息走端口而非结构体） | ❌ C2 ⇒ ⇒ 冒泡 ⇒ ⇒ 产物端口表变 ⇒ ⇒ **已排除**（改走结构体） |
| **在 ID 级写 CSR**（而非 EX） | ⚠️ 未选：ID 级写 ⇒ 写入更早 ⇒ ⇒ RAW 停顿**可以更短**（0 拍？）⇒ ⇒ 但 ID 级写会与「ID 级组合读同一拍」产生**同拍读写**（读到写前的旧值 ⇒ ⇒ 与 §4.2 的推导耦合）⇒ ⇒ ⇒ **选 EX 级**（推导更直白 + 与 trap 写口的仲裁留给 P6-3 统一处理） |
| **扩 TCM 容量** | ❌ 与本项无关 |
| **`misa` / `mip` 的可写性语义** | ❌ 母 improvement 的**第 6 项排除**（本版不 trap）⇒ ⇒ 保持「写入被忽略 · 读返回 0」 |


---

## 8. 实施期订正（**2026-11-25 · builder** · 追加 · **不改写上文历史叙述**）

### 8.1 载体订正（**§2.2 的载体写错，但结论不变**）

**实测**：`rv32i_pipeline_regs.h` 的 `IfIdRegs` / `IdExRegs` / `ExMemRegs` / `MemWbRegs`
**在流水线实现里零使用** ⇒ ⇒ grep 命中仅 3 处 include（`rv32i_branch_predict.h` +
3 个测试）⇒ ⇒ ⇒ **它们是「字段说明性质」的结构体，不是实现载体**。

**真实载体** = **`rv32i_pipeline.h` 顶层的 `ch_reg`**：
`exmem_*`（ID→EX）· `m2_*`（EX→MEM）· `memwb_*`（MEM→WB）⇒ ⇒ **顶层自己持有并更新**
⇒ ⇒ ⇒ **顶层可直接读写** ⇒ ⇒ ⇒ **零新增端口的结论成立** ⇒ ⇒ ⇒ 载体改为：
**`rv32i_pipeline.h` 的 `ch_reg` 字段**（新增 `exmem_is_csr` / `exmem_csr_addr` /
`exmem_csr_zimm` / `exmem_csr_kind` / `exmem_csr_read_value` / `memwb_is_csr` /
`memwb_csr_read_value`）。

### 8.2 **改动面从 6 个文件降到 3 个**

因顶层持有全部流水线寄存器 ⇒ ⇒ CSR 的**解碼 · 读值 · 写值 · 停顿判据****全部可在顶层完成**
⇒ ⇒ ⇒ **`id_stage.h` / `ex_stage.h` / `mem_stage.h` 零改动** ⇒ ⇒ 最终改动面：

| 文件 | 改动 | 端口面影响 |
|---|---|---|
| `include/cpu/pipeline/rv32i_decoder.h` | `illegal_reason` 判定细化（§1） | **零新增端口**（C1） |
| `include/cpu/pipeline/rv32i_pipeline.h` | `ch_reg` 字段 + `CsrBank` 实例 + CSR 逻辑 + 停顿判据 | **零新增端口**（顶层内部连接） |
| `include/cpu/pipeline/stages/wb_stage.h` | 写回 mux 加「CSR 读值」段 ⇒ ⇒ **+2 输入端口**（`csr_read_value` / `is_csr`） | ⚠️ **被顶层消费 ⇒ 不冒泡 ⇒ 产物端口表不变**（见 §8.3） |

### 8.3 C2 的**精确表述**（原文不够精确 ⇒ 订正）

原文「子组件的端口会冒泡到产物顶层」⇒ ⇒ **精确表述**是：
**只有「未被消费 / 未驱动」的子组件端口才冒泡** ⇒ ⇒ decoder 那 8 个端口冒泡是因为
**未驱动 / 未消费** ⇒ ⇒ ⇒ **本项新增的 2 个 WB 端口被顶层消费（连接）⇒ ⇒ 不冒泡**
⇒ ⇒ ⇒ **产物顶层端口表零变化** ⇒ ⇒ ⇒ **53 端口契约表 + C6 快照 + 独立 TB 端口表 + tsv + 期望表全部不动** ✔
⇒ ⇒ ⚠️ **但「推断不冒泡」是推断 ⇒ ⇒ AC10 MUST 实测比对**（tasks T5.1）。

### 8.4 T0 的**实测证据**（**先见红** · 零新代码 · 用现成 `tb_rv32i_soc --firmware`）

固件 = 手写 `itcm.mem`（每行 1 个 32 位字 · 小端 8 位十六进制）+ 空 `dtcm.mem`：

| # | 固件（首字） | 指令 | 实测输出 | 结论 |
|---|---|---|---|---|
| **T0.1** | `340020f3` | `csrrs x1, mscratch, x0` | `FIRMWARE-TRACE: retired=0 fetch0=0x340020f3 wb= stores=`<br>`RESULT: checks=5 fails=2`（**F3 / F4 红**） | ✅ **先见红**：取指**成功**（`fetch0` == 镜像首字 ⇒ 装载链路成立）但**退休 = 0** ⇒ ⇒ **冻结在 ID 级** |
| **T0.2** | `00500093` | `addi x1, x0, 5` | `FIRMWARE-TRACE: retired=15 fetch0=0x500093 wb=1:0x5`<br>`RESULT: checks=5 fails=0` | ✅ **负向对照**：**同一 TB · 同一装载链路 · 同一镜像格式** ⇒ ⇒ 唯一差别是**指令种类** ⇒ ⇒ **证明冻结是 CSR 指令特有的** |
| **T0.3** | `340040f3` | `funct3 = 4`（保留组合） | `FIRMWARE-TRACE: retired=0 fetch0=0x340040f3`<br>`RESULT: checks=5 fails=2` | ✅ **实测：保留组合当前也冻结** ⇒ ⇒ **整个 `OP_SYSTEM` 都是 `reason = 1`** ⇒ ⇒ ⇒ **「拿不到 `reason = 3`」已实测记录**（AC7 的先见红） |

⚠️ **T0.1 / T0.3 的观测限制（如实登记）**：`tb_rv32i_soc` 的固件模式**只观测 5 条装载链路判据**
（F1a / F1b / F2 / F3 / F4）⇒ ⇒ **它观测不到 `illegal_reason` 的具体值**
⇒ ⇒ 「冻结」是由 **retired = 0** **间接**得出的 ⇒ ⇒ ⇒ **`reason` 的具体值（1 vs 3）
MUST 由流水线级用例直接观测 `dbg_illegal_reason`**（⇒ ⇒ 归 T4 的 AC7 / AC8 判据）。

⚠️ **T0 用了「零新代码」路径**：探针 = **手写 `.mem` 镜像** + **现成的 `tb_rv32i_soc --firmware`**
⇒ ⇒ ⇒ **不新增测试文件** ⇒ ⇒ ⇒ **`CTEST_TOTAL` / `EXAMPLES_TEST_SOURCES` 本轮不动**
（⇒ ⇒ T4 的判据文件将新增 ⇒ ⇒ 那时**才**同步锚点 · 避免**分两次**改锚点）。


### 8.5 实施期订正（第二/三轮 · **层次化发射**与**前推**）

| # | 事实（实测） | 处置 |
|---|---|---|
| **①** | `id_stage.h` 的 `rs1_addr_mux` **不含** `OP_SYSTEM` ⇒ ⇒ CSR 的寄存器型源**恒读 x0**（= 0）⇒ ⇒ `CSRRS/CSRRC/CSRRW` 的 `csr_wdata` 全错 | 内部信号纳入 `OP_SYSTEM`（**`__io` 一字未动**）⇒ ⇒ 改动面 = **4 个文件**（§8.2 的 3 + id_stage 的**内部** mux） |
| **②** | **层次化发射失败**（4 条 ADR-060 §2.1 错误）：父层读**子组件的输入端口**（`id_stage.io().instruction` / `.valid` / `.freeze` / `.flush` / `ex_stage.io().rs1_data`）⇒ ⇒ 那些边「child side is not an output port」⇒ 无法映射到 module 端口 | 父层 MUST 读**父侧落地网 / 子组件输出**：`if_stage.io().instruction` / `if_stage.io().valid` / `hazard.io().flush` / 本层 `mem_stall` / `fwd_mux.io().alu_input_a` ⇒ ⇒ 与 ADR-066 D1 一致 |
| **③** | `csr_addr` 先写成 `bits<11,0>(instr)`（= `opcode/funct3/rd` 位段）⇒ ⇒ 读 0 / 写被 `CsrBank` 忽略 ⇒ ⇒ **判据全红**（自纠） | MUST 取 `instr[31:20]`（12 位 CSR 地址） |
| **④** | CSR 也写 `rd` ⇒ ⇒ 若 `hazard.ex_reg_write` / `mem_reg_write` 不含它，或三处写回 mux（`wb_stage` / `m2_write_data` / 档位 1）不同构 ⇒ ⇒ 距离 1/2/3 的消费者**读不到**写回值 | 四处**同批**补齐（见 `include/cpu/AGENTS.md` 的 CSR 通路段） |
| **⑤** | MEM 区是 **3 段**（EX/MEM → M2 → MEM/WB）⇒ ⇒ CSR 标记 / 读值 MUST **也过 M2** | `m2_is_csr` / `m2_csr_read_value`（§8.1 的清单只列了 exmem / memwb ⇒ 已补） |

**产物面实测（AC10 的证据）**：

| 产物 | 顶层端口 | module 数 |
|---|---|---|
| 扁平（`--emit-rtl`） | **53**（与改前**逐字相同**） | 1 |
| 层次化（`--emit-rtl --hierarchical`） | **53**（与扁平**同源**） | **8**（`CsrBank` 是 inline ⇒ **不新增层**） |
| SoC 侧核 module（`verify_riscv_soc_core_ports.sh`） | **V1/V2/V3 全绿**（53 / 51 + C6 快照） | — |
