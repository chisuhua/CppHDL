# openspec-purpose-placeholders-2026-11

## 动机

`openspec/specs/` 下 8 个 capability 的 `## Purpose` 仍是 openspec archive 自动写的占位符
（`TBD - created by archiving change <X>. Update Purpose after archive.`）⇒ ⇒ 它们是
`openspec validate --specs --strict`（CLI 1.14.3，仓级 23 passed / 28 failed / 51 items）的
红项之一 ⇒ ⇒ **每次读这 8 个 capability 的第一屏拿到的是工具噪声，不是语义**。

用户（2026-11-19）要求：**重新审查这 8 个 capability 的 Requirement 是否正确 / 合理 / 是否过期，
给出修订建议** ⇒ ⇒ 审查已完成（见下「审查产物」），结论是：**8 段 Purpose 该补，但这 8 个 capability
里至少 6 个含与实现相反 / 不可复验 / 把一次性历史写成常驻义务的 Requirement** ⇒ ⇒ 必须分批。

## 目标

1. **批 A（本 change）**：为 8 个 capability 补写**实质** `## Purpose`，**零 Requirement 文本改动**。
2. **交付一个可逐字批准的对象**：`design.md` §2 的 8 段文本 = 执行时**逐字**写进主 spec 的内容。
3. **交付一份机械可判的验收口径**：`tasks.md` 的 AC-0…AC-7。
4. **把「过期 Requirement」显式移出本批**并给出逐条判据与立项顺序（批 B），避免「顺手改」。

## 非目标

- ❌ **不改任何 Requirement 正文 / Scenario**（AC-4 机械钉死）。
- ❌ 不动其余 43 个 capability 的 `>500 字符` warning（= 另一个项目）。
- ❌ 不动仓级 strict 的其余红项（本批只清 8 条 Purpose warning ⇒ 仓级 23→27 passed / 28→24 failed）。
- ❌ 不给 `rv32i-privilege` 改名（正文已有范围声明；已登记的扩展计划会因此作废）。
- ❌ 不补 VCD「每拍自动 dump」的实现（= 实现缺口，独立立项）。
- ❌ 不新增 `openspec/specs/**` 的 capability，不改任何 `### Requirement:` 标题（⇒ 无 delta）。

## 裁决依据（为什么允许直改主 spec）

完整证据链见 `design.md` §1（G1–G7）；一句话：

> openspec 的 delta **机制上不含 Purpose**（CLI 提示 + 仓内护栏 `scripts/verify_openspec_delta.sh:112`
> 双重证实）⇒ ⇒ **change 流程对 Purpose 是死路** ⇒ ⇒ 而本仓**已裁决并已执行过**该场景
> （`ADR-072` §6 ⑨「归档后手工写」+ commit `1952ba5` 的 T17）⇒ ⇒ 本 change 采用**同一形状**：
> change 承载**任务 + 逐字文本**，执行时手工写进主 spec。

⇒ 本 change **不违反** AGENTS.md「spec 变更必须走 change」—— 该禁令的**本意**是防「绕过 delta 改
Requirement」，而本批 **零 Requirement 改动** ⇒ ⇒ 零绕过。

## 审查产物（本 change 的依据）

| 工件 | 内容 |
|---|---|
| `.rddf/reviews/openspec-purpose-placeholders-review-2026-11.md` | 完整审查报告：事实基线 / 19 条过期项逐条判据 / 8 段草稿 / AC / 护栏建议 / **oracle（判定 REJECT）+ metis 的逐条应答** |

## 交付物

1. `openspec/specs/<8 个 capability>/spec.md` 的 `## Purpose` 段（逐字取自 `design.md` §2）。
2. 根 `AGENTS.md`「OPENSPEC WORKFLOW」段的 **strict 快照订正**（`21 passed / 23 failed (44 items)`
   → 实测 `23/28/51`）+ **一行通道说明**（「Purpose 无 change 通道 ⇒ 归档后手工写」⇒ 引用 ADR-072 §6 ⑨）。
3. `scripts/verify_openspec_purpose.sh` + `.rddf/checks/openspec-purpose-placeholders.tsv`
   （护栏：占位符集合 **双向** 校验 + `--self-test` 投毒）⇒ 照 `scripts/verify_docs_links.sh` 的四件套抄。

## 本 change **无 spec delta**（`.openspec.yaml: skip_specs: true`）

- openspec 的 delta **机制上不含 `## Purpose`** —— CLI 提示原文：「a `## Purpose` in a delta is
  read only when the capability is created, so it cannot replace this one」；仓内护栏
  `scripts/verify_openspec_delta.sh:112` 同一口径（「`## Purpose` 不会被合并 ⇒ 不算违规」）。
- ⇒ ⇒ **走 change 流程无法修改既有 capability 的 `## Purpose`** ⇒ ⇒ 该动作属
  **「归档后手工写」通道**，本仓已裁决并已执行过：`ADR-072` §6 ⑨ + commit `1952ba5`（T17）。
- ⇒ ⇒ 本 change 承载的是**任务 + 逐字文本**（`tasks.md` T2…T9 逐字写进主 spec），
  **不产生任何 `### Requirement:` 改动**（AC-4 机械钉死）⇒ ⇒ 归档时无需合并 spec。
- ⇒ ⇒ 用 `.openspec.yaml` 的 `skip_specs: true` 声明「本 change 显式声明 no spec changes」
  （仓内先例：`fix-ns-pollution` 用归档 flag `--skip-specs`；本仓 AGENTS.md 允许「change 显式声明
  no spec changes」时使用该通道）⇒ ⇒ 二者等价，此处用 yaml 标记 ⇒ **不依赖人记得加 flag**。
- ⚠️ **代价（已知并接受）**：`openspec validate <change> --strict` 的「delta 完整性」检查对本 change
  **不适用** ⇒ ⇒ 它的实质校验落在 **AC-0…AC-7**（`tasks.md`）与 `design.md` §4.1 的机械预检上。

## 跟踪面（你在哪里能看到它的进度）

| 视图 | 命令 | 性质 |
|---|---|---|
| **openspec（权威 · 入库）** | `openspec list` / `openspec validate <change>` | ✅ 提案 + 逐字终稿 + 任务 checkbox **都在 git 里** ⇒ 跨会话/跨人可见 |
| **rddf 面板（本机 · 不入库）** | `rddf status openspec-purpose-placeholders-2026-11` | ✅ 已写入 `.rddf/state/iteration.json`（`status: proposed` · `tasks 0/23`）⇒ ⚠️ `.rddf/state/` 整个被 `.gitignore` ⇒ **只在本机可见** |

## 验收

见 `tasks.md` 的 **AC-0…AC-7**（AC-0 是否决性条款：**不得**用仓级 strict totals 当判据）。

## 风险

见 `design.md` §4 的三条（散文注水 / 把假话写进第一屏 / 假绿），每条配反面判据。

## 批准门

**T0 是硬门**：未经用户显式批准，**不得**执行 T1 及之后任何任务（改主 spec / 改 AGENTS.md / 落护栏）。
批准方式二选一（用户在会话中给出即可）：

- **A**：在会话里说「批准批 A」⇒ 执行者把批准语句 + 日期写进 `tasks.md` 的 T0 记录；
- **B**：直接在 `tasks.md` 的 T0 记录区填上「已批准 + 日期 + 批注」。

> ⚠️ **批准前请至少看这三处**：`design.md` §1（裁决依据）· `design.md` §2（**逐字终稿**）·
> `design.md` §4（三条风险 + 反面判据）· `tasks.md` 的 AC-0…AC-7。
