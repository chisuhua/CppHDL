# RV32I Mini Directory Cleanup Plan

**Created**: 2026-04-12  
**Goal**: Clean up `examples/riscv-mini/` directory, consolidate duplicate tests, fix disabled code, and ensure all tests are properly registered with CTest

---

## Executive Summary

**Current State**:
- ✅ 4 CTest tests passing (rv32i_phase1/2/3 + branch_predictor_v2)
- ❌ 2 source files disabled in CMakeLists (rv32i_minimal.cpp, phase3b_minimal.cpp)
- ❌ 1 source file unregistered (phase3b_complete.cpp)
- ❌ 7 stub test files in `tests/` with `REQUIRE(true)` placeholders
- ❌ Duplicate tests: `tests/chlib/test_forwarding.cpp` (537 lines) vs `tests/test_forwarding.cpp` (44 lines stub)
- ❌ Duplicate tests: `tests/chlib/test_hazard_complete.cpp` (235 lines) vs `tests/test_hazard_complete.cpp` (87 lines stub)
- ❌ `src/stages/*.h` (5 files) included but never wired together
- ❌ `dynamic_branch_predict.h` duplicates `branch_predictor_v2.h` (untested)
- ❌ `rv32i_core.h` is incomplete skeleton (110 lines)
- ❌ `test_rv32i_pipeline.cpp` references broken pipeline scaffold

**Target State**:
- All source files either fixed and enabled, or removed
- No duplicate tests - single source of truth
- All tests properly registered with CTest
- Pipeline stages either wired or removed
- Clean directory structure with clear purpose

---

## Phase 1: Discovery & Audit

### Task 1.1: Audit Disabled Source Files

**Description**: Analyze `rv32i_minimal.cpp` and `phase3b_minimal.cpp` to determine if they can be fixed or should be deleted.

**Dependencies**: None

**Estimated Effort**: 2 hours

**Success Criteria**:
- [ ] Each file marked as "fixable" or "delete"
- [ ] If fixable, list of required changes documented
- [ ] If deleted, migration path for any unique features

**Files Affected**:
- `rv32i_minimal.cpp` (8388 lines)
- `phase3b_minimal.cpp` (3681 lines)
- `CMakeLists.txt` (lines 4-14 commented)

---

### Task 1.2: Audit Unregistered Source File

**Description**: Analyze `phase3b_complete.cpp` to understand why it's not registered and determine if it should replace the minimal versions.

**Dependencies**: None

**Estimated Effort**: 1 hour

**Success Criteria**:
- [ ] Compilation status verified
- [ ] Relationship to phase3b_minimal.cpp documented
- [ ] Decision made: register as-is, fix, or delete

**Files Affected**:
- `phase3b_complete.cpp` (11308 lines)
- `CMakeLists.txt`

---

### Task 1.3: Audit Duplicate Test Files

**Description**: Compare stub tests in `examples/riscv-mini/tests/` against real tests in `tests/chlib/` to determine consolidation strategy.

**Dependencies**: None

**Estimated Effort**: 2 hours

**Success Criteria**:
- [ ] `test_forwarding.cpp` (44 lines stub) vs `tests/chlib/test_forwarding.cpp` (537 lines) → consolidation decision
- [ ] `test_hazard_complete.cpp` (87 lines stub) vs `tests/chlib/test_hazard_complete.cpp` (235 lines) → consolidation decision
- [ ] All other stub tests (`test_branch_predictor_v2.cpp`, `test_pipeline_integration.cpp`, `test_cache_pipeline_integration.cpp`, `test_phase4_integration.cpp`) reviewed

**Files Affected**:
- `examples/riscv-mini/tests/test_forwarding.cpp`
- `examples/riscv-mini/tests/test_hazard_complete.cpp`
- `examples/riscv-mini/tests/test_branch_predictor_v2.cpp`
- `examples/riscv-mini/tests/test_pipeline_integration.cpp`
- `examples/riscv-mini/tests/test_cache_pipeline_integration.cpp`
- `examples/riscv-mini/tests/test_phase4_integration.cpp`
- `tests/chlib/test_forwarding.cpp`
- `tests/chlib/test_hazard_complete.cpp`

---

### Task 1.4: Audit Pipeline Stages

**Description**: Verify if `src/stages/*.h` files are actually used or are orphaned code.

**Dependencies**: None

**Estimated Effort**: 1 hour

**Success Criteria**:
- [ ] Grep search confirms inclusion only in `rv32i_pipeline.h` and `rv32i_pipeline_cache.h`
- [ ] Decision made: wire stages together, or remove as unused

**Files Affected**:
- `src/stages/if_stage.h`
- `src/stages/id_stage.h`
- `src/stages/ex_stage.h`
- `src/stages/mem_stage.h`
- `src/stages/wb_stage.h`
- `src/rv32i_pipeline.h`
- `src/rv32i_pipeline_cache.h`

---

### Task 1.5: Audit Branch Predictor Duplication

**Description**: Compare `branch_predictor_v2.h` (fixed, tested) vs `dynamic_branch_predict.h` (simplified, untested).

**Dependencies**: None

**Estimated Effort**: 0.5 hours

**Success Criteria**:
- [ ] `dynamic_branch_predict.h` marked for deletion (redundant)
- [ ] Any unique features in `dynamic_branch_predict.h` migrated to `branch_predictor_v2.h` if needed

**Files Affected**:
- `src/branch_predictor_v2.h` (136 lines, fixed)
- `src/dynamic_branch_predict.h` (simplified skeleton)

---

### Task 1.6: Audit Pipeline Test Scaffold

**Description**: Analyze `test_rv32i_pipeline.cpp` to understand what's broken and whether it can be fixed.

**Dependencies**: None

**Estimated Effort**: 1.5 hours

**Success Criteria**:
- [ ] Compilation errors identified
- [ ] Decision made: fix pipeline scaffold, or remove test + associated code

**Files Affected**:
- `tests/test_rv32i_pipeline.cpp` (20416 bytes)
- `src/rv32i_pipeline.h`
- `src/rv32i_core.h`

---

## Phase 2: Cleanup

### Task 2.1: Remove Duplicate Stub Tests

**Description**: Delete stub test files that are consolidated into `tests/chlib/`.

**Dependencies**: Task 1.3 completed

**Estimated Effort**: 0.5 hours

**Success Criteria**:
- [ ] `examples/riscv-mini/tests/test_forwarding.cpp` deleted (or kept with real tests)
- [ ] `examples/riscv-mini/tests/test_hazard_complete.cpp` deleted (or kept with real tests)
- [ ] `examples/riscv-mini/tests/CMakeLists.txt` updated

**Files Affected**:
- `examples/riscv-mini/tests/test_forwarding.cpp` (DELETE candidate)
- `examples/riscv-mini/tests/test_hazard_complete.cpp` (DELETE candidate)
- `examples/riscv-mini/tests/CMakeLists.txt`

---

### Task 2.2: Remove Redundant Branch Predictor

**Description**: Delete `dynamic_branch_predict.h` as `branch_predictor_v2.h` is the authoritative version.

**Dependencies**: Task 1.5 completed

**Estimated Effort**: 0.25 hours

**Success Criteria**:
- [ ] `dynamic_branch_predict.h` deleted
- [ ] No include references remain

**Files Affected**:
- `src/dynamic_branch_predict.h` (DELETE)

---

### Task 2.3: Fix or Remove Disabled Source Files

**Description**: Either fix `rv32i_minimal.cpp` and `phase3b_minimal.cpp` to compile, or remove them and update CMakeLists.

**Dependencies**: Task 1.1, 1.2 completed

**Estimated Effort**: 3 hours (if fixing) / 0.5 hours (if deleting)

**Success Criteria**:
- [ ] If fixing: Both files compile and pass basic smoke test
- [ ] If deleting: CMakeLists.txt cleaned up, no orphaned includes
- [ ] `phase3b_complete.cpp` registered if it's the canonical version

**Files Affected**:
- `rv32i_minimal.cpp`
- `phase3b_minimal.cpp`
- `phase3b_complete.cpp`
- `CMakeLists.txt`

---

### Task 2.4: Wire or Remove Pipeline Stages

**Description**: Either complete the pipeline wiring in `rv32i_pipeline.h` or remove the unused stage files.

**Dependencies**: Task 1.4 completed

**Estimated Effort**: 4 hours (if wiring) / 0.5 hours (if removing)

**Success Criteria**:
- [ ] Decision documented in findings
- [ ] If wiring: All 5 stages connected, pipeline compiles
- [ ] If removing: All stage files removed, pipeline headers simplified

**Files Affected**:
- `src/stages/*.h` (5 files)
- `src/rv32i_pipeline.h`
- `src/rv32i_pipeline_cache.h`

---

### Task 2.5: Fix Pipeline Test or Remove

**Description**: Based on audit, either fix `test_rv32i_pipeline.cpp` scaffold or remove it.

**Dependencies**: Task 1.6, 2.4 completed

**Estimated Effort**: 2 hours (if fixing) / 0.25 hours (if removing)

**Success Criteria**:
- [ ] Test compiles and runs, OR
- [ ] Test removed, no orphaned references

**Files Affected**:
- `tests/test_rv32i_pipeline.cpp`

---

## Phase 3: Testing

### Task 3.1: Consolidate Forwarding Tests

**Description**: Ensure `tests/chlib/test_forwarding.cpp` is the authoritative test. If any riscv-mini-specific tests exist, migrate them.

**Dependencies**: Task 2.1 completed

**Estimated Effort**: 1 hour

**Success Criteria**:
- [ ] Single authoritative forwarding test in `tests/chlib/`
- [ ] All FORWARDING test cases passing
- [ ] No duplicate test registration

**Files Affected**:
- `tests/chlib/test_forwarding.cpp`
- `examples/riscv-mini/tests/test_forwarding.cpp` (if not deleted)

---

### Task 3.2: Consolidate Hazard Tests

**Description**: Ensure `tests/chlib/test_hazard_complete.cpp` is the authoritative test. Migrate any unique riscv-mini tests.

**Dependencies**: Task 2.1 completed

**Estimated Effort**: 1 hour

**Success Criteria**:
- [ ] Single authoritative hazard test in `tests/chlib/`
- [ ] All HAZARD test cases passing
- [ ] No duplicate test registration

**Files Affected**:
- `tests/chlib/test_hazard_complete.cpp`
- `examples/riscv-mini/tests/test_hazard_complete.cpp` (if not deleted)

---

### Task 3.3: Write Phase 3B Tests

**Description**: If `phase3b_complete.cpp` is enabled, write proper Catch2 tests for it.

**Dependencies**: Task 2.3 completed

**Estimated Effort**: 2 hours

**Success Criteria**:
- [ ] Phase 3B integration tests written
- [ ] Tests cover Load/Store + Forwarding + Hazards
- [ ] All tests passing

**Files Affected**:
- `examples/riscv-mini/rv32i_phase3_test.cpp` (may need updates)

---

### Task 3.4: Write Pipeline Integration Tests

**Description**: If pipeline is wired (Task 2.4), write integration tests. If not, skip this task.

**Dependencies**: Task 2.4 completed (wiring path only)

**Estimated Effort**: 3 hours

**Success Criteria**:
- [ ] Pipeline integration tests written
- [ ] Tests cover stall/flush behavior
- [ ] Tests cover forwarding paths

**Files Affected**:
- `tests/test_pipeline_integration.cpp` (rewrite)

---

## Phase 4: Integration

### Task 4.1: Update Root CMakeLists.txt

**Description**: Enable `examples/riscv-mini` subdirectory in root CMakeLists.txt if all tests pass.

**Dependencies**: All Phase 2 & 3 tasks completed

**Estimated Effort**: 0.25 hours

**Success Criteria**:
- [ ] `add_subdirectory(examples/riscv-mini)` uncommented
- [ ] Root project builds successfully

**Files Affected**:
- `CMakeLists.txt` (root)

---

### Task 4.2: Register All Tests with CTest

**Description**: Ensure all passing tests are registered via `add_test()` in appropriate CMakeLists.txt files.

**Dependencies**: Task 4.1 completed

**Estimated Effort**: 0.5 hours

**Success Criteria**:
- [ ] `rv32i_phase1_test` registered
- [ ] `rv32i_phase2_test` registered
- [ ] `rv32i_phase3_test` registered
- [ ] `test_branch_predictor_v2` registered
- [ ] `test_hazard_complete` registered (if kept in riscv-mini)
- [ ] `test_forwarding` registered (if kept in riscv-mini)
- [ ] `ctest -R rv32i` shows all tests

**Files Affected**:
- `examples/riscv-mini/CMakeLists.txt`
- `examples/riscv-mini/tests/CMakeLists.txt`
- `tests/CMakeLists.txt` (for chlib tests)

---

### Task 4.3: Run Full Test Suite

**Description**: Execute `ctest -R rv32i --output-on-failure` to verify all tests pass.

**Dependencies**: Task 4.2 completed

**Estimated Effort**: 0.5 hours

**Success Criteria**:
- [ ] All RV32I tests pass
- [ ] No compilation warnings
- [ ] Test output captured

**Files Affected**: None (verification only)

---

### Task 4.4: Document Cleanup Decisions

**Description**: Write a summary document explaining what was removed/fixed and why.

**Dependencies**: All tasks completed

**Estimated Effort**: 0.5 hours

**Success Criteria**:
- [ ] `plans/cleanup_summary.md` created
- [ ] All decisions documented
- [ ] Future maintainers understand rationale

**Files Affected**:
- `examples/riscv-mini/plans/cleanup_summary.md` (NEW)

---

## Task Dependency Graph

```
Phase 1 (Discovery)
├── 1.1 → 2.3
├── 1.2 → 2.3
├── 1.3 → 2.1 → 3.1, 3.2
├── 1.4 → 2.4 → 3.4
├── 1.5 → 2.2
└── 1.6 → 2.5

Phase 2 (Cleanup)
├── 2.1 → 3.1, 3.2
├── 2.2 (independent)
├── 2.3 → 3.3
├── 2.4 → 3.4
└── 2.5 (depends on 1.6)

Phase 3 (Testing)
├── 3.1 (consolidate forwarding)
├── 3.2 (consolidate hazard)
├── 3.3 (phase 3b tests)
└── 3.4 (pipeline tests, optional)

Phase 4 (Integration)
├── 4.1 (enable subdir)
├── 4.2 (register CTest)
├── 4.3 (run tests)
└── 4.4 (document)
```

---

## Parallel Execution Opportunities

**Can run in parallel (independent)**:
- Tasks 1.1, 1.2, 1.3, 1.4, 1.5, 1.6 (all discovery tasks)
- Tasks 2.2, 2.3 (if not blocked by 1.x)
- Tasks 3.1, 3.2, 3.3 (consolidation tasks)

**Must run sequentially**:
- 1.4 → 2.4 → 3.4 (pipeline chain)
- 2.1 → 3.1, 3.2 (test consolidation)
- All Phase 4 tasks depend on Phase 2 & 3 completion

---

## Risk Assessment

| Risk | Likelihood | Impact | Mitigation |
|------|------------|--------|------------|
| Pipeline wiring too complex | High | High | Default to removing unused stages |
| `rv32i_minimal.cpp` has too many API breaks | Medium | Medium | Delete and keep only phase tests |
| Test consolidation breaks existing tests | Low | High | Run tests after each consolidation step |
| CTest registration conflicts | Low | Medium | Careful review of all CMakeLists |

---

## Recommended Execution Order

1. **Phase 1 (Discovery)**: Run all 6 tasks in parallel → ~4 hours total wall time
2. **Decision Point**: Review findings, make go/no-go on pipeline wiring
3. **Phase 2 (Cleanup)**: Execute based on decisions → ~4-8 hours
4. **Phase 3 (Testing)**: Consolidate and write new tests → ~5 hours
5. **Phase 4 (Integration)**: Register and verify → ~1.5 hours

**Total Estimated Effort**: 14.5-18.5 hours

---

## Files That Will Likely Be Deleted

| File | Reason | Confidence |
|------|--------|------------|
| `src/dynamic_branch_predict.h` | Duplicates `branch_predictor_v2.h` | 95% |
| `examples/riscv-mini/tests/test_forwarding.cpp` | Stub, duplicate of `tests/chlib/` | 90% |
| `examples/riscv-mini/tests/test_hazard_complete.cpp` | Stub, duplicate of `tests/chlib/` | 90% |
| `src/stages/*.h` (5 files) | Never wired together, unused | 70% |
| `tests/test_rv32i_pipeline.cpp` | Broken scaffold | 70% |
| `rv32i_minimal.cpp` | API breaks, unclear value | 60% |
| `phase3b_minimal.cpp` | Replaced by `phase3b_complete.cpp` | 60% |

---

## Files That Will Likely Be Fixed

| File | Required Changes | Confidence |
|------|------------------|------------|
| `CMakeLists.txt` | Uncomment/register fixed targets | 95% |
| `phase3b_complete.cpp` | API updates if needed | 80% |
| `examples/riscv-mini/tests/CMakeLists.txt` | Remove deleted tests, add CTest | 95% |
| Root `CMakeLists.txt` | Uncomment riscv-mini subdir | 90% |

