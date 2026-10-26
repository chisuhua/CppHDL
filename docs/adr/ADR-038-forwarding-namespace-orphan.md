# ADR-038: forwarding.h namespace + location cleanup

- **Status**: Accepted (proposed 2026-10-17)
- **Date**: 2026-10-17
- **Deciders**: build-agent
- **Related**: include/cpu/AGENTS.md §ANTI-PATTERNS L48, §NAMESPACE MAP

---

## 1. Context

`include/cpu/forwarding.h` defines `chlib::ForwardingUnit` and
`chlib::ForwardingMux` for the 5-stage RV32I pipeline's
EX/MEM forwarding paths. The header was placed in `include/cpu/`
when CPU components were first added, before a project-wide
namespace convention was established.

`include/cpu/AGENTS.md` self-describes the orphan:

> ⚠️ **Note**: `forwarding.h` lives in `include/cpu/` but uses
> `chlib` namespace — likely should be in `include/chlib/`.

The same anti-pattern section further observes:

> **Duplicate forwarding**: `forwarding.h` (chlib) vs `hazard_unit.h`
> (riscv) — overlapping functionality.

A 2026-10-17 audit (in preparation for this ADR) confirms the
state of CPU forwarding infrastructure:

| Header | namespace | size | active callers |
|--------|-----------|------|----------------|
| `include/cpu/forwarding.h` | `chlib` | 347 lines | 1 (`tests/chlib/test_forwarding.cpp`) |
| `include/cpu/pipeline/hazard_unit.h` | `riscv` | 178 lines | ~9 (`examples/riscv-mini/src/rv32i_soc.h` + 8 test_*.cpp) |
| `include/cpu/branch_predictor/...` | — | (missing dir) | 0 |
| `include/cpu/hazard/...` | — | (missing dir) | 0 |
| `examples/riscv-mini/src/rv32i_forwarding.h` | (own ns) | — | 1 (`rv32i_core.h`, `tests/test_rv32i_pipeline.cpp`) |

> ⚠️ **2026-10-24 更新（ADR-046 `rv32i-canonical-home`）**：本表中的路径已随 C2 收敛而
> 变更 —— `examples/riscv-mini/src/rv32i_forwarding.h` → **`include/cpu/pipeline/rv32i_forwarding.h`**
> （namespace 仍是 `riscv`，ADR-046 D8）；`rv32i_core.h` → **`include/cpu/pipeline/hazard_detection_unit.h`**（改名，ADR-046 D8）。
> ⇒ §2.2「example 下的组件**无 canonical home**」的表述**已被 ADR-046 D6 修正**：
> 它们现在**有** canonical home（`include/cpu/`），只是当时尚未迁移。
>
> ⚠️ **2026-10-27 更新（ADR-047 `rv32i-data-forwarding`）**：流水线**侧**的前推接法
> 已由 ADR-047 裁决（**接**；mux = 既有 `chlib::ForwardingMux`，零新增组件）。
> ⇒ 本 ADR §2.3 的「两个 `ForwardingUnit` 合并留未来 change」**仍然有效**
> （ADR-047 **不动** `riscv::ForwardingUnit`）。
> ⚠️ **但必须点名警告**：`riscv::ForwardingUnit` 的 `ex_alu_result` 输入语义就是
> 「**活的 EX 级组合输出**」⇒ **拿它直接接进流水线装配会形成组合环**
> （= ADR-046 §1.3 的事故形态）⇒ 它是**环饵**，不得接入流水线。

So **forwarding.h has exactly one consumer** (a smoke test) and
is functionally distinct from `riscv::HazardUnit` (which is the
real production forwarding/hazard logic in `examples/riscv-mini`).

The `branch_predictor/` and `hazard/` subdirectories referenced
in `include/cpu/AGENTS.md` STRUCTURE (L13/L15) and NAMESPACE MAP
(L43) do not exist on disk. Their only on-disk analogues are in
`examples/riscv-mini/src/` and are not part of the main include
graph.

## 2. Decision

Two changes, recorded as a single ADR so the namespace
restructuring and the missing-directory clarification stay
together:

### 2.1 Move `include/cpu/forwarding.h` → `include/chlib/forwarding.h`

Pure path rename. The header uses `namespace chlib` already; the
file should sit alongside other `namespace chlib` components
(stream, fifo, state_machine, etc.). Active callers updated:

  - `tests/chlib/test_forwarding.cpp:2` — update include path.

The CMake target `cpphdl` already exposes `include/` as
`$<BUILD_INTERFACE>`, so no CMake change is needed for the move.
Other targets that consume forwarding.h transitively (none today)
need no update either, since `include/chlib.h` is the chlib
aggregator and `tests/test_forwarding` already includes it via
`chlib.h`. (Verified by audit: forwarding.h is *not* referenced
from any aggregator or transitive include path.)

### 2.2 Document the missing `branch_predictor/` and `hazard/` subdirs

The strings `branch_predictor/` and `hazard/` in
`include/cpu/AGENTS.md` STRUCTURE (L13/L15) and NAMESPACE MAP
(L43) reference directories that do not exist. The real
counterparts are in `examples/riscv-mini/src/`, which the
`include/cpu/AGENTS.md` TESTS section already points at
correctly (L72-75).

Remove the L13/L15 STRUCTURE tree entries for the missing
subdirs; correct the L43 NAMESPACE MAP `cpu` namespace row to
reflect actual files (the `branch_predictor/*.h`, `hazard/*.h`
text is replaced with a note that these namespaces have no
canonical home in the `include/` tree and live in
`examples/riscv-mini/src/`).

### 2.3 Don't merge `forwarding.h` and `riscv::HazardUnit`

The "Duplicate forwarding" anti-pattern (§ANTI-PATTERNS L48)
correctly observes that `chlib::ForwardingUnit` and
`riscv::HazardUnit` overlap. Merging them would be a much
larger change and is out of scope here:

  - `chlib::ForwardingUnit` is a ch_bool control path
    (ForwardSource enum + 3-way mux).
  - `riscv::HazardUnit` is a port-based component with
    stateful forwarding decisions.

  Merging would require redesigning one or both around a
  common core, which depends on usage patterns we do not yet
  have. Tracked as a separate change after this namespace
  cleanup lands.

## 3. Implementation Order

Atomic commits, each buildable in isolation:

1. **Move forwarding.h** — `git mv include/cpu/forwarding.h
   include/chlib/forwarding.h`, then update the one active
   test's include path.
2. **Clean AGENTS.md** — remove the L13/L15 STRUCTURE entries
   for the missing `branch_predictor/` and `hazard/` subdirs,
   correct the L43 NAMESPACE MAP row, and remove the residual
   "⚠️ Note" anti-pattern on L48 since the orphan claim is
   resolved by commit 1.

No public API change. The `chlib::ForwardingUnit` /
`chlib::ForwardingMux` symbols retain their namespace and
signatures; only the on-disk path moves. Per the
`fix-ns-pollution` precedent the change uses `--skip-specs`
at archive time.

## 4. Status

As of 2026-10-17, **accepted**. Implementation follows in this
order. Files affected: 2 (the header move + AGENTS.md fix).

## 5. Future Work (Out of Scope)

- **Merge chlib::ForwardingUnit + riscv::HazardUnit** — see §2.3.
  Recorded as a future architectural change, not part of this ADR.
- **Move examples/riscv-mini/src/{hazard,branch}_unit*.h into a
  shared include/cpu/riscv/ namespace** — depends on consumer
  migration and would change file paths visible to the
  `examples/riscv-mini/CMakeLists.txt` build that compiles against
  them. Skip until that subdirectory stabilizes.

## 6. Consequences

Positive:

- `cpu::ForwardingUnit` orphan disappears; `namespace chlib` now
  consistently means "file lives under include/chlib/".
- The misleading `branch_predictor/` and `hazard/` STRUCTURE tree
  lines are removed; future contributors no longer try to navigate
  to a directory that does not exist.
- `include/cpu/AGENTS.md` NAMESPACE MAP becomes a one-glance
  reference for what's actually on disk.

Negative:

- One test file's include directive changes (`tests/chlib/test_forwarding.cpp`).
  Any downstream consumer of `cpu/forwarding.h` (none found in
  this checkout) needs a coordinated include-path update.

## 7. Supersedes

None. This ADR resolves the orphan claim noted in
`include/cpu/AGENTS.md` §ANTI-PATTERNS L48 and the missing-
directory notes in §STRUCTURE L13/L15.
