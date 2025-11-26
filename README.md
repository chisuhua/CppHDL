# CppHDL

This repository is a collection of C++ libraries for high-level hardware description (HDL) development. The main purpose is to create a robust and efficient toolset for developing complex HDL designs using C++17, CMake, and Catch2.

## Key Technologies
- C++17: The primary programming language used for developing the libraries.
- CMake: A build system for managing dependencies and building the project.
- Catch2: A testing framework used for validating all components.

## Architecture Overview
The repository is organized into several components:

- **AST (Abstract Syntax Tree)**: Defines the internal structure of HDL designs.
- **CodeGen**: Converts AST nodes into C++ code for generating complex designs.
- **Core**: Provides the low-level components necessary for HDL design.
- **Device**: Simulates HDL designs in a robust manner.
- **IO**: Handles various communication protocols used in HDL designs.
- **Simulator**: The main execution environment for the repository.
- **Module**: Contains user-defined HDL modules, facilitating complex design development.

## Directory Structure
- **./.gitignore**: Specifies files and directories to be ignored in version control.
- **./build.sh**: Automates the build process.
- **./CMakeLists.txt**: Configures the CMake build system.
- **./docs**: Contains documentation files.
- **./examples**: Sample implementations of various HDL designs.
- **./include**: Header files for the project's components.
- **./samples**: More detailed sample implementations.
- **./src**: Source files for the project's components.
- **./tests**: Test files using Catch2.

## Usage
To build and run the project:

```bash
cmake .
make
./build.sh
```

To review the code for potential issues:

```bash
/continue/review
```

## Development Environment
Set up a development environment by ensuring you have:

- C++17 compiler
- CMake installed
- Catch2 library included

## Lint and Format
Use `clang-format` to format your code:

```bash
clang-format -i <filename>
```

Use `cpplint` to check for style and syntax issues:

```bash
cpplint <filename>
```