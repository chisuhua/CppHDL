# Proposal: 语料级覆盖栅栏的硬化（豁免机制的可机械核验）

## Why

### 1. 缺口：capability 承诺的**一半**没有机器判据 ⇒ ⇒ 假绿契约

`openspec/specs/hierarchical-corpus-regression/spec.md` 的 scenario「豁免不是垃圾桶」明文写：

> 该豁免 MUST 附**具体**原因 ⇒ ⇒ ⇒ 且该 DUT MUST **确实**存在于语料里

⇒ ⇒ **两条**承诺 ⇒ ⇒ ⇒ 但护栏 `scripts/verify_hier_corpus_coverage.sh` 只核验了
**后一条**（方向 **③'**）⇒ ⇒ ⇒ ⇒ **前一条「附具体原因」完全没有机器判据** ⇒ ⇒ ⇒
⇒ 在豁免表里写 `| `Foo` | — |` 或 `| `Foo` | 暂无 |` ⇒ ⇒ ⇒ **照样通过** ⇒ ⇒ ⇒
⇒ **假绿契约**（★ 契约面写着 MUST，护栏面没有对应判据 ⇒ ⇒ ⇒ 「契约强度」被**散文**支撑）。

### 2. 缺口：豁免与「已覆盖」的**互斥性**未钉住

「豁免」的语义 = 「**不覆盖**」⇒ ⇒ ⇒ 一个 DUT 若既在登记清单（= 已有覆盖 + 有用例）
又在豁免清单 ⇒ ⇒ ⇒ 两个语义**互相矛盾** ⇒ ⇒ ⇒ 而当前护栏**同时接受**这两者 ⇒ ⇒ ⇒
⇒ 「用豁免表给已覆盖的 DUT 打掩护」是一条**未被封**的路。

### 3. 缺口：方向 ③' 的**键控不一致**（潜伏 bug）

- 分析器把豁免表按**简单名**键控（`exempt[simple] = reason`）
- 而方向 ③' 拿豁免项去与 `COMPOSITE` 行做 `grep -qx` **全行**比对（⇒ ⇒ 比的是
  **带限定名的完整串**）
⇒ ⇒ ⇒ 若语料里出现**命名空间限定**的复合 DUT 调用（★ 例如 `chlib::Foo`）⇒ ⇒ ⇒
⇒ 合法豁免会被 ③' **误杀** ⇒ ⇒ ⇒ ⇒ 当前语料**零**限定名调用 ⇒ ⇒ ⇒ 这是**潜伏边**
（★ 由 critic 第 2 轮 NIT 报出 ⇒ ⇒ 当时判定「不影响本 change」⇒ ⇒ ⇒ 现登记为待修）。

## What Changes

1. **护栏新增 2 条方向**（`scripts/verify_hier_corpus_coverage.sh`）：
   - **③''**：豁免的**原因** MUST **非空**且 MUST NOT 命中**占位符集合**
     （`—` · `-` · `N/A` · `NA` · `TODO` · `TBD` · `无` · `?` · `待定`）
   - **③'''**：同一个 DUT MUST NOT **既**在登记清单**又**在豁免清单
2. **修方向 ③' 的键控不一致**（⇒ 豁免项与语料 DUT 的比较 MUST 两侧都取**简单名**）
3. **`--self-test` 新增 4 条投毒**（⑥ 空原因 ⇒ 红 · ⑦ 占位符原因 ⇒ 红 ·
   ⑧ 既登记又豁免 ⇒ 红 · ⑨ 限定名 DUT 的豁免 ⇒ **绿**（负向对照 ⇒ ⇒ 证明 ③' 的修复
   **不误杀**））
4. **capability delta**：`hierarchical-corpus-regression` **ADDED ×1**
   （把「具体原因」**钉成可判定形式** + 互斥性 + 键控差异不得误杀）

## 不做（Non-Goals）

- ✗ **不**改主 spec Requirement 1 的既有正文（⇒ ⇒ ⇒ **ADDED** 而非 **MODIFIED**
  ⇒ ⇒ ⇒ ⇒ 无「整条替换 ⇒ 静默覆盖」的风险）
- ✗ **不**动方向 ① / ② / ③ 的语义（★ 本 change 只**加**判据 + 修 ③' 的键控 bug）
- ✗ **不**做「全语料产物级扫描」（★ 122 个叶子 DUT ⇒ ⇒ 那是**独立的** backlog 项
  ⇒ ⇒ ⇒ **不在**本 change）
- ✗ **不**抽 lint 口径的共享 helper（★ critic 第 1 轮 SHOULD_FIX-3 已以**交叉引用
  注释**处置 ⇒ ⇒ ⇒ 抽 helper 是**后续项**）
- ✗ **不**改台账（★ 台账措辞归 architect ⇒ ⇒ ⇒ **不**在本 change）

## Acceptance

> 完整判据清单见 `tasks.md` §3。

- [ ] **契约**：capability 主 spec 新增 1 条 Requirement（「具体原因 + 不覆盖」可机械核验）
      ⇒ ⇒ `openspec validate --specs --strict` **无新增 warning**
- [ ] **实现**：护栏新增 2 条方向 + 修 ③' 键控 ⇒ ⇒ **9 条投毒**全部符合期望 + 还原复绿
- [ ] **负向对照**：投毒 ⑨（限定名 DUT 的豁免）**必须绿** ⇒ ⇒ ⇒ 否则 ③' 的修复**误杀**
- [ ] **零回归**：`ctest -LE perf` 全绿 · samples · examples · 计数锚点不变 · 既有 6 个护栏 PASS

## Capabilities

| capability | delta |
|---|---|
| `hierarchical-corpus-regression` | **ADDED ×1**（MODIFIED ×0 ⇒ ⇒ ⇒ ⇒ 归档**禁** `--skip-specs`） |
