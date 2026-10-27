---
name: dual-backend-full-coverage-inventory-2026-10
priority: P1
source: phase-5 计划 T7 的第一步（gap C：判据「≥ 100」可能不可达）
phase: phase-5
category: testing
type: research
created_at: 2026-10-27
---

# Improvement: T7（Phase 5 全量 dual）必须先交「可 dual 上限盘点」

- **Date drafted**: 2026-10-27
- **Drafted by**: build-agent (builder role)
- **Status**: 📋 5-segment draft，归属 feature `feat-dual-backend-coverage`（Phase 5）
  + objective `objective-test-coverage-100pct`（Go 判据第 2 条）
- **来源**: `.rddf/plans/2026-10-26-phase5-followup-execution-plan.md` §T7 与 gap C ——
  「T7 的『`ctest -L verilator_e2e` ≥ 100』**可能不可达** ⇒ 开工前先交『可 dual 上限盘点』」

## 1. Why (背景与动机)

`objective-test-coverage-100pct` 的 Go 判据第 2 条是「`ctest -L verilator_e2e` ≥ 100」
⇒ 现状 **37**（2026-10-27 实测）⇒ 差 **63** 个 dual 文件。

⚠️ 但**判据本身可能不可达**：`dual-backend-test-coverage` 的 R6 **排除清单**规定了
**5 类**不可 dual 的测试（纯编译期 / 裸 context / >64-bit 端口 / SECTION 重入 /
STATIC_REQUIRE）⇒ 在 166 个测试源（117 根 + 29 chlib + 3 jit + 17 examples）里
排除掉不可 dual 的部分后，**上限是未知数**。

⇒ 若盲目开工，会在做到一半才发现判据不可达 ⇒ 已投入的批次全部变成「为凑数而做」
⇒ plan 的 gap C 已明令：**先量化，若上限 < 100 ⇒ MUST 修订判据（由 planner），
不得硬凑**。

## 2. What Changes (变更范围)

**只读交付物**（**不改任何代码**）：

1. **盘点表**：166 个测试源逐个判定 ⇒「可 dual / 不可 dual」+ **理由**
   （逐条引用 R6 排除清单的哪一项，或给出「可 dual」的具体障碍）；
2. **可达上限估算**：可 dual 总数 + 按「每批 ≤ 10 文件」折算的**批次数** + 估时；
3. **判据结论**：
   - 上限 ≥ 100 ⇒ 给出达标路径（批次数 + 顺序建议）；
   - 上限 < 100 ⇒ 产出「**判据修订请求**」（建议值 + 依据）⇒ **交 planner 裁决**
     （⚠️ **build 不得自行修改 objective 的 Go 判据**）。

⚠️ **纪律**：盘点 MUST 用**可核**的形式（每行给 `文件:行号` + 判定 + 理由）⇒
不得给「大概 XX 个」这种不可核的数字。

## 3. Acceptance (验收标准)

- [ ] 盘点表覆盖**全部** 166 个测试源（可 dual + 排除 = 166，**行数守恒**）
- [ ] 每行给 `文件:行号` + 判定 + **理由**（引用 R6 的哪一条）
- [ ] 上限数字与盘点表**自洽**（可 dual 数 + 排除数 = 166）
- [ ] 判据结论明确（达标路径 / 判据修订请求），且**未**自行修改 objective 判据
- [ ] 交付物落在本 change 目录内（`design.md` 的盘点表 + `tasks.md` 勾选）

## 4. Capabilities (涉及的能力维度)

- `dual-backend-test-coverage` —— **可能 MODIFIED**（若判据需修订 ⇒ 把修订后的
  可达上限写进 capability 的 R6 说明）
- `objective-test-coverage-100pct` —— **判据修订属 planner 职责** ⇒ build 只**请求**，
  不自行修改

## 5. Impact (影响面分析)

- **改动文件**：**无代码改动** ⇒ 只有本 change 目录内的文档
- **计数**：**不变**
- **风险**：**低**（只读）—— 但**误判风险**高（若盘点偏保守 ⇒ 上限被低估 ⇒
  过早修订判据）⇒ MUST 逐条给理由，且**排除**方向要**保守**（拿不准的按「可 dual」记，
  把判断留给实施阶段）
