# AGENTS.md - CHLib Tests

Child of `tests/AGENTS.md`. Tag all TEST_CASE with `[chlib]`.

## OVERVIEW
17 Catch2 test files validating chlib components: arithmetic, logic, streams, FIFO, pipeline, arbiter.

## STRUCTURE
```
tests/chlib/
├── test_arithmetic.cpp        # Basic arithmetic ops
├── test_arithmetic_advance.cpp # Advanced arithmetic
├── test_bitwise.cpp           # Bitwise operations
├── test_combinational.cpp     # Combinational logic
├── test_converter.cpp         # Type converters
├── test_fifo.cpp              # FIFO components
├── test_logic.cpp             # Logic operations
├── test_memory.cpp            # RAM components
├── test_onehot.cpp            # One-hot encoding
├── test_pipeline_lib.cpp      # Pipeline components
├── test_selector_*.cpp        # Priority, round-robin, arbiter selection
├── test_sequential.cpp        # Sequential logic
├── test_stream*.cpp           # Stream arbiter tests
└── test_switch.cpp            # Switch/routing
```

## CONVENTIONS
- Inherits parent `tests/AGENTS.md` conventions (context pattern, tags, naming)
- All TEST_CASE tagged `[chlib]` for ctest -L chlib filtering
- Includes `chlib.h` (not individual headers)

## PHASE GATES
Follow root Zero-Debt Policy: **编译通过 + 测试覆盖 + 文档同步**. New chlib tests must:
- Every new component → at least one TEST_CASE
- Use `REQUIRE` assertions, not stub pass (empty tests use `SKIP()`)
- Register in `tests/CMakeLists.txt` immediately
- If testing a chlib header, the test file lives in `tests/chlib/`
