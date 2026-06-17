# CppHDL

> C++20 hardware description library for high-level HDL development. SpinalHDL patterns compiled to C++.

## Stack
C++20, CMake, Catch2 v3.7.0, clangd (LSP), SpinalHDL-style API

## Structure
```
CppHDL/
├── include/          # Header-only HDL library
│   ├── core/         # Core primitives: context, operators, IO, lnode
│   ├── ast/          # AST nodes + instruction classes
│   ├── jit/          # JIT compiler interface (AGENTS.md)
│   ├── chlib/        # HDL components: stream, fifo, pipeline, arbiter
│   ├── bundle/       # Bundle type definitions (AXI, Stream, Flow)
│   ├── lnode/        # Logic node implementations
│   ├── bv/           # Bit-vector math (gmp/mpfr wrapper)
│   ├── axi4/         # AXI4-Lite bundle + interconnect
│   └── cpu/          # Cache, pipeline, RV32I
├── src/              # Library implementations
│   ├── ast/          # Instruction eval() implementations
│   ├── core/         # Context + lnodeimpl
│   ├── jit/          # JIT compiler implementation
│   └── *.cpp         # CodeGen (Verilog/DAG), Simulator, Component
├── samples/          # 40 HDL pattern demos (bundles, streams, protocols)
├── examples/         # Full design examples
│   ├── spinalhdl-ported/  # 17 SpinalHDL port examples (each with main())
│   ├── axi4/              # AXI4 bus demos
│   ├── stream/            # Stream Mux/Demux/Arbiter/Fork
│   └── riscv-mini/        # RV32I 5-stage pipeline (partial)
├── tests/            # 111 Catch2 test files (83 base + 28 chlib)
│   └── chlib/              # Component library tests
└── docs/             # Architecture plans, usage guides, phase reports
```

## WHERE TO LOOK
| Task | Location |
|------|----------|
| Core types (ch_bool, ch_uint) | `include/core/operators.h` (aggregator) + `include/core/operators_{width,arith,logic,compare,shift}.h` |
| IO ports (ch_in, ch_out) | `include/core/io.h` (port classes + `<<=` + 4 CHREQUIRE) + `include/core/io_{logical,arithmetic,shift,lit}.h` |
| Context + simulation | `include/core/context.h`, `src/simulator.cpp` + `src/simulator_{init,trace}.cpp` |
| Stream/Flow operators | `include/chlib/stream.h`, `include/chlib/stream_operators.h` |
| FIFO | `include/chlib/fifo.h` |
| Bundle types | `include/bundle/` (stream_bundle, axi_bundle, flow_bundle) |
| State machine DSL | `include/chlib/state_machine.h` |
| If/else statement DSL | `include/chlib/if_stmt.h` |
| Verilog codegen | `include/codegen_verilog.h`, `src/codegen_verilog.cpp` |
| JIT compiler | `src/jit/jit_compiler.cpp`, `include/jit/AGENTS.md` |
| AXI4 interconnect | `include/axi4/` (AGENTS.md), `include/axi4/peripherals/` |
| AXI4 examples | `examples/axi4/` (AGENTS.md), `examples/axi4/axi4_lite_example.cpp` |
| Stream examples | `examples/stream/` (functional API — no Component subclass), `examples/spinalhdl-ported/stream/` (moved to sibling) |
| Verilator backend (ADR-035) | `include/core/verilator_backend.h`, `src/core/verilator_backend.cpp`, `tests/test_verilator_backend.cpp` |
| Verilator submodule integration | `docs/developer_guide/verilator-integration.md` + `third_party/verilator/` (git submodule) + `scripts/init-submodules.sh` |

## CONVENTIONS
- **IO assignment**: Use `select(condition, val1, val0)` NOT `&&` on IO ports
- **Module instantiation**: Use `ch::ch_module<T>` NOT `CH_MODULE` macro for templates
- **Testbench input**: Use `sim.set_input_value(port, value)` NOT `device.io().port = value`
- **Test context**: Always `ch::core::context ctx("name")` + `ch::core::ctx_swap swap(&ctx)`
- **Bundle as IO**: Use individual `ch_bool`/`ch_uint` ports, NOT bundle struct
- **Include aggregator**: Use `#include "ch.hpp"` (library) or `#include "chlib.h"` (chlib components)
- **Verilator install path**: `apt install verilator` is **WRONG** — Verilator comes from `third_party/verilator/` submodule, built by `cmake --build build --target verilator` (ExternalProject_Add, ~10-30 min cold). See `docs/developer_guide/verilator-integration.md`.

## COMPONENT LIFECYCLE PATTERNS
| Scenario | Pattern | Example |
|----------|---------|---------|
| **Standalone test** | `ch::ch_device<T>` | `ch::ch_device<HazardUnit> hazard;` |
| **Inside Component::describe()** | `ch::ch_module<T>` | `ch::ch_module<IfStage> if_stage{"if_stage"};` |
| **Top-level wrapper** | `ch::ch_device<T>` in main() | `ch::ch_device<Rv32iSoc> soc;` |

**CRITICAL**: `ch::ch_module<T>` **MUST** be created inside a `Component::describe()` method. It requires `Component::current()` to return a valid parent. Using it in standalone test functions causes silent failure and SIGSEGV.

```cpp
// ✅ CORRECT: Standalone test uses ch_device
TEST_CASE("HazardUnit test", "[hazard]") {
    ch::ch_device<HazardUnit> hazard;
    ch::Simulator sim(hazard.context());
    sim.set_input_value(hazard.instance().io().rs1_data_raw, 0);
    sim.tick();
}

// ✅ CORRECT: ch_module inside Component::describe()
class MyTop : public ch::Component {
    void describe() override {
        ch::ch_module<HazardUnit> hazard{"hazard"};  // Works - has parent
        ch::ch_module<IfStage> if_stage{"if_stage"};
        // ...
    }
};

// ❌ WRONG: ch_module in standalone test - SIGSEGV
TEST_CASE("Bad pattern", "[bad]") {
    ch::ch_module<HazardUnit> hazard{"hazard"};  // Fails - no parent Component
    // "Child component has been destroyed unexpectedly in io()!"
}
```

See `docs/developer_guide/patterns/COMPONENT-LIFECYCLE-GUIDE.md` for full details.

## ANTI-PATTERNS
- Direct IO port assignment: `io().port = value` (compiles but silently does nothing)
- `&&` on IO ports: use `select()` instead
- `CH_MODULE` with templates: use `ch::ch_module<T>`
- `ch_module` in standalone tests: use `ch_device` instead
- Empty TEST_CASE: use SKIP() or delete
- Missing ctx_swap: register ops segfault without it

## JIT COMPILER RULES (CRITICAL)
The JIT compiler (`src/jit/jit_compiler.cpp`) compiles HDL operations to native code. These rules prevent silent data corruption:

- **New `ch_op` → must add `JitOp`**: Adding an operation to `include/core/lnodeimpl.h` enum `ch_op` requires: (1) `JitOp` entry in `include/jit/jit_ir.h`, (2) mapping in `generate_ir()` switch, (3) LLVM lowering in `compile_to_llvm()` switch. Skipping any causes `CALL_EXTERNAL` fallback and **silently stale values** in downstream JIT-native nodes.
- **`type_input` must check driver**: Connected input ports (via `<<=`) have `num_srcs()>0`. JIT must load from `src(0)->id()` (the driver), not `node_id`. Otherwise Component IO hierarchy silently reads zero.
- **`type_lit` uses `continue`, never `break`**: In the `for (auto* node : eval_list)` loop; `break` exits the entire loop.
- **All <64-bit arithmetic must mask**: After ADD/SUB/MUL/SHL etc., apply `AND` with `(1<<bw)-1` bitmask.
- **See**: `include/jit/AGENTS.md`, `docs/developer_guide/patterns/JIT-DEBUGGING-GUIDE.md`

## ZERO-DEBT POLICY
Every Phase must exit with **zero technical debt** before marking complete:
- **No dead code**: Delete stubs, skeleton files, and unused includes immediately. Never "leave for later".
- **No compile warnings**: Treat warnings as errors. `-Wall -Wextra -Wpedantic` clean on all changed files.
- **No failing tests**: Red → fix immediately. Green is the only acceptable state.
- **No undocumented changes**: API additions/modifications require updated docs + inline comments.
- **No orphan files**: Every `.cpp` must be registered in CMake. Every header must be included somewhere.

## ARCHITECTURE CONSISTENCY
Long-term maintainability relies on strict pattern adherence:
- **SpinalHDL API parity**: `ch_bool`, `ch_uint<N>`, `ch_stream<T>`, `ch_reg<T>` — match SpinalHDL naming exactly.
- **IO direction semantics**: `ch_in<T>` (input from TB), `ch_out<T>` (output to TB). Never swap.
- **Component hierarchy**: `Component` → `Module` → `Device`. Never skip levels.
- **Simulation lifecycle**: `context` → `ctx_swap` → build graph → `Simulator` → `tick()`/`run_*()` → restore context.
- **Include discipline**: Headers self-contained; no `.cpp` include of other `.cpp`.

## PHASE COMPLETION GATE
A Phase is **NOT** complete until ALL three pass:

| Gate | Criteria | Verification Command |
|------|----------|---------------------|
| 🟢 **编译通过** | `cmake --build` 0 errors, 0 warnings on new files | `cmake --build build -j$(nproc) 2>&1 \| grep -E "error\|Error"` |
| 🟢 **测试覆盖** | All new features tested, all existing tests pass | `ctest -L base --output-on-failure` + `./run_all_ported_tests.sh` |
| 🟢 **文档同步** | AGENTS.md / usage-guide updated for new APIs | Review section updated within same commit |

## COMMANDS
```bash
# Build
cmake -B build && cmake --build build -j$(nproc)

# Run all example tests
./run_all_ported_tests.sh

# Run Catch2 tests
ctest --output-on-failure

# Run specific example
./build/examples/stream/stream_mux_demo
```

## NOTES
- 141 CTest tests registered (1 pre-existing timeout: `perf_tests` ~120s; pass count rerun before claiming)
- 28 main() examples tracked by `run_all_ported_tests.sh` (28/28 pass)
- riscv-mini: Pipeline compile-time fixes complete, runtime requires ch_device wrapper
- I2C controller is simplified (no ACK handling)
- **Verilator 三路 perf 对比** (interpreter / JIT / Verilator) 默认要求 `BUILD_VERILATOR=ON`；CI/快速迭代用 `-DBUILD_VERILATOR=OFF`，Verilator 列在 perf 报告中显示 `UNSUPPORTED`（详见 `docs/developer_guide/verilator-integration.md`）

## C-CLASS REFACTOR (2026-06, completed)

Major refactor splitting the 7 largest hot files into focused, single-responsibility units. **Pure structural decomposition** — no behavior changes, public API unchanged. See `.omo/plans/c-class-major-refactor.md` for the full plan.

### Aggregator pattern
The refactor introduces a standard **aggregator pattern** for hot headers: a slim aggregator file `#include`s per-category files and provides forward declarations for cross-category templates. The per-category files hold the actual implementation.

| Hot file (was) | Aggregator (now) | Per-category files |
|----------------|-----------------|---------------------|
| `include/core/operators.h` (933) | 157 lines | `operators_width.h` (233), `operators_arith.h` (344), `operators_logic.h` (198), `operators_compare.h` (48), `operators_shift.h` (95) |
| `include/core/io.h` (868) | 500 lines* | `io_logical.h` (177), `io_arithmetic.h` (42), `io_shift.h` (44), `io_lit.h` (145) |
| `include/ast/instr_op.h` (714) | 350 lines** | `instr_op_arith.h` (97), `instr_op_logic.h` (100), `instr_op_compare.h` (100), `instr_op_shift.h` (120) |
| `src/simulator.cpp` (1226) | 536 lines | `simulator_trace.cpp` (514), `simulator_init.cpp` (209) |

\* `io.h` retained port class definitions + `operator<<=` + the 4 CHREQUIRE assertions from `bc0cbdb` (ADR-010 Q3 port validation), so it can't be reduced to <100 lines without invasive refactoring.
\** `instr_op.h` retained the 3 template wrapper classes (`instr_op_binary`, `instr_op_unary`, `instr_op_ternary`) + width/concat ops + special ops + all 28+ type aliases, so it can't be reduced to <250 lines without breaking the public API.

### describe() method splits
5 monolithic `Component::describe()` methods broken into per-concern helper methods:

| File | Helpers extracted |
|------|-------------------|
| `axi_spi.h` | `build_clock_generator()`, `build_transfer_fsm()`, `build_interrupt_logic()` |
| `axi_i2c.h` | `build_register_bank()`, `build_i2c_fsm()` |
| `axi4_full.h` | `build_id_handshake()`, `build_data_channels()`, `build_response_logic()` |
| `axi_interconnect_4x4.h` | `build_address_map()`, `build_arbiter()`, `build_decoder()` (doc-only), `build_mux()` |
| `rv32i_pipeline.h` | `build_datapath()`, `build_control_signals()`, `build_forwarding_paths()` |

### Rollback tags
| Tag | Phase |
|-----|-------|
| `pre-jit-split` | Before Phase 0 |
| `interpreter-arith-fix` | Pre-Phase 1 fix (interpreter arithmetic width) |
| `ch-out-truncation-fix` | Pre-Phase 1 fix (ch_out truncation) |
| `post-jit-split` | After Phase 1 (JIT split) |
| `post-bv-split` | After Phase 2 (bv.h + bitvector.h split) |
| `post-operators-split` | After Phase 3 (operators.h split) |
| `post-io-split` | After Phase 4 (io.h split) |
| `post-describe-split` | After Phase 5 (5 describe() splits) |
| `post-simulator-split` | After Phase 6 (simulator.cpp split) |
| `post-ast-split` | After Phase 7 (instr_op.h split) |
| `c-class-refactor-complete` | After Phase 8 (final integration + this doc update) |

Use `git checkout <tag>` to roll back to any phase boundary.

- **macOS CI status (post PR-16 followup)**: macOS-14 enabled (clang/arm64 compile errors fixed). Windows-2022 still deferred (one-line matrix change when MSVC C++20 module edge cases are addressed). JIT path validated nightly (CH_JIT_ENABLE=ON default, -DCH_JIT_ENABLE=OFF only applies to PR-feedback matrix per design notes in .github/workflows/ci.yml).

## 已消除的过时信息
- "7 个禁用测试" → fix-test-completeness 计划将 4 个被注释的 `add_catch_test` 重新启用（`test_branch_predictor`、`test_cache_pipeline`、`test_phase4_chlib`、`test_rv32i_pipeline`），目标 0 禁用
- `test_mod_width_simple` → 已被 `test_mod_width` 覆盖，冗余
- `test_ch_flow`, `test_stream_width_adapter`, `test_branch_predictor_v2` → 已注册且通过

<!-- code-review-graph MCP tools -->
## MCP Tools: code-review-graph

**IMPORTANT: This project has a knowledge graph. ALWAYS use the
code-review-graph MCP tools BEFORE using Grep/Glob/Read to explore
the codebase.** The graph is faster, cheaper (fewer tokens), and gives
you structural context (callers, dependents, test coverage) that file
scanning cannot.

### When to use graph tools FIRST

- **Exploring code**: `semantic_search_nodes` or `query_graph` instead of Grep
- **Understanding impact**: `get_impact_radius` instead of manually tracing imports
- **Code review**: `detect_changes` + `get_review_context` instead of reading entire files
- **Finding relationships**: `query_graph` with callers_of/callees_of/imports_of/tests_for
- **Architecture questions**: `get_architecture_overview` + `list_communities`

Fall back to Grep/Glob/Read **only** when the graph doesn't cover what you need.

### Key Tools

| Tool | Use when |
| ------ | ---------- |
| `detect_changes` | Reviewing code changes — gives risk-scored analysis |
| `get_review_context` | Need source snippets for review — token-efficient |
| `get_impact_radius` | Understanding blast radius of a change |
| `get_affected_flows` | Finding which execution paths are impacted |
| `query_graph` | Tracing callers, callees, imports, tests, dependencies |
| `semantic_search_nodes` | Finding functions/classes by name or keyword |
| `get_architecture_overview` | Understanding high-level codebase structure |
| `refactor_tool` | Planning renames, finding dead code |

### Workflow

1. The graph auto-updates on file changes (via hooks).
2. Use `detect_changes` for code review.
3. Use `get_affected_flows` to understand impact.
4. Use `query_graph` pattern="tests_for" to check coverage.
