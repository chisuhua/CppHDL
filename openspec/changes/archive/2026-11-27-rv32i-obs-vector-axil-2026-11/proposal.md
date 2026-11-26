# proposal · 补齐 AXI-Lite 11 根输出进跨后端向量（步 2）

> **Change**：`rv32i-obs-vector-axil-2026-11`
> **母 improvement**：`.rddf/improvements/rv32i-obs-vector-axil-2026-11.md`
> **前置**：步 1 = change `2026-11-26-rv32i-dual-backend-obs-vector-guard-2026-11`（**已归档**）
> ⇒ 它把缺口登记成 11 行 + 立了 C1–C4 护栏；本项兑现**归零**。
> **零产品代码** · **零端口变更**（ADR-046 D10）· **零新增测试条目**

---

## Why

S4a（change `rv32i-soc-mmio-channel-2026-11`）给核顶层加了 AXI-Lite 通道（8 `ch_in` + 11 `ch_out`），
但跨后端逐拍比对向量停在上一个时代：**28 / 39**。⇒ ⇒ 交付的 Verilog 里，AXI-Lite 通道
（多拍握手 / VALID-READY 相位 / WSTRB 车道 —— codegen 最易算错的形态）**没有任何第二后端证据**，
而唯一覆盖它的两个判据（`test_axil_mmio_master` / `test_rv32i_core_mmio`）**只在解释器跑**。

步 1 解决了「静默漏」；本项解决「漏着的那一格」。

---

## What Changes

### ✅ IN

| # | 内容 | 载体 |
|---|---|---|
| **S1** | `kObsNames[]` +11 根 `axil_*`（28 → **39**）· `static_assert(kObsCount == 39)` · `obs_snapshot()` +11 读 | `test_rv32i_soc_dual_parity.cpp` |
| **S2** | `AxilSlave` + `drive_axil()`：8 根 `axil_*` 输入**显式驱动**，默认**立即应答**（全常量 ⇒ 逐拍输入相同 ⇒ 逐拍 diff 有效）· `load_firmware()` 内统一调用 | 同上 |
| **S3** | 新用例 `Rv32iSoc - AxilMmioCrossBackend`：MMIO 事务 golden（B–F 六条）+ 非 MMIO 程序「请求线全程 0」（G） | 同上 |
| **S4** | 缺口登记表 **11 行归零**；护栏 analyzer 允许**空登记表**（缺口归零是合法终态）+ 自检新增 **P0**（空表 + 全覆盖 ⇒ 绿） | `.rddf/checks/rv32i-obs-vector-registry.md` · `scripts/lib/rv32i_obs_vector.py` |
| **S5** | 文档状态同步（§2.1 规模 + 缺口说明 → 已收口；盘点 §0/§2/§3/§8/§9） | 2 份文档 |

### ❌ OUT

| # | 不做 | 归属 |
|---|---|---|
| **O1** | 核顶层端口变更 | ADR-046 D10 冻结 |
| **O2** | trap/CSR 的跨后端 golden | **步 3**（P0-2） |
| **O3** | 从设备侧**反应式**响应（`ready_lat` / 逐拍变化 `rdata`） | `test_rv32i_core_mmio` 已覆盖该判别面；本项用常量接线以保证逐拍 diff 的前提 |

---

## 验收标准（AC）

| # | AC | 判据 |
|---|---|---|
| **AC1** | 向量 == 端口面 | `verify_dual_backend_parity.sh` ⇒ `C1 守恒：向量 39 + 登记 0 == 核顶层 ch_out 39` |
| **AC2** | 全量用例绿 | `test_rv32i_soc_dual_parity` ⇒ `16 test cases / 44494 assertions` 全过 |
| **AC3** | AXI 语义 golden | 新用例的 B–F 六条手推断言全过（`awvalid`/`arvalid` 各 1 拍 · 地址/数据/车道 · store 1 次 · 写回 3 条） |
| **AC4** | 非 MMIO 无请求 | 新用例 G：plain 程序全程 `awvalid`/`wvalid`/`arvalid` == 0 |
| **AC5** | 护栏自检 | `--self-test` ⇒ 12 项 + **P0** + 5/5 投毒 |
| **AC6** | 锚点零变化 | `verify_test_counts` 绿（`CTEST_TOTAL` 284 · `VERILATOR_E2E_TOTAL` 81 · `DUAL_BACKEND_PARITY_*` 67/297 · `EXAMPLES_TEST_SOURCES` 26） |
| **AC7** | 其它护栏 | `verify_docs_links` / `verify_guard_reachability` / `verify_improvement_index` 全绿 |

---

## Capabilities

无新 capability（步 1 的契约已覆盖本项）⇒ 归档 `--skip-specs`。

---

## 用户可见变更

- `test_rv32i_soc_dual_parity` 的断言数 23213 → **44494**（+11 根 × 每拍）。
- 该二进制多一个用例（`Rv32iSoc - AxilMmioCrossBackend`）⇒ **CTest 条目数不变**（同一 binary）。
- 护栏输出：第二判据从「28 + 11 == 39」变成「**39 + 0 == 39**」。
