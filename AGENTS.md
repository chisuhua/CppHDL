# AGENTS.md

## Project Context
This is a C++ library for high-level hardware description (HDL).
The system converts C++ AST into hardware logic, with simulation and Verilog generation capabilities.
Key components: AST, CodeGen, Simulator, Core (logic nodes).

## Project Description
  - This project is a collection of C++ libraries for high-level hardware description (HDL) development.
  - The main purpose is to create a robust and efficient toolset for developing complex HDL designs using C++.
  - Key technologies used include C++17, CMake, and Catch2.

  Architecture Overview
  - The repository is organized into several components:
    - AST (Abstract Syntax Tree): Defines the structure of HDL designs.
    - CodeGen: Generates C++ code from the AST.
    - Core: Contains low-level components for HDL design.
    - Device: Simulates HDL designs.
    - IO: Handles various communication protocols.
    - Simulator: Main execution environment for HDL designs.
    - Module: Contains user-defined HDL modules.
  - Data Flow:
    - AST Nodes are read from source files.
    - Clock Implementation is generated for each clock in the design.
    - Memory Implementation and Memory Port Implementation are generated for each memory and memory port in the design.
    - Logic Buffer and Register components are generated based on the AST nodes.
  - System Interactions:
    - Simulation is triggered by the simulator component.
    - Instruction Execution (IO and memory) is managed by the instruction base and proxy components.


## Build Commands
- **Configure**: `cmake -B build -DCMAKE_BUILD_TYPE=Debug`
- **Build All**: `cmake --build build -j$(nproc)`
- **Clean**: `rm -rf build`

## Test Commands (Catch2)
Tests are located in `tests/`.
- **Run All Tests**: `cd build && ctest --output-on-failure`
- **Run Specific Test File**:
  1. Build the specific test target: `cmake --build build --target test_name` (e.g., `test_reg`)
  2. Run the executable: `./build/tests/test_name`
  - *Note*: Test targets map to filenames without `.cpp`. `tests/test_reg.cpp` -> target `test_reg`
- **Run Specific Test Case**: `./build/tests/test_name "Name of Test Case"`
- **List Test Cases**: `./build/tests/test_name --list-tests`

## Coding Standards

### C++ Modernity & Style
- **Standard**: C++20 (strictly enforced via CMake).
- **Naming**:
  - Files: snake_case (e.g., `ast_nodes.h`, `codegen_verilog.cpp`).
  - Namespaces: snake_case (e.g., `namespace ch`, `namespace ch::core`).
  - Classes/Structs: PascalCase (e.g., `Simulator`, `LogicNode`).
  - Functions/Methods: camelCase (e.g., `toString`, `addNode`).
  - Variables: snake_case (e.g., `trace_on`, `ctx`).
  - Members: snake_case with trailing underscore (e.g., `ctx_`, `trace_on_`).
  - Constants/Macros: SCREAMING_SNAKE_CASE (e.g., `CHDBG_FUNC`, `ENABLE_DEBUG_LOGGING`).

### Headers & Imports
- **Include Order**:
  1. Companion header (if .cpp file).
  2. Project headers (relative or "path/to/header.h").
  3. Standard library headers (`<vector>`, `<string>`).
  4. External library headers.
- **Paths**: Use standard project structure. Source in `src/`, headers in `include/`.
- Avoid `using namespace` in headers.

### Error Handling & Logging
- **Assertions**: Use `CHREQUIRE(condition, message)` for critical invariants.
- **Logging**:
  - Use `CHDBG_FUNC()` at entry of complex functions.
  - Use project-specific logging macros if available (e.g., `CH_LOG`).
- **Exceptions**: Prefer return codes or status objects for recoverable errors, but exceptions are used for fatal simulation/AST errors.

### Hardware Description Specifics
- **Types**: Use project-specific types `ch::bit<N>`, `ch::int<N>`, `ch::uint<N>` for hardware signals.
- **Logic**: Ensure logic buffer implementations in `Core` are efficient.
- **Simulation**: Use `Simulator` class. Always verify trace configuration if debugging.

## Common Workflows for Agents
1. **Adding a Feature**:
   - Create/Modify header in `include/`.
   - Implement in `src/`.
   - Add new test file in `tests/` (e.g., `test_new_feature.cpp`).
   - Update `tests/CMakeLists.txt` to register the new test:
     ```cmake
     add_catch_test(test_new_feature test_new_feature.cpp)
     set_tests_properties(test_new_feature PROPERTIES LABELS "base")
     ```
   - Build and verify: `cmake --build build && ./build/tests/test_new_feature`.

2. **Refactoring**:
   - Run existing relevant tests *before* changes to establish baseline.
   - Apply changes.
   - Run tests again. If logic changes, update tests.

3. **Debugging**:
   - Use `cmake -B build -DENABLE_DEBUG_LOGGING=ON` to enable verbose logs.
   - Check `trace.ini` logic in Simulator if tracing issues occur.

## Project Structure
- `src/`: Implementation files.
- `include/`: Header files (public API).
- `tests/`: Catch2-based unit tests.
- `samples/`: Example usage.
- `CMakeLists.txt`: Main build configuration.
- `build.sh`: A script to automate the build process.
- `docs`: Contains various design and usage documentation files.

