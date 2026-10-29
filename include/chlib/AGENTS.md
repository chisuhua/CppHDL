# AGENTS.md - CHLib Subsystem

Generated: 2026-03-05 | See parent: `AGENTS.md` (root)

## OVERVIEW
High-level HDL components: streams, FIFOs, pipelines, bundles, arithmetic, memory, AXI4-Lite.

## STRUCTURE
```
include/chlib/
├── arithmetic.h           # Basic arithmetic ops
├── arithmetic_advance.h   # Advanced arithmetic
├── bitwise.h              # Bitwise operations
├── combinational.h        # Combinational logic
├── fifo.h                 # FIFO implementations
├── if.h / if_stmt.h       # Conditional logic
├── logic.h                # Basic logic gates
├── memory.h               # Memory components
├── onehot.h               # One-hot encoding
├── selector_arbiter.h     # Selection/arbitration
├── sequential.h           # Sequential logic
├── state_machine.h        # FSM state machine DSL (ch_state_machine)
├── stream*.h              # Stream operators, pipelines, arbiters
├── switch.h               # Switch/routing logic
└── stream_width_adapter.h # Width conversion
```

## KEY FILES
| Task | File |
|------|------|
| Stream ops | chlib/stream.h, chlib/stream_operators.h |
| FIFO impl | chlib/fifo.h |
| Pipeline | chlib/stream_pipeline.h |
| Arbiter | chlib/selector_arbiter.h, chlib/stream_arbiter.h |
| Width adapter | chlib/stream_width_adapter.h |
| Sequential logic | chlib/sequential.h |
| Memory | chlib/memory.h |
| FSM state machine | chlib/state_machine.h（**契约见 capability `chlib-state-machine-semantics`**） |

## CONVENTIONS
- Namespace: `ch::chlib`
- Stream types: `ch_stream<T>`, `ch_flow<T>`
- Include via `include/chlib.h` (aggregator)
- Files under `bundle/` for stream/bundle types

## ANTI-PATTERNS
- **FSM 的 `on_active` 闭包内写寄存器 / 驱动输出**：`ch_state_machine::build()`
  **遍历所有** state 执行各自闭包 ⇒ 那些写入**必被覆盖**（`ch_reg::next` 是
  last-wins）⇒ 闭包内**只**写 `transition_when`，寄存器与输出写在
  `build()` **之后**（契约：`chlib-state-machine-semantics`；
  护栏：`scripts/verify_fsm_next_wiring.sh`）
- **用三元 `? :` 选择 `transition_when` 的 target**：target MUST 是**编译期常量**
  ⇒ 三元在**建图期**求值 ⇒ 另一个目标状态**永不可达**
- **Async stream patterns**: Not fully supported; avoid complex backpressure without FIFO
- **Stream without termination**: Always connect streams to sink or FIFO
- **Width mismatch**: Stream width adapters required for mismatched widths
- **Blocking in pipelines**: Use combinational/sequential components correctly

## RELATED DIRECTORIES
- tests/chlib/       # chlib component test sources (count in root AGENTS.md NOTES)
- include/bundle/    # Stream and Flow bundle definitions

## PHASE GATES
Follow root Zero-Debt Policy. chlib components are the user-facing API:
- New component → `chlib.h` aggregator update + `tests/chlib/test_*.cpp` test file
- Component uses stream → register in `chlib.h` aggregator, NOT individual include
- Width mismatches → stream_width_adapter required, document in component header
