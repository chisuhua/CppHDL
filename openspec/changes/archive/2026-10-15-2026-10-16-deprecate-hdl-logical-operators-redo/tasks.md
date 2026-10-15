# tasks: deprecate-hdl-logical-operators

This change was attempted in the 2026-10-15 session, the 8
`[[deprecated]]` attributes were added, the resulting 1128 warning
instances across 45 unique files were observed, and the attributes
were rolled back to keep the working tree quiet. The full plan
below is preserved for a future session that can absorb the
mechanical sed-rewrite pass.

## 0. Preparation (commit 0: read-only audit)

- [ ] 0.1. `grep -nE '[a-zA-Z_]\b\s*&&|&&.*[a-zA-Z_]' include/axi4/ examples/ src/ include/cpu/ samples/ -r` — capture the authoritative list of migration sites.

  **2026-10-16 redo audit established 20 files** (down from the 45 in the
  rolled-back attempt; many of the latter were docs/comment/`&&` on
  pointers to bool which do not call `operator&&`):

  include/axi4/axi4_full.h : 34 sites
  include/axi4/peripherals/axi_i2c.h : 39 sites
  include/axi4/peripherals/axi_dma.h : 5 sites
  include/axi4/axi_to_axilite.h : 1 site
  include/axi4/axi4_lite_matrix.h : 1 site
  examples/spinalhdl-ported/quadrature_encoder/quadrature_encoder.cpp : 88
  examples/spinalhdl-ported/i2c/i2c_controller.cpp : 36
  examples/spinalhdl-ported/ws2812/ws2812_controller.cpp : 16
  examples/spinalhdl-ported/spi/spi_controller.cpp : 12
  examples/spinalhdl-ported/uart_rx/uart_rx.cpp : 6
  examples/spinalhdl-ported/vga/vga_controller.cpp : 4
  examples/spinalhdl-ported/timer/timer.cpp : 1
  examples/riscv-mini/src/rv32i_regfile.h : 31 sites (counted once)
  examples/riscv-mini/src/rv32i_alu.h : 6 sites
  examples/riscv-mini/src/rv32i_tcm.h : 1 site
  examples/axi4/phase3a_complete.cpp : 1
  samples/pod_bundle_conversion.cpp : 1

- [ ] 0.2. `grep -nE '\bch_uint\b.*&&|&&.*ch_uint\b' include/ src/ examples/ tests/ samples/` — confirm zero matches (the 4 mixed overloads are not used in current code; if any appear, they need hand migration, not sed).
- [ ] 0.3. `git grep -nE 'logical_and|logical_or' tests/ src/ examples/` — confirm zero matches (no test asserts on the debug-name string; safe to change).

## Implementation order (commit 1+): MIGRATION FIRST

> **Inverted from the original plan.** The 2026-10-15 attempt landed
> attributes first and discovered only later that ~20 sites had not
> been migrated, producing 315 deprecation warnings. This redo lands
> migration first as a single multi-directory sweep (commits 1-N below),
> verifies zero `[[deprecated]]` warnings using a temporarily injected
> `__attribute__((deprecated))`, then *finally* adds the official C++
> `[[deprecated("...")]]` attributes in the last commit.

- [ ] 1.1. Apply ad-hoc `__attribute__((deprecated))` to the 8 overloads in `include/core/io_logical.h` and `include/core/operators_logic.h` (no `[[deprecated("...")]]` strings yet — we just need the warning to fire).
- [ ] 1.2. `cmake --build build -j$(nproc) > /tmp/opencode/deprecation_audit.log 2>&1 && grep "is deprecated" /tmp/opencode/deprecation_audit.log | grep -E "operator&&|operator\|\|" | wc -l` — establishes baseline count.

## 2. Migration by directory (commits 2-N: each must be self-contained)

> The 5 commits below each **must** bring the deprecation-warning count
> to zero when applied on top of the previous. Run the audit between
> commits; do not proceed if the count has *increased*.

- [ ] 2.1. `include/axi4/axi4_full.h` — sed `&&`→`&` and `||`→`|` (~34 sites)
- [ ] 2.2. `include/axi4/peripherals/axi_i2c.h` (~39 sites)
- [ ] 2.3. `include/axi4/peripherals/axi_dma.h` + `axi_to_axilite.h` + `axi4_lite_matrix.h` (~7 sites combined)
- [ ] 2.4. `examples/spinalhdl-ported/{quadrature_encoder,i2c,ws2812,spi,uart_rx,vga,timer}/` (~163 sites across 7 files)
- [ ] 2.5. `examples/riscv-mini/src/{rv32i_regfile,rv32i_alu,rv32i_tcm}.h` + `examples/axi4/phase3a_complete.cpp` + `samples/pod_bundle_conversion.cpp` (~40 sites)

### 3. Final attribute additions (commit N+1: replaces the ad-hoc __attribute__ with the public [[deprecated("...")]])

- [ ] 3.1. Replace ad-hoc `__attribute__((deprecated))` with `[[deprecated("use operator& for 1-bit AND; select() is for value muxing")]` etc. on `include/core/io_logical.h` and `include/core/operators_logic.h`.
- [ ] 3.2. Verify (final):
  - `cmake --build build -j$(nproc)` → 0 errors, 0 deprecation warnings for our 8 overloads (other unrelated `aligned_storage_t` warnings are pre-existing LLVM-internal).
  - `ctest -LE perf --output-on-failure -j$(nproc)` → 148/148 PASS.
  - `./run_all_ported_tests.sh` → 28/28 PASS.
  - `openspec validate --specs --strict` → 12/12 PASS.
- [ ] 3.4. `examples/spinalhdl-ported/` (7 files)
- [ ] 3.5. `examples/riscv-mini/src/` (5 files)
- [ ] 3.6. `examples/axi4/phase3a_complete.cpp` (port-level, 1 site)
- [ ] 3.7. `samples/` (2 files)
- [ ] 3.8. `tests/test_chipforge_mux_segv_repro.cpp` + `tests/test_comparison_ops.cpp` — wrap in `#pragma GCC diagnostic ignored "-Wdeprecated-declarations"` to preserve coverage of the still-existing (deprecated) API path.

## 4. Documentation sync (commit N+1)

- [ ] 4.1. `AGENTS.md:57,104` — change "use select() NOT &&" to "use `&`/`|` for 1-bit AND/OR; `select()` is for value muxing".
- [ ] 4.2. `include/axi4/AGENTS.md` — fix the anti-pattern entry that cites `axi_timer.h:77-81` (the cited lines will be different after migration).
- [ ] 4.3. `docs/adr/ADR-010-connection-semantics.md:146-169` — either add a "Superseded by: deprecate-hdl-logical-operators" note, or write a new ADR (per `docs/developer_guide/openspec-specs-guide.md`).

## 5. CI guard (commit N+2, optional but recommended)

- [ ] 5.1. Add `cmake --build ... 2>&1 | grep -c deprecated` assertion to `.github/workflows/ci.yml`, or scoped `add_compile_options(-Werror=deprecated-declarations)` to the `cpphdl` library target only.

## 6. Archive

- [ ] 6.1. `openspec validate deprecate-hdl-logical-operators --strict`
- [ ] 6.2. `openspec validate --specs --strict` (no spec changes; this is code-only)
- [ ] 6.3. `openspec archive deprecate-hdl-logical-operators` (no `--skip-specs`)

## 7. Regression after each commit

- [ ] 7.1. `cmake --build build -j$(nproc)` 0 errors
- [ ] 7.2. After all migrations: `cmake --build build -j$(nproc) 2>&1 | grep -c deprecated` returns 0 (or ≤ 8 if pragma-wrapped tests are exempted)
- [ ] 7.3. `ctest -E "^perf_"` 100% green
- [ ] 7.4. `ctest -L perf` 100% green (~22 min)
- [ ] 7.5. `./run_all_ported_tests.sh` 28/28 PASS
