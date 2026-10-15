# AGENTS.md - CPU / RISC-V Subsystem

Child of `include/AGENTS.md` → root `AGENTS.md` (ZERO-DEBT POLICY + PHASE GATES).

## OVERVIEW
RV32I 5-stage pipeline + cache hierarchy + forwarding + hazard detection + SoC integration.
22 files, ~5,600 lines. All header-only.

## STRUCTURE
```
include/cpu/
├── forwarding.h                    # ForwardingUnit + Mux (namespace chlib ⚠️ orphan)
├── branch_predictor/
│   └── branch_predictor_bundle.h   # Bundle interface (namespace cpu)
├── hazard/
│   └── hazard_unit_bundle.h        # Bundle interface (namespace cpu)
├── cache/
│   ├── i_cache.h / i_cache_complete.h / i_cache_prefetch.h
│   └── d_cache.h / d_cache_complete.h / d_cache_write_buffer.h
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
| `chlib` | `forwarding.h`, `cache/*.h` | Forwarding unit, cache controllers |
| `cpu` | `branch_predictor/*.h`, `hazard/*.h` | Bundle interfaces (unused by pipeline) |
| `ch::chlib` | `clint.h` | CLINT timer |

⚠️ **Note**: `forwarding.h` lives in `include/cpu/` but uses `chlib` namespace — likely should be in `include/chlib/`.

## KEY FILES
| Task | File |
|------|------|
| 5-stage pipeline | `pipeline/rv32i_pipeline.h` |
| Pipeline registers | `pipeline/rv32i_pipeline_regs.h` (490 lines) |
| Instruction decode | `riscv/rv32i_decoder.h` |
| CSR registers | `riscv/rv32i_csr.h` |
| Exception handling | `riscv/rv32i_exception.h` |
| Full SoC | `riscv/rv32i_soc.h` |
| I-Cache | `cache/i_cache_complete.h` |
| D-Cache | `cache/d_cache_complete.h` |

## CONVENTIONS
- **Pipeline instantiation**: `ch::ch_module<T>` inside `Component::describe()` — see root AGENTS.md
- **Runtime**: Pipeline requires `ch::ch_device<Rv32iSoc>` wrapper in main() (per README)
- **Bundle vs Port**: Pipeline uses port-based `hazard_unit.h` (riscv namespace), NOT `hazard_unit_bundle.h` (cpu namespace)
- **TCM**: `rv32i_tcm.h` provides single-cycle instruction/data memories

## ANTI-PATTERNS
- **Namespace inconsistency**: CPU components split across 4 namespaces (`riscv`, `chlib`, `cpu`, `ch::chlib`)
- **Duplicate forwarding**: `forwarding.h` (chlib) vs `hazard_unit.h` (riscv) — overlapping functionality
- **Orphan forwarding**: `forwarding.h` in `include/cpu/` but uses `chlib` namespace — should be in `include/chlib/`

## TESTS
Active CPU / RV32I test targets registered in `tests/CMakeLists.txt`:

| Target | Source path | Focus |
|--------|-------------|-------|
| `test_forwarding` | `tests/chlib/test_forwarding.cpp` | `chlib::ForwardingMux` + IfIdRegs struct field access |
| `test_hazard` | `tests/chlib/test_hazard_complete.cpp` | Hazard detection + forwarding |
| `test_rv32i_pipeline` | `tests/test_rv32i_pipeline.cpp` | 5-stage pipeline integration (Catch2) |
| `test_csr_unit` | `examples/riscv-mini/tests/test_csr_unit.cpp` | CSR register reset, R/W, MRET, exceptions, WFI, interrupts |
| `test_phase4_chlib` | `tests/chlib/test_phase4_integration.cpp` | Phase-4 chlib / CPU integration smoke (cache stubs + pipeline) |

`examples/riscv-mini/tests/` contains additional exploratory scripts
(`test_pipeline_debug*.cpp`, `test_pipeline_minimal.cpp`, etc.) that
are **not** registered in `tests/CMakeLists.txt` — they are kept as
runnable prototypes only. Run them with `cmake --build build --target
<name>` after wiring the source files in `examples/riscv-mini/CMakeLists.txt`,
or directly via `g++ -std=c++23 -Iinclude -Iexamples/riscv-mini/src`.

## PHASE GATES
Follow root Zero-Debt Policy: **编译通过 + 测试覆盖 + 文档同步**.
Pipeline phases (1/2/3) pass — full integration runtime requires `ch_device` wrapper.