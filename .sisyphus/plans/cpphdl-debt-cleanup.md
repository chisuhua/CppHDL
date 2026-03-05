# CppHDL Technical Debt Cleanup Plan

**Created:** 2026-03-05
**Based on:** Architecture and Code Debt Analysis by Prometheus
**Priority:** Critical

## Overview

This plan addresses critical memory management issues, build system anti-patterns, and code quality improvements identified in the technical debt analysis.

## Phase 1: Critical Fixes (High Priority - Week 1)

**Goal:** Resolve memory safety issues and stabilize core functionality

### Task 1.1: Fix Memory Management Segfaults ✅ COMPLETED
- **Status:** DONE
- **Files Modified:** `src/component.cpp`, `src/core/context.cpp`
- **Changes:**
  - Fixed `children_shared_.clear()` segfault in Component destructor by breaking parent references first
  - Fixed `node_storage_.clear()` segfault in context destructor by releasing nodes in reverse order
- **Verification:** Build passes; 59/63 CTest tests pass (94%). 4 tests currently failing: `test_trace` (1 case), `test_bundle_connection` (1 case), `test_stream_arbiter` (SEGFAULT), `test_arithmetic_advance` (bit-index-out-of-range errors)
- **Completed:** 2026-03-05

### Task 1.2: Implement Missing Core Functions
- **Status:** TODO
- **Files:** `include/utils/converter.h`, `include/chlib/fifo.h`
- **Effort:** 3 days
- **Dependencies:** Task 1.1
- **Success Criteria:** 
  - All BCD/binary conversion stubs implemented
  - Async FIFO fully functional
  - Unit tests added for new functionality
- **Steps:**
  1. Audit all `TODO`/`FIXME` comments in core headers
  2. Implement BCD/binary conversion functions in converter.h
  3. Complete async FIFO implementation with proper synchronization
  4. Add unit tests for new functionality
  5. Verify with existing test suite

### Task 1.3: Memory Safety Audit
- **Status:** TODO
- **Files:** All core implementation files
- **Effort:** 2 days
- **Dependencies:** Task 1.1
- **Success Criteria:** 
  - No raw pointers in public API
  - RAII patterns everywhere
  - Valgrind/ASan clean
- **Steps:**
  1. Replace raw pointers with `std::unique_ptr`/`std::shared_ptr`
  2. Add move semantics where appropriate
  3. Implement proper copy/move constructors
  4. Add memory leak detection in tests
  5. Run valgrind to verify no leaks

## Phase 2: Build System & Infrastructure (Medium Priority - Week 2)

**Goal:** Modernize build system and establish development workflow

### Task 2.1: Fix CMake Anti-patterns
- **Status:** TODO
- **Files:** `CMakeLists.txt`, `tests/CMakeLists.txt`
- **Effort:** 2 days
- **Dependencies:** None
- **Success Criteria:** 
  - Explicit file lists instead of glob
  - Proper dependency versioning
  - Simplified interface library pattern
- **Steps:**
  1. Replace `file(GLOB_RECURSE)` with explicit file lists
  2. Add versioning for external dependencies (inipp, nameof)
  3. Simplify interface library pattern
  4. Add proper install targets
  5. Verify build still works

### Task 2.2: Establish CI/CD Pipeline
- **Status:** TODO
- **Files:** `.github/workflows/` (new directory)
- **Effort:** 1 day
- **Dependencies:** Task 2.1
- **Success Criteria:** 
  - Automated builds on Linux/macOS/Windows
  - Automated tests
  - Code quality checks
- **Steps:**
  1. Create `.github/workflows/ci.yml`
  2. Set up GitHub Actions for multiple platforms
  3. Add clang-format and cpplint checks
  4. Configure test coverage reporting
  5. Test the pipeline

### Task 2.3: Code Quality Automation
- **Status:** TODO
- **Files:** `.clang-format`, `.pre-commit-config.yaml` (new)
- **Effort:** 1 day
- **Dependencies:** None
- **Success Criteria:** 
  - Consistent code style
  - Automated checks on commit
- **Steps:**
  1. Create `.clang-format` with project style
  2. Set up pre-commit hooks
  3. Add cpplint configuration
  4. Create `CONTRIBUTING.md` with guidelines
  5. Test the hooks

## Phase 3: Code Quality & Documentation (Low Priority - Week 3)

**Goal:** Improve maintainability and developer experience

### Task 3.1: Standardize Code Conventions
- **Status:** TODO
- **Files:** All source and header files
- **Effort:** 3 days
- **Dependencies:** Task 2.3
- **Success Criteria:** 
  - All comments in English
  - Consistent naming (PascalCase classes, snake_case functions)
  - Consistent header guards
- **Steps:**
  1. Identify all non-English comments
  2. Translate and update comments
  3. Standardize naming with clang-tidy
  4. Apply consistent header guards
  5. Organize includes

### Task 3.2: Enhance Test Suite
- **Status:** TODO
- **Files:** `tests/` organization
- **Effort:** 2 days
- **Dependencies:** None
- **Success Criteria:** 
  - Organized by component
  - Missing edge cases covered
  - Performance tests added
- **Steps:**
  1. Reorganize tests by component (ast/, core/, simulator/)
  2. Add missing edge case tests
  3. Implement performance benchmarks
  4. Add test documentation
  5. Verify coverage

### Task 3.3: Improve Documentation
- **Status:** TODO
- **Files:** `docs/`, API docs
- **Effort:** 2 days
- **Dependencies:** None
- **Success Criteria:** 
  - Complete API documentation
  - Architecture decision records
  - Enhanced examples
- **Steps:**
  1. Set up Doxygen configuration
  2. Generate API documentation
  3. Create `docs/adr/` for architecture decisions
  4. Enhance sample code with explanations
  5. Add troubleshooting guide

## Implementation Roadmap

### Week 1: Stabilization
- **Days 1-2:** ✅ Task 1.1 - Memory management fixes (DONE)
- **Days 3-5:** Task 1.2 - Core function implementation
- **Day 5:** Task 1.3 - Memory safety audit

### Week 2: Infrastructure
- **Days 6-7:** Task 2.1 - CMake modernization
- **Day 8:** Task 2.2 - CI/CD setup
- **Day 9:** Task 2.3 - Code quality automation

### Week 3: Polish
- **Days 10-12:** Task 3.1 - Code standardization
- **Days 13-14:** Task 3.2 - Test enhancement
- **Day 15:** Task 3.3 - Documentation improvement

## Dependencies & Risks

1. **Task 1.1** must precede other changes (blocking dependency) ✅ DONE
2. **Build system changes** may break existing workflows (mitigation: gradual migration)
3. **Code standardization** requires careful review to avoid regressions
4. **Test reorganization** should maintain backward compatibility

## Success Metrics

### Quantitative Metrics
1. **Memory Safety:** Zero valgrind errors, ASan clean
2. **Test Coverage:** >90% line coverage for core components
3. **Build Time:** <30% increase after removing glob patterns
4. **Code Quality:** <5% deviation from style guide

### Qualitative Metrics
1. **Developer Experience:** Clear error messages, intuitive APIs
2. **Maintainability:** Easy to add new features, minimal coupling
3. **Documentation:** Comprehensive guides for common tasks
4. **Performance:** No regression in simulation speed

## Current Status

- **Completed:** 1/9 tasks (11%)
- **In Progress:** 0/9 tasks
- **Remaining:** 8/9 tasks

### Task List
- [x] Task 1.1: Fix Memory Management Segfaults
- [ ] Task 1.2: Implement Missing Core Functions
- [ ] Task 1.3: Memory Safety Audit
- [ ] Task 2.1: Fix CMake Anti-patterns
- [ ] Task 2.2: Establish CI/CD Pipeline
- [ ] Task 2.3: Code Quality Automation
- [ ] Task 3.1: Standardize Code Conventions
- [ ] Task 3.2: Enhance Test Suite
- [ ] Task 3.3: Improve Documentation
