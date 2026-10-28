# Tasks: perf-baseline-refresh-2026-10

## 0. 准备
- [ ] 0.1 确认采集窗口**独占**（共享主机 ⇒ 不得有其它 `ctest` / `cmake --build` 在跑）
- [ ] 0.2 记录旧 baseline 的**基线事实**（供采集后逐项对照）：
      行数 **53**（PASS 30 / UNSUPPORTED 9 / LEGACY 8 / SKIPPED 6）、
      锚点（PASS 且 `median_us > 0`）**30** 行、`median_us == 0` 的 **15** 行
- [ ] 0.3 记录 `git_sha` 口径 = **采集时的代码状态**（父提交 hash），不是本 change 的 commit

## 1. 重采集
- [ ] 1.1 在**独占**窗口跑 `ctest -L perf`（全量，TIMEOUT 1800）
- [ ] 1.2 把 `tests/benchmark/perf_results.json` 落为新的
      `tests/benchmark/perf_baseline.json`
- [ ] 1.3 `note` 字段 MUST 含：git_sha / 采集日期 / 机器状态说明 /
      **与旧 baseline 的逐行比率** / 「为什么这不是代码回归」的论证
      （沿用 2026-10-25 那次的做法）
- [ ] 1.4 ⚠️ **变松披露**：超过该 backend 阈值的行 MUST 在 `note` 的
      **披露表**里逐行点名，格式 = `<test> <params> <backend> old= new= ratio=`
      ⇒ **数值必须与实际一致**（护栏 C3 会核对数值）

## 2. 守恒验收（`scripts/verify_perf_baseline_anchors.sh`）
- [ ] 2.1 C1 **行集守恒**：`(test_name, params, backend)` 集合不变（**53 行**）
- [ ] 2.2 C2 **锚点守恒**：旧 baseline 的 **30** 个 PASS 锚点仍 `median_us > 0`
- [ ] 2.3 C3 **变松披露**：变慢行已在披露表逐行点名且数值一致
- [ ] 2.4 `scripts/verify_perf_baseline_anchors.sh --self-test` 全绿
- [ ] 2.5 `scripts/verify_perf_baseline_anchors.sh <base-ref>` exit 0
- [ ] 2.6 ⚠️ **`median_us == 0` 的行数 MUST NOT 增加**（当前 **15**）
      —— 增加 = 比较集合缩小 = 未来真回归**永久不可见**

## 3. 回归
- [ ] 3.1 `ctest -L perf` **11/11**
- [ ] 3.2 `perf_regression` 仍比较 **18** 行且 **0 回归**
      （⚠️ **18** 是 results 侧的比较行数，**不是** baseline 的 53 行）
- [ ] 3.3 `ctest -LE perf` 零回归（non-perf **191/191**）
- [ ] 3.4 其它护栏全 exit 0（8 个 `scripts/verify_*.sh`）

## 4. 文档与归档
- [ ] 4.1 根 `AGENTS.md` 的 perf NOTES 条目同步（新 baseline 的 git_sha / 日期 / 比率）
- [ ] 4.2 `openspec archive perf-baseline-refresh-2026-10 --yes --skip-specs`
      （⚠️ `skip_specs: true` **成立** ⇒ `perf-test-isolation` 的既有 Requirement
      「perf baseline 重生成 MUST 守恒（行集 / 锚点 / 变松披露）」已覆盖本流程
      ⇒ 无需 MODIFIED）
