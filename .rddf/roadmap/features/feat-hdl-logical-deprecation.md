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
| `[[deprecated]]` 移除 | ⏳ **不在 phase-5**（2026-10-26 修订：ADR-036 §8.1 禁 2027-04-17 前移除 ⇒ 归 phase-6 calendar-gated 清单） |

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

## 验收标准

- [x] 全量 migration 完成
- [x] `[[deprecated]]` 标记落地
- [ ] `[[deprecated]]` 移除（无 `-Wdeprecated-declarations` 残留）
