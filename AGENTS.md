# AGENTS.md - CppHDL Project Knowledge Base

Generated: 2026-03-05 | Commit: 982c1b5 | Branch: main

## OVERVIEW
C++ header-only library converting C++ AST to hardware logic. Supports simulation, Verilog generation. Components: AST, CodeGen, Simulator, Core (logic nodes), Device, IO, Module.

## STRUCTURE
```
CppHDL/
├── include/           # Public headers (ch namespace)
│   └── ch/           # Core HDL types: bit<N>, int<N>, uint<N>
├── src/              # Implementation files
├── tests/            # Catch2 unit tests (test_*.cpp)
├── samples/          # Example HDL designs
├── docs/             # Design documentation
├── build/            # CMake build output
└── AGENTS.md         # This file
```

## WHERE TO LOOK
| Task | Location |
|------|----------|
| Add new HDL type | include/ch/types.h |
| Implement logic node | src/core/logic_node.cpp |
| Fix simulator bug | src/simulator/simulator.cpp |
| Add AST node | include/ast/nodes.h |
| Verilog generation | src/codegen/verilog.cpp |
| IO protocol impl | src/io/*.cpp |
| Write test | tests/test_*.cpp |
| Debug tracing | src/simulator/trace.cpp |

## CONVENTIONS
- **C++20**: Strictly enforced via CMake
- **Files**: snake_case (ast_nodes.h, codegen_verilog.cpp)
- **Namespaces**: snake_case (ch, ch::core, ch::ast)
- **Classes**: PascalCase (Simulator, LogicNode, AstNode)
- **Functions**: camelCase (toString, addNode)
- **Variables**: snake_case (trace_on, ctx)
- **Members**: snake_case + trailing underscore (ctx_, trace_on_)
- **Macros**: SCREAMING_SNAKE_CASE (CHDBG_FUNC, CHREQUIRE)

## ANTI-PATTERNS
- **SEGFAULT workarounds**: Do not add null checks as band-aids. Fix root cause in memory management.
- **Unimplemented functions**: Do not leave empty implementations. Throw or mark explicit.
- **Async FIFO**: Not fully supported. Avoid complex async patterns.
- **file(GLOB_RECURSE)**: Anti-pattern in CMake. Use explicit file lists.
- **using namespace**: Never in headers.

## UNIQUE STYLES
- **CH namespace**: All public API under `ch::` prefix
- **Catch2**: Amalgamated single-header in tests/
- **Interface library**: Pure virtual base classes for IO protocols
- **Custom types**: ch::bit<N>, ch::int<N>, ch::uint<N> for hardware signals
- **CHREQUIRE**: Custom assertion macro for critical invariants
- **CHDBG_FUNC**: Logging macro at function entry

## COMMANDS
```bash
# Configure
cmake -B build -DCMAKE_BUILD_TYPE=Debug

# Build all
cmake --build build -j$(nproc)

# Run all tests
cd build && ctest --output-on-failure

# Run specific test
cmake --build build --target test_reg
./build/tests/test_reg

# Debug logging
cmake -B build -DENABLE_DEBUG_LOGGING=ON

# Clean
rm -rf build
```

## NOTES
- Test targets = filename without .cpp (test_reg.cpp → test_reg)
- Trace config via trace.ini in Simulator
- Exceptions used for fatal simulation/AST errors
- Return codes preferred for recoverable errors

## HIERARCHICAL AGENTS.MD
Create child files for component-specific knowledge:
- include/AGENTS.md      # API conventions, types
- src/AGENTS.md          # Implementation patterns
- tests/AGENTS.md        # Test conventions, Catch2 usage
- samples/AGENTS.md      # Example patterns
- docs/AGENTS.md         # Documentation structure
