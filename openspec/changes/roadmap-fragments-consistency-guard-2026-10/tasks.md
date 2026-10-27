# Tasks: roadmap-fragments-consistency-guard-2026-10

## 0. 准备
- [ ] 0.1 读 `rddf roadmap --update-agent-md` 的输出格式 ⇒ 确定 AUTO 块的解析口径
- [ ] 0.2 用 `git show` 的旧版本复现本次实测的**两处漂移**（做 fixture）

## 1. 实现
- [ ] 1.1 `scripts/verify_roadmap_fragments.sh`：解析 AUTO 块 + feature front-matter ⇒ 比对
- [ ] 1.2 解析失败一律 **fail-closed**（不许静默跳过）
- [ ] 1.3 feature 文件**内部**的状态符号一致性（仅在两处含同一 change 名时比对）
- [ ] 1.4 `--self-test`：每种不一致各一个 fixture（≥ 4 项断言）

## 2. 验收
- [ ] 2.1 当前仓库 exit 0（零误报）
- [ ] 2.2 两处真实漂移的 fixture 都能被抓
- [ ] 2.3 挂进 CI（与 `verify_test_counts.sh` 同一 job）且 CI 绿

## 3. 文档与归档
- [ ] 3.1 根 `AGENTS.md` 记录「AUTO 块的一致性由本护栏守」
- [ ] 3.2 `openspec archive roadmap-fragments-consistency-guard-2026-10 --yes --skip-specs`
