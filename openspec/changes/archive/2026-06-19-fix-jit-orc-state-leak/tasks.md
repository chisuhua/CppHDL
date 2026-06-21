## 1. 预实现调查 — LLVM ORC API 验证

- [ ] 1.1 查看 `cmake/FindLLVM.cmake` 确认项目锁定的 LLVM 版本范围(从 `find_package(LLVM REQUIRED)` 的 `REQUIRED` 版本)
- [ ] 1.2 在 LLVM 文档(对应锁定版本)上确认 `ExecutionSession::endSession()` 与 `JITDylib::dispose()` 的精确签名与调用顺序要求
- [ ] 1.3 `grep -rn "JITDylib\|ExecutionSession" src/jit/ include/jit/` 确认本项目持有的 `JITDylib` 数量(预期 1 个,需验证)
- [ ] 1.4 在 `src/jit/jit_compiler.cpp:167` `clear()` 现有实现上阅读,识别 IR 缓存清理与 ORC 资源释放的当前分割点

## 2. 核心实现 — JitCompiler 析构与 clear() 修复

- [ ] 2.1 修改 `src/jit/jit_compiler.cpp` 的 `JitCompiler::clear()`:在清空 IR 缓存之前,先 `static_cast<llvm::orc::ExecutionSession*>(jit_session_)->endSession()`,然后对所有 `JITDylib` 调 `dispose()`(per LLVM 文档顺序)
- [ ] 2.2 在 `clear()` 末尾释放 `static_cast<llvm::Module*>(llvm_module_)` 与 `llvm::IRBuilder` 等持有的 LLVM 对象,然后把 `jit_session_` / `llvm_module_` / `compiled_func_*` 全部置 `nullptr`
- [ ] 2.3 在 `clear()` 顶部加幂等保护:`if (jit_session_ == nullptr) return;` 避免重复释放
- [ ] 2.4 确认 `JitCompiler::~JitCompiler()` 仍仅调 `clear()`(`src/jit/jit_compiler.cpp:78`),不重复清理
- [ ] 2.5 用 `#if defined(CH_JIT_ENABLED) && __has_include(<llvm/IR/LLVMContext.h>)` 守卫整个 ORC 释放路径,与现有 `llvm_module_` 字段保持一致

## 3. 核心实现 — Simulator 析构顺序修复

- [ ] 3.1 修改 `src/simulator.cpp` `Simulator::~Simulator()` 顶部(line 103 之后):在 `disconnect()` 与 `trace_blocks_` 释放之前显式 `jit_compiler_.reset()`
- [ ] 3.2 确认 `jit_compiler_.reset()` 调用后,`data_map_` 仍在有效状态(Lifecycle 顺序:`jit_compiler_` 先于 `disconnect()` 析构)
- [ ] 3.3 检查 `Simulator` 其他成员(`trace_blocks_`、data_map 等)与 `jit_compiler_` 的依赖关系,确保 reset 顺序不会触发悬空访问

## 4. 新增自动化回归测试

- [ ] 4.1 新建 `tests/jit/` 目录(若不存在)与 `tests/jit/CMakeLists.txt`
- [ ] 4.2 创建 `tests/jit/test_jit_orc_state_isolation.cpp`,实现 2 个 `ch_device<T>`(不同 DUT,如 `Tc07XorChain` 与 `Tc08RegChain`)的顺序 benchmark,断言第二个的 JIT `sim_us` ≤ 第一个的 1.5×
- [ ] 4.3 在 `tests/jit/CMakeLists.txt` 用 `add_catch_test(test_jit_orc_state_isolation jit/test_jit_orc_state_isolation.cpp)` 注册
- [ ] 4.4 在 `tests/CMakeLists.txt` 顶层 `add_subdirectory(jit)` 引入新目录(若项目未自动包含 `tests/jit/`)
- [ ] 4.5 跑 `ctest -R test_jit_orc_state_isolation --output-on-failure` 确认新测试通过
- [ ] 4.6 跑 `ctest -L jit --output-on-failure` 确认整个 JIT 标签测试集通过

## 5. 全量回归验证

- [ ] 5.1 `cmake --build build -j$(nproc)` 0 errors 0 warnings
- [ ] 5.2 `ctest -L base --output-on-failure` 全部 83 个 base 测试通过
- [ ] 5.3 `ctest -L chlib --output-on-failure` 全部 28 个 chlib 测试通过
- [ ] 5.4 `./run_all_ported_tests.sh` 28 个 examples 全部通过
- [ ] 5.5 `ctest -R perf_jit_smallgraph --output-on-failure` 通过(JIT 性能不退化)
- [ ] 5.6 `ctest -R perf_jit_vs_interp_ratio --output-on-failure` 通过(结构性 gate 仍工作)
- [ ] 5.7 `lsp_diagnostics` 在改动文件上无新增警告(`src/jit/jit_compiler.cpp`、`src/simulator.cpp`、`tests/jit/test_jit_orc_state_isolation.cpp`)

## 6. F1 临时方案回退

- [ ] 6.1 验证所有回归测试通过后,修改 `tests/benchmark/perf_regression.cpp:141` 把 JIT 阈值从 `4.0` 恢复为 `1.6`
- [ ] 6.2 删除 2026-06-19 添加的 K1 临时注释(`// JIT threshold is TEMPORARILY raised from 1.6x to 4.0x...` 整段)
- [ ] 6.3 跑 `ctest -R perf_regression --output-on-failure`,确认此时 0 回归(current 数据反映干净 ORC 状态,与 baseline 的 clean state 捕获直接可比)
- [ ] 6.4 若 6.3 失败(说明 ORC 修复未完全解决 K1),回到 Decision 4 重新评估,可能需要同时实施 F2 子进程隔离

## 7. 文档同步

- [ ] 7.1 `docs/developer_guide/tech-reports/perf-report-todo.md`:
  - W1 描述改为"✅ 已根治 by fix-jit-orc-state-leak"
  - W12 改为"✅ 完成,链接本 change"
- [ ] 7.2 `docs/simulation/PERF_COMPARISON_REPORT.md` §6 K1:加"修复状态:已根治 by fix-jit-orc-state-leak" + 修复时间戳
- [ ] 7.3 `include/jit/AGENTS.md`:在"ANIT-PATTERNS" 表格前新增规则节"JIT 析构必须释放 ORC 资源",交叉引用本 change
- [ ] 7.4 (可选)重新生成 `tests/benchmark/perf_baseline.json`:在干净 ORC 状态下跑 `./build/tests/benchmark/perf_tests --all` 一次,把新 `perf_results.json` 重命名为 `perf_baseline.json` 并提交

## 8. 验证与归档

- [ ] 8.1 跑 `openspec validate "fix-jit-orc-state-leak" --strict` 确认 change 内部一致
- [ ] 8.2 跑 `openspec validate --specs --strict` 确认 main specs 未受影响
- [ ] 8.3 跑 `git status` 确认改动范围与 design.md 预期一致(只动 `src/jit/jit_compiler.cpp`、`src/simulator.cpp`、`tests/benchmark/perf_regression.cpp`、新建 `tests/jit/*`、文档 3 个)
- [ ] 8.4 跑 `git diff --stat` 检查改动量在 300 行内(本 change 是 refactor 类,不应超过)
- [ ] 8.5 按 OpenSpec 标准流程 `openspec archive "fix-jit-orc-state-leak"`,自动把 `specs/jit-compiler-lifecycle/spec.md` 合并到 `openspec/specs/jit-compiler-lifecycle/spec.md`
- [ ] 8.6 archive 完成后 `git log --oneline -3` 确认 archive commit 在 main 上
- [ ] 8.7 跑 `cmake --build build -j$(nproc) && ctest --output-on-failure` 一次最终回归

## 9. (可选) Follow-up 跟踪

- [ ] 9.1 在 `docs/developer_guide/tech-reports/perf-report-todo.md` 评估 W3(build_us 字段为 0)与 W4(overhead_percent 计算异常)是否在本 change archive 后继续推进
- [ ] 9.2 评估是否需要独立 OpenSpec change 实施 F2(`tests/benchmark/perf_main.cpp` 子进程隔离)作为 defense-in-depth
- [ ] 9.3 评估 K1 修复后是否需要新增"IR 缓存跨 DUT 复用"作为 follow-up(本 change 不做,但作为潜在优化记录)
