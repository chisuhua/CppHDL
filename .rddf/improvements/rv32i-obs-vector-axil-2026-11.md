---
name: rv32i-obs-vector-axil-2026-11
priority: P1
phase: phase-6
category: testing
type: feature
状态: implemented
进度: "**立项（2026-11-26 · 用户「按建议顺序执行」⇒ 步 2）** ⇒ ⇒ 步 1（护栏）已归档 ⇒ 本项兑现其登记表的**归零动作** ⇒ ⇒ ✅ **已归档**（change `2026-11-27-rv32i-obs-vector-axil-2026-11` · `--skip-specs`）· 向量 39/39 · 登记表归零 · 16 用例 / 44494 断言全绿"
主题: dual-backend 覆盖守恒 —— 补齐 AXI-Lite 11 根输出进跨后端向量
依赖: change `2026-11-26-rv32i-dual-backend-obs-vector-guard-2026-11`（**步 1 护栏** · 已归档 · 其登记表登记了本项要补的 11 行）· change `rv32i-soc-mmio-channel-2026-11`（**S4a** 加的 AXI-Lite 通道）· `test_rv32i_core_mmio.cpp`（MMIO 程序与手推期望的**先例**）
来源: 用户 2026-11-26「按你建议的顺序执行」⇒ 步 2 = 盘点里的 P0-1
生成时间: 2026-11-26
evidence: probe:examples/riscv-mini/tests/test_rv32i_soc_dual_parity.cpp
---

# Improvement: 补齐 AXI-Lite 11 根输出进跨后端向量

**主题**: dual-backend 全量覆盖残余

> 具体对象 = `test_rv32i_soc_dual_parity` 的逐拍比对向量（28 → **39**）+ 8 根 `axil_*` 输入的显式驱动。
> （frontmatter 的 `主题:` 是完整表述；本行取**路线图主题**以过 `add-improve` 的 HARD-GATE 匹配。）

---

## 架构依据

| # | 权威 / 实测事实 | 对本项的约束 |
|---|---|---|
| ① | 核顶层 `ch_out` = **39**，向量 = **28** | 差 **11** 根 `axil_*` 输出（S4a 加通道时未同步） |
| ② | 该 fixture **不驱动** 8 根 `axil_*` 输入 | 未驱动 = 未定义值（根 `AGENTS.md` 铁律）⇒ MUST 显式驱动，且必须是**常量**（逐拍 diff 的前提） |
| ③ | 步 1 的护栏（C1–C4）已就位，登记表登记了 11 行 | 补齐后 MUST 删登记行（C3 强制清账） |
| ④ | MMIO 程序与手推期望有**先例**：`test_rv32i_core_mmio.cpp`（5 字程序 · 地址/数据/写回数全部手推 + 库表交叉校验） | 复用同一组常量与 `riscv::in_mmio` / `kGpio.base` 校验 |

---

## 范围

- **In Scope**：
  - `kObsNames[]` +11 根（`axil_*`）· `static_assert` 39 · `obs_snapshot()` +11
  - `AxilSlave` + `drive_axil()`（默认**立即应答** ⇒ 常量接线）· `load_firmware()` 内调用
  - 新用例 `Rv32iSoc - AxilMmioCrossBackend`（MMIO golden + 非 MMIO「请求线恒 0」）
  - 缺口登记表 **11 行归零** · 护栏第二判据改为**强形式**（空登记表合法）
  - `docs/riscv-mini/{test-architecture,dual-backend-gap-analysis}.md` 状态同步
- **Out of Scope**：核顶层端口变更（ADR-046 D10 冻结）· P0-2 / P1（步 3）

---

## Why

S4a 之后、「交付的 Verilog」里 AXI-Lite 通道的逐拍行为**没有任何第二后端证据**——
唯一覆盖它的两个判据（`test_axil_mmio_master` / `test_rv32i_core_mmio`）只在解释器跑。
而 AXI-Lite 恰好是「多拍握手 / VALID-READY 相位 / WSTRB 车道」这类 codegen 最易算错的形态。
步 1 只立了「不许静默漏」的护栏；本项**把漏掉的那一格真正补上**。

---

## What Changes

| # | 内容 |
|---|---|
| **S1** | 向量 28 → **39**（+11 根 `axil_*`）⇒ 所有既有用例顺带获得「无 MMIO 时请求线恒 0」的跨后端证据 |
| **S2** | 8 根 `axil_*` 输入**显式驱动**（`AxilSlave` 默认立即应答 · 全常量） |
| **S3** | 新用例：MMIO 事务的 AXI 语义 golden（`awvalid`/`arvalid` 各恰 1 拍 · 地址/数据/车道手推 · store 事件 1 次 · 写回 3 条）+ 非 MMIO 程序「请求线全程 0」 |
| **S4** | 登记表归零 + 护栏允许「空登记表」（缺口归零是**合法终态**，不是解析失败） |

---

## Acceptance

- [x] `verify_dual_backend_parity.sh` 主路径 PASS，第二判据输出 `向量 39 + 登记 0 == 核顶层 ch_out 39`
- [x] `--self-test`：既有 12 项 + **P0（空登记表 ⇒ 绿）** + 5/5 投毒必红
- [x] `test_rv32i_soc_dual_parity` 全绿：**16 用例 / 44494 断言**（原 15 / 23213）
- [x] 新用例的手推 golden 全部命中（地址 0x8010_2000 / 0x8010_2004 · 数据 0x5A · wstrb 0xF · 1 拍 VALID · 3 条写回）
- [x] 锚点零变化（`CTEST_TOTAL` / `VERILATOR_E2E_TOTAL` / `DUAL_BACKEND_PARITY_*` / `EXAMPLES_TEST_SOURCES`）
- [x] `verify_docs_links` / `verify_test_counts` / `verify_guard_reachability` / `verify_improvement_index` 全绿

---

## Capabilities

| capability | 变化 | 内容 |
|---|---|---|
| `verilator-rv32i-e2e` | **已由步 1 的契约覆盖**（本项是**兑现**，不是新契约） | 观测集合现在 == 端口面（39/39）⇒ 登记表归零 |

---

## Impact

| 面 | 影响 |
|---|---|
| 产品代码 | **零**（只改测试 + 护栏数据 + 文档） |
| 测试运行时 | `test_rv32i_soc_dual_parity` 断言数 23213 → 44494（+91%）；实测耗时仍 ≈ 3–4 min（断言是内存比较，不是 IO） |
| CI | 零新增条目 / 零新增 job |
| 风险 | 既有 15 个用例现在也比 39 根（多 11 根恒 0 的 `axil_*`）⇒ 若某用例**意外**触发 MMIO，会出现「两端一致但 golden 未覆盖」的新面 ⇒ 由 S3 的非 MMIO 用例兜底 |
