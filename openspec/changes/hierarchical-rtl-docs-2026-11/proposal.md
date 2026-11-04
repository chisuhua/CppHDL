# Proposal: 层次化 RTL 输出 · 第 8 段 —— 文档同步（`hierarchical-rtl-docs-2026-11`）

## Why

新能力若无文档 = 用户**猜**用法 ⇒ ⇒ 而本仓的用户面文档
`docs/developer_guide/patterns/HIERARCHICAL-RTL-AUTHORING-GUIDE.md` 此刻**仍在**
教一个**不存在**的宏（`CH_MODULE_NAME`）⇒ ⇒ 且**合并宏**的签名面共 **5 处**仍是旧形态
（架构文档 §10.6-A-⑥b）。

⇒ ⇒ 文档 MUST 与实现**零漂移**，否则用户照抄即编译不过。

## What Changes

1. `docs/usage_guide/05-component-design.md` §5 改写（层次化 RTL 输出）。
2. `docs/developer_guide/patterns/HIERARCHICAL-RTL-AUTHORING-GUIDE.md`：
   删掉 `CH_MODULE_NAME`（该宏**不存在**）· 改为 `CH_HIERARCHY_LAYER("name")` 形态
   · 补 `ch_layer<Helper> h{"h","helper_layer"}` 的**档 ③** 形态。
3. `docs/architecture/2026-11-02-hierarchical-rtl-output.md` 转正（当前态快照）。
4. 根 `AGENTS.md` 的「WHERE TO LOOK」补层次化入口。

## 不做（Non-Goals）

- **不**改任何实现代码。
- **不**引入新概念。

## Acceptance

- [ ] 上述 4 处文档与实现一致（**零漂移**）
- [ ] 用户面指南**不含** `CH_MODULE_NAME`（该宏**不存在**）；
      `CH_HIERARCHY_LAYER` MUST 写**带名字**的形态
- [ ] 合并宏的**签名面 5 处**全部改毕（架构文档 §10.6-A-⑥b 清单）
- [ ] `docs/development_guide` 的示例**可编译**（照抄即过）

## Capabilities

**无 delta** —— 纯文档 change ⇒ ⇒ `.openspec.yaml` 显式 `skip_specs: true`。
