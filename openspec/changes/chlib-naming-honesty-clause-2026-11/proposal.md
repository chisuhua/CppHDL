<!-- touched-components: stream_demux, stream_fork, stream_arbiter_round_robin, sync_fifo, stream_fifo -->

# 提案：命名诚实性的**可判定**条款（B8）

- **架构依据**：`.rddf/checks/architect-decisions-pending-2026-11.md` 的 **B8 裁决**
  ——「**收窄为**『本次 change 引入或改动的组件 MUST 有一条行为用例锁定其
  名字/注释声称的语义』（**可判定**口径）⇒ ⇒ ⇒ **不是**「全仓普查」那种
  不可验证的义务」
- **规格影响**：`chlib-stream-arbitration` 的 **MODIFIED ×1**
  （`仲裁器的命名与文档 MUST 与实现行为一致` —— 既有 2 个 Scenario **逐字保留**
  + 新增 1 个）⇒ 归档**禁** `--skip-specs`
- **规模 / 风险**：小（1 张登记表 + 1 个分析器 + 1 个护栏脚本 + 1 条 spec 条款）
- **关联台账**：`.rddf/improvements/chlib-naming-honesty-audit-2026-11.md`

## Why

本仓已发生**三次**「名字/注释承诺了不存在的行为」：

| 组件 | 承诺 | 实际 | 发现方式 |
|---|---|---|---|
| `fwft_fifo` | FWFT（show-ahead） | `q` 只在**读之后**才更新 | A1 普查 |
| `stream_arbiter_sequence` | 「using round-robin selection」/「wraps around」 | `priority_encoder`（固定优先级） | A4 普查 |
| `stream_arbiter_round_robin` | 轮转 | 恒传 `last_grant = 0_d` ⇒ 低位通道饿死 | 轮转修复时 |

⇒ ⇒ 三次都是**临时普查**发现的 ⇒ ⇒ **下次还会漏** ⇒ ⇒ 需要一条**持续**的机制。

⚠️ **为什么不能直接立「全仓普查」条款**：全仓 `chlib` 有 **27** 个对外暴露的
组合子，其中大量只有**结构断言**（width / 节点存在）⇒ ⇒ 条款一旦落地就
**立刻全红** ⇒ ⇒ **不可达成** ⇒ ⇒ 这正是 B8 裁决把范围**收窄**到
「本次 change 引入或改动的组件」的原因。

## What Changes

| 文件 | 改动 |
|---|---|
| `.rddf/checks/chlib-naming-honesty-registry.md`（**新**） | 登记表：组件 → 它**声称**的语义 → 判别性用例（tag/macro）→ 登记原因。初始 **5** 条 |
| `scripts/lib/chlib_naming_honesty.py`（**新**） | **R1** 登记 ⊆ 组件全集（登记漂移）· **R2** 判别性用例存在且仍带其 tag（判别力消失）· **R3** per-change 义务（最近的 chlib change MUST 声明它触及的组件） |
| `scripts/verify_chlib_naming_honesty.sh`（**新**） | 护栏 + `--self-test`（3 种投毒） |
| `.github/workflows/ci.yml` | CI **2 处**挂载（主路径 + `--self-test`） |
| `openspec/specs/chlib-stream-arbitration/spec.md` | MODIFIED delta：Requirement 正文补「登记表」段 + 新增 Scenario「被改动的组件 MUST 登记并绑定判别性行为用例」（既有 2 个 Scenario **逐字保留**） |

## Non-Goals

- **不做**「全仓 27 个组件」的普查与补测（不可达成 ⇒ B8 裁决已否决）；
- **不实现**「未驱动字段诊断 API」（B7，独立 change）；
- **不改**任何组件的行为（本 change 只有契约 + 判据 + 护栏）。

## Verification（前瞻 · 可勾选）

- [ ] 登记表的每条：组件仍存在于 `include/chlib/`；判别性用例文件存在且仍带其 tag
- [ ] 最近的 chlib change 的 `<!-- touched-components: ... -->` 里的组件全已登记
- [ ] 护栏 self-test **3/3** 投毒全红 + 还原复绿
- [ ] CI 两处挂载可用（YAML 合法 + 脚本 `bash -n` 通过）
- [ ] `openspec validate … --strict` valid · `verify_openspec_delta.sh` 4 条全绿
- [ ] 收口清单第 14 行 → ✅ ⇒ 锚点 `16/1 → 16/0` ⇒ **stream 域收官**
