## Phase 1.1: Disabled Source Files Audit

**Task ID**: 1.1  
**Status**: Ready for execution  

### Files to Analyze

| File | Lines | Status in CMakeLists.txt |
|------|-------|-------------------------|
| `rv32i_minimal.cpp` | 8,388 | Commented out (lines 4-8) |
| `phase3b_minimal.cpp` | 3,681 | Commented out (lines 10-14) |

### Audit Checklist

- [ ] Attempt standalone compilation of each file
- [ ] List all API breakage errors
- [ ] Identify unique functionality vs phase tests
- [ ] Decide: **FIX** (if < 20 changes needed) or **DELETE** (if extensive rewrites)

### Key Questions

1. Does `rv32i_minimal.cpp` test anything not covered by `rv32i_phase1/2/3_test.cpp`?
2. Is `phase3b_minimal.cpp` a subset of `phase3b_complete.cpp`?
3. Are the disabled files used by any external documentation or tutorials?

### Expected Outcome

**Most likely**: Both files deleted
- `rv32i_minimal.cpp`: Too complex, phase tests are cleaner
- `phase3b_minimal.cpp`: Redundant with `phase3b_complete.cpp`

---

## Phase 1.2: Unregistered Source File Audit

**Task ID**: 1.2  
**Status**: Ready for execution  

### File to Analyze

| File | Lines | Registered in CMakeLists? |
|------|-------|--------------------------|
| `phase3b_complete.cpp` | 11,308 | ❌ No |

### Audit Checklist

- [ ] Attempt standalone compilation
- [ ] Identify what distinguishes it from `phase3b_minimal.cpp`
- [ ] Check if it has a `main()` function (example vs library)
- [ ] Decide: **REGISTER** (if useful standalone example) or **DELETE** (if it's a library header)

### Expected Outcome

**Most likely**: Register as example OR convert to test

---

## Phase 1.3: Duplicate Test Files Audit

**Task ID**: 1.3  
**Status**: Ready for execution  

### Duplicate Matrix

| riscv-mini/tests/ | tests/chlib/ | Action |
|-------------------|--------------|--------|
| `test_forwarding.cpp` (44 lines, all `REQUIRE(true)`) | `test_forwarding.cpp` (537 lines, real tests) | DELETE stub, keep chlib |
| `test_hazard_complete.cpp` (87 lines, mostly stubs) | `test_hazard_complete.cpp` (235 lines, real tests) | DELETE stub, keep chlib |

### Audit Checklist

- [ ] Compare test coverage between stub and real versions
- [ ] Check if riscv-mini stubs use different namespaces/APIs
- [ ] Verify chlib tests are registered with CTest
- [ ] Confirm no riscv-mini-specific test logic

### Expected Outcome

**Nearly certain**: Delete both stub files, migrate any unique test registrations

---

## Phase 1.4: Pipeline Stages Audit

**Task ID**: 1.4  
**Status**: Ready for execution  

### Files to Analyze

| File | Lines | Included by |
|------|-------|-------------|
| `src/stages/if_stage.h` | ~150 | `rv32i_pipeline.h`, `rv32i_pipeline_cache.h` |
| `src/stages/id_stage.h` | ~200 | `rv32i_pipeline.h`, `rv32i_pipeline_cache.h` |
| `src/stages/ex_stage.h` | ~200 | `rv32i_pipeline.h`, `rv32i_pipeline_cache.h` |
| `src/stages/mem_stage.h` | ~180 | `rv32i_pipeline.h`, `rv32i_pipeline_cache.h` |
| `src/stages/wb_stage.h` | ~100 | `rv32i_pipeline.h`, `rv32i_pipeline_cache.h` |

### Audit Checklist

- [ ] Check if `rv32i_pipeline.h` actually instantiates these stages
- [ ] Check if any test uses the pipeline
- [ ] Estimate effort to wire stages together
- [ ] Decide: **WIRE** (if pipeline is战略目标) or **DELETE** (if orphaned)

### Expected Outcome

**Likely**: DELETE all stage files + `rv32i_pipeline.h` + `rv32i_pipeline_cache.h`
- Rationale: No test uses the full pipeline, `test_rv32i_pipeline.cpp` is broken
- Alternative: Keep only if there's a clear plan to complete the pipeline

---

## Phase 1.5: Branch Predictor Duplication Audit

**Task ID**: 1.5  
**Status**: Ready for execution  

### Files to Compare

| File | Lines | Status |
|------|-------|--------|
| `src/branch_predictor_v2.h` | 136 | ✅ Fixed (ch_reg→ch_mem), tested |
| `src/dynamic_branch_predict.h` | ~50 | ❌ Simplified skeleton, untested |

### `dynamic_branch_predict.h` Content Preview

```cpp
// Fixed prediction: always Not Taken
io().predict_taken = ch_bool(false);
io().predict_not_taken = ch_bool(true);

// BHT framework retained but not functional
ch_reg<ch_uint<2>> bht_dummy(ch_uint<2>(2_d));
```

### Audit Checklist

- [ ] Confirm `branch_predictor_v2.h` has all features of `dynamic_branch_predict.h`
- [ ] Check include references to `dynamic_branch_predict.h`
- [ ] Verify `test_branch_predictor_v2.cpp` tests the right file

### Expected Outcome

**Certain**: DELETE `dynamic_branch_predict.h`

---

## Phase 1.6: Pipeline Test Scaffold Audit

**Task ID**: 1.6  
**Status**: Ready for execution  

### File to Analyze

| File | Size | Status |
|------|------|--------|
| `tests/test_rv32i_pipeline.cpp` | 20,416 bytes | ❌ Broken scaffold |

### Audit Checklist

- [ ] Attempt compilation
- [ ] Identify all missing dependencies
- [ ] Check what pipeline components it requires
- [ ] Estimate fix effort vs delete effort

### Expected Outcome

**Very likely**: DELETE test + dependent pipeline code
- Rationale: If stages are deleted (1.4), this test cannot work

---

## Discovery Phase Summary

### Tasks Parallelizable

All 6 discovery tasks can run **in parallel**:
- 1.1: Disabled source files
- 1.2: Unregistered source file
- 1.3: Duplicate tests
- 1.4: Pipeline stages
- 1.5: Branch predictors
- 1.6: Pipeline test scaffold

### Decision Matrix (Post-Discovery)

| File Group | Likely Decision | Confidence |
|------------|-----------------|------------|
| Disabled sources | DELETE | 70% |
| Duplicate stub tests | DELETE | 95% |
| Pipeline stages | DELETE | 70% |
| Dynamic branch predictor | DELETE | 95% |
| Pipeline test scaffold | DELETE | 80% |
| phase3b_complete.cpp | REGISTER | 60% |

### Next Actions After Discovery

1. Execute Phase 2 (Cleanup) based on decisions
2. Run Phase 3 (Testing) to consolidate remaining tests
3. Complete Phase 4 (Integration) for CTest registration

---

**Estimated Discovery Effort**: 6-7 hours total (can parallelize to ~4 hours wall time)

**Output**: Updated findings in this document with concrete decisions for each file group
