## Why

`perf_main` 在同一进程顺序执行 TC-07 → TC-08 → TC-09 时,LLVM ORC JIT 层在 `ch_device<T>` 实例间保留状态(symbol collisions / layer caching / `JitCompiler` 单例泄漏),导致 TC-08 JIT 路径在 polluted 状态下退化为 437k-711k us,而 standalone TC-08 JIT 仅需 4-4.5k us(快于 interpreter 3-5×)。这是一个**测试方法论层面**的污染问题(已在 `docs/simulation/PERF_COMPARISON_REPORT.md §6 K1` 文档化)与**JIT 生命周期实现缺陷**叠加的产物,根治必须在 JIT 编译器层面确保跨 DUT 的状态正确清理。

F1(`tests/benchmark/perf_regression.cpp:141` JIT 阈值 1.6× → 4.0×)是临时止血,会掩盖真实的 JIT 性能回归;根治后才能把阈值恢复为 1.6× 并恢复 `perf_regression` 的检测灵敏度。

## What Changes

- **`src/jit/jit_compiler.cpp` 析构函数**:`JitCompiler` 析构时显式调用 `ExecutionSession::endSession()` 并 `dispose()` 所有 `JITDylib`,确保 LLVM ORC 层在每个 `JitCompiler` 实例销毁时完全释放,不污染下一个实例。
- **`src/simulator.cpp` `Simulator` 析构函数**:`Simulator::~Simulator` 显式 `reset()` `jit_compiler_` 单例(若存在),切断 ch_device/ch_module 跨实例的 ORC 状态共享。
- **`include/jit/jit_compiler.h` 类定义**:在 `JitCompiler` 类中声明非默认析构函数(如需要)并显式列出被拥有的 ORC 资源(ExecutionSession / JITDylib / SymbolMap),RAII 化所有权。
- **新增自动化回归测试** `tests/jit/test_jit_orc_state_isolation.cpp`:在同一进程内连续实例化两个不同的 `ch_device<T>`(分别测 JIT `sim_us`),断言第二个实例的 JIT 性能不劣于第一个的 1.5×(捕捉 K1 污染信号)。
- **F1 阈值恢复**:`tests/benchmark/perf_regression.cpp:141` JIT 阈值从 4.0× 恢复为 1.6×,删除 2026-06-19 添加的 K1 临时注释。`docs/developer_guide/tech-reports/perf-report-todo.md` W12 标记完成,W1 重新标为 `🔴 P0` 但诊断为"已根治"。
- **不在本 change scope 内**:
  - 不重生成 `tests/benchmark/perf_baseline.json`(独立 sub-task,放在 archive 阶段由 Prometheus 决定时机)
  - 不修 `tests/benchmark/perf_main.cpp` 的子进程隔离(此为 F2,独立 change,scope 互斥)
  - 不动 `jit_ir_gen_lnode_state.cpp`(ch_reg 路径已验证为 native,无需改)
  - 不调 `kBackendThresholds["interpreter"]` 与 `["verilator"]`

## Capabilities

### New Capabilities

- `jit-compiler-lifecycle`: 定义 CppHDL JIT 编译器的实例生命周期契约 — 每个 `JitCompiler` 实例自包含 ORC 资源,析构时必须完整释放;`Simulator` 析构时必须 reset `jit_compiler_` 单例,确保跨 `ch_device<T>` 实例的 JIT 状态隔离。本 change 在此 capability 下新增 1-2 条 `SHALL` 级别的 REQUIREMENT。

### Modified Capabilities

- 无。`openspec/specs/` 下现有 `chlib-aggregator/` 与本 change 无关(本 change 修改 `src/jit/` 与 `src/simulator.cpp`,不触及 chlib 聚合层)。

## Impact

| 类别 | 影响 |
|------|------|
| 受影响代码 | `src/jit/jit_compiler.cpp`(析构函数 + RAII 资源管理)、`include/jit/jit_compiler.h`(类定义)、`src/simulator.cpp`(`Simulator` 析构)、`tests/benchmark/perf_regression.cpp`(阈值恢复)、`docs/developer_guide/tech-reports/perf-report-todo.md`(W12 标记完成) |
| 新增测试 | `tests/jit/test_jit_orc_state_isolation.cpp`(新建 Catch2 测试,验证跨实例 JIT 性能不退化) |
| 外部 API | 无 breaking change。`JitCompiler` 与 `Simulator` 的公有接口保持稳定,仅析构语义补强 |
| 依赖 | LLVM/ORC2 已在 `cmake/FindLLVM.cmake` 接入,无需新增依赖 |
| 性能 | 修复后:同进程第二个 `ch_device` 的 JIT 路径应与 standalone 性能一致(4-4.5k us 级),不再有 437k-711k us 污染 |
| CI | `perf_regression` 阈值恢复 1.6×,`tests/jit/test_jit_orc_state_isolation` 加入 `ctest -L jit` |
| 风险 | 中等 — 触及 JIT 析构路径,需回归全部 141 个 ctest 与 28 个示例;LLVM ORC API 的 `endSession()` / `dispose()` 调用顺序有顺序依赖(参考 LLVM 文档) |
| 回滚策略 | `git revert` 单一 commit 即可恢复;`perf_regression` 阈值改回 4.0× 是临时回滚 |
| 关联 issue | `docs/simulation/PERF_COMPARISON_REPORT.md §6 K1`、`docs/developer_guide/tech-reports/perf-report-todo.md` W1/W12、`include/jit/AGENTS.md` |

## Non-Goals

- 不重写 JIT 编译器架构(只补析构,不动 generate_ir / compile_to_llvm 主体)
- 不优化 JIT 性能(只修状态污染,性能顺带恢复)
- 不动 `perf_main.cpp` 的进程内执行模型(F2 单独 change)
- 不动 baseline.json(F1 已落地,re-baseline 留作 archive 阶段)
- 不解决 Verilator 集成(`docs/developer_guide/tech-reports/perf-report-todo.md` W5 跟踪,独立 P1)
- 不实现 JIT 缓存层(可能后续优化,但本 change 范围内 RAII 化即可)
