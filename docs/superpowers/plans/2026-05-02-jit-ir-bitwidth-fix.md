# JIT IR Bitwidth Fix Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Fix LLVM IR generation in JIT compiler to correctly handle bitwidth masking, eliminating garbage values in narrow signals.

**Architecture:** Maintain dual-function (comb/seq) JIT architecture. Fix is purely in LLVM IR generation layer - mask values instead of truncating, and ensure all node types have proper handlers.

**Tech Stack:** C++20, LLVM 18, Catch2, CMake

---

## Problem Summary

| Issue | Root Cause | Impact |
|-------|------------|--------|
| vreg garbage bits | `load_node` truncates to iN then stores in i64 alloca, subsequent i64 load reads garbage | All <64-bit signals have wrong values |
| Arithmetic overflow | ADD/SUB/MUL don't mask to target bitwidth | Overflow wraps incorrectly |
| type_lit skips nodes | `break` exits entire for loop | Literal's successors never get IR |
| Missing handlers | type_output/bitsupdate fall through to default | Some nodes produce no IR |

---

## File Structure

**Single file to modify:** `src/jit/jit_compiler.cpp`

Key sections in this file:
- `generate_ir()` function (lines ~195-377): IR generation from node list
- `compile_to_llvm()` function (lines ~380-680): LLVM IR lowering

---

## Task 1: Fix `type_lit` - change break to continue

**Files:**
- Modify: `src/jit/jit_compiler.cpp:246-252`

**Context from Oracle:**
```
break exits the entire for loop, not just the switch.
This means after the first literal node, ALL subsequent nodes
in eval_list are skipped entirely.
```

- [ ] **Step 1: Verify current code at line 246**

```cpp
// Should see:
if (node_type == ch::core::lnodetype::type_lit) {
    auto vreg = next_comb_vreg++;
    block_comb.instrs.push_back(make_load(vreg, node_id, bitwidth));
    break;  // ← This is wrong
}
```

Run: `sed -n '246,252p' src/jit/jit_compiler.cpp`

Expected: Shows the break statement

- [ ] **Step 2: Change break to continue**

```cpp
if (node_type == ch::core::lnodetype::type_lit) {
    continue;  // Skip literal nodes - values come from sync_from_buffer
}
```

Run: `sed -i '251s/break;/continue;/' src/jit/jit_compiler.cpp`

- [ ] **Step 3: Verify change**

Run: `sed -n '246,252p' src/jit/jit_compiler.cpp`

Expected: Line 251 shows `continue;`

- [ ] **Step 4: Build and test**

```bash
cd build_jit_on
cmake --build . --target test_jit_compiler -j$(nproc)
./tests/test_jit_compiler 2>&1 | grep -E "JIT compilation|ir_instr_count"
```

Expected: JIT compilation succeeds with ir_instr_count > 1 (was 1 before)

---

## Task 2: Fix `load_node` - mask instead of truncate

**Files:**
- Modify: `src/jit/jit_compiler.cpp:416-422`

**Context from Oracle:**
```
load_node does Trunc(i64 → iN) when bw < 64.
But vreg alloca is i64, so storing iN then loading i64 gives garbage in upper bits.
Fix: AND with mask instead of Trunc, keeping i64 in vreg.
```

- [ ] **Step 1: Find load_node lambda**

Run: `grep -n "auto load_node" src/jit/jit_compiler.cpp`

Expected: Shows line ~416

- [ ] **Step 2: Read current load_node implementation**

```cpp
// Current (wrong):
auto load_node = [&](uint32_t node_id, BitWidth bw) -> llvm::Value* {
    auto* ptr = get_node_ptr(node_id);
    auto* val = builder.CreateLoad(builder.getInt64Ty(), ptr, "load_node");
    if (bw < 64) {
        return builder.CreateTrunc(val, builder.getIntNTy(bw), "trunc");
    }
    return val;
};
```

- [ ] **Step 3: Replace with masked version**

```cpp
// Fixed:
auto load_node = [&](uint32_t node_id, BitWidth bw) -> llvm::Value* {
    auto* ptr = get_node_ptr(node_id);
    auto* val = builder.CreateLoad(builder.getInt64Ty(), ptr, "load_node");
    if (bw < 64) {
        uint64_t mask = (1ULL << bw) - 1;
        return builder.CreateAnd(val, builder.getInt64(mask), "mask_load");
    }
    return val;
};
```

- [ ] **Step 4: Build and verify**

```bash
cd build_jit_on
cmake --build . --target test_jit_compiler -j$(nproc) 2>&1 | tail -5
```

Expected: Builds successfully

---

## Task 3: Fix arithmetic operations - mask after each op

**Files:**
- Modify: `src/jit/jit_compiler.cpp:463-533` (ADD, SUB, MUL, NOT, SHL, SHR cases)

**Context from Oracle:**
```
Without masking after arithmetic, i64 operations overflow.
0xFF + 0x01 = 0x100 instead of 0x00 for 8-bit.
```

**Pattern for each arithmetic op:**
```cpp
case JitOp::ADD: {
    auto* a = builder.CreateLoad(builder.getInt64Ty(), vregs[instr.src0], "load_a");
    auto* b = builder.CreateLoad(builder.getInt64Ty(), vregs[instr.src1], "load_b");
    auto* res = builder.CreateAdd(a, b, "add");
    // ADD MASK
    if (instr.bitwidth < 64) {
        uint64_t mask = (1ULL << instr.bitwidth) - 1;
        res = builder.CreateAnd(res, builder.getInt64(mask), "mask_add");
    }
    builder.CreateStore(res, vregs[instr.dst], "store_add");
    break;
}
```

- [ ] **Step 1: Find ADD case**

Run: `grep -n "case JitOp::ADD" src/jit/jit_compiler.cpp`

Expected: Line ~463

- [ ] **Step 2: Read and modify ADD case**

Read lines 463-470, then edit to add masking after add.

- [ ] **Step 3: Modify SUB case similarly**

- [ ] **Step 4: Modify MUL case similarly**

- [ ] **Step 5: Fix NOT case (special - XOR with mask, not flip all bits)**

```cpp
case JitOp::NOT: {
    auto* a = builder.CreateLoad(builder.getInt64Ty(), vregs[instr.src0], "load_a");
    uint64_t mask = (instr.bitwidth < 64) ? ((1ULL << instr.bitwidth) - 1) : ~0ULL;
    auto* res = builder.CreateXor(a, builder.getInt64(mask), "not");
    builder.CreateStore(res, vregs[instr.dst], "store_not");
    break;
}
```

- [ ] **Step 6: Modify SHL case (left shift overflow)**

```cpp
case JitOp::SHIFT_LEFT: {
    auto* a = builder.CreateLoad(builder.getInt64Ty(), vregs[instr.src0], "load_a");
    auto* b = builder.CreateLoad(builder.getInt64Ty(), vregs[instr.src1], "load_b");
    auto* res = builder.CreateShl(a, b, "shl");
    if (instr.bitwidth < 64) {
        uint64_t mask = (1ULL << instr.bitwidth) - 1;
        res = builder.CreateAnd(res, builder.getInt64(mask), "mask_shl");
    }
    builder.CreateStore(res, vregs[instr.dst], "store_shl");
    break;
}
```

- [ ] **Step 7: Verify all edits**

```bash
cd build_jit_on
cmake --build . --target test_jit_compiler -j$(nproc) 2>&1 | tail -3
```

---

## Task 4: Add `type_output` handler

**Files:**
- Modify: `src/jit/jit_compiler.cpp` - add case before line 344

**Context from Oracle:**
```
type_output nodes copy their source value to their own node_id.
Same semantic as type_proxy - load from source, store to self.
```

- [ ] **Step 1: Find where to insert**

Run: `grep -n "case ch::core::lnodetype::type_clock" src/jit/jit_compiler.cpp`

Expected: Line ~344

- [ ] **Step 2: Add type_output case before type_clock**

```cpp
case ch::core::lnodetype::type_output: {
    if (node->num_srcs() > 0 && node->src(0)) {
        auto src_vreg = next_comb_vreg++;
        auto src_bitwidth = static_cast<BitWidth>(node->src(0)->size());
        block_comb.instrs.push_back(
            make_load(src_vreg, node->src(0)->id(), src_bitwidth));
        block_comb.instrs.push_back(
            make_store(node_id, src_vreg, bitwidth));
    }
    break;
}
```

---

## Task 5: Handle `type_bitsupdate` - return UNSUPPORTED_OP

**Files:**
- Modify: `src/jit/jit_compiler.cpp` - add case for bitsupdate

**Context from Oracle:**
```
bitsupdate requires complex bit manipulation (SLICE + SHIFT + OR).
Current IR doesn't support this. Return UNSUPPORTED_OP to fall back to interpreter.
```

- [ ] **Step 1: Add bitsupdate case**

Insert after type_output case:

```cpp
case ch::core::lnodetype::type_bitsupdate: {
    // bitsupdate needs full bit manipulation (SLICE/SHIFT/OR) - not yet supported
    // Return UNSUPPORTED_OP to fall back to interpreter
    return JitResult::UNSUPPORTED_OP;
}
```

---

## Task 6: Verify full test suite

- [ ] **Step 1: Build all targets**

```bash
cd build_jit_on
cmake --build . -j$(nproc) 2>&1 | tail -5
```

Expected: All targets build without errors

- [ ] **Step 2: Run test_jit_compiler**

```bash
./tests/test_jit_compiler 2>&1 | grep -E "test cases|assertions"
```

Expected: All 15 test cases pass (was 1/15)

- [ ] **Step 3: Run test_fifo**

```bash
./tests/test_fifo 2>&1 | grep -E "All tests|FAILED"
```

Expected: All tests pass (28 assertions in 4 test cases)

- [ ] **Step 4: Run base tests**

```bash
ctest -L base --output-on-failure 2>&1 | tail -10
```

Expected: 97/97 pass (was 96/97 - only test_jit_compiler failed)

- [ ] **Step 5: Run full suite**

```bash
ctest --output-on-failure 2>&1 | tail -15
```

Expected: 122/123 pass (only perf_tests timeout is acceptable)

---

## Self-Review Checklist

- [ ] All arithmetic ops have masking after them (ADD, SUB, MUL, NOT, SHL, SHR)
- [ ] load_node uses AND mask instead of Trunc
- [ ] type_lit uses continue, not break
- [ ] type_output has proper handler
- [ ] type_bitsupdate returns UNSUPPORTED_OP
- [ ] No placeholders (TBD, TODO) in the code
- [ ] Tests pass: test_jit_compiler (15/15), test_fifo (28/28), base (97/97)

---

## Execution Options

**Plan complete.**

Two execution options:

**1. Subagent-Driven (recommended)** - I dispatch a fresh subagent per task, review between tasks, fast iteration

**2. Inline Execution** - Execute tasks in this session using executing-plans, batch execution with checkpoints

Which approach?