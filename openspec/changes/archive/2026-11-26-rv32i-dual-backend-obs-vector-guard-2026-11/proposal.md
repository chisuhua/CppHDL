# proposal · 跨后端观测向量**守恒护栏**（RV32I · 步 1）

> **Change**：`rv32i-dual-backend-obs-vector-guard-2026-11`
> **母 improvement**：`.rddf/improvements/rv32i-dual-backend-obs-vector-guard-2026-11.md`
> **触发**：用户 2026-11-26「审查 `docs/riscv-mini/test-architecture.md`」⇒ 双后端覆盖盘点
> ⇒ 采纳**步 1 = 护栏先行**
> **零产品代码**：只加**护栏 + 缺口登记表 + spec 契约**；缺口本体（11 根 `axil_*` 进向量）
> 归**步 2**（独立 change）。
> ⚠️ **本 change 不新增脚本** ⇒ `scripts/verify_dual_backend_parity.sh` 内加**第二判据**
> ⇒ ⇒ **guard-reachability 台账 1:1 与 `CTEST_TOTAL` / `VERILATOR_E2E_TOTAL` / `DUAL_BACKEND_PARITY_*`
> 锚点全部不动**。

---

## Why（为什么现在做）

S4a（change `rv32i-soc-mmio-channel-2026-11` · 2026-11-24）给核顶层加了 **AXI-Lite 通道**
（8 `ch_in` + 11 `ch_out`）⇒ 核顶层 `ch_out` 从 28 涨到 **39**。而**跨后端逐拍比对向量**
（`test_rv32i_soc_dual_parity.cpp` 的 `kObsNames[]`）仍是 **28**：

```
核顶层 ch_out                    39
kObsNames[]（跨后端比对向量）     28   ← static_assert(kObsCount == 28)
                                 ──
                                 11   根 axil_* 输出**从未被跨后端比对**
```

⇒ 而 `verify_dual_backend_parity.sh` 的**双锚点**只数 `DUAL_BACKEND_TEST_PAIR(` 的
**文件数**与**调用数** ⇒ ⇒ 对「向量 vs 端口面」这类**语义覆盖缩面**结构性失明
⇒ ⇒ **S4a 加 19 个端口时没有任何护栏转红**（实测：本 change 立项当天复算，差值仍是 11）。

⇒ ⇒ 本 change 不修那个洞（步 2 修），只做一件事：**让这个洞第二次出现时必须有人签字**。

---

## What Changes

### ✅ IN

| # | 内容 | 载体 |
|---|---|---|
| **S1** | **新逻辑载体** `scripts/lib/rv32i_obs_vector.py`：4 条判据（守恒 / 登记有效 / 不 stale / 归属）+ 5 类投毒自检 | 新文件 |
| **S2** | `scripts/verify_dual_backend_parity.sh` **挂第二判据**（主路径 + `--self-test` 串联），脚本头注释同步 | 改 1 文件 |
| **S3** | **缺口登记表** `.rddf/checks/rv32i-obs-vector-registry.md`：11 条 `axil_*` 输出 × {为什么未纳入, 归位条件} | 新文件 |
| **S4** | **spec 契约修正**：`verilator-rv32i-e2e` 的「全部 **27** 个输出观测值」（S4a 前的数字）⇒ 「核顶层**全部** `ch_out` 观测端口 + 缺口 MUST 登记」 | spec delta（MODIFIED） |
| **S5** | `docs/riscv-mini/test-architecture.md` §6 的 parity 行补新判据指针 | 改 1 文件 |

### ❌ OUT（本项**不做** ⇒ ⇒ 归属逐条）

| # | 不做的事 | 归属 |
|---|---|---|
| **O1** | 把 11 根 `axil_*` **加进向量** + 驱动 8 根 `axil_*` 输入 + 固件加 MMIO 事务 | **步 2**（`rv32i-obs-vector-axil-2026-11`） |
| **O2** | 其余 23 条解释器-only 条目的双后端化 | 盘点结论 = **P2/P3 不做**（仓根 `tests/` 已有等价双后端；见 `docs/riscv-mini/test-architecture.md` §1/§2） |
| **O3** | 核顶层端口面变更 | ADR-046 D10 **冻结** ⇒ 本项**零端口变更** |
| **O4** | 把判据推广到**所有** DUT 的向量 | 判据 C4 的端口面一侧已是通用的；其它 DUT 按需扩展（不在本项） |

---

## 验收标准（AC · 可机械判定）

> ⚠️ 判据面 MUST 落在**可观测输出**（脚本的 4 行结论 + 退出码 + 自检输出），不是「代码看起来对」。

| # | AC | 判据（命令 / 输出） |
|---|---|---|
| **AC1** | 主路径 PASS 且输出 **4 行判据结论** | `bash scripts/verify_dual_backend_parity.sh` ⇒ 末行 `PASS` + 4 行 `✅ C1..C4` |
| **AC2** | **5 类投毒逐条必红** | `--self-test` ⇒ `5/5` 且每类输出 `投毒 ⇒ 红` |
| **AC3** | 登记表与核顶层 `axil_*` 输出**逐字一致** | 登记 11 行；把任一行删掉 ⇒ **C1 红**；加一行编造名 ⇒ **C2 红** |
| **AC4** | 篡改实验（**手工**，三条） | ① 删 `kObsNames[]` 一项 ⇒ C1 红；② 删登记一行 ⇒ C1 红；③ 把某 `axil_*` 加进向量但**不删登记** ⇒ C3 红 |
| **AC5** | 锚点零变化 | `verify_test_counts.sh` exit 0；`DUAL_BACKEND_PARITY_*` 仍 67 / 297 |
| **AC6** | 可达性零变化 | `verify_guard_reachability.sh` PASS（**无新脚本** ⇒ 58 个被引用脚本不变） |
| **AC7** | 文档链接 | `verify_docs_links.sh` PASS |
| **AC8** | spec delta 合法 | `openspec validate rv32i-dual-backend-obs-vector-guard-2026-11 --strict` 无 ERROR；`verify_openspec_delta.sh` 绿 |

---

## Capabilities

| capability | 变化 | 内容 |
|---|---|---|
| `verilator-rv32i-e2e` | **MODIFIED** | 双路逐周期 parity 的**观测面** MUST == 核顶层 `ch_out` 端口面；未纳入者 MUST 登记（带「为什么」+「归位条件」）；补齐后 MUST 清账（登记残留在向量里 ⇒ 红） |
| （结构面 · 非 capability） | 护栏判据扩展 | `verify_dual_backend_parity.sh`：宏调用计数（2 锚点）**+** 观测向量守恒（4 判据） |

---

## 用户可见变更

- `bash scripts/verify_dual_backend_parity.sh` 的输出**变长**：多 4 行判据结论 + 新增失败态
  （`❌ C1 守恒失败：向量 27 ≠ 39 − 11`）。
- `--self-test` 从「10 项 fixture」变成「10 项 + 5 类投毒」⇒ 输出多 5 段。
- **零**测试条目增减、**零**产品行为变化。
