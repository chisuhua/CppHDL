# 提案：T7（Phase 5 全量 dual）开工前 MUST 先交「可 dual 上限盘点」

- **架构依据**：无（只读盘点）
- **5 段草案**：`.rddf/improvements/dual-backend-full-coverage-inventory-2026-10.md`
- **roadmap 归属**：`feat-dual-backend-coverage` Phase 5 + `objective-test-coverage-100pct`
- **规格影响**：**可能**（若判据需修订 ⇒ `dual-backend-test-coverage` MODIFIED；
  objective 判据属 **planner** 职责）⇒ 默认 `--skip-specs`，实施时判定
- **规模 / 风险**：**低**（无代码改动）—— 但**结论**影响后续**数个批次**的工作量

## Why

`objective-test-coverage-100pct` 的 Go 判据第 2 条是「`ctest -L verilator_e2e` ≥ 100」
⇒ 现状 **37**（2026-10-27 实测）⇒ 差 **63**。

⚠️ 但**判据本身可能不可达**：`dual-backend-test-coverage` 的 R6 **排除清单**规定了
**5 类**不可 dual 的测试（纯编译期 / 裸 context / >64-bit 端口 / SECTION 重入 /
STATIC_REQUIRE）⇒ 在 **166** 个测试源（117 根 + 29 chlib + 3 jit + 17 examples）
里排除掉不可 dual 的部分后，**上限是未知数**。

⇒ 盲目开工 ⇒ 可能做到一半才发现判据不可达 ⇒ 已投入的批次变成「为凑数而做」。
⇒ plan 的 gap C 已明令：**先量化，若上限 < 100 ⇒ MUST 修订判据（由 planner），不得硬凑**。

## What Changes（**只读，不改代码**）

1. **盘点表**：166 个测试源逐个判定 ⇒「可 dual / 不可 dual」+ **理由**
   （逐条引用 R6 排除清单的哪一项，或给出「可 dual」的具体障碍）；
2. **可达上限估算**：可 dual 总数 + 按「每批 ≤ 10 文件」折算的**批次数** + 估时；
3. **判据结论**：
   - 上限 ≥ 100 ⇒ 给出达标路径（批次划分 + 顺序建议）；
   - 上限 < 100 ⇒ 产出「**判据修订请求**」（建议值 + 依据）⇒ **交 planner 裁决**
     （⚠️ **build 不得自行修改 objective 的 Go 判据**）。

⚠️ **纪律**：盘点 MUST 用**可核**形式（每行给 `文件:行号` + 判定 + 理由）⇒
不得给「大概 XX 个」这种不可核的数字；**排除**方向要**保守**（拿不准的按「可 dual」记）。

## Acceptance

- [ ] 盘点表覆盖**全部 166** 个测试源（可 dual + 排除 = 166 ⇒ **行数守恒**）
- [ ] 每行给 `文件:行号` + 判定 + **理由**（引用 R6 的哪一条）
- [ ] 上限数字与盘点表**自洽**
- [ ] 判据结论明确（达标路径 / 判据修订请求），且**未**自行修改 objective 判据
- [ ] **无代码改动**（交付物在本 change 目录内）

## Capabilities

- `dual-backend-test-coverage` —— **可能 MODIFIED ×1**（若判据需修订 ⇒ 把修订后的
  可达上限写进 capability 的 R6 说明）
- `objective-test-coverage-100pct` —— **判据修订属 planner 职责** ⇒ build 只**请求**

## Impact

- **计数**：**不变**
- **爆炸半径**：**零**（只读）
- **价值**：把「工程量问题」变成**有答案的问题** ⇒ 决定后续数个批次是否值得做

## Non-Goals

- **不**实施任何 dual 化（那是 Phase 5 的后续批次）
- **不**修改 objective 的 Go 判据（planner 职责）
- 不改 capability（除非 planner 裁决判据修订）
