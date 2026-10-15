# CppHDL

> C++23 hardware description library for high-level HDL development. SpinalHDL patterns compiled to C++.

## Stack
C++23, CMake, Catch2 v3.7.0, clangd (LSP), SpinalHDL-style API

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
├── tests/            # Catch2 unit tests (counts in NOTES §Test counts)
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
| Verilog codegen | `include/codegen_verilog.h`, `src/codegen_verilog.cpp` (SystemVerilog 2017: `always_ff` + `logic`) |
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
- **Mem read port**: `aread` emits combinational `assign` in Verilog (0-cycle); `sread` emits `always_ff` (1-cycle). In Simulator, both are evaluated at posedge in `sequential_instr_list_`; downstream registers in the consumer provide the required latency. `imem` must use `sread` to maintain `pc_lag` alignment. See `docs/simulation/ARCHITECTURE.md` "Memory read port timing".

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

## OPENSPEC WORKFLOW
The project is `schema: spec-driven` (see `openspec/config.yaml`). `openspec/specs/` is the **current spec source of truth** — never bypass it.

- **Directory layout (OpenSpec 1.4.0)**: 主规范使用 capability 文件夹结构 — `openspec/specs/<capability>/spec.md`。**禁止**使用扁平的 `openspec/specs/<capability>.md`（CLI 不识别）。详见 `docs/developer_guide/openspec-specs-guide.md`。
- **Archive must merge specs**: `openspec archive <name>` 默认会合并 `openspec/changes/<name>/specs/<capability>/spec.md` 的 `## ADDED Requirements` 到 `openspec/specs/<capability>/spec.md`。**禁止**使用 `--skip-specs` 标志，除非该 change 显式声明 "no spec changes"（参见 `fix-ns-pollution` 模式）。
- **Spec 变更必须走 change 流程**：直接修改 `openspec/specs/<capability>/spec.md` 是禁止的；先创建 change 提案，archive 时由 CLI 自动合并。
- **Spec 内容规范**：每条 requirement 描述**必须**含 `SHALL` 或 `MUST` 关键字（RFC 2119）；`## Purpose` 与 `## Requirements` 必须是 heading 而非引用块。
- **CI 校验**：`openspec validate --specs --strict` 必须在 archive 前通过。当前 `openspec/specs/` 包含 `chlib-aggregator/`（2 requirements，来自 `2026-06-17-fix-include-aggregation`）。
- **See**: `docs/developer_guide/openspec-specs-guide.md`（完整格式规范与迁移历史）

## JIT COMPILER RULES (CRITICAL)
The JIT compiler (`src/jit/jit_compiler.cpp`) compiles HDL operations to native code. These rules prevent silent data corruption:

- **New `ch_op` → must add `JitOp`**: Adding an operation to `include/core/lnodeimpl.h` enum `ch_op` requires: (1) `JitOp` entry in `include/jit/jit_ir.h`, (2) mapping in `generate_ir()` switch, (3) LLVM lowering in `compile_to_llvm()` switch. Skipping any causes `CALL_EXTERNAL` fallback and **silently stale values** in downstream JIT-native nodes.
- **`type_input` must check driver**: Connected input ports (via `<<=`) have `num_srcs()>0`. JIT must load from `src(0)->id()` (the driver), not `node_id`. Otherwise Component IO hierarchy silently reads zero.
- **`type_lit` uses `continue`, never `break`**: In the `for (auto* node : eval_list)` loop; `break` exits the entire loop.
- **All <64-bit arithmetic must mask**: After ADD/SUB/MUL/SHL etc., apply `AND` with `(1<<bw)-1` bitmask.
- **See**: `include/jit/AGENTS.md`, `docs/developer_guide/patterns/JIT-DEBUGGING-GUIDE.md`

## VERILATOR BACKEND RULES (CRITICAL)
The Verilator backend (`src/core/verilator_backend.cpp`) dlopen's a compiled Vtop shared library and drives its ports from the CppHDL data_map. The following contracts prevent silent data corruption and performance regression:

- **Port bitwidth limit: 64 bits**. Any `ch_in<ch_uint<N>>` or `ch_out<ch_uint<N>>` with `N > 64` causes `initialize()` to return `false` with `CHERROR`. Verilator's QData/UData accessor model in `emit_sim_main_postlude` casts Vtop fields to `QData`, silently truncating high bits of `VlWide` vectors. `type_clock` and `type_reset` are infrastructure (1-bit by convention) and are NOT subject to this check — `node->size()` returns garbage for them and must not be validated.
- **Cache write-back is mandatory on cache miss**. After `invoke_verilator()` succeeds, the freshly built `libVtop.so` must be copied to `$HOME/.cache/cpphdl/verilator/<key>/libVtop.so` via `writeback_to_cache()`. Skipping this makes the SHA-1 cache a no-op (every `initialize()` recompiles — minutes per call). The cache key is computed from verilog source + sim_main template + flags + verilator version (`compute_cache_key()`).
- **mkdir for cache must be recursive**. `mkdir(2)` is single-level; `~/.cache/cpphdl/verilator/<key>/` requires creating `~/.cache/`, `~/.cache/cpphdl/`, `~/.cache/cpphdl/verilator/`, and the key dir in sequence. The implementation walks `dst` and `mkdir`s each `/`-separated segment.
- **`build_port_access_table()` returns `bool`**. False means the design was rejected (currently only for wide ports). The function must end with `return true;` — falling off without returning is UB and crashes with SIGFPE in `if (!build_port_access_table())` checks.
- **2-step clock toggle in `eval_sequential`** (`clk=1` → `eval_fn_` → `clk=0` → `eval_fn_`). The clock Vtop field is written directly via `clock_field_ptr_` (NOT round-tripped through `data_map_`) so Verilator's `VlClockSig`/`__Vclklast__` edge-detection sees a clean 0→1→0 sequence per cycle. `sync_inputs_to_vtop(exclude_clock=true)` skips clock/reset (they are toggled directly); `sync_outputs_from_vtop()` skips clock/reset (they are infrastructure, never user-visible). Without the 2-step toggle, `always_ff @(posedge clk)` blocks never fire and counter-style regressions silently read 0.
- **initialize() MUST pulse default_reset** (1 → eval → 0 → eval). The reset pulse loads `ch_reg<ch_uint<N>>(INIT_d)` init values into the Verilog `always_ff @(posedge clk or posedge default_reset)` reset branch via the `@(posedge default_reset)` edge. Without this pulse, `VerilatorBackend` returns `0 + add-addend` after `tick(1)` on designs with `INIT != 0` (verilator-init-zero default), whereas `InterpreterBackend` returns `INIT + add-addend` (interpreter assigns `*init_buf` at register construction). `try_initialize()` and `reset()` MUST share the same 4-step sequence via `pulse_reset()` — implementing them separately leads to trace-parity bugs that are subtle to debug. Idempotent across repeated `initialize()` calls (same INIT loaded N times → final state == INIT).
- **`ch_mem` imem in RV32I / RISC-V fixtures MUST use sread (not aread)**. `mem.sread(addr, enable=true)` is posedge-aligned and trace-parity-safe with both InterpreterBackend and VerilatorBackend. `mem.aread(addr)` is 0-cycle combinational in Verilog (Verilator returns the value in the same cycle as the address change) but posedge-sampled in the Simulator (the value lands in the next cycle's data_map_) — a 1-cycle skew that breaks trace parity. See `verilator-e2e-coverage` spec §Requirement 1 invariant 3.
- **Hierarchical input driver MUST be emitted by codegen**. A submodule `ch_in` with an internal driver (`sub.io().x <<= some_wire` invokes `inputimpl::set_driver`, equivalent to `num_srcs() > 0`) MUST NOT be emitted as a top-level Verilog `input` port — it becomes an internal `logic` wire (`src/codegen_verilog.cpp::print_header` filters these out), declared by `print_decl`, and assigned from its driver in `print_logic` (walks the unnamed-proxy chain, mirroring `type_output`). The matching backend code MUST also skip them: `emit_sim_main_postlude` skips the `set_input_Vtop` / `get_field_ptr_Vtop` case (the Vtop field does not exist), and `build_port_access_table` does not add the port to `port_access_` (`sync_inputs_to_vtop` won't try to write it). Skipping any of the four sites makes verilator either reference a non-existent Vtop field (compile/link error) or keep the port at X (=0 after `initialize()`'s reset pulse), silently disabling every `ch_in` below the top component — RV32I `instr_data` becomes NOP (commit_count=0), AXI4-Lite `slave.awvalid` triggers handshake deadlock (rvalid=0). This was originally misdiagnosed as issue #25 register propagation. JIT already follows the same rule (`src/jit/jit_ir_gen_lnode_io.cpp:31-46`); the verilog codegen path is now in parity.

## KNOWN LIMITATIONS
Documented gaps that are by-design or awaiting redesign. Each entry points to authoritative docs (ADR / usage guide) to avoid drift between this file and the linked source.

- **`Simulator::set_ab_verification()` is a no-op stub** (ADR-035 R9, `docs/usage_guide/10-verilator-backend.md:133`). After the dual-function JIT refactor (`tick_comb` + `tick_seq`), the A/B comparison logic was not redesigned — enabling the flag only emits a `CHWARN` per `tick()` (`src/simulator.cpp:332-338`) but does not actually diff interpreter vs JIT `data_map_`. Re-enabling dual-backend comparison requires saving snapshots around the JIT `eval_fn_` call and reconciling per-instruction. Do NOT depend on `is_ab_verification()` returning meaningful parity status. The whole `set_ab_verification` block is guarded by `#if __has_include("jit/jit_compiler.h")` (`include/simulator.h:90-97`); with `CH_JIT_ENABLE=OFF` the symbol is absent.
- **Cache stubs `i_cache_stub.h` / `d_cache_stub.h` are pass-through** (no real Hit/Miss or LRU). Both classes are marked `[[deprecated]]` (2026-10-17 deprecate-cache-stubs). Semantics: `ready := req, miss := false, data := axi_data`. The full implementation lives in `include/cpu/cache/{i,d}_cache_complete.h` and is what `examples/riscv-mini/src/{i_cache_prefetch.h,d_cache_write_buffer.h}` actually instantiate. If you need a real cache, use the `_complete` variants directly. **Do not co-include the stub with the `_complete` header** — both define `chlib::ICacheConfig` / `chlib::DCacheConfig` and the ODR clash breaks the build. See `include/cpu/AGENTS.md` for the canonical key-files table.

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
<!-- CI anchor: the next line is the single-source-of-truth CTest total.
     scripts/verify_test_counts.sh and the CI step in .github/workflows/ci.yml
     parse this line. Update this number (and the matching ctest -N output)
     together when adding/removing tests. Do not reformat. -->
- CTEST_TOTAL: 159
- **Test counts (single source of truth — re-verify before quoting in PRs/docs)**
  - **159 CTest tests registered** as of 2026-10-15. Verify: `cd build && ctest -N | tail -1`.
  - **148 non-perf CTest** (= 159 − 11 perf). Verify: `cd build && ctest -N -LE perf | tail -1`.
  - **91 Catch2 test source files** at `tests/test_*.cpp` (all registered; 3 bit-rot orphans — `test_memory_leak.cpp` / `test_memory_optimization.cpp` / `test_memory_performance.cpp` — deleted 2026-10-15 because they used a pre-refactor `ch_reg` API that no longer compiles; see `tests/CMakeLists.txt:279-281`).
  - **27 Catch2 test source files** at `tests/chlib/test_*.cpp` (all registered).
  - **3 Catch2 test source files** at `tests/jit/test_*.cpp` (registered separately).
  - **40 sample programs** at `samples/*.cpp`.
  - **32 main()-bearing examples** in `examples/`. **28 are tracked** by `run_all_ported_tests.sh`; the remaining 4 (`examples/verilator/0{1,2,3}_*.cpp` + `examples/spinalhdl-ported/testbench_template.cpp`) are demos not run by CI.
  - Verify: `ls tests/test_*.cpp tests/chlib/test_*.cpp tests/jit/test_*.cpp | wc -l` (= 121) and `grep -rl '^int main' examples --include='*.cpp' | wc -l` (= 32).
- `perf_tests` runs `perf_tests --all` (F2 subprocess-isolated), wall-clock ~15-20 min; TIMEOUT 1800 in CMake. Pass count rerun before claiming.
- 28 main() examples tracked by `run_all_ported_tests.sh` (28/28 pass)
- riscv-mini: Pipeline compile-time fixes complete, runtime requires ch_device wrapper
- I2C controller is simplified (no ACK handling)
- **Verilator 三路 perf 对比** (interpreter / JIT / Verilator) 默认要求 `BUILD_VERILATOR=ON`；CI/快速迭代用 `-DBUILD_VERILATOR=OFF`，Verilator 列在 perf 报告中显示 `UNSUPPORTED`（详见 `docs/developer_guide/verilator-integration.md`）
- **`perf_three_way` ctest 入口**（`ctest -L verilator -R perf_three_way`）专跑 TC-07（XOR 链 depth=10/100/1000）+ TC-08（寄存器链）的三路子集 subprocess 隔离对比，TIMEOUT 360。完整三路 perf 仍走 `perf_tests --all`（TIMEOUT 1800）。修复 TC-07 depth=1000 verilator 编译失败：`src/codegen_verilog.cpp::get_literal_str` 单参数版本对字面量值按宽度 mask，避免 `ch_uint<N>(literal)` 触发 Verilator "Too many digits for N bit number" 语法错误（width=64 路径保留 `value` 原值，符合 ADR-035 64-bit port cap）

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
- **2026-10-15 cleanup-orphan-code** → 8 个孤儿文件删除（`include/bundle.h`, `include/chlib/axi4lite.h`, `tests/test_branch_predict.cpp`, root `tests/test_forwarding.cpp`, `tests/test_pipeline_regs.cpp`, `tests/test_memory_leak.cpp` / `test_memory_optimization.cpp` / `test_memory_performance.cpp`, `tests/chlib/test_axi4lite.cpp`）；同步 6 个 AGENTS.md 文件到 single-source-of-truth 测试计数；新增 `scripts/verify_test_counts.sh` 作为 CI 防 drift 护栏。`CTEST_TOTAL: 159` (= 148 non-perf + 11 perf)。
- **2026-10-15 deprecate-hdl-logical-operators 首次尝试 rollback** → 加 `[[deprecated]]` 时触发 315 个 `-Wdeprecated-declarations` 警告，因 migration 不完整（剩余 ~200 行未迁移，包括 `include/axi4/{axi4_full,axi_to_axilite,axi4_lite_matrix,peripherals/axi_i2c,peripherals/axi_dma}.h`、`examples/spinalhdl-ported/{quadrature_encoder,i2c,ws2812,spi,uart_rx,vga,timer}`、`examples/riscv-mini/src/{rv32i_regfile,rv32i_alu,rv32i_tcm}`、`examples/axi4/phase3a_complete.cpp`、`samples/pod_bundle_conversion.cpp`），attributes 已全部撤回。Follow-up change `deprecate-hdl-logical-operators-redo` 重新发起，**必须先完成全量 migration，再加 attributes**，避免 self-warning 状态。
- **2026-10-16 deprecate-hdl-logical-operators-redo 落地** → 颠倒顺序完成：先 migration（35 个文件、~233 行 `&&`/`||` → `&`/`|` rewrite，涵盖 `stream_bundle.h` template 边界 / `chlib/*` 全集 / `axi4/*` / `cpu/pipeline/*` / `spinalhdl-ported` 7 例 / `riscv-mini/src` / `phase3a_complete.cpp` / `bundle_top_example` / `pod_bundle_conversion`），然后加 `[[deprecated("...")]]` 到 8 个 `ch_bool` / `port<ch_bool>` overload（2 `port<ch_bool>&&`/`||` + 4 mixed `ch_uint<N>` 显式 nonzero-test 警告 + 2 `ch_bool` only）。5 个 test 文件：3 个 sed-rewrite（`test_operator` / `test_comparison_ops` / `test_reg`），2 个 pragma 包裹保留 deprecated API 调用（`test_chipforge_mux_segv_repro` 因 SEGV repro 必须保留 `ch_bool(false) && (a != 0)` 模式；`test_operator_advanced` 的 mixed `ch_bool/ch_uint` 是 nonzero-test 重载行为覆盖）。变更归档至 `openspec/changes/archive/2026-10-15-2026-10-16-deprecate-hdl-logical-operators-redo/`。

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
