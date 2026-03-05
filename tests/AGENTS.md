#TN|# tests/AGENTS.md - Test Subsystem Knowledge Base

Parent: [AGENTS.md](../AGENTS.md)

## OVERVIEW

Catch2 v3.7.0 unit tests validating AST, Core, Simulator, CodeGen. 79 test files: 64 base + 15 chlib.

## STRUCTURE

```
tests/
├── test_*.cpp          # 64 base tests (core functionality)
├── chlib/              # 15 chlib component tests
├── catch_amalgamated.cpp   # Catch2 single-header
└── CMakeLists.txt      # Custom add_catch_test() function
```

## KEY FILES

| Category | Files | Purpose |
|----------|-------|---------|
| Core types | test_reg.cpp, test_basic.cpp | Register, width traits |
| Operators | test_operator.cpp, test_operator_advanced.cpp | Arithmetic, bitwise, comparison |
| Bundles | test_bundle.cpp, test_bundle_advanced.cpp | Bundle construction, connections |
| Memory | test_mem.cpp, test_memory_optimization.cpp | Memory operations |
| Streams | test_ch_stream.cpp, test_stream_*.cpp | Stream operators |
| IO | test_simple_io.cpp, test_io_operations.cpp | IO protocols |
| chlib | test_arithmetic.cpp, test_logic.cpp | Component library |

## CONVENTIONS

- **Include order**: catch_amalgamated.hpp → core headers → ast headers
- **Context pattern**: Always create ch::core::context + ctx_swap for register tests
- **Tags**: `[category][subcategory][feature]` e.g., `[reg][width][basic]`
- **Static tests**: Use STATIC_REQUIRE for compile-time checks
- **Test naming**: test_<feature>.cpp, target = filename without .cpp
- **CMake**: add_catch_test() auto-appends catch_amalgamated.cpp

## ANTI-PATTERNS

- **Empty tests**: Never leave TEST_CASE empty. Use SKIP() or remove.
- **Missing context**: Register operations require ctx_swap or segfault occurs.
- **Hardcoded paths**: Use relative includes from include/ directory.
- **No tags**: Every TEST_CASE needs tags for test selection via ctest -L.
- **Forgotten chlib**: New component tests go in tests/chlib/, tagged [chlib].

## COMMANDS

```bash
# Run base tests only
ctest -L base

# Run chlib tests only
ctest -L chlib

# Run specific tag
ctest -L reg

# Run single test
./build/tests/test_reg
```

## RELATED

- tests/chlib/ : Component library tests
- Catch2 tags: `[basic]`, `[reg]`, `[bundle]`, `[stream]`, `[memory]`, `[io]`, `[chlib]`
