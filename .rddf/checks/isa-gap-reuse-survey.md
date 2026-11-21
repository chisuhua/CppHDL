---
evidence: probe:include/cpu/riscv/rv32i_csr.h
---

# P6-0 侦察：RV32I ISA 缺口的**复用面清单**（R7 · 2026-11-15）

> ⚠️ **本文件是只读侦察的产物**（`phase-6-next-steps.md` 的 **R7**）⇒ ⇒ 用途 = 给 **P6**
> （`CSR` / `ECALL` / `EBREAK` 的独立立项）提供**实测**依据 ⇒ ⇒ **不含裁决**
> （ISA 语义裁决归 architect · 见 **ADR-071**）。
>
> ⚠️ **`FENCE` 已出列**：2026-11-15 由 **ADR-071** 裁决为「已知 = NOP」并落地 ⇒ ⇒ 本清单
> **只覆盖仍未实现的 `CSR` / `ECALL` / `EBREAK` / `MRET` / 中断 / trap 入口**。
>
> 证据口径：行数 = `grep -c ''`；端口 = `__io` 声明；生产消费者 = `include/cpu/pipeline/**`
> + `examples/riscv-mini/src/**` 的符号命中数（**测试不算**）。

---

## 1. 可复用的既有组件（**已存在 · 有组件级测试**）

| 组件 | 文件 | 行数 | 已有能力 | 关键端口 |
|---|---|---|---|---|
| `riscv::CsrBank` | `include/cpu/riscv/rv32i_csr.h` | **252** | 9 个 CSR · **单写口** · 中断聚合 | 写：`csr_addr`/`csr_wdata`/`csr_write_en` · 读：`csr_rdata` · 中断入：`mtip`/`meip` · 中断出：`interrupt_req`/`timer_interrupt_pending`/`external_interrupt_pending` · 状态出：`mstatus_mie`/`mpie`/`mpp`/`mstatus_value`/`mtvec_value` |
| `riscv::ExceptionUnit` | `include/cpu/riscv/rv32i_exception.h` | **177** | trap 入口 + `MRET` · CSR 写请求生成 | 入：`current_pc`/`exception_cause`/`exception_valid`/`csr_mstatus`/`csr_mtvec`/`csr_mepc`/`csr_mie`/`csr_mcause` · 出：`csr_write_addr`/`csr_write_data`/`csr_write_en` · **`trap_entry_pc`** / **`trap_entry_valid`** / **`trap_flush`** |
| `ch::chlib::Clint` | `include/cpu/riscv/clint.h` | **213** | 64 位 `mtime`（自由运行 · ADR-049）· `mtimecmp` 比较 ⇒ 定时器中断 | — |
| `riscv` 地址映射 | `include/cpu/riscv/rv32i_mem_map.h` | **86** | 区域 base/size 的**单一真相**（L1） | — |
| MMIO 解码 | `include/cpu/riscv/address_decoder.h` | **128** | 6 区域 MMIO 解码 | — |

### 1.1 ⚠️ 关键实测：**trap 的三个承重输出已经存在**

`ExceptionUnit` **已经**给出 `trap_entry_pc` / `trap_entry_valid` / `trap_flush`
⇒ ⇒ **缺的不是「trap 逻辑」，而是「把它接进流水线」**。

### 1.2 ⚠️ 关键实测：**生产消费者 = 0**

```
grep -rn "CsrBank|ExceptionUnit|rv32i_csr|rv32i_exception" include/cpu/pipeline/ examples/riscv-mini/src/ | wc -l
⇒ 0
```

⇒ ⇒ 特权侧组件**只在组件级测试里被用过**（`examples/riscv-mini/tests/test_csr_unit.cpp` ·
`test_interrupt_flow.cpp`）⇒ ⇒ 「接入」是**纯新增接线**，风险集中在**端口驱动义务**
（仓内铁律：新增组件输入端口 ⇒ **全部实例化点 MUST 显式驱动**）。

---

## 2. 必须**新增**的面（按接入顺序）

| # | 面 | 现状 | 为什么 MUST 新增 |
|---|---|---|---|
| N1 | **`mcause` 可写**（G5） | `ExceptionUnit` 的 `write_mepc = write_mcause = do_trap_entry` ⇒ ⇒ `csr_addr` 的 select 链**恒命中 `MEPC`** ⇒ ⇒ `mcause` 分支**不可达** | `CsrBank` **单写口** ⇒ 一拍只能写一个 CSR ⇒ ⇒ trap 需要**两拍**（或第二写口）⇒ ⇒ 这是**结构性**的，不是优先级写错 |
| N2 | **CSR 指令译码** | `OP_SYSTEM`（`0x73`）**零消费者**；本版归「已知未实现」= `illegal_reason 1` | `funct3` 的 6 种（`1/2/3/5/6/7` ⇒ `CSRRW/S/C` + 立即数变体）**完全没有**译码与执行通路 |
| N3 | **第三路 PC 源** | `if_stage` 只有**两路**（`branch_valid`/`branch_target`）；ADR-048 D? 明文说「引入第三路 MUST 重新论证不变量」 | trap 入口要跳 `mtvec` ⇒ ⇒ 必须有第三路（且与 `flush` 的相互作用要重新论证） |
| N4 | **`exception_cause` 的数据源** | **无**（`illegal_reason` 是 2 bit 的分类，不是 `mcause` 码） | 要把「未知 opcode / 已知未实现」映射到 RISC-V 的 `mcause`（`2` = illegal instruction） |
| N5 | **中断接线** | CLINT 未接 `CsrBank.mtip`；`mie`/`mip`/`mstatus` 语义未定 | 中断 = 另一条独立立项（**不改** trap 入口时序） |
| N6 | **`mtval` 数据源** | **无**（`ExceptionUnit` 只有 `exception_cause`） | 本版定为「恒 0」（ADR 已在 G5 段裁决）；真值 = 特权级后续 |

---

## 3. 与 **ADR-071（冻结）** 的关系：升级路径是**同一检测点**

`illegal_reason`（ID 级 · 含 `valid` 门控）就是**trap 的触发点** ⇒ ⇒ 升级 = 把
「`stall` 冻结」换成「`exception_valid = 1` ⇒ `ExceptionUnit` 驱动 `trap_entry_pc`」：

| 现在（ADR-071） | 升级后（特权级） |
|---|---|
| `illegal_active` ⇒ IF/ID 同时 `stall` | `exception_valid` ⇒ `trap_entry_valid` ⇒ **第三路 PC 源** + `trap_flush` |
| 该指令**永不退休**（冻结） | 该指令**不退休**，但 `mepc` 记下它的 PC（可恢复） |
| `dbg_illegal_reason`（2 bit） | 同信号 ⇒ 映射为 `mcause = 2`（illegal instruction） |

⇒ ⇒ **ADR-071 的冻结不是一次性设计**：它把「检测」与「处置」分开 ⇒ ⇒ 处置可从
「冻结」**原位**替换为「trap」，检测面（`illegal_reason` + `valid` 门控）**零改动**。

---

## 4. 建议的接入顺序（**给 P6 立项用** · 非裁决）

| 步 | 内容 | 依赖 | 判据（前瞻） |
|---|---|---|---|
| **P6-1** | **G5**：`mcause` 可写（首选：`ExceptionUnit` 内部**跨两拍** · `CsrBank` 零改动） | 无（组件级） | **复合 DUT**（`ExceptionUnit` → `CsrBank`）⇒ 写入后**可读回**（现有 `test_csr_unit.cpp` 的 `mcause` 断言是**直接对 `CsrBank` 写**，抓不到该缺陷） |
| **P6-2** | **CSR 指令**（译码 + 读写 + `mstatus` 副作用） | P6-1 | 6 种 `funct3` 逐条用例（期望值手推）· `illegal_reason` 不再对它们报 `1` |
| **P6-3** | **trap 入口**（第三路 PC 源 + `trap_flush` + `mepc` 保现场） | P6-1 · P6-2 | ADR-048 的 flush 不变量**重新论证** + 逐拍等价（层次化/扁平） |
| **P6-4** | `ECALL` / `EBREAK`（`mcause` 11 / 3） | P6-3 | trap 后 `mepc` 指向该指令（`ECALL` 不前进 / `EBREAK` 同） |
| **P6-5** | `MRET` | P6-3 | 从 `mepc` 返回 + `mstatus.mie` 恢复（`mpie → mie`） |
| **P6-6** | **中断**（CLINT ⇒ `mtip` · `mie`/`mip`/`mstatus`） | P6-5 | 定时器中断可抢占 + `mepc`/`mcause` = 中断码 · **与 trap 入口同一通路** ⇒ 时序不得分叉 |
| **P6-7** | `mtval` 真值（已知限制收敛） | P6-3 | 需 `exception_cause` 之外的数据源（新增输入端口 ⇒ **全部实例化点显式驱动**义务） |

---

## 5. 风险与已知边界

| # | 风险 | 依据 |
|---|---|---|
| R1 | **`CsrBank` 单写口** ⇒ trap 同拍要写两个 CSR ⇒ 只能跨拍（改变 trap 时序）或加第二写口（**公共 API 变化** ⇒ ~20 处实例化的驱动义务） | `rv32i_csr.h:41-43` |
| R2 | **第三路 PC 源**会打破 ADR-048 的「flush 唯一来源」不变量 ⇒ 必须重新论证（含与 `flush_d1` 多杀一拍的交互） | ADR-048 · `if_stage.h` 的注释 |
| R3 | **ADR-071 的冻结要与 trap 共存**：真 trap 落地后，「未知 opcode」的处置 MUST 从「冻结」改为「trap」⇒ ⇒ 两套机制**不得同时生效**（否则 trap 前先冻住） | ADR-071 D3 / D3-b |
| R4 | `mepc` 的**写入时机**与流水线 flush 的交互（`trap_flush` vs `hazard.flush`） | `rv32i_exception.h:63-72` |
| R5 | 中断与异常**共用** trap 入口 ⇒ ⇒ 优先级（中断 vs 异常）必须显式 | 未定 |

---

## 6. 结论（一句话）

**「trap 逻辑」基本已在库里，「接入」才是工作本体**：`ExceptionUnit` 的
`trap_entry_pc`/`trap_entry_valid`/`trap_flush` 三根输出 + `CsrBank` 的 9 个 CSR 都可复用
⇒ ⇒ 真正的新增面 = **`mcause` 可写（G5）· CSR 译码 · 第三路 PC 源 · `illegal_reason → mcause`
映射 · 中断接线** ⇒ ⇒ 且 **ADR-071 的检测点是可原位升级的钩子**（冻结 → trap 无需改检测面）。
