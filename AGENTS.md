# CppHDL

> C++17 hardware description library for high-level HDL development. SpinalHDL patterns compiled to C++.

## Stack
C++20, CMake, Catch2 v3.7.0, clangd (LSP), SpinalHDL-style API

## Structure
```
CppHDL/
├── include/          # Header-only HDL library
│   ├── core/         # Core primitives: context, operators, IO, lnode
│   ├── ast/          # AST nodes + instruction classes
│   ├── chlib/        # HDL components: stream, fifo, pipeline, arbiter
│   ├── bundle/       # Bundle type definitions (AXI, Stream, Flow)
│   ├── lnode/        # Logic node implementations
│   ├── bv/           # Bit-vector math (gmp/mpfr wrapper)
│   ├── axi4/         # AXI4-Lite bundle + interconnect
│   └── cpu/          # Cache, pipeline, RV32I
├── src/              # Library implementations
│   ├── ast/          # Instruction eval() implementations
│   ├── core/         # Context + lnodeimpl
│   └── *.cpp         # CodeGen (Verilog/DAG), Simulator, Component
├── samples/          # 41 HDL pattern demos (bundles, streams, protocols)
├── examples/         # Full design examples
│   ├── spinalhdl-ported/  # 17 SpinalHDL port examples (each with main())
│   ├── axi4/              # AXI4 bus demos
│   ├── stream/            # Stream Mux/Demux/Arbiter/Fork
│   └── riscv-mini/        # RV32I 5-stage pipeline (partial)
├── tests/            # 79 Catch2 test files
│   └── chlib/              # Component library tests
└── docs/             # Architecture plans, usage guides, phase reports
```

## WHERE TO LOOK
| Task | Location |
|------|----------|
| Core types (ch_bool, ch_uint) | `include/core/operators.h`, `include/core/types.h` |
| IO ports (ch_in, ch_out) | `include/core/io.h` |
| Context + simulation | `include/core/context.h`, `src/simulator.cpp` |
| Stream/Flow operators | `include/chlib/stream.h`, `include/chlib/stream_operators.h` |
| FIFO | `include/chlib/fifo.h` |
| Bundle types | `include/bundle/` (stream_bundle, axi_bundle, flow_bundle) |
| State machine DSL | `include/chlib/state_machine.h` |
| Verilog codegen | `include/codegen_verilog.h`, `src/codegen_verilog.cpp` |
| AXI4 interconnect | `include/axi4/` |

## CONVENTIONS
- **IO assignment**: Use `select(condition, val1, val0)` NOT `&&` on IO ports
- **Module instantiation**: Use `ch::ch_module<T>` NOT `CH_MODULE` macro for templates
- **Testbench input**: Use `sim.set_input_value(port, value)` NOT `device.io().port = value`
- **Test context**: Always `ch::core::context ctx("name")` + `ch::core::ctx_swap swap(&ctx)`
- **Bundle as IO**: Use individual `ch_bool`/`ch_uint` ports, NOT bundle struct
- **Include aggregator**: Use `#include "ch.hpp"` (library) or `#include "chlib.h"` (chlib components)

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

See `docs/COMPONENT-LIFECYCLE-GUIDE.md` for full details.

## ANTI-PATTERNS
- Direct IO port assignment: `io().port = value` (compiles but silently does nothing)
- `&&` on IO ports: use `select()` instead
- `CH_MODULE` with templates: use `ch::ch_module<T>`
- `ch_module` in standalone tests: use `ch_device` instead
- Empty TEST_CASE: use SKIP() or delete
- Missing ctx_swap: register ops segfault without it

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
- 119 CTest entries (base + chlib labels)
- 28 main() examples tracked by `run_all_ported_tests.sh` (28/28 pass)
- riscv-mini: Pipeline compile-time fixes complete, runtime requires ch_device wrapper
- I2C controller is simplified (no ACK handling)
