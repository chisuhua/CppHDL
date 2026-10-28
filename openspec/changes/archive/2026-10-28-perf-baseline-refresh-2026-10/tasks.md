# Tasks: perf-baseline-refresh-2026-10

## 0. 准备
- [x] 0.1 确认采集窗口**独占**（共享主机 ⇒ 不得有其它 `ctest` / `cmake --build` 在跑）
      —— 实测：采集前 `uptime` = load **1.52 / 1.77 / 2.19**（4 核），
      `ps` 中**零** `ctest` / `cmake` / `cc1plus` 进程 ⇒ 机器空闲；
      采集期间**唯一**的负载来自 perf 自身（见 design.md 的 load 快照）
- [x] 0.2 记录旧 baseline 的**基线事实**（供采集后逐项对照）：
      行数 **53**（PASS 30 / UNSUPPORTED 9 / LEGACY 8 / SKIPPED 6）、
      锚点（PASS 且 `median_us > 0`）**30** 行、`median_us == 0` 的 **15** 行
      （6 SKIPPED + 9 UNSUPPORTED）、`note_restored` **6** 行（TC-09 interpreter/jit）
- [x] 0.3 记录 `git_sha` 口径 = **采集时的代码状态**（父提交 hash），不是本 change 的 commit
      —— 实测旧 baseline 的惯例：`git_sha=7c5038e`（采集时的 HEAD）而写入它的 commit 是
      `90d335d` ⇒ 本次填 **0758fa0**（采集时的 HEAD，工作区干净）

## 1. 重采集
- [x] 1.1 在**独占**窗口跑 `perf_tests --all`（全量，TIMEOUT 1800）
      —— 实测 **3 个独立样本**（同一 ctest 条目 = 与 CI 同旗标）：
      **908.90 s / 899.87 s / 863.20 s**（design.md §4.1）
- [x] 1.2 把 `tests/benchmark/perf_results.json` 落为新的
      `tests/benchmark/perf_baseline.json`
      —— ⚠️ **不是单次采样**：取 **3 个样本的逐行中位数**（design.md §3.2 的规则）
      ⇒ 行的其余字段取自「中位数所在的那个样本」⇒ 行内部自洽
- [x] 1.3 `note` 字段 MUST 含：git_sha / 采集日期 / 机器状态说明 /
      **与旧 baseline 的逐行比率** / 「为什么这不是代码回归」的论证
      —— 实测全部就位（4 条证据 + 采样方法学 + 逐行比率 + 离散度 + 披露表 + TC-09 段）
      （沿用 2026-10-25 那次的做法）
- [x] 1.4 ⚠️ **变松披露**：超过该 backend 阈值的行 MUST 在 `note` 的
      **披露表**里逐行点名，格式 = `<test> <params> <backend> old= new= ratio=`
      ⇒ **数值必须与实际一致**（护栏 C3 会核对数值）
      —— 实测**仅 1 行**超阈：`TC-11 regs=10 jit`（old 49654.42 → new 99906.55，
      ratio **2.012×** > 阈 1.6×）⇒ 已逐行点名 ⇒ 护栏 C3 核对数值一致 ✓
      —— ⚠️ **披露表与「逐行比率表」刻意用不同形状**（`old=` vs `old_us=`）⇒
      否则删掉披露表行后护栏仍能在比率表里找到同一行 ⇒ 披露缺失变成不可见

## 2. 守恒验收（`scripts/verify_perf_baseline_anchors.sh`）
- [x] 2.1 C1 **行集守恒**：`(test_name, params, backend)` 集合不变（**53 行**）
      —— 实测：3 个样本与旧 baseline 的行集**逐项相同** ✓
- [x] 2.2 C2 **锚点守恒**：旧 baseline 的 **30** 个 PASS 锚点仍 `median_us > 0`
      —— 实测：新 baseline 仍 **30** 个 PASS 锚点 ✓（其中 6 个 TC-09 为逐字恢复）
- [x] 2.3 C3 **变松披露**：变慢行已在披露表逐行点名且数值一致
      —— 实测：护栏打印「锚点变慢的行均已在 note 的披露表中逐行点名且数值一致（1 行）」✓
- [x] 2.4 `scripts/verify_perf_baseline_anchors.sh --self-test` 全绿
- [x] 2.5 `scripts/verify_perf_baseline_anchors.sh <base-ref>` exit 0
      —— 实测：工作树=新 baseline、基准=HEAD 的旧 baseline ⇒ **PASS** ✓
- [x] 2.6 ⚠️ **`median_us == 0` 的行数 MUST NOT 增加**（当前 **15**）
      —— 增加 = 比较集合缩小 = 未来真回归**永久不可见**
      —— 实测：旧 **15** → 新 **15** ✓（status 分布也逐项相同：
      PASS 30 / SKIPPED 6 / UNSUPPORTED 9 / LEGACY 8）
- [x] 2.7 ⚠️ **TC-09 的 6 个锚点**：若本次采集再次 SKIPPED ⇒ MUST **逐字恢复**
      （`note_restored` + 取值与旧 baseline 完全相同）⇒ 见 design.md §3 的
      **Shape A** 裁决（并已写进 capability 的 TC-09 条款）
      —— 实测：3 个样本**全部** SKIPPED（180 s 超时）⇒ 已 **6 行逐字恢复** ✓
      ⇒ 取值与旧 baseline 完全相同（82796.151 / 4873.126 / 786479.762 /
      45310.845 / 8489177.91 / 484189.365）⇒ 护栏 C2 的「逐字相等」断言通过 ✓

## 3. 回归
- [x] 3.1 `ctest -L perf` **11/11**
      —— 实测 **11/11 全绿**（2101 s / 35m01s）：`perf_tests` 926.77 s /
      `perf_subprocess_isolation` 1004.43 s / `perf_three_way` 163.11 s /
      `perf_regression` 0.02 s ✓ ⇒ 该次运行同时是**第 4 个独立样本**，
      闸门在新 baseline 下**实测绿**（不是「只在采集时绿」）
- [x] 3.2 `perf_regression` 仍比较 **18** 行且 **0 回归**
      （⚠️ **18** 是 results 侧的比较行数，**不是** baseline 的 53 行）
      —— 实测输出逐字吻合：`Compared 18 rows against baseline … skipped 0
      LEGACY/unknown-backend row(s)` + `No regressions.` ⇒ exit 0 ✓
- [x] 3.3 `ctest -LE perf` 零回归（non-perf **191/191**）
      —— 实测 **191/191 全绿**（431 s）
- [x] 3.4 其它护栏全 exit 0（9 个 `scripts/verify_*.sh`）
      —— 实测 **9/9 exit 0** + 3 个 `--self-test` 全绿 +
      `openspec validate --strict` valid + `verify_openspec_delta.sh` 4 条全绿

## 4. 规格（⚠️ 实施期新增：初稿的「无 spec 变更」被实测推翻）
- [x] 4.1 ⚠️ **发现两条 Requirement 互相矛盾**（design.md §2）
      —— ① TC-09 条款要求 baseline 与 current **两侧皆 `median_us=0`**；
      ② 锚点守恒条款要求 TC-09 锚点 **MUST 仍 `> 0`**
      ⇒ 仓内**已提交**的 baseline 里那 6 个 TC-09 锚点正是 `PASS` 且 `> 0`
      ⇒ **互斥** ⇒ 「无 spec 变更 ⇒ `--skip-specs`」的前提**不成立**
- [x] 4.2 写 MODIFIED delta（`perf-test-isolation` 的 TC-09 Scenario）
      —— 改写为 **Shape A（锚点保留 + current 为 0 ⇒ 比值 0 ⇒ 无假阳性）**
      与 **Shape B（两侧皆 0 ⇒ 比较被跳过）** 都合法，
      并明确「若上一份 baseline 持有 TC-09 的 PASS 锚点 ⇒ **必须**走 Shape A」
      ⇒ 契约与实现**逐字一致**
- [x] 4.3 `.openspec.yaml` 的 `skip_specs: true` **删除**（改为 `false`）
      ⇒ 归档**不得**用 `--skip-specs`
- [x] 4.4 `openspec validate --strict` + `scripts/verify_openspec_delta.sh` 全绿
- [x] 4.5 ⚠️ **前置证据**：「自旧 baseline 以来，方法学要素 (a)-(e) **未变**」
      —— `perf_main.cpp` / `subprocess_runner.h` / `verilator_runner.h` /
      `perf_regression.cpp`（阈值）**全部未变**，`BUILD_VERILATOR` 仍为 `ON`
      ⇒ 本次重采集**不是**方法学变更触发的 ⇒ 触发的是**环境漂移**（机器状态脱节）

## 5. 文档与归档
- [x] 5.1 根 `AGENTS.md` 的 perf NOTES 条目同步（新 baseline 的 git_sha / 日期 / 比率）
      —— 已加 2026-10-28 的 NOTES 条目（git_sha / 采样方法学 / 4 条「不是代码回归」
      证据 / 抖动行 / 披露行 / TC-09 恢复）+ **修正两处已过时的「backlog」陈述**
      （原写「baseline 已脱节 ⇒ 需重采集」⇒ 现写「已解决」）
      + `tests/benchmark/CMakeLists.txt` 的 `t_meas` 注释块同步
      （契约要求记录测量日期与数值）
- [x] 5.2 `openspec archive perf-baseline-refresh-2026-10 --yes`
      （**禁** `--skip-specs` ⇒ 主 spec 合并 MODIFIED ×1）
           —— 实测：已归档为 `2026-10-28-perf-baseline-refresh-2026-10`
           ⇒ 主 spec `perf-test-isolation` **~ 1 modified**（TC-09 条款的 Shape A/B 改写）
           ⇒ **未**用 `--skip-specs` ✓
