---
id: feat-hdl-logical-deprecation
kind: feature
status: active
phase_refs: [phase-4, phase-5]
主题: ADR-036 逻辑算子 deprecation 标记 → 全量迁移 → 移除（removal gate）
---

## 概述

`ch_bool` / `port<ch_bool>` 上的 `operator&&` / `operator||` 语义易误用（尤其 mixed `ch_bool` + `ch_uint` 的 nonzero-test）。ADR-036 采用"先标记 `[[deprecated]]` → 全量 migration → 到期移除"两阶段模式（之前第一次尝试因 migration 不完整回滚）。第二次尝试颠倒顺序成功：先 migration（35 文件、~233 行 `&&`/`||` → `&`/`|`），再加 attributes。

| 子项 | 状态 |
|---|---|
| 全量 migration（35 文件、~233 行） | ✅ 已实施（2026-10-16） |
| 8 个 overload `[[deprecated]]` | ✅ 已实施（2026-10-16） |
| 5 个 test 文件处理（3 sed + 2 pragma） | ✅ 已实施 |
| `[[deprecated]]` 移除 | ⏳ **不在 phase-5**（2026-10-26 修订：ADR-036 §8.1 禁 2027-04-17 前移除 ⇒ 归 phase-6 calendar-gated 清单）|
| 「0 新增调用点」机械化 | ✅ **已实施**（2026-11-03）：`scripts/verify_deprecation_callsites.sh`（R1 标记在位 / R2 0 新增调用点 / R3 豁免登记双向核对）+ `--self-test` 4/4 |
| 日历锁机械化 | ✅ **已实施**（2026-11-03）：`scripts/verify_deprecation_calendar_gate.sh`（移除日期 vs 生效期限 + §8.2 低活动顺延 + 部分移除即红 + fail-closed）+ `--self-test` 2/2 |

## 跨阶段拆分

### phase-4
- ✅ 35 文件 migration（`stream_bundle.h` / `chlib/*` / `axi4/*` / `cpu/pipeline/*` / `spinalhdl-ported` 7 例 / `riscv-mini/src` 等）
- ✅ 8 个 `ch_bool` / `port<ch_bool>` overload `[[deprecated]]`
- ✅ archive 至 `openspec/changes/archive/2026-10-15-2026-10-16-deprecate-hdl-logical-operators-redo/`

### phase-5
- [x] 确认 0 新增调用点（`feat-hdl-logical-deprecation` 的 phase-5 完成条件 —— 见
      `phase-5.md`：8 个 overload 仍标记在位 + 0 新增调用点；2 个 pragma 包裹的 test 是
      **既有覆盖**，不是新增）
- ⏳ ~~`[[deprecated]]` attributes 移除~~ ⇒ **已移出 phase-5**（2026-10-26 修订）：
  ADR-036 §8.1 规定 **2027-04-17 前拒绝任何移除 commit**（日历锁）⇒ 归 **phase-6** 的
  calendar-gated 清单（`phase-5.md` 完成条件同步说明）

## ✅ 移除归属已裁决（2026-11-03 · 用户拍板 = **选项 ③**）

**「`[[deprecated]]` 物理移除」留在本 feature 里，不建 `phase-6.md`、不改指 `phase-5`。**

| 项 | 内容 |
|---|---|
| 裁决 | **选项 ③**（留在本 feature 里） |
| 直接后果 | 本 feature **会一直 `active`** 到 2027-04-17 之后 —— 这是**已知的、有意的**状态，不是遗漏 |
| 为什么可以接受 | **日历锁已有机械信号**（`scripts/verify_deprecation_calendar_gate.sh` + 其 `--self-test` 2/2，2026-11-03）⇒ ⇒ 「提前移除会被 CI 抓到」**不再依赖任何人记得** |
| 剩余风险（如实登记） | 路线图上挂着一个只等日历的长期 `active` 条目 ⇒ ⇒ 用 `scripts/verify_roadmap_fragments.sh` 的 `C1/C2/C3` 仍能守住「计数与状态自洽」，但**守不住**「这个 `active` 是不是该结束了」⇒ ⇒ 靠人工在 2027-04-17 后复核 |
| 复苏条件 | 若届时希望把它变成 `completed` ⇒ 走**一次真正的移除 change**（8 个 overload + 7 处豁免调用点一起改，`docs/adr/ADR-036` §6）|

**被否决的选项**（留档，避免重复讨论）：
- ① 建 `phase-6.md` ⇒ 成本 = 路线图结构变更；收益 = 一个正式归属。**否决理由**：为一个只等日历的条目新增一个阶段，收益不抵结构成本。
- ② 改指 `phase-5` ⇒ 成本 = phase-5 的收口被推迟到 2027-04-17 之后，**与 2026-10-26 修订的初衷相反**（已被否决过一次）。

## 验收标准

- [x] 全量 migration 完成
- [x] `[[deprecated]]` 标记落地
- [ ] `[[deprecated]]` 移除（无 `-Wdeprecated-declarations` 残留）
