# Phase F: riscv-tests Through Pipeline — Unblocking Plan

**Created:** 2026-04-15  
**Status:** Blocked by 3 compilation issues  
**Goal:** Run 41 RV32UI riscv-tests through Rv32iSoc pipeline with tohost@0x80001000 verification

---

## User Request Summary

Phase F is blocked by:
1. `rv32i_soc.h` pre-existing compilation errors (ch_var<ch_bool> template failures, clint variable shadowing, build_bit_select errors in operators.h)
2. `Rv32iPipeline` compilation issues when used with other headers
3. `test_riscv_tests_soc.cpp` redesign needed (shadow memory approach doesn't work)

**Immediate question:** Should we fix compilation errors first or redesign the test approach?

**Answer:** Fix compilation errors FIRST — test redesign is blocked by them anyway.

---

## Task Dependency Graph

| Task | Depends On | Reason |
|------|------------|--------|
| 1. Fix operators.h ch_var template issues | None | Core library issue, blocks all RV32I hardware |
| 2. Fix clint.h variable shadowing | None | Independent header fix |
| 3. Fix rv32i_soc.h module instantiation | Task 1 | Uses ch_var which depends on operators.h |
| 4. Fix Rv32iPipeline header include guards | Task 1 | Depends on operators.h patterns |
| 5. Create minimal riscv-tests runner | Tasks 3, 4 | Needs SoC and Pipeline to compile |
| 6. Implement tohost verification | Task 5 | Depends on runner infrastructure |
| 7. Run 41 riscv-tests | Task 6 | Final verification step |

---

## Parallel Execution Graph

**Wave 1 (Start immediately - no dependencies):**
├── Task 1: Fix operators.h ch_var template issues
└── Task 2: Fix clint.h variable shadowing

**Wave 2 (After Wave 1 completes):**
├── Task 3: Fix rv32i_soc.h module instantiation
└── Task 4: Fix Rv32iPipeline header patterns

**Wave 3 (After Wave 2 completes):**
└── Task 5: Create minimal riscv-tests runner

**Wave 4 (After Wave 3 completes):**
├── Task 6: Implement tohost verification
└── Task 7: Run 41 riscv-tests

**Critical Path:** Task 1 → Task 3 → Task 5 → Task 6 → Task 7  
**Estimated Parallel Speedup:** 30% faster than sequential

---

## Tasks

### Task 1: Fix operators.h ch_var template issues

**Description:** Fix build_bit_select template failures when used with ch_var<ch_bool> in rv32i_soc.h

**Delegation Recommendation:**
- Category: `cpp-pro` - Template metaprogramming and C++17 specialization
- Skills: [`cpp-debug`] - Debugging template instantiation failures

**Skills Evaluation:**
- ✅ INCLUDED `cpp-debug`: Essential for diagnosing template instantiation chain
- ❌ OMITTED `cpp-modernize`: Not a modernization issue, it's a template SFINAE issue
- ❌ OMITTED `cmake-manage`: Build system is unrelated to this issue

**Depends On:** None  
**Acceptance Criteria:**
1. rv32i_soc.h compiles without ch_var template errors
2. All existing tests still pass (ctest -L base)
3. No new compiler warnings introduced

**Implementation Steps:**
1. Build test_address_decoder to see exact error messages
2. Search for ch_var template specializations in operators.h
3. Identify missing specialization for ch_var<ch_bool>
4. Add template specialization or convert to ch_uint<1>
5. Verify compilation success

---

### Task 2: Fix clint.h variable shadowing

**Description:** Fix variable shadowing warnings/errors in clint.h (likely local variable vs parameter name collision)

**Delegation Recommendation:**
- Category: `quick` - Simple variable renaming
- Skills: [] - No specialized skills needed

**Skills Evaluation:**
- ✅ INCLUDED none: Straightforward fix, no skill required
- ❌ OMITTED `cpp-debug`: Shadowing is a simple lexical issue

**Depends On:** None  
**Acceptance Criteria:**
1. clint.h compiles with `-Wshadow -Werror` without warnings
2. All existing CLINT functionality preserved
3. No behavior changes

**Implementation Steps:**
1. Compile with `-Wshadow` to identify exact shadowing locations
2. Rename shadowed variables (prefix with underscore or local_ prefix)
3. Verify no semantic changes

---

### Task 3: Fix rv32i_soc.h module instantiation

**Description:** Fix Rv32iSoc describe() method — module instantiation and port connection patterns

**Delegation Recommendation:**
- Category: `cpp-pro` - Complex C++ template instantiation with HDL patterns
- Skills: [`cpp-architecture`] - Understanding module hierarchy in CppHDL

**Skills Evaluation:**
- ✅ INCLUDED `cpp-architecture`: Critical for understanding Component→Module→Device pattern
- ❌ OMITTED `cpp-debug`: Not debugging, needs architectural fix
- ✅ INCLUDED (implicit) `cpp-pro`: Pattern matching SpinalHDL semantics

**Depends On:** Task 1  
**Acceptance Criteria:**
1. rv32i_soc.h compiles cleanly
2. No more ch_var template errors in describe()
3. Module instantiation follows SpinalHDL patterns

**Implementation Steps:**
1. Review clint.h connection pattern (likely uses invalid ch_var assignment)
2. Replace ch_var assignments with proper ch_uint<1> or ch_bool
3. Verify AXI4-Lite UART connection pattern matches working examples
4. Test SOC-level compilation

---

### Task 4: Fix Rv32iPipeline header patterns

**Description:** Fix Rv32iPipeline include guards, forward declarations, and header dependencies

**Delegation Recommendation:**
- Category: `cpp-pro` - Header dependency management
- Skills: [`cpp-architecture`] - Module boundary analysis

**Skills Evaluation:**
- ✅ INCLUDED `cpp-architecture`: Understanding include dependency graph
- ❌ OMITTED `cpp-debug`: Not a runtime bug

**Depends On:** Task 1  
**Acceptance Criteria:**
1. Rv32iPipeline.h can be included with rv32i_soc.h without ODR violations
2. No circular includes
3. All forward declarations explicit

**Implementation Steps:**
1. Check include guards (#pragma once present?)
2. Identify circular includes (rv32i_pipeline.h <-> hazard_unit.h)
3. Use forward declarations where possible
4. Test both headers together

---

### Task 5: Create minimal riscv-tests runner

**Description:** Build minimal test harness to load riscv-tests ELF and run through Rv32iSoc

**Delegation Recommendation:**
- Category: `cpp-pro` - Test infrastructure with ELF loading
- Skills: [`planning-with-files`] - Complex multi-step test setup

**Skills Evaluation:**
- ✅ INCLUDED `planning-with-files`: Multi-phase test runner setup
- ❌ OMITTED `web-search`: Already have riscv-tests integration patterns

**Depends On:** Tasks 3, 4  
**Acceptance Criteria:**
1. Test can load RV32I assembly into I-TCM at 0x80000000
2. Test can tick the SoC simulation
3. tohost variable accessible at 0x80001000 in D-TCM

**Implementation Steps:**
1. Load riscv-tests binary (riscv32-unknown-elf-objcopy to raw binary)
2. Map binary into I-TCM memory
3. Create test loop: tick() until tohost != 0 or timeout
4. Verify fromhost/tohost semantics

---

### Task 6: Implement tohost verification

**Description:** Add tohost/fromhost MMIO verification at 0x80001000

**Delegation Recommendation:**
- Category: `cpp-pro` - Test verification logic
- Skills: [] - Straightforward assertion logic

**Skills Evaluation:**
- ✅ INCLUDED none: Simple comparison logic
- ❌ OMITTED all: No specialized patterns needed

**Depends On:** Task 5  
**Acceptance Criteria:**
1. tohost@0x80001000 monitored during simulation
2. Test passes when tohost != 0
3. Test fails on timeout (>1M cycles)

**Implementation Steps:**
1. Add memory-mapped tohost variable in D-TCM
2. Create assertion: REQUIRE(tohost_value != 0)
3. Add cycle counter and timeout logic
4. Log pass/fail with test name

---

### Task 7: Run 41 riscv-tests and verify

**Description:** Execute full RV32UI test suite and verify results

**Delegation Recommendation:**
- Category: `unspecified-high` - Test execution with analysis
- Skills: [`review-work`] - Automated test result verification

**Skills Evaluation:**
- ✅ INCLUDED `review-work`: Batch test execution and result analysis
- ❌ OMITTED `cpp-debug`: Running tests, not debugging yet

**Depends On:** Task 6  
**Acceptance Criteria:**
1. All 41 RV32UI tests executed
2. Results logged (PASS/FAIL with test name)
3. Failure analysis for failed tests

**Implementation Steps:**
1. Create CMake target: test_riscv_tests_soc
2. Run each test (rv32ui-p-*, rv32ui-v-*)
3. Aggregate results
4. Identify failure patterns

---

## Commit Strategy

**Atomic commits required:** Each fix in separate commit for git bisect

```
1. fix(operators.h): Add ch_var<ch_bool> template specialization
   - Minimal change, no behavior modification
   
2. fix(clint.h): Rename shadowed variables
   - Simple variable renaming, no logic change
   
3. fix(rv32i_soc.h): Correct module instantiation patterns
   - Follow existing patterns from axi4 peripherals
   
4. fix(rv32i_pipeline.h): Resolve header dependencies
   - Forward declarations, include guards
   
5. test(riscv-tests): Add minimal riscv-tests runner
   - Infrastructure only, no test logic
   
6. test(tohost): Implement tohost verification
   - Verification logic
   
7. ci(riscv-tests): Add 41-test CTest integration
   - CMakeLists.txt and CI integration
```

**Branch protection:** Each commit must:
1. Compile cleanly (`cmake --build build -j$(nproc)`)
2. Pass existing tests (`ctest -L base --output-on-failure`)
3. No new warnings

---

## Success Criteria

**Phase F complete when:**

1. ✅ **编译通过**: rv32i_soc.h + Rv32iPipeline compile with 0 errors, 0 warnings
2. ✅ **测试覆盖**: test_riscv_tests_soc compiles and runs
3. ✅ **文档同步**: findings.md updated with compilation patterns learned

**Verification commands:**
```bash
# Full build verification
cmake --build build -j$(nproc) 2>&1 | grep -E "error|warning"

# Test execution
./build/examples/riscv-mini/test_riscv_tests_soc

# Existing tests unchanged
ctest -L base --output-on-failure
```

---

## Risks & Mitigation

| Risk | Probability | Mitigation |
|------|-------------|------------|
| ch_var template chain longer than expected | Medium | Escalate after 3 strikes (per cpp-debug skill) |
| Rv32iPipeline needs complete rewrite | Low | Use riscv-mini/src/rv32i_pipeline.h as reference (already passes) |
| riscv-tests ABI mismatch | Medium | Verify with riscv32-unknown-elf-objdump -h |
| tohost semantics not understood | Low | Refer to riscv-tests README and pk.c source |

---

## Notes

**Key insight:** The test redesign (Task 5+) is BLOCKED by compilation errors (Tasks 1-4). Therefore:
1. Fix compilation errors FIRST (highest priority)
2. Then redesign test approach
3. Do NOT attempt both in parallel — compilation issues will block test anyway

**Why ch_var<ch_bool> fails:** Template specialization likely missing for ch_bool as width=1 special case. Solution: either add specialization or convert all ch_var<ch_bool> to ch_uint<1>.

**Why clint shadowing matters:** `-Werror=shadow` likely enabled, which treats warnings as errors. Simple fix, but must be done before testing.

**Test redesign:** Shadow memory approach doesn't work because CppHDL simulator doesn't support memory-mapped I/O via direct pointer access. Must use `sim.set_input_value()` / `sim.get_port_value()` API.
