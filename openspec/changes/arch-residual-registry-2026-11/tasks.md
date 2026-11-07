## Tasks

- [x] ### Task 1: 分析器骨架 —— 路径解析 + 登记表/台账表解析 + **fail-closed**
- [x] ### Task 2: 规范句生成 + **前缀判定**（判据 ③）+ 生成句唯一（判据 ④c）
- [x] ### Task 3: 双向 + 多重性敏感 1:1 核对（判据 ①）+ 结算打印（判据 ④b）
- [x] ### Task 4: 证据存在性（判据 ②）+ 候选闭口证据（判据 ②'）
- [x] ### Task 5: 登记表格式校验（判据 ⑩）+ `check` 子命令 + 退出码语义（判据 ⑤）
- [x] ### Task 6: `--render` 子命令（判据 ⑦）
- [x] ### Task 7: `--check-normalization` 子命令（判据 ⑦'）
- [x] ### Task 8: `--self-test` 合成 fixture + **11 条投毒**（判据 ⑥）
- [x] ### Task 9: 护栏薄壳 `scripts/verify_arch_ledger_residuals.sh`
- [x] ### Task 10: 登记表首期 **15 行** + front-matter（含**生成句模板草案**）
- [x] ### Task 11: 探针改 **wrapper 转发**（判据 ⑨）
- [x] ### Task 12: 文档同步 + 全量护栏复验（★ **不接 CI**）

## S1 收尾（builder · 本批已完成）

- [x] **S1-1** 分析器 `scripts/lib/ledger_residual.py`（12 行判据 + 3 个只读子命令 + 11 投毒自检）
- [x] **S1-2** 护栏 `scripts/verify_arch_ledger_residuals.sh`（薄壳 · 退出码 0/1/2）
- [x] **S1-3** 登记表 `.rddf/checks/arch-residual-registry.md`（9 列 × 15 行 + front-matter）
- [x] **S1-4** 探针 `scripts/probe_arch_ledger_residuals.sh` 改 wrapper（★ 路径保留 ⇒ 证据链不断）
- [x] **S1-5** 文档同步（`AGENTS.md` COMMANDS 段 · ★ 未动 AUTO 块）
- [x] **S1-6** 全量护栏复验（自检 11 投毒全绿 · 台账证据 / roadmap / 语料 / 端口命名 / openspec validate 全 PASS · 自检零污染）

## S2（★ architect · 台账归一化 —— **builder MUST NOT 单方面改**）

- [x] **S2-1** 生成句模板复核（builder 草案：`✅ 已收口（<日期> · <后缀>）` / `✅ 已裁决（…）` / `不做 ⇒ <后缀>` / `⚠️ 已知 ⇒ <后缀>` / `⚠️ 未决 ⇒ <后缀>` ⇒ 无加粗 · 无反引号）
- [x] **S2-2** 台账 §7 **15 行状态格归一化**（用 `--render` 输出落笔；★ 删除「因本 change 而变假」的散文 ⇒ **逐条**登记进 architect review 记录）
- [x] **S2-3** §7 **前言注记去副本化**（只列同步了哪些行 id + 「状态以登记表为准」⇒ MUST NOT 逐字复述状态）
- [x] **S2-4** **行 14 归一化裁定**（builder 暂定 `已裁决` ⇒ architect 确认或改 ⇒ 改了 ⇒ 只改登记表那一行）

### S2 补充项（★ architect · oracle 1 轮审稿后追加）

- [x] **S2-5** 登记表 **8 行日期更正**（`日期` = 该状态在台账**首次落账**之日 ⇒ 非归一化日；逐行锚点见审稿记录 §2）
- [x] **S2-6** 登记表 **6 处后缀改写** + 行 5 / 行 13 的**闭口证据**更正（ADR → 归档目录）
- [x] **S2-7** 审稿记录 `.rddf/reviews/arch-residual-registry-normalization-2026-11.md`（逐行裁定 + **被删 7 段逐条登记** + 旧注记逐字存档 + oracle 1 轮处置）
- [x] **S2-8** 行 11 删除「探针覆盖缺口」7 段散文（★ 因本 change 而变假）+ 追加「已收口」说明
- [x] **S2-9** §7 前言 2 条注记 → 1 条（★ 旧注记逐字存档进审稿记录 §5.1）
- [x] **S2-10** 旧审稿记录 `.rddf/reviews/hierarchical-ledger-residuals-sync-2026-11.md` §4 追加「本工作项已收口」（★ 跟踪载体闭环）
- [x] **S2-11** oracle 1 轮审稿（★ 主题文档同步 ⇒ **只 oracle**）⇒ 8 问全过 + 5 条建议**全部采纳**（含 2 条判别力边界的自陈修正）

## S3（★ builder · CI 接线 —— **MUST 不早于 S2 合并**）

- [x] **S3-1** `.github/workflows/ci.yml` guard 段 +2 步（护栏 + `--self-test`）
- [x] **S3-2** CI 内投毒验证（⇒ CI 红）—— ★ **本地等价证据**：护栏判定 + 原地投毒（登记表行 11 的两个归档目录各改错一次 ⇒ exit 1 ⇒ sha256 逐字还原 ⇒ 复绿）⇒ ⇒ CI 上同一命令 ⇒ 同一退出码

## S4（★ builder · 归档）

- [x] **S4-1** verifier 验收（20 条 AC 逐条取证 ⇒ 全 PASS ⇒ verdict = `.rddf/state/.ac-verdict-arch-residual-registry-2026-11.json`；报告 = `.rddf/reviews/arch-residual-registry-2026-11-verify.md`）
- [ ] **S4-2** `openspec archive arch-residual-registry-2026-11`（★ **禁** `--skip-specs` ⇒ capability `ledger-residual-registry` 的 delta MUST 合入主 spec）
- [ ] **S4-3** worktree 分支合回 main（★ `check_worktree_commits` 要求该分支有新提交 ⇒ 已有 3 个）
