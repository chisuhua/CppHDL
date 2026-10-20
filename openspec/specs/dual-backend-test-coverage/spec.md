# Capability: dual-backend-test-coverage

## Purpose

定义 CppHDL 单元测试统一跑 InterpreterBackend 与 VerilatorBackend 的契约：通过 `DUAL_BACKEND_TEST` macro + `add_dual_backend_test` CMake function，让**任何可观测顶层 `ch_out` 端口、且具有 `ch_device` / `ch_module` 形态的设计**自动走两套 backend，逐周期比较 `ch_out` 端口值，使得 codegen 错误与底层 eval 错误在测试时即时暴露，而非延迟到 nightly / nightly-rv32i 集成时才发现。

In English: Defines the contract for CppHDL unit tests uniformly exercising both InterpreterBackend and VerilatorBackend through a `DUAL_BACKEND_TEST` macro + `add_dual_backend_test` CMake function. The macro MUST automatically construct two `ch_device<T>` instances (one per backend), run the same per-cycle assertions across both, and surface codegen or eval errors at test-time rather than during nightly RV32I integration.
## Requirements
### Requirement: DUAL_BACKEND_TEST macro MUST 钉死 InterpreterBackend 与禁用 JIT

`tests/dual_backend_fixture.h` 提供的 `DUAL_BACKEND_TEST(DutType, name, tags)` macro MUST 在 interpreter 那一遍**同时**调用 `sim.set_jit_enabled(false)` 与 `sim.set_backend(std::make_unique<InterpreterBackend>())`，以确保该 path 真正走 InterpreterBackend 而非 inlined JIT。

#### Scenario: InterpreterBackend 显式 pin + JIT 禁用

- **WHEN** TEST_CASE 通过 `DUAL_BACKEND_TEST` macro 执行
- **THEN** interpreter 一侧的 `Simulator` 实例 MUST 调用 `set_jit_enabled(false)` 与 `set_backend(make_unique<InterpreterBackend>())`
- **AND** 即使 `CH_JIT_ENABLE=ON`（项目默认），该 TEST_CASE 的 interpreter 侧也不走 JIT
- **NOTE**: 失败归因时排除 JIT 干扰（per `verilator-rv32i-e2e` spec §R2 已锁定此契约；`Simulator::backend_->is_native()==false` 仍允许 simulator 内部走 inlined 分支，必须用 `set_jit_enabled(false)` 双保险）

### Requirement: DUAL_BACKEND_TEST macro MUST 构造两份独立 ch_device

`DUAL_BACKEND_TEST` macro MUST 为两套 backend 各构造一个独立 `ch::ch_device<DutType>` 实例（共享同一 fixture class 但不同 context 实例），即 lambda body 内 `(sim_a, dev_a.instance(), BackendKind::Interpreter)` 与 `(sim_b, dev_b.instance(), BackendKind::Verilator)` 是**两个独立的 runtime hierarchy**。Macro MUST 不复用单一 `ch_device`，因为 `data_map_` 与 `node_id` 是 context-local 的，无法跨 backend run 复用 port 引用。

#### Scenario: 双 device 强制

- **WHEN** TEST_CASE 跑 `DUAL_BACKEND_TEST(MyDut, ...)`
- **THEN** macro 内部 MUST 构造 `ch_device<MyDut> dev_a` 与 `ch_device<MyDut> dev_b` 两个独立实例
- **AND** `dev_a.context()` 与 `dev_b.context()` 必须是两个独立 context 指针
- **AND** lambda body 收到的 `(sim, dev.instance())` 是同一个 device 的两条路径之一

#### Scenario: 复用单一 device 编译期失败

- **WHEN** reviewer 检查 `tests/dual_backend_fixture.h` 源码
- **THEN** macro 展开体 MUST NOT 出现 `dev_a = std::move(dev_b)` 或类似路径下的 share-context 代码

### Requirement: DUAL_BACKEND_TEST macro MUST 端口值比较（per-cycle parity）

Macro 内部的 verilator 那一遍**逐周期**调用 `sim_b.tick(N)` 同时**逐周期**对相同 cycle 数执行 lambda body 中的 `REQUIRE(get_port_value(ch_out_port) == closed_form)` 断言。Macro MUST 不在 lambda 结束后才"事后比对完整 trace"，因为现有单元测试大量是中途 `REQUIRE`（stream handshake / FIFO 状态 / AXI 写读），仅比较终值 trace 会漏掉时序 bug。

#### Scenario: per-cycle REQUIRE 触发端口值比较

- **WHEN** lambda body 包含 `sim.set_input_value(io().a, X); sim.tick(); REQUIRE(get_port_value(io().sum) == X+1);`
- **THEN** macro 在 interpreter 侧跑 1 cycle，断言 sum == X+1
- **AND** macro 在 verilator 侧跑 1 cycle，独立断言 sum == X+1
- **AND** 任一侧失败，整 TEST_CASE 失败（不中止另一侧，保证 Catch2 输出两端日志）

#### Scenario: 事后 trace 比对禁止

- **WHEN** reviewer 检查 `tests/dual_backend_fixture.h`
- **THEN** macro MUST NOT 收集 `data_map_` 完整 trace 后做 `sim_a.data_map() == sim_b.data_map()` 大块比较
- **AND** 每周期断言必须由 lambda body 自带，不依赖 macro 隐式收集

### Requirement: Verilator 缺席 macro MUST log-and-skip（不全 SKIP TEST_CASE）

`DUAL_BACKEND_TEST` macro 在 `verilator` 不在 `PATH` 时 MUST 走"interpreter 单独跑 + `WARN` 一行 + 不调用 `SKIP()`"的降级路径，而非 `SKIP("verilator not on PATH")`。原因：interpreter 侧的端口值断言仍贡献有效覆盖，全 `SKIP` 会让 Catch2 返回 exit code 4（ctest 判失败）。

#### Scenario: verilator 缺失时降级为 interpreter-only

- **WHEN** `command -v verilator` 返回非 0 退出码
- **AND** TEST_CASE 跑 `DUAL_BACKEND_TEST`
- **THEN** macro MUST 输出 `WARN("verilator not on PATH; interpreter half only")` 一行（用 Catch2 `WARN` 非 `FAIL`）
- **AND** macro MUST NOT 调用 `SKIP()` 整个 TEST_CASE
- **AND** interpreter 侧的 lambda body MUST 继续执行并提供断言覆盖

#### Scenario: verilator 缺失时 Catch2 exit code 0

- **WHEN** 单一 TEST_CASE 由 `DUAL_BACKEND_TEST` 包装且 verilator 缺失
- **THEN** ctest 调用该可执行文件退出码 MUST == 0（不是 4）
- **NOTE**: 这避免 `test_verilator_trace_parity.cpp:508-518` 中 `CATCH_AMALGAMATED_CUSTOM_MAIN` 模式的额外复杂度

### Requirement: macro MUST 对 >64-bit 端口设计 log-and-skip verilator half

`DUAL_BACKEND_TEST` macro 在 verilator 初始化**前** MUST 扫描 fixture 的 `ch_out` 端口宽度，发现任一端口 `N > 64` 时 WARN + 跳过 verilator 那一半（interpreter 仍跑全）。原因：VerilatorBackend 的 `build_port_access_table` 对 `N > 64` 端口 `return false`（`include/core/verilator_backend.h:40`），与 `verilator-backend-e2e` spec §"port bitwidth limit: 64 bits" 契约一致。

#### Scenario: >64-bit 端口设计降级

- **WHEN** fixture 顶层 `ch_out<ch_uint<128>>` 端口
- **AND** TEST_CASE 跑 `DUAL_BACKEND_TEST`
- **THEN** macro MUST 输出 `WARN("fixture has ch_out wider than 64-bit; skipping verilator half")`
- **AND** verilator 那一半 MUST NOT 调用 `backend->initialize()`
- **AND** interpreter 那一半 MUST 完整执行

#### Scenario: ≤64-bit 端口设计 dual-run

- **WHEN** fixture 顶层 `ch_out<ch_uint<32>>` 端口
- **THEN** macro MUST 完整跑两遍（interpreter + verilator）
- **NOTE**: 与 ADR-035 64-bit cap 一致；fixture 重设计（如降为 `ch_uint<64>` 或 split-then-merge）超出本 capability scope

### Requirement: 适用范围排除清单 MUST 写明不可 dual 的测试类别

Capability MUST 明确划分"不可 dual"测试类别，避免迁移工作浪费与无意义的"dual 失败"误判。

#### Scenario: 不可 dual 测试类别

- **WHEN** reviewer 阅读本 capability spec 的排除清单
- **THEN** 下列测试类别 MUST 标注为 "interpreter-only"，不得 dual：
  1. **纯编译期 / 静态断言测试**（如 `test_reg.cpp` 的 `STATIC_REQUIRE` 块、concept 测试、`test_precise_width` / `test_source_info` / `test_bv_cmp` / `test_bitvector` 等无 Simulator 调用的）
  2. **裸 context 测试**（如 `tests/chlib/test_arithmetic.cpp:13-39` 直接在裸 context 构图，无 `ch_device` / `ch_module` / `ch_out` 端口）
  3. **aread-based 设计**（与 `mem-read-timing` spec §"aread/sread timing skew" 契约冲突，verilator 0-cycle 与 simulator posedge 设计 drift 导致必然 parity 失败）
  4. **>64-bit 端口设计**（VerilatorBackend 物理拒收）
  5. **Catch2 SECTION 重入测试**（macro 双 run 结构下 SECTION 会各跑一次，行为语义不一致）
  6. **STATIC_REQUIRE / 编译期元函数测试**（无 runtime 语义）

#### Scenario: 排除清单由 spec 引用 grep 模式定义

- **WHEN** builder 阶段执行"分类扫描"以填充测试名单
- **THEN** MUST 用下列 grep 命令锁定可 dual 候选：
  ```
  grep -l "ch_device\|ch_module" tests/test_*.cpp tests/chlib/test_*.cpp | \
    xargs grep -L "STATIC_REQUIRE" | \
    xargs grep -L "aread" | \
    xargs grep -L "ch_out<ch_uint<[0-9]\{3,\}>>"
  ```
- **AND** 输出名单中"`grep -L aread`"环节**未排除** dmem stub（dmem stub 是 stub-wide 设计，按 cpu-cache-subsystem spec §R5 分类处理）

### Requirement: add_dual_backend_test CMake function MUST 注入 verilator 属性

`tests/CMakeLists.txt` MUST 新增 `add_dual_backend_test(test_name src1 [src2...])` CMake function，内部组合：
- `add_catch_test(test_name src1 [src2...])` 保留现有 base LABELS
- `set_tests_properties(test_name PROPERTIES LABELS "verilator_e2e" RESOURCE_LOCK "verilator_backend" ENVIRONMENT "${_vl_path_env}")`
- 接受可选 `TIMEOUT` kwarg，默认 60s（trace_parity 用 300s, complex_e2e 用 600s，由各测试单独 override）

注：现有 `add_catch_test` 函数不支持 `RESOURCE_LOCK` / `ENVIRONMENT` / `TIMEOUT` 参数，必须由 `add_dual_backend_test` 单独包装。

#### Scenario: CMake function 签名稳定

- **WHEN** 阅读 `tests/CMakeLists.txt` 中的 `add_dual_backend_test` 定义
- **THEN** 函数签名 MUST 接受 `test_name` 必选 + 可变 source files + 可选 `TIMEOUT` kwarg
- **AND** 内部 MUST 调 `add_catch_test(${test_name} ${ARGV})` 之外附加 set_tests_properties 三件套

#### Scenario: 所有 verilator_e2e 测试共享全局资源锁

- **WHEN** 10 个 dual-backend 测试通过 `add_dual_backend_test` 注册
- **THEN** 它们共享 `RESOURCE_LOCK "verilator_backend"`（与现有 8 个 verilator 测试的锁同名）
- **AND** ctest 调度器 MUST 保证同一时刻仅 1 个 verilator 测试在 dlopen/编译（避免同 SHA-1 cache 冲突）
- **NOTE**: 此锁串行化导致墙钟估算需实测，文档禁止拍脑袋数字

### Requirement: 现有 9 个 verilator 测试 MUST 保留原状

`tests/test_verilator_trace_parity.cpp` / `tests/test_verilator_bundle_e2e.cpp` / `tests/test_verilator_stream_e2e.cpp` / `tests/test_verilator_memory_e2e.cpp` / `tests/test_verilator_complex_e2e.cpp` / `tests/test_verilator_e2e_harness.cpp` / `tests/test_axi_lite_verilator_e2e.cpp` / `tests/test_verilator_backend.cpp` / `tests/test_verilator_error_flow.cpp` 9 个测试 MUST 保留原状，不迁移到 `DUAL_BACKEND_TEST` macro。理由：
1. 它们是 macro 的 **reference implementation**，将 macro 与现有 hand-crafted 代码漂移会破坏 pattern 传承
2. `verilator-e2e-coverage` / `verilator-backend-e2e` / `verilator-rv32i-e2e` 等既有 spec 已锁定其 file-level / count-level requirements
3. 这些测试已配置 `CATCH_AMALGAMATED_CUSTOM_MAIN` 与个性化 `main()`（verilator 缺失时 return 0），迁移会导致既有 TIMEOUT 与 exit-code 契约二次破坏

#### Scenario: 9 个测试保留原状

- **WHEN** 本 capability 的 builder 阶段执行
- **THEN** 上述 9 个文件的 `git diff` MUST 仅限"加 `LABELS verilator_e2e` 标签 + 在文件顶注释声明 `Reference implementation for DUAL_BACKEND_TEST macro`"两类变更
- **AND** 测试体、TEST_CASE 名、Catch2 SECTION、add_catch_test / set_tests_properties 调用 MUST NOT 变化

#### Scenario: 全 9 个测试贴上 verilator_e2e label

- **WHEN** `ctest -N -L verilator_e2e` 执行
- **THEN** 输出 MUST 包含这 9 个测试 + 所有用 `add_dual_backend_test` 注册的新测试
- **AND** 总数 MUST ≥ 12（9 旧 + ≥ 3 新 Phase 1 target）
- **NOTE**: 本 capability 落地后，label `verilator_e2e` 是统一对外标签，取代现有各种临时 label

### Requirement: CTEST_TOTAL 同步 MUST 在 phase 边界更新根 AGENTS.md

每次新测试 target 通过 `add_dual_backend_test` 注册，MUST 按根 `AGENTS.md §NOTES` 的 `CTEST_TOTAL: <N>` 行更新计数（与 `scripts/verify_test_counts.sh` 同步）。Phase 1 边界 `CTEST_TOTAL` 数字 MUST 由实测 `ctest -N | tail -1` 回填（禁止拍脑袋）。

#### Scenario: CTEST_TOTAL 数字不漂移

- **WHEN** builder 阶段完成 phase 1 注册（macro + 10 个基础 test 迁移）
- **THEN** MUST 执行 `cd build && ctest -N | tail -1` 并记录实际数字
- **AND** MUST 更新根 `AGENTS.md §NOTES` 的 `CTEST_TOTAL: <N>` 行
- **AND** MUST 同步 `.github/workflows/ci.yml` 中的 `verify_test_counts.sh` 阈值（若有）

#### Scenario: verify_test_counts.sh 不报错

- **WHEN** `bash scripts/verify_test_counts.sh` 在 archive 前跑
- **THEN** exit code MUST == 0
- **NOTE**: 这是 CI 硬门，archive 前必跑

### Requirement: 64-bit 端口边界 / 2-step 时钟 / reset pulse 专项 fixture MUST 落地

Phase 1 之后，至少 3 个专项 fixture MUST 存在以固化 verilator backend rules：

1. **64-bit 端口边界 fixture**：观测 fixture 顶层含一个 `ch_out<ch_uint<64>>` 端口跑 dual-backend，验证 ADR-035 64-bit cap 边界恰好走通；并在同一 target 加一个 `[verilator-skip-wide]` 案例验证 >64-bit 降级路径（与 R5 互补）
2. **2-step 时钟 toggle fixture**：simple counter 跑 1 cycle，验证 `sim.tick()` 后 `perf_instructions` / `out` 字段在两后端一致（与 `verilator-backend-e2e` §"3-step clock toggle" 契约互补）
3. **default_reset pulse fixture**：counter 设 `INIT_d = 42`，跑 dual-backend 验证 cycle 1 后 counter == 43（与 `verilator-reg-init` spec §R1 契约互补）

#### Scenario: 3 专项 fixture 全部注册

- **WHEN** `ctest -R "dual_backend_64_bit|dual_backend_clock|dual_backend_reset" -N` 执行
- **THEN** 输出 MUST ≥ 3 个 test target
- **AND** 全部 `LABELS verilator_e2e`

### Requirement: cache 双后端 MUST 按 Sync/Async 分类（Sync 走 parity / Async 走 golden-value）

cache 双后端集成时 MUST 区分 `ICacheSync` / `DCacheSync`（1-cycle sread，default）与 `ICacheAsync` / `DCacheAsync`（0-cycle aread）：
- **Sync** dual-run 走 parity（verilator 与 interpreter 逐周期一致）
- **Async** dual-run 走 golden-value（verilator 跑 N+1 cycle 后断言终值与 interpreter 独立跑 M cycle 后断言一致，n 与 m 反映 aread-vs-sread latency skew，per `mem-read-timing` spec）

理由：`mem-read-timing` spec §"aread 0-cycle vs posedge 1-cycle skew" 已锁定 Async 设计在两后端有设计 drift，强制 parity 会导致 Async 测试永远红。

#### Scenario: Sync cache test 走 parity

- **WHEN** fixture 用 `ICacheSync`（同步 L1 imem）
- **AND** TEST_CASE 跑 `DUAL_BACKEND_TEST`
- **THEN** macro MUST 逐周期比对 `data` / `miss` / `ready` 等顶层端口
- **AND** 填充完成后 hit 路径两后端 MUST 在同 cycle 反映缓存数据

#### Scenario: Async cache test 走 golden-value

- **WHEN** fixture 用 `ICacheAsync`（TCM-style 异步）
- **AND** TEST_CASE 必须用 `DUAL_BACKEND_GOLDEN_TEST`（独立 macro or `DUAL_BACKEND_TEST` 的 `[backend-shift-N]` tag）
- **THEN** MUST 跑 verilator N+1 cycle 后断言终值
- **AND** MUST 跑 interpreter M cycle 后断言终值（M != N，反映 latency skew）
- **AND** MUST NOT 强制逐周期比对（would-be 红的设计 drift 视为已知 non-issue）

### Requirement: PR 阶段 dual-backend feedback time MUST 实测

CI PR matrix 落地后，所有 dual-backend tests 在 PR feedback cycle MUST 实测墙钟。当前 base 测试 PR feedback 5-8 min，加载 dual-backend test 后总墙钟 MUST ≤ 45 min（per §11.2 decision A "PR 强制 verilator"）。

#### Scenario: 实测基线记录

- **WHEN** Phase 1 完工后 builder 跑 `ctest -L verilator_e2e -LE perf --output-on-failure --schedule-random`
- **THEN** MUST 记录 wall-clock 秒数至本 spec Appendix（本 capability 落地后由 reviewer 验证墙钟可接受）

#### Scenario: PR feedback 时间不爆 45 min

- **WHEN** Phase 1 完 PR 模拟 (`time ctest -L verilator_e2e -LE perf`)
- **AND** wall-clock > 45 min
- **THEN** 必须**自动降级** phase 2-5 拆分（不动 Phase 1 基础 test 数量），仅延后 4 个后续 phase 的双后端扩展

### Requirement: ci.yml PR-verilator-fast job MUST 显式改动

为实现 §11.2 decision A（PR 强制 verilator），builder 阶段 MUST 修改 `.github/workflows/ci.yml`：
- 新增 `pr-verilator-fast` job（与现有 `pr-build` 并列），触发条件 `pull_request`
- 该 job 调用 `cmake -B build -DBUILD_VERILATOR=ON` + `cmake --build build -j$(nproc)` + `ctest -L verilator_e2e -LE perf`
- 默认 timeout 45 min（与 R10 一致）
- 现有 nightly 仍跑完整 dual-backend + perf（不破坏）

#### Scenario: ci.yml PR-verilator-fast job 注册

- **WHEN** 阅读 `.github/workflows/ci.yml` 改动后
- **THEN** MUST 出现 `pr-verilator-fast:` job definition
- **AND** 该 job 必含 `BUILD_VERILATOR=ON` cmake flag
- **AND** 该 job 必调用 `ctest -L verilator_e2e -LE perf`

#### Scenario: 双 job 并存不互相阻塞

- **WHEN** PR 触发了 `pr-build` 与 `pr-verilator-fast` 两个 job
- **THEN** 两个 MUST 平行运行（不互相 wait）
- **AND** 任一失败 MUST 触发 PR review-blocker

### Requirement: cache Sync runtime test SHALL 走 DUAL_BACKEND_TEST_PAIR macro

`tests/chlib/test_cache_sync.cpp` MUST 使用 `tests/dual_backend_fixture.h::DUAL_BACKEND_TEST_PAIR` macro，让 `ICacheSync` / `DCacheSync` 每个 TEST_CASE 都跑 interpreter + verilator 双后端。**不允许**用裸 `TEST_CASE` 单独跑 interpreter；**不允许**手写 dual half（避免与 macro 漂移）。

#### Scenario: macro invocation 计数

- **WHEN** 在 `tests/chlib/test_cache_sync.cpp` grep `DUAL_BACKEND_TEST_PAIR`
- **THEN** 必须 ≥ 9 个 macro invocation（5 ICacheSync + 4 DCacheSync）

#### Scenario: register 接口约束

- **WHEN** 测试用 `sim.set_input_value(io().port, value)` 和 `sim.get_port_value(io().port)`
- **THEN** 不允许直接访问 `sim.data_map()[node_id]`（与 macro R3 per-cycle REQUIRE 契约一致）；不允许用 `node_id` 跳过 ch_out 接口

#### Scenario: spec R6 排除清单遵守

- **WHEN** runtime test 涉及 `ICacheAsync` 或 `DCacheAsync`
- **THEN** MUST NOT 使用 `DUAL_BACKEND_TEST_PAIR`（Async variant 是 0-cycle aread，命中 R6 #3，verilator 0-cycle vs simulator posedge 1-cycle skew）；runtime test 仅适用 Sync variant（1-cycle sread，RV32I L1 默认）

### Requirement: cache Sync runtime test MUST 用 Sync variant 而非 stub

`tests/chlib/test_cache_sync.cpp` SHALL 实例化 `chlib::ICacheSync` 和 `chlib::DCacheSync`（来自 `include/cpu/cache/i_cache_sync.h` 与 `d_cache_sync.h`，ADR-041 双变体之一）。**不允许**实例化 `cpu/cache/i_cache_stub.h` 或 `d_cache_stub.h`（stub 是 ADR-037 待退役对象，2027-04-17 deadline）。允许 `#pragma GCC diagnostic ignored "-Wdeprecated-declarations"` 包裹（与 `tests/chlib/test_cache_pipeline_integration.cpp` 一致），但不进 stub 头文件。

#### Scenario: include 头文件约束

- **WHEN** runtime test 用 `#include "cpu/cache/i_cache_sync.h"` 和 `#include "cpu/cache/d_cache_sync.h"`
- **THEN** 不允许 `#include "cpu/cache/i_cache_stub.h"` 或 `#include "cpu/cache/d_cache_stub.h"`

#### Scenario: instance 类型约束

- **WHEN** runtime test 用 `ch::ch_device<ICacheSync>` 和 `ch::ch_device<DCacheSync>`
- **THEN** 不允许 `ch::ch_device<ICache>` (stub 别名) 或 `ch::ch_device<DCache>` (stub 别名)

### Requirement: AXI4 peripheral behavior test SHALL 走 DUAL_BACKEND_TEST_PAIR

`tests/test_axi_<peripheral>_behavior.cpp` MUST 用 `tests/dual_backend_fixture.h::DUAL_BACKEND_TEST_PAIR` macro
（`_behavior` suffix 区别原 `test_axi_<peripheral>.cpp` 接口结构测试）。

该 Requirement 的 peripheral 列表 MUST 与实际存在 `_behavior` 测试的 AXI4-Lite slave
一一对应：`AxiLiteSpi` / `AxiLitePwm` / `AxiLiteI2c` / `AxiLiteTimer` / `AxiLiteGpio` /
`AxiLiteUart`。

`AxiDma`（AXI4 **master**，非 AXI4-Lite slave，复位契约不同）不在本 Requirement 范围；
其 dual 化尚未排期（`axi4-peripheral-followups-2026-10` 已 archive，未包含该测试）。

原 `tests/test_axi_<peripheral>.cpp` MUST NOT 修改（interpreter-only baseline）。

#### Scenario: file 命名约束

- **WHEN** 新增 AXI4 peripheral behavior runtime test
- **THEN** 文件名 MUST 用 `_behavior` suffix 区别原 `test_axi_<peripheral>.cpp`
- **AND** 原 `test_axi_<peripheral>.cpp` MUST NOT 修改

#### Scenario: peripheral 列表与 `_behavior` 文件一一对应

- **WHEN** 枚举 `tests/test_axi_*_behavior.cpp` 的覆盖对象
- **THEN** 结果 MUST 与上述列表完全一致（6 个 AXI4-Lite slave）
- **AND** 列表 MUST NOT 含无 `_behavior` 测试的类（如 `AxiDma`）

#### Scenario: macro 接口契约

- **WHEN** 测试访问 `ch_out<ch_uint<N>>` 等端口值
- **THEN** MUST 用 `sim.set_input_value(io().port, value)` + `sim.get_port_value(io().port)`
- **AND** MUST NOT 用 `port.impl()` 跳过 ch_out 接口（违反 macro R3）

#### Scenario: 依赖前置 design 自洽

- **WHEN** 本 change 启动
- **THEN** 前置 `fix-axi4-peripheral-rst-n-port-2026-10` MUST 已 archive
- **AND** `AxiLiteTimer` / `AxiLiteGpio` / `AxiLiteUart` design MUST 满足规范形态
  （`rst_n` + AW→W 相位分离 + 响应保持）

#### Scenario: 不破既有 spec 锁

- **WHEN** reviewer 检查本 change 的 diff
- **THEN** MUST NOT 改 `tests/test_verilator_*.cpp` 测试体（spec 锁定 macro reference impl）
- **AND** MUST NOT 改 `tests/test_axi_lite_verilator_e2e.cpp`（spec 锁定 reference impl）
- **AND** MUST NOT 改 `tests/test_axi_{spi,pwm,i2c}_behavior.cpp`（已 archived）

### Requirement: AXI4-Lite slave peripheral MUST 收敛到相位分离 + 响应保持的规范形态

`AxiLiteUart` / `AxiLiteTimer` / `AxiLiteGpio` 三个 AXI4-Lite slave MUST 同时满足：

1. 具备 `ch_in<ch_bool> rst_n` 端口（低有效异步复位）；
2. 所有 `ch_reg` MUST 用 `select(!io().rst_n, <init_val>, ...)` 复位 pattern，
   且 `<init_val>` MUST 显式带寄存器位宽（如 `ch_uint<32>(0_d)`）；
3. 具备 `busy` / `aw_done` / `b_valid` / `r_valid` 状态寄存器组，实现 AW→W 相位分离；
4. `bvalid` MUST 在 WDATA 被接受之后拉高并保持到 BREADY；`rvalid` MUST 保持到 RREADY；
5. MUST NOT 使用 `&&` / `||` 逻辑 overload（ADR-036）；
6. 每个 `ch_reg` 在同一文件内 MUST 只有**一处** `X->next =` 赋值；
7. **MUST 在 AW / AR 握手时锁存地址**（见下方 Scenario）。

`AxiDma`（AXI4 master）不在本 Requirement 范围内。

#### Scenario: 三个 peripheral 含 rst_n 端口

- **WHEN** reviewer 阅读 `axi_uart.h` / `axi_timer.h` / `axi_gpio.h` 的 `__io` section
- **THEN** 每个 MUST 含 `ch_in<ch_bool> rst_n;`

#### Scenario: 状态寄存器组与相位分离

- **WHEN** reviewer 阅读三个 peripheral 的 `describe()`
- **THEN** MUST 存在 `aw_done` / `b_valid` / `r_valid` / `busy` 四个 `ch_reg<ch_bool>`
- **AND** `w_handshake` MUST 由 `io().wvalid` 与 `aw_done` 组合得出
  （W MUST 可在 AW 之后的任意周期被接受）
- **AND** `aw_done` MUST 仅在 `b_valid && bready` 时清零
  （MUST NOT 在 `bready` 上直接清零 —— master 常态拉高 BREADY 时会导致 W 永远不被接受）

#### Scenario: 地址锁存

- **WHEN** master 在 AW 握手完成后改写 `AWADDR`（AXI 只要求 AWADDR 保持到 AW 握手）
- **THEN** peripheral MUST 仍把 WDATA 写入 **AW 握手时锁存的地址**指向的寄存器
- **AND** 写侧地址解码 MUST 基于锁存寄存器，MUST NOT 基于活的 `io().awaddr`
- **AND** 读侧同理：`ARADDR` MUST 在 AR 握手时锁存，读数据 MUST 基于锁存地址
- **AND** MUST 存在回归测试覆盖"AW 握手后改写 awaddr，写目标不变"

#### Scenario: 响应保持语义

- **WHEN** master 在 AW/W 握手后撤掉 AWVALID/WVALID，改为等待 BVALID
- **THEN** `bvalid` MUST 保持为 1 直到 master 拉高 BREADY
- **AND** `rvalid` MUST 保持为 1 直到 master 拉高 RREADY
- **AND** MUST NOT 出现 `bvalid` 恒 0 且 `awready` 恒 0 的死锁状态

#### Scenario: 复位语义

- **WHEN** `rst_n` 拉低
- **THEN** 所有 `ch_reg` MUST 在下一周期回到各自 init 值
- **AND** `busy` / `aw_done` / `b_valid` / `r_valid` MUST 回到 0
- **AND** MUST 提供复位逃生路径（事务卡死时可被复位打断）

#### Scenario: 禁止逻辑 overload

- **WHEN** 对 `include/axi4/peripherals/axi_{uart,timer,gpio}.h` 执行
  `grep -nE 'io\(\)\.[a-z_]+ *(&&|\|\|)'`
- **THEN** MUST 命中 0 行
- **AND** 编译 MUST NOT 产生 `-Wdeprecated-declarations`

#### Scenario: 单处 next 赋值

- **WHEN** reviewer 对每个 `ch_reg` 统计 `X->next =` 出现次数
- **THEN** MUST 恰好为 1（禁止后赋值覆盖前赋值 —— 现状 `axi_uart.h` 的 `status_reg` 有 2 处）

#### Scenario: dual backend parity 解锁前置

- **WHEN** 本 change 落地后
- **THEN** `AxiLiteUart` / `AxiLiteTimer` / `AxiLiteGpio` MUST 具备可运行的
  interpreter 侧回归测试（`tests/test_axi_{uart,timer,gpio}.cpp`）
- **AND** `dual-backend-axi4-peripherals-extended-2026-10` 的 `_behavior` 测试
  MUST NOT 出现因 busy 锁死或 BVALID 不保持导致的 fixture 卡死

### Requirement: AXI4 master peripheral MUST 有独立于 slave 的复位契约

`AxiDma`（`include/axi4/peripherals/axi_dma.h`，AXI4 master）MUST 具备
`ch_in<ch_bool> rst_n` 端口，且所有 `ch_reg` MUST 有**显式列出**的复位值。
`AxiDma` 的复位契约 MUST NOT 复用 AXI4-Lite slave 的 Requirement
（master 复位的是自身 FSM 状态，而非从设备寄存器值）。

#### Scenario: `AxiDma` 含 rst_n 端口

- **WHEN** reviewer 阅读 `axi_dma.h` 的 `__io` section
- **THEN** MUST 含 `ch_in<ch_bool> rst_n;`

#### Scenario: 复位值表显式

- **WHEN** reviewer 阅读 `axi_dma.h` 的 `describe()`
- **THEN** 每个 `ch_reg` MUST 有显式的复位值（不允许 `init_val` 之类占位描述）
- **AND** `state` 的复位值 MUST 为 0（与 `is_idle` 判定一致）

#### Scenario: 复位逃生路径

- **WHEN** `rst_n` 拉低
- **THEN** `state` MUST 回到 idle（0）
- **AND** `data_vld` / `err_flag` / `irq_en` MUST 回到 false
- **AND** MUST 提供复位逃生路径（事务卡死时可被复位打断）

### Requirement: AXI4 peripheral 参考实现 MUST 与规范形态一致

`include/axi4/peripherals/axi_{spi,pwm,i2c}.h` MUST 与
`fix-axi4-peripheral-rst-n-port-2026-10` 确立的规范形态一致：

1. MUST 有 `b_valid` 寄存器；`bvalid` MUST 由其驱动；
2. `bvalid` MUST 在 **WDATA 被接受之后**才拉高，并保持到 BREADY
   （MUST NOT 是 `bvalid = aw_done` —— 那会早于 WDATA）；
3. `aw_done` MUST 仅在 `b_valid && bready` 时清零
   （MUST NOT 在裸 `bready` 上清零 —— master 常态拉高 BREADY 时 W 永不被接受）；
4. MUST 在 AW / AR 握手时锁存地址，写侧/读侧解码 MUST 基于锁存寄存器；
5. 每个文件 MUST 只有一处 `bvalid` 赋值。

#### Scenario: 消除 bvalid 双赋值

- **WHEN** 对 `axi_pwm.h` / `axi_i2c.h` 执行 `grep -c "bvalid = " <file>`
- **THEN** 每个文件 MUST 命中 1
- **AND** MUST NOT 出现后赋值覆盖前赋值
  （`axi_pwm.h` L116/L153、`axi_i2c.h` L149/L290 现状各 2 处）

#### Scenario: BVALID 响应保持且不早于 WDATA

- **WHEN** master 在 AW/W 握手后等待 BVALID
- **THEN** `bvalid` MUST 保持为 1 直到 BREADY
- **AND** MUST NOT 出现 `bvalid` 恒 0 的死锁状态
- **AND** `bvalid` MUST NOT 在 WDATA 被接受之前拉高

#### Scenario: aw_done 清零条件

- **WHEN** reviewer 检查 `aw_done->next` 的赋值
- **THEN** 清零条件 MUST 包含 `b_valid`（即 `b_valid && bready`）
- **AND** MUST NOT 直接依赖裸 `io().bready`

#### Scenario: 地址锁存

- **WHEN** 对 `axi_{spi,pwm,i2c}.h` 执行
  `grep -cE 'addr_bits = io\(\)\.aw?r?addr' <file>`
- **THEN** MUST 命中 0 行（解码 MUST 基于锁存寄存器）
- **AND** MUST 存在 `addr_reg` / `ar_addr_reg`

### Requirement: AXI4 peripheral MUST 驱动响应错误信号

`include/axi4/peripherals/*.h` MUST：

1. 驱动 `bresp` 与 `rresp`（MUST NOT 悬空）；
2. 对未解码地址返回 `DECERR`，MUST NOT 返回 OKAY。

> `wstrb` 字节写语义**不在本 Requirement 范围**（另见 change
> `axi4-peripheral-deferred-2026-10`）—— 它属新增功能而非修缺陷。

#### Scenario: bresp 不悬空

- **WHEN** 对 `include/axi4/peripherals/*.h` 执行 `grep -c "io().bresp = " <file>`
- **THEN** 每个文件 MUST 命中 ≥ 1
- **AND** 现状（`axi_spi.h` 完全未驱动 BRESP）MUST NOT 保留

#### Scenario: 未映射地址返回 DECERR

- **WHEN** master 访问未解码的寄存器地址
- **THEN** `bresp` MUST 为 `DECERR`
- **AND** MUST NOT 为 OKAY（与 `include/axi4/AGENTS.md` 的 ANTI-PATTERN 一致）

### Requirement: AXI4 peripheral 写侧 / 读侧地址映射 MUST 一致

`include/axi4/peripherals/*.h` 的写侧地址解码与读侧地址解码 MUST 指向**同一组**寄存器，
且 MUST 与该 peripheral 头部注释中登记的寄存器映射一致。

#### Scenario: GPIO 写读映射对齐

- **WHEN** reviewer 对照 `axi_gpio.h` 的写侧解码（`sel_*`）与读侧解码（`read_sel_*`）
- **THEN** 两者 MUST 指向同一组寄存器
- **AND** MUST 与头部注释的映射一致：
  `0x00→DATA_OUT`、`0x04→DATA_IN`、`0x08→DIRECTION`、`0x0C→INTERRUPT`
- **AND** 写侧对只读寄存器（DATA_IN）的写入 MUST 被忽略
- **AND** 现状（写侧 `0x04→DIRECTION` / `0x08→INTERRUPT`，与读侧错位一档）MUST NOT 保留

#### Scenario: 测试与映射同步

- **WHEN** 写侧映射被修正
- **THEN** `tests/test_axi_gpio.cpp` MUST 同步更新（MUST NOT 保留只对旧写侧映射成立的断言）

### Requirement: AXI4 peripheral MUST 支持字节写

- **MODIFIED**: 适用范围 MUST 扩到**通用 AXI slave**（`AxiLiteSlave` / `Axi4Slave`）——
  两者的写路径 MUST 消费 `wstrb`，MUST NOT 整字覆盖寄存器；并**合并**
  `axi4-byte-lane-dual-parity-2026-10`（archive
  `2026-10-20-axi4-byte-lane-dual-parity-2026-10`）加入的 2 个双后端 Scenario
  —— openspec 的 `MODIFIED` 是**整条替换**，不显式合并会丢掉那两条 MUST 断言。

AXI4 slave 的写路径 MUST 消费 `wstrb` 字节掩码，MUST NOT 整字覆盖寄存器。
字节道语义 MUST 按 `include/axi4/AGENTS.md` CONVENTIONS 登记的规则执行。

适用对象（本次扩到通用 AXI slave）：

- `include/axi4/peripherals/*.h` 的 6 个 AXI4-Lite slave peripheral
  （`AxiLiteGpio` / `AxiLiteUart` / `AxiLiteTimer` / `AxiLitePwm` / `AxiLiteI2c` / `AxiLiteSpi`）
- `include/axi4/axi4_lite_slave.h` 的 `AxiLiteSlave`（通用 AXI4-Lite slave）
- `include/axi4/axi4_full.h` 的 `Axi4Slave`（通用 AXI4 Full slave）

#### Scenario: 部分字节写只改选中字节

- **WHEN** master 以非全 1 的 `wstrb` 写一个 32 位寄存器
- **THEN** `wstrb` 未选中的字节 MUST 保持原值
- **AND** 选中的字节 MUST 被写入 `wdata` 对应字节
- **AND** 写全 1 掩码时行为 MUST 等价于整字写（既有测试前提）

#### Scenario: 只读寄存器不被字节写改写

- **WHEN** master 写一个只读寄存器（如 `DATA_IN` / `RX_DATA`）
- **THEN** 该寄存器 MUST 保持原值
- **AND** MUST NOT 产生部分字节改写

#### Scenario: 通用 AXI4-Lite slave MUST 消费 wstrb

- **WHEN** master 以非全 1 的 `wstrb` 写 `AxiLiteSlave` 的任一寄存器
- **THEN** 未选中的字节 MUST 保持原值
- **AND** 写全 1 掩码时行为 MUST 等价于整字写

#### Scenario: 通用 AXI4 Full slave MUST 在每次 W 握手取当拍 wstrb

- **WHEN** master 对 `Axi4Slave` 发一次写事务，AW 拍与 W 拍的 `wstrb` 不同
- **THEN** 写路径 MUST 采用 **W 握手拍** 的 `wstrb`（MUST NOT 在 AW 拍锁存）
- **AND** 未被该 `wstrb` 选中的字节 MUST 保持原值
- **AND** 多 beat burst 的逐 beat 语义当前**未实现**（`Axi4Slave` 写状态机每笔事务
  只接受 1 个 W beat）—— MUST NOT 被本 Requirement 隐含为已实现，单独立项

#### Scenario: 字节道索引相对被访问的字

- **WHEN** master 写一个位于字内某字节道上的寄存器
- **THEN** 字节道索引 MUST 相对**被访问的字**解释，MUST NOT 相对寄存器重新编号
- **AND** 亚 32 位寄存器 MUST 位于所在字的首字节道（当前全部寄存器均如此）

#### Scenario: 掩码为空的写是空写

- **WHEN** master 以 `wstrb == 0` 写一个已解码地址
- **THEN** 目标寄存器 MUST 保持原值
- **AND** 写响应 MUST 仍为 `OKAY`（地址已解码）

#### Scenario: 通用 slave 与 peripheral 行为 MUST 一致

- **WHEN** 同一 `wstrb` 序列分别作用于 `AxiLiteSlave` 与任一 peripheral
- **THEN** 寄存器的字节级结果 MUST 一致（同一套字节道语义）
- **AND** 32 bit 路径的既有行为 MUST NOT 因掩码实现泛化而变化

#### Scenario: 字节道行为 MUST 双后端一致

- **WHEN** 同一字节道写场景分别在 InterpreterBackend 与 VerilatorBackend 下执行
  （peripheral 侧：`tests/test_axi_byte_lane.cpp`；通用 slave 侧：`tests/test_axi_slave_byte_lane.cpp`）
- **THEN** 写后读回的寄存器值 MUST 逐对象一致
- **AND** `bresp`（含只读地址的 `DECERR`）MUST 一致

#### Scenario: 亚 32 位寄存器的有效字节道 MUST 双后端一致

- **WHEN** 对 8 bit 寄存器（i2c `TX_DATA`）或 16 bit 寄存器（i2c `PRESCALE`、
  spi `BAUD`）执行部分字节写
- **THEN** 落在**有效字节道**（宽度 W 的寄存器为 `ceil(W/8)` 个低位道）内的写
  MUST 生效，其余字节 MUST 保持原值
- **AND** 落在无效道上的写 MUST 被忽略（值不变）
- **AND** interpreter 与 verilator 的结果 MUST 一致
  （截断到寄存器宽度 W 的行为在两条后端路径上 MUST 相同）

### Requirement: UART peripheral MUST 提供串行收发引擎

`include/axi4/peripherals/axi_uart.h` MUST 提供 TX 移位引擎与 RX 接收引擎，
使 `uart_tx` / `uart_rx` / `rx_data_reg` / `rx_valid` / `irq` 具备真实串行语义。
帧格式 MUST 为 8N1（1 起始位 + 8 数据位 LSB 先出 + 1 停止位），且
**bit 周期 MUST 确定为编译期常量**（`AxiLiteUart<UART_BIT_PERIOD>` 模板参数，
MUST 在 `include/axi4/AGENTS.md` 登记），以保证串行波形可被确定性断言。

#### Scenario: TX 输出可还原的串行帧

- **WHEN** master 写 `TX_DATA`
- **THEN** `uart_tx` MUST 输出含起始位 + 8 个数据位 + 停止位的帧
- **AND** 采样 `uart_tx` MUST 能还原出写入的字节
- **AND** `tx_empty` MUST 反映移位寄存器是否为空

#### Scenario: RX 接收并置位 rx_valid

- **WHEN** 外部在 `uart_rx` 上发送一个合法帧
- **THEN** `rx_data_reg` MUST 收到该字节
- **AND** `rx_valid` MUST 置 1，`status_reg` bit1 MUST 反映 `rx_valid`
- **AND** `irq` MUST 由 `rx_valid` 驱动
- **AND** master 读走 `RX_DATA` 后 `rx_valid` MUST 清零

#### Scenario: 停止位错误置帧错误标志

- **WHEN** 外部在 `uart_rx` 上发送一帧且**停止位为低**
- **THEN** `status_reg` bit2（`RX_FRAME_ERROR`）MUST 置 1
- **AND** `rx_valid` MUST 保持 0（该帧不视为有效）
- **AND** `RX_DATA` MUST NOT 被该帧改写

#### Scenario: 帧错误标志仅由复位清除

- **WHEN** `RX_FRAME_ERROR` 已置 1 且之后又收到合法帧
- **THEN** `RX_FRAME_ERROR` MUST 保持 1（仅 `rst_n` 清除）

#### Scenario: 串行引擎行为 MUST 双后端一致

- **WHEN** 同一串行场景分别在 InterpreterBackend 与 VerilatorBackend 下执行
- **THEN** `uart_tx` 波形 MUST 逐拍一致
- **AND** 收到的字节 / `rx_valid` / 帧错误标志 MUST 一致

## See also

- `openspec/specs/verilator-e2e-coverage/spec.md` — 现有 9 个 verilator e2e 测试的契约（部分 requirement 是本 capability 的 reference impl）
- `openspec/specs/verilator-backend-e2e/spec.md` — VerilatorBackend 的 dlopen/cache/3-eval 契约（本 capability 依赖 R3 "data_map 同步" + R4 "3-step clock toggle"）
- `openspec/specs/verilator-rv32i-e2e/spec.md` — RV32I + Verilator e2e 契约（本 capability Phase 4 续接该 spec）
- `openspec/specs/verilator-reg-init/spec.md` — default_reset pulse 契约（与本能力 R8 互补）
- `openspec/specs/verilator-hier-input/spec.md` — 层次 input driver 契约（与本能力 macro 适用范围有关）
- `openspec/specs/mem-read-timing/spec.md` — aread/sread 时序契约（与 R11 cache Sync/Async 分类互补）
- `openspec/specs/cpu-cache-subsystem/spec.md` — cache 整体契约（与 R11 cache Sync/Async 分类互补）
- `.rddf/improvements/verilator-parity-all-tests.md` — 用户原始需求 draft
- `.rddf/improvements/cache-stub-physical-removal.md` — 关联 backlog（必须在 dual-backend cache test 落地后才执行）
- ADR-035 — VerilatorBackend 64-bit cap / clock toggle / reset pulse 设计决策
- ADR-041 — cache Sync/Async variant split
