---
name: rv32i-trap-csr-crossbackend-2026-11
priority: P1
phase: phase-6
category: testing
type: feature
状态: implemented
进度: "**立项（2026-11-27 · 用户「按建议的顺序执行」⇒ 步 3 的 P0-2）** ⇒ ⇒ P1（反压期操作数完整性）**未纳入本项** ⇒ 仍挂 `test_rv32i_stall_integrity` ⇒ ⇒ ✅ **已归档**（change `2026-11-27-rv32i-trap-csr-crossbackend-2026-11` · `--skip-specs`）· 18 用例 / 60311 断言全绿 · 2 条变异必红"
主题: dual-backend 覆盖 —— trap / CSR 的核级跨后端 golden
依赖: change `2026-11-26-rv32i-dual-backend-obs-vector-guard-2026-11`（步 1 · 已归档 · 观测向量守恒护栏）· change `2026-11-27-rv32i-obs-vector-axil-2026-11`（步 2 · 已归档 · 向量 39/39 + `run_both` 前的 fixture 范式）· `test_rv32i_trap_entry.cpp` / `test_rv32i_csr_instructions.cpp`（手推常量的**先例**）
来源: 用户 2026-11-26「按你建议的顺序执行」⇒ 步 3 = 盘点 P0-2
生成时间: 2026-11-27
evidence: probe:examples/riscv-mini/tests/test_rv32i_trap_entry.cpp
---

# Improvement: trap / CSR 的核级跨后端 golden

**主题**: dual-backend 全量覆盖残余

> 具体对象 = 把**核级语义**（trap 入口的 `mepc`/`mcause` 写回 + PC 重定向；6 种 `funct3` 的
> `csr_wdata`）从「只有解释器证据」提升到「**双后端 + 手推 golden**」。
> （frontmatter 的 `主题:` 是完整表述；本行取**路线图主题**以过 `add-improve` 的 HARD-GATE 匹配。）

---

## 架构依据

| # | 权威 / 实测事实 | 对本项的约束 |
|---|---|---|
| ① | `test_rv32i_trap_entry`（11 用例 / 1196 断言）与 `test_rv32i_csr_instructions`（8 / 31）**只在解释器跑** | 交付的 Verilog 里 trap / CSR 语义**无第二后端证据** |
| ② | 双后端 parity 是**同构盲区**（解释器与 codegen 同源） | ⇒ 本项的价值 = 手推 golden + codegen 侧缺陷；**不**宣称「parity 即正确」 |
| ③ | 两个判据的期望值**已手推**（规范 + 复位值），且已在解释器侧验证 | ⇒ 双后端化 = **复用同一组常量**（禁抄仿真输出） |
| ④ | spec 要求逐拍 diff MUST 用「同一 TEST_CASE 内 2×device + 2×Simulator」（**不能**用 `DUAL_BACKEND_TEST_PAIR`） | ⇒ 载体 = `test_rv32i_soc_dual_parity.cpp` 的既有 harness（步 2 已抽出 `run_both`） |

---

## 范围

- **In Scope**：`Rv32iSoc - TrapEntryCrossBackend` + `Rv32iSoc - CsrInstrsCrossBackend`
  （逐拍 diff 39 根 + 写回序列 golden + trap 的「不退休 / 两拍空 ID」）
- **Out of Scope**：P1（`test_rv32i_stall_integrity` 的跨后端化）· `ECALL`/`EBREAK` 之外的
  特权面（中断 / `MRET` 恢复）· 核顶层端口变更

---

## Why

P6-3 / P6-4 把 trap 入口与 `ECALL`/`EBREAK`/`WFI` 的语义做进了 RTL，但这些语义的判据
**只在解释器跑** ⇒ ⇒ 一旦 codegen 侧（cause 编码 mux / `mepc` 写口仲裁 / PC 重定向三路）
算错，「parity 全绿」也发现不了（那份文件里根本没有第二个后端）。

---

## What Changes

| # | 内容 |
|---|---|
| **S1** | `run_both()` / `require_per_cycle_equal()` / `collect_writebacks()` 抽成共享辅助（复用步 2 的 fixture） |
| **S2** | `Rv32iSoc - TrapEntryCrossBackend`：未知 opcode ⇒ `mepc` = 0x8000_0000 · `mcause` = 2 ⇒ 写回序列恰好 2 条；触发指令**不退休** + 其后连续两拍 ID 空 |
| **S3** | `Rv32iSoc - CsrInstrsCrossBackend`：6 种 `funct3` ⇒ 9 条写回 golden（含 `CSRRC` 的「按位取反后 AND」判别位）+ 合法 CSR 全程 `reason == 0` |

---

## Acceptance

- [x] `test_rv32i_soc_dual_parity` 全绿：**18 用例 / 60311 断言**（16 / 44494）
- [x] 两个新用例的逐拍 diff 覆盖 **39 / 39** 根输出（无掩码）
- [x] trap：写回序列 == 手推 2 条 · `mepc` == 0x8000_0000 · `mcause` == 2 · 触发拍 retired == 0 · 其后两拍 ID 空
- [x] CSR：9 条写回 == 手推 · 合法 CSR 全程 `reason == 0` · 编码手算常量交叉校验
- [x] 锚点零变化（`CTEST_TOTAL` / `VERILATOR_E2E_TOTAL` / `DUAL_BACKEND_PARITY_*` / `EXAMPLES_TEST_SOURCES`）
- [x] 护栏全绿（`verify_dual_backend_parity` / `verify_test_counts` / `verify_docs_links` / `verify_guard_reachability`）

---

## Capabilities

无新 capability（步 1 的契约已覆盖；本项是**兑现判别力**）⇒ 归档 `--skip-specs`。

---

## Impact

| 面 | 影响 |
|---|---|
| 产品代码 | 零 |
| 运行时 | `test_rv32i_soc_dual_parity` 断言 44494 → **60311**（+2 用例 × 200 拍 × 39 根 + golden）；耗时仍 ≈ 4–5 min |
| CI | 零新增条目 / 零新增 job |
| 残余 | **P1（反压期操作数完整性）未做** ⇒ 已登记在盘点 §5 / §9 |
