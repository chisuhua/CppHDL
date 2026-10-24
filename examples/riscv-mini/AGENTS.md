# AGENTS.md - RISC-V Mini Examples

Child of root `AGENTS.md`.

## OVERVIEW
RV32I 5-stage pipeline 的**示例与测试**。微架构组件本身已收敛到 `include/cpu/`
（本目录只留 SoC 顶层，ADR-046）。Phases 1-3 pass.

## STRUCTURE
```
examples/riscv-mini/
├── CMakeLists.txt
├── rv32i_phase1_test.cpp    # RegFile + ALU + Decoder
├── rv32i_phase2_test.cpp    # Branch + Jump (13/13)
├── rv32i_phase3_test.cpp    # Load/Store (13/13)
├── src/                     # ⚠️ 只剩 **1** 个头（SoC 顶层）
│   └── rv32i_pipeline_top.h # SoC 顶层装配（ADR-046 D1 类别 1 / D3 #15）
│                             #  ⚠️ 当前**零消费者**（唯一消费者 test_pipeline_smoke
│                             #  在 C1 被删除）⇒ 登记在单源化护栏的豁免表，
│                             #  归位条件 = C3（rv32i-soc-dual-parity-2026-10）
└── tests/                   # ✅ 14 个 Catch2 target 已注册进 CTest
    ├── test_*.cpp           # 16 个 test source（另有 8 个已删除，留痕见 removed_examples.txt）
    ├── minitest_prog.h      # 测试辅助头（非 test source）
    └── （Catch2 框架在根 `tests/catch_amalgamated.{cpp,hpp}`，不在本目录）
```

> ⚠️ **2026-10-23（change `riscv-mini-test-triage-2026-10`）**：本目录的 14 个
> Catch2 target **已注册进 CTest**（此前「每次 CI 都编译、却从未被执行」——
> 根因是根 `CMakeLists.txt` 的 `enable_testing()` 排在 `add_subdirectory(examples)`
> 之后 ⇒ examples 子目录的 `add_test()` 是空操作）。
> 护栏：`scripts/verify_example_test_wiring.sh`。

## PASSING TESTS
- Phase 1 (RegFile/ALU/Decoder): ✅ 537 lines test_forwarding, 235 lines test_hazard_complete
- Phase 2 (Branch/Jump): ✅ 13/13 instructions
- Phase 3 (Load/Store): ✅ 13/13 instructions
- cache-complete-rewrite smoke (`riscv_mini_cache_smoke`): ✅ compile probe + 3 PASS instantiate (2026-10-20 `fix-riscv-mini-cache-prefetch-sigsegv` 修复: ch_reg array → ch_mem + ch_module 替换)

## KNOWN ISSUES
- ~~`rv32i_pipeline_cache.h` — I/D-Cache integration chain incomplete~~ —— ⚠️ 该文件**在磁盘上不存在**（零消费者 ⇒ 已于 2026-10-24 按 ADR-046 D4 删除）
- `dynamic_branch_predict.h` — DELETED during code debt cleanup
- `phase3b_complete.cpp` — DELETED (used `ch::log2_ceil` and `ch_array` which don't exist)
- `tests/` subdirectory — ✅ 已注册进 CTest（2026-10-23，14 个 target）；
  `test_csr_unit` 早已以根 `tests/` 的 target 身份存在
- 5-stage pipeline `test_rv32i_pipeline.cpp` — ✅ rewritten, passes (5 tests, 23 assertions)；
  **2026-10-23 起以 `test_rv32i_pipeline_example` 注册进 CTest**（此前从未编译）。
  ⚠️ 它与根 `tests/test_rv32i_pipeline.cpp`（12 用例）**不是**副本关系：前者测
  **前推通路**（`HazardUnit` + `chlib::ForwardingMux` 组合 DUT，见
  `tests/forwarding_dut_fixture.h`），后者测 `hazard_detection_unit.h` 的
  `HazardDetectionUnit`。
  ⚠️ 2026-10-24（ADR-046 D3 #14）：前推数据 mux 从 `HazardUnit` 拆到
  **既有的** `chlib::ForwardingMux` —— 因为 mux 留在 `HazardUnit` 里并接进真实
  流水线装配会形成**组合环**（实测 `FATAL: Cycle detected in IR graph during
  topological sort`）。断言的**期望值不变**。
- `test_interrupt_flow.cpp` — ✅ **2026-10-23 修好并注册进 CTest**（10 用例 / 28 断言；
  覆盖 CLINT 定时器 / CSR 中断条件 / 陷阱入口 / MRET）。修它时挖出
  `include/cpu/riscv/clint.h` 的**真实 RTL bug**：地址解码用 `bits<1,0>` 而非
  `bits<3,2>` ⇒ 4 个寄存器全部别名到 `mtime_lo`（已修 1 行）。
- **`i_cache_prefetch.h` SIGSEGV** ✅ **FIXED** (2026-10-20, change `fix-riscv-mini-cache-prefetch-sigsegv`): (1) `ch_reg<ch_bool>[N]` + runtime init for-loop → `ch_mem<ch_uint<1>, N>` (per ADR-039 + cache-complete-doa-note §6.1 R1); (2) `ICacheComplete icache{this, ...}` 替换为 `ch::ch_module<ICacheComplete>` (per ROOT AGENTS.md "COMPONENT LIFECYCLE PATTERNS"). Smoke test 2 SKIP → 2 PASS instantiate.
- **`d_cache_write_buffer.h` review** (2026-10-20): buffer 是普通 struct 数组 (WriteBufferEntry buffer[4])，**非 ch_reg 数组**, 无根架构问题; instantiate PASS 验证无 SIGSEGV.
- **wrapper 端口契约错误** (2026-10-19) — `i_cache_prefetch.h:91` 引用 `icache.io().hit` (不存在); 已修: `ch_bool(icache.io().miss.impl())`. 同时 `0xFFFFFFF0_d` → `0xFFFFFFF0_h` typo fix.

## WHERE TO LOOK
| Task | Location |
|------|----------|
| Forwarding (源选择) | `include/cpu/pipeline/rv32i_forwarding.h`（`riscv::ForwardingUnit`）|
| Forwarding (数据 mux) | `include/chlib/forwarding.h`（`chlib::ForwardingMux`）|
| Hazard detection | `include/cpu/pipeline/hazard_unit.h` + `hazard_detection_unit.h` |
| Branch predictor | `include/cpu/pipeline/branch_predictor_v2.h` / `rv32i_branch_predict.h` |
| I-Cache wrapper | `include/cpu/cache/i_cache_prefetch.h` (✅ FIXED 2026-10-20) |
| D-Cache wrapper | `include/cpu/cache/d_cache_write_buffer.h` (✅ review PASS 2026-10-20) |
| Pipeline stages | `include/cpu/pipeline/stages/` |
| ALU / RegFile / PC / decoder | `include/cpu/pipeline/{rv32i_alu,rv32i_regfile,rv32i_pc,rv32i_decoder}.h` |
| SoC 顶层（唯一留在本目录的头）| `src/rv32i_pipeline_top.h` |

> ⚠️ **2026-10-24（change `rv32i-canonical-convergence-2026-10`，ADR-046）**：
> 本目录的 `src/` 曾有 **32** 个头，其中 10 组与 `include/cpu/` **同名**
> （消歧纯靠 `-I` 顺序 ⇒ 调序即静默换实现）⇒ 已收敛到**只剩 1 个**
> （SoC 顶层）。所有可复用的微架构组件现在都在 `include/cpu/`。
> 护栏：`scripts/verify_no_duplicate_rv32i_headers.sh`。

## PHASE GATES
Follow root Zero-Debt Policy: **编译通过 + 测试覆盖 + 文档同步**. For riscv-mini:
- `.cpp` registered in `CMakeLists.txt` (no untracked binaries)
- `tests/` subdirectory must be wired to main CMake (delete if not needed)
- **No stubs**: empty test files use `SKIP()` or are removed
- **No dead code**: `phase3b_complete.cpp` must be fully integrated or removed
