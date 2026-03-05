# AGENTS.md - Core Subsystem

Child of: [AGENTS.md](../../AGENTS.md)

## OVERVIEW
Core logic layer: node builders, operators, context management, buffers. Provides low-level hardware simulation primitives.

## STRUCTURE
| Category | Files |
|----------|-------|
| Context | context.h, context.cpp |
| Node Builders | node_builder.h, lnodeimpl.h, lnode.h |
| Operators | operators.h, operators_runtime.h |
| Types | types.h, traits.h, uint.h, bool.h |
| Memory | mem.h, reg.h |
| IO | io.h, direction.h |
| Utils | literal.h, logic_buffer.h |

## KEY FILES
| Task | Location |
|------|----------|
| Add logic node | include/core/node_builder.h |
| Implement operator | include/core/operators.h |
| Context management | include/core/context.h |
| Buffer implementation | include/core/logic_buffer.h |
| Context impl | src/core/context.cpp |

## CONVENTIONS
- Context class manages global simulation state
- Node builders use factory pattern for lnode creation
- Operators defined via templates in operators.h
- lnodeimpl.h: runtime polymorphism for nodes
- LogicBuffer: wraps signal propagation with timing

## ANTI-PATTERNS
- **Direct lnode manipulation**: Use node_builder instead of raw lnode
- **Context leaks**: Always pair createContext() with destroyContext()
- **Operator misuse**: Runtime operators in operators_runtime.h differ from compile-time in operators.h
- **Missing context**: Operations require valid context; check ctx validity

## RELATED DIRECTORIES
- src/core/ - Implementation files (context.cpp, lnodeimpl.cpp)
- include/lnode/ - Additional logic node types
- include/bundle/ - Bundle type definitions
