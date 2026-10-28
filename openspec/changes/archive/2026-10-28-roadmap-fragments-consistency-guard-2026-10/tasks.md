# Tasks: roadmap-fragments-consistency-guard-2026-10

## 0. 准备
- [x] 0.1 读 `rdd-workflow` 工具链的生成器实现
      —— 抽取的渲染规则已写入 `design.md` §2.1（顺序 / `phase_refs` / `(none)` /
      `|` 转义 / 表头 / 尾注 / `kind` 缺省 = `phase` / `主题` 才是 theme）
- [x] 0.2 用 `git show` 的旧版本复现本次实测的**两处漂移**（做 fixture）
      —— ⚠️ **实测结果**：两处里**只有一处可机械判定**
      （`feat-dual-backend-coverage` 的 phase-4 符号冲突 ⇒ **C2** 抓到 ✓）；
      另一处（`feat-rv32i-integration` 的「Batch 2 需要 clk-spike」）是**叙事性**陈旧
      ⇒ 两侧符号**一致** ⇒ 任何符号判据都**判不出来** ⇒ 如实登记为人工项
      （design.md §4.3）
- [x] 0.3 确认 CLI dispatch 现状
      —— `rddf roadmap --help` 只暴露 `init / status / edit / validate / advance`
      ⇒ `--update-agent-md` **不存在** ⇒ 脚本**不可达** ⇒ 记入 design.md §1

## 1. 实现
- [x] 1.1 `scripts/verify_roadmap_fragments.sh` **C1**：按 0.1 的渲染规则从
      `.rddf/roadmap/**` 的 front-matter 渲染期望 AUTO 块
      ⇒ 与根 `AGENTS.md` 的 AUTO 块**逐字比对**
- [x] 1.2 **C2 / C3**：feature 文件**内部**的完成度一致性
      —— 三个比对面（`### phase-*` bullet / `## 验收标准` checkbox / **表格行**）
      —— 键 = 行首的 `Phase N` 标签 或 行首的**已知 change 名**
- [x] 1.3 解析失败一律 **fail-closed**（front-matter 损坏 / AUTO 哨兵缺失）
      ⇒ **环境性缺失**（fragments 目录不存在）⇒ 软着陆（warning + exit 0）
- [x] 1.4 `--self-test`：每种不一致各一个 fixture
      —— 实测 **13 个 fixture / 13 项断言**全绿

## 2. oracle 等价性（**本地**，不进 CI）
- [x] 2.1 用工具链的生成器（绕开 CLI dispatch 直接调用）渲染出期望块
- [x] 2.2 与本护栏 C1 的渲染结果**逐字比对** ⇒ MUST 一致
      —— 实测：**护栏渲染 == 工具生成 == 已提交块**（三者逐字一致）✓
      —— ⚠️ 工具链版本 `rdd-workflow@96af1629c8994fc079e100817c8540172c21226e`
      已记入 design.md §3 ⇒ 结论的边界是「**在该版本处**一致」
- [x] 2.3 ⚠️ **CI 拿不到工具链**（仓外）⇒ oracle **不得**进 CI；护栏 MUST 自包含

## 3. 验收
- [x] 3.1 当前仓库 exit 0（零误报）
- [x] 3.2 **从 git 历史取**的**可机械判定**的真实漂移被抓
      —— 实测：整体 checkout `3436ba4`（真实历史状态）⇒ **C1 绿**（AUTO 块当时一致）
      + **C2 红**（`feat-dual-backend-coverage` 的 phase-4 符号冲突）✓
- [x] 3.3 `--self-test` 全绿（13/13）
- [x] 3.4 挂进 CI（与 `verify_test_counts.sh` 同一 job）且 CI 绿
      —— 挂进 **2 处**：`build-and-test` + `pr-verilator-fast`（各含检查 + `--self-test`）
      ⇒ YAML 已用 `yaml.safe_load` 校验 ✓
- [x] 3.5 ⚠️ **BSD sed 平台行为一致**（审稿 BLOCKER 的闭合证据）
      —— 用 **BSD sed 模拟器**（`-i` 必须带后缀 ⇒ 文件不被改写）复跑：
      **修复后 13/13 全绿** ✓；含 `sed -i` 的旧版**转红** ✓（坐实 macos-14 腿会红）
- [x] 3.6 ⚠️ **change 名注册表键空间**（审稿 BLOCKER 的闭合证据）
      —— 修复后语料命中从 **0 → 25** ⇒ 该判据**不再**是死判据 ✓

## 4. 契约与文档
- [x] 4.1 根 `AGENTS.md` 记录「AUTO 块的一致性由本护栏守」
      —— 已加进 `## COMMANDS`（含「判不了叙事性陈旧」的如实声明）
      + ⚠️ **根因订正**（CLI flag 缺失 ≠ 脚本缺失）已写入 `design.md` §1
- [x] 4.2 ⚠️ 「C1 红了怎么办」的可操作动作写进 `AGENTS.md`
      —— 护栏打印的 unified diff 即**期望块** ⇒ 正确动作是**同步 vendored 规则**
      或修 front-matter ⇒ **不要**手工改 AUTO 块
- [x] 4.3 ⚠️ **新增 capability 契约**（审稿 SHOULD_FIX：仓内每个 CI 护栏都有）
      —— `specs/roadmap-bookkeeping-consistency/spec.md`（ADDED ×4）
      ⇒ `.openspec.yaml` 的 `skip_specs: true` **已删除**
      ⇒ 归档**不得**用 `--skip-specs`
- [x] 4.4 对抗审稿（critic，`review_profile: plan, build`）⇒ ≤ 2 轮
      —— **第 1 轮 = REJECT**（plan 1 BLOCKER + build 2 BLOCKER；另 7 SF + 5 NIT）
      ⇒ 逐条处置留痕见 `design.md` §7
- [x] 4.5 审稿处置（BLOCKER ⇒ 返工；SF / NIT ⇒ 同批闭合留痕）
      —— **3 个 BLOCKER 均已闭合**（判据集合/AC 不一致 · `sed -i` 不可移植 ·
      change 名注册表键空间）⇒ 均有**可复现的闭合证据**（§3.5 / §3.6 / design §7）

## 5. 归档
- [x] 5.1 `openspec archive roadmap-fragments-consistency-guard-2026-10 --yes`
      （**禁** `--skip-specs` ⇒ 主 spec 合并新增 capability）
      —— 已归档：`openspec/changes/archive/2026-10-28-roadmap-fragments-consistency-guard-2026-10/`
      ⇒ 主 spec `openspec/specs/roadmap-bookkeeping-consistency/spec.md`
      合并 **+4 added**（4 条 Requirement）⇒ **未**用 `--skip-specs` ✓
- [x] 5.2 主 spec 的 `## Purpose` **手工**补充（delta 的 Purpose 不合并）
      —— 已写：由来（手工维护 + 工具不可达）+ 三条要求 + **判不了的边界**
      + 与其它护栏 capability 的分工
