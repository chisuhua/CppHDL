# 提案：roadmap 簿记 MUST 有自动一致性护栏

- **架构依据**：无（不涉及架构决策 ⇒ **不需要 ADR**；仓内 3 个护栏 capability
  均无 ADR ⇒ 无先例）
- **5 段草案**：`.rddf/improvements/roadmap-fragments-consistency-guard-2026-10.md`
- **roadmap 归属**：工程纪律项（`feat-dual-backend-coverage` 或独立 tooling 归口）
- **规格影响**：**有** —— 新增 capability `roadmap-bookkeeping-consistency`
  （ADDED ×4）⇒ **不得**用 `--skip-specs`
  （⚠️ 初稿写「无 spec 变更」⇒ 与仓内惯例不符：仓内**每个 CI 护栏都有 capability
  契约** —— `example-assertion-quality` / `example-runtime-coverage` /
  `sample-regression-guard` ⇒ 护栏契约若只落在 proposal+design，归档后**无 spec 承载**
  ⇒ 后人可无声削弱或删除该闸门）
- **规模 / 风险**：**低**（只读脚本）—— 风险在**误报** ⇒ 用 `--self-test` 钉住

## Why

**实测漂移（2026-10-27 实施 ISA ④ 期间，两处）**：

1. `feat-dual-backend-coverage.md` 的 phase-5 bullet 仍写「🟡 Phase 4: RV32I partial」
   ⇒ 而同文件**验收标准**的对应 checkbox 早已 `[x] Phase 4 partial ship`；
2. `feat-rv32i-integration.md` 的 phase-5 bullet 同样 🟡 + 过时的「Batch 2 需要
   clk-spike」说法（该风险早已被读码推翻）⇒ **两处都靠人工发现**。

⚠️ **这两处漂移已被修复**（2026-10-28 实测）⇒ fixture MUST 从 **git 历史**取
（`git show <旧commit>:.rddf/roadmap/features/<file>`）⇒ **不能**指望在当前树上复现。

⚠️ **两处里只有一处可机械判定**（实测，2026-10-28）：

| 漂移 | 形态 | 可机械判定？ | 归属判据 |
|---|---|---|---|
| ① `feat-dual-backend-coverage` | phase bullet `🟡 Phase 4` vs 验收标准 `[x] Phase 4` ⇒ **符号**冲突 | ✅ **可** | **C2** |
| ② `feat-rv32i-integration` | bullet `🟡 <change>` vs checkbox `[ ] <change>` ⇒ **符号一致**，只是**内容**过时 | ❌ **不可** | 人工 + 代码评审 |

⇒ 第 ② 类**没有**可机械判定的对应物 ⇒ 护栏**如实声明判不了**（不假装覆盖）。

### ⚠️ 根因订正（2026-10-28 实测；初稿的陈述是**反的**）

初稿写：「`rddf roadmap --update-agent-md` 的 CLI flag **存在**，但**脚本 payload 缺失**
（`skills/roadmap/scripts/roadmap_update_agent_md.sh` 不存在）」⇒ **实测两头都反了**：

| 事实 | 实测结果 |
|---|---|
| 脚本 payload | **存在且可用** —— 工具链的 `skills/roadmap/scripts/roadmap_update_agent_md.sh`（1656 B，可执行）**存在** |
| 能否直接跑 | **能** —— 绕开 CLI dispatch 直接调用生成器 ⇒ 产出 8 个 feature fragment ⇒ 与当前 `AGENTS.md` 的 AUTO 块**逐字一致**（实测 diff 为空） |
| CLI dispatch | **缺** —— `rddf roadmap` 只暴露 `init / status / edit / validate / advance` ⇒ `--update-agent-md` **flag 不存在** ⇒ 脚本**不可达** |

⇒ ⇒ **真正的根因 = CLI 没把已有的脚本接出来**（工具侧职责），
**不是**「脚本缺失」⇒ Non-Goal 里「**不**实现脚本 payload」是基于**假前提**的措辞
⇒ 已订正（见 Non-Goals）。

⇒ **这个订正改变了护栏的设计**：既然生成器**可用**，护栏就**不必**自己重新实现
一套「解析已提交块 + 逐字段比对」的逻辑 ⇒ 应当照抄生成器的策略：
**渲染期望块 ⇒ 与已提交块逐字比对**（见 What Changes 1）。

## What Changes

1. 新增 `scripts/verify_roadmap_fragments.sh`，**三条**独立判据：
   - **C1（AUTO 块守恒）**：按**生成器的同一套渲染规则**从
     `.rddf/roadmap/**` 的 front-matter 渲染出期望的 AUTO 块
     ⇒ 与根 `AGENTS.md` 的 AUTO 块**逐字比对** ⇒ 不一致即失败（打印差异）。
     ⚠️ **为什么是「渲染 + 比对」而不是「双向解析 + 字段比对」**：前者只需
     **一套**解析器（front-matter）⇒ 误报面更小；后者要同时解析**已提交块**的
     Markdown 表格 ⇒ 多一类边界 ⇒ 多一类误报。
   - **C2（phase 标签的完成度一致性）** 与 **C3（change 名的完成度一致性）**：
     同一个 feature 文件的**三个比对面** —— `### phase-*` 段的 bullet /
     `## 验收标准` 的 checkbox / **正文里的 Markdown 表格行** ——
     若**都**提到**同一个键** ⇒ 完成度 MUST 一致。
     ⚠️ 键 = **行首**的 `Phase N` 标签，或**行首**的**已知 change 名**
     （已知 = `openspec/changes/**` 的目录名，含去日期前缀 / 去日期后缀的形态 ——
     ⚠️ **只收目录名会让该判据在真实语料上零命中** = 死判据）。
     ⚠️ **只在行首比对**（行中提及 MUST NOT 参与 ⇒ 否则误报）；
     同一面多符号 ⇒ **跳过**（无法判定 ≠ 违规）。
2. **一次性 oracle 等价性验证**（**本地**执行，不进 CI）：用工具链的生成器
   渲染出期望块，与本护栏 C1 的渲染结果**逐字比对** ⇒ 证明 vendored 渲染规则
   与工具**在该版本处**一致（工具链 SHA 记在 `design.md` §3）。
   ⚠️ **CI 拿不到工具链**（`rdd-workflow` 在**仓外**）⇒ 护栏 MUST **自包含**
   ⇒ oracle 只在**开发期**跑一次并把结论写进 `design.md`。
3. **`--self-test` 自测**（**必须有**）：构造 fixture（一致 / AUTO 块 status 不符 /
   AUTO 块缺行 / front-matter 重复键 / AUTO 哨兵缺失 / phase 标签符号冲突 /
   行首 vs 行中提及 / 同一面多符号 / change 名符号冲突 / 叙事性陈旧 /
   表格行符号冲突 / 表格单元格多符号 / fragments 目录缺失）
   ⇒ 断言每种不一致都能被抓 ⇒ 与既有护栏同一纪律。
   ⚠️ fixture 的变异 MUST 用**可移植**写法（`python3` heredoc）⇒ **不用** `sed -i`
   （那是 GNU-only ⇒ macOS 上静默不生效 ⇒ self-test 在 macos-14 腿必红）。
4. 挂进 CI（与 `verify_test_counts.sh` 同一 job）：**检查** + **`--self-test`** 各一步。

## Acceptance

- [ ] 护栏能抓到**从 git 历史取**的**可机械判定**的真实漂移
      （`feat-dual-backend-coverage` 的 phase-4 符号冲突 ⇒ **C2**）
- [ ] **另一处历史漂移（叙事性陈旧）如实登记为「判不了」** ⇒ 义务留在
      `AGENTS.md` + 代码评审（**不**假装覆盖）
- [ ] `--self-test` 全部通过（每种不一致各一个 fixture + 误报陷阱 + 软着陆）
- [ ] 护栏在**当前仓库**上 exit 0（不得误报）
- [ ] 解析失败一律 **fail-closed**（解析不出来 ⇒ 失败，不许静默跳过）；
      **环境性缺失**（fragments 目录不存在）⇒ 软着陆（warning + exit 0）
- [ ] **oracle 等价性**已验证：护栏渲染结果 == 工具链生成器输出（逐字一致），
      且**工具链 SHA 已记录**
- [ ] 护栏在 **BSD sed 平台**（macOS）上行为一致（实测：用 BSD sed 模拟器复现
      「`sed -i` 静默不生效」⇒ 修复后仍全绿）
- [ ] 挂进 CI 且 CI 绿

## Capabilities

- **新增** `roadmap-bookkeeping-consistency`（ADDED ×4）：AUTO 块守恒 /
  feature 文件内部的完成度一致 / fixture 规格与可移植性 / fail-closed 与软着陆边界
- ⇒ **不得**用 `--skip-specs`（仓内每个 CI 护栏都有 capability 契约）

## Impact

- **文件面**：`scripts/verify_roadmap_fragments.sh`（新增）+
  `.github/workflows/ci.yml` + 根 `AGENTS.md` + 新 capability spec
- **计数**：**不变**
- **风险**：**低**（只读脚本）；主要失败模式 = **误报** ⇒ 由 `--self-test`、
  「只比对行首」「同一面多符号跳过」「表格单元格多符号跳过」三条限制兜住

## Non-Goals

- **不**修改 `rdd-workflow` 工具链（CLI dispatch 缺 `--update-agent-md` flag ⇒
  **工具侧职责**；本仓只**消费**其渲染规则）
- **不**实现 `rddf roadmap --update-agent-md` 的**脚本 payload**
  —— ⚠️ **该 payload 已存在**（实测，见 Why 的订正表）⇒ 本条不是「不做」，
  而是「**不需要做**」（无缺失可补）
- 不做 roadmap 的**编辑**功能（护栏只**检查**）
- 不检查 roadmap 之外的一致性（如 ADR ↔ spec 的引用）
- **不**覆盖**叙事性**陈旧（判不了 ⇒ 人工 + 代码评审）
