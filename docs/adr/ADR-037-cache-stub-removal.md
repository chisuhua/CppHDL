# ADR-037: cache stub 物理删除策略

- **Status**: Accepted (2026-10-17)
- **Date**: 2026-10-17
- **Deciders**: build-agent
- **Supersedes / builds on**: `ADR-036-deprecate-hdl-logical-operators.md` (the deprecate-then-remove two-phase pattern), commit `78fa178` (stubs marked `__attribute__((deprecated))`), commit `67cc739` (renamed to `*_stub.h` suffix)
- **Related**: root `AGENTS.md` §KNOWN LIMITATIONS "Cache stubs i_cache_stub.h / d_cache_stub.h are pass-through"

---

## 1. Context

The cache stubs in `include/cpu/cache/{i,d}_cache_stub.h` are
**pass-through smoke fixtures**:

```
ready := req, miss := false, data := axi_data
```

They were marked `__attribute__((deprecated))` in commit `78fa178`
(2026-10-17) pointing at the canonical `*_complete.h` variants,
then renamed to `*_stub.h` suffix in commit `67cc739` to eliminate
the include-path ambiguity. Root `AGENTS.md` §KNOWN LIMITATIONS
documents them as a "by-design gap, awaiting redesign" debt.

Three test files in `tests/chlib/` exercise the stubs under
`#pragma GCC diagnostic ignored "-Wdeprecated-declarations"`:

| File | TEST_CASEs | What they actually assert |
|------|------------|---------------------------|
| `test_cache_simple.cpp` | 2 | `REQUIRE(true)` (smoke instantiate) |
| `test_cache_pipeline_integration.cpp` | 5 (3 SKIP) | `ready==req, miss==0, data==axi_data` (pass-through semantics explicit) |
| `test_phase4_integration.cpp` | 7 (5 SKIP) | `REQUIRE(true)` (smoke) |

Audit (2026-10-17) of `include/cpu/cache/{i,d}_cache_complete.h`
confirms the canonical implementation:

- `ICacheComplete` does a real LRU Hit/Miss check
  (`hit_way0 = valid && tag_match` then `hit = hit_way0 || hit_way1`)
- On **first access** every entry is invalid → `miss = true`
- `data` comes from `cache_data(63_d, 32_d)` (high 32 bits of the
  hit way's 64-bit storage), **not** from `io().axi_data`
- There is **no** `pass-through` / `bypass` / `mode` config field
  in `ICacheConfig`

So the stub's `ready := req, miss := false, data := axi_data`
behavior is **not replicable** by `ICacheComplete` without code
changes. Test `test_cache_pipeline_integration.cpp` L62-69
explicitly asserts `miss == 0u`, which `ICacheComplete` will fail
on the first access by construction.

The deprecation phase (commits `78fa178` + `67cc739`) is the
correct pre-step; this ADR records the **removal** decision.

## 2. Decision

**Defer physical removal by 6 months** (until 2027-04-17) and
record the concrete 3-prong migration path that must complete
before deletion:

1. **Add a `BYPASS` mode to `ICacheConfig` / `DCacheConfig`** in
   `*_complete.h` that, when set, emits the stub's pass-through
   semantics: `ready := req, miss := false, data := axi_data`.
   This makes the stub **redundant**, not a different feature.
2. **Rewrite `test_cache_pipeline_integration.cpp`** to
   instantiate `ICacheComplete` (not the stub) with `BYPASS=true`
   in its 3 active `TEST_CASE`s; the 2 SKIP'd ones stay SKIP'd
   (they were already gated on a not-yet-implemented
   `Rv32iPipelineWithCache`).
3. **Delete the 3 test files' pragma wrappers** and the
   `*_stub.h` headers themselves. Update root `AGENTS.md`
   §KNOWN LIMITATIONS to drop the cache-stub bullet. Update
   `include/cpu/AGENTS.md` STRUCTURE row.

If (1) turns out to be infeasible (e.g. `BYPASS` mode would
require a non-trivial refactor that conflicts with the
`ICacheComplete` Hit/Miss state machine), fall back to
**variant (b)**: delete only `test_cache_pipeline_integration.cpp`
(which asserts pass-through semantics) and keep
`test_cache_simple.cpp` / `test_phase4_integration.cpp` against a
slimmed-down `ICacheComplete` smoke fixture (no pass-through
assertions, just "instantiate, ch_reg<...> visible").

## 3. Why 6 Months (Not "≥1 Release Cycle")

ADR-036 records the ch_bool `&&` / `||` deprecation as gated by
"≥1 release cycle", but the project has no formal release
cadence (every change is "merge when CI green"). That phrasing is
unverifiable. This ADR fixes a concrete calendar deadline
(2027-04-17) so the deprecation period is machine-checkable via
`git log --since=2026-10-17 --until=2027-04-17` (any time the
removal PR opens, reject if earlier).

Six months balances two concerns:

- Long enough for downstream consumers (none tracked in this
  repo, but any external fork) to see the `[[deprecated]]`
  diagnostic and react.
- Short enough that the stubs don't rot in the source tree for
  another year. 6 months is a single quarter — aligns with
  typical project planning cycles.

If at the 6-month gate the project has fewer than 5 commits
since 2026-10-17, treat as "low-activity period" and roll
forward 3 months (to 2027-07-17) rather than delete the stubs
prematurely.

## 4. Implementation Order (Future PR, Not This ADR)

Three atomic commits, each buildable in isolation:

1. **Add `BYPASS` mode to `ICacheConfig` / `DCacheConfig`** in
   `*_complete.h`; new `make_passthrough_*` factory helper that
   returns an instance pre-configured for stub semantics. New
   `tests/test_cache_complete_bypass.cpp` smoke-asserts
   `ready==req, miss==0, data==axi_data` against the bypass-mode
   complete cache. Gate: this test passes.
2. **Rewrite `test_cache_pipeline_integration.cpp` L45-69 and
   L74-100** to use `ICacheComplete` with `BYPASS=true`. Drop
   the `pragma GCC diagnostic` wrapper (no longer needed; the
   stub is not referenced). Gate: `ctest -R test_cache_pipeline
   -j$(nproc)` passes.
3. **`git rm` `include/cpu/cache/{i,d}_cache_stub.h`** and the
   two SKIP-only test files (`test_cache_simple.cpp`,
   `test_phase4_integration.cpp` — both asserted only
   `REQUIRE(true)`, fully covered by (1)'s smoke). Update
   `tests/CMakeLists.txt` to drop the `add_catch_test()` lines.
   Update `include/cpu/AGENTS.md` STRUCTURE row and root
   `AGENTS.md` §KNOWN LIMITATIONS. Gate: `cmake --build build`
   and `ctest -LE perf` both green.

If (1) is infeasible, replace (1) with a slimmed-down
`ICacheComplete` smoke test that does NOT assert pass-through
semantics (just "instantiate, all `ch_reg` slots visible"). The
net effect is the same: the `*_stub.h` headers and their 3 test
consumers disappear.

## 5. Status

As of 2026-10-17, the stubs are **deprecated but live** —
commit `78fa178` adds `__attribute__((deprecated))` on the
class declarations; commit `67cc739` renames them to
`i_cache_stub.h` / `d_cache_stub.h`. Three test files
pragma-wrap their use. This ADR records the removal plan;
implementation follows in a future change gated on the 6-month
deadline.

**2026-10-18 update (architect review)**: A first attempt to execute
the §4 plan ahead of schedule (in change
`.rddf/improvements/cache-stub-physical-removal.md`, status: docs-only
phase) revealed that the migration target (`*_complete.h`) is
**dead-on-arrival**: cmake build probe of the `describe()` body surfaces
11 pre-existing compile errors, including a structural one (SRAM
declared as C-style `ch_reg<...>[128][2]` but indexed at runtime via
`ch_uint<7>` from `bits<...>(io().addr)`). The rewrite required to
make `*_complete.h` actually instantiable is **out of scope of this
ADR** — it is tracked separately as improvement
`.rddf/improvements/cache-complete-doa-note.md` (a `ch_mem`-based
rewrite). Consequence: the 6-month deadline (2027-04-17) now has a
**hard prerequisite** (the ch_mem rewrite must land first). If the
rewrite is not done by 2027-04-17, the stubs are **kept** (with their
existing `[[deprecated]]` attribute) rather than deleted with a broken
replacement, per ZERO-DEBT POLICY "no orphan files / no broken
build". The §2 variant (b) "delete only `test_cache_pipeline_integration.cpp`"
remains a valid fallback if the rewrite is still in progress at the
deadline.

**2026-10-19 update (cache-complete-rewrite Session B done)**:
Prerequisite 已满足 — cache-complete-rewrite 落地 (commit 91a0833 ch_mem rewrite
+ 1221819/49902b2/bb286a2 test + variant split). openspec change
'cache-complete-rewrite' 已 archive (2026-10-19). 本 ADR §2 重启流程:
- 流程已 `.rddf/improvements/cache-stub-physical-removal.md` status 改回 Active
- 下一步: 用户决议 variant (a) "BYPASS mode" 还是 variant (b) "Slim smoke + git rm stub"
- Deadline 2027-04-17 还有 6 个月, **不立即 git rm** — 按用户决议推进 variant
- 当前 stub 维持 `[[deprecated]]`, 3 个 test 文件仍 pragma-wrap; Sync/Async 测试
  不引用 stub (cache_complete.h 已 shim → Sync)

## 6. Future Work (Out of Scope)

- A new `BYPASS` mode in `*_complete.h` is a substantive
  feature, not a refactor. If it grows, consider a
  `config/ICacheMode` enum rather than a boolean, with future
  modes like `WRITE_THROUGH`, `WRITE_BACK`, `STREAMING`. Not
  needed for the stub-removal scope.
- The `*_complete.h` files' test surface today is `tests/CMakeLists.txt`
  `add_catch_test(test_cache_simple ...)` and
  `test_cache_pipeline_integration`. Both are stub-consuming;
  after this change, neither will reference the stubs. New
  cache-specific test should be `test_cache_complete_bypass.cpp`
  (added in commit 1 of §4) and `test_cache_complete_lru.cpp`
  (out of scope; would be the first real LRU-behavior coverage
  in the tree, also recorded as a future follow-up).

## 7. Consequences

Positive:

- The "stub vs complete" include-path ambiguity disappears
  entirely. Grep `cpu/cache/` returns either `*_complete.h`
  (canonical) or future variants; never two paths that look
  plausible.
- A 6-month concrete deadline replaces the "≥1 release cycle"
  verbiage that the project has no way to check.
- The new `BYPASS` mode is a small, opt-in config that lets
  future smoke fixtures be written without re-introducing a
  stub class.

Negative:

- The 3 test files stay alive (with pragma) for ~6 more months
  before they can be deleted. Until then, every
  `cmake --build` runs the deprecated-class warning suppressed
  in 3 TUs.
- Adding `BYPASS` to `*_complete.h` is a feature addition (not
  a refactor). It must come with a unit test and may need
  state-machine plumbing if the bypass path crosses the
  Hit/Miss boundary.

## 8. Supersedes

None. This ADR documents the next step in the chain:
- ADR-036 (deprecate) — done in commit `78fa178` (marking) + `1bc979c` (`[[deprecated]]` attributes on ch_bool `&&`/`||`, not cache stubs but same pattern)
- This ADR (decide removal strategy) — filed
- Future PR (implement removal) — gated on 2027-04-17 calendar deadline
