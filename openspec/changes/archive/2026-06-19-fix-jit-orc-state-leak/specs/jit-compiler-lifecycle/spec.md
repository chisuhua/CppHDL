## ADDED Requirements

### Requirement: JitCompiler releases all ORC resources on destruction

The `JitCompiler` class (`include/jit/jit_compiler.h:37`) MUST release all LLVM ORC resources it owns when its destructor runs, including but not limited to: the `ExecutionSession` referenced by `jit_session_`, all `JITDylib` instances registered with that session, and the `llvm::Module` referenced by `llvm_module_`. Released resources MUST NOT be referenced again by any subsequent `JitCompiler` instance within the same process. The destructor MUST be idempotent — multiple invocations (e.g., explicit `clear()` followed by destructor) MUST NOT cause double-free or use-after-free.

#### Scenario: First JitCompiler destruction releases its ORC resources

- **WHEN** a `ch_device<T>` instance goes out of scope and triggers `Simulator::~Simulator` which triggers `JitCompiler::~JitCompiler`
- **THEN** the underlying `llvm::orc::ExecutionSession` is ended (`endSession()`) and all owned `JITDylib` instances are disposed
- **AND** the `jit_session_` and `llvm_module_` opaque pointer fields are reset to `nullptr` to prevent double-release

#### Scenario: Cross-instance JIT state is isolated after destruction

- **WHEN** a second `ch_device<T>` is instantiated in the same process after the first `ch_device<T>` has been destroyed
- **THEN** the second instance's JIT compile path produces a fresh `ExecutionSession` and `JITDylib` with no symbols, layers, or cached state carried over from the first instance
- **AND** the second instance's JIT `sim_us` for TC-08 (ch_reg chain) is no more than 1.5× the standalone JIT baseline (4-4.5k us) per the `test_jit_orc_state_isolation` Catch2 test

### Requirement: Simulator explicitly resets JitCompiler before disconnect

The `Simulator` class (`src/simulator.cpp`) MUST explicitly invoke `jit_compiler_.reset()` near the top of its destructor, before calling `disconnect()` and before any other teardown steps that may touch `data_map_` or the LLVM context. This ordering guarantees the ORC layer is fully torn down while the `data_map_` and `LLVMContext` referenced by JIT-compiled code are still in a valid state.

#### Scenario: Simulator destructor orders teardown correctly

- **WHEN** a `Simulator` instance is destroyed
- **THEN** `jit_compiler_.reset()` is called first, triggering `JitCompiler::~JitCompiler` and the ORC resource release
- **AND** `disconnect()` and trace buffer cleanup occur after the JIT reset
- **AND** no `use-after-free` is observed in the next `ch_device<T>` instantiation in the same process

### Requirement: JitCompiler is single-instance per Simulator

Within a single `Simulator` instance lifetime, there MUST be at most one `JitCompiler` instance (`Simulator::jit_compiler_`). Multiple `Simulator` instances within the same process MUST each own an independent `JitCompiler` whose ORC resources do not alias. The `JitCompiler` MUST NOT be a process-wide singleton (it is owned by `Simulator` via `std::unique_ptr<JitCompiler>`).

#### Scenario: Multiple Simulator instances have independent JIT state

- **WHEN** two `Simulator` instances are created sequentially in the same process (e.g., `perf_main` running TC-07 then TC-08 then TC-09)
- **THEN** each `Simulator` owns a separate `JitCompiler` with its own `ExecutionSession` and `JITDylib`
- **AND** the `JITDylib` of the second `Simulator` does not contain symbols registered by the first `Simulator`'s `JITDylib`
- **AND** no symbol lookup in the second `Simulator` resolves to a function compiled by the first `Simulator`

### Requirement: JitCompiler clear() is callable multiple times safely

The `JitCompiler::clear()` public method (`src/jit/jit_compiler.cpp:167`) MUST be safely callable multiple times in succession without resource leaks or double-free. After the first call, the `JitCompiler` MUST be in a state where it can be reused by a subsequent `compile()` call (allocating fresh ORC resources), or safely destroyed.

#### Scenario: clear() followed by compile() produces a working compiler

- **WHEN** a `JitCompiler::clear()` call is followed by `JitCompiler::compile(ctx)` on the same instance
- **THEN** the `compile()` succeeds and produces a working `compiled_comb_func_` / `compiled_seq_func_`
- **AND** no stale `JITDylib` from the previous `compile()` cycle is referenced

### Requirement: JIT state isolation is verified by automated regression test

The repository MUST include a Catch2 test `tests/jit/test_jit_orc_state_isolation.cpp` that verifies the cross-`ch_device<T>` JIT state isolation guarantee. The test MUST instantiate two distinct `ch_device<T>` instances sequentially in the same `TEST_CASE`, run JIT benchmarks on each, and assert that the second instance's JIT `sim_us` is no more than 1.5× the first instance's JIT `sim_us` (i.e., no 100× pollution). The test MUST be registered in `ctest -L jit` and MUST pass on every CI run.

#### Scenario: test_jit_orc_state_isolation detects the K1 pollution

- **WHEN** the regression test runs on a build where the ORC resources are not properly released (e.g., the bug described in `docs/simulation/PERF_COMPARISON_REPORT.md §6 K1`)
- **THEN** the second `ch_device<T>`'s JIT `sim_us` is more than 1.5× the first `ch_device<T>`'s JIT `sim_us`
- **AND** the test FAILS with a clear message indicating the K1 pollution regression

#### Scenario: test_jit_orc_state_isolation passes after the fix

- **WHEN** the regression test runs on a build where `JitCompiler::clear()` properly releases ORC resources and `Simulator::~Simulator` explicitly resets `jit_compiler_`
- **THEN** the second `ch_device<T>`'s JIT `sim_us` is at most 1.5× the first `ch_device<T>`'s JIT `sim_us`
- **AND** the test PASSES, locking in the lifecycle contract
