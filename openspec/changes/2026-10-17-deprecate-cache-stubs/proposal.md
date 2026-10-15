# deprecate-cache-stubs

## Why

`include/cpu/cache/{i,d}_cache.h` are pass-through stubs that:

1. Export `chlib::{I,D}CacheConfig` struct + `ICache` / `DCache` class
2. Implement only `ready := req, miss := false, data := axi_data`
3. Co-exist with `include/cpu/cache/{i,d}_cache_complete.h` which
   export the same `chlib::{I,D}CacheConfig` types

This is a documented "已 KNOWN LIMITATIONS" debt in root AGENTS.md.
Three test files (`test_cache_simple.cpp`, `test_cache_pipeline_integration.cpp`,
`test_phase4_integration.cpp`) actively use the stubs as smoke tests
of the pass-through semantics. The real cache integration is in
`examples/riscv-mini/src/{i_cache_prefetch,d_cache_write_buffer}.h`
and uses `*_complete.h`.

Two issues:

1. **Co-include hazard** — including both `cache/i_cache.h` and
   `cache/i_cache_complete.h` in the same TU causes ODR clash
   because both define `chlib::ICacheConfig`. AGENTS.md warns about
   this but cannot enforce it.
2. **Search ambiguity** — `grep "cpu/cache/i_cache"` returns both
   the stub and the canonical file; downstream contributors cannot
   tell which one to use without reading internal design notes.

## What Changes

### A. Mark stub headers `[[deprecated]]` (immediate)

Add `[[deprecated("use i_cache_complete.h / d_cache_complete.h instead — see root AGENTS.md §KNOWN LIMITATIONS")]]` to the
class declarations in:

- `include/cpu/cache/i_cache.h` (class ICache)
- `include/cpu/cache/d_cache.h` (class DCache)

The deprecation message points users to `_complete` and to the
KNOWN LIMITATIONS section, without renaming the file (which would
be a separate atomic change).

### B. Wrap active test consumers with `#pragma`

The three smoke tests intentionally exercise the deprecated stub
(they would otherwise have nothing to test). Wrap each translation
unit in:

```cpp
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
// existing test body using stub
#pragma GCC diagnostic pop
```

This preserves the regression coverage of the stub until the
follow-up change that physically removes the stub (out of scope
here).

### C. Rename `i_cache.h` → `i_cache_stub.h`, `d_cache.h` → `d_cache_stub.h`

Once the deprecation is live and the three test files are
pragma-wrapped, rename the headers to make the intent unambiguous
at the include path itself:

- `include/cpu/cache/i_cache.h` → `include/cpu/cache/i_cache_stub.h`
- `include/cpu/cache/d_cache.h` → `include/cpu/cache/d_cache_stub.h`

Update:
- The 3 test files' `#include "cpu/cache/i_cache.h"` →
  `"cpu/cache/i_cache_stub.h"` (and the d_cache equivalent).
- Root AGENTS.md §KNOWN LIMITATIONS wording: drop "pass-through stub"
  style and reference the stub by name.
- `include/cpu/AGENTS.md` STRUCTURE / TESTS tables.
- `include/AGENTS.md` cache mentions if any.

The plain `cache/i_cache.h` and `cache/d_cache.h` paths will no
longer exist, eliminating the "which one do I include?" ambiguity.

## Out of Scope

- **Physical deletion of the stub** — separate change. Requires
  either rewriting the three test files against `*_complete` (if
  the complete variant supports the smoke assertions) or accepting
  the loss of the smoke-test-only signal. Tracked as a future
  change with its own ADR.
- **ODR guard** — adding `#error` to one of the two headers to
  crash co-included TUs is tempting but risks breaking downstream
  consumers we don't know about. Skip until F9 (physical delete)
  or until we have a way to detect non-test co-includes.
- **Cache semantic implementation** (real LRU / Write-Through) —
  already exists in `*_complete.h` and `*_prefetch.h`. Adding
  another layer is redundant.

## Acceptance

1. `git grep "deprecated" include/cpu/cache/` shows 2 hits (one per
   stub header).
2. `git ls-files include/cpu/cache/` lists exactly `i_cache.h`,
   `d_cache.h`, `i_cache_complete.h`, `d_cache_complete.h`,
   `i_cache_prefetch.h`, `d_cache_write_buffer.h` (no `_stub`
   suffix yet — only added after step C commits).
3. After commit C, `git ls-files include/cpu/cache/` lists
   `i_cache_stub.h` and `d_cache_stub.h` instead of the bare
   names.
4. `cmake --build build -j$(nproc)` succeeds with 0
   `-Wdeprecated-declarations` for our stub attributes (3 pragma-
   wrapped test files + any pragmas from previous changes).
5. `ctest -LE perf -j$(nproc)` → 148/148 PASS.
6. `./run_all_ported_tests.sh` → 28/28 PASS.
7. `openspec validate --specs --strict` → 12/12 PASS.
8. `bash scripts/verify_test_counts.sh` → match.

## Implementation Order

Atomic commits, each building on the previous:

1. Mark stub classes `[[deprecated]]` + add 3 test pragmas.
2. Rename `i_cache.h` → `i_cache_stub.h`, update includes.
3. Rename `d_cache.h` → `d_cache_stub.h`, update includes.
4. Sync doc files (root AGENTS.md, include/cpu/AGENTS.md).

"no spec changes" — the change affects existing API by adding a
deprecation message and renaming a header, both of which are
changes to existing semantics not new behavior. Per the
`fix-ns-pollution` precedent, archive with `--skip-specs`.
