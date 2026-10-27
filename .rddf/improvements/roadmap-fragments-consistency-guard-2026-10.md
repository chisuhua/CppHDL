---
name: roadmap-fragments-consistency-guard-2026-10
priority: P0
source: 2026-10-27 实施 ISA ④ 时的实测漂移
phase: phase-5
category: tooling
type: tooling
created_at: 2026-10-27
---

# Improvement: roadmap 簿记 MUST 有自动一致性护栏（AUTO 块 + feature 状态）

- **Date drafted**: 2026-10-27
- **Drafted by**: build-agent (builder role)
- **Status**: 📋 5-segment draft，归属 `feat-dual-backend-coverage` 的**工程纪律**项
  （或独立 tooling 归口，取决于 roadmap 的 feature 划分）
- **来源**: 2026-10-27 实施 ISA ④ 期间**实测到的簿记漂移**（两处）：
  ① `feat-dual-backend-coverage.md` 的 phase-5 bullet 仍写「🟡 Phase 4 partial
  （Batch 2/3 follow-up）」⇒ 而同文件的表格行早已 ✅；
  ② `feat-rv32i-integration.md` 的 phase-5 bullet 同样 🟡 + 过时的「Batch 2 需要
  clk-spike」说法（该风险早已被读码推翻）⇒ 两处都**靠人工发现**。

## 1. Why (背景与动机)

⚠️ **根因是工具链缺一环**：`rddf roadmap --update-agent-md` **在本仓不可用** ——
CLI 的 flag 存在，但**脚本 payload 缺失**
（`skills/roadmap/scripts/roadmap_update_agent_md.sh` 不存在）⇒ 根 `AGENTS.md` 的
AUTO 块只能**手工**维护 ⇒ 手工 = 会漂。

⇒ 而 roadmap 的 AUTO 块与 `.rddf/roadmap/features/*.md` 是**两份**真相
（一份在 AGENTS.md、一份在 feature 文件）⇒ **漂移无声**（没有护栏会红）。

## 2. What Changes (变更范围)

- 新增 `scripts/verify_roadmap_fragments.sh`：
  1. 解析根 `AGENTS.md` 的 AUTO 块（`<!-- AUTO: feature fragments start -->` 区间）
     的 `id | status | phase_refs | theme` 行；
  2. 解析 `.rddf/roadmap/features/*.md` 的 front-matter（`id` / `status` / `phase_refs`）；
  3. 比对 ⇒ **不一致即失败**（打印差异行 + 两边的取值）；
  4. 额外检查 **feature 文件内部**的「状态符号」一致性（表格行 vs phase 拆分 bullet）：
     ⚠️ 只在**同一 change 名**同时出现在两处时比对 ⇒ 避免语义不对齐造成误报；
- **`--self-test` 自测**（**必须有**）：构造 3–4 个 fixture（一致 / id 不符 /
  status 不符 / phase_refs 不符）⇒ 断言每种不一致都能被抓 ⇒ 与既有护栏
  （`verify_perf_baseline_anchors.sh` 的 15 项自测、`verify_no_duplicate_rv32i_headers.sh`
  的 44 项）同一纪律；
- 挂进 CI（与 `verify_test_counts.sh` 同一 job）。

## 3. Acceptance (验收标准)

- [ ] 护栏能抓到本次实测的**两处真实漂移**（用 `git show` 的旧版本做 fixture）
- [ ] `--self-test` 全部通过（每条不一致各一个 fixture）
- [ ] 护栏在**当前仓库**上 exit 0（不得误报）
- [ ] 挂进 CI 且 CI 绿

## 4. Capabilities (涉及的能力维度)

- **不涉及** ⇒ 本 change **无 spec 变更** ⇒ 归档用 `--skip-specs`

## 5. Impact (影响面分析)

- **改动文件**：`scripts/verify_roadmap_fragments.sh`（新增）+ `.github/workflows/ci.yml`
- **计数**：**不变**
- **风险**：**低**（只读脚本）—— 但**误报风险**存在（feature 文件的格式自由度高）
  ⇒ MUST 用 `--self-test` 钉住，且解析失败一律 **fail-closed**（解析不出来 ⇒ 失败，
  不许静默跳过）
