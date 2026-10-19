# Design: verilator-parity-all-tests

> Phase 1 实施级设计。Phase 2-5 仅规划，不在本 change scope。

## 1. 架构概览

```
tests/
├── dual_backend_fixture.h (新)
│   ├── enum class BackendKind { Interpreter, Verilator };  // JIT 留演进位
│   ├── DUAL_BACKEND_TEST(DutType, name, tags)         // macro A
│   ├── DUAL_BACKEND_GOLDEN_TEST(DutType, name, tags)  // macro A' for Async cache
│   └── helpers:
│       ├── tool_available("verilator")   // 复用 tests/external_tool_helpers.h
│       ├── make_temp_dir(suffix)        // RAII + /tmp/opencode/<name>
│       └── port_width_check(ctx)        // 扫 ch_out 宽度
├── test_dual_backend_64_bit.cpp (新)
├── test_dual_backend_clock.cpp (新)
├── test_dual_backend_reset.cpp (新)
└── [迁移的 ~8-10 个测试]，TEST_CASE → DUAL_BACKEND_TEST
tests/CMakeLists.txt
├── add_dual_backend_test(name src... [TIMEOUT N])  // 新 function
└── 现有 9 个 verilator test 加 set_tests_properties LABELS "verilator_e2e"
.github/workflows/ci.yml
└── pr-verilator-fast job (新)
openspec/specs/dual-backend-test-coverage/spec.md (永久)
openspec/changes/verilator-parity-all-tests/
├── proposal.md (草案)
├── design.md (本文件)
└── tasks.md (builder 填)
```

## 2. Macro API 设计

### 2.1 `DUAL_BACKEND_TEST` (Macro A — primary)

**签名**：

```cpp
#define DUAL_BACKEND_TEST(DutType, name, tags)                            \
  TEST_CASE(name, tags) {                                                 \
    auto body_lambda = [&](ch::Simulator& sim,                            \
                          DutType& dut,                                   \
                          BackendKind kind) { /* 用户 body 在此 */ };     \
    { /* interpreter run */                                               \
      ch::ch_device<DutType> dev_a;                                       \
      ch::core::ctx_swap swap_a(&dev_a.context());                        \
      ch::Simulator sim_a(dev_a.context());                              \
      sim_a.set_jit_enabled(false);                  /* 钉死 JIT */       \
      sim_a.set_backend(std::make_unique<ch::InterpreterBackend>());      \
      body_lambda(sim_a, dev_a.instance(), BackendKind::Interpreter);    \
    }                                                                     \
    if (!ch_test::tool_available("verilator")) {                          \
      WARN("verilator not on PATH; interpreter half only");               \
    } else if (!ch_test::port_width_compatible(dev_a_top_ports)) {        \
      WARN("fixture has ch_out wider than 64-bit; skipping verilator");   \
    } else {                                                              \
      auto workdir = ch_test::make_temp_dir("dual");                      \
      ch::ch_device<DutType> dev_b;                                       \
      auto be = std::make_unique<ch::VerilatorBackend>(workdir.path());   \
      if (be->initialize(dev_b.context(), dev_b.data_map())) {            \
        ch::Simulator sim_b(dev_b.context());                            \
        sim_b.set_backend(std::move(be));                                 \
        body_lambda(sim_b, dev_b.instance(), BackendKind::Verilator);     \
      }                                                                   \
    }                                                                     \
  }
```

**Lambda 形态（用户 body）**：

```cpp
DUAL_BACKEND_TEST(MyAdder, "Adder 8-bit basic add", "[dual][arith][adder]") {
    // 'sim' 与 'dut' 由 macro 注入；'kind' 用于条件分支（可选）
    ch::ch_device<Adder> dev;
    ch::Simulator sim(dev.context());

    sim.set_input_value(dut.io().a, 3_d);
    sim.set_input_value(dut.io().b, 5_d);
    sim.tick();
    REQUIRE(static_cast<uint64_t>(sim.get_port_value(dut.io().sum)) == 8u);
}
```

注意：用户 body 必须**不再**重复 `ch_device` / `Simulator` 构造；macro 已注入 `sim` / `dut`。`kind` 参数可用也可不用（典型用法：cache Async 用 `kind` 决定跑 cycle 数差异）。

### 2.2 `DUAL_BACKEND_GOLDEN_TEST` (Macro A' — Async cache 等设计 drift 场景)

**签名**（仅 cycle 数随 kind 调整，per `cpu-cache-subsystem` §R5 + `mem-read-timing` spec）：

```cpp
DUAL_BACKEND_GOLDEN_TEST(ICacheTopAsync, "Async cache hit", "[dual][cache][async]") {
    ch::ch_device<ICacheTopAsync> dev;
    ch::Simulator sim(dev.context());

    sim.set_input_value(dut.io().addr, 0x1000_h);
    sim.set_input_value(dut.io().req, ch_bool(true));
    sim.tick(N);  // Macro 注入 N = interpreter: M, verilator: M+1
    REQUIRE(get_port_value(dut.io().ready) == 1);
}
```

### 2.3 为什么不用 B（template fixture）

per oracle 调研：B 的"类型安全"买到的东西很少——lambda body 本身编译期类型检查，Catch2 TEMPLATE_TEST_CASE 模板实例化栈很深反而拖慢错误信息。Macro 物理边界已被 `test_verilator_trace_parity.cpp:270-316` 钉死（必须双 device 双 context），C 选项 macro A 已经足够。BackendKind 枚举位保留给后续 DUAL_SNAPSHOT recorder（per-cycle 终值比对）扩展。

## 3. CMake function 设计

### 3.1 `tests/CMakeLists.txt::add_dual_backend_test`

```cmake
function(add_dual_backend_test TEST_NAME)
    # Parse args: 第一个为 name, 之后为 sources, 末尾可选 TIMEOUT
    set(SOURCES ${ARGV})
    list(REMOVE_ITEM SOURCES ${TEST_NAME})
    # 提取可选 TIMEOUT kwarg
    set(DL_TIMEOUT 60)  # default
    list(FIND SOURCES "TIMEOUT" TIMEOUT_IDX)
    if (NOT TIMEOUT_IDX EQUAL -1)
        math(EXPR TIMEOUT_VAL_IDX "${TIMEOUT_IDX}+1")
        list(GET SOURCES ${TIMEOUT_VAL_IDX} DL_TIMEOUT)
        list(REMOVE_AT SOURCES ${TIMEOUT_IDX})
        list(REMOVE_AT SOURCES ${TIMEOUT_VAL_IDX})
    endif()
    # 实际注册
    add_catch_test(${TEST_NAME} ${SOURCES})
    set_tests_properties(${TEST_NAME} PROPERTIES
        LABELS "verilator_e2e"
        RESOURCE_LOCK "verilator_backend"
        ENVIRONMENT "${_vl_path_env}"
        TIMEOUT ${DL_TIMEOUT})
endfunction()
```

### 3.2 现有 9 个 verilator test 补 label

```cmake
# tests/CMakeLists.txt 每行现有 set_tests_properties 追加 LABELS "verilator_e2e"
set_tests_properties(test_verilator_trace_parity PROPERTIES
    ... (existing PROPERTIES)
    LABELS "verilator_e2e")
```

仅追加 `LABELS`，不动 `RESOURCE_LOCK` / `TIMEOUT` / `ENVIRONMENT`。

## 4. CI matrix 改动（**显式列出**）

### 4.1 `.github/workflows/ci.yml` 新增 `pr-verilator-fast` job

```yaml
pr-verilator-fast:
    name: PR Verilator Dual-Backend Fast Feedback
    runs-on: ubuntu-22.04
    if: github.event_name == 'pull_request'
    timeout-minutes: 45
    steps:
      - uses: actions/checkout@v4
        with:
            submodules: recursive
      - name: Configure (BUILD_VERILATOR=ON)
        run: cmake -B build -DBUILD_VERILATOR=ON -DCH_JIT_ENABLE=OFF
      - name: Build
        run: cmake --build build -j$(nproc)
      - name: Run verilator_e2e non-perf
        run: cd build && ctest -L verilator_e2e -LE perf --output-on-failure
```

### 4.2 现有 nightly job 仍走完整 dual-backend + perf

不动。现有 matrix 在 `ci.yml:456-458` 已包含 `BUILD_VERILATOR=ON` nightly，跳过。

### 4.3 PR job 与现有 `pr-build` 关系

- `pr-build`：现状 `BUILD_VERILATOR=OFF` + `ctest -LE perf`（5-8 min）
- `pr-verilator-fast`：新增 `BUILD_VERILATOR=ON` + `ctest -L verilator_e2e -LE perf`（≤45 min）

两者**并行**触发，不互相 wait。任一失败 PR review-blocker。

## 5. 8-10 个基础 test 候选名单（按 explore 实测）

| 候选测试 | 是否可 dual | 备注 |
|---|---|---|
| `test_basic.cpp` 中含 `ch_device` 的 case | ✅ | 阶段 1 目标 |
| `tests/test_module.cpp` ch_module 段 | ✅ | 阶段 1 目标 |
| `tests/test_reg_timing.cpp` | ✅ | 阶段 1 目标 |
| `tests/test_simple_io.cpp` 含 Simulator 的 | ✅（部分）| 阶段 1 目标（仅迁有 ch_out 端口的 case） |
| `tests/test_io_*.cpp` | ✅（看具体文件）| 阶段 1 目标（按 R6 grep 筛） |
| `tests/test_ch_stream.cpp` 流数据段 | ✅ | 阶段 1 目标 |
| `tests/test_ch_flow.cpp`（如存在）| ✅（看代码）| 阶段 1 候选 |
| `tests/test_reg.cpp` STATIC_REQUIRE 段 | ❌ (R6) | 不可 dual |
| `tests/test_arithmetic.cpp` (chlib) 裸 context 段 | ❌ (R6) | 不可 dual |
| `tests/test_ch_reg_bitwise.cpp` 裸 context 段 | ❌ (R6) | 不可 dual |
| `tests/test_mem_comb_read.cpp` aread 段 | ❌ (R6) | 不可 dual |
| `tests/test_state_machine.cpp` DSL lambda | ❌ (R6 Catch2 SECTION) | 不可 dual |

**阶段 1 目标数：≥ 8 个**（不强求 10，per acceptance R6 + explore 实测）

## 6. 3 个专项 fixture 设计

### 6.1 `test_dual_backend_64_bit.cpp`

```cpp
DUAL_BACKEND_TEST(CounterWide64, "64-bit counter under cap", "[dual][width-cap]") {
    ch::ch_device<CounterWide64> dev;
    ch::Simulator sim(dev.context());
    sim.tick(50);
    REQUIRE(get_port_value(dut.io().out) == 50ULL);
}

DUAL_BACKEND_TEST(CounterWide96, "96-bit counter skips verilator", "[dual][width-cap][.verilator-skip-wide]") {
    ch::ch_device<CounterWide96> dev;
    ch::Simulator sim(dev.context());
    sim.tick(50);
    // verilator half SKIP'd by macro; interpreter-only assert here
    REQUIRE(sim.backend()->active_backend_name() == "interpreter");
}
```

### 6.2 `test_dual_backend_clock.cpp`

Simple counter 单 cycle dual，验证 `verilator-backend-e2e` §"3-step clock toggle" 契约。

### 6.3 `test_dual_backend_reset.cpp`

Counter 设 `INIT_d = 42`，跑 dual 验证 cycle 1 后 counter == 43（与 `verilator-reg-init` spec §R1 互补）。

## 7. 实施 TDD 步骤（builder 阶段）

Phase 1 共 9 步（按 builder TDD 5-step 节奏展开）：

1. **建宏骨架 + 1 个自证测试**：`tests/dual_backend_fixture.h::DUAL_BACKEND_TEST` empty body + `tests/test_dual_backend_template.cpp` 1 个 dummy `Counter32` fixture，验证 interpreter run PASS + verilator 在 PATH 时 PASS，verilator 缺失时仅 WARN 不 SKIP
2. **建 `add_dual_backend_test` CMake function**：注册第 1 步 test，验证 `RESOURCE_LOCK` / `ENVIRONMENT` / `LABELS` 三件套生效（`ctest -N -L verilator_e2e` 列出 + `set_tests_properties` 显示）
3. **迁 1 个真实基础 test**：`tests/test_basic.cpp` 1 个 case，确认 macro 真实工作
4. **加 3 个专项 fixture**（64-bit / clock / reset）：每个独立 test_target，按 R10
5. **批量迁 7+ 基础 test**：按 §5 名单 `DUAL_BACKEND_TEST` 替换
6. **9 个现有 verilator test 补 label** + 顶注释（仅 diff ≤ 4 行/文件）
7. **ci.yml 加 `pr-verilator-fast` job**：yaml 改动 + dry run PR 模拟
8. **实测墙钟**：`time ctest -L verilator_e2e -LE perf` + 同步 CTEST_TOTAL 到根 AGENTS.md
9. **archive 前置 gate**：`openspec validate --strict` + `verify_test_counts.sh` + `git diff --stat` 检查

每步独立 commit。

## 8. 风险与 mitigation

| 风险 | 等 | mitigation |
|---|---|---|
| Phase 1 实测墙钟 > 45 min | 🟡 | R10 锁定自动降级 Phase 2-5 |
| >64-bit 测试被 macro 拒收 | 🟢 | R5 字段检测 + WARN；interpreter 仍贡献覆盖 |
| cache Async dual 必须跑 | 🟡 | R11 与 DUAL_BACKEND_GOLDEN_TEST macro 隔离；Sync 走 parity / Async 走 golden-value |
| 9 个旧 test diff > 4 行 | 🟢 | Open Question 2 锁定极严格 ≤ 4 行/文件 |
| 现有 4 个 verilator tests 因 `LABELS` 命名冲突 | 🟢 | `verilator_e2e` 是新 label，无冲突；与现有 `base` 同时存在 |
| `add_catch_test` 内部硬编 LABELS `base` | 🟡 | `set_tests_properties` 覆盖为 `verilator_e2e`（与现有 8 个 verilator test 模式一致） |
| Phase 4 RV32I integration 隐藏 SIGSEGV | 🔴 | 已显式标 backlog 项；Phase 1 不触及 riscv-mini wrapper |
| dual-backend 暴露 cache codegen bug | 🟡 | Phase 1 不强制跑 cache（per 阶段 1 范围）；Phase 2 才进 cache，bug fix 单独立 change |

## 9. 反向约束（哪些 spec/ADR/契约本 design 不能违反）

- **`verilator-e2e-coverage` spec**：9 个现有 verilator test 的 file-level / count-level requirement 不能动；本 change 仅加 `LABELS`，不破 spec
- **`verilator-backend-e2e` spec §"port bitwidth limit: 64 bits"** 与 `§"3-step clock toggle"`：本 macro 设计不绕过这些契约
- **`verilator-rv32i-e2e` spec §R2 "explicit InterpreterBackend pinning"**：本 macro 的 `set_jit_enabled(false) + set_backend(Interpreter)` 双保险恰好满足该 R2
- **`mem-read-timing` spec**：本 macro R11 把 aread 设计明确列入排除清单 + DUAL_BACKEND_GOLDEN_TEST 隔离，不打破 aread/sread 时序契约
- **ADR-035**：64-bit cap 维持，>64-bit 降级不修改 backend 行为
- **ADR-041**：cache Sync/Async variant 维持；dual-backend 按 variant 分类

## 10. 验收命令清单（builder 必跑）

```bash
# 编译
cmake --build build -j$(nproc) 2>&1 | grep -E "error|Error"  # 0 errors

# SPEC 验证
openspec validate --specs --strict  # 14/14 通过
openspec validate verilator-parity-all-tests --strict  # 通过

# 测试运行
ctest -LE perf --output-on-failure  # 不退化
ctest -L verilator_e2e -LE perf --output-on-failure  # 12+ 全绿
ctest -N -L verilator_e2e -LE perf | tail -1  # 输出实测 verilator_e2e 数

# 墙钟
time ctest -L verilator_e2e -LE perf  # ≤ 45 min

# 计数同步
cd build && ctest -N | tail -1  # 实测 CTEST_TOTAL_N
bash scripts/verify_test_counts.sh  # exit 0

# 9 个旧 test diff 限制
git diff --stat tests/test_verilator_*.cpp tests/test_axi_lite_verilator_e2e.cpp | wc -l  # ≤ 4 行/文件
```

## 11. Phase 2-5 简表（不在本 change scope）

| Phase | change 名 | 风险 | 阻塞项 |
|---|---|---|---|
| 2 | `dual-backend-cache-tests` | 🟡 | Phase 1 落地 + ADR-041 Sync/Async 分类 |
| 3 | `dual-backend-chlib-tests` | 🟡 | ~50 个 test |
| 4 | `dual-backend-rv32i-integration` | 🔴 | **i_cache_prefetch.h wrapper SIGSEGV 先单独立项修** |
| 5 | `dual-backend-full-coverage` | 🟡 | 触发 cache-stub-physical-removal backlog |
