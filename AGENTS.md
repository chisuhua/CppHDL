Project Description
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

  Directory Structure
  - ./.gitignore: Specifies files and directories to be ignored in version control.
  - ./build.sh: A script to automate the build process.
  - ./CMakeLists.txt: Defines the CMake build system for the project.
  - ./docs: Contains various design and usage documentation files.
  - ./examples: Sample implementations of various HDL designs.
  - ./include: Header files for the project's components.
  - ./samples: More detailed sample implementations.
  - ./src: Source files for the project's components.
  - ./tests: Test files using Catch2 for validation.

  Development Workflow
  - Build: Use the provided CMakeLists.txt to build the project. Run cmake . followed by make in the terminal.
  - Run: Use the provided build.sh script to run the project. Navigate to the build directory and execute ./build.sh.
  - Testing: Execute tests using the provided tests directory. Use the catch_amalgamated.sh script to run all tests.
  - Development Environment: Use C++17, CMake, and Catch2 to set up the development environment.
  - Lint and Format: Use clang-format to ensure code is formatted correctly and cpplint to check for style and syntax issues.

  Custom Slash Command

  ---
  invokable: true
  ---

  Review this code for potential issues, including:

  - Ensure all files follow idiomatic C++ coding standards.
  - Check for unused imports that can be removed.
  - Verify that memory and logic buffer implementations are correct and efficient.
  - Review the high-level architecture for clarity and correctness.
  - Ensure the simulation environment is properly configured and tested.

  Provide specific, actionable feedback for improvements.
