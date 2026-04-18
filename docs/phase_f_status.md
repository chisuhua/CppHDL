# Phase F: riscv-tests Pipeline Integration - Status

## Architecture Verification ✅

The hardware pipeline executes and can be observed:

| Check | Status | Evidence |
|-------|--------|----------|
| Build (5-stage + TCM) | ✅ | Compiles with zero errors |
| Context propagation | ✅ | Nested ch_module no longer crashes |
| IR graph construction | ✅ | 871 nodes, no comb explosions |
| Sim tick execution | ✅ | Pipeline ticks (~0.5s/tick) |
| ITCM preload | ✅ | 1042 words loaded to ch_mem init_data |
| DTCM init | ✅ | Zero-initialized at tohost addr |
| Port connectivity | ✅ | dtcm_read_data port accessible |
| Regression integrity | ✅ | 28/28 ported tests pass |

## Blocking Issues 🔴

1. **Simulation speed**: ~500ms/tick. simple.bin (1042 instr, ~300 cycles needed) = 150+ seconds. Not viable for CI.
   
2. **Runtime memory access**: ch_mem writes go to simulator's internal `memory_` vector. `init_data_` (which we preload) is NOT updated by writes. No API to read runtime memory state.

3. **Store instruction path**: Pipeline ticks but store to DTCM doesn't complete in observed ticks. Root cause: EX→MEM pipeline register wiring for `is_store` signal is architecturally correct but timing in simulation mode doesn't complete store within observed window.

## What Works

- Pipeline build with all 5 stages + ITCM/DTCM + HazardUnit ✅
- Nested ch_module context propagation ✅  
- Direct ITCM/DTCM preloading to init_data_ ✅
- Pipeline tick without crashes ✅
- DTCM output port read via `sim.get_port_value()` ✅

## Next Steps

1. Optimize IR evaluation (node caching, eval graph reduction) → 100x speedup needed
2. Add `Simulator::get_memory()` API for runtime memory inspection
3. Verify store execution with instrumented pipeline
