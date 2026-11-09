# improvement-suggestions —— 改进提案索引

> **本文件** = `.rddf/improvements/` 下 improvement 草稿的**索引**（一行一条）⇒ ⇒ ⇒
> 提案本体在 `.rddf/improvements/<name>.md`（★ **5 段 / 7 段**草稿 ⇒ 创建前 MUST 过
> `skills/rdd-workflow-brainstorm/scripts/pre_create_brainstorm_check.sh` 的 HARD-GATE 校验）。
>
> ★ **生命周期**：本文件（提案池 · 待审查）⇒ ⇒ **审查批准** ⇒ ⇒ `improvement-approved.md`（已批准 · 待实施）
> ⇒ ⇒ ⇒ 实施 / 归档后从本表移除。

| 提案 | 优先级 | 来源 | 添加时间 | 状态 |
|---|---|---|---|---|
| [mem-port-arg-cross-layer-2026-11](.rddf/improvements/mem-port-arg-cross-layer-2026-11.md) | P1 | ADR-066 §2.7 决策 ⑦（归下游独立立项）· 台账 §7 行 20 | 2026-11-09 | 📝 draft（**待 P4 的 architect 范围裁定**落地后才可转 approved ⇒ 才可立项为 change） |
| [chlib-dtor-noexcept-composability-2026-11](.rddf/improvements/chlib-dtor-noexcept-composability-2026-11.md) | P1 | 上游 issue #28 **第 ② 半**（析构 `noexcept(false)` ⇒ 类型不可组合）· **ADR-067（Accepted 2026-11-09 · 用户拍板）** | 2026-11-09 | ✅ **approved**（架构阻塞已解除 ⇒ **可立项为 change** ⇒ 实施 = phase-6 **P15**） |
| [public-api-remove-visibility-2026-11](.rddf/improvements/public-api-remove-visibility-2026-11.md) | P1 | 上游 issue #28 **第 ① 半**（公开 API 移除的跨仓可见性 · CHANGELOG 载体 · 判决规则边界）· phase-6 P12 | 2026-11-09 | ✅ **approved（用户拍板 R1 + R2）+ 已实施并归档**（change = `openspec/changes/archive/2026-11-09-public-api-remove-visibility/` · 实施 = phase-6 **P14**） |
