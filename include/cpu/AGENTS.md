# AGENTS.md - CPU / RISC-V Subsystem

Child of `include/AGENTS.md` → root `AGENTS.md` (ZERO-DEBT POLICY + PHASE GATES).

## OVERVIEW
RV32I 5-stage pipeline + cache hierarchy + forwarding + hazard detection + SoC integration.
22 files, ~5,600 lines. All header-only.

## STRUCTURE
```
include/cpu/
├── cache/                            # ch_mem-based cache (ADR-039, ADR-041)
│   ├── i_cache_sync.h / d_cache_sync.h       # 1-cycle sread — RV32I L1 默认
│   ├── i_cache_async.h / d_cache_async.h     # 0-cycle aread — TCM-style 备选
│   ├── i_cache_complete.h / d_cache_complete.h  # 兼容 shim (= *_sync.h 别名)
│   ├── i_cache_prefetch.h                    # riscv-mini wrapper (instance ICacheSync)
│   └── d_cache_write_buffer.h                # riscv-mini wrapper (instance DCacheSync)
├── pipeline/
│   ├── rv32i_decoder.h            # R/I/S/B/U/J format decode
│   ├── rv32i_pipeline_regs.h       # IF/ID/EX/MEM/WB regs (490 lines — largest)
│   ├── rv32i_pipeline.h            # 5-stage top-level
│   ├── rv32i_tcm.h                # Tightly-coupled memory (ITCM/DTCM)
│   ├── hazard_unit.h               # Port-based hazard (riscv namespace)
│   └── stages/{if,id,ex,mem,wb}_stage.h
└── riscv/
    ├── rv32i_control.h             # Control signal generation
    ├── rv32i_csr.h                 # CSR bank (9 registers, 252 lines)
    ├── rv32i_exception.h          # Trap entry + MRET
    ├── rv32i_system.h              # SYSTEM instr (CSRRW/CSRRS/MRET/ECALL)
    ├── address_decoder.h          # MMIO 6-region decoder
    ├── clint.h                    # Core Local Interrupt Controller
    ├── firmware_loader.h          # Intel HEX loader (non-HDL)
    └── rv32i_soc.h                # Full SoC integration (359 lines)
```

## NAMESPACE MAP
| Namespace | Files | Purpose |
|-----------|-------|---------|
| `riscv` | `pipeline/stages/*.h`, `riscv/*.h` | Pipeline stages, decoder, CSR, exception |
| `chlib` | `cache/*.h` | Cache controllers (`i_cache_*`, `d_cache_*`) — forwarding.h moved to `include/chlib/` per ADR-038 |
| `cpu` | (no in-tree headers under this namespace) | Historic placeholders `branch_predictor/*.h` and `hazard/*.h` were removed; the live bundle interfaces live under `examples/riscv-mini/src/` and are not part of the main `include/cpu/` graph |
| `ch::chlib` | `clint.h` | CLINT timer |

## KEY FILES
| Task | File |
|------|------|
| 5-stage pipeline | `pipeline/rv32i_pipeline.h` |
| Pipeline registers | `pipeline/rv32i_pipeline_regs.h` (490 lines) |
| Instruction decode | `riscv/rv32i_decoder.h` |
| CSR registers | `riscv/rv32i_csr.h` |
| Exception handling | `riscv/rv32i_exception.h` |
| Full SoC | `riscv/rv32i_soc.h` |
| I-Cache (1-cycle, RV32I L1 默认) | `cache/i_cache_sync.h` (`i_cache_complete.h` 是兼容 shim 别名) |
| I-Cache (0-cycle, TCM-style) | `cache/i_cache_async.h` |
| D-Cache (1-cycle, RV32I L1 默认) | `cache/d_cache_sync.h` (`d_cache_complete.h` 是兼容 shim 别名) |
| D-Cache (0-cycle, TCM-style) | `cache/d_cache_async.h` |
| I-Cache wrapper (riscv-mini) | `cache/i_cache_prefetch.h` (instance ICacheSync) |
| D-Cache wrapper (riscv-mini) | `cache/d_cache_write_buffer.h` (instance DCacheSync) |

## CONVENTIONS
- **Pipeline instantiation**: `ch::ch_module<T>` inside `Component::describe()` — see root AGENTS.md
- **Runtime**: Pipeline requires `ch::ch_device<Rv32iSoc>` wrapper in main() (per README)
- **Bundle vs Port**: Pipeline uses port-based `hazard_unit.h` (riscv namespace), NOT `hazard_unit_bundle.h` (cpu namespace)
- **TCM**: `rv32i_tcm.h` provides single-cycle instruction/data memories
- **Cache variant selection** (ADR-041):
  - **RV32I L1 imem/dmem** (默认) → `ICacheSync` / `DCacheSync` (1-cycle sread)
  - **软核 TCM-style** (低频, 延迟关键) → `ICacheAsync` / `DCacheAsync` (0-cycle aread)
  - **imem 必须 sread** (per `verilator-e2e-coverage` spec §R1 invariant 3): 不要在 RV32I imem 用 Async

## ANTI-PATTERNS
- **Namespace inconsistency**: CPU components split across 4 namespaces (`riscv`, `chlib`, `cpu`, `ch::chlib`)
- **Duplicate forwarding**: `include/chlib/forwarding.h` vs `include/cpu/pipeline/hazard_unit.h` — overlapping functionality (forwarding mover per ADR-038; merge deferred to a future change)

## TESTS
Active CPU / RV32I test targets registered in `tests/CMakeLists.txt`
(the `test_interrupt_flow` row below is registered in
`examples/riscv-mini/CMakeLists.txt` instead):

| Target | Source path | Focus |
|--------|-------------|-------|
| `test_forwarding` | `tests/chlib/test_forwarding.cpp` | `chlib::ForwardingMux` + IfIdRegs struct field access |
| `test_hazard` | `tests/chlib/test_hazard_complete.cpp` | Hazard detection + forwarding |
| `test_rv32i_pipeline` | `tests/test_rv32i_pipeline.cpp` | 5-stage pipeline integration (Catch2) |
| `test_csr_unit` | `examples/riscv-mini/tests/test_csr_unit.cpp` | CSR register reset, R/W, MRET, exceptions, WFI, interrupts |
| `test_interrupt_flow` | `examples/riscv-mini/tests/test_interrupt_flow.cpp` | CLINT 定时器 / CSR 中断条件 / 陷阱入口 / MRET（10 用例 / 28 断言；2026-10-23 注册进 CTest，此前从未编译） |
| `test_phase4_chlib` | `tests/chlib/test_phase4_integration.cpp` | Phase-4 chlib / CPU integration smoke (cache 用 `ICacheSync` / `DCacheSync` + hazard) |

`examples/riscv-mini/tests/` 的 Catch2 测试 target 自 2026-10-23 起
**已注册进 CTest**（change `riscv-mini-test-triage-2026-10`，**14** 个 target；
另有 1 个 `riscv_mini_catch_test_noctest` 只构建不注册 ⇒ 共 15 个声明），
由 `examples/riscv-mini/CMakeLists.txt` 的 `riscv_mini_catch_test()` helper
统一 `add_test`。⚠️ 此前它们「每次 CI 都编译、却从未被执行」——根因是根
`CMakeLists.txt` 的 `enable_testing()` 排在 `add_subdirectory(examples)` **之后**
⇒ examples 子目录的 `add_test()` 是空操作。护栏：
`scripts/verify_example_test_wiring.sh`（归属守恒 + 数量守恒）。

其余 `examples/riscv-mini/src/` 下的头文件仍**不在**本目录的包含图里
（它们是 example 专属件或待收敛对象，见 ADR-046 `rv32i-canonical-home`）。

## PHASE GATES
Follow root Zero-Debt Policy: **编译通过 + 测试覆盖 + 文档同步**.
Pipeline phases (1/2/3) pass — full integration runtime requires `ch_device` wrapper.