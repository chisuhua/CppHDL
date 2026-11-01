# Change: chlib-stream-ready-ownership-purpose-correction-2026-11

> ⚠️ **零 spec 变更声明**（见 `.openspec.yaml`）⇒ ⇒ **归档后手工补写** Purpose。

## Why

🔴 **权威契约面正在陈述一个【已被实测推翻】的缺陷**（critic BLOCKER B2 · 2026-11-01）。

`openspec/specs/chlib-stream-ready-ownership/spec.md` 的 `## Purpose` 段
（**line 54-56**）写：

> ⚠️ **另有一项独立的真实缺陷**（⇒ ⇒ **不属于**本 capability）：builder 的
> 链式 `queue<>()` **静默丢字** ⇒ ⇒ ⇒ `.rddf/improvements/stream-builder-chain-silent-word-loss-2026-11.md`。

⇒ ⇒⇒ **但 A3（2026-11-01）已实测推翻「静默丢字」**：

| 台账原描述 | 实测真相 |
|---|---|
| 「第 10 拍 `out.payload=0x00 out.valid=0`（期望 5A 1）❌」 | **消费端恒 ready ⇒ 每拍弹一个 ⇒ 弹光后自然为空** ⇒ ⇒ **不是丢字**（是**消费完成**） |
| 「链长 2：第 1 拍后 `out.payload=0x00 out.valid=0`」 | **每级 `sread` 1 拍 ⇒ 链长 2 ⇒ 延迟更深** ⇒ ⇒ 第 1 拍为空是**正常**的 |

⇒ ⇒⇒ **真正的缺陷是「上游反压从未建立」**：
`stream_fifo` **不驱动**入参 `ready` ⇒ ⇒ ⇒ builder 链里 `source.ready` **恒 0**
⇒ ⇒ ⇒ ⇒ **⇒ 上游遵守 ready 协议 ⇒ ⇒ 永远推不动 = 死路**（实测 E10）。

⇒ ⇒⇒ **⇒ 后果**：in-force 契约面把一个**不存在**的缺陷写成「真实缺陷」
⇒ ⇒ ⇒ **⇒ 下一个人按 spec 去查「丢字」永远查不到** ⇒ ⇒ ⇒ ⇒ ⇒
**⇒ ⇒ ⇒ ⇒ ⇒ ⇒ 这就是「假绿固化」的【契约面】版本**（断言面没有这个问题）

## What Changes

**唯一动作**：归档后**手工补写** `## Purpose` 的那三行（**只改这一处**）：

- 「**静默丢字**」⇒ ⇒ ⇒ 「**上游反压从未建立**（`source.ready` 恒 0 ⇒ 上游遵守
  ready 协议即永远推不动 = 死路）」
- 补上「**已被 A3 实测推翻**『静默丢字』」的注记 + 指向新台账

⚠️ **MUST NOT** 改动任何 `## Requirements` 段 ⇒ ⇒ ⇒ **⇒ 本 capability 的
契约条款零变更**（本次只订正 Purpose 的**事实陈述**）。

## 为什么不做什么

- ❌ **不**改任何 Requirement ⇒ ⇒ ⇒ 那些条款**仍然成立**（A3 推翻的是
  **台账的根因描述**，不是 capability 的条款）；
- ❌ **不**改台账文件名（`stream-builder-chain-silent-word-loss-2026-11.md`
  仍含旧词）⇒ ⇒ ⇒ ⚠️ 已在 critic 的 SHOULD_FIX 里登记 ⇒ ⇒ ⇒ **⇒ 改名会
  打断收口清单的 `debt:` 指针** ⇒ ⇒ ⇒ ⇒ ⇒ **⇒ 本 change 不动**；
- ❌ **不**把 B1 的修复（上游反压）提前做进来 ⇒ ⇒ ⇒ 那需要 architect 裁决
  ⇒ ⇒ ⇒ **独立 change**（见 `.rddf/checks/architect-decisions-pending-2026-11.md`
  的 B1）。

## Impact

- **改动文件**：`openspec/specs/chlib-stream-ready-ownership/spec.md`
  （**仅 `## Purpose` 的 3 行**）+ 本 change 工件
- **爆炸半径**：**极小**（纯事实订正，零实现改动）
- **契约条款变更**：**零**（`## Requirements` 逐字不动）
- **风险**：**低** —— ⚠️ 唯一需注意的是「**手工补写 spec**」这一步本身绕过
  了常规流程 ⇒ ⇒ ⇒ **⇒ 故 MUST 留注记 + MUST 只改那一处** ⇒ ⇒ ⇒ **⇒
  ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒**

## 验收

- [ ] 归档**后**手工补写 `## Purpose` 的那 3 行（**且只改这 3 行**）
- [ ] 补写处**留注记**（改动人 / 依据 / change 名 / A3 实测结论）
- [ ] `openspec validate chlib-stream-ready-ownership --type spec --strict` ⇒ ⇒ **valid**
- [ ] `openspec validate --specs --strict` 的失败数**不增加**（基线 21）
- [ ] `git diff` 确认 `## Requirements` 段**零改动**
- [ ] `ctest -LE perf` 零回归 · 全部护栏 exit 0
