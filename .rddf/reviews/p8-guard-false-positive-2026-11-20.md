# `verify_hierarchical_p8.sh` 假阳性修复（P0 · 2026-11-20）

## 症状

CI job `build-and-test:Verify public API surface (P8 guard, R1–R4)` 自 2026-11-18 起
**恒红**，与当次改动无关：

```
❌ R4② 活账登记的头**不存在**：include/axi4/axi4_lite_example_top.h
❌ R4② 活账登记的头**不存在**：include/axi4/axi_soc_example_top.h
verify_hierarchical_p8: FAIL（2 条违规）
```

## 根因

`check_r4_registry()` 的如实意图（注释已写）是「①② 都只看『登记清单』段」，
但实现是**黑名单**：只跳过 `^## .*已移除` 一个段标题。

`88eae35`（2026-11-18 · `public-api-remove-visibility-2026-11` D2）把两个
`include/axi4/*_example_top.h` 的退役登记搬进 **`## ⚠️ **行为变更（非符号移除）**`**
段的迁移指引 ⇒ ⇒ 该段路径**按定义**不存在（是「改了行为、没删符号」的叙述）⇒ ⇒ 护栏
把它们当「已登记的头不存在」⇒ ⇒ 假阳性。

⇒ 这是 F-2（2026-11-13 修「已移除」段同类假阳性）**漏掉的第二个段标题**。

## 修法

`scripts/verify_hierarchical_p8.sh::check_r4_registry()`：

1. **登记面收窄为白名单** —— `reg_scan` 只取 `## ` 标题含「登记清单」的段
   （其余各段是历史记录 / 叙述，一律不参与 ①②）。
2. **fail-closed** —— 该段缺失或为空（`grep -q '`include/'` 不命中）⇒ **红**并直接
   `return 1`，否则「改一个段标题」就能把双向核对**静默关掉**。

附带收益（不是本次的目的，但方向一致）：黑名单下可以把一个**活着**的公共头的路径写进
任意非历史段（如「维护规则」/「相关」）冒充「已登记」⇒ ② 静默放过；白名单堵住这条路。

`.rddf/checks/public-api-surface.md` 的「维护规则」段补一条：登记面 = 只有
「## 登记清单」这一段，其余段路径不要求存在、也不构成「已登记」。

## 判别力证据（`--self-test` 投毒）

新增/搬迁后共 **9 条投毒**，全部预期红：

- ⑦（**搬迁**）活账登记一个不存在的头 —— 旧实现是「追加到文件尾」（落在「相关」段），
  收窄后会**失效**；现改为用 awk 插到「登记清单」段末，且改用 `$WORK` 下的活账副本
  （旧实现声称「用副本」但实际临时改写真活账）。
- ⑧（**新增**）把**活着**的 `include/hierarchy_equivalence.h` 从登记清单搬进
  「行为变更」段 ⇒ 期望红（② 判「存在未登记」）。
- ⑨（**新增**）把「## 登记清单」段标题改掉 ⇒ 期望红（fail-closed）。
  两条都带**投毒构造自检**（搬移/改标题没真的发生 ⇒ 直接 fail「投毒构造失败」）。

**变异自证**（把判据换回旧实现、其余不变）：

| 场景 | 旧判据（黑名单，无 fail-closed） | 新判据 |
|---|---|---|
| 真活账 · 主路径 | FAIL 2 条（= CI 现状的假阳性） | PASS |
| 活账 fixture（去掉两条 permanence 历史行，使基线可绿）· `--self-test` | ⑧ 竟被判绿 · ⑨ 竟被判绿 | ⑧ ⑨ 均红 |

⇒ 两条新投毒各自对应新判据的一个分支，且在旧判据下**确实漏报**（不是恒绿的空投毒）。

## 验证

```
bash scripts/verify_hierarchical_p8.sh            → PASS（R1–R4）
bash scripts/verify_hierarchical_p8.sh --self-test → 基线绿 + 9 条投毒全红 + 还原复绿
bash scripts/verify_ledger_evidence.sh            → PASS
bash scripts/verify_guard_reachability.sh         → PASS
bash scripts/verify_docs_links.sh                 → PASS
```

## 残留（本次不做）

- `mod` 字面量的定宽口径：框架侧 `compute_bit_width(N-1)` vs Verilog 自然宽
  `max(两侧)` ⇒ N 非 2 的幂时两后端可能不同（N=3、a=4 ⇒ 框架 0 / Verilog 1）。
  仓内 N 全是 2 的幂（4/2）⇒ 当前不可触发 ⇒ 属架构裁决，登记待办，不在本次范围。
