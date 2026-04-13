# AGENTS.md - CppHDL Samples Subsystem

See [root AGENTS.md](../AGENTS.md) for project overview and shared conventions.

## OVERVIEW

Demonstrates CppHDL usage: HDL design patterns, IO protocols, simulation, stream/bundle APIs.

## STRUCTURE

```
samples/
├── bundles/           # Bundle type examples
├── streams/           # Stream/flow examples
├── fifos/             # FIFO implementations
├── pipelines/         # Pipeline/register examples
├── protocols/         # IO protocol demos
└── *.cpp              # Standalone examples
```

## KEY FILES

| Category | Files |
|----------|-------|
| Counters | counter.cpp, feedback_counter.cpp |
| FIFOs | fifo_example.cpp, fifo_bundle.cpp, minimal_fifo_bundle.cpp |
| Pipelines | pipeline.cpp, shift_register.cpp |
| Bundles | bundle_demo.cpp, bundle_advanced_features.cpp, nested_bundle_demo.cpp, large_pod_bundle_example.cpp |
| Streams | stream_fifo_example.cpp, stream_arbiter_example.cpp, stream_fork_example.cpp, stream_handshake_example.cpp |
| Protocols | axi_lite_demo.cpp, simple_io.cpp |
| Control | conditional_demo.cpp, if_demo.cpp, switch_demo.cpp |
| SpinalHDL compat | spinalhdl_stream_example.cpp, spinalhdl_component_function_area_example.cpp |

## CONVENTIONS

- Each sample has standalone main() function
- Samples build to separate executables in build/samples/
- Naming: `<feature>_demo.cpp` or `<feature>_example.cpp`
- Samples demonstrate one concept per file
- Use ch:: types (bit<N>, int<N>, uint<N>) for signals
- Run: `./build/samples/<sample_name>`

## RELATED DIRECTORIES

- build/samples/: Compiled sample executables
- docs/: Additional usage guides
- tests/: Unit tests for core functionality

## PHASE GATES
Follow root Zero-Debt Policy. Samples serve as living documentation:
- Each sample must compile and run (`./build/samples/<name>` — no segfaults)
- One concept per file, naming: `<feature>_demo.cpp` or `<feature>_example.cpp`
- Samples are NOT tests — Catch2 tests go in `tests/`
