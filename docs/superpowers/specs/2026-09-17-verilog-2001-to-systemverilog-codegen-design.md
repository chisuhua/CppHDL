# Codegen Migration: Verilog 2001 → SystemVerilog 2017

> **Status**: Proposed
> **Date**: 2026-09-17
> **Author**: CppHDL orchestration
> **Scope**: `include/codegen_verilog.h`, `src/codegen_verilog.cpp`, `tests/test_verilog_gen.cpp`

## Why

The CppHDL codegen currently emits Verilog 2001 syntax (`always @(posedge clk)`,
`reg`, `wire`). SystemVerilog (IEEE 1800-2017) provides clearer intent-signaling
constructs (`always_ff`, `always_comb`, `always_latch`, `logic`) that Verilator
already supports. Migrating the codegen unlocks:

1. **Static intent enforcement**: `always_ff` prohibits non-blocking race patterns
   that `always @(posedge clk)` would silently allow (e.g. driving the same
   variable from two always blocks).
2. **Reduced keyword set**: `logic` replaces both `reg` and `wire` (single
   4-state variable type), shrinking the surface area designers must know.
3. **Verilator diagnostic richness**: Verilator emits sharper lint warnings on
   SV constructs (`UNOPTFLAT`, `LATCH` on `always_latch` violations).
4. **Future-proofing**: All modern tooling (Verilator ≥ 4.200, Synopsys VCS,
   Cadence Xcelium) defaults to SV; the 2001 path is increasingly legacy.

The user request explicitly notes that Verilator already accepts SV syntax.

## What Changes

### Decisions captured (4 clarifying rounds)

| Decision | Choice | Rationale |
|---|---|---|
| Scope | **Minimal set** (`always_ff` + `logic`) | User-selected: lowest risk, all other codegen (`assign`, mux, concat, bits_update) is already SV-compliant |
| Backward compat | **SV-only default**, no 2001 fallback | User rejected dual-path; no downstream consumer requires 2001 |
| Keyword replacement | `reg` + `wire` → `logic` | User-selected recommended; ports stay as bare `input`/`output` (implicit wire) |
| Filename | Keep `.v` extension | User-selected recommended; Verilator auto-detects SV inside `.v` |

### Architecture

Two new helper methods on `verilogwriter` (`emit_signal_decl`,
`emit_always_ff`) absorb the keyword choice so future SV extensions
(`always_comb`, reset handling) only need to modify the helpers, not every
emission site.

| File | Net change | Purpose |
|---|---|---|
| `include/codegen_verilog.h` | +2 private method decls | Helper interface |
| `src/codegen_verilog.cpp` | +30 lines impl, -4 inline literals | Helper bodies + 2 call-site swaps |
| `tests/test_verilog_gen.cpp` | -1 + 2 assertion lines + 2 new TEST_CASEs | Coverage |

### Behavior contract (before / after)

| Context | Before (Verilog 2001) | After (SystemVerilog 2017) |
|---|---|---|
| Register decl | `reg [3:0] reg_counter;` | `logic [3:0] reg_counter;` |
| Intermediate wire | `wire [7:0] sum_wire;` | `logic [7:0] sum_wire;` |
| Sequential block | `always @(posedge default_clock) begin ... end` | `always_ff @(posedge default_clock) begin ... end` |
| Ports | `input [3:0] a,` / `output [3:0] b,` | **Unchanged** (implicit wire, SV default) |
| `assign`/`mux`/`concat`/`bits_update` | `assign ... = ...;` | **Unchanged** (SV-compliant) |
| File extension | `.v` | **Unchanged** |
| Public API | `void toVerilog(const std::string&, context*)` | **Unchanged** |
| Reset handling | None (no reset code emitted) | **Unchanged** (out of scope) |

### Invariants preserved

- Public API signature unchanged.
- Behavior of `interpreter` and `JIT` backends unchanged (only generated text format
  changes).
- Verilator consumes the generated `.v` without backend changes (Verilator ≥ 4.200
  auto-detects SV syntax).
- All file/port naming, width emission, and name sanitization rules unchanged.

## Requirements

### R1 — `logic` keyword for storage declarations

**SHALL** replace every `reg [N:0] name;` and `wire [N:0] name;` emission with
`logic [N:0] name;` in generated Verilog. **MUST** be implemented via the
`emit_signal_decl()` helper so future extensions only touch one site.

### R2 — `always_ff` for sequential blocks

**SHALL** emit `always_ff @(posedge default_clock) begin ... end` in place of
`always @(posedge default_clock) begin ... end`. **MUST** be implemented via
the `emit_always_ff()` helper.

### R3 — Helper isolation

The two helpers **MUST** be private methods of `verilogwriter`. They **MUST
NOT** be called from outside the class. Their bodies **MUST** swallow all
exceptions with `try/catch(...)` consistent with the existing print methods
(this is the codebase convention to avoid static-destruction segfaults).

### R4 — Public API stability

The signature `void toVerilog(const std::string &filename, ch::core::context *ctx)`
**MUST NOT** change. Existing call sites in `tests/` and `examples/` continue to
compile and link without modification.

### R5 — Port declaration preservation

Port declarations (`input [N:0] a,` / `output [N:0] b,`) **SHALL NOT** be
modified. They remain bare to preserve SV default semantics (implicit wire)
and avoid disturbing Verilator's IO type inference.

### R6 — Behavior equivalence on interpreter / JIT paths

**SHALL NOT** alter `interpreter` or `JIT` codegen paths. Only `verilogwriter`
is modified. Confirmed by running the full `ctest -L base` suite with zero
non-pre-existing failures.

### R7 — Test coverage

The implementation **SHALL** include:

- (a) Update of `tests/test_verilog_gen.cpp:69` from `always @(posedge default_clock)`
      to `always_ff @(posedge default_clock)`.
- (b) New `TEST_CASE` `VerilogGen - UsesLogicKeyword` asserting no `reg [N:0]`
      or `wire [N:0]` literal in the generated module body (regex negative match).
- (c) New `TEST_CASE` `VerilogGen - AlwaysFFBlocks` asserting `always_ff` is
      emitted and bare `always @(` (excluding `always_ff @(`) is absent in
      the register-update section.

### R8 — Zero-debt exit

Per AGENTS.md `ZERO-DEBT POLICY`:

- `cmake --build build -j$(nproc)` exits 0 with **zero** new warnings on changed files.
- `ctest -L base --output-on-failure` all green (or explicit pre-existing-failure note).
- No dead code introduced (no leftover `reg`/`wire` literal emissions).
- AGENTS.md (root + include/AGENTS.md) updated with the SV migration note.

### R9 — Verilator backend compatibility

**SHALL NOT** require modifications to `include/core/verilator_backend.h` or
`src/core/verilator_backend.cpp`. Verilator auto-detects SV syntax in `.v`
files at the chosen version. If future regression testing reveals a parsing
issue, the fix **SHALL** be a single `--language 1800-2017` flag injection
in `verilator_backend.cpp` (not a structural redesign).

## Out of Scope

- `always_comb` / `always_latch`: current codegen emits no such blocks
  (combinational logic uses `assign`). Adding triggers is future work.
- Synchronous / asynchronous reset blocks: out of scope (decision: minimal set).
- `typedef` / `struct` / `package` / `interface`: out of scope (decision: minimal set).
- Renaming `toVerilog` to `toSystemVerilog`: out of scope (decision: API stability).
- 2001 fallback path: out of scope (decision: SV-only default).
- Changing output filename to `.sv`: out of scope (decision: keep `.v`).
- Modifying `codegen_dag.cpp`: out of scope (DAG path is unrelated).

## Components

### `verilogwriter::emit_signal_decl(lnodeimpl *node)`

```cpp
void verilogwriter::emit_signal_decl(std::ostream &out,
                                     ch::core::lnodeimpl *node) {
    if (!node_names_.count(node)) return;
    out << "    logic " << get_width_str(node->size())
        << " " << node_names_[node] << ";\n";
}
```

Replaces:
- `print_decl`: `wire [N:0] name;\n` literal at line ~382
- `print_reg`: `reg [N:0] name;\n` literal at line ~484

### `verilogwriter::emit_always_ff(reg_name, next_name)`

```cpp
void verilogwriter::emit_always_ff(std::ostream &out,
                                   const std::string &reg_name,
                                   const std::string &next_name) {
    if (reg_name.empty() || next_name.empty()) return;
    out << "    always_ff @(posedge default_clock) begin // "
        << "Register update for " << reg_name << "\n";
    out << "        " << reg_name << " <= " << next_name << ";\n";
    out << "    end\n";
}
```

Replaces:
- `print_reg`: `always @(posedge default_clock) begin ... end` literal at lines ~497-501

## Data Flow

```
verilogwriter::print(out)
    ↓
print_header(out)        ← unchanged (ports unchanged)
    ↓
print_body(out)
    ├── print_decl(out)
    │   └── for each wire-declared node:
    │       └── emit_signal_decl(out, node)   ← NEW helper
    │
    └── print_logic(out)
        └── for each register node:
            ├── emit_signal_decl(out, reg_node)        ← NEW helper (reg → logic)
            └── emit_always_ff(out, reg_name, next)    ← NEW helper
```

## Error Handling

- All helper bodies use `try/catch(...)` to swallow exceptions silently —
  consistent with every existing `print_*` method (AGENTS.md does not specify
  but the codebase convention avoids segfaults during static destruction).
- Helpers return early on missing names (mirrors current `print_reg` behavior
  of emitting a `// Warning: ...` comment when `next_node` is missing).
- Tests assert happy-path emission; failure modes are not formally tested
  (matches existing test philosophy — coverage of the public Verilog surface,
  not internal exception paths).

## Testing

### Unit tests (Catch2)

```cpp
// tests/test_verilog_gen.cpp — update existing
- REQUIRE(verilog_code.find("always @(posedge default_clock)") != npos);
+ REQUIRE(verilog_code.find("always_ff @(posedge default_clock)") != npos);

// tests/test_verilog_gen.cpp — add new
TEST_CASE("VerilogGen - UsesLogicKeyword", "[verilog][sv]") {
    auto ctx = std::make_unique<ch::core::context>("sv_logic_test");
    ch::core::ctx_swap guard(ctx.get());
    ch_reg<ch_uint<4>> reg(ch_uint<4>(0));
    reg->next = reg + ch_uint<4>(1);
    std::string code = generateVerilogToString(ctx.get());
    // Negative match: reg/wire declaration lines absent
    auto decl_has = [&](const char* kw) {
        return code.find(std::string("\n    ") + kw + " [") != std::string::npos
            || code.find(std::string("\n    ") + kw + " ") != std::string::npos;
    };
    REQUIRE_FALSE(decl_has("reg"));
    REQUIRE_FALSE(decl_has("wire"));
    REQUIRE(code.find("logic [3:0]") != std::string::npos);
}

TEST_CASE("VerilogGen - AlwaysFFBlocks", "[verilog][sv]") {
    auto ctx = std::make_unique<ch::core::context>("sv_alwaysff_test");
    ch::core::ctx_swap guard(ctx.get());
    ch_reg<ch_uint<4>> reg(ch_uint<4>(0));
    reg->next = reg + ch_uint<4>(1);
    std::string code = generateVerilogToString(ctx.get());
    REQUIRE(code.find("always_ff @(posedge default_clock)") != std::string::npos);
    // Negative: bare always @( without always_ff prefix absent
    auto pos = code.find("always @(");
    REQUIRE(pos == std::string::npos);
}
```

### Manual verification checklist

1. `cmake --build build -j$(nproc) 2>&1 | grep -E "error|warning"` → exit 0,
   zero new warnings on changed files.
2. `./build/tests/test_verilog_gen` → all green.
3. `ctest -L base --output-on-failure` → all green.
4. Sample `.v` in `tests/output/`:
   - `grep -E "^\s*reg\b" file.v` → no matches.
   - `grep -E "^\s*wire\b" file.v` → no matches.
   - `grep "always_ff" file.v` → at least one match.
   - `grep "always @(" file.v` (excluding `always_ff @(`) → no matches.
5. (If `BUILD_VERILATOR=ON`) `verilator --lint-only -Wall file.v` → exit 0.

## Risks & Mitigations

| Risk | Likelihood | Impact | Mitigation |
|---|---|---|---|
| `tests/output/*.v` golden files become stale | High (regenerated by tests) | Low | Tests call `toVerilog()` to regenerate; check that golden-vs-runtime comparison is **not** done in any test (golden is a regenerated artifact, not a checked-in fixture) |
| Verilator fails to parse SV syntax in `.v` extension | Very low (Verilator ≥ 4.x accepts both) | Medium | Add `--language 1800-2017` flag to verilator invocation if reported |
| Existing examples break (regenerated .v diff visible in git) | Certain (intentional) | Low | Examples regenerate at runtime; commit the new .v outputs as part of this change |
| Third-party downstream users depend on Verilog 2001 output | None in repo | Low | Decision recorded in R4; document in AGENTS.md |
| `logic` keyword collides with reserved namespace | None | Low | `logic` is an SV keyword, not a C++ identifier |

## Rollback

Revert the commit. No data migration, no schema change. Single-feature scope.

## Followups (Future)

1. Add `emit_always_comb` helper for future combinational-always emission
   (e.g. if/else statement DSL grows into always blocks).
2. Add synchronous reset using `regimpl::rst_` / `rst_val_` fields:
   ```cpp
   always_ff @(posedge default_clock) begin
       if (rst) reg <= rst_val;
       else     reg <= next;
   end
   ```
3. Consider renaming `toVerilog` → `toSystemVerilog` (breaking, needs migration
   notice — defer until consumer feedback signals appetite).