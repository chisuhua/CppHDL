# design: deprecate-hdl-logical-operators

## Why the 2026-10-15 attempt was rolled back

The session's plan was to mark 8 `&&` / `||` overloads
`[[deprecated]]` and migrate all internal call sites in the same
commit. Oracle and metis review surfaced the right scale
(45 unique files, ~200-400 call sites), but the actual build
emitted **1128 deprecation warning instances** (not 200-400),
because `include/bundle/stream_bundle.h:44,47` defines `fire()` and
`isStall()` as inline template members of `ch_stream<T>`, and the
build expanded them into every TU that instantiates a `ch_stream`
(roughly every test file plus all `include/chlib/*.h`).

The session had already completed Phase 1 (trivial docs) and
Phase 2 (orphan-code cleanup) before reaching this change, and the
remaining session budget was insufficient for the mechanical sed
rewrite across 45 files plus a full perf-test regression cycle. To
avoid leaving the working tree in a self-warning state — which
would (a) violate the project's "no new warnings" gate, and (b)
hide the very warnings that this change is supposed to surface —
the attributes were rolled back and the proposal is preserved for
a follow-up session.

## Why `&` / `|` not `select()`

`ch_bool::operator&&` is `logical_and_op`, and
`include/lnode/operators_ext.h:164` defines
`logical_and_op::op_type = ch_op::and_` — the same `ch_op` enum
value used by `and_op` (the `operator&` policy). The DAG,
Verilog emission, JIT lowering, and trace signal are bit-identical
between `&&` and `&` for `ch_bool` operands; only the debug name
differs (`"logical_and"` vs `"and"`). Confirmed by
`git grep -nE 'logical_and|logical_or' tests/ src/ examples/` →
0 matches, so no test depends on the debug name.

By contrast, `select(cond, v1, v0)` lowers to a mux node, which
adds a `ch_op::mux` to the DAG, changes the generated Verilog
shape, and would force a full trace-parity re-check on every
design. The original AGENTS.md wording "use `select()` NOT `&&`"
is overly broad — it conflated two distinct things. This change
sharpens the wording.

## Why 4 mixed ch_bool / ch_uint<N> overloads are not safe to mechanically rewrite

`include/core/operators_logic.h:174-196` define 4 overloads of
`&&` / `||` between `ch_bool` and `ch_uint<N>`. They are
implemented as `lhs & (rhs != 0_d)`, i.e. **nonzero reduction**:
`true && 0b10 == 1` but `true & 0b10 == 0`. A future contributor
who runs `sed -i 's/&&/\&/g'` to clear deprecation warnings
would silently change the semantics for any caller using a
non-bool `ch_uint<N>` operand in a `&&`. The deprecation message
on these 4 overloads therefore explicitly names the nonzero test
in its diagnostic string, to prevent the mechanical rewrite.

The current tree has zero call sites for these 4 overloads
(verified by `git grep` and confirmed by the 1128 warning count
which is entirely from `ch_bool` and `port` overloads). The 4
overloads are kept for backwards compatibility but can be removed
in a later change without call-site migration.

## Why a pragma in 2 test files is the right move

`tests/test_chipforge_mux_segv_repro.cpp` and
`tests/test_comparison_ops.cpp` are **positive** tests of the
`&&` / `||` operators: they exist to catch SEGV regressions in
the operator implementations. After deprecation, the operators
are still present (deprecation is a soft signal, not removal),
so these tests still have coverage value. Wrapping them in
`#pragma GCC diagnostic ignored "-Wdeprecated-declarations"`
preserves the coverage without polluting the build log. The
pragma can be removed when the operators are physically deleted
in a future change.

## Commit ordering

The 6 commits in this change are ordered so that no commit
leaves the working tree in a state with **un-migrated internal
call sites** of the new deprecation. Commit 2 (the
`stream_bundle.h:44,47` migration) must land in the same commit
or before commit 1 (the attribute additions) lands, because
`stream_bundle.h` is a header that template-expands into every
TU. Otherwise every stream user TU carries the warning between
commit 1 and commit 2.

The remaining 3-N commits migrate by directory, in dependency
order: library internal headers first (`include/bundle`,
`include/chlib`, `include/axi4`, `include/cpu`), then downstream
users (`examples/`, `samples/`), then tests.
