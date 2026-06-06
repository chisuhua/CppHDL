# JIT CONCAT Lowering Implementation Plan

**Date**: 2026-06-02
**Status**: 📋 PLANNING
**Priority**: P1 (fixes `test_reg_timing` JIT sync bug, removes workaround)
**Target**: Wire up `JitOp::CONCAT` so that `concat()` becomes JIT-native

---

## 1. Background & Motivation

### 1.1 Current State (Bug Confirmed)

The JIT compiler has a half-implemented CONCAT path:

| Location | Status | Issue |
|----------|--------|-------|
| `include/jit/jit_ir.h:38` | ✅ `JitOp::CONCAT` enum exists | — |
| `src/jit/jit_compiler.cpp:1039-1053` | ⚠️ Lowering exists but has **reversed argument bug** | `(src1 << src0_width) \| src0` instead of `(src0 << src1_width) \| src1` |
| `src/jit/jit_compiler.cpp:377-382` | ❌ `ch_op::concat` mapped to `JitOp::CALL_EXTERNAL` | Never reaches CONCAT lowering |
| `src/jit/jit_compiler.cpp:450-454` | ❌ Binary op path doesn't set `src_bitwidth` for CONCAT | If wired, shift amount would be 0 |

### 1.2 Why This Matters

1. **`test_reg_timing` fails with JIT** when `concat()` is used on `io().out` because the interpreter executes concat in `data_map_`, but downstream JIT-native nodes read from `data_buffer_` which is out of sync. The 1-cycle lag is a **direct consequence of the hybrid execution gap**.

2. **The no-concat workaround works** (separate 1-bit output ports bypass the issue), but it's an architectural bandage. The **real fix is to make concat JIT-native**, eliminating the CALL_EXTERNAL gap entirely.

3. **The existing CONCAT lowering has been dead code** since `generate_ir()` was hardcoded to use CALL_EXTERNAL. It contains a latent bug that would manifest immediately if the wiring was fixed naïvely.

### 1.3 Reference: Interpreter Semantics

`include/ast/instr_op.h:416-447` — `Concat::eval`:

```cpp
// CONCAT (连接) - src0 (高位) + src1 (低位)
struct Concat {
  static void eval(sdata_type *dst, sdata_type *src0, sdata_type *src1) {
    // src1 → LOW bits
    for (i in [0, src1_width)) dst[i] = src1[i];
    // src0 → HIGH bits
    for (i in [0, src0_width)) dst[src1_width + i] = src0[i];
  }
};
```

**Result = (src0 << src1_width) | src1**, where `src0` is the first (left) argument and `src1` is the second (right) argument.

For `concat(a=3bit, b=5bit)` where `a=5` and `b=26`:
- result = (5 << 5) | 26 = 160 | 26 = **186** ✓
- Verified in `tests/test_operation_results.cpp:695-704`

---

## 2. The Bug in Existing CONCAT Lowering

`src/jit/jit_compiler.cpp:1039-1053`:

```cpp
case JitOp::CONCAT: {
  auto *src0_val = builder.CreateLoad(builder.getInt64Ty(), vregs[instr.src0], "load_concat_src0");
  auto *src1_val = builder.CreateLoad(builder.getInt64Ty(), vregs[instr.src1], "load_concat_src1");
  auto *shift_amt = builder.getInt64(instr.src_bitwidth);          // ← BUG: assumes src0_width
  auto *shifted   = builder.CreateShl(src1_val, shift_amt, "concat_shift");
  auto *res       = builder.CreateOr(shifted, src0_val, "concat_or"); // ← BUG: puts src0 in LOW bits
  if (instr.bitwidth < 64) { ... mask ... }
  builder.CreateStore(res, vregs[instr.dst], "store_concat");
  break;
}
```

**Two bugs**:
1. `shifted = src1 << shift_amt` — shifts the **wrong** operand (should shift src0)
2. `res = shifted | src0` — ORs with the wrong source (should OR with src1)

**Correct lowering**:
```cpp
auto *shift_amt = builder.getInt64(instr.src_bitwidth);   // = src1_width (the LOW operand width)
auto *shifted   = builder.CreateShl(src0_val, shift_amt, "concat_shift"); // src0 in HIGH bits
auto *res       = builder.CreateOr(shifted, src1_val, "concat_or");       // src1 in LOW bits
```

**Why this matters for `test_reg_timing`**:
- If we naively fix the wiring (map concat to JitOp::CONCAT) without fixing the lowering, the test would FAIL with the same symptoms as the CALL_EXTERNAL version, but with a different bug pattern.
- Both bugs must be fixed in the same commit.

---

## 3. Implementation Plan

### Phase 1: Fix `compile_to_llvm()` CONCAT Lowering

**File**: `src/jit/jit_compiler.cpp:1039-1053`

**Change**:

```cpp
case JitOp::CONCAT: {
  // Concat semantics (matches interpreter): src0 → HIGH bits, src1 → LOW bits
  // result = (src0 << src1_width) | src1
  auto *src0_val   = builder.CreateLoad(builder.getInt64Ty(), vregs[instr.src0],
                                        "load_concat_src0");
  auto *src1_val   = builder.CreateLoad(builder.getInt64Ty(), vregs[instr.src1],
                                        "load_concat_src1");
  auto *shift_amt  = builder.getInt64(instr.src_bitwidth);  // src1_width (LOW operand width)
  auto *shifted    = builder.CreateShl(src0_val, shift_amt, "concat_shift");
  auto *res        = builder.CreateOr(shifted, src1_val, "concat_or");
  if (instr.bitwidth < 64) {
    uint64_t bw_mask = (1ULL << instr.bitwidth) - 1;
    res = builder.CreateAnd(res, builder.getInt64(bw_mask), "mask_concat");
  }
  builder.CreateStore(res, vregs[instr.dst], "store_concat");
  break;
}
```

**Acceptance**: Manually verify against `Concat::eval` semantics for at least these cases:
- `concat(0b101, 0b11010)` (3-bit, 5-bit) → `0b10111010` (8-bit) = 186
- `concat(0b0, 0b1)` (1-bit, 1-bit) → `0b01` (2-bit) = 1
- `concat(0xff, 0x0)` (8-bit, 8-bit) → `0xff00` (16-bit) = 65280

### Phase 2: Wire up CONCAT in `generate_ir()`

**File**: `src/jit/jit_compiler.cpp`

**Step 2.1**: Change `ch_op::concat` to map to `JitOp::CONCAT` (line 377-382):

```cpp
case ch::core::ch_op::concat:
  jit_op = JitOp::CONCAT;     // ← changed from CALL_EXTERNAL
  break;
// remove concat from the multi-case CALL_EXTERNAL block (lines 377-381)
```

**Step 2.2**: Add CONCAT-specific IR building in `type_op` case (around line 450).

CONCAT needs special handling because it must set `src_bitwidth = src1_width` (the shift amount). Add a dedicated `else if` branch before the generic binary path:

```cpp
} else if (jit_op == JitOp::CONCAT) {
  // Concat: src0 in HIGH bits, src1 in LOW bits
  // src_bitwidth = src1_width (shift amount for src0)
  jit_instr = JitInstr(jit_op, result_bitwidth);
  jit_instr.dst  = dst_vreg;
  jit_instr.src0 = src0_vreg;
  jit_instr.src1 = src1_vreg;
  auto src1_bitwidth = (node->num_srcs() > 1 && node->src(1))
                          ? static_cast<BitWidth>(node->src(1)->size())
                          : BitWidth{0};
  jit_instr.src_bitwidth = src1_bitwidth;
  jit_instr.node_id = node_id;
  block_comb.instrs.push_back(jit_instr);
  block_comb.instrs.push_back(
      make_store(node_id, dst_vreg, result_bitwidth));
} else {
  // generic binary op path (existing)
  block_comb.instrs.push_back(make_binary(jit_op, dst_vreg, src0_vreg,
                                          src1_vreg, result_bitwidth));
  ...
}
```

**Acceptance**:
- For a node with `ch_op::concat` and `src1->size() == 5`, `instr.src_bitwidth == 5` (used as shift amount in lowering)
- For a node with `ch_op::concat` and `src0->size() == 3, src1->size() == 5`, result bitwidth is 8, shift amount is 5

### Phase 3: Remove CALL_EXTERNAL Fallback for Concat

**File**: `src/jit/jit_compiler.cpp:377-381`

The `ch_op::concat` case must be removed from the CALL_EXTERNAL block. After Phase 2.1, only `bits_update`, `mux`, and `assign` remain in the CALL_EXTERNAL group.

```cpp
case ch::core::ch_op::bits_update:
case ch::core::ch_op::mux:
case ch::core::ch_op::assign:
  jit_op = JitOp::CALL_EXTERNAL;
  break;
```

**Acceptance**: `concat` no longer appears in `external_node_ids_` (verify by adding a CHDBG print or checking via test).

### Phase 4: Add Tests

**File**: `tests/test_jit_concat.cpp` (new)

Test cases (each must pass with BOTH `jit_enabled=true` AND `jit_enabled=false`):

1. **Basic concat** — `concat(3bit, 5bit) = 8bit`
   - Input: a=5, b=26
   - Expected: 186 (matches existing test_operation_results.cpp)

2. **Width-asymmetric concat** — different widths
   - `concat(1bit, 7bit) = 8bit`
   - Input: a=1, b=0x55
   - Expected: 0xD5 (= (1<<7) | 0x55)

3. **Concat with literal** — `concat(lit, signal)`
   - `concat(2bit_lit=2, 4bit_sig=0xA) = 6bit`
   - Expected: 0x2A (= (2<<4) | 0xA)

4. **Concat in shift register** (the original failure case)
   - Reproduce `test_reg_timing` ShiftRegister (with concat) using the existing component
   - Tick 0–8, expect 0, 0, 1, 3, 7, 15
   - Must pass with JIT enabled

5. **Concat in nested bundles** (avoid false positives)
   - Use `ch_uint<4>` output from `concat(1bit, 1bit, 1bit, 1bit)` chained
   - Verify each tick's bit pattern matches interpreter

6. **Concat with 64-bit boundary**
   - `concat(32bit, 32bit) = 64bit`
   - Edge case: result == 0xFFFFFFFF_00000000

**Test pattern** (from existing `test_operation_results.cpp:685-705`):

```cpp
TEST_CASE("JIT Concat: basic 3+5 bit", "[jit][concat]") {
  ch::core::context ctx("concat_jit_test");
  ch::core::ctx_swap swap(&ctx);

  struct ConcatDevice : public ch::Component {
    ch_in<ch_uint<3>> a;
    ch_in<ch_uint<5>> b;
    ch_out<ch_uint<8>> result;

    void describe() override {
      result = concat(a, b);
    }
  };

  ch::ch_device<ConcatDevice> dev;
  ch::Simulator sim(dev.context());
  sim.set_jit_enabled(true);  // ← KEY: enable JIT

  sim.set_input_value(dev.instance().a, 5);
  sim.set_input_value(dev.instance().b, 26);
  sim.tick();
  REQUIRE(static_cast<uint64_t>(dev.instance().result) == 186);
}
```

### Phase 5: Update Documentation

**File**: `include/jit/AGENTS.md`

Remove concat from the "已知不可 JIT 的操作" list:

```diff
- ### 已知不可 JIT 的操作（ADR-021）
- - `concat`: JIT IR 无对应指令，`compile_to_llvm()` 会拒绝 → CALL_EXTERNAL
+ ### JIT 支持的操作
+ ...
+ - `concat` (JitOp::CONCAT) — `(src0 << src1_width) | src1`, lowered to SHL + OR + mask
```

**File**: `docs/developer_guide/patterns/JIT-DEBUGGING-GUIDE.md`

Add a new section: "Concat on Output Ports — RESOLVED"

```markdown
### Concat on Output Ports (RESOLVED in v0.X)

**Symptom**: Output port reads `1 cycle behind` expected value when concat() is used.

**Root Cause**: Hybrid execution gap — interpreter executes concat in data_map_,
JIT reads from data_buffer_ (stale).

**Fix**: JitOp::CONCAT lowering now native (Phase 4, 2026-06-02). concat is no
longer marked as CALL_EXTERNAL. Test `test_reg_timing` passes with JIT.

**Verification**:
- Build with JIT enabled
- Run `ctest -R "concat" --output-on-failure`
- Run `ctest -R "reg_timing" --output-on-failure`
```

---

## 4. Verification Strategy

### 4.1 Build Verification
```bash
cd /workspace/project/CppHDL
cmake --build build -j$(nproc) 2>&1 | grep -E "error|warning" | head
```
Expected: 0 errors, 0 warnings (or only pre-existing).

### 4.2 Test Verification
```bash
# New tests
ctest -R "jit_concat" --output-on-failure -L base

# Existing tests that exercise concat (regression check)
ctest -R "test_operation_results" --output-on-failure -L base
ctest -R "test_reg_timing" --output-on-failure -L base

# Full test suite
ctest -L base --output-on-failure
./run_all_ported_tests.sh
```
Expected: All tests pass, including `test_reg_timing` original ShiftRegister (with concat).

### 4.3 DAG Verification

Re-run `samples/dag_compare.cpp` to confirm concat nodes are now JIT-compiled
(not in `external_node_ids_`). Add a debug print to `generate_ir()`:

```cpp
if (jit_op == JitOp::CONCAT) {
  CHDBG("[JIT] CONCAT node %u: src0_w=%u, src1_w=%u, result_w=%u",
        node_id, src0_bitwidth, src1_bitwidth, result_bitwidth);
}
```

---

## 5. Risks & Mitigations

| Risk | Likelihood | Impact | Mitigation |
|------|------------|--------|------------|
| Argument order bug introduces silent data corruption | Medium | High (test pass + wrong output) | Phase 4 tests verify against interpreter values for multiple widths |
| Concat nodes appear in `external_node_ids_` (forgot to remove CALL_EXTERNAL) | Low | Medium (test_reg_timing still fails) | Add explicit assertion in test_jit_concat.cpp |
| Shift amount is 0 because `src_bitwidth` not set in `generate_ir()` | Medium | High (wrong output, no crash) | Phase 4.6 tests include 32+32=64 bit boundary |
| Truncation issue: src0 has garbage in upper bits | Low | Medium | The mask `AND (1ULL << result_bw) - 1` should handle this |
| Other operations using CONCAT pattern (e.g., `bits_update` could be implemented similarly) accidentally get affected | Low | Low | Don't touch other ops; scope is strictly `ch_op::concat` |
| 1-bit src0/src1 edge case (concat of literals) | Low | Low | Test Phase 4.3 (concat with literal) covers this |

---

## 6. Rollback Plan

If Phase 4 reveals unexpected failures that can't be fixed in the same PR:

1. Revert `ch_op::concat` mapping back to `CALL_EXTERNAL` (Phase 2.1 change)
2. Keep the lowered CONCAT code (Phase 1) as it doesn't affect anything if not used
3. Re-enable the `test_reg_timing` no-concat workaround (already in place)
4. File an issue for future investigation

The no-concat workaround (`ShifterRegisterNoConcat`) remains valid as a documented pattern.

---

## 7. Success Criteria

- [ ] `ctest -L base` passes 100% (including `test_reg_timing` original ShiftRegister with concat)
- [ ] `ctest -R "jit_concat"` passes for all 6 new test cases
- [ ] `./run_all_ported_tests.sh` passes 28/28
- [ ] `cmake --build build` reports 0 errors, 0 new warnings
- [ ] `include/jit/AGENTS.md` updated to reflect CONCAT support
- [ ] DAG comparison (with vs without JIT) shows identical node structures for concat
- [ ] No regression in CALL_EXTERNAL count for previously-working tests

---

## 8. Out of Scope

- Implementing JIT lowering for `bits_update`, `mux`, `assign` (still CALL_EXTERNAL)
- Optimizing concat to avoid intermediate vreg allocation
- Multi-operand concat (current API is binary only)
- Concat in `cond_*` expressions (covered by generic JIT path)

---

## 9. Related Files

| File | Lines | Change |
|------|-------|--------|
| `src/jit/jit_compiler.cpp` | 377-381 | Remove concat from CALL_EXTERNAL, add `case ch_op::concat: jit_op = JitOp::CONCAT;` |
| `src/jit/jit_compiler.cpp` | 1039-1053 | Fix arg order bug in CONCAT lowering |
| `src/jit/jit_compiler.cpp` | 450-454 | Add CONCAT-specific IR building with `src_bitwidth = src1_width` |
| `tests/test_jit_concat.cpp` | new | 6 test cases |
| `tests/CMakeLists.txt` | append | `add_catch_test(test_jit_concat test_jit_concat.cpp)` |
| `include/jit/AGENTS.md` | 19-22 | Remove concat from "不可JIT" list, add to "supported" |
| `docs/developer_guide/patterns/JIT-DEBUGGING-GUIDE.md` | append | "Concat on Output Ports — RESOLVED" section |
| `samples/dag_compare.cpp` | unchanged | Re-run to verify DAG after fix |

---

**Author**: Sisyphus (Cascade)
**Reviewer**: TBD
**Estimated effort**: 2-3 hours (1 hour code + 1 hour tests + 30 min docs)
