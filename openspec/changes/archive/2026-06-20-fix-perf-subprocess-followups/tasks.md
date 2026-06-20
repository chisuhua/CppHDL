## 1. CTEST TIMEOUT 调整

- [ ] 1.1 `tests/CMakeLists.txt:99-110` — `perf_subprocess_isolation` `TIMEOUT 300` → `TIMEOUT 1800`,同步注释(实测 wall-clock 868.20s,1.6× 安全余量)
- [ ] 1.2 `tests/benchmark/CMakeLists.txt:69-91` — `perf_tests` 两个分支(if `CPPHDL_VERILATOR_WRAPPER`/else)的 `TIMEOUT 900` → `TIMEOUT 1800`
- [ ] 1.3 `tests/benchmark/CMakeLists.txt:69-72` — 删除过时 "The 300s timeout guards against hangs" 注释,补实测 ~930s + 1800s 安全余量说明(F2 文档漂移修复)
- [ ] 1.4 `cmake -B build` 触发 `CTestTestfile.cmake` 重生成,验证两个 TIMEOUT 都生效为 `1800`

## 2. perf_baseline.json 重新生成(F2 task 5.4-5.5 落地)

- [ ] 2.1 备份旧 baseline:`cp tests/benchmark/perf_baseline.json /tmp/perf_baseline_pre_F2_regen.json`(已 2026-06-20 完成)
- [ ] 2.2 确认 `perf_results.json` 是最新一次 `perf_tests --all` 的输出(确认 timestamp 与 `git_sha=7b27de5` 一致,2026-06-20)
- [ ] 2.3 用 Python 脚本从 `perf_results.json` 重生成 `tests/benchmark/perf_baseline.json`(复制 `runs` 数组,更新 `git_sha` / `timestamp` / `note`)
- [ ] 2.4 `note` 字段写明:F2 subprocess isolation baseline、TC-09 SKIPPED 根因(180s 子进程超时)、regenerated 2026-06-20
- [ ] 2.5 `git diff --stat tests/benchmark/perf_baseline.json` 确认改动范围合理(应 ~20-25 KB,53 行 runs)

## 3. 文档同步

- [ ] 3.1 `tests/AGENTS.md:87` — "5-10 minute total runtime" 修正为 "~15-20 minute total runtime",TIMEOUT `300` 修正为 `1800`,补 TC-09/TC-11 耗时瓶颈说明
- [ ] 3.2 `AGENTS.md:169` — "1 pre-existing timeout: `perf_tests` ~120s" 修正为 "TIMEOUT 1800 (F2 subprocess-isolated, ~15-20 min)";`141` → `142`(新测试计数)
- [ ] 3.3 验证两处文档与代码一致(grep TIMEOUT 数字)

## 4. 验证(本 change 落地后立即跑)

- [ ] 4.1 `cmake -B build && cmake --build build -j$(nproc)` — 0 errors 0 warnings
- [ ] 4.2 `ctest -R "^perf_subprocess_isolation$" --timeout 1800` — PASSED(实测 868.20s)
- [ ] 4.3 `ctest -R "^perf_tests$" --timeout 1800` — PASSED(实测 1110.95s)
- [ ] 4.4 `ctest -R "^perf_regression$"` — PASSED(0.02s,no regressions)
- [ ] 4.5 `ctest -L perf -E "perf_tests|perf_subprocess_isolation"` — 7/7 PASSED
- [ ] 4.6 `ctest -L base -j4 --timeout 60` — 110/110 PASSED(确认无回归)
- [ ] 4.7 `git status` — 改动范围:`tests/CMakeLists.txt`(改)+ `tests/benchmark/CMakeLists.txt`(改)+ `tests/benchmark/perf_baseline.json`(重生成)+ `tests/AGENTS.md`(改)+ `AGENTS.md`(改)

## 5. OpenSpec 验证与归档

- [ ] 5.1 `openspec validate "fix-perf-subprocess-followups" --strict` — PASS(本 change 内部一致)
- [ ] 5.2 `openspec validate --specs --strict` — PASS(main specs 未受影响)
- [ ] 5.3 `openspec show "fix-perf-subprocess-followups"` — 确认 4 个 artifact 全部 done
- [ ] 5.4 `git add -A && git diff --cached --stat` — 确认改动范围合理(预计 ~5 文件,~30 KB diff)
- [ ] 5.5 提交(若用户允许):commit message 含"perf TIMEOUT re-evaluated: 300/900 → 1800 (measured 868/1110s)"和"baseline regenerated: git_sha=7b27de5 timestamp=2026-06-20"两个 footer
- [ ] 5.6 `openspec archive "fix-perf-subprocess-followups"` — 把 `specs/perf-test-isolation/spec.md` 的 2 条 ADDED REQUIREMENT 合并到 `openspec/specs/perf-test-isolation/spec.md`
- [ ] 5.7 `git log --oneline -3` — 确认 archive commit 在 main 上
- [ ] 5.8 `openspec list --specs` — 验证 `perf-test-isolation` 现在含 7 条 REQUIREMENT(原 5 + 新增 2)

## 6. (已知不在本 change scope)Follow-up 跟踪

- [ ] 6.1 TC-09 子进程 180s timeout 根因:`subprocess_runner.h:168` timeout 提升 vs `perf_main.cpp:744-747` TC-09 params 缩减的方案对比(独立 PR)
- [ ] 6.2 baseline 自动重新生成机制(目前手工,CI 触发器超出 scope)
- [ ] 6.3 TC-11 subprocess 隔离(`perf_main.cpp:773-787` `run_three_way_tc11` in-process 是否需要改 subprocess,F2 遗留不一致)
- [ ] 6.4 `perf_subprocess_isolation` 与 `perf_tests` 都跑同一个 `perf_tests --all`(~30-40 min CI 累积),评估 fixture 共享可行性