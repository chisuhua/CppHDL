# deprecate-hdl-logical-operators-redo

## Status: 2026-10-16 redo-pending-migration

This is the **sequel** to `2026-10-15-deprecate-hdl-logical-operators`
(which archived in the same state but never landed code). The original
attempt emitted [[deprecated]] attributes with only **partial migration**;
a clean rebuild produced 315 `-Wdeprecated-declarations` warnings across
20 files (a different and smaller cluster than the 1128 originally
quoted — many of the AGENTS.md / docs landsite `&&`s were template /
comment / pointer-to-bool leftovers, not ch_bool overload calls, and
the previous budget quote was inflated).

This redo reverses the order documented in the original proposal:

**Migration FIRST, attributes SECOND.**

1. Land all internal call-site rewrites (`&&`/`||` on HDL values
   → `&`/`|`) across the full ~200-line remaining cluster —
   primarily `include/axi4/*.h`, `examples/spinalhdl-ported/*`,
   `examples/riscv-mini/src/*`, `examples/axi4/phase3a_complete.cpp`,
   `samples/pod_bundle_conversion.cpp`.

2. Verify: `cmake --build build --target install -j$(nproc)` produces
   zero `-Wdeprecated-declarations` for the 8 overloads (LLVM
   `aligned_storage_t` warnings are unrelated and pre-existing).

3. Only then add the 8 `[[deprecated(...)]]` attributes.

This order guarantee avoids the "self-warning state" failure mode
that killed the first attempt.

## What Changes

This change is the proposal + planning artifact for the eventual
`[[deprecated]]` annotation of the 8 `&&` / `||` overloads. It is
documentation-only at the working-tree level; the actual code change
is gated on completing the full internal call-site migration
(see `tasks.md`). Archiving this change closes the planning phase
but does **not** mark the deprecation as applied to the codebase.

The change is declared "no spec changes" because the deprecation
affects the public operator surface but does not change any
behaviour tracked by `openspec/specs/*/spec.md`. Per the
`fix-ns-pollution` precedent, the archive step uses `--skip-specs`.

## Why

`AGENTS.md` already bans `operator&&` / `operator||` on IO ports and
recommends `select()`. The existing ban is a soft documentation rule;
there is no compile-time enforcement. Meanwhile the 8 `&&` / `||`
overloads (2 in `include/core/io_logical.h` for `port<>`, 2 in
`include/core/operators_logic.h` for `ch_bool`, 4 mixed
`ch_bool`/`ch_uint<N>`) silently drop the rule whenever a user types
`bundle.valid && bundle.ready` — a hot path in stream code.

The intent of this change is to lift the soft ban into a hard
compile-time signal: marking the 8 overloads `[[deprecated]]` with
diagnostic messages, then migrating all internal call sites to the
recommended `&` / `|` (for 1-bit logic) or `select()` (for value
muxing).

The 4 mixed `ch_bool`/`ch_uint<N>` overloads have a **separate**
correctness motivation: they implement `rhs != 0` reduction
(`true && 0b10 == 1` but `true & 0b10 == 0`). A future contributor
who mechanically rewrites `&&` to `&` to "fix" the deprecation would
silently break this. Marking them `[[deprecated]]` with a message
that names the nonzero test prevents that mistake.

## What changes (the eventual end state)

### A. 8 attribute additions

| File | Symbol | Migration target |
|------|--------|------------------|
| `include/core/io_logical.h:66-72` | `port && port` | `&` (DAG-identical) |
| `include/core/io_logical.h:74-80` | `port \|\| port` | `\|` (DAG-identical) |
| `include/core/operators_logic.h:161-163` | `ch_bool && ch_bool` | `&` (DAG-identical) |
| `include/core/operators_logic.h:165-167` | `ch_bool \|\| ch_bool` | `\|` (DAG-identical) |
| `include/core/operators_logic.h:174-178` | `ch_bool && ch_uint<N>` | `(rhs != 0_d) & lhs` (do NOT rewrite to `&`) |
| `include/core/operators_logic.h:180-184` | `ch_uint<N> && ch_bool` | `(lhs != 0_d) & rhs` |
| `include/core/operators_logic.h:186-190` | `ch_bool \|\| ch_uint<N>` | `lhs \| (rhs != 0_d)` |
| `include/core/operators_logic.h:192-196` | `ch_uint<N> \|\| ch_bool` | `lhs \| (rhs != 0_d)` |

Deprecation messages:

- `ch_bool`-only and `port`-only overloads: `"use operator& for 1-bit AND; select() is for value muxing"`
- mixed `ch_uint<N>` overloads: `"mixed &&/|| with ch_uint<N> uses nonzero test; write (rhs != 0) explicitly"`

### B. ~200-400 internal call-site migrations (45 files, 1128 warning instances)

| Region | Files | Notes |
|--------|-------|-------|
| `include/bundle/stream_bundle.h:44,47` | 1 file, 2 lines, 2 instances, but **template-expanded per TU** | **Critical**: must be migrated in the same commit as the attributes, otherwise every stream user TU carries the warning |
| `include/chlib/` | ~12 files (`stream.h`, `fifo.h`, `state_machine.h`, ...) | sed-able: `s/&& /& /g` is safe for `ch_bool` operands; the 4 mixed overloads are not used in this directory |
| `include/axi4/` | ~6 files (`axi4_full.h`, `axi_i2c.h`, `axi_dma.h`, `axi_timer.h`, `axi_to_axilite.h`, `axi_lite_matrix.h`) | `axi_timer.h` has port-level `&&` (`io().x && io().y`); 5 others are ch_bool |
| `include/cpu/` | ~5 files (`i_cache_complete.h`, `d_cache_complete.h`, `id_stage.h`, `if_stage.h`, `rv32i_tcm.h`) | ch_bool |
| `examples/spinalhdl-ported/` | ~7 files | ch_bool |
| `examples/riscv-mini/src/` | ~5 files (`rv32i_regfile.h`, `rv32i_regs.h`, `rv32i_alu.h`, `rv32i_core_simple.h`, `stages/{ex,id}_stage.h`) | ch_bool |
| `examples/axi4/phase3a_complete.cpp` | 1 file | port-level |
| `samples/` | 2 files (`bundle_top_example.cpp`, `pod_bundle_conversion.cpp`) | mixed |
| `tests/test_chipforge_mux_segv_repro.cpp`, `tests/test_comparison_ops.cpp` | 2 files | **keep** the deprecation with `#pragma GCC diagnostic ignored "-Wdeprecated-declarations"` to preserve regression coverage of the still-existing (deprecated) API path until physical removal |

### C. 4 categories of "do not migrate" sites

1. **Truly dead code**: `examples/riscv-mini/src/rv32i_system.h:182` uses
   `ch_bool && port` which has no viable overload (port has no
   `operator lnode<ch_bool>()` direct conversion). The file is not in
   any CMake target (`examples/riscv-mini/CMakeLists.txt:8-64`). It
   was already orphaned by an earlier change; deprecate-hdl-logical
   should NOT attempt to migrate it — that is a separate "fix bit-rot"
   change.
2. **Mixed ch_uint overloads**: see Section A table — these are not
   safe to `&&`→`&` rewrite. Pre-validate with
   `grep -nE '\bch_uint\b.*\&\&|&&.*ch_uint\b' include/ src/ examples/`
   to confirm zero in-tree use before deleting the overloads.
3. **Test files covering the deprecation itself**: see
   `tests/test_chipforge_mux_segv_repro.cpp` and
   `tests/test_comparison_ops.cpp` — wrap in pragma to retain
   coverage.
4. **VCD/trace signal-name compatibility**: the `ch_bool` overloads
   produce debug names `"logical_and"` / `"logical_or"`. After
   migration, downstream sites will produce `"and"` / `"or"`. No
   test currently asserts on these names (verified by
   `grep -rn 'logical_and\|logical_or' tests/` → 0 hits), so this is
   safe to break.

### D. Documentation sync (mandatory)

- `AGENTS.md:57,104` — the "use select() NOT &&" wording is too broad.
  After this change it should be: *"`&&`/`||` on HDL values is
  deprecated; use `&`/`|` (1-bit logically equivalent) or `select()`
  (value muxing)"*.
- `include/axi4/AGENTS.md` — its anti-pattern entry currently cites
  `axi_timer.h:77-81`; after migration the cited lines are different.
- `docs/adr/ADR-010-connection-semantics.md:146-169` — section §5
  currently says "保留操作符 + TODO". The supersede is implicit: the
  TODO becomes a real deprecation. Either add a "Superseded by:
  deprecate-hdl-logical-operators" note to ADR-010, or write a new
  ADR-039 (or whatever the next number is) that records the
  decision to deprecate. The latter is the project's house style
  per `docs/developer_guide/openspec-specs-guide.md`.

### E. CI guard (optional, recommended)

Add a check to the existing CI workflow that fails the build when
`-Wdeprecated-declarations` warnings appear in the changed files.
Without this, the policy is paper-only: nothing prevents a future
contributor from re-adding `&&` and silently living with 1128
warnings. The cheapest implementation is:

```yaml
- name: Build with deprecation-as-error
  run: |
    cmake --build build -j$(nproc) 2>&1 | tee build.log
    ! grep -q "deprecated-declarations" build.log
```

Or scoped: add `add_compile_options(-Werror=deprecated-declarations)`
to `target_compile_options(cpphdl PRIVATE ...)` only.

## Out of scope (recorded but not done)

- **Removing the 8 overloads entirely**. Deprecation first; physical
  removal in a future change after at least one release cycle.
- **The `examples/riscv-mini/src/rv32i_system.h` orphan** — separate
  "fix bit-rot" change.
- **Updating `Compare.md`, `docs/archive/`, and other historical
  documents** that mention `&&` / `||` on HDL values — historical
  record is frozen.

## Acceptance

1. `git grep -nE 'operator&&\|operator\|\|' include/core/io_logical.h include/core/operators_logic.h` shows exactly 8 definitions, each preceded by `[[deprecated(`.
2. `cmake --build build -j$(nproc) 2>&1 | grep -c deprecated` returns 0 (or, if pragma-wrapped tests are exempted, ≤ 8 — the 8 from the pragma'd test files).
3. `ctest -E "^perf_"` 100% green.
4. `ctest -L perf` 100% green.
5. `./run_all_ported_tests.sh` 28/28 PASS.
6. `openspec validate deprecate-hdl-logical-operators --strict` passes.
7. `openspec validate --specs --strict` passes (no spec changes are introduced; this is a code-deprecation change).

## Why deferred from 2026-10-15 session

- 8 attribute additions: 1 commit, ~10 min
- 200-400 internal call-site migrations: estimated 2-3 commits by
  directory (`include/bundle` first, then `include/chlib`, then
  `include/axi4`, then `examples/`, then `tests/`). Sed-able for
  ch_bool regions, manual for the 4 mixed overloads (zero found in
  current code, but each must be re-verified).
- Documentation sync: 3 files, 1 commit.
- CI guard: 1 commit.

Total estimate: 5-7 commits over 1-2 hours of focused editing, plus
a full `ctest -L perf` re-run (~22 min) and the `run_all_ported_tests.sh`
sweep. The 2026-10-15 session exhausted its budget on Phase 1 (trivial
docs) and Phase 2 (orphan-code cleanup) and surfaced this change as
the next-priority item, but did not have headroom for the
mechanical sed-rewrite pass.

## See also

- `AGENTS.md` "ANTI-PATTERNS" — the soft ban this change hardens.
- `docs/adr/ADR-010-connection-semantics.md:146-169` — the prior
  decision to retain the operators.
- `openspec/changes/cleanup-orphan-code/proposal.md` — the previous
  cleanup batch; its "Out of scope: D3" pointer to this change.
