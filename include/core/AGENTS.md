# AGENTS.md - Core Subsystem

Child of: `include/AGENTS.md` → root `AGENTS.md` (ZERO-DEBT POLICY + PHASE GATES). Core is the foundation of all HDL types; new primitives must not break existing patterns.

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
- **Context leaks**: Always pair context creation and destruction via the CppHDL context lifecycle APIs defined in context.h
- **Operator misuse**: Runtime operators in operators_runtime.h differ from compile-time in operators.h
- **Missing context**: Operations require a valid Context instance; acquire and release it via the same lifecycle APIs

## RELATED DIRECTORIES
- src/core/ - Implementation files (context.cpp, lnodeimpl.cpp)
- include/lnode/ - Additional logic node types
- include/bundle/ - Bundle type definitions

## PHASE GATES
Follow root Zero-Debt Policy. Core changes affect everything downstream; extra caution:
- New primitives → update `ch.hpp` aggregator + add test in `tests/test_*.cpp`
- API changes → update existing usage examples, verify no compile regressions
- Operators → both compile-time (`operators.h`) and runtime (`operators_runtime.h`) must stay in sync
