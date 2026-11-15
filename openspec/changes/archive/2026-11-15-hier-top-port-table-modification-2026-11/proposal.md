# Proposal: 顶层端口表判据的 **MODIFIED 订正**（消除与实测/新口径的**活矛盾**）

## Why

⚠️ **2026-11-15 · oracle 对抗复核（F1 · 高危）**：ADR-062 v1.7 已把「顶层端口表 == 扁平端口表
（逐字逐序）」收窄为「**根段逐字逐序 + 其余 ⊆**」，且同一批归档的 change 已把新口径写进
capability（`## 顶层端口表与扁平产物的关系` Scenario）⇒ ⇒ **但主 spec 里那条旧 Requirement
仍原样保留**（「RTL 顶层 module MUST 走扁平规则」⇒ 顶层端口表 MUST 含「**全部** `ch_out`
（含 inline 后代的）」AND「与扁平产物**逐字逐序**相同」）⇒ ⇒ ⇒ **两条在仓内同时存在且互相矛盾**：

- 旧条：对 RV32I 交付物**被违反**（层次化顶层**不含**成层子组件的 `ch_out`；且实测 61 端口差 / 序差）
- 新条：对 RV32I 交付物**被满足**

⇒ ⇒ spec 是仓内「**current spec source of truth**」（根 `AGENTS.md`）⇒ ⇒ 下游（含 ChipForge）
此刻**无法判断哪条有效** ⇒ ⇒ **契约二义** ⇒ ⇒ 且 `rdd-verifier` 按 spec 验收 RV32I 交付物
必然判红 ⇒ ⇒ ⇒ **F1 必须在下一个归档前闭合**。

⚠️ **根因 = 镜像单向漂移**：ADR 侧改了、spec 侧没改 ⇒ ⇒ 本 change 把 spec 侧对齐（**只改口径，
不改能力**）。

## What Changes

**MODIFIED**（标题逐字匹配主 spec）：`成层且非根的 module 端口 MUST 覆盖自身 ch_in / ch_out，
且每个端口可追溯`

- 只改其中**一个 Scenario**（**标题保留**「RTL 顶层 module MUST 走扁平规则」⇒ ⇒ **可追溯性**；
  ⇒ ⇒ **正文**改为两段口径）：
  - ① **根 `io()` 端口段** ⇒ **逐字逐序**（⚠️ **不含** inline 后代 `ch_out`）
  - ② **其余端口** ⇒ 层次化顶层 **⊆** 扁平（每个条目逐字存在）
  - ③ 扁平 MAY 多出**成层子组件**的 `ch_out`（= 观测盲区）
  - ④ **排他句（承重）**：**扁平 \\ 层次化顶层** 的端口 MUST ⊆ {成层子组件的 `ch_out`}
    ⇒ ⇒ 等价 = 层次化顶层 MUST 含**全部** inline 后代 `ch_out`（⇒ 堵「漏抬」缝）
  - ⑤ **段归属 MUST 以 `PortProvenanceOrigin` 为准**（MUST NOT 以名称前缀推断）
- 其余 Scenario 与正文**逐字不动** ⇒ ⇒ **能力面零收缩**（非根 module 的 ⊇ + 五类来源 + 自驱动拒绝
  **全部保留**）。

## Out of Scope

- **ADR 侧正文**（ADR-062 v1.7 / ADR-060 v1.19 / ADR-066 v1.6）⇒ ⇒ 同一批已改（ADR 面）
- **能力面扩张**（新增能力 / 新增 Requirement）⇒ ⇒ 本 change **只 MODIFIED 一条既有 Requirement**
- **RV32I 交付物专属 Requirement**（「顶层端口表与扁平产物的关系」Scenario 已在
  `2026-11-15-rv32i-hierarchical-example-2026-11` 归档时并入）⇒ ⇒ 本 change 不动它
  ⇒ ⇒ 它与本条**一致**（同一口径）⇒ ⇒ **无重复、无矛盾**

## 验收标准（可勾选）

- [x] MODIFIED 标题与主 spec **逐字一致**（`scripts/verify_openspec_delta.sh` R3 绿）
- [x] 旧 Scenario 的三句（顶层端口表 = clock→reset→无 driver input→**全部** `ch_out`（含 inline 后代）
      / 与扁平**逐字逐序相同**）**已不存在于主 spec**
- [x] ⚠️ **Scenario 标题保留**（同名替换 ⇒ openspec CLI 的 MODIFIED 语义要求 Scenario 集合**同名**）
- [x] 新口径含**排他句**（堵「漏抬 inline 后代 `ch_out`」的缝）
- [x] 新口径含**机械切法**（`PortProvenanceOrigin` ⇒ MUST NOT 名称前缀推断）
- [x] 主 spec 里**无**「顶层端口表 == 扁平端口表（逐字逐序）」的旧句残留（grep 可验）
- [x] `openspec validate --specs` 绿 · 归档合并成功

## 用户可见变更

| 面 | 变更 | 迁移动作 |
|---|---|---|
| **capability `rtl-hierarchical-output`** | 「成层且非根的 module 端口 MUST 覆盖自身 `ch_in`/`ch_out`」这一条 Requirement 的**顶层端口表 Scenario 正文**改写：① **根 `io()` 端口段** ⇒ **逐字逐序**（⚠️ **不含** inline 后代 `ch_out`）② **其余端口** ⇒ 层次化顶层 **⊆** 扁平 ③ 扁平 MAY 多出**成层子组件**的 `ch_out`（= 观测盲区）④ **排他句**：**扁平 \\ 层次化顶层** 的端口 MUST ⊆ {成层子组件的 `ch_out`} ⑤ 段归属 MUST 以 `PortProvenanceOrigin` 为准 | ⚠️ **Scenario 标题保留**（openspec MODIFIED 语义要求 Scenario 同名）⇒ ⇒ **口径以正文 + `docs/adr/ADR-062-hierarchical-unit-interface-typing.md` §5 为准** |
| **判据的可判定性** | 「根段」不再靠**名称前缀**推断 ⇒ ⇒ 依赖**顶层端口表 == 扁平**这一强承诺的下游（外部 TB / ChipForge）**必须**按新口径核对 | 依赖「顶层端口表与扁平**逐字逐序相同**」的下游**必须**改为「根段逐字逐序 + 其余 ⊆」 |
| **能力面** | **零收缩**（非根 module 的 `⊇` + 五类来源可追溯 + 自驱动拒绝**全部保留**）⇒ ⇒ 无需迁移动作 | — |
