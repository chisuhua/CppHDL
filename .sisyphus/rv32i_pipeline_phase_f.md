# RV32I Hardware Pipeline Fix Plan — Phase F

## Goal
Fix the RV32I 5-stage hardware pipeline to run 41 riscv-tests through actual hardware (not the software interpreter). Target: All RV32UI tests passing via `Rv32iSoc` hardware pipeline.

**Decision**: Option B — fix hardware pipeline + `rv32i_soc.h` properly (not interpreter shortcut)

**Date**: 2026-04-15  
**Phase**: Phase F  
**Status**: Planning Complete — Ready for Execution

---

## Executive Summary

**Four-wave approach**:
1. **Wave 1**: Fix 6 compiler bugs (PRE-REQUISITE — nothing compiles without these)
2. **Wave 2**: Fix pipeline wiring bugs (IF/EX/MEM/WB stage connections)
3. **Wave 3**: Write TDD integration test (minimal viable test proves pipeline works)
4. **Wave 4**: Scale to all 41 RV32UI tests

**Key constraints**:
- Zero-debt policy: Each wave exits with 0 errors, 0 warnings, all tests passing
- TDD discipline: Write tests BEFORE fixes (red → green → refactor)
- Atomic commits: Each wave = separate commit(s), verified independently

---

## Dependency Graph

```
Wave 1 (Compiler Bugs)
├── A1: ch_var<T> phantom type → Delete from rv32i_soc.h
├── A2: clint.h not in chlib.h → Add include
├── A3: 0x80000000_d invalid → Use 0x80000000ULL
├── A4: operators.h:756 static access → Fix compile-time check
├── A5: operators.h:494 template mismatch → Fix signature
└── A6: Missing includes → Add #include "ch.hpp"
           ↓
Wave 2 (Pipeline Wiring)
├── W2.1: IF stage PC initialization
├── W2.2: EX stage PC connection (from ID not RS1)
├── W2.3: WB stage register write-back (MISSING → Add)
├── W2.4: MEM/WB connections (alu_result from correct source)
└── W2.5: Hazard unit (connect EX/MEM/WB inputs)
           ↓
Wave 3 (TDD Integration Test)
├── W3.1: Write test_riscv_tests_soc.cpp (MVT: simple.bin)
├── W3.2: Build hardware testbench (tick loop + tohost monitor)
└── W3.3: Run through pipeline until tohost==1
           ↓
Wave 4 (Full Suite)
└── W4: Run all 41 RV32UI tests through hardware
```

---

## Wave 1: Compiler Bug Fixes (CRITICAL PATH)

**Status**: BLOCKING — All other work depends on Wave 1 completion

**Estimated effort**: 2-3 hours  
**Risk**: HIGH (operators.h template changes may cascade)

### Category A: Pre-existing Compiler Bugs

| ID | Issue | Location | Fix | Owner | Parallel? |
|----|-------|----------|-----|-------|-----------|
| **A1** | `ch_var<T>` phantom type in rv32i_soc.h | `include/cpu/riscv/rv32i_soc.h:118-169` | Replace `ch_var<T>` with `ch_uint<32>` or `ch_bool` | Engineer 1 | ✅ Yes |
| **A2** | `clint.h` not included by chlib.h | `include/chlib.h` | Add `#include "chlib/clint.h"` at line ~30 | Engineer 2 | ✅ Yes |
| **A3** | `0x80000000_d` invalid hex literal | `examples/riscv-mini/src/stages/if_stage.h:66` | Replace with `0x80000000ULL` | Engineer 2 | ✅ Yes |
| **A4** | `operators.h:756` RHS::actual_value static access | `include/core/operators.h:755-760` | Fix: Use template specialization or if constexpr | Engineer 3 (CPP-PRO) | ❌ No |
| **A5** | `operators.h:494` build_bit_select template mismatch | `include/core/operators.h:494` | Match template signature to node_builder method | Engineer 3 (CPP-PRO) | ❌ No |
| **A6** | Missing includes (if_stage.h, rv32i_tcm.h, hazard_unit.h) | `examples/riscv-mini/src/stages/*.h` | Add `#include "ch.hpp"` at top | Engineer 1 | ✅ Yes |

### Verification Commands

```bash
# After Wave 1 completion:
cd /workspace/project/CppHDL
cmake --build build -j$(nproc) 2>&1 | tee /tmp/build.log
grep -E "error:|warning:" /tmp/build.log | wc -l  # Must return: 0
```

### Atomic Commit Strategy

**Single commit** (all compiler fixes together — verified as atomic unit):
```
fix(compiler): resolve Phase F blockers for RV32I pipeline

- A1: Replace ch_var<T> with ch_uint<32>/ch_bool in rv32i_soc.h
- A2: Add clint.h include to chlib.h aggregator
- A3: Fix 0x80000000_d → 0x80000000ULL in if_stage.h
- A4: Fix operators.h:756 RHS::actual_value static access
- A5: Fix operators.h:494 build_bit_select template signature
- A6: Add ch.hpp includes to stage headers

Verified:
- cmake --build build: 0 errors, 0 warnings
- ctest -L base --output-on-failure: all existing tests pass
```

---

## Wave 2: Pipeline Wiring Fixes (TDD Approach)

**Status**: Blocked on Wave 1 completion  
**Estimated effort**: 6-8 hours  
**Risk**: MEDIUM (wiring bugs are localized, but require careful tracing)

### TDD Discipline

**For each sub-wave**:
1. Write FAILING test first (defines success criteria)
2. Run test → RED (expected failure)
3. Fix hardware connection
4. Run test → GREEN
5. Refactor (if needed)

### Wave 2.1: IF Stage

**Bug**: PC reset value uses invalid literal `0x80000000_d`

**Test to write first**:
```cpp
// File: tests/test_rv32i_pipeline.cpp
TEST_CASE("rv32i_pipeline: IF stage PC resets to 0x80000000", "[if-stage]") {
    context ctx("if_stage_test");
    ctx_swap swap(&ctx);
    ch::ch_device<IfStage> if_stage;
    Simulator sim(if_stage.context());
    
    sim.set_input_value(if_stage.instance().io().rst, 1);
    sim.eval();
    
    auto pc = sim.get_port_value(if_stage.instance().io().pc);
    REQUIRE(static_cast<uint32_t>(pc) == 0x80000000);
}
```

**Fix location**: `examples/riscv-mini/src/stages/if_stage.h:66`

### Wave 2.2: EX Stage

**Bug**: PC input connected to wrong source
```cpp
// WRONG (current):
ex_stage.io().pc <<= id_stage.io().rs1_data;

// CORRECT:
ex_stage.io().pc <<= id_stage.io().pc;
```

**Test to write first**:
```cpp
TEST_CASE("rv32i_pipeline: EX stage PC from ID not RS1", "[ex-stage]") {
    // Verify PC flows: IF → ID → EX
    // Set ID.pc = 0xDEADBEEF, verify EX.pc == 0xDEADBEEF after tick
}
```

**Fix location**: Pipeline connection code (likely `rv32i_pipeline.h` or core wrapper)

### Wave 2.3: WB Stage (CRITICAL GAP)

**Bug**: No register write-back from WB stage to register file

**Missing hardware**:
```cpp
// WB stage needs这些 ports:
ch_out<ch_uint<5>>  rd_addr;        // Register to write
ch_out<ch_uint<32>> write_data;     // Data to write (from MEM or ALU)
ch_out<ch_bool>     write_en;       // Write enable (when regfile should write)
```

**Test to write first**:
```cpp
TEST_CASE("rv32i_pipeline: WB stage writes to register file", "[wb-stage]") {
    // Setup: WB.rd_addr = 5, WB.write_data = 0x12345678, WB.write_en = 1
    // Tick
    // Verify: reg_file[5] == 0x12345678
}
```

**Fix location**: 
- Add `wb_stage.h` write ports (if missing)
- Connect `wb_stage.io().rd_addr` → `reg_file.write_addr`
- Connect `wb_stage.io().write_data` → `reg_file.write_data`
- Connect `wb_stage.io().write_en` → `reg_file.write_en`

### Wave 2.4: MEM/WB Pipeline Register Connections

**Bug**: alu_result, rd_addr from wrong sources

**Current (WRONG)**:
```cpp
mem_stage.io().alu_result <<= /* from wrong source */;
wb_stage.io().rd_addr <<= /* from wrong source */;
```

**Correct**:
```cpp
// MEM stage gets alu_result from EX stage
mem_stage.io().alu_result <<= ex_stage.io().alu_result;

// WB stage gets rd_addr from MEM stage
wb_stage.io().rd_addr <<= mem_stage.io().rd_addr;

// WB stage gets write_data from MUX:
// - If instruction is LOAD: use memory read data
// - Otherwise: use ALU result
wb_stage.io().write_data <<= select(
    mem_stage.io().is_load,
    mem_stage.io().mem_read_data,
    mem_stage.io().alu_result
);
```

### Wave 2.5: Hazard Unit

**Bug**: All connections are stubs from ID stage only

**Required connections**:
```cpp
// Hazard unit inputs from ALL pipeline stages:
hazard.io().id_rs1_addr      <<= id_stage.io().rs1_addr;
hazard.io().id_rs2_addr      <<= id_stage.io().rs2_addr;
hazard.io().ex_rd_addr       <<= ex_stage.io().rd_addr;
hazard.io().ex_reg_write     <<= ex_stage.io().reg_write;
hazard.io().mem_rd_addr      <<= mem_stage.io().rd_addr;
hazard.io().mem_reg_write    <<= mem_stage.io().reg_write;
hazard.io().mem_is_load      <<= mem_stage.io().is_load;
hazard.io().wb_rd_addr       <<= wb_stage.io().rd_addr;
hazard.io().wb_reg_write     <<= wb_stage.io().reg_write;
```

**Test to write first**:
```cpp
TEST_CASE("rv32i_pipeline: LOAD-USE hazard detection", "[hazard-unit]") {
    // Scenario: 
    // - EX/MEM: LW x3, 0(x1)  (loads to x3)
    // - ID/EX:  ADD x4, x3, x5  (reads x3)
    // Expected:
    // - hazard.stall_if = 1
    // - hazard.stall_id = 1
    // - hazard.flush_id_ex = 1
}
```

### Verification Commands

```bash
# After Wave 2 completion:
ctest -R "test_rv32i_pipeline|test_forwarding|test_hazard" --output-on-failure
# Expected: 5 tests, 10 assertions — ALL PASS
```

### Atomic Commit Strategy

**Multiple commits** (each stage fixed independently, verified before next):
```
commit W2.1: "test(if-stage): add PC reset unit test"
commit W2.2: "fix(if-stage): PC initializes to 0x80000000"
commit W2.3: "test(ex-stage): add PC connection unit test"
commit W2.4: "fix(ex-stage): PC input from ID not RS1"
commit W2.5: "feat(wb-stage): add register write-back ports"
commit W2.6: "test(mem-wb): verify pipeline connections"
commit W2.7: "fix(hazard-unit): connect EX/MEM/WB inputs"
```

---

## Wave 3: TDD Integration Test

**Status**: Blocked on Wave 1 + Wave 2 completion  
**Estimated effort**: 4-6 hours  
**Risk**: HIGH (requires full SoC integration, multiple failure modes)

### Minimal Viable Test (MVT)

**Test file**: `examples/riscv-mini/tests/test_riscv_tests_soc.cpp` (REWRITE)

**Test flow**:
```cpp
TEST_CASE("rv32i_soc: simple.bin through hardware pipeline", "[rv32i-soc][integration]") {
    /**
     * MVT Criteria:
     * 1. SoC instantiates without segfault
     * 2. simple.bin loads to I-TCM
     * 3. sim.tick() loop runs without infinite loop
     * 4. tohost written (value != 0)
     * 5. tohost == 1 (PASS)
     */
    
    context ctx("rv32i_soc_test");
    ctx_swap swap(&ctx);
    
    // 1. Instantiate SoC
    ch::ch_device<Rv32iSoc> soc;
    Simulator sim(soc.context());
    
    // 2. Load simple.bin to I-TCM (via direct memory access)
    //    - Read simple.bin file
    //    - Write to soc.instr_tcm memory array
    bool loaded = load_bin_to_itcm("simple.bin", soc);
    REQUIRE(loaded == true);
    
    // 3. Run tick loop (max 1000 cycles)
    uint32_t tohost = 0;
    constexpr uint32_t TOHOST_ADDR = 0x80001000;
    constexpr uint32_t MAX_TICKS = 1000;
    
    for (uint32_t tick = 0; tick < MAX_TICKS; tick++) {
        sim.tick();
        
        // Read tohost from D-TCM (or MMIO)
        // This requires: soc.data_tcm.read(TOHOST_ADDR) or equivalent
        tohost = read_tohost(soc, TOHOST_ADDR);
        
        if (tohost != 0) {
            break;  // Test completed
        }
    }
    
    // 4. Verify result
    std::cout << "simple.bin: tohost = 0x" << std::hex << tohost << std::dec << std::endl;
    REQUIRE(tohost == 1);  // riscv-tests convention: 1=PASS, 0=FAIL
}
```

**Why simple.bin**:
- ~20 instructions (smallest test)
- Exercises: ADDI, LUI, SW, LW, branches
- Clear pass/fail via tohost

### Hardware Testbench Requirements

1. **Binary loader**: Function to load `.bin` file to I-TCM
   ```cpp
   bool load_bin_to_itcm(const std::string& bin_path, Rv32iSoc& soc);
   ```

2. **Tohost reader**: Function to read tohost from D-TCM
   ```cpp
   uint32_t read_tohost(Rv32iSoc& soc, uint32_t addr);
   ```

3. **Tick loop**: Integration with `ch::simulator`

### Success Criteria

| Criterion | Verification |
|-----------|--------------|
| Test compiles | `cmake --build build` succeeds |
| No segfault | `./build/test_riscv_tests_soc` runs without crash |
| tohost written | Loop exits with `tohost != 0` |
| Test passes | `tohost == 1` |

### Atomic Commit Strategy

```
commit W3.1: "test(rv32i-soc): add minimal viable integration test"
  - test_riscv_tests_soc.cpp rewritten (MVT: simple.bin)
  - Test compiles, runs, currently FAILS (expected)
  - Defines success: tohost == 1

commit W3.2: "feat(rv32i-soc): add binary loader for I-TCM"
  - load_bin_to_itcm() helper function

commit W3.3: "fix(rv32i-soc): wire tohost output to testbench"
  - Enable tohost monitoring from test
  - After this commit: MVT PASSES
```

---

## Wave 4: Full RV32UI Test Suite

**Status**: Blocked on Wave 3 MVT passing  
**Estimated effort**: 8-12 hours (debugging failing tests)  
**Risk**: MEDIUM (failure modes known, systematic debugging required)

### Test Suite Structure

```cpp
// Parameterized test for all 41 RV32UI tests
const std::vector<std::pair<const char*, const char*>> RV32UI_TESTS = {
    {"add", "add.bin"},
    {"addi", "addi.bin"},
    {"and", "and.bin"},
    // ... 41 tests total (see test_riscv_tests.cpp for full list)
};

TEST_CASE("rv32i_soc: RV32UI test suite", "[rv32i-soc][riscv-tests]") {
    for (const auto& [name, bin] : RV32UI_TESTS) {
        SECTION(name) {
            bool passed = run_riscv_test_soc(bin);
            INFO("Test " << name << " (" << bin << ")");
            REQUIRE(passed == true);
        }
    }
}
```

### Expected Failure Modes

| Test | Likely Issue | Fix Location |
|------|--------------|--------------|
| `beq.bin` | Branch target calculation wrong | `if_stage.branch_target` mux |
| `bne.bin` | Branch condition inverted | `ex_stage.branch_taken` |
| `jal.bin` | Jump link (rd write) missing | `id_stage.rd_write_en` |
| `jalr.bin` | JALR register update broken | `wb_stage.write_en` |
| `lw.bin` | LOAD-USE stall not working | `hazard_unit.stall` logic |
| `sw.bin` | Store byte enable wrong | `mem_stage.strb_logic` |
| `sll.bin` | Shift amount wrong | `ex_stage.shift_amt` |
| `sra.bin` | Arithmetic vs logical shift | `ex_stage.sra_sign_fill` |

### Debugging Workflow (Per Failing Test)

```
1. Run test → FAIL (e.g., tohost == 0 or timeout)
2. Check which instruction hangs:
   - Add PC logging to tick loop
   - Note where PC stops advancing
3. Trace signal at that instruction:
   - Add waveform dump (if available)
   - Or add debug prints at each stage
4. Compare expected vs actual behavior:
   - Use riscv-tests source to know expected result
   - Compare with actual hardware output
5. Fix + re-run → PASS
```

### Verification Commands

```bash
# After Wave 4 completion:
ctest -R "rv32i_soc" --output-on-failure
# Expected: 41 tests pass (or close to it)
```

### Atomic Commit Strategy

**One commit per test fix** (or per logical group):
```
commit W4.1: "fix(pipeline): add.bin passes through hardware"
commit W4.2: "fix(pipeline): addi.bin passes through hardware"
commit W4.3: "fix(pipeline): beq.bin passes (branch target fix)"
commit W4.4: "fix(pipeline): jal.bin passes (rd write-back fix)"
...
```

---

## Parallel Execution Plan

### Wave 1.1 (Parallel) — Compiler Bugs A1, A2, A3, A6

**Can be done simultaneously by multiple engineers**:

| Engineer | Task | File | Estimated Time |
|----------|------|------|----------------|
| Engineer 1 | A1: Remove `ch_var<T>` | `rv32i_soc.h` | 30 min |
| Engineer 2 | A2: Add `clint.h` include | `chlib.h` | 5 min |
| Engineer 2 | A3: Fix hex literal | `if_stage.h` | 10 min |
| Engineer 1 | A6: Add missing includes | `if_stage.h`, `rv32i_tcm.h`, `hazard_unit.h` | 15 min |

**Coordination**: Independent changes — no merge conflicts expected.

### Wave 1.2 (Sequential) — Operators.h Bugs A4, A5

**MUST be single engineer** (template changes cascade):

| Engineer | Task | File | Estimated Time |
|----------|------|------|----------------|
| Engineer 3 | A4 + A5: Fix operators.h templates | `operators.h:494, 756` | 1-2 hours |

**Skill required**: `cpp-pro` (template metaprogramming expertise)

### Wave 2.1 (Parallel) — Unit Tests

**TDD parallelization** (each engineer writes tests for their stage):

| Engineer | Stage Test | File |
|----------|------------|------|
| Engineer 1 | IF stage test | `tests/test_rv32i_pipeline.cpp` |
| Engineer 2 | EX stage test | `tests/test_rv32i_pipeline.cpp` |
| Engineer 3 | MEM/WB stage test | `tests/test_rv32i_pipeline.cpp` |
| Engineer 4 | Hazard unit test | `tests/test_hazard.cpp` |

### Wave 2.2 (Parallel) — Stage Fixes

**After tests written, same engineers fix their stages**:

| Engineer | Stage Fix | Verification |
|----------|-----------|--------------|
| Engineer 1 | IF stage fix | test IF stage passes |
| Engineer 2 | EX stage fix | test EX stage passes |
| Engineer 3 | MEM/WB fix | test MEM/WB stage passes |
| Engineer 4 | Hazard unit fix | test hazard passes |

### Wave 3 (Sequential) — Integration Test

**Single owner recommended** (integration requires system-level understanding):

| Engineer | Task | File |
|----------|------|------|
| Engineer 3 | Write MVT + testbench | `test_riscv_tests_soc.cpp` |

**Others**: Review code, prepare Wave 4 debugging setup

### Wave 4 (Parallel) — Debug Failing Tests

**Divide tests by category**:

| Engineer | Test Category | Tests |
|----------|---------------|-------|
| Engineer 1 | ALU tests | add, addi, sub, and, or, xor, sll, srl, sra |
| Engineer 2 | Branch tests | beq, bne, blt, bge, bltu, bgeu, jal, jalr |
| Engineer 3 | Load/Store tests | lb, lh, lw, lbu, lhu, sb, sh, sw, st_ld |
| Engineer 4 | Misc tests | auipc, lui, fence_i, ma_data, simple |

---

## Skills Required

| Wave | Skills | Purpose |
|------|--------|---------|
| W1.1 | None (simple fixes) | Basic C++ syntax fixes |
| W1.2 | `cpp-pro` | Template metaprogramming in operators.h |
| W2.x | `test-driven-development`, `cpp-pro` | Write unit tests + fix pipeline |
| W3.x | `cpp-pro`, `cpp-debug` | Build integration testbench |
| W4.x | `systematic-debugging`, `cpp-debug` | Debug failing tests |

---

## Risk Mitigation

### High-Risk Items

| Risk | Likelihood | Impact | Mitigation |
|------|------------|--------|------------|
| operators.h fixes break other code | MEDIUM | HIGH | Run full test suite after W1: `ctest --output-on-failure` |
| WB stage requires major refactoring | MEDIUM | HIGH | Scope creep: keep minimal (only what MVT needs) |
| Integration test segfaults | HIGH | MEDIUM | Use ASan: compile with `-fsanitize=address` |
| 41 tests take too long | MEDIUM | MEDIUM | Parallelize (Wave 4.4 strategy) |
| riscv-tests binaries not found | LOW | HIGH | Pre-check: `ls /workspace/project/riscv-tests/build/rv32ui/*.bin` |

### Fallback Strategy

If Wave 3 MVT fails repeatedly:
1. **Debug with simpler test**: Write custom minimal program (5 instructions)
2. **Use interpreter as reference**: Compare hardware vs interpreter state at each tick
3. **Reduce scope**: Get 1 test passing first, then scale to 41

---

## Zero-Debt Policy Enforcement

Each wave **MUST** exit with zero technical debt:

| Debt Type | Check | Command |
|-----------|-------|---------|
| Compile errors | 0 errors | `cmake --build build 2>&1 \| grep error \| wc -l` |
| Compile warnings | 0 warnings | `cmake --build build 2>&1 \| grep warning \| wc -l` |
| Failing tests | All pass | `ctest --output-on-failure 2>&1 \| grep -E "FAIL|TIMEOUT" \| wc -l` |
| Dead code | No stubs | `find . -name "*.h" -o -name "*.cpp" \| xargs grep -l "TODO\|FIXME" \| wc -l` |
| Orphan files | All registered | `ls build/*.o \| wc -l` matches CMakeLists.txt |

---

## Definition of Done

**Phase F complete when**:

1. ✅ Wave 1: 0 compiler errors, 0 warnings
2. ✅ Wave 2: All unit tests pass (5 tests, 10 assertions)
3. ✅ Wave 3: MVT (simple.bin) passes via hardware
4. ✅ Wave 4: All 41 RV32UI tests pass via hardware

**Exit criteria**:
```bash
# All of these must pass:
cmake --build build -j$(nproc)  # 0 errors
ctest -L base --output-on-failure  # all base tests pass
ctest -R rv32i_soc --output-on-failure  # 41 RV32UI tests pass
```

---

## Appendix: File Locations Reference

| File | Purpose |
|------|---------|
| `include/cpu/riscv/rv32i_soc.h` | Top-level SoC integration (A1 fix) |
| `include/chlib.h` | chlib aggregator (A2 fix) |
| `examples/riscv-mini/src/stages/if_stage.h` | IF stage (A3, W2.1 fix) |
| `examples/riscv-mini/src/stages/ex_stage.h` | EX stage (W2.2 fix) |
| `examples/riscv-mini/src/stages/wb_stage.h` | WB stage (W2.3 fix) |
| `examples/riscv-mini/src/hazard_unit.h` | Hazard unit (W2.5 fix) |
| `include/core/operators.h` | Core operators (A4, A5 fix) |
| `examples/riscv-mini/tests/test_riscv_tests_soc.cpp` | Integration test (W3) |
| `tests/test_rv32i_pipeline.cpp` | Pipeline unit tests (W2) |

---

## Appendix: riscv-tests Memory Map

```
Memory Region    Base Address    Size    Purpose
--------------   ------------    ----    -------
I-TCM            0x80000000      64KB    Instructions
D-TCM            0x80010000      64KB    Data + tohost
CLINT            0x80100000      4KB     Timer (mtime, mtimecmp)
UART             0x80101000      4KB     Serial output
GPIO             0x80102000      4KB     General purpose IO

tohost address:  0x80001000  (in D-TCM region)
fromhost addr:   0x80001008  (not used in this phase)
```

---

**END OF PLAN**
