# Proposal: 7 个 RV32I 模块类型名改判为**明文非契约**（ADR-070 §6 ⑦ 的用户裁决）

## Why

ADR-070 §2.3 把 7 个模块类型名（`IfStage` / `IdStage` / `ExStage` / `MemStage` / `WbStage` /
`HazardUnit` / `ForwardingMux<32>`）按**推荐取向**写成「产物级契约名」，但其**强度**
在 §6 ⑦ 明确标注为**待裁决项**（ADR 自身声明「§2.3 不冻结」）。

⇒ ⇒ **用户 2026-11-15 裁决 = 明文非契约**。已按该取向落入
`rtl-hierarchical-output` 的 Requirement（`rv32i-hierarchical-example-2026-11` 归档时），
但那段文字仍写着「MUST 是产物级契约名」⇒ ⇒ **与本裁决相反** ⇒ ⇒ 必须改。

## What Changes

`rtl-hierarchical-output` 的该 Requirement：**标题改写**（`REMOVED` + `ADDED`，因为标题本身
写着「MUST 是产物级契约」）+ 正文把「MUST 是产物级契约名（改名 = 破坏性变更）」改为
「**不是契约**（改名 **不**构成破坏性变更）」：

- 改名的**唯一**义务 = 产物**重新 lint 干净**（名字仍 MUST 是合法 Verilog 标识符）；
- 稳定性来源是**成层粒度**（`ch_layer` 7 处）而**不是**名字。

## Out of Scope

- **ADR-070 正文**（§2.3 / §6 ⑦ 的措辞）⇒ **architect 域**；本 change 只落 capability 面
  （ADR §2.3 已明文授权：「capability 起草方 MUST 以 §6 ⑦ 的裁决为准」）。
- **成层粒度**（7 处、`load_lane` 保持 inline、8 module）⇒ **零变更**。

## 验收标准（可勾选）

- [x] 主 spec 的该 Requirement 标题不再含「MUST 是产物级契约」
- [x] 正文含「不是契约」且明写裁决来源与日期
- [x] `openspec validate --specs` 通过
- [x] `docs/CHANGELOG.md` 的同一措辞同步

## 用户可见变更

| 面 | 变更 | 迁移动作 |
|---|---|---|
| 产物里的 module 名 | **不再是契约**（改名不构成破坏性变更） | 无 |
| 产物本身 | **零变更** | 无 |
