# Interpreter Arithmetic Width Fix — Post-Phase-0 Addendum

**Date**: 2026-06-16
**Status**: Completed (4 atomic commits + git tag)
**Tag**: `interpreter-arith-fix`

## Background

During Phase 0 of the c-class major refactor (JIT safety net via golden
tests in `tests/jit/test_jit_golden.cpp`), a pre-existing divergence
between the interpreter and JIT was discovered for arithmetic operations
that change the natural result bitwidth.

The interpreter's `Add::eval` and `Mul::eval` in `include/ast/instr_op.h`
manually resized `dst->bv_` to a wider bitwidth than the compile-time
result type, while the JIT correctly applied a `(1<<bw)-1` mask to keep
the result within the compile-time-determined bitwidth. This caused
`interp != jit` for ADD/MUL/SHL/NEG expressions.

The LoadConstDut in `tests/jit/test_jit_golden.cpp` originally used
`r0 ^ r1` (XOR) instead of `r0 + r1` to avoid this divergence.

## Fixes Applied

**`include/ast/instr_op.h`**:

| Op        | Bug                                                | Fix                                                                                      |
|-----------|----------------------------------------------------|------------------------------------------------------------------------------------------|
| `Add`     | Manual resize to `max(src0, src1) + 1`             | Removed; `bv_add_truncate` truncates natural result to `dst->size()`                    |
| `Mul`     | Manual resize to `src0.bw + src1.bw`               | Removed; `bv_mul_truncate` truncates natural result to `dst->size()`                    |
| `Shl`     | `*dst = (*src0) << shift` — implicit resize        | Replaced with explicit `bv_shl_truncate(&dst->bv_, &src0->bv_, shift)`                  |
| `Neg`     | `*dst = -(*src)` — implicit resize to `src.bw+1`   | Replaced with uint64_t `(~src + 1) & mask(dst.bw)`, then integer assign (preserves bw)  |

**`tests/test_interpreter_jit_compat.cpp`** (new, 5 test cases, 31 asserts):
Pins the new semantics by running each operation in BOTH interpreter
and JIT modes and asserting byte-identical output for overflow and
non-overflow inputs. Coverage:

- ADD (widens to ch_uint<N+1> per `add_op::result_width`)
- MUL (widens to ch_uint<2N>)
- SHL (widens to ch_uint<2N>)
- SUB (stays ch_uint<N>)
- NEG (stays ch_uint<N>)

## Commits

```
99089ad chore(arith): investigate remaining interpreter ops for width bugs
f790ca6 fix(arith): align interpreter ADD/MUL/SHL/NEG width with JIT
9e05569 test(arith): add regression test for interpreter-JIT ADD/MUL/SHL/NEG compatibility
668b25e docs(jit): document deeper ch_out truncation divergence in LoadConstDut
```

## Verification

- 141/141 ctest pass (140 prior + 1 new)
- 28/28 ported examples pass
- 0 new compiler warnings
- Single file changed: `include/ast/instr_op.h` (+25 / −7)
- New test file: `tests/test_interpreter_jit_compat.cpp`

## LoadConstDut Status: `^` Workaround Retained

Task 4 of the original plan was to replace `r0 ^ r1` with `r0 + r1` in
LoadConstDut. **This was not done** because the change exposed a
**separate pre-existing divergence at the `ch_out<ch_uint<N>>` level**:

- `ch_uint<8> + ch_uint<8>` = `ch_uint<9>` (SpinalHDL: carry bit kept)
- The interpreter returns the source opimpl's full natural width
  (`ch_uint<9>` value `0x140` for `0xED + 0x53`)
- The JIT masks the stored value to the ch_out's declared 8-bit width
  at STORE_DATA, producing `0x40`

The `interp != jit` divergence for `r0 + r1` assigned to `ch_out<ch_uint<8>>`
is **NOT** caused by the Add::eval fix (which was correct). It is caused
by the ch_out not truncating the source to its declared bitwidth.

**This divergence is out of scope for the arithmetic width fix.**
A full resolution requires one of:
1. Make `ch_out::operator<<=` truncate the source to the ch_out's bitwidth
2. Change the compile-time result type of `ch_uint<N> + ch_uint<N>` to
   `ch_uint<N>` (not `ch_uint<N+1>`) — would diverge from SpinalHDL
3. Make the JIT match the interpreter by using the source opimpl's
   natural width instead of the ch_out's declared width

The `^` workaround remains in LoadConstDut with a 9-line inline comment
documenting the situation. See
`tests/jit/test_jit_golden.cpp:LoadConstDut` for the comment.

## C-Class Refactor Impact

The JIT golden tests (Phase 0) are now safe — the test_jit_golden
suite passes 100% and pins the corrected behavior. The Phase 1
`generate_ir()` and `compile_to_llvm()` splits can proceed without
worrying about silent interpreter/JIT divergence in ADD/MUL/SHL/NEG.

The deeper ch_out truncation issue should be tracked separately and
addressed before any further work on ch_out semantics. It is a
pre-existing bug, not introduced by this fix.
