# AGENTS.md - CppHDL Samples Subsystem

See [root AGENTS.md](../AGENTS.md) for project overview and shared conventions.

## OVERVIEW

Demonstrates CppHDL usage: HDL design patterns, IO protocols, simulation, stream/bundle APIs.

## STRUCTURE

All samples are flat files at `samples/*.cpp` (no subdirectories).

Key files:

| Category | Files |
|----------|-------|
| Counters | counter.cpp, feedback_counter.cpp |
| FIFOs | fifo_example.cpp, fifo_bundle.cpp, minimal_fifo_bundle.cpp |
| Pipelines | pipeline.cpp, shift_register.cpp |
| Bundles | bundle_demo.cpp, bundle_advanced_features.cpp, nested_bundle_demo.cpp |
| Streams | stream_fifo_example.cpp, stream_arbiter_example.cpp, stream_fork_example.cpp |
| Protocols | axi_lite_demo.cpp, simple_io.cpp |
| Control | conditional_demo.cpp, if_demo.cpp, switch_demo.cpp |

## CONVENTIONS

- Each sample has standalone main() function
- Samples build to separate executables in build/samples/
- Naming: `<feature>_demo.cpp` or `<feature>_example.cpp`
- Samples demonstrate one concept per file
- Use ch:: types (bit<N>, int<N>, uint<N>) for signals
- Run: `./build/samples/<sample_name>`

### 自动化回归护栏（2026-10-22 起）

- **每个 sample MUST 以 0 退出**（含 segfault 与超时）⇒ 由
  `bash scripts/run_all_samples.sh` 守护（CI job `samples-regression`，
  **Linux-only**：macOS 无 GNU `timeout`，脚本内做 `timeout` → `gtimeout` 回退）。
- **新增 sample MUST 同时注册进 `samples/CMakeLists.txt` 的 `CH_SAMPLE_TARGETS`**，
  否则护栏的 **C1 注册一致性**会失败。⇒ 现状：**40 个源文件 = 37 已注册 +
  3 已禁用**（`samples/disabled_samples.txt`；⚠️ **2026-10-31** `if_demo` 归位
  ⇒ 36 → **37** / 4 → **3**）。
- **禁用清单**（`samples/disabled_samples.txt`）每行格式
  `<sample 名> <TAB> 为什么不能编 <TAB> 归位条件` ⇒ 「为什么它没被编译」不再失传。
  ⚠️ 本清单只做**登记**，不负责修复；归位条件达成后 MUST 移除该行并补进
  `CH_SAMPLE_TARGETS`。
- **节点创建需要活跃 context**：任何 `make_uint` / `case_` / `switch_` 等建节点的
  调用，MUST 在 `ch::core::context` + `ch::core::ctx_swap` 作用域内
  （否则 `node_builder` 报「No active context for literal creation」并返回
  nullptr ⇒ 后续解引用 segfault）。范例：`samples/switch_demo.cpp`、
  `samples/axi_lite_demo.cpp`。
- **豁免不是静默的**：已知失败的 sample 登记在护栏脚本的 `KNOWN_FAILING` 里，
  以 `::notice` 点名；豁免项**仍会被执行**，且一旦转绿 ⇒ 报 **stale** 并失败。

## RELATED DIRECTORIES

- build/samples/: Compiled sample executables
- docs/: Additional usage guides
- tests/: Unit tests for core functionality

## PHASE GATES
Follow root Zero-Debt Policy. Samples serve as living documentation:
- Each sample must compile and run (`./build/samples/<name>` — no segfaults)
- One concept per file, naming: `<feature>_demo.cpp` or `<feature>_example.cpp`
- Samples are NOT tests — Catch2 tests go in `tests/`
