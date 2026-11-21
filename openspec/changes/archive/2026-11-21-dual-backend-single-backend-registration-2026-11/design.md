# Design — `dual-backend-single-backend-registration-2026-11`

## 1 判据链

```
tests/test_verif_history_primitives.cpp  带 verilator_e2e 标签
        │
        ├─ macro 形态？ ── 否（两个 half 是「两份载体跑同一份 .verif」，不是「同一 DUT 跑两个 backend」）
        │
        ▼
capability「现有 9 个 verilator 测试 MUST 保留原状」= 契约锁定清单（非 macro 形态条目的登记处）
        │
        ├─ 已登记？ ── 否 ⇒ ⇒ 护栏判 unclassified-labeled-entry（本次修的格）
        │                        （已登记的同型先例：test_verif_wait_level.cpp · 2026-11-20）
        ▼
登记后 ⇒ 护栏把它归入「契约锁定」类 ⇒ ⇒ 不判违规 ⇒ ⇒ 三个锚点全部不动
```

## 2 为什么走 change 流程（不是直接改主 spec）

`docs/developer_guide/openspec-specs-guide.md` §硬性规则 4：**Spec 变更 MUST 走 change 流程**
⇒ ⇒ 本 change 带 `## MODIFIED Requirements` delta ⇒ ⇒ 归档时由 CLI 自动合并
⇒ ⇒ **不得** `--skip-specs`。

⚠️ **MODIFIED delta 的载体形态** = 该 Requirement 的**完整新正文**（CLI 按整条替换）⇒ ⇒
delta 正文 = 主 spec 现有 Requirement 正文（**逐字复制**）+ 新增的 dated 登记段。

## 3 边界

- **不动** guard 的判据（哪些条目 MUST 登记）⇒ ⇒ 只补**登记面**的漏项。
- **不动** 三个锚点 ⇒ ⇒ 分类修正不是覆盖变化。
- **不动** 测试代码 ⇒ ⇒ 该套件的行为与标签**均正确**。
