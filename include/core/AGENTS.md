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
- **端口具名（`ch_in` / `ch_out` 成员）**：MUST 用**直接初始化 + 名字串**形态
  `ch_in<ch_bool> prog_we{"prog_we"};` ⇒ ⇒ **拷贝初始化形态 `= "name"` 编译不过**
  （`port` 的具名构造是 `explicit`）⇒ ⇒ 且**名字串 MUST 与标识符逐字相同**
  （不一致时**名字串胜出** ⇒ ⇒ `cpp_member` 会与端口名对不上 ⇒ ⇒ 静态护栏 P1 拦）
  ⇒ ⇒ 组件**必须** override `create_ports()` 并 `new (io_storage_) io_type;`
  ⇒ ⇒ **完整约定 + 已知限制 + 验证命令** = `docs/developer_guide/patterns/PORT-NAMING-GUIDE.md`
  ⇒ ⇒ 端到端探针 = `tests/test_port_name_named_ctor.cpp`
  ⚠️ **这条路径此前零调用点**（2026-11-11 首次端到端验证）⇒ ⇒ 新增具名端口 MUST 跑该探针

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
