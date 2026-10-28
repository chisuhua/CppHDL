# 提案：T7（Phase 5 全量 dual）开工前 MUST 先交「可 dual 上限盘点」

- **架构依据**：无（只读盘点）
- **5 段草案**：`.rddf/improvements/dual-backend-full-coverage-inventory-2026-10.md`
- **roadmap 归属**：`feat-dual-backend-coverage` Phase 5 + `objective-test-coverage-100pct`
- **规格影响**：**可能**（若判据需修订 ⇒ `dual-backend-test-coverage` MODIFIED；
  objective 判据属 **planner** 职责）⇒ 默认 `--skip-specs`，实施时判定
- **规模 / 风险**：**低**（无代码改动）—— 但**结论**影响后续**数个批次**的工作量

## Why

`objective-test-coverage-100pct` 的 Go 判据第 2 条是「`ctest -L verilator_e2e` ≥ 100」
⇒ 现状 **38**（2026-10-28 实测）⇒ 差 **62**。
（2026-10-27 记为 37 ⇒ 2026-10-28 `riscv-mini-cache-wrapper-live-logic-2026-10`
给 `riscv_mini_cache_smoke` 加了 `verilator_e2e` 标签 ⇒ 37 → **38**。）

### ⚠️ 口径桥接：判据数的是 **ctest 条目**，不是「测试源文件」

初稿把「166 个测试源文件」与判据「`ctest -L verilator_e2e` ≥ 100」直接对比 ⇒
**两者不是同一个量** ⇒ 盘点表若以「文件」为行，算出的上限**与判据不可比**。

实测（2026-10-28）：

| 事实 | 实测值 | 含义 |
|---|---|---|
| `ctest -N -L verilator_e2e` | **38** 条 | 判据数的**是 ctest 条目**（= 带该标签的 target） |
| 含 `DUAL_BACKEND_TEST_PAIR` 的 `.cpp` 文件 | **38** 个 | ⚠️ 与上者**数值巧合**，**集合不同** |
| 带标签但**不含** dual pair 的条目 | **18** 个 | `test_ch_stream` / `test_uint_compare` / `test_module` / `test_verilator_*` 等 ⇒ **单后端** verilator e2e |

⇒ 「38 = 38」是**巧合**（当前 dual 文件数恰好 == 标签条目数），**不是恒等式** ⇒
不可用它当「判据单位 == 文件数」的依据。

⇒ 且存在**一文件 ≠ 一条目**的形态：
- `tests/test_csr_unit` 编译的是 `examples/riscv-mini/tests/test_csr_unit.cpp`
  （根 `AGENTS.md §NOTES` 登记的「库 → example 反向耦合第三种形态」）
  ⇒ **1 个文件**可能产出 **0 或 2** 个条目；
- `add_catch_test(name src1 src2 …)` 允许**多文件一个 target** ⇒ **N 个文件 = 1 个条目**。

⇒ ⇒ **盘点表 MUST 以「ctest 条目（target）」为行**，并附「文件 → 条目」的映射
（哪些文件各自注册成独立 target / 哪些被合并进同一 target / 哪些被注册两次）
⇒ 否则算出的上限**不可与判据比较**。

⚠️ 但**判据本身可能不可达**：`dual-backend-test-coverage` 的 R6 **排除清单**规定了
**5 类**不可 dual 的测试（纯编译期 / 裸 context / >64-bit 端口 / SECTION 重入 /
STATIC_REQUIRE）⇒ 在 **166** 个测试源（117 根 + 29 chlib + 3 jit + 17 examples）
里排除掉不可 dual 的部分后，**上限是未知数**。

⇒ 盲目开工 ⇒ 可能做到一半才发现判据不可达 ⇒ 已投入的批次变成「为凑数而做」。
⇒ plan 的 gap C 已明令：**先量化，若上限 < 100 ⇒ MUST 修订判据（由 planner），不得硬凑**。

## What Changes（**只读，不改代码**）

1. **盘点表**：以 **ctest 条目（target）为行** ⇒ 逐个判定「可 dual / 不可 dual」
   + **理由**（逐条引用 R6 排除清单的哪一项，或给出「可 dual」的具体障碍）
   + 附「文件 → 条目」映射；
2. **可达上限估算**：可 dual 条目总数 + 按「每批 ≤ 10 文件」折算的**批次数** + 估时；
3. **判据结论**：
   - 上限 ≥ 100 ⇒ 给出达标路径（批次划分 + 顺序建议）；
   - 上限 < 100 ⇒ 产出「**判据修订请求**」（建议值 + 依据）⇒ **交 planner 裁决**
     （⚠️ **build 不得自行修改 objective 的 Go 判据**）。

⚠️ **纪律**：盘点 MUST 用**可核**形式（每行给 `文件:行号` + 判定 + 理由）⇒
不得给「大概 XX 个」这种不可核的数字。

⚠️ **「排除方向要保守」与「上限估算」的用途冲突**（初稿的这条纪律会让结论**偏乐观**）：
「拿不准的按『可 dual』记」⇒ 上限被**高估** ⇒ 而本盘点的目的正是回答
「上限够不够 100」⇒ 高估 ⇒ 「够」可能是假的。
⇒ ⇒ 盘点 MUST 给**双侧估算**：
**乐观上限**（拿不准的按可 dual）/ **保守上限**（拿不准的按不可 dual）
⇒ 结论 MUST 以**区间**表述（`上限 ∈ [保守, 乐观]`）⇒
「乐观 ≥ 100 但保守 < 100」本身就是**需要 planner 裁决**的结论
（判据是否按乐观口径计 ⇒ 那是判据语义问题 ⇒ planner 职责）。

## Acceptance

- [ ] 盘点表覆盖**全部测试条目**（行 = ctest 条目）⇒ 「可 dual + 排除 = 总条目数」
      ⇒ **行数守恒**（⚠️ 口径 = **条目**，不是文件 ⇒ 文件数 **166** 只作参考）
- [ ] 每行给 `target:文件:行号` + 判定 + **理由**（引用 R6 的哪一条）
- [ ] 附「文件 → 条目」映射（标出 1 文件多条目 / 多文件一条目的形态）
- [ ] **双侧上限**（乐观 / 保守）都给出 ⇒ 结论以**区间**表述
- [ ] 上限数字与盘点表**自洽**
- [ ] 判据结论明确（达标路径 / 判据修订请求），且**未**自行修改 objective 判据
- [ ] **无代码改动**（交付物在本 change 目录内）

## Capabilities

- `dual-backend-test-coverage` —— **可能 MODIFIED ×1**（若判据需修订 ⇒ 把修订后的
  可达上限写进 capability 的 R6 说明）
- `objective-test-coverage-100pct` —— **判据修订属 planner 职责** ⇒ build 只**请求**
  ⇒ ⚠️ **该 objective 的 `37` 出现在 3 处**（§1 的 2026-10-26 状态更新、
  §2 done-when 的第 3 条、§9.2 Go 判据第 2 条）⇒ 现值 **38**
  ⇒ **请求 planner 一并更新这 3 处**（build **不得**自行改）

## Impact

- **计数**：**不变**
- **爆炸半径**：**零**（只读）
- **价值**：把「工程量问题」变成**有答案的问题** ⇒ 决定后续数个批次是否值得做

## Non-Goals

- **不**实施任何 dual 化（那是 Phase 5 的后续批次）
- **不**修改 objective 的 Go 判据（planner 职责）
- 不改 capability（除非 planner 裁决判据修订）
