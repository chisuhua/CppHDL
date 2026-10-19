# verilator-parity-all-tests Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use skill_use("execute") to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 把 `verilator-parity-all-tests` change (Phase 1) 落地——`DUAL_BACKEND_TEST` 基础设施 + 8-10 个基础测试迁移 + 3 个专项 fixture + ci.yml `pr-verilator-fast` job + 9 个现有 verilator 测试补 `verilator_e2e` 标签。

**Architecture:** 头文件-only fixture (`tests/dual_backend_fixture.h`) 提供 `DUAL_BACKEND_TEST(DutType, name, tags)` 宏，封装"interpreter 跑一次 + verilator 跑一次"的样板；CMake `add_dual_backend_test()` 函数追加 `RESOURCE_LOCK "verilator_backend"` + `ENVIRONMENT "${_vl_path_env}"` + `LABELS "verilator_e2e"` 三件套；9 个现有 verilator 测试仅追加 `verilator_e2e` 标签（保留 `base`）；CI 新增 `pr-verilator-fast` job 跑 `ctest -L verilator_e2e -LE perf` ≤45 min。

**Tech Stack:** C++23, Catch2 v3.7.0, CMake 3.20+, CMake ExternalProject_Add (verilator 子模块), GitHub Actions (ubuntu-22.04), system verilator on PATH (or `${CPPHDL_VERILATOR_WRAPPER}` bin dir injected via `_vl_path_env`)。

---

## 设计要点：resolved API + 修正

以下列出**对 design.md / proposal.md 的关键修正**（基于代码实测），作为 Task 1-9 实施依据。

### R1. `DUAL_BACKEND_TEST` 宏的正确形态

design.md §2.1 给的宏写法在 C++ 预处理器层面**不可编译**——用户写 `DUAL_BACKEND_TEST(Type, "n", "[t]") { body }` 仅消耗一个 `}`，但宏需要既关闭 lambda 又要 `;` + 关闭 TEST_CASE 的 `}`，无法实现。本 plan 采用 **"for + if trick"**：宏在 `TEST_CASE` body 内开 `for (BackendKind kind : dual_kinds()) if (select_backend(kind))`，用户的 `{ body }` 直接是 `if` 的 body，`sim`/`dut` 必须在宏前缀就声明好引用。

```cpp
namespace ch_test {
enum class BackendKind { Interpreter, Verilator };
}
```

### R2. 单 device + 双 sim 还是双 device？

参考实现 `tests/test_verilator_trace_parity.cpp:277-316` 用**双 device + 双 sim**（per-cycle 交叉对比需要独立 data_map）。但本 design.md 的 macro 是"整体跑两次"（非 per-cycle 对比），可省一个 device。`ch_device<DutType>` 仅持有 context/AST（与 Simulator 的 data_map_ 解耦），所以**单 device + 单 sim + `set_backend()` 切换**是可行的：第一次 `set_backend(InterpreterBackend)` + `body()`；第二次 `set_backend(VerilatorBackend)`（其 `initialize` 内部脉冲 reset 加载 `ch_reg` 的 INIT） + `body()`。本 plan 采用单 device 路径（更简单、`sim`/`dut` 引用稳定）。

### R3. `set_backend()` 内部就调 `initialize`

`Simulator::set_backend` (`src/simulator.cpp:530-545`) 内部无条件 `backend_->initialize(ctx_, data_map_)`。所以 macro 里**不要**额外先 `be->initialize(ctx, tmp_map)` 再 `set_backend`——直接 `sim.set_backend(std::move(be))` 即可。但 VerilatorBackend 编译失败时 `initialize` 返回 false 而 `set_backend` **忽略 bool**，需要 macro 事后用 `sim.backend()` 和 `port_access_snapshot()` 探测失败并 WARN+跳过。

### R4. `is_native()` gate

`Simulator::eval_sequential`/`eval_combinational` (`src/simulator.cpp:192,231`) 仅在 `backend_ && backend_->is_native()` 时委托给 backend。`VerilatorBackend::is_native()` 返 true（native 执行）；`InterpreterBackend::is_native()` 返 false → Simulator 走 instr 循环。所以 `set_backend(InterpreterBackend)` + `set_jit_enabled(false)` 才是 interpreter 路径的完整双保险。

### R5. `>64-bit` 端口

VerilatorBackend 的 `build_port_access_table()` 返 false 时 `initialize()` 返 false（per `root AGENTS.md §VERILATOR BACKEND RULES`）→ macro 探测 `sim.backend()` 失败 / `port_access_snapshot().empty()` 时 WARN 并跳过 verilator half。无需单独的 `port_width_compatible` helper。

### R6. CMake `LABELS` 是覆盖不是追加

`set_tests_properties(test_x PROPERTIES LABELS "verilator_e2e")` 会**覆盖** `add_catch_test()` 设的 `LABELS "base"`。9 个现有测试若只写 `LABELS "verilator_e2e"` 会丢失 base → 破坏 `ctest -LE perf` 基线（acceptance §8 零回归）。**必须**写 `LABELS "base;verilator_e2e"` 保留 base 同时加新 label。

### R7. `add_catch_test` 不支持 `RESOURCE_LOCK` / `ENVIRONMENT` / `TIMEOUT`

现有 verilator 测试在 `add_catch_test()` 之后用 `set_tests_properties()` 追加（见 `tests/CMakeLists.txt:89-94, 364-368, 372-376, ...`）。`add_dual_backend_test` 函数必须**内部**调 `add_catch_test` + `set_tests_properties` 两步。

### R8. `Simulator::set_jit_enabled` 是 `#if __has_include("jit/jit_compiler.h")` guarded

`include/simulator.h:90-97`：当 `CH_JIT_ENABLE=OFF` 时符号不存在。macro 用 `if constexpr (__has_include("jit/jit_compiler.h"))` 探测；pr-verilator-fast job 设 `-DCH_JIT_ENABLE=OFF` 避免 JIT 与 Verilator 交互（JIT 与 verilator 已正交，按 proposal Out-of-Scope §1 排除）。

### R9. Acceptance 实际数据

- 当前 baseline (per `root AGENTS.md`): `CTEST_TOTAL: 159` (= 148 non-perf + 11 perf)。
- 9 个现有 verilator 测试已存在，新增 3 个专项 fixture + 1 个 dual-backend-template（macro 自证） = +4 target。
- verilator_e2e count 验收线 `≥ 12`（9 旧 + ≥ 3 新），不是 `≥ 100`（proposal §4 已修正）。
- CTEST_TOTAL 实测回填到根 `AGENTS.md §NOTES` 的 `<TBD>` 字段。

---

## File Structure

### Production Code (新增)

| File | Responsibility |
|---|---|
| `tests/dual_backend_fixture.h` | `ch_test::BackendKind` enum + `DUAL_BACKEND_TEST` 宏 + `DUAL_BACKEND_GOLDEN_TEST` 宏 (Phase 2 用) + `make_temp_dir`/`tool_available` 转封装 |
| `tests/CMakeLists.txt` (改) | 新增 `add_dual_backend_test()` 函数 + 现有 9 个 verilator 测试加 `verilator_e2e` 标签 + 4 个新测试 target 注册 |
| `.github/workflows/ci.yml` (改) | 新增 `pr-verilator-fast` job |

### Tests (新增 / 迁移)

| File | Responsibility |
|---|---|
| `tests/test_dual_backend_template.cpp` | macro 自证测试：1 个 `Counter32` fixture，interpreter + verilator 两遍全过 |
| `tests/test_dual_backend_64_bit.cpp` | 64-bit 端口边界 + `>64-bit` 降级 |
| `tests/test_dual_backend_clock.cpp` | 2-step 时钟 toggle |
| `tests/test_dual_backend_reset.cpp` | default_reset pulse (INIT=42 → cycle 1 == 43) |
| `tests/test_basic.cpp` (改) | 有 `ch_device` 的 case 迁 macro |
| `tests/test_module.cpp` (改) | ch_module 段迁 macro |
| `tests/test_reg_timing.cpp` (改) | 整文件迁 macro |
| `tests/test_simple_io.cpp` (改) | 有 `Simulator` 的 case 迁 macro |
| `tests/test_io_*.cpp` (改) | R6 grep 筛后迁 macro |
| `tests/test_ch_stream.cpp` (改) | 流数据段迁 macro |

### 锁 / 不动

- `src/codegen_verilog.cpp`（不动）
- `include/core/verilator_backend.h`（不动）
- 9 个现有 verilator 测试体（只加 `LABELS "base;verilator_e2e"` + 顶注释）

---

### Task 1: 建宏骨架 + macro 自证测试

**Files:**
- Create: `tests/dual_backend_fixture.h`
- Create: `tests/test_dual_backend_template.cpp`
- Test: `tests/test_dual_backend_template.cpp`

- [ ] **Step 1: 写失败测试（macro 自证）**

写 `tests/test_dual_backend_template.cpp`：

```cpp
// tests/test_dual_backend_template.cpp
// DUAL_BACKEND_TEST macro 自证测试: 1 个 Counter32 fixture,
// interpreter + verilator 各跑一次，assert cycle 50 后 counter == 50。
// 验证 macro 的双重保险 (set_jit_enabled(false) + set_backend(InterpreterBackend))
// 真的走 interpreter 路径，以及 verilator half 在 verilator 不在 PATH 时 WARN 不 fail。
#include "catch_amalgamated.hpp"
#include "dual_backend_fixture.h"
#include "ch.hpp"

namespace {

template <unsigned N>
class Counter32 : public ch::Component {
public:
    __io(ch_out<ch_uint<N>> out;)
    Counter32(ch::Component *parent = nullptr,
              const std::string &name = "c32")
        : ch::Component(parent, name) {}
    void create_ports() override { new (io_storage_) io_type; }
    void describe() override {
        ch_reg<ch_uint<N>> reg(0_d);
        reg->next = reg + 1_d;
        io().out = reg;
    }
};

} // namespace

DUAL_BACKEND_TEST(Counter32, "Counter32 50 cycles dual backend",
                  "[dual][template][counter]") {
    sim.tick(50);
    REQUIRE(static_cast<uint64_t>(sim.get_port_value(dut.io().out)) == 50u);
}
```

- [ ] **Step 2: 运行测试验证失败（macro 不存在）**

Run: `cmake --build build -j$(nproc) 2>&1 | grep -E "DUAL_BACKEND|unresolved|error" | head -5`
Expected: FAIL with `DUAL_BACKEND_TEST not declared`。

- [ ] **Step 3: 写最小实现（fixture header 骨架）**

写 `tests/dual_backend_fixture.h`：

```cpp
// tests/dual_backend_fixture.h
//
// Phase 1 DUAL_BACKEND_TEST infrastructure. Single-file header.
//
// Design decisions (see .rddf/plans/verilator-parity-all-tests.md R1-R8):
//   - single ch_device + single Simulator per TEST_CASE; switch backends
//     via sim.set_backend() between two body runs
//   - set_jit_enabled(false) + set_backend(InterpreterBackend) double-pins
//     interpreter path (Simulator::eval_* gates on is_native() +
//     CH_JIT_ENABLE=OFF in ci.yml eliminates JIT dispatch)
//   - macro emits for (kind : dual_kinds()) if (select_backend(kind)) { body }
//     so the user's trailing { body } is the if-body (Catch2's TEST_CASE
//     expands to `static void fn()` so the body must be a compound stmt,
//     not a lambda body — see plan R1)
//   - sim/dut references are bound before the loop and remain stable
//     because the single device/simulator objects live in DualContext
//     storage for the entire TEST_CASE

#pragma once

#include "ch.hpp"
#include "catch_amalgamated.hpp"
#include "external_tool_helpers.h"
#include <memory>
#include <string>

namespace ch_test {

enum class BackendKind { Interpreter, Verilator };

namespace detail {

// Holds the single ch_device + Simulator for the lifetime of a DUAL_BACKEND_TEST.
// sim() and dut() return stable references valid across both backend passes.
template <typename DutType>
struct DualContext {
    ch::ch_device<DutType> dev;
    ch::Simulator *sim_ptr = nullptr;  // non-owning; points at owned_sim
    std::unique_ptr<ch::Simulator> owned_sim;
    DutType *dut_ptr = nullptr;

    DualContext() : dev{} {
        owned_sim = std::make_unique<ch::Simulator>(dev.context());
        sim_ptr = owned_sim.get();
        dut_ptr = &dev.instance();
    }

    void setup_interpreter() {
#if __has_include("jit/jit_compiler.h")
        owned_sim->set_jit_enabled(false);
#endif
        owned_sim->set_backend(std::make_unique<ch::InterpreterBackend>());
    }

    bool setup_verilator() {
        if (!ch_test::tool_available("verilator")) {
            WARN("verilator not on PATH; skipping verilator half");
            return false;
        }
        std::string workdir = ch_test::make_temp_dir("_dual_backend");
        if (workdir.empty()) {
            WARN("failed to create verilator workdir; skipping verilator half");
            return false;
        }
        auto be = std::make_unique<ch::VerilatorBackend>(workdir);
        owned_sim->set_backend(std::move(be));
        // set_backend internally calls initialize(ctx, data_map); the
        // backend pointer is still retrievable via sim.backend() for
        // failure detection.
        if (!owned_sim->backend() ||
            owned_sim->active_backend_name() != "verilator") {
            WARN("VerilatorBackend initialize failed; skipping verilator half");
            return false;
        }
        if (owned_sim->backend()->port_access_snapshot().empty()) {
            WARN("verilator port_access_ empty; build/dlopen failed");
            return false;
        }
        return true;
    }

    ch::Simulator &sim() { return *sim_ptr; }
    DutType &dut() { return *dut_ptr; }
};

inline bool select_backend(ch_test::detail::DualContext<void> &, ch_test::BackendKind) {
    return false; // unreachable; template specialization below
}

template <typename DutType>
bool select_backend(DualContext<DutType> &ctx, BackendKind kind) {
    if (kind == BackendKind::Interpreter) {
        ctx.setup_interpreter();
        return true;
    }
    return ctx.setup_verilator();
}

} // namespace detail
} // namespace ch_test

#define DUAL_BACKEND_TEST(DutType, name, tags)                              \
    TEST_CASE(name, tags) {                                                 \
        ch_test::detail::DualContext<DutType> __dual;                       \
        ch::Simulator &sim = __dual.sim();                                  \
        DutType &dut = __dual.dut();                                        \
        (void)sim; (void)dut;                                               \
        for (ch_test::BackendKind __kind :                                  \
             {ch_test::BackendKind::Interpreter,                            \
              ch_test::BackendKind::Verilator})                             \
            if (ch_test::detail::select_backend(__dual, __kind))
```

> Note: the `select_backend` template + the `void` overload above are illustrative — the real impl uses a single template specialized for `DualContext<DutType>`. The macro ends with `if (select_backend(...))` so the user's trailing `{ body }` is the if-body. `sim`/`dut` references are bound before the loop and reused.

- [ ] **Step 4: 重新 build + run 测试验证通过**

Run:
```bash
cmake --build build -j$(nproc) 2>&1 | grep -E "error|Error" | head
ctest -R test_dual_backend_template --output-on-failure
```
Expected: 0 compile errors, test PASSES (interpreter half always; verilator half PASSES if `which verilator` succeeds, else WARN + interpreter-only).

- [ ] **Step 5: Defer commit**

按 AGENTS.md 原则，execute 阶段不逐任务 commit；所有变更在 archive 阶段统一提交。

---

### Task 2: 建 `add_dual_backend_test` CMake function

**Files:**
- Modify: `tests/CMakeLists.txt` (新增 function + 注册 Task 1 测试)

- [ ] **Step 1: 写失败测试 (验证 function 不存在)**

Run: `grep -n "add_dual_backend_test" tests/CMakeLists.txt`
Expected: empty output (function not defined yet).

- [ ] **Step 2: 不需要真写失败 case (function absence 即失败)**

- [ ] **Step 3: 写最小实现 (CMake function)**

在 `tests/CMakeLists.txt` `add_catch_test` 函数定义后（约 line 18）追加：

```cmake
# ============================================================================
# DUAL_BACKEND_TEST CMake wrapper (verilator-parity-all-tests Phase 1)
# Wraps add_catch_test() with the 3-property set the verilator side
# needs. See .rddf/plans/verilator-parity-all-tests.md R6 + R7.
# - RESOURCE_LOCK "verilator_backend" — verilator build dir is shared,
#   serialize all dual-backend tests per ci.yml design.
# - ENVIRONMENT "${_vl_path_env}" — inject the local verilator bin dir
#   (set by the block at tests/CMakeLists.txt:83-88).
# - LABELS "verilator_e2e" — picks up via ctest -L verilator_e2e.
#
# Usage:
#   add_dual_backend_test(test_dual_backend_template
#       test_dual_backend_template.cpp)
#   add_dual_backend_test(test_dual_backend_64_bit
#       test_dual_backend_64_bit.cpp TIMEOUT 60)
# ============================================================================
function(add_dual_backend_test TEST_NAME)
    set(_sources ${ARGN})
    set(_timeout 60)  # default; override via TIMEOUT kwarg
    list(FIND _sources "TIMEOUT" _ti)
    if(NOT _ti EQUAL -1)
        math(EXPR _tv "${_ti}+1")
        list(GET _sources ${_tv} _timeout)
        list(REMOVE_AT _sources ${_ti})
        list(REMOVE_AT _sources ${_tv})
    endif()
    add_catch_test(${TEST_NAME} ${_sources})
    set_tests_properties(${TEST_NAME} PROPERTIES
        LABELS "verilator_e2e"
        RESOURCE_LOCK "verilator_backend"
        ENVIRONMENT "${_vl_path_env}"
        TIMEOUT ${_timeout})
endfunction()
```

把 Task 1 的 `add_catch_test(test_dual_backend_template ...)` 改为 `add_dual_backend_test(test_dual_backend_template test_dual_backend_template.cpp)`。

- [ ] **Step 4: 验证 CMake 解析 + test 可见**

Run:
```bash
cmake --build build --target test_dual_backend_template -j$(nproc) 2>&1 | grep -E "error|Error" | head
ctest -N -L verilator_e2e 2>&1 | grep test_dual_backend_template
```
Expected: build OK; `ctest -N -L verilator_e2e` lists `test_dual_backend_template` with label `verilator_e2e`.

- [ ] **Step 5: Defer commit**

---

### Task 3: 迁移 1 个真实基础测试 (`tests/test_basic.cpp`)

**Files:**
- Modify: `tests/test_basic.cpp` (1 个 case: `ch_device` + `ch_out`)

- [ ] **Step 1: 找一个有 `ch_device` + `ch_out` 的 case 并原状跑通**

Run: `ctest -R "^test_basic$" --output-on-failure`
Expected: PASS (baseline).

- [ ] **Step 2: 迁 macro (改 TEST_CASE→DUAL_BACKEND_TEST + 去重 sim/dev 构造)**

找一个 case（例如 `"Basic counter 8-bit"`），原代码大致：
```cpp
TEST_CASE("Basic counter 8-bit", "[basic][counter]") {
    ch::ch_device<BasicCounter<8>> dev;
    ch::core::ctx_swap swap(&dev.context());
    ch::Simulator sim(dev.context());
    sim.tick(5);
    REQUIRE(static_cast<uint64_t>(sim.get_port_value(dev.instance().io().out)) == 5u);
}
```
改为：
```cpp
DUAL_BACKEND_TEST(BasicCounter<8>, "Basic counter 8-bit", "[basic][counter][dual]") {
    sim.tick(5);
    REQUIRE(static_cast<uint64_t>(sim.get_port_value(dut.io().out)) == 5u);
}
```
注意去掉 `ch_device`/`Simulator`/`ctx_swap` 构造（macro 注入 `sim`/`dut`）；include `dual_backend_fixture.h`；template 形参 `BasicCounter<8>` 含逗号，宏调用需 `(BasicCounter<8>, "...")` 形式或用 typedef。

- [ ] **Step 3: 重新 build + run 测试**

Run:
```bash
cmake --build build --target test_basic -j$(nproc) 2>&1 | grep -E "error|Error" | head
ctest -R "^test_basic$" --output-on-failure
```
Expected: PASS (interpreter half); verilator half PASS 或 WARN（如果 design 不兼容 verilator）。

- [ ] **Step 4: Defer commit**

---

### Task 4: 3 个专项 fixture

**Files:**
- Create: `tests/test_dual_backend_64_bit.cpp`
- Create: `tests/test_dual_backend_clock.cpp`
- Create: `tests/test_dual_backend_reset.cpp`
- Modify: `tests/CMakeLists.txt` (注册 3 个 target)

- [ ] **Step 1: 写失败测试（3 个 fixture）**

`tests/test_dual_backend_64_bit.cpp`（节选）：
```cpp
#include "catch_amalgamated.hpp"
#include "dual_backend_fixture.h"
#include "ch.hpp"
namespace {
template <unsigned N> class CounterWide : public ch::Component {
public: __io(ch_out<ch_uint<N>> out;)
    CounterWide(ch::Component *p = nullptr, const std::string &n = "cw")
        : ch::Component(p, n) {}
    void create_ports() override { new (io_storage_) io_type; }
    void describe() override { ch_reg<ch_uint<N>> r(0_d); r->next = r + 1_d; io().out = r; }
};
}
// 64-bit exact: verilator should accept (within QData 64-bit cap)
DUAL_BACKEND_TEST(CounterWide<64>, "Counter 64-bit under cap",
                  "[dual][width-cap][counter]") {
    sim.tick(50);
    REQUIRE(static_cast<uint64_t>(sim.get_port_value(dut.io().out)) == 50ULL);
}
// 96-bit: verilator half SKIP'd by macro; interpreter-only assert
DUAL_BACKEND_TEST(CounterWide<96>, "Counter 96-bit skips verilator",
                  "[dual][width-cap][.verilator-skip-wide]") {
    sim.tick(50);
    REQUIRE(static_cast<uint64_t>(sim.get_port_value(dut.io().out)) == 50ULL);
}
```

`tests/test_dual_backend_clock.cpp`：
```cpp
DUAL_BACKEND_TEST(CounterWide<32>, "Clock 2-step toggle 20 cycles",
                  "[dual][clock][counter]") {
    sim.tick(20);
    REQUIRE(static_cast<uint64_t>(sim.get_port_value(dut.io().out)) == 20u);
}
```

`tests/test_dual_backend_reset.cpp`：
```cpp
template <unsigned N> class CounterInit42 : public ch::Component {
public: __io(ch_out<ch_uint<N>> out;)
    void create_ports() override { new (io_storage_) io_type; }
    void describe() override { ch_reg<ch_uint<N>> r(42_d); r->next = r + 1_d; io().out = r; }
};
DUAL_BACKEND_TEST(CounterInit42<32>, "Reset pulse loads INIT=42",
                  "[dual][reset][counter]") {
    sim.tick(1);
    REQUIRE(static_cast<uint64_t>(sim.get_port_value(dut.io().out)) == 43u);
}
```

- [ ] **Step 2: CMake 注册**

```cmake
add_dual_backend_test(test_dual_backend_64_bit test_dual_backend_64_bit.cpp)
add_dual_backend_test(test_dual_backend_clock test_dual_backend_clock.cpp)
add_dual_backend_test(test_dual_backend_reset test_dual_backend_reset.cpp)
```

- [ ] **Step 3: build + run**

Run:
```bash
cmake --build build -j$(nproc) 2>&1 | grep -E "error|Error" | head
ctest -L verilator_e2e -LE perf -R "test_dual_backend_(64_bit|clock|reset)" --output-on-failure
```
Expected: 3 tests PASS (interpreter) + 96-bit's verilator half WARN+SKIP.

- [ ] **Step 4: Defer commit**

---

### Task 5: 批量迁 7+ 基础测试

**Files:**
- Modify: `tests/test_module.cpp` (ch_module 段)
- Modify: `tests/test_reg_timing.cpp` (整文件)
- Modify: `tests/test_simple_io.cpp` (有 Simulator 的 case)
- Modify: `tests/test_io_*.cpp` (按 R6 grep 筛)
- Modify: `tests/test_ch_stream.cpp` (流数据段)

- [ ] **Step 1: 跑 baseline**

Run: `ctest -LE perf --output-on-failure | tail -3`
Expected: baseline 148/148 PASS.

- [ ] **Step 2: 按 design.md §5 名单逐个迁**

每个文件按 Task 3 Step 2 的方式改 TEST_CASE→DUAL_BACKEND_TEST（去掉 sim/dev 构造，使用 macro 注入的 `sim`/`dut`）。

- [ ] **Step 3: 逐个验证**

每改一个文件：
```bash
cmake --build build --target test_X -j$(nproc) 2>&1 | grep -E "error" | head
ctest -R "^test_X$" --output-on-failure
```

- [ ] **Step 4: 全量回归**

Run: `ctest -LE perf --output-on-failure | tail -3`
Expected: 148/148 PASS（interpreter 路径不变）。

- [ ] **Step 5: 统计迁移数**

Run: `grep -rlE "DUAL_BACKEND_TEST\(.*\[\"" tests/ | wc -l`
Expected: ≥ 8 (per R6 + design.md §5 目标)。

- [ ] **Step 6: Defer commit**

---

### Task 6: 9 个现有 verilator 测试加 `verilator_e2e` 标签

**Files:**
- Modify: `tests/CMakeLists.txt` (9 处 `set_tests_properties` 加 `LABELS "base;verilator_e2e"`)

- [ ] **Step 1: 列 9 个测试**

目标测试（按 `tests/CMakeLists.txt` 全文 grep `RESOURCE_LOCK "verilator_backend"`）:
1. `test_axi_lite_verilator_e2e` (line 89)
2. `test_verilator_backend` (line 363)
3. `test_verilator_e2e_harness` (line 371)
4. `test_verilator_trace_parity` (line 381)
5. `test_verilator_bundle_e2e` (line 392)
6. `test_verilator_stream_e2e` (line 403)
7. `test_verilator_memory_e2e` (line 416)
8. `test_verilator_complex_e2e` (line 429)
9. `test_verilog_gen` (line 337) — 注入 `_vl_path_env` 但无 RESOURCE_LOCK，**可选**

- [ ] **Step 2: 改 8 个 `set_tests_properties` 追加 `;verilator_e2e`**

每个 `set_tests_properties(test_x PROPERTIES LABELS "base" RESOURCE_LOCK "verilator_backend" ENVIRONMENT "${_vl_path_env}")` 改为 `LABELS "base;verilator_e2e"`（按 R6：CMake `set_tests_properties` 的 LABELS 是覆盖，必须保留 base 同时加新 label）。

`test_verilator_memory_e2e` 还有 `TIMEOUT 300` / `test_verilator_complex_e2e` 有 `TIMEOUT 600` — 保留。

- [ ] **Step 3: 加 4 行/文件顶注释**

每个测试 .cpp 文件首行加：
```cpp
// Reference implementation for DUAL_BACKEND_TEST macro (see openspec/specs/dual-backend-test-coverage).
// Tag [verilator][e2e] and LABELS verilator_e2e added by verilator-parity-all-tests Phase 1.
```
（注：实际 diff 行数 ≤ 4 行/文件：1 行 LABELS 改 + 1 行 top comment + 1 行空行 + 1 行改后 alignment）

- [ ] **Step 4: 验证**

Run:
```bash
cmake --build build -j$(nproc) 2>&1 | grep -E "error" | head
ctest -N -L verilator_e2e 2>&1 | tail -1
ctest -LE perf -L verilator_e2e --output-on-failure 2>&1 | tail -3
```
Expected: 0 errors; `ctest -N -L verilator_e2e` ≥ 12 tests; 现有 9 个 pass。

- [ ] **Step 5: Defer commit**

---

### Task 7: ci.yml 新增 `pr-verilator-fast` job

**Files:**
- Modify: `.github/workflows/ci.yml`

- [ ] **Step 1: 写失败测试（验证 ci.yml 缺该 job）**

Run: `grep -n "pr-verilator-fast" .github/workflows/ci.yml`
Expected: empty (job 不存在).

- [ ] **Step 2: 写最小实现（添加 job）**

在 `pr-build` job 后追加（`pr-build` 当前为 `BUILD_VERILATOR=OFF` 快速反馈，新 job 与其并行）：

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
      - name: Configure (BUILD_VERILATOR=ON, JIT off)
        run: cmake -B build -DBUILD_VERILATOR=ON -DCH_JIT_ENABLE=OFF
      - name: Build
        run: cmake --build build -j$(nproc)
      - name: Run verilator_e2e non-perf
        run: cd build && ctest -L verilator_e2e -LE perf --output-on-failure
      - name: Run interpreter baseline (zero regression)
        run: cd build && ctest -LE perf --output-on-failure
```

- [ ] **Step 3: yaml 语法 lint**

Run: `python3 -c "import yaml; yaml.safe_load(open('.github/workflows/ci.yml'))"`
Expected: 0 errors。

- [ ] **Step 4: Defer commit**

---

### Task 8: 实测墙钟 + 同步 CTEST_TOTAL

**Files:**
- Modify: `AGENTS.md` (`CTEST_TOTAL: <TBD>` 回填实测值)

- [ ] **Step 1: 测 CTEST_TOTAL**

Run: `cd build && ctest -N | tail -1`
Expected: 输出形如 `Total Tests: <N>` (N = 159 + 新增数 = 159 + 4 = 163, 视实际是否去重)。

- [ ] **Step 2: 测 verilator_e2e count**

Run: `cd build && ctest -N -L verilator_e2e -LE perf | tail -1`
Expected: `Total Tests: <M>` (M ≥ 12)。

- [ ] **Step 3: 测墙钟**

Run: `time (cd build && ctest -L verilator_e2e -LE perf)`
Expected: ≤ 45 min (R10 锁定；超出触发 proposal §Open Q 1 选项 A 自动降级 — 暂延后 Phase 2-5)。

- [ ] **Step 4: 回填 AGENTS.md**

把根 `AGENTS.md §NOTES` 的 `CTEST_TOTAL: 159` 改为实测值，并更新 NOTES 段（baseline 同步、verilator_e2e 列表等）。

- [ ] **Step 5: Defer commit**

---

### Task 9: archive 前置 gate

**Files:**
- (无新文件；全验证)

- [ ] **Step 1: openspec validate**

Run:
```bash
openspec validate --specs --strict  # 14/14 PASS
openspec validate verilator-parity-all-tests --strict --type change  # is valid
```

- [ ] **Step 2: 零回归**

Run:
```bash
ctest -LE perf --output-on-failure  # 159/159 PASS
ctest -L perf --output-on-failure   # 11/11 PASS
```

- [ ] **Step 3: `verify_test_counts.sh`**

Run: `bash scripts/verify_test_counts.sh`
Expected: exit 0。

- [ ] **Step 4: 9 个现有测试 diff ≤ 4 行/文件**

Run:
```bash
git diff --stat tests/test_verilator_*.cpp tests/test_axi_lite_verilator_e2e.cpp 2>&1 | tail -10
```
Expected: 每文件 ≤ 4 行 diff。

- [ ] **Step 5: Defer commit（archive 阶段统一提交）**

---

## Notes for implementer

- **CH_JIT_ENABLE=OFF**: per R8，pr-verilator-fast job 必须关 JIT（`cmake -DCH_JIT_ENABLE=OFF`），避免 `is_native()=false` 的 InterpreterBackend 被 JIT 替换时触发双重保险逻辑中的 `set_jit_enabled(false)` noop。
- **template 形参含逗号**: `DUAL_BACKEND_TEST(MyAdder<32>, "name", "[tags]") { ... }` 的 `MyAdder<32>` 无逗号，但若 `MyAdder<ch_uint<32>>` 含 `,` 需用 typedef 或 `using` 别名。
- **verilator cache**: 每次 test 用 `make_temp_dir("_dual_backend")` 创建独立 workdir，cache 不共享（避免污染）。verilator cold compile 出现在首次 run，后续走 cache。
- **Macro 自证 96-bit 测试**: `CounterWide<96>` 的 verilator half 必然 SKIP（per root AGENTS.md §VERILATOR BACKEND RULES），interpreter 仍跑全 → 验证 macro 优雅降级路径。
- **不重写 9 个现有测试体**: per design.md §8 反向约束（verilator-e2e-coverage spec file-level/count-level contract）。
