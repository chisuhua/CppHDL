# Proposal: RV32I 产品级层次化语料的顶层端口表 == 手推期望集（R8′）

- **状态**: **立项（2026-11-21 · builder · 用户 2026-11-21 裁决选项 B 后开工）** · 待归档
- **性质**: **判据补齐**（台账 id=25 的下游实施项）
- **依据**: `docs/architecture/2026-11-02-hierarchical-rtl-output.md` §7 行 25（id=25）
  + `.rddf/checks/decision-request-hierarchical-rv32i-corpus-r2-r8-2026-11.md`（决策请求单）

---

## Why

RV32I 层次化产物（`examples/riscv-mini` 的 `emit_rtl_rv32i_soc_hier`）现有**两条**顶层端口面判据：

1. 「层次化顶层端口行 == **扁平**顶层端口行的前缀」（逐字逐序）；
2. 「层次化顶层端口行 == `manifest.port_table[]`」（逐字逐序）。

⇒ ⇒ **两条都与产物同源**（扁平与 manifest 都来自**同一次发射**）⇒ ⇒ 「发射底座算错」时
**两侧一起错** ⇒ ⇒ **抓不到** ⇒ ⇒ 这正是仓内既有纪律（AGENTS.md：「**两后端一致不能当作
正确性证据**」）在顶层端口面上的同型缺口。

⇒ ⇒ 唯一**独立于产物**的端口面判据 = **一份手推的顶层端口期望集**（34 根）⇒ ⇒ 本 change
把它落成护栏判据。

⚠️ **为什么是「独立的一条（R8′）」而不是 R8 的延伸**：R8 守的是**专用 DUT 语料**
（`tests/hier_rtl_products` 的 3 个 ⇒ 发射器在 `tests/`），而 RV32I SoC 是**产品级语料**
（发射器在 `examples/riscv-mini/`）⇒ ⇒ **发射器不同** ⇒ ⇒ 独立块 + 独立前置
（⇒ `tests/**` MUST NOT 引用 `examples/**`（`example-file-boundary` 裁决）⇒ ⇒ **不可能**
把 RV32I 语料塞进 `hier_rtl_products`）。

## What Changes

### ① 护栏加 **R8′** 块（`scripts/verify_hierarchical_rtl.sh`）

- 新前置（fail-closed）：`build/examples/riscv-mini/emit_rtl_rv32i_soc_hier` 不可用 ⇒ **红**；
- 新判据：发射 RV32I 层次化产物 ⇒ 用 `verilator --json-only` + `scripts/lib/verilog_tree_extract.py --ports`
  结构化提取 `top` 的端口表 ⇒ ⇒ 与**手推期望集**（34 根 = 框架 2 + 根自有 32）**逐项**比对
  （名字 + 方向 + 宽度 + 顺序）；
- 期望集**推导源**（手推 ⇒ 禁抄产物输出）：`examples/riscv-mini/src/rv32i_pipeline_top.h`
  的 `__io`（4 in + 28 out ⇒ 声明顺序即端口顺序）⇒ 端口名 = `top_<C++ 成员名>`（ADR-075 E1）。

### ② 判别力自证（`--self-test` 新增投毒）

投毒**产品端口宽度**（`top_dbg_id_pc` 32 → 16）⇒ ⇒ R8′ 必红 ⇒ ⇒ 证明「期望集真的在比」。

### ③ 期望源登记

`.rddf/checks/examples-rtl-expectations.md` 的 RV32I 段补一句指向（R8′ 的期望集在护栏内，
推导源是根头的 `__io`）。

## 判据

| 判据 | 命令 |
|---|---|
| 护栏主路径（含 R8′） | `bash scripts/verify_hierarchical_rtl.sh` |
| 判别力自检（含新投毒） | `bash scripts/verify_hierarchical_rtl.sh --self-test` |
| RV32I 产物面不回归 | `bash scripts/verify_examples_rtl_sim.sh` |
| 残留两侧同批（id=25 → 已收口） | `bash scripts/verify_arch_ledger_residuals.sh` |

## Out of Scope

- **不**接入 R6 的 4 条树结构不变量（实测**今天恒绿** ⇒ 价值 = 未来回归防护 ⇒ 按 id=25 的裁决
  **不单独立项**，已在台账后缀写明）。
- **不**改 RV32I 的顶层端口面本身（34 根 = 既有形态）⇒ ⇒ 本 change **零行为改动**。
- **不**动扁平产物的端口面（那是 id=23 的范围）。
