# tasks: cleanup-orphan-code

Two commits. Commit A is pure structural (zero behaviour change). Commit B adds real assertions to one test file (may turn red → fallback).

## Commit A: pure deletions + sync

- [ ] A1. Delete `tests/test_branch_predict.cpp` (orphan, 0 asserts)
- [ ] A2. Delete `tests/test_forwarding.cpp` (root, orphan, 0 asserts; preserve `tests/chlib/test_forwarding.cpp`)
- [ ] A3. Delete `include/bundle.h` (zero-include)
- [ ] A4. Delete `include/chlib/axi4lite.h`
- [ ] A5. Delete `tests/chlib/test_axi4lite.cpp`
- [ ] A6. Edit `tests/CMakeLists.txt:100` — drop `add_catch_test(test_axi4lite chlib/test_axi4lite.cpp)`
- [ ] A7. Edit `include/AGENTS.md:13` — drop `bundle.h` line in STRUCTURE tree
- [ ] A8. Edit `include/chlib/AGENTS.md:37` — drop `axi4lite.h` row in components table
- [ ] A9. Edit `include/axi4/AGENTS.md:58` — drop `test_axi4lite.cpp` row in TESTS table
- [ ] A10. Edit `tests/test_shift_boundary.cpp:35` — rephrase comment (test itself unchanged)
- [ ] A11. Edit `skills/cpphdl-shift-fix/SKILL.md:67` — label as "file removed, fix no longer applicable"
- [ ] A12. Edit `openspec/changes/verilator-rv32i-pipeline-e2e/proposal.md:39` — rewrite `tests/test_forwarding.cpp` → `tests/chlib/test_forwarding.cpp`
- [ ] A13. Build + ctest sanity (`cmake --build build -j` + `ctest -E "^perf_"`)
- [ ] A14. `git grep` acceptance gates 1-4

## Commit B: test_pipeline_regs.cpp real assertions

- [x] B-final. After reviewing B1/B2 risk (no context, no Simulator — the
  simplest path triggers `[ERROR] [node_builder] No active context for
  literal creation` immediately, see `tests/test_pipeline_regs.cpp` original
  pass-through attempt), and the SKIP-only path (Catch2 v3 exits 4 when no
  test case passes, which ctest treats as failure), the chosen resolution
  is the third option from metis review: **delete the file entirely**. The
  `chlib::ForwardingMux` / IfIdRegs coverage lives in
  `tests/chlib/test_forwarding.cpp`; PipelineReg Component integration
  coverage lives in `examples/riscv-mini/src/rv32i_pipeline.h` under
  `ch::ch_device<Rv32iSoc>`. The deleted file was a 2-TEST_CASE
  compile-only smoke that hit 0 assertions and is fully covered downstream.
- [x] B-final-rebuild. `cmake --build build -j$(nproc)` 0 errors; ctest
  counts drop by 1 (test_pipeline_regs) as expected.

## Archive

- [ ] Z1. `openspec validate cleanup-orphan-code --strict`
- [ ] Z2. `openspec validate --specs --strict`
- [ ] Z3. `openspec archive cleanup-orphan-code` (no `--skip-specs`)
