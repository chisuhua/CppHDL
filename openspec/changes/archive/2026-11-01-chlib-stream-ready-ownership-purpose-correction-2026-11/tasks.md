# Tasks: chlib-stream-ready-ownership-purpose-correction-2026-11

> 勾选规则：**commit 之后**按 `git show --name-only <sha>` 核对实际改动再勾。

## T1 事实核对（**先证据后动手**）

- [x] **T1.1** 定位主 spec 的被推翻陈述：`chlib-stream-ready-ownership/spec.md:54-56`
- [x] **T1.2** 核对 A3 的实测结论（「静默丢字」被推翻 ⇒ 真缺陷 = 上游反压
      从未建立 ⇒ `source.ready` 恒 0 = 死路）
- [x] **T1.3** 确认「归档**不合并** delta 的 `## Purpose`」（AGENTS.md 3 处明文）
- [x] **T1.4** 确认仓内**先例**：`cpu-clint-timer` / `bundle-master-slave-io` /
      `bundle-base-integer-ctor` 均在归档后手工补写 Purpose

## T2 归档

- [x] **T2.1** `openspec validate --type change --strict` ⇒ ⇒ **valid**
- [x] **T2.2** 归档（`--skip-specs`，依据 `.openspec.yaml` 的零 spec 变更声明）

## T3 归档**后**手工补写 Purpose（**唯一动作**）

- [x] **T3.1** 只改 `## Purpose` 的那 3 行：「静默丢字」⇒ ⇒ 「上游反压从未
      建立（`source.ready` 恒 0 ⇒ 死路）」
- [x] **T3.2** 在补写处**留注记**：改动人 / 依据（change 名）/ A3 的实测结论
- [x] **T3.3** `git diff` 确认 `## Requirements` 段**零改动**

## T4 验收

- [x] **T4.1** `openspec validate chlib-stream-ready-ownership --type spec --strict` ⇒ ⇒ **valid**
- [x] **T4.2** `openspec validate --specs --strict` 失败数**不增加**（基线 **21**）
- [x] **T4.3** `ctest -LE perf` 零回归（**196/196**）
- [x] **T4.4** 全部护栏 exit 0

## 验证命令

```bash
openspec validate chlib-stream-ready-ownership-purpose-correction-2026-11 --type change --strict
openspec archive chlib-stream-ready-ownership-purpose-correction-2026-11 --yes --skip-specs
openspec validate chlib-stream-ready-ownership --type spec --strict
openspec validate --specs --strict | grep -cE "^✗ spec/"
git diff --stat openspec/specs/chlib-stream-ready-ownership/spec.md
```
