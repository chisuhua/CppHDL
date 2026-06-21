## Why

`perf_main` 在同一进程顺序执行 TC-07 → TC-08 → TC-09 时,`docs/simulation/PERF_COMPARISON_REPORT.md §6 K1` 描述的进程内污染导致 TC-08 JIT 路径在 polluted 状态下退化为 641-711k us,而 standalone TC-08 JIT 仅需 4.5-6k us(快于 interpreter 3-5×),**~142× 性能差异**。F1(`tests/benchmark/perf_regression.cpp:141` JIT 阈值 1.6× → 4.0×)是临时止血,会掩盖真实的 JIT 性能回归。

F3(`fix-jit-orc-state-leak`)原计划通过在 `JitCompiler` 析构中显式调用 `ExecutionSession::endSession()` + `JITDylib::dispose()` 治根。**Phase 1 实施调查发现 F3 设计前提错误**:`src/jit/jit_compiler.cpp:179` 的 `clear()` 已正确 `delete static_cast<LLJIT*>(jit_session_)`,`LLJIT` 析构自动清理所有 ORC 资源。ORC 资源**已正确释放**,但 K1 仍真实存在,真因可能是 LLVM 22 JITLink 段累积、类型 uniquing 表膨胀、mmap 累积等(均需 valgrind/strace 等 runtime profiling 验证,超出当前对话能力)。

**F2 是治标不治本的工程方案**:`tests/benchmark/perf_main.cpp` 每个 TC 改用 `std::system("perf_tests --tc=NN")` 起独立子进程,让每个 DUT 在 fresh process 跑,完全绕开 K1 污染。F2 不依赖根因调查,风险低,效果可验证。

## What Changes

- **`tests/benchmark/perf_main.cpp`**:主循环把 `run_three_way_tc07/08/09` 改为 `run_three_way_subprocess` — 每个 TC 调 `std::system("./build/tests/benchmark/perf_tests --tc=NN --report=json --workdir=...")`,捕获子进程 stdout 的 JSON,合并到主 reporter。
- **`tests/benchmark/perf_main.cpp` 新增** `subprocess_runner.h`(或 inline 辅助函数):封装子进程 fork+exec+waitpid+stdout 捕获,提供超时与错误传播。
- **`tests/benchmark/CMakeLists.txt`**:`perf_tests` 二进制已存在(无需新建),确保 ctest 与 CLI 入口均能找到 `build/tests/benchmark/perf_tests` 绝对路径。
- **新增或更新** `tests/benchmark/test_perf_subprocess_isolation.cpp`:验证子进程隔离下,连续 2 个 TC 的 perf 测量独立,无 100× 污染。
- **F1 阈值恢复**:`tests/benchmark/perf_regression.cpp:141` JIT 阈值从 4.0× 恢复为 1.6×,删除 2026-06-19 添加的 K1 临时注释。
- **新生成 baseline**:`tests/benchmark/perf_baseline.json` 重新在子进程隔离下生成,反映 clean per-DUT 状态。
- **文档同步**:
  - `docs/developer_guide/tech-reports/perf-report-todo.md` W12 标记为 "✅ 完成 (F2 子进程隔离)"
  - `docs/simulation/PERF_COMPARISON_REPORT.md` §6 K1 标注"已缓解 (F2 子进程隔离);根因未根治,见 fix-jit-orc-state-leak"
- **不在本 change scope 内**:
  - 不动 `src/jit/*`(K1 真因留作独立调研,见 `fix-jit-orc-state-leak`)
  - 不动 `src/simulator.cpp`(`Simulator` 析构顺序已正确,unique_ptr 自动管理)
  - 不重写 JIT 编译器架构
  - 不动 `JIT_MIN_NODES` 阈值

## Capabilities

### New Capabilities

- `perf-test-isolation`: 定义 `perf_main` 测试驱动的进程隔离契约 — 每个 TC(TC-07/08/09)必须在独立子进程中运行,父进程通过 stdout JSON 收集结果并合并到 `perf_results.json`。子进程隔离保证跨 DUT 的 LLVM ORC 状态不污染。本 change 在此 capability 下新增 2-3 条 `SHALL` 级 REQUIREMENT。

### Modified Capabilities

- 无。`openspec/specs/` 下现有 `chlib-aggregator/` 与本 change 无关。

## Impact

| 类别 | 影响 |
|------|------|
| 受影响代码 | `tests/benchmark/perf_main.cpp`(主循环改子进程调用)、`tests/benchmark/CMakeLists.txt`(确保 perf_tests 路径可访问) |
| 新增辅助 | `tests/benchmark/subprocess_runner.h`(新增,~80-120 行,封装 fork/exec/waitpid) |
| 新增测试 | `tests/benchmark/test_perf_subprocess_isolation.cpp`(新增 Catch2 测试) |
| 外部 API | 无 breaking change。`perf_main` CLI 行为兼容(`--all`、`--tc=NN` 仍工作) |
| 性能 | **正面**:子进程隔离后,perf 测量反映真实 per-DUT 性能(无 100× 污染)。**负面**:每次 TC fork+exec 增加 ~200-500ms 启动开销,total runtime 约增加 1-2s |
| CI | `perf_regression` 阈值从 4.0× 恢复为 1.6×,新增 `test_perf_subprocess_isolation` 加入 `ctest -L perf` |
| 风险 | **低**:子进程隔离是 well-known pattern,风险点仅为 fork/exec 路径、stdout 解析、超时处理 |
| 回滚 | `git revert` 单一 commit;回退后 perf_regression 阈值需改回 4.0× |
| 关联 issue | `docs/simulation/PERF_COMPARISON_REPORT.md §6 K1`、`docs/developer_guide/tech-reports/perf-report-todo.md` W12、`tests/benchmark/perf_regression.cpp:139-156`(F1 临时方案) |

## Non-Goals

- 不根治 K1(真因未明,留作独立调研 — `fix-jit-orc-state-leak` 标 SUPERSEDED,等待 runtime profiling 工具)
- 不重写 `perf_main` 主循环逻辑(只把 in-process 调 3-way 改成 subprocess 调)
- 不实现子进程池/缓存(每次 fork 一次,简单可靠)
- 不并行化 TC 测量(顺序执行足够,perf 测量本身需 wall-clock 真实)
- 不动 `perf_tests` 二进制本身的实现(只通过 CLI 调用)
- 不修其他 perf 测量框架缺陷(`build_us` 为 0、`overhead_percent` 计算异常 — W3/W4 独立)
- 不动 Verilator 集成(`verilator not found on PATH` 是环境问题,非 perf 测量问题)
