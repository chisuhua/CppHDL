# CppHDL Changelog

User-visible changes to the CppHDL HDL library. Entries are grouped by
the change that introduced them (see `openspec/changes/` and
`openspec/specs/`).

---

## 2026-10-15 — `verilator-hier-input-flattening-fix`

### Fixed
- **`VerilatorBackend` now propagates driven submodule `ch_in` ports
  end-to-end.** `codegen_verilog.cpp::print_header` previously flattened
  driven submodule `ch_in`s (anything with `inputimpl::driver() != nullptr`,
  e.g. `sub.io().x <<= some_wire`) to top-level Verilog `input` ports that
  nothing drove. The matching `print_logic` switch had no `type_input`
  branch, so there was no `assign` to feed the driver value into the
  Vtop field. Verilator initialised those ports to X (=0 after the
  `initialize()` reset pulse), silently disabling every `ch_in` below
  the top component. After the fix:
  - `print_header` filters out driven inputs from the top-port list.
  - `print_decl` declares them as internal `logic` (via `emit_signal_decl`).
  - `print_logic` adds a `type_input` branch that walks the unnamed-proxy
    chain and emits `assign <in> = <driver>;` (mirroring the `type_output`
    branch).
  - `VerilatorBackend::emit_sim_main_postlude` skips them in
    `set_input_Vtop` / `get_field_ptr_Vtop` (the Vtop field does not
    exist), and `build_port_access_table` does not add them to
    `port_access_` so `sync_inputs_to_vtop` will not try to write them.
  JIT already handled this correctly (`src/jit/jit_ir_gen_lnode_io.cpp:31-46`).
  This brings the Verilog codegen path into parity with JIT semantics.

### Resolved (was a known limitation of `verilator-issue-25-fix`)
- The `verilator-issue-25-fix` entry below mentioned that
  "RVALID / RDATA readback via `data_map_` returns 0" and was tracked
  under issue #25 / option A in the Oracle audit. That symptom was
  actually the same hierarchical-input bug, not a separate issue #25
  register-propagation problem: in `test_axi_lite_verilator_e2e.cpp`,
  `slave.io().awaddr <<= io().awaddr` etc. flatten `slave.io().awaddr`
  to a top-level `input` port the testbench never drives, which then
  propagates through to `rvalid`/`rdata` via the `always_ff` busy
  latch. With the codegen fix, the AXI4-Lite WriteReadReg0ThroughBackend
  fixture now asserts `rvalid == 1 && rdata == 0xDEADBEEF` as a real
  REQUIRE, and the RV32I 3-instruction parity fixture runs
  `perf_instructions == 12` on both interpreter and verilator (was 0).

### AGENTS.md
- New VERILATOR BACKEND RULE: hierarchical input driver MUST be emitted
  by codegen. The four-site contract is now enforced (filter, declare,
  assign, backend skip). Reflects the actual root cause of the
  previously-tracked symptom.

---

## 2026-09-23 — `verilator-issue-25-fix`

### Fixed
- **`VerilatorBackend::eval_sequential` performs a 2-step clock toggle** so
  Verilator's `VlClockSig`/`__Vclklast__` edge detection fires
  `always_ff @(posedge clk)` blocks. The clock Vtop field is written
  directly via the new `clock_field_ptr_` (NOT round-tripped through
  `data_map_`); `sync_inputs_to_vtop(exclude_clock=true)` skips clock/reset
  on the input pass, and `sync_outputs_from_vtop()` skips clock/reset on
  the output pass (they are infrastructure, never user-visible). Before
  the fix, `always_ff` blocks never fired — counter-style regressions
  read 0 even though `eval()` was called, AXI4-Lite `awready`/`wready`/
  `bvalid` combinational handshakes did not propagate to `data_map_`, and
  sequential `rdata` reads returned 0. After the fix, the e2e tier
  (`test_verilator_e2e_harness`, `test_axi_lite_verilator_e2e`) passes with
  `REQUIRE` assertions, and TC-07 depth=1000 verilator runs at
  ~580 ticks/sec (was ~34 ticks/sec in the broken baseline). Also fixed
  `emit_sim_main_postlude` (`src/core/verilator_backend.cpp`) to use the
  codegen's `get_verilog_name(node)` instead of `cpp_safe_name(node->name())`
  — the codegen applies a `_N` uniqueness suffix that `lnode->name()`
  does not carry, and without the fix the generated accessor code had
  `&v->top.slave.unnamed_input` (with literal dots) which verilator
  rejected as a hard compile error.

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

## 2026-09-22 — `axi4lite-verilator-e2e` (Gap F2 closure)

### Added
- **`tests/test_axi_lite_verilator_e2e.cpp`** (268 lines, 2 test cases /
  9 assertions) closes Gap F2 from the `verilator-perf-three-way`
  review: `examples/axi4/axi4_lite_example.cpp` runs only through the
  interpreter — no VerilatorBackend e2e coverage existed for any AXI
  class. Two test cases:
  - `AxiLiteTopPortAccessSnapshot`: VerilatorBackend compiles an
    `AxiLiteTop` fixture (wraps `AxiLiteSlave<32,32,4>` via
    `ch::ch_module`), dlopens `libVtop.so`, and verifies
    `port_access_` is populated with all 17 AXI4-Lite IO signals
    (10 inputs + 7 outputs + clock + reset).
  - `WriteReadReg0ThroughBackend`: dispatches through Simulator +
    VerilatorBackend, performs a write transaction (AW + W →
    `reg0 = 0xDEADBEEF`) and a read transaction (AR → R), and logs
    `RVALID` / `RDATA` for nightly inspection.

### Known limitations carried forward (issue #25)
- ~~**`RVALID` / `RDATA` readback via `data_map_` returns 0 even though
  the generated Verilog has pure `assign top_slave_unnamed_output =
  top_slave_mux_select_1;` (combinational).** Originally Oracle
  audit (C12) suggested this would propagate; in practice it does
  not, indicating issue #25 extends beyond `always_ff` reg updates to
  the **entire `sync_outputs_from_vtop` path**. Strong evidence that
  the VerilatorBackend eval-after-input-sync sequence does not
  refresh wire outputs. Resolution tracked under
  `verilator-issue-25-fix` (proposed next change, option A in the
  Oracle audit).~~ **Resolved 2026-10-15 by
  `verilator-hier-input-flattening-fix`**: the symptom was actually the
  hierarchical-input flattening bug (driven submodule `ch_in` ports
  became top-level input ports that nothing drove). Once the codegen
  emits `assign` for those ports, `RVALID` / `RDATA` propagate normally
  through the existing `sync_outputs_from_vtop` path. No changes to
  the sync machinery were needed.
- ~~Test case 2 uses `INFO(...)` only for the readback values; no
  `CHECK` is asserted on `RVALID` / `RDATA` until the underlying
  Verilator eval-eval-sync issue is fixed.~~ **Resolved 2026-10-15**:
  `test_axi_lite_verilator_e2e.cpp::WriteReadReg0ThroughBackend` now
  asserts `REQUIRE(rvalid == 1)` + `REQUIRE(rdata == 0xDEADBEEF)`.

### Test invocation
- Tagged `[verilator][e2e][axi4lite][m5]`. PR matrix (no verilator
  binary) SKIPs both cases via the standard `tool_available("verilator")`
  gate. Nightly Verilator CI (`.github/workflows/ci.yml`) runs them
  for real.

### Files
- `tests/test_axi_lite_verilator_e2e.cpp` (new)
- `tests/CMakeLists.txt` (+5)

---

## Format

Entries are reverse-chronological. Each entry may have sections:
**Added**, **Changed**, **Deprecated**, **Removed**, **Fixed**,
**Security**. The opening line of each entry is the date and the
OpenSpec change name (or external release tag if no change is in
flight).
