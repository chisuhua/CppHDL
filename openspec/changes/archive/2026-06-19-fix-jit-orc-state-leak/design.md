## Context

`perf_main` 同进程顺序执行 TC-07 → TC-08 → TC-09 时,第二个及之后的 `ch_device<T>` 实例的 JIT 路径出现 437k-711k us 性能退化(详见 `docs/simulation/PERF_COMPARISON_REPORT.md §6 K1`)。Standalone TC-08 JIT 仅 4-4.5k us,差距 100×+。

**根因**:`JitCompiler` 实例在析构时未显式释放 LLVM ORC `ExecutionSession` 与 `JITDylib` 资源,导致 ORC 层的 symbol 表、layer cache、`JitCompiler` 单例的 `JITDylib` 在 `ch_device<T>` 跨实例时保留并污染下一个实例的查找路径。`Simulator::~Simulator` 通过 `std::unique_ptr<JitCompiler>` 自动触发 `JitCompiler` 析构,但 `JitCompiler::clear()` 只清空自身状态(IR 缓存、外部节点集合),未调用 `ExecutionSession::endSession()` / `JITDylib::dispose()`,ORC 资源泄漏到下一个 `Simulator` 实例。

**当前代码状态**(`include/jit/jit_compiler.h:37-103`):
- `JitCompiler` 类持有 `void* jit_session_`(封装 `ExecutionSession`)、`void* llvm_module_`(封装 `llvm::Module`)、`void* compiled_func_/compiled_comb_func_/compiled_seq_func_`
- 用 `void*` 是为了不把 LLVM 头泄漏到公共头(只在 `src/jit/jit_compiler.cpp` 内部 include)
- `JitCompiler::clear()`(`src/jit/jit_compiler.cpp:167`)清空 IR 缓存,但**未触及 ORC 资源**
- `Simulator::~Simulator()`(`src/simulator.cpp:103-115`)调用 `disconnect()` 并释放 `trace_blocks_`,**未显式 `jit_compiler_.reset()`**

**JIT MIN_NODES 阈值**:5(`include/jit/jit_compiler.h:46`,W2 修复),depth=10 的小图仍走 JIT 路径。

**Stakeholders**:
- JIT 路径使用者(开发 perf benchmark、ax4 interconnect、riscv-mini)
- 仿真器维护者(需要理解 Simulator ↔ JitCompiler 生命周期耦合)
- CI 守门员(`perf_regression` ctest)

## Goals / Non-Goals

**Goals:**
- 跨 `ch_device<T>` 实例的 JIT 状态完全隔离 — 第二个实例的 JIT `sim_us` 应与 standalone 一致(4-4.5k us 级,无 437k+ 污染)
- `JitCompiler` 析构时显式释放所有 ORC 资源,不留悬挂引用
- `Simulator` 析构时显式触发 `JitCompiler` 清理(不依赖 `unique_ptr` 隐式顺序)
- 修复后 `perf_regression` 阈值从 4.0× 恢复为 1.6×,`tests/jit/test_jit_orc_state_isolation` 加入 ctest

**Non-Goals:**
- 不重写 `JitCompiler::compile()` / `generate_ir()` / `compile_to_llvm()` 主流程
- 不动 `JIT_MIN_NODES` 阈值
- 不重生成 `perf_baseline.json`(独立 sub-task,archive 阶段由 Prometheus 决定)
- 不实现子进程隔离(F2,独立 change)
- 不动 `jit_ir_gen_lnode_state.cpp`(ch_reg 路径已验证为 native)
- 不动 `kBackendThresholds["interpreter"]` 与 `["verilator"]`
- 不实现 JIT 缓存层(可能后续优化)

## Decisions

### Decision 1: 在 `JitCompiler::clear()` 内部释放 ORC 资源,不在新方法中分散

**Rationale**: `clear()` 已经是析构函数唯一调用的清理入口(`src/jit/jit_compiler.cpp:78` `~JitCompiler() { clear(); }`),把 ORC 释放放进去保证析构路径与显式清理路径行为一致。新增独立方法会导致双路径分叉,易漏调。

**Alternatives considered**:
- (A) 新增 `~JitCompilerImpl()` PIMPL idiom — 工作量大,需要拆分类;**拒绝**:本 change 范围只想修状态泄漏,非架构重构
- (B) 在 `JitCompiler` 内持有 `std::unique_ptr<ExecutionSession, custom_deleter>` — 模板复杂,需前向声明 LLVM 类型;**拒绝**:与现有 `void*` 封装模式冲突
- (C) 选用本方案 — 在 `clear()` 内 `static_cast<llvm::orc::ExecutionSession*>(jit_session_)->endSession()`,然后 `dispose()` `JITDylib`;**接受**:改动局部,符合现有架构

### Decision 2: 在 `Simulator::~Simulator` 显式 `jit_compiler_.reset()`

**Rationale**: `unique_ptr` 的析构顺序依赖成员声明顺序。当前 `jit_compiler_` 是 `std::unique_ptr<JitCompiler>`,会在 `Simulator` 自身析构时自动销毁。问题是 `disconnect()` 与 `jit_compiler_` 销毁顺序不保证 — 若 `disconnect()` 触发了 `data_map_` 修改,可能在 ORC 释放后访问悬空资源。显式 `reset()` 把顺序确定化。

**Alternatives considered**:
- (A) 调整成员声明顺序,让 `jit_compiler_` 先于 `disconnect()` 涉及的成员析构 — **拒绝**:隐式,不可见,易回归
- (B) 在 `disconnect()` 内显式 `jit_compiler_.reset()` — **拒绝**:`disconnect()` 是正常路径也调用的,不应有副作用
- (C) 在析构函数顶部 `jit_compiler_.reset()` 显式调用 — **接受**:可见、可审计、局部改动

### Decision 3: 保持 `void*` 封装,不在公共头引入 LLVM 前向声明

**Rationale**: 当前架构故意不把 LLVM 类型泄漏到 `include/jit/jit_compiler.h`(per `include/jit/AGENTS.md` 隐含约定 — header-only 库,LLVM 是 build-time only)。改动 `JitCompiler` 类持有 `void*` 的设计会污染公共 API。

**Alternatives considered**:
- (A) 引入 PIMPL idiom(`JitCompiler` 持 `std::unique_ptr<Impl>`,Impl 定义在 `src/jit/`) — **接受度评估**:变更较大但隔离彻底;**保留为未来重构候选**,本 change 不做
- (B) 把 `void*` 改为 `llvm::orc::ExecutionSession*` 前向声明 — **拒绝**:需要 include LLVM 头,违反 header-only 原则
- (C) 维持 `void*` 封装,所有 ORC 操作在 `src/jit/jit_compiler.cpp` 内 `static_cast` — **接受**:本 change 选用

### Decision 4: 用新测试 `tests/jit/test_jit_orc_state_isolation.cpp` 显式覆盖污染场景

**Rationale**: `perf_regression` 阈值变化是间接信号,W1/W2 历史反复出现"阈值调了又调"的循环。**显式回归测试**是根治的契约:第二个 `ch_device<T>` 的 JIT 性能不应劣于第一个的 1.5×。

**Test design**:
```cpp
TEST_CASE("JIT ORC state isolation between ch_device instances", "[jit][isolation]") {
    ch::core::context ctx_a("a"); ch::core::ctx_swap swap_a(&ctx_a);
    ch::ch_device<Tc07XorChain> dev_a;
    ch::Simulator sim_a(dev_a.context());
    sim_a.set_jit_enabled(true);
    auto median_a = bench(dev_a, /* ticks */ 1000);
    // a 析构触发 ORC 释放

    ch::core::context ctx_b("b"); ch::core::ctx_swap swap_b(&ctx_b);
    ch::ch_device<Tc08RegChain> dev_b;  // 不同 DUT
    ch::Simulator sim_b(dev_b.context());
    sim_b.set_jit_enabled(true);
    auto median_b = bench(dev_b, /* ticks */ 1000);

    REQUIRE(median_b < median_a * 1.5);  // 不应有 100× 污染
}
```

**注册位置**:`tests/jit/CMakeLists.txt` 新建,`add_catch_test(test_jit_orc_state_isolation jit/test_jit_orc_state_isolation.cpp)`,`LABELS "jit"`。

## Risks / Trade-offs

| Risk | Mitigation |
|------|-----------|
| `ExecutionSession::endSession()` / `JITDylib::dispose()` 调用顺序错误,导致 LLVM 段错误 | 在 `clear()` 中按 LLVM 文档推荐顺序:`endSession()` → `dispose()` 所有 `JITDylib` → 释放 `llvm::Module*` → 重置 `void*` 字段;新增的回归测试会捕获此类崩溃 |
| `void*` 转型到 LLVM 类型的 `static_cast` 在某些编译配置下不安全 | 维持 `#if defined(CH_JIT_ENABLED) && __has_include(...)` 守卫,与现有 `llvm_module_` 字段保持一致 |
| 析构时 ORC 资源清理可能与正在执行的 JIT 函数调用冲突 | `Simulator` 析构时 `jit_compiler_.reset()` 在 `disconnect()` 之前,`disconnect()` 已停止所有 tick 调度,无活跃调用 |
| 修复后 `perf_regression` 阈值恢复 1.6×,可能暴露之前被 4.0× 掩盖的真实回归 | 跑全部 141 ctest + 28 examples,确认无回归;若发现,记录为 follow-up issue(不阻塞本 change) |
| `JitCompiler::clear()` 被多次调用(析构 + 显式 clear)时 ORC 释放重复执行 | 释放后置 `void*` 字段为 `nullptr`,后续调用检查指针为空时直接 return |
| `JIT_MIN_NODES=5` 阈值与 K1 污染叠加时的 TC-07 depth=10 当前已是 polluted 数据 | K1 修复后 baseline.json 的 TC-07 depth=10 数据需要重新生成;**archive 阶段处理**,不在本 change scope |

## Migration Plan

**阶段 1:实现** (估计 4-6 小时)
1. 修改 `src/jit/jit_compiler.cpp` `clear()` 实现:按顺序 `endSession()` → `dispose()` → 释放 module → reset `void*` 字段
2. 修改 `src/simulator.cpp` `Simulator::~Simulator()` 顶部:显式 `jit_compiler_.reset()`
3. 新建 `tests/jit/test_jit_orc_state_isolation.cpp` + `tests/jit/CMakeLists.txt`
4. 跑 `cmake --build build -j$(nproc)`,确认 0 errors 0 warnings

**阶段 2:验证**
1. 跑 `ctest -L base --output-on-failure` — 确认全部 141 测试通过
2. 跑 `ctest -L chlib --output-on-failure` — 确认 chlib 测试通过
3. 跑 `./run_all_ported_tests.sh` — 确认 28 个 examples 通过
4. 跑新测试 `ctest -R test_jit_orc_state_isolation --output-on-failure` — 确认通过
5. 跑 `ctest -R perf_jit_smallgraph` — 确认 JIT 路径不退化
6. 跑 `ctest -R perf_jit_vs_interp_ratio` — 确认结构性 gate 仍通过

**阶段 3:回归 F1**
1. 验证通过后,修改 `tests/benchmark/perf_regression.cpp:141` 把 JIT 阈值从 4.0× 恢复为 1.6×
2. 删除 2026-06-19 添加的 K1 临时注释(`// JIT threshold is TEMPORARILY raised...`)
3. 跑 `ctest -R perf_regression` — 确认 0 回归(此时 current 数据应反映干净 ORC 状态,baseline 是 clean state 捕获,直接可比)

**阶段 4:文档同步**
1. `docs/developer_guide/tech-reports/perf-report-todo.md`:
   - W1 描述更新:K1 已根治,标"已修复 [date]"
   - W12 标"✅ 完成",链接本 change
2. `docs/simulation/PERF_COMPARISON_REPORT.md` §6 K1:加"修复状态:已根治 by fix-jit-orc-state-leak"
3. `include/jit/AGENTS.md` 添加新规则"JitCompiler 析构必须释放 ORC 资源" + 交叉引用本 change

**Rollback strategy**:
- 单一 commit revert 即可恢复全部代码改动
- 临时回滚:JIT 阈值改回 4.0× + 新测试用 `#if 0` 包裹(避免 CI 失败)
- 最坏情况:若 LLVM ORC API 在某版本上行为不一致,可能需要锁定 LLVM 版本范围(写进 `cmake/FindLLVM.cmake`)

## Open Questions

1. **LLVM ORC API 版本兼容性**:`ExecutionSession::endSession()` 与 `JITDylib::dispose()` 在 LLVM 14/15/16/17 上的签名是否一致?需在实现前对照当前项目锁定的 LLVM 版本(看 `cmake/FindLLVM.cmake` 找 `find_package(LLVM REQUIRED)` 的版本范围)验证。**责任**:实施人;**阻塞**:是,若 API 不兼容需重新设计
2. **是否需要把 `JITDylib` 列表显式跟踪**:目前 `JitCompiler` 似乎只持一个 `JITDylib`(per `jit_session_` 的封装模式),但若 `compile()` 过程中创建了多个 dylib,析构时需要遍历所有。需在实现前 grep `src/jit/` 确认 `JITDylib` 实例化点数量。**责任**:实施人;**阻塞**:否,极端情况用 `llvm::orc::ExecutionSession`'s internal tracking 兜底
3. **JIT 缓存层是否在本 change 之后作为 follow-up**:K1 修复后,跨 `ch_device<T>` 的 IR 缓存复用成为可能(目前因污染被清空)。是否需要单独 change 引入缓存?**责任**:架构师决定;**非本 change 阻塞**
4. **perf_regression baseline.json 何时重生成**:本 change 后 current 数据是干净的,但 baseline 是 clean state 捕获,需要重生成以建立新基线。**建议**:archive 阶段(本 change 落地后)运行 `./build/tests/benchmark/perf_tests --all` 一次,生成新 `perf_baseline.json` 并作为 archive commit 的一部分提交

## Cross-References

- **Proposal**:`proposal.md`
- **Spec**:`specs/jit-compiler-lifecycle/spec.md`
- **Tasks**:`tasks.md`
- **关联文档**:
  - `docs/simulation/PERF_COMPARISON_REPORT.md §6 K1`(污染权威依据)
  - `docs/developer_guide/tech-reports/perf-report-todo.md` W1/W12(本 change 直接解决)
  - `include/jit/AGENTS.md`(JIT 规则,本 change 完成后需追加析构规则)
  - `docs/developer_guide/patterns/JIT-DEBUGGING-GUIDE.md`(调试参考)
- **F1 临时方案**:`tests/benchmark/perf_regression.cpp:139-156` 4.0× 阈值(本 change 完成后恢复 1.6×)
