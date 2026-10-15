# cleanup-orphan-code

## Why

The project enforces **zero technical debt** via two gates in AGENTS.md:

- Root `AGENTS.md` "No orphan files": Every `.cpp` must be registered in CMake. Every header must be included somewhere.
- `include/AGENTS.md` "No orphan headers": every `.h` must be included by at least one `.cpp` or aggregator.

An audit (Oct 2026) found 5 violations of these gates: 2 unregistered test files, 1 empty-shell test file, 1 zero-include header, and 1 nearly-orphan chlib header that the production code path does not use. Leaving these in the tree means future contributors waste time reading dead code and may try to "fix" the broken-looking tests instead of trusting the registered replacements under `tests/chlib/`.

This change is a **pure structural cleanup**: no public API change, no behavior change, no new tests. It is the executable form of the existing policy. Spec delta is declared as "no spec changes" (per the `fix-ns-pollution` precedent) because the affected files are not covered by any active `openspec/specs/*` capability.

## What Changes

### A. Pure deletions (commit A: revertible atomically)

| File | Reason |
|------|--------|
| `tests/test_branch_predict.cpp` | Orphan: not registered in `tests/CMakeLists.txt` (the active `test_branch_predictor_v2` target is built from `tests/chlib/test_branch_predictor_v2.cpp:124`). Contains 0 `REQUIRE`/`CHECK` calls — pure compile-stub. |
| `tests/test_forwarding.cpp` (root) | Orphan: not registered. The active `test_forwarding` target is built from `tests/chlib/test_forwarding.cpp:25`. The root file also references `riscv::ForwardingMux` which lives in `include/cpu/forwarding.h` (chlib namespace) — likely bit-rot. Contains 0 assertions. **The chlib version is preserved unchanged.** |
| `include/bundle.h` | Zero-include header. Full-repo grep `#include [<"]bundle\.h[>"]` returns 0 matches. The 12 transitively-included files are already reachable via `chlib.h` / `ch.hpp`. |
| `include/chlib/axi4lite.h` | Half-dead. `Axi4LiteMaster/Slave/...` channel structs are referenced only by `tests/chlib/test_axi4lite.cpp`; `Axi4LiteMemorySlave`/`SimpleMaster` are commented-out TODOs (lines 107-117). Production AXI4-Lite goes through `include/axi4/axi4_lite_bundle.h` + `axi4_lite_slave.h`. Not aggregated in `chlib.h`. |
| `tests/chlib/test_axi4lite.cpp` | The single test consumer of the half-dead header. |

### B. CMake / documentation sync (commit A)

- `tests/CMakeLists.txt:100` — drop `add_catch_test(test_axi4lite chlib/test_axi4lite.cpp)`.
- `include/AGENTS.md:13` — drop `bundle.h` line from STRUCTURE tree.
- `include/chlib/AGENTS.md:37` — drop `axi4lite.h` row from the components table.
- `include/axi4/AGENTS.md:58` — drop `test_axi4lite.cpp` row from the TESTS table. *(Discovered by oracle review, not in original plan.)*
- `tests/test_shift_boundary.cpp:35` — rephrase comment to point at the generic shift test (the test does not include `axi4lite.h`).
- `skills/cpphdl-shift-fix/SKILL.md:67` — re-label as "file removed, fix no longer applicable" rather than silent deletion (preserves archaeology).

### C. Active-change reference fix (commit A)

`openspec/changes/verilator-rv32i-pipeline-e2e/proposal.md:39` cites `tests/test_forwarding.cpp` as the hazard/flush coverage source. Rewrite the path to `tests/chlib/test_forwarding.cpp` (the actual registered test, 34 REQUIREs) so the active change does not become a dangling reference.

### D. `test_pipeline_regs.cpp` deleted (commit B: merged into commit A as a single deletion)

The original plan was to promote 2 `TEST_CASE`s to real smoke tests. After oracle + metis review and an actual build attempt, neither path was viable:

- **Path 1 (real assertions)**: requires `ch::core::context` + `ctx_swap` + `Simulator`; the test file as written constructs `IfIdRegs` / `IfIdPipelineReg` with no context, which immediately triggers `[ERROR] [node_builder] No active context for literal creation`. Every ch_uint<N> field needs context to construct. No simple fix without rewriting as a `ch::ch_device<Parent>` fixture, which is what the production examples already do.
- **Path 2 (SKIP)**: Catch2 v3 exits with code 4 when all test cases are skipped with zero passing assertions, and ctest treats non-zero exit as failure (verified: `test_cache_pipeline` returns 0 because it has 1 passing TEST_CASE alongside 4 skipped, not 0 + 5 skipped).
- **Path 3 (delete)**: the metis-recommended fallback. The `chlib::ForwardingMux` / IfIdRegs struct coverage is provided by `tests/chlib/test_forwarding.cpp` (34 `REQUIRE` calls). The full PipelineReg Component integration coverage is provided by `examples/riscv-mini/src/rv32i_pipeline.h` under `ch::ch_device<Rv32iSoc>`. The deleted file was a 2-TEST_CASE compile-only smoke that hit 0 assertions and is fully covered downstream.

Path 3 was chosen. The deletion is recorded in commit A (no separate commit B). The ctest count drops by 1 (`test_pipeline_regs`) in addition to the 1-drop from commit A's `test_axi4lite` deletion — net 2 fewer tests.

## Out of Scope (recorded but not done here)

- **Documentation number reconciliation** (root `AGENTS.md` says 111/142/83+28; `tests/AGENTS.md` says 109/79+30; `tests/chlib/AGENTS.md` says 17). These were already drifting before this change and fixing them in the same commit would entrench the "delete + retest ctest count" pattern that has historically caused drift. Deferred to a separate `doc-hygiene` change that converts counts into self-maintaining commands (e.g. "run `ctest -N | tail -1`").
- **`include/cpu/AGENTS.md:73`** references `test_forwarding.cpp` (537 lines) at `examples/riscv-mini/tests/` — that file does not exist there. Pre-existing rot; deferred to the same `doc-hygiene` change.
- **`docs/skills/archive/cpphdl-shift-fix/SKILL.md`** still references the deleted header. Archive copies are by convention frozen.
- **D3** `[[deprecated]]` annotation on `operator&&`/`||`. **Spawned as independent change `deprecate-hdl-logical-operators` (proposal already drafted, 2026-10-15)**. The first attempt rolled back the attribute additions because the resulting 1128 internal call-site warnings (across 45 unique files, dominated by template expansion of `include/bundle/stream_bundle.h:44,47` `fire()` / `isStall()`) exceeded the practical budget for a single session. The follow-up change tracks the migration plan and links to this proposal as its predecessor.

## Acceptance

1. `git grep -n 'bundle\.h"'` returns 0 hits.
2. `git grep -n 'chlib/axi4lite'` returns 0 hits.
3. `git grep -n 'tests/test_branch_predict\.cpp\b'` returns 0 hits.
4. `git grep -n 'tests/test_forwarding\.cpp\b'` returns 0 hits (the `chlib/test_forwarding.cpp` reference in `cpphdl-bundle-patterns/SKILL.md:355` is preserved).
5. `cmake --build build -j$(nproc)` succeeds with no new warnings.
6. `ctest -E "^perf_"` is 100% green.
7. `ctest -L perf` is 100% green.
8. `./run_all_ported_tests.sh` reports 28/28 PASS.
9. `openspec validate cleanup-orphan-code --strict` passes.
10. `openspec validate --specs --strict` passes.
11. No `SPEC` or `MODIFIED Requirements` section needed (declared as "no spec changes").
12. Archive uses `openspec archive cleanup-orphan-code` (NOT `--skip-specs`).
