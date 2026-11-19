# 审稿记录 —— 文档治理口径（`docs/architecture/2026-11-19-document-governance.md`）

- **日期**：2026-11-19 · **审查者**：oracle（第 1 轮）· **作者**：architect
- **结论**：**APPROVE**（附 **6 项必改** ⇒ 全部已落进方案文档）
- **审稿输入**：`docs/architecture/2026-11-19-document-governance.md`（v0.1）
- **关联**：仓规「一文档一决策」· `docs/architecture/README.md` §3 判读纪律

---

## 一、oracle 第 1 轮的 6 项必改与处置

| # | finding | 处置 |
|---|---|---|
| **Q1** | 「一文档一决策」的适用域被混用（该原则的仓内实证只在 ADR 语境）；**D5**（规范校准）才是真正游离的独立决策 | ✅ §3 开头加**决策同一性声明**（D1–D4 共享选项空间 ⇒ 非独立决策点）· **D5 降为派生实施裁决**并移入 §7.3 |
| **Q2** | D2 的例外条款方向对但**不够可判**：①「决策依据」无机械判据 ②允许在 ADR **行内**追落 ③未规定追落位置 | ✅ 三处收窄：**可判的操作判据**（章节标题 ∈ {实现·代码位置·依据} 或引文含「见/位于/体现」）· **追落位置 = 文末 Addendum 节**（正文一行不动）· **明示人工裁决、护栏不执行**；另补**正向模式**「L1 负责刷新 pin · ADR 保留历史 pin」 |
| **Q3** | D1 清单是新的手工清单 ⇒ 会有棘轮风险；反向判据只对「已有死链」有效，**新 L1 文档零死链未登记 ⇒ 零信号** | ✅ D1′ 改**派生式登记表**（`--render` 生成 · 禁手改 · 形态对齐 `guard-reachability.md`）+ **目录前缀默认**（零维护）+ 仅混合目录逐文件登记；新增 **C5**（`docs/**` 下任一 `.md` 无法归级 ⇒ 红） |
| **Q4** | D3 表内**自相矛盾**（L2「不追」vs「任何链接 MUST 修」）；绝对路径不是死链问题，是**可移植性**问题 | ✅ D3 拆出**独立条款 P1**（级别无关）；L1 违规 ⇒ 改相对路径 · L2/L3 违规 ⇒ **delink**（不得 relink 到不存在的目标）· C3 口径改为「全仓扫 · 级别无关」 |
| **Q5** | 漏面：`docs/simulation/**` 四级都不落 · `docs/freertos/**` 未列 · 仓根散落 `*.md` 未裁决 · `AGENTS.md` 家族的排除是**隐含**的 | ✅ D1′ 补齐前缀规则 + 仓根逐条裁决（`improvement-*` = **L1 活索引**，经核实非 legacy 镜像）· D1″ 新增**显式 out-of-scope**（`AGENTS.md` 家族：`file:line` 密度全仓最高 ⇒ 另立独立小项） |
| **Q6** | V13「登记为残留」可行但**次优** —— 裁决本身就能现在给出 | ✅ V13 改「**已裁决待执行**」：ADR 7 处 = **4 处修链 + 3 处 delink**，ADR 正文一律不改 |
| **Q7** | 实施范围三处措辞问题 + 一处遗漏 | ✅ §8.1 改「**如实标注**矛盾，不改写任何签署行」· 规范校准加**版本 bump + 变更记录**（v2.0 → v2.1）· 新增 **V14**（分级覆盖率）· item 3 补「**删前 MUST 逐条留档**到实施记录」 |

---

## 二、事实基线的证据明细（方案 §2 的全量数据）

| 扫描项 | 实测 | 结论 |
|---|---|---|
| Markdown 链接 | 314 条 | **27 处死链**在 6 个**活文档导航**里（`docs/README.md` · `README.md` · `READING-GUIDE.md` · `QUICK_REFERENCE.md` · `docs/PROJECT-OVERVIEW.md` · `docs/developer_guide/README.md`）⇒ ⛔ 缺陷 |
| 其余死链 | 34 处 | `phases/**` 21（历史计划）· `docs/adr/` 7（决策正本）· `docs/architecture/` 2（**本机绝对路径**）· `external/` 4（第三方）⇒ 非本方案实施面（§7.4） |
| `file:line` 引用 | 4076 处 | 失效 **357 处** ⇒ 其中 **330 处属历史快照**（ADR 71 · reviews 59 · improvements 53 · openspec/archive 11 · 扫描器假阳性 ~150）⇒ **不是缺陷**；真正属活文档的仅 **14 处**（`usage_guide/05`，已在 `a4127e9` 修完 ⇒ 当前 0） |
| 规范自身 | `docs/DOCUMENT-ARCHITECTURE.md` | §2.1 目录树列的 **21 个文件不存在** · §6.1 统计停在「0 篇」· §5.1 的「CI 检查」**无实现**（仓内无任何文档类护栏）· §8.1「已批准」与「待批准」**自相矛盾** ⇒ ⇒ **它是当前态的误导源** |
| 既有口径 | `docs/architecture/README.md` §2/§3 | 已有「历史快照按日期读」「dated addendum 追落」「新增主题文档 MUST 登记」⇒ ⇒ 本方案**扩展**其适用范围，**不自立门户** |

⇒ ⇒ **「引用失效」这个信号 MUST 先分类再判定** ⇒ 否则每次扫仓都产出几百条**伪缺陷** ⇒ ⇒ 治理口径的第一条就是**分级**。

### 死链逐条（活文档 27 处 · 实施时 MUST 逐条留档）

| 文件 | 处数 | 典型死链 | 正确目标 |
|---|---|---|---|
| `READING-GUIDE.md` | 9 | `docs/Bundle_UsageGuide.md` · `docs/CppHDL_UsageGuide.md` · `docs/Bundle_DeveloperGuide.md` · `docs/problem-reports/bundle-connection-issue.md` | `docs/usage_guide/{05,06,11}-*.md` · `docs/developer_guide/tech-reports/bundle-connection-issue.md` |
| `docs/README.md` | 7 | `CppHDL_Testing_Guide.md` · `Bundle_DeveloperGuide.md` · `CppHDL_UsageGuide.md` · `CppHDL_vs_SpinalHDL_Stream_Flow_Usage.md` · `developer_guide/contributing.md` · `developer_guide/tech-reports/T001-analysis.md` | `docs/usage_guide/{05,06,09,11}-*.md` · `docs/developer_guide/README.md` · `docs/developer_guide/api-reference/T001-simulator-api.md` |
| `docs/developer_guide/README.md` | 5 | `architecture/overview.md` · `architecture/simulator-internals.md` · `architecture/node-lifecycle.md` · `tech-reports/T001-analysis.md` · `contributing.md` | 前三项**该文件已不存在** ⇒ 改指 `architecture/verilog-codegen-architecture.md` 或删该行 |
| `QUICK_REFERENCE.md` | 3 | `Bundle_UsageGuide.md` · `CppHDL_UsageGuide.md` · `docs/problem-reports/bundle-connection-issue.md` | 同上 |
| `docs/PROJECT-OVERVIEW.md` | 3 | `QUICK_REFERENCE.md` · `Compare.md` · `docs/` | **相对路径错**（前两者在**仓根**） |

### ADR 7 处死链的 successor 核实（方案 §7.4 的逐条依据）

| ADR | 死链 | successor | 处置 |
|---|---|---|---|
| ADR-001 | `../2026-03-30-cpphdl-architecture-gap-analysis.md` | `docs/architecture/2026-03-30-cpphdl-architecture-gap-analysis.md` | **修链** + 文末 Addendum |
| ADR-001 | `../plans/phase0-tasks.md` | **无** | **delink** |
| ADR-001 | `../2026-03-30-spinalhdl-port-architecture.md` | `docs/architecture/2026-03-30-spinalhdl-port-architecture.md` | **修链** + 文末 Addendum |
| ADR-002 | `bundle-connection-fix-plan.md` | `docs/plans/bundle-connection-fix-plan.md` | **修链** + 文末 Addendum |
| ADR-002 | `bundle-connection-issue.md` | `docs/developer_guide/tech-reports/bundle-connection-issue.md`（活）· `docs/archive/problem-reports/…`（归档） | **修链**（指向活的 tech-report）+ 文末 Addendum |
| ADR-003 | `../architecture-review-report.md` | **无** | **delink** |
| ADR-006 | `prd.md` | **无** | **delink** |

---

## 三、方案自身的形式复核（写入后自检）

| 检查 | 结果 |
|---|---|
| 章节序列 | `§0 元数据` → `§1 意图` → `§2 事实基线` → `§3 决策` → `§4 选项对比` → `§5 后果` → `§6 Verification` → `§7 实施范围` → `§8 关联文档` ✓ |
| 表格竖线配平 | 全部表格块内竖线数一致 ✓ |
| 代码块 fence 配平 | 偶数 ✓ |
| oracle 6 项必改 | **19 项可点数判据全部落实**（逐条脚本核对）✓ |
| 正文行数 | ≤ 250 ✓（证据已移入本记录） |
