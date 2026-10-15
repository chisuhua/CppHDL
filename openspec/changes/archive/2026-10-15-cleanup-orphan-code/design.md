# design: cleanup-orphan-code

## Why two commits

Commit A is a pure structural cleanup. The file deletions, the `tests/CMakeLists.txt` registration removal, and all documentation sync are mutually consistent: a `git revert -n HEAD~1..HEAD` cleanly undoes everything. The test count drops by exactly 2 (`test_axi4lite`, `test_branch_predictor_v2`'s name shadow is unaffected).

Commit B changes observable test behaviour: it adds `REQUIRE_NOTHROW` and `sim.get_value(...)` calls. There is a real risk that TEST_CASE 2 (Component instantiation with `parent = nullptr`) hits `Component::current() == nullptr` (root AGENTS.md ANTI-PATTERNS warns about this). If that happens, the fallback is the *delete* branch (B3 option "drop TEST_CASE 2"), which is preferred over relaxing the assertion into a meaningless one.

Mixing A and B into one commit would entangle the revert: reverting would also undo the deletions, defeating the cleanup. Splitting is the revert-friendly default.

## Why no worktree

This main worktree is the only one (verified `git worktree list` returns just the main checkout). Phase 1 is in the working tree as uncommitted edits on files that do not overlap Phase 2's files (Phase 1: `AGENTS.md`, `include/core/{context,io}.h`, `include/cpu/cache/{i,d}_cache.h`, `src/codegen_*.cpp`; Phase 2: `tests/test_*.cpp`, `include/bundle.h`, `include/chlib/axi4lite.h`, `tests/CMakeLists.txt`, plus 4 doc files). The risk of a CMake re-configure wiping build artifacts is real but bounded; the cost of a worktree (independent configure, full ctest, archive-and-merge) does not pay back for a change that touches no public API.

## Why no spec delta

`openspec/specs/` covers `chlib-aggregator` and `verilator-e2e-coverage`. Neither names the files being deleted. The `chlib-aggregator` spec constrains the *aggregator* `chlib.h` and the `chlib/AGENTS.md` components table — we are updating the latter, not violating the former (the spec mandates the *entry point*, not the table contents). `verilator-e2e-coverage` does not mention `axi4lite.h` or the chlib/axi4lite/ test.

Therefore the change is declared "no spec changes" per the `fix-ns-pollution` precedent. `openspec archive` will not touch `openspec/specs/`.

## Why SKILL.md:67 is re-labeled not deleted

That row is in a "files with applied fix" archaeology table. The fix is in git history. Silently deleting the row loses the audit trail; relabeling preserves it.

## Why `axi4/AGENTS.md:58` is in scope (was discovered late)

`include/axi4/AGENTS.md:58` lists `test_axi4lite.cpp` in the TESTS table. The test file is being deleted. If the table row is not removed, `openspec validate` and any future `tests/AGENTS.md` audit will see a dangling reference.
