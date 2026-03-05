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
| AXI4-Lite | chlib/axi4lite.h |

## CONVENTIONS
- Namespace: `ch::chlib`
- Stream types: `ch_stream<T>`, `ch_flow<T>`
- Include via `include/chlib.h` (aggregator)
- Files under `bundle/` for stream/bundle types

## ANTI-PATTERNS
- **Async stream patterns**: Not fully supported; avoid complex backpressure without FIFO
- **Stream without termination**: Always connect streams to sink or FIFO
- **Width mismatch**: Stream width adapters required for mismatched widths
- **Blocking in pipelines**: Use combinational/sequential components correctly

## RELATED DIRECTORIES
- tests/chlib/           # 17 test files for chlib components
- include/bundle/        # Stream and Flow bundle definitions
