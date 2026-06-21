## Context

`perf_main` 当前在同一进程内顺序执行 TC-07 → TC-08 → TC-09,触发 K1 ORC JIT 跨 DUT 状态污染。Standalone TC-08 JIT = 4.5-6k us,polluted TC-08 JIT = 641-711k us,~142× 性能差异(K1 doc 与 2026-06-19 实测一致)。F1 把 `perf_regression` 阈值临时提到 4.0× 是止血,F3 (`fix-jit-orc-state-leak`) 原计划治根但 Phase 1 实施调查发现设计前提错误(`clear()` 已正确 `delete LLJIT`,ORC 资源已释放)。

**当前 perf_main 主循环结构**(`tests/benchmark/perf_main.cpp:564-614`):
```cpp
if (run_all || tc07) {
    for (int d : {10, 100, 1000}) {
        auto twr = run_three_way_tc07(d, ...);  // 同进程内构造 3 个 Simulator
        reporter.add_result(twr.interp); reporter.add_result(twr.jit); ...
    }
}
if (run_all || tc08) {
    for (int n : {10, 100, 1000}) {
        auto twr = run_three_way_tc08(n, ...);  // 已被 TC-07 污染
        ...
    }
}
```

**F2 改造目标**:把每个 TC 改为独立子进程。父进程 `perf_main` 在 `--all` 模式下 fork 9 个子进程(TC-07/08/09 × 3 sizes),每个子进程跑单一 TC,生成 `perf_results.json` 临时文件,父进程读 JSON 合并到主 reporter。

**F2 不解决 K1 真因** — 只确保每次 perf 测量在 fresh process 跑。真因调查(LLVM 22 JITLink 段累积、类型 uniquing 表膨胀等)留作独立工作,在 `fix-jit-orc-state-leak` 标 SUPERSEDED 后另立。

## Goals / Non-Goals

**Goals:**
- 每个 TC(TC-07/08/09 × 3 sizes = 9 个)在独立子进程跑,`perf_main` 父进程收集 JSON 合并结果
- 子进程隔离下,`perf_regression` 阈值从 4.0× 恢复为 1.6×,F1 临时注释删除
- `tests/benchmark/perf_baseline.json` 重新在子进程隔离下生成,作为新基线
- 新增 `tests/benchmark/test_perf_subprocess_isolation.cpp` 验证子进程隔离契约

**Non-Goals:**
- 不根治 K1 ORC 状态污染
- 不并行化 TC 测量(顺序执行足够)
- 不实现子进程池/缓存
- 不修改 `perf_tests` 子进程二进制本身(只通过 CLI 调用)
- 不动 `src/jit/*`、`src/simulator.cpp`、`include/jit/*`
- 不修复其他 perf 测量框架缺陷(W3 build_us=0、W4 overhead_percent 异常)

## Decisions

### Decision 1: 用 `std::system()` 而非 `posix_spawn`/`fork+execve`

**Rationale**: `std::system()` 在 C++ 标准库中,跨平台兼容(Linux/macOS/Windows),API 简单。子进程 stdout 通过临时文件重定向:`std::system("perf_tests --tc=NN > /tmp/perf_tcNN.json 2>&1")`,父进程读临时文件解析 JSON。

**Alternatives considered**:
- (A) `posix_spawn` (POSIX-specific) — 流控制更精细但 API 复杂;**拒绝**:跨平台需求 + 复杂度不必要
- (B) `fork+execve` with pipe — 实时 stdout 流;**拒绝**:perf 测量在子进程一次性跑完,无需实时流
- (C) `std::system` + temp file — **接受**:简单可靠,适合一次性 perf 测量

### Decision 2: 子进程入口用 `perf_tests --tc=NN` 已有 CLI

**Rationale**: `perf_tests` 已有 `--tc=NN` 单 TC 模式(`tests/benchmark/perf_main.cpp:467`),standalone TC-08 实测 4.5-6k us(干净状态)。复用现有 CLI,无需新增二进制或参数。

**Alternatives considered**:
- (A) 新增 `perf_isolated_tc --tc=NN` 专用二进制 — **拒绝**:与 `perf_tests` 功能重复
- (B) 通过共享库加载 + 自定义测试函数 — **拒绝**:复杂度高,需要 ABI 稳定
- (C) 复用 `perf_tests --tc=NN` — **接受**:已实现,已验证可用

### Decision 3: JSON 合并在父进程 reporter 端

**Rationale**: `report_generator.h` 已有 `add_result()` 接口(`tests/benchmark/perf_main.cpp:655` `reporter.export_json(...)`)。子进程输出独立 JSON,父进程读 JSON → 调 `add_result()` 逐行合并 → 主 reporter 一次性写 `perf_results.json`。

**Alternatives considered**:
- (A) 子进程写 partial JSON,父进程拼接字符串 — **拒绝**:JSON 解析风险高
- (B) 子进程通过 stdin 传 result row(实时) — **拒绝**:需要新协议,复杂度高
- (C) 父进程读 JSON → `add_result()` 合并 — **接受**:复用现有 reporter API

### Decision 4: 新增 `subprocess_runner.h` 而非直接 inline 在 `perf_main.cpp`

**Rationale**: `perf_main.cpp` 当前 ~28k 行,新增 ~100 行子进程调用会膨胀文件。提到独立 header 便于:
- 单测(可独立 mock)
- 复用(未来其他场景如 `benchmark_tracker.sh` 也能用)
- 可读性(子进程封装是独立 concern)

**Alternatives considered**:
- (A) inline 静态函数 — **拒绝**:难单测
- (B) 独立 `.cpp` 文件 + 链接到 perf_tests 目标 — **拒绝**:增加构建复杂度
- (C) `subprocess_runner.h` (header-only,inline 实现) — **接受**:header-only 库一致风格,单测可包含

## Risks / Trade-offs

| Risk | Mitigation |
|------|-----------|
| `std::system()` 路径含空格或特殊字符会失败 | 用绝对路径 + 转义(`build/tests/benchmark/perf_tests`),CTest 工作目录已是项目根 |
| 子进程超时(perf_tests 跑 --all 可能 >2 分钟) | 父进程设 wall-clock 软超时 180s(per TC),超时 SIGTERM 子进程,记录 SKIPPED |
| 子进程 stdout JSON 解析失败 | `parse_subprocess_json()` 返回 `std::optional<nlohmann::json>`,失败时 SKIPPED 该 TC,继续后续 |
| 子进程退出码非 0(LLVM 段错误等) | 父进程读 exit code,非 0 时 stderr 包含错误,标记 SKIPPED |
| 临时文件残留污染后续运行 | 每次用 PID + 时间戳生成唯一临时文件名(`/tmp/perf_main_pid_tcNN.json`),运行结束 `std::remove()` |
| Windows 兼容 | `std::system()` 跨平台,JSON 路径用 `std::filesystem::temp_directory_path()` 替代硬编码 `/tmp` |
| 总运行时间增加 1-2s(9 次 fork+exec) | 可接受 — 当前 perf 总时间 ~2-5 分钟,1-2s 增量 < 5% |
| 子进程隔离可能掩盖真实的 in-process bug | K1 仍记录在 `PERF_COMPARISON_REPORT.md` + `fix-jit-orc-state-leak` SUPERSEDED 文件,真因调研独立推进 |

## Migration Plan

**阶段 1: 实现** (估计 3-4 小时)
1. 新建 `tests/benchmark/subprocess_runner.h`(header-only,~100-150 行)
2. 修改 `tests/benchmark/perf_main.cpp` 主循环:替换 `run_three_way_tc07/08/09` 调用为 `run_three_way_subprocess("07"/"08"/"09")`
3. 跑 `cmake --build build --target perf_tests` 0 errors 0 warnings
4. 跑 `./build/tests/benchmark/perf_tests --tc=08` 验证子进程路径与 standalone 一致(~4.5-6k us)

**阶段 2: 验证**
1. 跑 `./build/tests/benchmark/perf_tests --all`,确认子进程模式下所有 9 个 TC 测出
2. 跑 `ctest -L perf --output-on-failure` 全部通过
3. 跑新测试 `ctest -R test_perf_subprocess_isolation --output-on-failure` 验证隔离契约

**阶段 3: 恢复 F1**
1. 验证全过 → 修改 `tests/benchmark/perf_regression.cpp:141` JIT 阈值 4.0× → 1.6×
2. 删除 2026-06-19 添加的 K1 临时注释
3. 跑 `ctest -R perf_regression` 确认 0 回归(此时 current 是 clean per-DUT 状态,baseline 也是 clean state,直接可比)
4. 重新生成 `tests/benchmark/perf_baseline.json`:跑 `./build/tests/benchmark/perf_tests --all`,把新 `perf_results.json` 重命名为 `perf_baseline.json`,提交

**阶段 4: 文档同步**
1. `docs/developer_guide/tech-reports/perf-report-todo.md`:W12 标 "✅ 完成 (F2 子进程隔离)"
2. `docs/simulation/PERF_COMPARISON_REPORT.md` §6 K1 加"缓解状态:F2 子进程隔离;根因未根治"
3. `tests/AGENTS.md` 加新章节"perf measurement 隔离":说明 --all 模式走子进程,benchmarks 间无 ORC 状态污染

**Rollback strategy**:
- `git revert` 单一 commit 即可回退全部代码
- 临时回滚:保留 F1 4.0× 阈值 + 把 `--all` 模式 flag 关闭(默认 in-process)
- 紧急回滚:删除 `subprocess_runner.h` include,恢复 `run_three_way_tc07/08/09` in-process 调用

## Open Questions

1. **是否需要 `--parallel` 选项**让用户选择串行 or 并行子进程?当前决策是只支持串行(简单可靠)。并行会引入新的复杂度(资源竞争、报告合并顺序)。**责任**:用户决定;**非阻塞**
2. **verilator 子进程是否也走同样模式**?当前 verilator 在 perf_main 内是 SKIPPED 状态(`verilator not found on PATH`),实际不起作用。如果将来集成 verilator,可能也需子进程隔离。**责任**:未来 W5 决策;**非阻塞**
3. **`benchmark_tracker.sh` 是否也走子进程**?`PERF_COMPARISON_REPORT.md` 提到 `benchmark_tracker.sh` 也跑 in-process。F2 实施后是否让 `benchmark_tracker.sh` 也改用 `perf_tests --all` 子进程调用?**责任**:用户决定;**非阻塞**,作为 follow-up
4. **W12 文档 / K1 doc 是否需要明确"已缓解 vs 已根治"**?本次 F2 是缓解(治标),不是根治(治本)。文档措辞需谨慎。**责任**:文档撰写时把控;**已在本 design 中标明**

## Cross-References

- **Proposal**:`proposal.md`
- **Spec**:`specs/perf-test-isolation/spec.md`
- **Tasks**:`tasks.md`
- **关联文档**:
  - `docs/simulation/PERF_COMPARISON_REPORT.md §6 K1`(污染权威依据)
  - `docs/developer_guide/tech-reports/perf-report-todo.md` W12(本 change 解决)
  - `../fix-jit-orc-state-leak/README.md`(标 SUPERSEDED,真因调研独立)
  - `tests/benchmark/perf_main.cpp`(主循环改子进程)
  - `tests/benchmark/CMakeLists.txt`(`perf_tests` 路径)
- **F1 临时方案**:`tests/benchmark/perf_regression.cpp:139-156` 4.0× 阈值(本 change 完成后恢复 1.6×)
