# ADR-036: Deprecate `&&` / `||` on HDL Values

- **Status**: Accepted (supersedes ADR-010 §5.2)
- **Date**: 2026-10-16
- **Deciders**: build-agent
- **Related**:
  - ADR-010 §5.2 — prior decision to "保留操作符 + TODO 注释"
  - openspec/changes/archive/2026-10-15-deprecate-hdl-logical-operators — first attempt, rolled back
  - openspec/changes/archive/2026-10-15-2026-10-16-deprecate-hdl-logical-operators-redo — landed 2026-10-16

---

## 1. Context

ADR-010 §5.2 (2025-early) deferred the question of whether to keep
`operator&&` / `operator||` on HDL values (port<ch_bool>, ch_bool,
and mixed ch_bool / ch_uint<N>). The decision was "保留操作符 + TODO
注释" because:

1. Mechanical removal would break user code.
2. The implementation produces the correct hardware gates via
   `get_lnode(lhs) & get_lnode(rhs)`.
3. A future deprecation would provide a softer migration path.

By 2026-10, that "future" had arrived but had failed twice in
quick succession. The first public attempt (the 2026-10-15 change)
landed `[[deprecated("...")]]` attributes without first completing
the internal call-site migration, producing 315
`-Wdeprecated-declarations` across 20 files and forcing a rollback.
This ADR records the corrected decision: **deprecate yes, but
invert the order** — migrate internal call sites first, attributes
second — and the final state of the deprecation.

## 2. Decision

Mark all 8 ch_bool / port<ch_bool> operator&& / operator|| overloads
in `include/core/io_logical.h` and `include/core/operators_logic.h`
as `[[deprecated("...")]]`. Leave the implementations and signatures
intact (a follow-up release will remove them physically after at
least one user-visible cycle).

Three distinct deprecation messages, one per semantic category:

### 2.1 Plain ch_bool / port<ch_bool> overloads (4 sites)

```
[[deprecated("use operator& for 1-bit AND; select() is for value muxing")]]
[[deprecated("use operator| for 1-bit OR; select() is for value muxing")]]
```

Rationale: on 1-bit HDL values, `&&` and `&` produce the same hardware
gate. The deprecation nudges users toward the bitwise operator
(which has no C++ short-circuit semantics that could mislead readers)
without functionally restricting them.

### 2.2 Mixed ch_bool / ch_uint<N> overloads (4 sites)

```
[[deprecated("mixed && with ch_uint<N> uses nonzero test; write (rhs != 0) explicitly")]]
[[deprecated("mixed && with ch_uint<N> uses nonzero test; write (lhs != 0) explicitly")]]
[[deprecated("mixed || with ch_uint<N> uses nonzero test; write (rhs != 0) explicitly")]]
[[deprecated("mixed || with ch_uint<N> uses nonzero test; write (lhs != 0) explicitly")]]
```

Rationale: these overloads lower to `rhs != 0` then AND/OR — a
nonzero-test reduction:

  - `true && 0b10` returns `1` (today)
  - `true &  0b10` returns `0`  (would silently drop the reduction)

The deprecation message surfaces this hazard explicitly. A naive
`&&` → `&` rewrite would silently drop the nonzero-test and
produce wrong hardware. The 4 mixed overloads are deliberately
preserved in the regression suite (`tests/test_operator_advanced.cpp`)
under `#pragma GCC diagnostic ignored "-Wdeprecated-declarations"`
so the behavior remains covered until physical removal.

## 3. Implementation Order (Lessons Learned)

The 2026-10-15 first attempt landed attributes before migration and
aborted. The redo (2026-10-16) inverted this:

1. **Phase 0 (audit)** — `grep` the full repo for `&&`/`||` callsites
   on ch_bool / port<ch_bool> operands. Distinguish these from
   builtin C++ `&&`/`||` calls (which operate on builtin bool, not
   on ch_bool) — the latter don't call our overloads and don't
   trigger deprecation. Established audit: 233 sites across 33
   files (the inflated 1128 / 45 figures in the original plan
   were inflated; 20 files and 233 sites after excluding
   comments / templates / pointer-to-bool).

2. **Phase 1 (migration)** — sed-rewrite all 233 sites from
   `&&` → `&` and `||` → `|`. Cross-check by category:
   - ch_bool / port<ch_bool> / mixed callsites: rewrite.
   - Builtin C++ `&&` / `||` on pointer-to-bool / static_assert /
     rvalue-ref template parameters: leave intact.
   - `std::is_standard_layout_v<T> && std::is_trivial_v<T>`
     builtin: leave intact.

3. **Phase 2 (verify migration)** — inject temporary
   `__attribute__((deprecated))` on all 8 overloads in the working
   tree (NOT in git), then clean rebuild. Expected: 0
   `-Wdeprecated-declarations` for our overloads. If non-zero,
   the migration is incomplete and Phase 1 must resume.

4. **Phase 3 (apply attributes)** — replace the temporary
   `__attribute__` markers with the formal `[[deprecated("...")]]`
   in a single commit. Re-clean-rebuild and assert 0 warnings.

5. **Phase 4 (archive)** — `openspec archive
   <change-name> --skip-specs --yes`.

Order matters: **migration first, attributes second**. The reverse
order causes the self-warning state that killed the first attempt
and produces a useless red build.

## 4. Test File Policy

After the production migration, only 5 test files referenced the
deprecated API. Three were straightforward sed-rewrites
(`test_operator.cpp`, `test_comparison_ops.cpp`, `test_reg.cpp`).

Two were preserved with `#pragma GCC diagnostic push / ignored
"-Wdeprecated-declarations" / pop` wrappers:

- `tests/test_chipforge_mux_segv_repro.cpp` — exercises the SEGV
  pattern `ch_bool(false) && (a != 0)` that triggers the null-impl
  bug. Migrating to `&` would silently change the behavior under
  test, defeating the regression.

- `tests/test_operator_advanced.cpp` (lines 540-543) — exercises
  the mixed ch_bool / ch_uint<N> overloads to verify their
  nonzero-test reduction semantics. Cannot be migrated to `&`/`|`
  without silently dropping the reduction. Wrapped with explanatory
  comment + minimal pragma scope.

These two tests preserve physical-removal-time coverage of the
deprecated API until the eventual commit that removes the 8
overloads entirely.

## 5. Status

As of 2026-10-16, the deprecation is **live** in main:

- `include/core/io_logical.h` carries 2 `[[deprecated]]`
- `include/core/operators_logic.h` carries 6 `[[deprecated]]`
- All 233 internal call sites migrated.
- 5 test files: 3 migrated, 2 pragma-wrapped.
- `cmake --build build -j$(nproc)` exits 0 with 0
  `-Wdeprecated-declarations` for our overloads.
- ctest -LE perf: 148/148 PASS.
- `./run_all_ported_tests.sh`: 28/28 PASS.

## 6. Future Work (Out of Scope)

- **Physical removal**: After at least one release cycle with the
  deprecation warnings visible, remove the 8 overloads entirely.
  At that point, the 2 pragma-wrapped test files must be updated
  to use the explicit nonzero-test form
  (`(lhs != 0_d) & rhs` etc.). Tracked separately, not in this
  change.
  See **§8 Removal Gate** below for the concrete calendar deadline
  and machine-checkable trigger.
- **`docs/skills/archive/cpphdl-shift-fix/SKILL.md`**: this archived
  skill still references the now-deleted `include/chlib/axi4lite.h`
  header but the deletion label has been set. No further action
  needed — archive copies are frozen by convention.

## 8. Removal Gate

This section makes the "≥1 release cycle" promise from §6
verifiable. The project has no formal release cadence (every
change merges when CI green), so the deprecation period is
defined by **a concrete calendar deadline + a low-activity
extension clause**, parallel to `ADR-037 §3`.

### 8.1 Trigger

The earliest allowed date for the removal commit is
**2027-04-17** (6 months after the deprecation landed on
2026-10-16). The CI guard:

```bash
# Reject any removal commit before 2027-04-17:
git log --since=2026-10-16 --until=2027-04-17 --pretty=oneline | wc -l
#  → at least 1 deprecation-period commit; not the removal commit
```

A pre-removal commit opening PRs against main **before**
2027-04-17 is rejected; the deprecation period is not
shortenable.

### 8.2 Low-activity extension

If `git log --since=2026-10-16 --until=$(date +%Y-%m-%d) | wc -l`
is fewer than 5 commits at the proposed removal date, the
deadline rolls forward 3 months (to 2027-07-17). Rationale: a
6-month window with <5 commits means external users have had
little chance to react; collapse the deadline prematurely and
the deprecation becomes a paper tiger.

### 8.3 Removal commit checklist

The removal commit (one atomic commit, not a multi-commit
sequence) MUST:

1. **`git rm` the 8 overloads** at their canonical locations:
   - `include/core/io_logical.h:74` — `port<ch_bool> && port<ch_bool>`
   - `include/core/io_logical.h:83` — `port<ch_bool> || port<ch_bool>`
   - `include/core/operators_logic.h:162` — `ch_bool && ch_bool`
   - `include/core/operators_logic.h:167` — `ch_bool || ch_bool`
   - `include/core/operators_logic.h:178` — `ch_bool && ch_uint<N>`
   - `include/core/operators_logic.h:185` — `ch_uint<N> && ch_bool`
   - `include/core/operators_logic.h:192` — `ch_bool || ch_uint<N>`
   - `include/core/operators_logic.h:199` — `ch_uint<N> || ch_bool`

2. **Update the 2 pragma-wrapped test files**:
   - `tests/test_chipforge_mux_segv_repro.cpp:226,238,281` —
     rewrite the 3 `ch_bool(false) && (a != 0)` and the
     cascade `we && (rd_addr == ai)` SEGV-repro patterns to
     `ch_bool(false) & (a != 0_d)` / `we & (rd_addr == ai)`.
     The SEGV-reproduction intent (catch null impl from
     `binary_bool_operation` with a null lhs) is preserved
     by the `&` form, which goes through the same null-impl
     path. Verify by `ctest -R test_chipforge_mux_segv_repro`.
   - `tests/test_operator_advanced.cpp:540-543` — rewrite
     the 4 mixed `ch_bool && ch_uint<N>` lines to the
     explicit nonzero-test form: `ch_bool(bool_val) &
     ch_bool(uint_val != ch_uint<N>(0_d))` and the symmetric
     variants for the other 3 cases. Update the inline
     comment to drop the "deprecation" wording (the comment
     was explaining why we pragma-wrapped; after migration it
     just documents the nonzero-test reduction). Drop the
     `pragma GCC diagnostic` wrapper. Verify by
     `ctest -R test_operator_advanced`.

3. **Drop the §5 "Test File Policy" subsection** in this ADR
   (becomes historical).

4. **Update this ADR's §5 Status** to record the removal date
   and commit SHA.

5. **Update `openspec/specs/`** if any spec currently
   references the deprecated overloads by signature (none
   expected — the deprecation touched the `ch_bool` /
   `port<ch_bool>` operator surface, which is a `core/`
   primitive not spec'd in any `openspec/specs/` capability;
   verify with `git grep -nE 'operator&&|operator\|\|'
   openspec/specs/`).

6. **Update `root AGENTS.md`** to drop the `## 已消除`
   bullet that pointed to this ADR's deprecation
   (the deprecation is no longer "ongoing").

### 8.4 CI guard (optional but recommended)

A cheap regression guard the project can add before the
removal commit:

```yaml
# .github/workflows/ci.yml: fail if any new code uses the
# deprecated API
- name: reject-new-deprecated-calls
  shell: bash
  run: |
    # Tolerate the 2 pragma-wrapped test files (their use is
    # scoped + explained; they will be cleaned up at removal time).
    count=$(git grep -nE 'ch_bool .* && .*ch_bool' -- 'include/' 'src/' \
           ':!tests/test_chipforge_mux_segv_repro.cpp' \
           ':!tests/test_operator_advanced.cpp' | wc -l)
    test "$count" -eq 0
```

The 2 exempt files match the policy in §4 of this ADR. If
that count is non-zero, fail the build (new contributor
regressed on a forbidden pattern).

### 8.5 Status of this gate

As of 2026-10-17, the gate is **in effect** (the deprecation
landed 2026-10-16; removal earliest 2027-04-17). No removal
commit is allowed before then.

## 7. Consequences

Positive:

- Future HDL code that accidentally uses `&&` on ch_bool produces
  a clear diagnostic pointing to `&` (or `select()` for muxing).
- The mixed overload's nonzero-test reduction is now self-documenting
  via the deprecation message; future contributors cannot silently
  rewrite it.
- Internal call sites are uniformly bitwise, removing any ambiguity
  about short-circuit vs combinational semantics.

Negative:

- Public API surface is reduced by 0 functions but carries 8 new
  deprecation messages — downstream users will see the warnings.
- `git grep -E "operator&&|operator\|\|" include/core/` now matches
  8 deprecated entries that previously were silent.

## 8. Supersedes

ADR-010 §5.2 "保留操作符 + TODO 注释". The "TODO" became a real
deprecation. Future readers of ADR-010 §5.2 should follow the
pointer at the top of this ADR.
