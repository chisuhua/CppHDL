# CppHDL Changelog

User-visible changes to the CppHDL HDL library. Entries are grouped by
the change that introduced them (see `openspec/changes/` and
`openspec/specs/`).

---

## 2026-09-22 — `verilator-perf-three-way`

### Fixed
- **`get_literal_str` masks literal value to target width.**
  `src/codegen_verilog.cpp::verilogwriter::get_literal_str` (single-arg
  overload, `src/codegen_verilog.cpp:142-166`) now masks `value` to
  `width` bits before emitting the SystemVerilog `<width>'h<hex>` literal.
  Previously, the function trusted `lit.bv_.size()` (the reported width)
  but emitted the raw `lit.bv_.words_[0]`, which `bitvector::operator=`
  does not mask (`include/bv/bv_assign.h:60-70`). Constructs like
  `ch_uint<8>(999)` therefore reported `width=8` while carrying 999, and
  the codegen emitted `8'h3e7` — a hard SystemVerilog syntax error
  (`Too many digits for N bit number`). Affected the three-way perf
  benchmark TC-07 (XOR chain) at `depth=1000`, where hundreds of
  intermediate `ch_uint<8>` literals produced 744 verilator errors and
  broke the Verilator compile. After the fix, TC-07 depth=1000 compiles
  cleanly and the Verilator row reports `34.54 ticks/sec` (was 0.00).
  `width >= 64` path is preserved (`ADR-035` wide-signal rejection
  caps port widths at 64; any wider literal would have been rejected
  upstream).

### Added
- **`perf_three_way` ctest entry** for CI-friendly three-way perf
  smoke tests. Runs TC-07 (XOR chain depth=10/100/1000) and TC-08
  (register chain) via `perf_tests --tc=07 --tc=08 --report=json` in a
  subprocess-isolated execution context (F2 perf subprocess isolation).
  TIMEOUT 360s, LABELS `perf;verilator`. The full `--all` perf suite
  remains available via the `perf_tests` ctest entry (TIMEOUT 1800).
  See `tests/benchmark/CMakeLists.txt:103-128`.

### Test coverage
- **`tests/test_verilator_three_way.cpp`** (391 lines, 12 test cases
  / 28 assertions, all PASS) covers:
  - Boundary values: `ch_uint<8>(1)` → `8'h1`, `ch_uint<8>(0)` → `8'h0`,
    `ch_uint<8>(255)` → `8'hff`, `ch_uint<8>(256)` → `8'h0` (mask drops
    high bit), `ch_uint<8>(999)` → `8'he7` (the regression case).
  - Multiple widths: 8, 16, 32, 64.
  - `print_concat` path (concatenation RHS literal).
  - `zext<32>` path (zero-extend output literal).
  - Random fuzz: 100 trials of random `(width, value)` pairs asserting
    no `'<width>'h<overflow-hex>` literal is emitted.
  - TC-07 end-to-end codegen for depth=10/100/1000.
  - Real `verilator --lint-only` integration (skipped if verilator not
    on PATH).

---

## 2026-09-21 — `adr-035-phase-3-complete`

See `openspec/specs/verilator-backend-e2e/spec.md` and
`openspec/specs/jit-llvm-compat-tests/spec.md` for the archived
specifications.

---

## Format

Entries are reverse-chronological. Each entry may have sections:
**Added**, **Changed**, **Deprecated**, **Removed**, **Fixed**,
**Security**. The opening line of each entry is the date and the
OpenSpec change name (or external release tag if no change is in
flight).
