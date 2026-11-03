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

## ⚠️ 已知缺口（**待 planner 裁决** · 2026-11-03 登记）

**移除项没有归属实体** ⇒ 本 feature 会一直 `active`：

- 本条把「移除」判为 **phase-6** 的 calendar-gated 清单
  ⇒ ⇒ 但 `.rddf/roadmap/phases/` **只有 phase-1 … phase-5** ⇒ **phase-6 不存在**
  ⇒ ⇒ 该项目前**只**挂在本 feature fragment 里（`status: active`）。
- 三个可选项（**planner 裁决**）：
  ① **建 `phase-6.md`**（把 calendar-gated 清单正式化）⇒ 成本 = 路线图结构变更；
  ② **改指 phase-5**（把移除写成 phase-5 的验收标准之一，注明日历锁）⇒ 成本 = phase-5 的
     收口被推迟到 2027-04-17 之后（**与 2026-10-26 修订的初衷相反** ⇒ 已被否决过一次）；
  ③ **留在本 feature 里**（现状）⇒ 成本 = feature 永远 `active`（一个只等日历的条目）。
- ⚠️ **日历锁本身已有机械信号**（2026-11-03）⇒ ⇒ 「提前移除会被 CI 抓到」这件事
  **不再依赖 planner** ⇒ ⇒ 选项 ③ 的实际风险只剩「路线图上挂着一个长期 open 的条目」。

## 验收标准

- [x] 全量 migration 完成
- [x] `[[deprecated]]` 标记落地
- [ ] `[[deprecated]]` 移除（无 `-Wdeprecated-declarations` 残留）
