# Phase F: RISC-V Tests Through Rv32iSoc Pipeline

**Goal**: 41 riscv-tests passing through actual `Rv32iSoc` hardware pipeline  
**Status**: DRAFT - Ready for execution  
**Date**: 2026-04-15  
**Approach**: TDD-first, incremental, zero-debt

---

## CRITICAL REVIEW: Current Approach Assessment

### Review Summary: ⚠️ APPROACH PARTIALLY SOUND

The existing approach (fix rv32i_soc.h → create test → run) is **correct in direction but fatally incomplete**. Below is the detailed analysis.

### Question 1: Is fixing `rv32i_soc.h` the right first step?

**Answer: YES, but not as written.**

The file `include/cpu/riscv/rv32i_soc.h` is fundamentally broken and must be fixed before integration testing can work. However, the approach should be:

1. **First** unblock compilation fixes that are NOT in rv32i_soc.h itself (they affect existing tests)
2. **Then** rewrite rv32i_soc.h to use existing, working patterns
3. **Then** create tests that drive the SoC through simulation
4. **Finally** plug in riscv-tests binaries

Creating a standalone test FIRST (without fixing SoC) would just test the mock `SimpleRv32iSoc` from `test_soc_hello_world.cpp`, which is NOT the real hardware.

### Question 2: Why did the "shadow memory" approach fail?

`test_riscv_tests.cpp` contains a `Rv32iSimpleMem` class that is completely decoupled from the CppHDL `InstrTCM` and `DataTCM` hardware components. It's a **software interpreter with its own memory array**:

```
┌─────────────────────────────────────────────┐
│ Rv32iInterpreter (software interpreter)      │
│                                              │
│   ┌─────────────┐      ┌──────────────────┐  │
│   │  Rv32i      │      │   memory[WORD_   │  │
│   │  SimpleMem  │◄────►│   COUNT]         │  │
│   │  (shadow)   │      │  (raw uint32_t)  │  │
│   └─────────────┘      └──────────────────┘  │
│                                              │
│   Step: decode + execute one instruction     │
│                                              │
└──────────────────────────────────────────────┘
```

The problem:
- **Hardware writes happen in TCM** (CppHDL's `InstrTCM`/`DataTCM` which use `sdata_type` and internal arrays)
- **Shadow reads its own array** (`uint32_t memory[WORD_COUNT]`)
- **Zero connection between them** - they are parallel universes

When the hardware pipeline stores to 0x80001000 (tohost), the TCM receives the write, but the shadow array remains unchanged. The test then reads from shadow, sees old data, and concludes "tohost not written."

**The fix**: Use `sim.get_port_value(tcm.io().data)` at the correct address after simulation, and check for the tohost value directly in hardware. No shadow array needed.

### Question 3: Are there hidden risks in the proposed approaches?

**YES - 3 critical hidden risks identified:**

1. **Pipeline execution is simulation-based, not interpretation-based**: The `Simulator` in CppHDL evaluates combinational logic and updates registers via tick(). Running a complex riscv-test through actual simulation will be **extremely slow** (each tick processes a fraction of the CPU state). If a test needs 10000+ instructions, that may need 100,000+ ticks at ~10 ticks per instruction.

2. **The `ch_literal_runtime` width trait bug**: In `operators.h:756`, `operator>>` and `operator<<` try to use `RHS::actual_value` as a compile-time constant, but `ch_literal_runtime::actual_value` is a runtime `uint64_t` field. This breaks compilation when ANY code uses runtime width shifts.

3. **No existing integration test infrastructure**: There's no way to "run the SoC for N ticks and check TCM memory." The Simulator API (`tick()`, `eval()`, `set_input_value`, `get_port_value`) must be composed manually with no existing helper class.

---

## BLOCKING ISSUES DISCOVERED (verified by compilation test)

### B1: `ch_var<T>` does NOT exist anywhere in the codebase
**Impact**: Lines 117-172 of rv32i_soc.h - EVERY signal declaration fails  
**Evidence**: `grep -rn "ch_var" include/` returns ONLY rv32i_soc.h (40+ errors)  
**Root Cause**: File was written assuming a `ch_var` template that was never implemented

### B2: `chlib.h` does NOT include `clint.h`
**Impact**: Line 107 of rv32i_soc.h - `ch::chlib::Clint` is undefined  
**Evidence**: `grep -i "clint" include/chlib.h include/ch.hpp` returns nothing

### B3: `if_stage.h:66` uses invalid hex literal `0x80000000_d`
**Impact**: `_d` literal suffix only accepts decimal digits (not hex)  
**Evidence**: `literal_ext.h:92` static assertion: "invalid character in literal"

### B4: `operators.h:756` - runtime `actual_value` accessed as static
**Impact**: `ch_literal_runtime::actual_value` is an instance member, not `static constexpr`  
**Evidence**: `literal.h:41` declares `uint64_t actual_value;` (non-static)

### B5: `build_bit_select` template argument mismatch
**Impact**: `bit_select(operand, index)` passes wrong template args to `build_bit_select<T,Index>`  
**Evidence**: The 2-arg overload expects `<T>` (single template arg), not `<T, Index>`

### B6: 5 files in `include/` depend on `rv32i_soc.h` headers that don't exist at those paths
**Impact**: Missing includes: `rv32i_pipeline.h`, `hazard_unit.h`, `rv32i_tcm.h` (all in `examples/riscv-mini/src/`)

---

## IMPLEMENTATION PLAN (TDD-based, 7 tasks with atomic commits)

### Task 1: Fix Pre-existing Compilation Errors
**Tests FIRST**: Write test that exposes the issues  
**Then fix**: The 4 bugs (B3-B6) that exist independently of rv32i_soc.h

#### QA Scenario
- Tool: `g++ -std=c++20` compile test
- Steps: 
  1. Verify `cmake --build build` passes clean (baseline - already passes)
  2. Compile a file that includes `if_stage.h` (exposes B3)
  3. Compile a file that uses `operator>>` with runtime value (exposes B4)
  4. Compile a file that uses `bits<5,0>(operand)` (exposes B5)
- Expected: All compile errors appear → fix each → all pass

#### Fix Details

**B3 Fix** (`if_stage.h:66`): Replace `0x80000000_d` with `ch_uint<32>(0x80000000)`
**B4 Fix** (`operators.h:751-760`): The `>>` and `<<` operators with `RHS::actual_value` for `CHLiteral` should use an alternative approach. For `ch_literal_runtime`, compute via runtime path:
```cpp
constexpr unsigned rhs_extra_width = [] {
    if constexpr (CHLiteral<RHS> && requires { typename RHS::actual_value; }) {
        return RHS::actual_value; // compile-time literal - static constexpr
    } else if constexpr (CHLiteral<RHS>) {
        return (static_cast<uint64_t>(1) << ch_width_v<RHS>) - 1;
    } else {
        return (static_cast<uint64_t>(1) << ch_width_v<RHS>) - 1;
    }
}();
```
**B5 Fix** (`operators.h:494`): Remove explicit template args, let compiler deduce:
```cpp
auto *op_node = node_builder::instance().build_bit_select(
    operand_node, index_node, "bit_select", ...);
```
**B6 Fix**: Move or symlink headers, OR add include paths to CMakeLists

**Commit 1**: `fix(compiler): fix 4 pre-existing bugs blocking SoC compilation`

---

### Task 2: Write SoC Integration Test (TDD - Test First)
**Before writing any SoC code**, create `tests/test_soc_integration.cpp` that:
- Loads a minimal binary program into TCM via simulator
- Simulates tick-by-tick execution  
- Reads tohost from hardware TCM at 0x80001000
- Expects tohost=1 (pass)

#### QA Scenario
- Tool: Catch2 via `ctest`
- Steps:
  1. Create `test_soc_integration.cpp` with one test that loads a known-good binary
  2. The test WILL NOT compile yet (rv32i_soc.h broken) - that's expected
  3. Test will compile after Task 3
  4. Run: expects FAIL (SoC doesn't execute yet, just validates the test harness)
  5. Then fix SoC until the test PASSES

```cpp
// Minimal test structure (pseudo-code):
TEST_CASE("SoC integration: rv32i-p-test passes", "[soc][riscv-tests]") {
    context ctx("soc_riscv_test"); ctx_swap swap(&ctx);
    
    // Load binary
    auto binary = load_test_binary("rv32ui-p-add");
    
    // Create real SoC (not mock)
    ch::ch_device<Rv32iSoc> soc;
    Simulator sim(soc.context());
    
    // Load program into I-TCM
    for (uint32_t i = 0; i < binary.size(); i++) {
        sim.set_input_value(soc.instance()...write_addr, i);
        sim.set_input_value(soc.instance()...write_data, binary[i]);
        sim.set_input_value(soc.instance()...write_en, 1);
        sim.tick();
    }
    
    // Run simulation
    for (int tick = 0; tick < 10000; tick++) {
        sim.tick();
        uint32_t tohost = sim.get_port_value(
            get_tcm_value_at(0x001000)); // offset for tohost
        if (tohost != 0) {
            REQUIRE(tohost == 1);
            return;
        }
    }
    FAIL("tohost not set within 10000 ticks");
}
```

**Commit 2**: `test(soc): add SoC integration test harness (TDD-first)`

---

### Task 3: Rewrite `rv32i_soc.h` Using Existing Patterns
**Replace ALL `ch_var<T>` with correct patterns:**

From existing samples (e.g., `conditional_demo.cpp`), the pattern is:

| Current (broken) | Correct pattern |
|---|---|
| `ch_var<ch_uint<32>> x{"x", 0_d}` | `auto x = ch_uint<32>(0_d)` (wire) |
| `ch_var<ch_bool> flag{"flag", true}` | `auto flag = ch_bool(true)` (wire) |
| Stateful register | `ch_reg<ch_uint<32>> reg(val)` (register) |

**Also fix:**
- Add `#include "clint.h"` (not via `chlib.h`)
- Add missing includes with correct paths
- Use `auto` for ALL internal wire signals (NOT `ch_var`)
- Rename the local variable `clint` to avoid shadowing the type

**The rewrite will be substantial (~150 lines changed) but MECHANICAL** - same pattern repeated:
```cpp
// OLD (broken):
ch_var<ch_uint<32>> instr_addr{"instr_addr", 0_d};
ch_var<ch_bool>     instr_ready{"instr_ready", true};

// NEW (working):
auto instr_addr = ch_uint<32>(0_d);
auto instr_ready = ch_bool(true);
```

**Commit 3**: `refactor(soc): rewrite rv32i_soc.h using existing Ch patterns (fix ch_var phantom type)`

---

### Task 4: Register Test in CMake + Verify Compile
- Add `test_soc_integration.cpp` to `examples/riscv-mini/CMakeLists.txt`
- Wire `clint.h` into `chlib.h` aggregator
- Ensure `build` target includes `examples/riscv-mini/src` as include path for the test
- Verify `test_soc_integration` target compiles (not passes yet)

#### QA Scenario
- Tool: `cmake --build build --target test_soc_integration`
- Expected: Clean compilation (0 errors, 0 warnings)

**Commit 4**: `build(cmake): register SoC integration test, wire clint.h into aggregator`

---

### Task 5: Fix TCM Memory Read for Tohost Detection
The key challenge: reading a specific address from TCM memory during simulation.

The TCM (`InstrTCM`, `DataTCM`) has an `addr` port and a `data` output port. To read the tohost value at address 0x001000 (offset from 0x8000):

1. Set TCM `addr` input to the target offset (0x001000 / 4 = 1024 for word addressing)
2. Tick the simulator
3. Read `data` port value

This requires direct TCM port access. The challenge is that `Rv32iSoc` internally connects these ports. We need to either:
- Expose internal TCM ports (via additional SoC IO), OR
- Use a simpler approach: poll the UART output or a simpler pass mechanism

**Alternative approach for first pass**: Instead of polling tohost, modify the firmware wrapper to write to UART on completion. Then poll UART output.

#### QA Scenario
- Tool: Catch2 + Simulator API  
- Steps:
  1. Create a trivial test binary that writes 42 to 0x80001000
  2. Load it into I-TCM
  3. Simulate N ticks
  4. Read DataTCM at tohost offset
  5. Verify: value == 42
- Expected: Pass

**Commit 5**: `test(soc): fix tohost read through actual TCM hardware (remove shadow memory)`

---

### Task 6: Run First riscv-test Through Real SoC
**Pick the simplest test first**: `rv32ui-p-addi`  
Binary available at `/workspace/project/riscv-tests/build/rv32ui/`

1. Load binary into I-TCM
2. Run simulation
3. Monitor tohost via TCM reads
4. Verify pass/fail

#### QA Scenario
- Tool: Catch2
- Steps:
  1. Load `rv32ui-p-addi.bin` into SoC I-TCM
  2. Run simulation up to 100,000 ticks
  3. Read tohost from DataTCM every 100 ticks
  4. Expect tohost == 1 (pass)
  5. If tohost == 0 → FAIL (debug instruction by instruction)

**Commit 6**: `test(riscv-tests): run rv32ui-p-addi through Rv32iSoc pipeline`

---

### Task 7: Parameterize for All 41 Tests
Convert the single-test pattern into a parameterized Catch2 test:

```cpp
TEST_CASE("riscv-tests: [param]", "[soc][riscv-tests]") {
    // param = list of all .bin files in rv32ui/ rv32um/ etc.
    // For each: load → simulate → verify tohost
}
```

#### QA Scenario  
- Tool: Catch2 parameterized test
- Steps:
  1. Enumerate all .bin files in riscv-tests build directory
  2. For each binary: load, simulate (max 50000 ticks), verify tohost=1
  3. Report: X/41 pass, Y failed
  4. If < 41 pass: investigate failures, fix bugs
- Expected: 41/41 pass

**Commit 7**: `test(riscv-tests): parameterize all 41 tests, verify 41/41 pass through Rv32iSoc`

---

## ATOMIC COMMIT STRATEGY

```
C1: fix(compiler): fix 4 pre-existing bugs blocking SoC compilation
    • if_stage.h:66 - hex literal _d → runtime constructor  
    • operators.h:756 - actual_value static/instance fix
    • operators.h:494 - build_bit_select deduce template args
    • rv32i_decoder.h - include path fixes

C2: test(soc): add SoC integration test harness (TDD-first)  
    • tests/test_soc_integration.cpp - single test, will not compile yet
    • Add CMakeLists.txt entry

C3: refactor(soc): rewrite rv32i_soc.h using existing Ch patterns
    • ch_var<T> → auto (all ~40 declarations)
    • Add missing includes (clint.h, etc.)
    • Rename clint variable to avoid shadowing

C4: build(cmake): register SoC integration test, wire clint.h into aggregator
    • chlib.h: add #include "cpu/riscv/clint.h"
    • CMake test registration

C5: test(soc): fix tohost read through actual TCM hardware  
    • Remove shadow memory approach
    • Direct TCM port reading via Simulator API

C6: test(riscv-tests): run rv32ui-p-addi through Rv32iSoc pipeline
    • First actual riscv-test through hardware
    • Debug + verify single test

C7: test(riscv-tests): parameterize all 41 tests, verify 41/41 pass
    • Catch2 parameterized test over all .bin files
    • Final integration verification
```

**Each commit is independently buildable** (compiles and existing tests pass).

---

## RISK MATRIX

| Risk | Probability | Impact | Mitigation |
|------|------------|--------|------------|
| Pipeline simulation too slow | MEDIUM | HIGH (blocks Task 6) | Start with 100-tick budget, increase if needed. Use rv32ui-p-addi first (shortest test). |
| Missing ALU/instruction support | MEDIUM | HIGH | rv32ui-p-addi tests only addi. If fails, debug instruction-by-instruction. |
| Simulator API incomplete | LOW | HIGH | Verify Simulator.get_port_value works for TCM data. Test in Tasks 2, 5. |
| Tohost address mismatch | LOW | MEDIUM | Verify 0x80001000 maps correctly through address_decoder. Test in Task 5. |
| TCM word addressing | LOW | MEDIUM | Verify TCM addr uses word (not byte) addressing. Test in Task 5. |

---

## DEPENDENCY GRAPH

```
C1 (compiler fixes) ────────────────────────────┐
                                                 ▼
C2 (test harness) ───→ C3 (SoC rewrite) ───→ C4 (CMake wiring) ──→ C5 (tohost read)
                                                                        │
                                                                        ▼
                                                              C6 (first test) ──→ C7 (all 41)
```

**C1 is the critical path** - it unblocks everything else and can be merged independently.
**C3 depends on C1** because the SoC file can't compile without B3-B5 fixes in the includes.

---

## VERIFICATION GATES (per AGENTS.md phase completion)

| Gate | Criteria | Command |
|------|----------|---------|
| 🟢 Build | `cmake --build` 0 errors, 0 warnings | `cmake --build build -j$(nproc) 2>&1 \| grep -E "error\|Error" \| grep -v "warning" = empty` |
| 🟢 Tests | All existing tests pass + new SoC test | `ctest --output-on-failure` (existing) + `test_soc_integration` (new) |
| 🟢 Docs | AGENTS.md updated for any API changes | Check `include/AGENTS.md` for new include paths |

---

## NOTES FROM REVIEW

1. **The `SimpleRv32iSoc` in `test_soc_hello_world.cpp` is a MOCK** with a software interpreter. It tests the concept but NOT the real hardware. The real integration test must use `ch::ch_device<Rv32iSoc>` with the `Simulator`.

2. **The `_d` literal suffix is DECIMAL-ONLY**. For runtime addresses like 0x80000000, use `ch_uint<32>(0x80000000)`. The literal system only supports `42_d`, `0_d` etc.

3. **`ch_literal_runtime` vs `ch_literal_impl<...>`**: The `_d` suffix produces `ch_literal_impl` (compile-time). `ch_literal_runtime` is created dynamically and cannot be used where compile-time width is needed.

4. **The pipeline exists and has 5-stage tests that pass**. Phase 1-3 tests (RegFile/ALU, Branch/Jump, Load/Store) all pass. The issue has always been the TOP-LEVEL integration, not the individual components.
