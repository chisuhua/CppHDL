---
name: dual-backend-criterion-revision-2026-10
priority: P1
source: dual-backend-full-coverage-inventory-2026-10（只读盘点实测推翻 3 处前提）
phase: phase-5
category: testing
type: chore
created_at: 2026-10-28
---

# Improvement: dual 覆盖率判据 MUST 与实测语料一致（单位 / 阈值 / 取舍）

- **Date drafted**: 2026-10-28
- **Drafted by**: build-agent (builder role)
- **Status**: 📋 5-segment draft，**决策权归 planner**（build 只登记，不定方案）
- **来源**：`dual-backend-full-coverage-inventory-2026-10` 的只读盘点（172 行条目表）
  ⇒ 交付物 = `openspec/changes/archive/2026-10-28-dual-backend-full-coverage-inventory-2026-10/design.md`
  §9（**取舍集与候选阈值**）

## 1. Why (背景与动机)

`objective-test-coverage-100pct` 的 Go 判据第 2 条是
「`ctest -L verilator_e2e` ≥ 100」⇒ 现状 **38**（2026-10-28 实测）⇒ 差 **62**。

⚠️ **盘点实测结论**：该判据在**现有语料内不可达**（上限区间见 §2 的表）
⇒ ⇒ 继续盲做 ⇒ Phase 5 的批次变成「为凑数而做」（plan gap C 明令禁止）。

⚠️ **盘点同时实测推翻了 3 处既有前提**（⇒ 判据的**地基**本身有问题）：

| # | 既有说法 | 实测 | 影响 |
|---|---|---|---|
| ① | 「38 = 含 `DUAL_BACKEND_TEST_PAIR` 的 `.cpp` 文件数」 | 真**调用**只有 **28** 个（另 1 个 hand-crafted 真 dual、9 个契约锁定的单后端参考实现）⇒ **38 是标签条目数** | ⇒ 判据**奖励加标签**而不是**加 parity** |
| ② | 「在既有文件里加 dual pair **不会**推进本指标 ⇒ Phase 5 MUST 以**新文件**为主」 | **证伪**：把既有 `base` 条目的注册改成 `add_dual_backend_test` ⇒ 标签 +1 ⇒ 指标 +1 ⇒ 本次盘点的 **33** 个「可 dual」**全是既有文件** | ⇒ Phase 5 的**转换路线**不需要新建文件 ⇒ 工作量估算要改（`CTEST_TOTAL` / `EXAMPLES_TEST_SOURCES` 锚点**均不变**） |
| ③ | objective 里 `37` 出现在 **3** 处 | 实测 **6** 处（4 处需处理 / 2 处是历史记录） | ⇒ 需分类处理，不能一刀切替换 |

⇒ ⇒ **判据的「度量单位」比阈值本身更根本** ⇒ 先定单位，再定阈值。

## 2. What Changes (变更范围)

⚠️ **方案未定**（planner 裁决）。取舍集共 **4 个轴**（完整表述见交付物 §9.2.2）：

| # | 方向 | 上限影响 | 备注 |
|---|---|---|---|
| ① | **改阈值**（保持口径 A） | 阈值 ≤ **71** ⇒ 立即可达 | ⚠️ 仍奖励加标签 |
| ② | **改度量单位**（改数「真 dual pair 数」） | 阈值 ≤ **62** ⇒ 立即可达 | ⭐ **推荐**（奖励 parity） |
| ③ | **新增有意义的 dual 测试文件** | 理论上可到 ≥ 100 | ⚠️ 本盘点**未覆盖**（盘点的是现有条目）⇒ 工作量**另做**盘点 |
| ④ | **放松 R6 排除清单**（67 个裸 context 条目改写为 component 包装形式） | 可显著抬高 | ⚠️ 那是**改契约** ⇒ 需显式裁决 |

**候选阈值**（仅当选 ① 或 ② 时适用；margin 理由见交付物 §9.2.3）：

| 候选 | 口径 | 诚实上限（保守 / 乐观） | 建议阈值 | margin |
|---|---|---|---|---|
| **B（推荐）** | 真 dual pair 数 | 62 / 87 | **≥ 60** | 留 **2** 条余量（吸收条目退役 / hand-crafted 条目被合并） |
| A（现状） | 标签条目数 | 71 / 96 | **≥ 70** | 留 **1** 条余量（口径 A 本身高估了可达空间 ⇒ 余量须更紧） |

⇒ 两条**硬约束**：
- 阈值 MUST ≤ 对应口径的**诚实保守上限** ⇒ 否则就是「硬凑」的判据；
- 若保留 ≥ 100 ⇒ **没有诚实路径**（口径 A 的技术上限无界但全是「加标签」的不诚实路径；
  诚实路径只剩轴 ③ ⇒ 工作量另计）⇒ 需要显式记录该取舍。

**顺带**（同属 planner 职责，可一并处理）：
- objective 文件的 `37` 共 **6** 处 ⇒ **4 处需处理**（L37 单位陈述 / L45 done-when /
  L72 Go 判据 / L93 历史表加注）+ **2 处是历史记录**（L25/L26 ⇒ 不改）
  ⇒ 逐处清单见交付物 §9.3；
- `dual-backend-test-coverage` 的 R6 候选 grep 盲区：
  `ch_out<ch_uint<[0-9]\{3,\}>>` 只匹配 **≥100 位** ⇒ **65–99 位**漏网
  ⇒ 建议收紧为 `[0-9]\{2,3\}` 或直接判 `N > 64`
  （实测：全仓真实代码里只有 1 处 >64-bit 端口，且在**契约锁定**的文件里 ⇒ 当前无实际影响）。

## 3. Acceptance (验收标准)

- [ ] planner **显式选择**取舍集里的方向（①/②/③/④，或组合）并**记录理由**
- [ ] 若选 ① 或 ② ⇒ 阈值 MUST ≤ 对应口径的诚实保守上限（① ≤ 71 / ② ≤ 62）
- [ ] objective 的 `37` 的 **4 处**被处理（数值 → 38；单位陈述 → 「带 `verilator_e2e`
      标签的 ctest 条目数」）；**2 处历史记录**未被改写
- [ ] `dual-backend-test-coverage` 的 R6 候选 grep 已收紧（或明文记录「维持现状 +
      已知盲区」）
- [ ] 判据修订后，`ctest -N -L verilator_e2e` 的实测值与新判据**同单位**
      （⇒ 可用 `scripts/verify_test_counts.sh` 的 `VERILATOR_E2E_TOTAL` 锚点核对）
- [ ] 若改的是 capability ⇒ 走 OpenSpec change 流程（**禁**直接改主 spec）

## 4. Capabilities (涉及的能力维度)

- `dual-backend-test-coverage` —— **可能 MODIFIED**（若「判据的度量单位」要写进
  capability；当前 capability 只规定了 R6 排除清单，**未**规定判据怎么数）
  ⇒ **待实施时判定**
- `objective-test-coverage-100pct` —— **objective 文件属 planner 职责**
  ⇒ build 只**请求**，不自行修改

## 5. Impact (影响面分析)

- **改动文件**：`objective-test-coverage-100pct.md`（planner 改）+ 可能
  `openspec/specs/dual-backend-test-coverage/spec.md`（走 change 流程）
- **计数**：**不变**（本 change 只读；判据修订本身也不改 ctest 条目数）
- **风险**：**中** —— 放宽判据 = 削弱唯一的覆盖率验收闸门 ⇒ MUST 显式披露
  （交付物已给出「诚实上限」的口径 ⇒ 判据与上限的关系可被复核）
- **下游**：Phase 5 的**推进方式**取决于选轴 ② 还是 ③ ⇒ 选轴 ② ⇒ 改既有条目的注册
  （不新增测试文件）；选轴 ③ ⇒ 需另做「新增 dual 文件的内容规划」盘点
