# AGENTS.md - RISC-V Mini Examples

Child of root `AGENTS.md`.

## OVERVIEW
Partial RV32I 5-stage pipeline implementation. Phases 1-3 pass, full pipeline has compile errors.

## STRUCTURE
```
examples/riscv-mini/
├── CMakeLists.txt
├── rv32i_phase1_test.cpp    # RegFile + ALU + Decoder
├── rv32i_phase2_test.cpp    # Branch + Jump (13/13)
├── rv32i_phase3_test.cpp    # Load/Store (13/13)
├── src/                     # Pipeline stages + components
│   ├── forwarding.h         # Data forwarding mux
│   ├── hazard_detection.h   # Hazard detection unit
│   ├── branch_predictor_v2.h # BHT + BTB predictor
│   ├── i_cache_prefetch.h   # I-Cache wrapper (instance ICacheSync, handoff §3.4)
│   ├── d_cache_write_buffer.h # D-Cache wrapper (instance DCacheSync, handoff §3.4)
│   └── stages/              # IF/ID/EX/MEM/WB stage headers
└── tests/                   # ✅ 18 个 Catch2 target 已注册进 CTest
    ├── test_*.cpp           # 19 test files（其中 5 个已删除，见 disabled 清单）
    └── catch_amalgamated.*  # Catch2 framework
```

> ⚠️ **2026-10-23（change `riscv-mini-test-triage-2026-10`）**：本目录的 18 个
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
- `rv32i_pipeline_cache.h` — I/D-Cache integration chain incomplete
- `dynamic_branch_predict.h` — DELETED during code debt cleanup
- `phase3b_complete.cpp` — DELETED (used `ch::log2_ceil` and `ch_array` which don't exist)
- `tests/` subdirectory — ✅ 已注册进 CTest（2026-10-23，18 个 target）；
  `test_csr_unit` 早已以根 `tests/` 的 target 身份存在
- 5-stage pipeline `test_rv32i_pipeline.cpp` — ✅ rewritten, passes (5 tests, 10 assertions)；
  **2026-10-23 起以 `test_rv32i_pipeline_example` 注册进 CTest**（此前从未编译）。
  ⚠️ 它与根 `tests/test_rv32i_pipeline.cpp`（12 用例）**不是**副本关系：前者测
  example 侧**前推启用版** `hazard_unit.h`，后者测 `rv32i_core.h` 的 `HazardDetectionUnit`。
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
| Forwarding logic | `src/forwarding.h` |
| Hazard detection | `src/hazard_detection.h` |
| Branch predictor | `src/branch_predictor_v2.h` |
| I-Cache wrapper | `src/i_cache_prefetch.h` (✅ FIXED 2026-10-20) |
| D-Cache wrapper | `src/d_cache_write_buffer.h` (✅ review PASS 2026-10-20) |
| Pipeline stages | `src/stages/` |

## PHASE GATES
Follow root Zero-Debt Policy: **编译通过 + 测试覆盖 + 文档同步**. For riscv-mini:
- `.cpp` registered in `CMakeLists.txt` (no untracked binaries)
- `tests/` subdirectory must be wired to main CMake (delete if not needed)
- **No stubs**: empty test files use `SKIP()` or are removed
- **No dead code**: `phase3b_complete.cpp` must be fully integrated or removed
