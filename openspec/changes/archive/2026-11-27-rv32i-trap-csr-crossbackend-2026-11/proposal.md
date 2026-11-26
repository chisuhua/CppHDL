# proposal · trap / CSR 的**核级跨后端** golden（步 3 · P0-2）

> **Change**：`rv32i-trap-csr-crossbackend-2026-11`
> **母 improvement**：`.rddf/improvements/rv32i-trap-csr-crossbackend-2026-11.md`
> **前置**：步 1（观测向量守恒护栏，已归档）· 步 2（向量 39/39 + `run_both` fixture，已归档）
> **零产品代码** · **零端口变更** · **零新增 CTest 条目**（新用例在同一 binary 内）

---

## Why

P6-3 / P6-4 把 **trap 入口**（`mepc`/`mcause` 写回 + PC 重定向到 `mtvec`）与 **6 种 `funct3` 的
CSR 指令**做进了 RTL，但它们的判据（`test_rv32i_trap_entry` 11 用例 / 1196 断言 ·
`test_rv32i_csr_instructions` 8 / 31）**只在解释器跑**。

⇒ ⇒ **交付的 Verilog 里，这两族语义没有任何第二后端证据**。而它们恰好落在 codegen 最易
写错的格子上：cause 编码 mux / `mepc` 单写口仲裁 / PC 重定向三路 mux / `CSRRC` 的
「按位取反后 AND」。

⇒ ⇒ 本项把这两族判据搬进**已有**的双后端逐拍 harness（39 根输出 · 无掩码），
期望值**沿用已手推的那一组常量**（禁抄仿真输出）。

---

## What Changes

### ✅ IN

| # | 内容 | 载体 |
|---|---|---|
| **S1** | 共享辅助 `run_both()` / `require_per_cycle_equal()` / `collect_writebacks()`（复用步 2 的 `load_firmware` span 版 + `AxilSlave`） | `test_rv32i_soc_dual_parity.cpp` |
| **S2** | `Rv32iSoc - TrapEntryCrossBackend`：未知 opcode ⇒ `mepc` = 0x8000_0000 · `mcause` = 2 ⇒ 写回**恰好 2 条**；触发指令**不退休**（触发拍 retired == 0）+ 其后**连续两拍** ID 空 | 同上 |
| **S3** | `Rv32iSoc - CsrInstrsCrossBackend`：6 种 `funct3` ⇒ **9 条**写回 golden（`CSRRC` 的判别位 = `old & ~x` 而非 `old ^ x`）+ 合法 CSR 全程 `reason == 0` | 同上 |

### ❌ OUT

| # | 不做 | 归属 |
|---|---|---|
| O1 | P1：`test_rv32i_stall_integrity` 的跨后端化 | 盘点 §5（下一项） |
| O2 | 中断 / `MRET` 恢复的跨后端 | 中断路径在核顶层**尚不存在**（`irq` 归 S5） |
| O3 | 核顶层端口变更 | ADR-046 D10 |

---

## 验收标准（AC）

| # | AC | 判据 |
|---|---|---|
| **AC1** | 全量用例绿 | `test_rv32i_soc_dual_parity` ⇒ **18 用例 / 60311 断言** |
| **AC2** | 逐拍 diff 覆盖全端口面 | 两个新用例的 `require_per_cycle_equal` 遍历 `kObsCount == 39` |
| **AC3** | trap golden | 写回 == {(1, 0x8000_0000), (2, 2)} · 触发拍 `retired == 0` · `trigger+1`/`trigger+2` 的 `id_valid == 0` |
| **AC4** | CSR golden | 9 条写回 == 手推 · 合法 CSR 全程 `reason == 0` · 编码手算常量交叉校验（`0x34009173` / `0x3407F3F3`） |
| **AC5** | 锚点零变化 | `verify_test_counts` 绿 |
| **AC6** | 护栏 | `verify_dual_backend_parity` / `verify_docs_links` / `verify_guard_reachability` / `verify_improvement_index` 全绿 |

---

## Capabilities

无新 capability（步 1 的 `verilator-rv32i-e2e` 契约已覆盖「观测集合 == 端口面」；
本项是**兑现判别力**）⇒ 归档 `--skip-specs`。

---

## 用户可见变更

- `test_rv32i_soc_dual_parity` 断言 44494 → **60311**（+2 用例 × 200 拍 × 39 根 + golden）。
- CTest 条目数 **不变**（同一 binary）。
