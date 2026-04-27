# JIT P2: LLVM Codegen Implementation Plan

**Project**: CppHDL JIT Compilation
**Phase**: P2 - LLVM Codegen
**Created**: 2026-04-27
**Based on**: Metis analysis

---

## Overview

P2 implements `compile_to_llvm()` to convert JitInstr IR to executable machine code via LLVM ORC JIT.

**Prerequisites**: P0 (LLVM infrastructure), P1 (IR generation)
**Estimated complexity**: High (LLVM API + simulator integration)

---

## Execution Model

```
Simulator::tick() (C++ control flow preserved)
  ├── eval_combinational()
  │     └── JIT available → JitCompiler::execute_tick()
  │         └── Iterate JitInstr, read/write data_buffer_
  │     └── JIT unavailable → Original interpreter
  ├── clock eval (C++)
  └── sequential eval (C++)
```

**Rationale**: Clock/timing control is complex C++ logic; combinational is pure dataflow → best JIT target.

---

## Data Layout

```
uint64_t data_buffer[max_node_id + 1]
```

- Flat array indexed by node_id (O(1) access vs unordered_map)
- Each entry: node's current value (low 64 bits)
- **Hard constraint**: bitwidth <= 64; >64 returns UNSUPPORTED_OP

---

## Steps

### Step 0: Fix CMake LLVM Linking

| Item | Description |
|------|-------------|
| Input | CMakeLists.txt |
| Output | Correct LLVM include + link config |
| Action | After `find_package(LLVM)`, add `include_directories(${LLVM_INCLUDE_DIRS})`, `llvm_map_components_to_libnames()`, `target_link_libraries(cpphdl PRIVATE ${LLVM_LIBS})` |
| Verification | `cmake -B build -DCH_JIT_ENABLE=ON` succeeds without LLVM warnings |

### Step 1: Data Buffer Management

| Item | Description |
|------|-------------|
| Input | data_map_t (simulator state) |
| Output | `std::vector<uint64_t> data_buffer_` + sync methods |
| Methods | `allocate_buffer(ctx)`, `sync_to_buffer()`, `sync_from_buffer()` |
| Verification | Buffer size = max_node_id + 1; sync preserves values |

### Step 2: LLVM IR Generation (compile_to_llvm)

| JitOp | LLVM IR |
|-------|---------|
| LOAD_CONST | `ConstantInt::get(IntegerType::get(ctx, bw), imm.value)` |
| LOAD_DATA | `CreateGEP` + `CreateLoad` from data_buffer[node_id] |
| STORE_DATA | `CreateGEP` + `CreateStore` to data_buffer[node_id] |
| ADD/SUB/MUL | `CreateAdd`, `CreateSub`, `CreateMul` |
| AND/OR/XOR/NOT | `CreateAnd`, `CreateOr`, `CreateXor`, `CreateNot` |
| SHIFT_LEFT/RIGHT | `CreateShl`, `CreateLShr` |
| EQ/NE/LT/LE/GT/GE | `CreateICmpEQ/NE/ULT/ULE/UGT/UGE` |
| SELECT | `CreateSelect` |
| REG_NEXT | Same as STORE_DATA (P1 semantics) |
| CONCAT/SLICE/CALL_EXTERNAL | UNSUPPORTED_OP (fallback) |

### Step 3: ORC JIT Compilation

| Item | Description |
|------|-------------|
| Input | llvm::Module |
| Output | `void(*)(uint64_t*)` function pointer |
| API | LLJIT: `addIRModule()` → `lookup("tick")` → cast to function pointer |
| Error handling | Any `cantFail`/`Expected` failure → COMPILATION_FAILED |

### Step 4: execute_tick() Implementation

| Item | Description |
|------|-------------|
| Logic | `sync_to_buffer()` → `compiled_func_(buffer)` → `sync_from_buffer()` |
| Verification | Data consistent after execution; no crashes on repeated calls |

### Step 5: Simulator Integration

| Item | Description |
|------|-------------|
| Location | Simulator::eval_combinational() |
| Logic | If `jit_compiler_.is_available() && jit_ready_` → JIT path; else interpreter |
| Init | After `Simulator::initialize()`, call `jit_compiler_.compile(ctx_)` |

### Step 6: Boundary Checks

| Condition | Result |
|-----------|--------|
| bitwidth > 64 | UNSUPPORTED_OP |
| CONCAT/SLICE/CALL_EXTERNAL present | UNSUPPORTED_OP |
| eval_list empty | SUCCESS (empty function) |
| No LLVM | LLVM_INIT_FAILED |

---

## File Changes

| File | Change |
|------|--------|
| CMakeLists.txt | Fix LLVM linking |
| include/jit/jit_compiler.h | Add data_buffer_, sync methods |
| src/jit/jit_compiler.cpp | Implement compile_to_llvm, execute_tick |
| include/simulator.h | Add JitCompiler member, jit_ready_ flag |
| src/simulator.cpp | Integrate JIT in eval_combinational() |
| tests/test_jit_compiler.cpp | New: unit tests for codegen |

---

## QA Verification

| Test | Command | Expected |
|------|---------|----------|
| CMake fix | `grep llvm_map_components CMakeLists.txt` | Non-empty |
| Build | `cmake --build build 2>&1 \| grep -c error` | 0 |
| Empty circuit | ctest -R "jit.*empty" | PASS |
| Arithmetic ops | ctest -R "jit.*arith" | PASS |
| >64bit fallback | ctest -R "jit.*unsupported" | PASS |
| Regression | ctest --output-on-failure | 0 failures |

---

## Constraints

- MUST: All JIT code guarded by `#ifdef CH_JIT_ENABLED`
- MUST: >64bit nodes → UNSUPPORTED_OP (no silent truncation)
- MUST NOT: Modify P1 generate_ir() semantics
- MUST NOT: JIT handles clock/timing logic (stays in C++)

---

## Status

- [x] Plan created
- [ ] Step 0: CMake fix
- [ ] Step 1: Data buffer
- [ ] Step 2: IR generation
- [ ] Step 3: ORC JIT
- [ ] Step 4: execute_tick
- [ ] Step 5: Simulator integration
- [ ] Step 6: Boundary checks