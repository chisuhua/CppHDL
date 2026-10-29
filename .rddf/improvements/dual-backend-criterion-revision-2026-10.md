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
- **Status**: ✅ **已裁决（2026-10-30，planner）** —— 裁决记录见 §6；
  落地见 `.rddf/roadmap/objectives/objective-test-coverage-100pct.md` §2 / §9.2 / §11
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

## 6. ✅ 裁决记录（2026-10-30，**planner**）

### 6.1 取舍集的选择

| 轴 | 选择 | 理由 |
|---|---|---|
| ① 改阈值（保持口径 A） | **部分采纳**（并入 ②） | 单独采纳①仍**奖励加标签** ⇒ 必须与②（换口径）组合才有意义 |
| ② **改度量单位**（数真 dual） | ✅ **采纳（主轴）** | 标签 ≠ parity ⇒ 旧口径把 9 个**单后端**契约锁定参考实现算成「双后端覆盖」⇒ **判据与被测物不是同一个东西** |
| ③ 新增有意义的 dual 测试文件 | ✅ **保留为 Phase 5 的正题** | 盘点只**证伪**了它的「**必要性**」，**未**证伪「可行性」⇒ 它仍是真正提升 parity 面的路径；**只是不再被「≥100 这个数」卡住** |
| ④ 放松 R6 排除清单 | ❌ **否决** | 那是**改 capability 契约**（67 个裸 context 条目改写为 component 包装形式）⇒ 属独立立项，**不**夹带在判据修订里 |

### 6.2 裁决结果（**口径 B**）

- **度量单位**：**双后端 parity 源文件数**（不是 ctest 标签条目数，也不是 Catch2 用例数）
- **定义**：测试源文件（`tests/**`、`examples/**`）中满足任一者：
  ① **剥注释后**含 `DUAL_BACKEND_TEST_PAIR(` 的**调用**（⚠️ 必须是**调用**——
  10 个契约锁定参考实现只在**注释**里提到该宏 ⇒ 子串匹配会误判）；
  ② ∈ hand-crafted 真 dual 白名单（当前 **1** 个：
  `examples/riscv-mini/tests/test_rv32i_soc_dual_parity.cpp`）
- **现状**：**29**（28 个 macro 文件 + 1 个 hand-crafted）—— 2026-10-30 实测
- **诚实保守上限**：**62**（29 + 33 个可转换的既有条目）
- **阈值**：**≥ 60**（margin **2**：吸收条目退役 / hand-crafted 被合并 / R6 新增类别判死）
- ⚠️ 阈值 MUST ≤ 诚实保守上限 ⇒ 本裁决满足（60 ≤ 62）

### 6.3 顺带的裁决

- **R6 候选 grep 的 65–99 bit 盲区**：**维持现状 + 明文登记为已知盲区**
  （实测：全仓真实代码里只有 **1** 处 >64-bit `ch_out<ch_uint<N>>`
  —— `tests/test_verilator_backend.cpp` 的 `ch_out<ch_uint<128>>` ⇒ 且该文件是
  **契约锁定**对象 ⇒ 当前零实际影响）⇒ ⇒ **不改 capability 契约**
- ⇒ ⇒ **`dual-backend-test-coverage` 零 MODIFIED** ⇒ ⇒ **不需要 OpenSpec change**
  （判据只写在 objective 文件里 ⇒ 那是 planner 的产物）
- ⇒ ⇒ **机械锚点**：根 `AGENTS.md` 的 `DUAL_BACKEND_PARITY_TOTAL` 行 +
  `scripts/verify_test_counts.sh` 的对应检查（防「双后端覆盖**静默缩面**」）

### 6.4 机械锚点与护栏（判据 MUST 可机械核验）

| 锚点 | 值 | 口径 |
|---|---|---|
| `DUAL_BACKEND_PARITY_TOTAL` | **29** | **广度**：parity **源文件**数 |
| `DUAL_BACKEND_PARITY_PAIRS` | **184** | **深度**：真 dual 的 `DUAL_BACKEND_TEST_PAIR(` **调用**总数 |

⇒ 护栏 `scripts/verify_dual_backend_parity.sh`（分析器 `scripts/lib/dual_backend_parity.py`
⇒ 与 **12** 项 fixture 自检**共用同一份** ⇒ 不会漂移）⇒ CI **2 个 job × 主路径 + self-test**。

⚠️ **为什么两个锚点都要**（实测得来的）：只钉「文件数」的话，**从有多个 dual pair
的文件里删掉 1 个 pair** ⇒ 该文件仍被计数 ⇒ **护栏看不见**（变异 M1 第一轮**恒绿**）
⇒ ⇒ 深度锚点补上这个盲区。

⚠️ **护栏的交叉核验**（用 **CMake 自己生成的** `build.make` 做 target→源文件映射
⇒ 没有需要维护的语法 ⇒ 不会「静默漏解析」）：
- 每个 parity 源文件 MUST 被**至少一个**带 `verilator_e2e` 标签的 ctest 条目构建
  （否则「该覆盖**不会真的被跑**」）；
- 带标签但**背后没有** parity 的条目 MUST 属 capability 的**契约锁定清单**
  （否则**旧口径**会把它误算成「双后端覆盖」—— 这正是本次修订要消灭的漏洞）；
- 契约锁定文件 MUST NOT 带 parity 证据（capability：MUST NOT 迁到 macro）；
- 契约锁定清单**无法解析** ⇒ **fail-closed**（不得当作「空清单」）。

### 6.5 护栏的判别力（变异 **4/4 全被抓**）

| 变异 | 注入 | 期望 | 实测 |
|---|---|---|---|
| M1 | 把一个真 dual 的 macro **调用**改成注释 | 深度锚点漂移 | ✅ `PAIRS=184 → 183` |
| M2 | 把 `DUAL_BACKEND_PARITY_TOTAL` 改成 30 | 广度锚点漂移 | ✅ `TOTAL=29 ≠ 30` |
| M3 | 给一个**非** dual 的 `base` 条目加 `verilator_e2e` 标签（+ 重新 configure） | 抓到「带标签但无 parity」 | ✅ `unclassified-labeled-entry` |
| M4 | 把一个**契约锁定**的参考实现改成真 dual | 抓到违反 capability | ✅ `contract-locked-parity` |

⚠️ **M1 第一轮恒绿** ⇒ 定位到「文件数口径对**深度**不敏感」⇒ ⇒ 补**深度锚点**
⇒ ⇒ 转红（这就是「恒绿比转红信息量更大」的又一例）。

### 6.6 验收对照（本草案 §3 的逐条关闭）

| §3 验收项 | 状态 |
|---|---|
| planner 显式选择取舍集方向并记录理由 | ✅ §6.1 |
| 阈值 ≤ 对应口径的诚实保守上限 | ✅ 60 ≤ 62（§6.2） |
| objective 的 `37` 的 **4 处**被处理（2 处历史记录未改写） | ✅ L25/L26 历史快照链**原样保留**（并加现状注）· L37 单位陈述**已改为新口径** · L45 done-when**已改为新判据** · L72 Go 判据**已改为新判据** · L93 历史台账行**已加注**（未改写） |
| R6 候选 grep 已收紧 **或** 明文记录「维持现状 + 已知盲区」 | ✅ §6.3（后者） |
| 判据修订后实测值与新判据同单位 | ✅ 现状 29（源文件）/ 上限 62（源文件）—— 同单位；锚点 `DUAL_BACKEND_PARITY_TOTAL` |
| 若改的是 capability ⇒ 走 OpenSpec change 流程 | ✅ **未改 capability** ⇒ 不适用 |
