# 提案：roadmap 簿记 MUST 有自动一致性护栏

- **架构依据**：无
- **5 段草案**：`.rddf/improvements/roadmap-fragments-consistency-guard-2026-10.md`
- **roadmap 归属**：工程纪律项（`feat-dual-backend-coverage` 或独立 tooling 归口）
- **规格影响**：**无** ⇒ 归档用 `--skip-specs`
- **规模 / 风险**：**低**（只读脚本）—— 风险在**误报** ⇒ 用 `--self-test` 钉住

## Why

**实测漂移（2026-10-27 实施 ISA ④ 期间，两处）**：

1. `feat-dual-backend-coverage.md` 的 phase-5 bullet 仍写「🟡 Phase 4 partial
   （Batch 2/3 follow-up）」⇒ 而同文件的**表格行**早已 ✅；
2. `feat-rv32i-integration.md` 的 phase-5 bullet 同样 🟡 + 过时的「Batch 2 需要
   clk-spike」说法（该风险早已被读码推翻）⇒ **两处都靠人工发现**。

⚠️ **根因是工具链缺一环**：`rddf roadmap --update-agent-md` 在本仓**不可用** ——
CLI 的 flag 存在，但**脚本 payload 缺失**
（`skills/roadmap/scripts/roadmap_update_agent_md.sh` 不存在）⇒ 根 `AGENTS.md` 的
AUTO 块只能**手工**维护 ⇒ 手工 = 会漂。

⇒ 而 AUTO 块与 `.rddf/roadmap/features/*.md` 是**两份**真相 ⇒ 漂移**无声**。

## What Changes

1. 新增 `scripts/verify_roadmap_fragments.sh`：
   - 解析根 `AGENTS.md` 的 AUTO 块（`<!-- AUTO: feature fragments start -->` 区间）的
     `id | status | phase_refs | theme` 行；
   - 解析 `.rddf/roadmap/features/*.md` 的 front-matter（`id` / `status` / `phase_refs`）；
   - 比对 ⇒ **不一致即失败**（打印差异行 + 两边取值）；
   - 额外检查 **feature 文件内部**的状态符号一致性（表格行 vs phase 拆分 bullet）：
     ⚠️ 只在**同一 change 名**同时出现在两处时比对 ⇒ 避免语义不对齐造成误报；
2. **`--self-test` 自测**（**必须有**）：构造 3–4 个 fixture（一致 / id 不符 /
   status 不符 / phase_refs 不符）⇒ 断言每种不一致都能被抓 ⇒ 与既有护栏
   （`verify_perf_baseline_anchors.sh` 15 项 / `verify_no_duplicate_rv32i_headers.sh` 44 项）
   同一纪律；
3. 挂进 CI（与 `verify_test_counts.sh` 同一 job）。

## Acceptance

- [ ] 护栏能抓到本次实测的**两处真实漂移**（用 `git show` 的旧版本做 fixture）
- [ ] `--self-test` 全部通过（每种不一致各一个 fixture）
- [ ] 护栏在**当前仓库**上 exit 0（不得误报）
- [ ] 解析失败一律 **fail-closed**（解析不出来 ⇒ 失败，不许静默跳过）
- [ ] 挂进 CI 且 CI 绿

## Capabilities

- **不涉及** ⇒ 无 spec 变更 ⇒ 归档用 `--skip-specs`

## Impact

- **文件面**：`scripts/verify_roadmap_fragments.sh`（新增）+ `.github/workflows/ci.yml`
- **计数**：**不变**
- **风险**：**低**（只读脚本）；主要失败模式 = **误报** ⇒ 由 `--self-test` 与
  「只比对有共同 change 名」的限制兜住

## Non-Goals

- **不**实现 `rddf roadmap --update-agent-md` 的脚本 payload（工具侧职责）
- 不做 roadmap 的**编辑**功能（护栏只**检查**）
- 不检查 roadmap 之外的一致性（如 ADR ↔ spec 的引用）
