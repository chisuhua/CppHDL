# tasks: deprecate-cache-stubs

Implementation: 4 atomic commits in this order (each buildable in isolation):

## Commit 1: Mark stub classes [[deprecated]]

- [ ] 1.1. Add `[[deprecated("use i_cache_complete.h / d_cache_complete.h instead — see root AGENTS.md §KNOWN LIMITATIONS")]]`
  to `class ICache` in `include/cpu/cache/i_cache.h`.
- [ ] 1.2. Same for `class DCache` in `include/cpu/cache/d_cache.h`.
- [ ] 1.3. Wrap each of the 3 smoke test files in
  `#pragma GCC diagnostic push / ignored "-Wdeprecated-declarations" / pop`:
    - tests/chlib/test_cache_simple.cpp
    - tests/chlib/test_cache_pipeline_integration.cpp
    - tests/chlib/test_phase4_integration.cpp
- [ ] 1.4. Verify: cmake --build build → 0 deprecation warnings for our attributes.

## Commit 2: Rename i_cache.h → i_cache_stub.h

- [ ] 2.1. `git mv include/cpu/cache/i_cache.h include/cpu/cache/i_cache_stub.h`.
- [ ] 2.2. Update include path in the 3 test files.
- [ ] 2.3. Verify: cmake --build build → 0 errors, 3 test binaries still pass.

## Commit 3: Rename d_cache.h → d_cache_stub.h

- [ ] 3.1. `git mv include/cpu/cache/d_cache.h include/cpu/cache/d_cache_stub.h`.
- [ ] 3.2. Update include path in the 3 test files.
- [ ] 3.3. Verify: same as 2.3.

## Commit 4: Doc sync

- [ ] 4.1. Root AGENTS.md §KNOWN LIMITATIONS: replace "Cache stubs i_cache.h / d_cache.h" → "Cache stubs i_cache_stub.h / d_cache_stub.h".
- [ ] 4.2. include/cpu/AGENTS.md STRUCTURE: rename lines.
- [ ] 4.3. include/cpu/AGENTS.md KEY FILES "I-Cache" / "D-Cache" entries: confirm `_complete` is the canonical entry (no change needed but verify).

## Archive

- [ ] Z1. ctest -LE perf 148/148 + run_all_ported_tests 28/28.
- [ ] Z2. openspec validate --specs --strict 12/12.
- [ ] Z3. openspec archive 2026-10-17-deprecate-cache-stubs --skip-specs --yes.
