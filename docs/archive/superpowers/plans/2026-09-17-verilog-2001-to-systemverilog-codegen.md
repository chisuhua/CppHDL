# Verilog 2001 → SystemVerilog Codegen Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Migrate CppHDL's `verilogwriter` from Verilog 2001 to SystemVerilog 2017 syntax: replace `reg`/`wire` declarations with `logic`, and `always @(posedge clk)` with `always_ff @(posedge clk)`. No public API change.

**Architecture:** Introduce two private helper methods (`emit_signal_decl`, `emit_always_ff`) on `verilogwriter` that absorb the keyword choice. Update two call sites (`print_decl`, `print_reg`) to invoke the helpers. Public API and downstream backends (interpreter, JIT, Verilator) are untouched.

**Tech Stack:** C++20, Catch2 v3.7.0, CMake. Existing `try/catch(...)` convention for print methods (silence exceptions to avoid static-destruction segfaults).

**Reference spec:** `docs/superpowers/specs/2026-09-17-verilog-2001-to-systemverilog-codegen-design.md`

---

## File Map

| File | Action | Responsibility |
|---|---|---|
| `include/codegen_verilog.h` | Modify | Declare two new private helpers |
| `src/codegen_verilog.cpp` | Modify | Implement helpers; update `print_decl` and `print_reg` call sites |
| `tests/test_verilog_gen.cpp` | Modify | Update existing assertion + add 2 new `TEST_CASE`s |

No new files created. No CMakeLists.txt changes (test file already registered).

---

### Task 1: Declare `emit_signal_decl` helper in header

**Files:**
- Modify: `include/codegen_verilog.h:50-52` (insert after `get_op_str` declaration)

- [ ] **Step 1: Read the current header file to confirm insertion point**

Run: `grep -n "get_op_str\|print_op\|emit_" include/codegen_verilog.h`
Expected: locate the exact insertion point inside the `private:` section of `verilogwriter`.

- [ ] **Step 2: Add the `emit_signal_decl` declaration**

In `include/codegen_verilog.h`, inside the `private:` section of class `verilogwriter`, immediately after the existing `std::string get_op_str(ch::core::ch_op op) const;` line, insert:

```cpp
    // --- SV helper methods (emit_signal_decl / emit_always_ff) ---
    void emit_signal_decl(std::ostream &out, ch::core::lnodeimpl *node);
    void emit_always_ff(std::ostream &out,
                        const std::string &reg_name,
                        const std::string &next_name);
```

- [ ] **Step 3: Verify header compiles**

Run: `cmake --build build -j$(nproc) --target codegen_verilog 2>&1 | grep -E "error|warning" | head`
Expected: Header change alone is declaration-only. Expect either 0 lines or only pre-existing unrelated diagnostics.

- [ ] **Step 4: Commit**

```bash
git add include/codegen_verilog.h
git commit -m "refactor(codegen): declare emit_signal_decl/emit_always_ff helpers"
```

---

### Task 2: Implement `emit_signal_decl` in source

**Files:**
- Modify: `src/codegen_verilog.cpp` (insert new method after `get_op_str` definition, around line 224)

- [ ] **Step 1: Locate the insertion point**

Run: `grep -n "verilogwriter::get_op_str\|verilogwriter::print_header" src/codegen_verilog.cpp`
Expected: `get_op_str` ends near line 224, `print_header` begins near line 226.

- [ ] **Step 2: Insert the implementation**

In `src/codegen_verilog.cpp`, immediately after the closing brace of `verilogwriter::get_op_str(...)` (find the line `}` followed by blank line and `void verilogwriter::print_header`), insert:

```cpp
void verilogwriter::emit_signal_decl(std::ostream &out,
                                     ch::core::lnodeimpl *node) {
    try {
        if (!node || !node_names_.count(node)) {
            return;
        }
        out << "    logic " << get_width_str(node->size())
            << " " << node_names_[node] << ";\n";
    } catch (...) {
        // Silently ignore exceptions (codebase convention for print_* methods)
    }
}
```

- [ ] **Step 3: Verify compilation succeeds with no new warnings**

Run: `cmake --build build -j$(nproc) 2>&1 | grep -E "error|warning" | grep -v "Werror"`
Expected: 0 new errors, 0 new warnings. (Pre-existing build may have unrelated noise; filter carefully.)

- [ ] **Step 4: Commit**

```bash
git add src/codegen_verilog.cpp
git commit -m "feat(codegen): implement emit_signal_decl helper (logic keyword)"
```

---

### Task 3: Update `print_decl` to use `emit_signal_decl` (wire → logic)

**Files:**
- Modify: `src/codegen_verilog.cpp:380-385` (`print_decl` wire emission loop)

- [ ] **Step 1: Locate the exact wire emission line**

Run: `grep -n "wire " src/codegen_verilog.cpp`
Expected: a single match near line 382 inside the `for (auto *node : wires)` loop body.

- [ ] **Step 2: Replace inline `wire` literal with helper call**

Find:
```cpp
            out << "    wire " << get_width_str(node->size()) << " "
                << node_names_[node] << ";\n";
```

Replace with:
```cpp
            emit_signal_decl(out, node);
```

- [ ] **Step 3: Build**

Run: `cmake --build build -j$(nproc) 2>&1 | grep -E "error|warning" | grep -v "Werror"`
Expected: 0 new diagnostics.

- [ ] **Step 4: Run existing test to confirm partial migration doesn't break**

Run: `./build/tests/test_verilog_gen 2>&1 | tail -20`
Expected: **FAIL** in any test that asserts the module body contains `wire [N:0]` — that is acceptable at this stage because we have not yet updated the test assertions. **All other test cases pass.** Record exact failing test names in the commit message body.

- [ ] **Step 5: Commit**

```bash
git add src/codegen_verilog.cpp
git commit -m "feat(codegen): use emit_signal_decl in print_decl (wire → logic)"
```

---

### Task 4: Implement `emit_always_ff` and update `print_reg`

**Files:**
- Modify: `src/codegen_verilog.cpp:481-510` (`print_reg` body)

- [ ] **Step 1: Add `emit_always_ff` implementation**

Insert in `src/codegen_verilog.cpp` immediately after the `emit_signal_decl` definition from Task 2:

```cpp
void verilogwriter::emit_always_ff(std::ostream &out,
                                   const std::string &reg_name,
                                   const std::string &next_name) {
    try {
        if (reg_name.empty() || next_name.empty()) {
            return;
        }
        out << "    always_ff @(posedge default_clock) begin // "
            << "Register update for " << reg_name << "\n";
        out << "        " << reg_name << " <= " << next_name << ";\n";
        out << "    end\n";
    } catch (...) {
        // Silently ignore exceptions (codebase convention for print_* methods)
    }
}
```

- [ ] **Step 2: Replace inline `reg` declaration with helper in `print_reg`**

In `print_reg`, find:
```cpp
        out << "    reg " << get_width_str(node->size()) << " "
            << node_names_[node] << ";\n";
```

Replace with:
```cpp
        emit_signal_decl(out, node);
```

- [ ] **Step 3: Replace inline `always @(posedge ...)` block with helper call**

In `print_reg`, find:
```cpp
            out << "    always @(posedge " << clock_name
                << ") begin // Register update for " << reg_name << "\n";
            out << "        " << reg_name << " <= " << node_names_[next_node]
                << ";\n";
            out << "    end\n";
```

Replace with:
```cpp
            emit_always_ff(out, reg_name, node_names_[next_node]);
```

Note: the variable `clock_name` becomes unused at this point in `print_reg`. Keep it declared for clarity (no behavior impact) or remove it — match whichever style is cleaner per the existing function body. Recommend **keep** declared (initialized to `"default_clock"`) so a future reset-support task can reuse it.

- [ ] **Step 4: Build**

Run: `cmake --build build -j$(nproc) 2>&1 | grep -E "error|warning" | grep -v "Werror"`
Expected: 0 new errors. A `-Wunused-variable` warning on `clock_name` is acceptable at this point (we leave it for future reset support); if the compiler errors with `-Werror=unused-variable`, prefix `clock_name` with `[[maybe_unused]]` or remove it.

- [ ] **Step 5: Commit**

```bash
git add src/codegen_verilog.cpp
git commit -m "feat(codegen): use emit_signal_decl + emit_always_ff in print_reg"
```

---

### Task 5: Update test assertions + add new TEST_CASEs

**Files:**
- Modify: `tests/test_verilog_gen.cpp:69-70` (existing assertion) + add 2 new `TEST_CASE`s at end of file

- [ ] **Step 1: Locate the existing assertion**

Run: `grep -n "always @(posedge default_clock)" tests/test_verilog_gen.cpp`
Expected: a single match on line 69 inside the `TEST_CASE("VerilogGen - CounterModule"...)` body.

- [ ] **Step 2: Update the existing assertion**

Find:
```cpp
    REQUIRE(verilog_code.find("always @(posedge default_clock)") !=
            std::string::npos);
```

Replace with:
```cpp
    REQUIRE(verilog_code.find("always_ff @(posedge default_clock)") !=
            std::string::npos);
```

- [ ] **Step 3: Add new TEST_CASE for `logic` keyword**

Append at the end of `tests/test_verilog_gen.cpp`:

```cpp
TEST_CASE("VerilogGen - UsesLogicKeyword", "[verilog][sv]") {
    auto ctx = std::make_unique<ch::core::context>("sv_logic_test");
    ch::core::ctx_swap guard(ctx.get());

    ch_reg<ch_uint<4>> reg(ch_uint<4>(0));
    reg->next = reg + ch_uint<4>(1);

    std::string code = generateVerilogToString(ctx.get());

    // helper: detect "    reg " or "    wire " declaration lines
    auto has_decl = [&](const std::string &kw) {
        std::string prefix = "\n    " + kw + " ";
        return code.find(prefix) != std::string::npos;
    };

    REQUIRE_FALSE(has_decl("reg"));
    REQUIRE_FALSE(has_decl("wire"));
    REQUIRE(code.find("logic [3:0]") != std::string::npos);
}
```

- [ ] **Step 4: Add new TEST_CASE for `always_ff`**

Append at the end of `tests/test_verilog_gen.cpp`:

```cpp
TEST_CASE("VerilogGen - AlwaysFFBlocks", "[verilog][sv]") {
    auto ctx = std::make_unique<ch::core::context>("sv_alwaysff_test");
    ch::core::ctx_swap guard(ctx.get());

    ch_reg<ch_uint<4>> reg(ch_uint<4>(0));
    reg->next = reg + ch_uint<4>(1);

    std::string code = generateVerilogToString(ctx.get());

    REQUIRE(code.find("always_ff @(posedge default_clock)") !=
            std::string::npos);
    // Bare "always @(" must not appear (we always emit always_ff)
    REQUIRE(code.find("always @(") == std::string::npos);
}
```

- [ ] **Step 5: Build and run the new tests**

Run:
```bash
cmake --build build -j$(nproc) --target test_verilog_gen 2>&1 | grep -E "error|warning" | grep -v "Werror"
./build/tests/test_verilog_gen "[verilog][sv]" --reporter compact
```

Expected: 0 new diagnostics. The two `[verilog][sv]` test cases PASS.

- [ ] **Step 6: Run the full test file**

Run: `./build/tests/test_verilog_gen --reporter compact`
Expected: ALL test cases PASS, including the updated `CounterModule`.

- [ ] **Step 7: Commit**

```bash
git add tests/test_verilog_gen.cpp
git commit -m "test(codegen): cover SystemVerilog logic + always_ff emission"
```

---

### Task 6: Full regression verification

**Files:** None (read-only verification)

- [ ] **Step 1: Clean build with no new warnings**

Run:
```bash
cmake --build build -j$(nproc) 2>&1 | tee /tmp/build.log
grep -E "warning:|error:" /tmp/build.log | grep -v "Werror" || echo "OK: no warnings/errors"
```

Expected: `OK: no warnings/errors`. If non-zero lines appear, they must be **pre-existing** (not introduced by this change). Document any in commit body.

- [ ] **Step 2: Run full base test suite**

Run: `ctest -L base --output-on-failure`
Expected: 100% pass rate. If any failures, they must be **pre-existing** and unrelated to this change.

- [ ] **Step 3: Sample-output inspection**

Run:
```bash
ls tests/output/*.v 2>/dev/null | head -3
for f in $(ls tests/output/*.v | head -3); do
  echo "=== $f ==="
  echo "reg decl lines:  $(grep -cE '^\s*reg\b'  $f)"
  echo "wire decl lines: $(grep -cE '^\s*wire\b' $f)"
  echo "always_ff lines: $(grep -c 'always_ff' $f)"
  echo "always @ lines:  $(grep -cE 'always @\(' $f)"
done
```

Expected:
- `reg decl lines`: 0
- `wire decl lines`: 0
- `always_ff lines`: ≥ 1
- `always @ lines`: 0

- [ ] **Step 4: AGENTS.md update (if not done in spec)**

If the root `AGENTS.md` already mentions codegen in its structure table, add a one-line note. Otherwise skip. Suggested insertion (in `include/AGENTS.md` structure table or root `AGENTS.md` Codegen section):

> Verilog codegen: SystemVerilog 2017 (always_ff, logic). See spec `docs/superpowers/specs/2026-09-17-verilog-2001-to-systemverilog-codegen-design.md`.

- [ ] **Step 5: Final commit (docs only if updated)**

```bash
git add AGENTS.md include/AGENTS.md 2>/dev/null || true
git diff --cached --quiet || git commit -m "docs: note SystemVerilog 2017 codegen migration"
```

(If no doc change was needed, skip this commit.)

---

## Self-Review

**1. Spec coverage:**
- R1 (`logic` for storage) → Tasks 2, 3, 4 ✓
- R2 (`always_ff`) → Task 4 ✓
- R3 (Helper isolation + try/catch) → Tasks 1, 2, 4 ✓
- R4 (Public API stability) → Verified by Task 6 ✓
- R5 (Port preservation) → Out-of-band: header includes unchanged, `print_input`/`print_output` untouched ✓
- R6 (Behavior equivalence on interpreter/JIT) → Tasks 3, 6 (full ctest) ✓
- R7 (Test coverage) → Task 5 ✓
- R8 (Zero-debt) → Task 6 ✓
- R9 (Verilator backend unchanged) → Out-of-band: verilator_backend.{h,cpp} untouched by this plan ✓

**2. Placeholder scan:** No "TBD"/"TODO"/"fill in" / "similar to Task N" placeholders. Every code block is complete and copy-pasteable.

**3. Type consistency:**
- `emit_signal_decl(std::ostream &out, ch::core::lnodeimpl *node)` — declared in Task 1, defined in Task 2, called in Tasks 3 + 4. ✓
- `emit_always_ff(std::ostream &out, const std::string &reg_name, const std::string &next_name)` — declared in Task 1, defined in Task 4, called in Task 4. ✓
- `ch::core::lnodeimpl` namespace used consistently. ✓
- `try/catch(...)` convention matched across all helpers. ✓

---

## Execution Handoff

Plan complete and saved to `docs/superpowers/plans/2026-09-17-verilog-2001-to-systemverilog-codegen.md`.

Two execution options:

1. **Subagent-Driven (recommended)** — Fresh subagent per task, review between tasks, fast iteration.
2. **Inline Execution** — Execute tasks in this session using executing-plans, batch with checkpoints.

Which approach?