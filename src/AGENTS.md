# AGENTS.md - Library Implementation (src/)

Child of root `AGENTS.md`.

## OVERVIEW
Source implementations for CppHDL: AST instructions, simulator, codegen, core context.

## STRUCTURE
```
src/
├── core/
│   ├── context.cpp       # Context lifecycle, node factory
│   └── lnodeimpl.cpp     # LNodeImpl base operations
├── ast/
│   ├── ast_nodes.cpp     # create_instruction() factory
│   ├── instr_clock.cpp   # Clock instruction eval
│   ├── instr_io.cpp      # IO instruction eval
│   ├── instr_mem.cpp     # Memory instruction eval
│   ├── instr_proxy.cpp   # Proxy instruction eval
│   ├── instr_reg.cpp     # Register instruction eval
│   ├── clockimpl.cpp     # Clock domain logic
│   ├── resetimpl.cpp     # Reset logic
│   ├── memimpl.cpp       # Memory operations
│   ├── mem_port_impl.cpp # Memory port operations
│   └── muximpl.cpp       # Mux operations
├── lnode/                # Logic node implementations
├── utils/                # Type definitions
├── component.cpp         # Component base class
├── simulator.cpp         # Simulator main loop
├── codegen_verilog.cpp   # Verilog code generator
├── codegen_dag.cpp       # DAG code generator
└── stream_pipeline.cpp   # Stream pipeline implementation
```

## CONVENTIONS
- Each `instr_*.cpp` implements the `eval()` method for simulation
- `context.cpp` handles node allocation via `create_node<T>()`
- `simulator.cpp` drives the clock tick loop and port data_map

## PHASE GATES
Follow root AGENTS.md: **编译通过 + 测试覆盖 + 文档同步** before any merge.
New `.cpp` must be registered in `CMakeLists.txt` immediately. No orphan source files.
