# 提案：订正「`multi_if` 的接受面 MUST 完备」Requirement 的**标题**

- **来源**：change `multi-if-acceptance-surface-2026-10` 归档后复核
  （`openspec validate --specs --strict` + 逐条比对主 spec）发现
- **规格影响**：capability `chlib-conditional-semantics` ⇒ **RENAMED ×1**
  ⇒ 归档**禁** `--skip-specs`
- **规模 / 风险**：**极小**（**零**行为变更 · **零**代码改动 · 纯标题订正）

## Why

该 Requirement 的**正文**是对的：

> `multi_if<T>` 的接受面里**每一格** MUST 要么**语义正确**、要么**响亮拒绝**
> （编译期 `static_assert` 或建图期 `CHFATAL_EXCEPTION`）⇒ MUST NOT 存在第三态

⇒ ⇒ 但**标题**写的是「（要么语义正确、要么**编译期**拒绝）」⇒ ⇒ **不准确**：

- **形态 B**（单分支）⇒ 拒绝发生在**建图期**（`operator T()` 抛
  `CHFATAL_EXCEPTION`）⇒ **它编译通过** ⇒ ⇒ 标题会让人以为它是编译期拒绝；
- **形态 D** 的**整数字面量值域检查**（`elif(c, 256)` 进 `T=8`）⇒ 同样是
  **建图期**拒绝（字面量的**值**是运行期信息 ⇒ 编译期**够不到**）。

⇒ ⇒ 标题与正文**自相矛盾** ⇒ ⇒ 读者（尤其修复者）会去找错的地方。

## What Changes

- **仅**该 Requirement 的**标题**：「要么语义正确、要么**编译期**拒绝」
  ⇒ 「要么语义正确、要么**响亮拒绝**」；
- ⇒ ⇒ 用 OpenSpec 的 **`RENAMED`** delta（`FROM:` / `TO:`）——
  ⚠️ **实测**：用 `MODIFIED` 改标题会被 CLI **静默不合并**
  （仓内护栏 `verify_openspec_delta.sh` 的 **R3** 正好抓到了这一点：
  「MODIFIED 标题与主 spec 不逐字一致 ⇒ CLI 会静默不合并」）⇒ ⇒ 而
  `RENAMED` **不动正文** ⇒ ⇒ 正文与**全部 7 个 Scenario** 天然**零漂移**。

## Acceptance

- [ ] 标题已订正，且与正文一致；
- [ ] 该 Requirement 的**全部 Scenario** 仍在（逐字保留）；
- [ ] `openspec validate --specs --strict` 的**总体**结果**零回归**
      （✗ 清单与改动前**逐条一致**）；
- [ ] **零**代码改动；`ctest -LE perf` 零回归；
- [ ] 归档（**禁** `--skip-specs`）。

## Capabilities

- `chlib-conditional-semantics` ⇒ **RENAMED ×1** ⇒ 归档**禁** `--skip-specs`。

## Impact

- **改动文件**：`openspec/specs/chlib-conditional-semantics/spec.md`（归档时合并）；
- **计数**：**不变**；**爆炸半径**：**零**（**无代码改动**）；**风险**：**极低**。

## Non-Goals

- **不**改该 Requirement 的**正文**与任何 Scenario；
- **不**改任何代码。
