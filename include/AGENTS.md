# AGENTS.md - HDL Library Headers

Child of root `AGENTS.md`. Covers all under `include/`.

## OVERVIEW
Header-only HDL library: core primitives, AST definitions, component library, bundles, AXI, CPU.

## STRUCTURE
```
include/
├── ch.hpp              # Aggregator: core + types (use in examples/tests)
├── chlib.h             # Aggregator: chlib components (arithmetic, stream, fifo...)
├── bundle.h            # Bundle base template
├── component.h         # Component base class
├── device.h            # Device wrapper for simulation
├── simulator.h         # Simulator main interface
├── codegen_verilog.h   # Verilog code generator
├── codegen_dag.h       # DAG code generator
├── module.h            # Module template
├── core/               # Core primitives (AGENTS.md)
├── ast/                # AST nodes (AGENTS.md)
├── chlib/              # HDL components (AGENTS.md)
├── bundle/             # Bundle type defs (stream, axi, flow)
├── axi4/               # AXI4 interconnect + peripherals
├── cpu/                # RV32I core, cache
├── lnode/              # Logic node impl types
├── bv/                 # Bit-vector math (gmp/mpfr)
└── abstract/           # Abstract base classes
```

## CONVENTIONS
- **Public entry**: `#include "ch.hpp"` (core) or `#include "chlib.h"` (components)
- **Include guards**: `#pragma once` throughout
- **Namespace**: `ch::core` for core, `ch::chlib` for components, bare `ch` for API
- **Header-only**: All headers are self-contained; impl in `src/` for codegen/simulator

## ANTI-PATTERNS
- Include individual `include/core/*.h` from examples — use `ch.hpp` instead
- Include individual `include/chlib/*.h` from examples — use `chlib.h` or `ch.hpp`
- Define new AST nodes without registering in `create_instruction()`

## PHASE GATES
Follow root AGENTS.md: **编译通过 + 测试覆盖 + 文档同步** before any merge. New headers must be self-contained, self-testable, and have a corresponding usage example. **No orphan headers** — every `.h` must be included by at least one `.cpp` or aggregator.
