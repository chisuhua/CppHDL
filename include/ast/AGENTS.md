# AGENTS.md - AST Subsystem

Child of: `include/AGENTS.md` → root `AGENTS.md` (ZERO-DEBT POLICY + PHASE GATES).

## OVERVIEW
AST (Abstract Syntax Tree) defines internal structure of HDL designs. Converts C++ hardware descriptions into executable simulation instructions.

## STRUCTURE
```
include/ast/
├── ast_nodes.h       # Core nodes: regimpl, opimpl, selectimpl, memimpl
├── instr_*.h         # Instruction classes: base, clock, io, mem, mux, proxy, reg
├── clockimpl.h       # Clock domain implementation
├── resetimpl.h       # Reset signal implementation
├── memimpl.h         # Memory implementation
└── mem_port_impl.h   # Memory port implementation

src/ast/
├── ast_nodes.cpp     # create_instruction() implementations
├── instr_*.cpp       # Instruction eval() implementations
├── clockimpl.cpp     # Clock domain logic
├── resetimpl.cpp     # Reset logic
└── memimpl.cpp       # Memory operations
```

## KEY FILES
| Task | Location |
|------|----------|
| Add new AST node type | include/ast/ast_nodes.h |
| Add new instruction | include/ast/instr_*.h + src/ast/instr_*.cpp |
| Implement clock domain | include/ast/clockimpl.h + src/ast/clockimpl.cpp |
| Implement reset logic | include/ast/resetimpl.h + src/ast/resetimpl.cpp |
| Implement memory | include/ast/memimpl.h + src/ast/memimpl.cpp |

## CONVENTIONS
- **Namespace**: ch::core for AST implementation classes (regimpl, opimpl, etc.)
- **Forward declarations**: Use in headers to avoid circular dependencies
- **create_instruction()**: Declare in header, implement in cpp file
- **eval()**: Each instruction class implements eval() for simulation
- **Source location**: Use std::source_location for error reporting

## ANTI-PATTERNS
- **Include concrete instr headers**: Never include instr_clock.h, instr_mem.h in other headers. Use forward declarations.
- **Empty create_instruction()**: Must throw or return valid instruction. Never leave unimplemented.
- **Missing cpp implementation**: Every instr_*.h needs corresponding instr_*.cpp with eval().

## RELATED DIRECTORIES
- src/ast/: Contains implementations for all AST node logic
- src/simulator/: Uses AST instructions for simulation execution

## PHASE GATES
Follow root Zero-Debt Policy. AST changes affect codegen and simulation:
- New AST node → add to `ast_nodes.h` + implement `create_instruction()` in `src/ast/ast_nodes.cpp`
- New instruction → `instr_*.h` + `instr_*.cpp` must implement `eval()` method
- Empty `create_instruction()` → throw immediately, never return nullptr
