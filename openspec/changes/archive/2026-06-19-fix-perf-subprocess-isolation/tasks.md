## 1. 实现 subprocess_runner.h

- [ ] 1.1 新建 `tests/benchmark/subprocess_runner.h` 头文件(header-only,inline 实现)
- [ ] 1.2 实现 `run_subprocess(const std::string& exe_path, const std::vector<std::string>& args, int timeout_sec)` 函数 — 封装 fork+exec+waitpid,捕获 stdout 到临时文件
- [ ] 1.3 实现 `parse_subprocess_json(const std::string& path)` 函数 — 读 JSON,返回 `std::optional<nlohmann::json>`
- [ ] 1.4 实现超时 SIGTERM 机制(基于 `alarm()` + `waitpid(pid, &status, WNOHANG)` 轮询)
- [ ] 1.5 实现临时文件路径生成: `std::filesystem::temp_directory_path() / "perf_main_<pid>_<tc>_<idx>.json"`
- [ ] 1.6 在头文件 include 守卫 `#pragma once`,namespace `ch::perf_test`
- [ ] 1.7 写最小烟雾测试(在文件内 `#ifdef RUN_SUBPROCESS_SMOKE` 块):启一个 echo 子进程,验证 stdout 捕获

## 2. 改造 perf_main.cpp 主循环

- [ ] 2.1 在 `tests/benchmark/perf_main.cpp` 顶部 include `subprocess_runner.h`
- [ ] 2.2 替换 `run_three_way_tc07` 调用为 `run_three_way_subprocess("07", depth, ...)`(在 main 循环 line 569-574)
- [ ] 2.3 替换 `run_three_way_tc08` 调用为 `run_three_way_subprocess("08", regs, ...)`(在 main 循环 line 586-591)
- [ ] 2.4 替换 `run_three_way_tc09` 调用为 `run_three_way_subprocess("09", depth, ...)`(在 main 循环 line 603-608)
- [ ] 2.5 `run_three_way_subprocess` 内部:构造子进程命令 `./build/tests/benchmark/perf_tests --tc=NN --report=json --workdir=<wd>`,调 `run_subprocess`,解析 JSON,合并到 reporter
- [ ] 2.6 保留 `run_three_way_tc07/08/09` 函数定义(可能用于其他场景如单元测试),加 `#ifdef ENABLE_INPROCESS_TC` 守卫

## 3. 新增自动化回归测试

- [ ] 3.1 新建 `tests/benchmark/test_perf_subprocess_isolation.cpp` Catch2 测试
- [ ] 3.2 第一个 TEST_CASE:跑 `./build/tests/benchmark/perf_tests --tc=08` 独立,记录 TC-08 JIT median_us
- [ ] 3.3 第二个 TEST_CASE:跑 `perf_main --all` 通过 subprocess 隔离,提取 TC-08 JIT median_us,断言 ratio ≤ 1.5×(per spec)
- [ ] 3.4 第三个 TEST_CASE:验证 subprocess 超时不导致 parent 崩溃(用 `sleep 999` 子进程 + 1s timeout)
- [ ] 3.5 在 `tests/CMakeLists.txt` 注册 `add_catch_test(test_perf_subprocess_isolation benchmark/test_perf_subprocess_isolation.cpp)`,`LABELS "perf" TIMEOUT 300`
- [ ] 3.6 跑 `ctest -R test_perf_subprocess_isolation --output-on-failure` 验证通过

## 4. 编译与全量回归

- [ ] 4.1 `cmake --build build -j$(nproc)` 0 errors 0 warnings
- [ ] 4.2 跑 `./build/tests/benchmark/perf_tests --tc=08` 验证子进程 standalone 模式工作,median_us 在 4.5-6k us 范围
- [ ] 4.3 跑 `./build/tests/benchmark/perf_tests --all` 验证 9 个 TC 全部测出,TC-08 JIT median_us 不超过 6.5k us(per spec 1.5× tolerance)
- [ ] 4.4 跑 `ctest -L perf --output-on-failure` 全部通过
- [ ] 4.5 跑 `ctest -L base --output-on-failure` 全部 83 个 base 测试通过(确认无回归)
- [ ] 4.6 跑 `ctest -L chlib --output-on-failure` 全部 28 个 chlib 测试通过
- [ ] 4.7 跑 `./run_all_ported_tests.sh` 28 个 examples 全部通过
- [ ] 4.8 跑 `lsp_diagnostics` 在改动文件上无新增警告

## 5. F1 临时方案回退 + baseline 重生成

- [ ] 5.1 验证全过 → 修改 `tests/benchmark/perf_regression.cpp:141` JIT 阈值 4.0× → 1.6×
- [ ] 5.2 删除 2026-06-19 添加的 K1 临时注释(整段 `// JIT threshold is TEMPORARILY raised...`)
- [ ] 5.3 跑 `ctest -R perf_regression --output-on-failure`,确认此时 0 回归(子进程隔离下 current 与 baseline 都反映 clean per-DUT 状态)
- [ ] 5.4 重新生成 `tests/benchmark/perf_baseline.json`:跑 `./build/tests/benchmark/perf_tests --all` 一次,把新 `perf_results.json` 内容复制到 `tests/benchmark/perf_baseline.json`,更新 `git_sha` 字段
- [ ] 5.5 更新 `perf_baseline.json` 的 `note` 字段,标注"F2 subprocess isolation baseline, regenerated 2026-06-19"
- [ ] 5.6 跑 `ctest -R perf_regression` 最终验证 0 回归

## 6. 文档同步

- [ ] 6.1 `docs/developer_guide/tech-reports/perf-report-todo.md`:
  - W12 描述改为"✅ 完成(F2 子进程隔离 by fix-perf-subprocess-isolation)"
  - 推进顺序:删除 W12(已完成),保留 W1(根因未根治,标 deferred to runtime profiling)
- [ ] 6.2 `docs/simulation/PERF_COMPARISON_REPORT.md` §6 K1:加"缓解状态:F2 子进程隔离 (2026-06-19);根因未根治,见 fix-jit-orc-state-leak (SUPERSEDED)"
- [ ] 6.3 `tests/AGENTS.md`:在新增"perf measurement 隔离"章节说明 `--all` 模式走子进程,benchmarks 间无 ORC 状态污染
- [ ] 6.4 更新根 `AGENTS.md` 中"`perf_tests` ~120s; pass count rerun before claiming"` 注释 — 现在是 9 个子进程串行,总时间 ~3-5 分钟,加 1-2s subprocess 启动开销

## 7. 验证与归档

- [ ] 7.1 跑 `openspec validate "fix-perf-subprocess-isolation" --strict` 确认 change 内部一致
- [ ] 7.2 跑 `openspec validate --specs --strict` 确认 main specs 未受影响
- [ ] 7.3 跑 `git status` 确认改动范围:`tests/benchmark/subprocess_runner.h` (新) + `tests/benchmark/perf_main.cpp` (改) + `tests/benchmark/test_perf_subprocess_isolation.cpp` (新) + `tests/CMakeLists.txt` (改) + `tests/benchmark/perf_regression.cpp` (改) + 3 个文档(改) + `perf_baseline.json` (改)
- [ ] 7.4 跑 `git diff --stat` 检查改动量在 500 行内(本 change 含新文件 2 个,合理范围)
- [ ] 7.5 按 OpenSpec 标准流程 `openspec archive "fix-perf-subprocess-isolation"`,自动把 `specs/perf-test-isolation/spec.md` 合并到 `openspec/specs/perf-test-isolation/spec.md`
- [ ] 7.6 archive 完成后 `git log --oneline -3` 确认 archive commit 在 main 上
- [ ] 7.7 跑 `cmake --build build -j$(nproc) && ctest --output-on-failure` 最终一次回归

## 8. (可选) Follow-up 跟踪

- [ ] 8.1 评估 K1 真因调研是否需独立 PR(需要 valgrind massif + strace + 真实 perf run,作为单独工程任务)
- [ ] 8.2 评估 `benchmark_tracker.sh` 是否也改用子进程模式(PERF_COMPARISON_REPORT.md §4 提到它跑 in-process)
- [ ] 8.3 评估 W3(build_us=0)与 W4(overhead_percent 计算异常)是否在本 change archive 后继续推进
- [ ] 8.4 评估 verilator 集成是否需要类似子进程隔离(目前 UNSUPPORTED,非阻塞)
