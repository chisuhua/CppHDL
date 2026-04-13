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
├── phase3b_complete.cpp      # ⚠️ Source exists, NOT in CMake (API changes)
├── src/                     # Pipeline stages + components
│   ├── forwarding.h         # Data forwarding mux
│   ├── hazard_detection.h   # Hazard detection unit
│   ├── branch_predictor_v2.h # BHT + BTB predictor
│   └── stages/              # IF/ID/EX/MEM/WB stage headers
└── tests/                   # ⚠️ CMakeLists NOT registered (isolated)
    ├── test_*.cpp           # 9 test files (forwarding, hazard, cache, etc.)
    └── catch_amalgamated.*  # Catch2 framework
```

## PASSING TESTS
- Phase 1 (RegFile/ALU/Decoder): ✅ 537 lines test_forwarding, 235 lines test_hazard_complete
- Phase 2 (Branch/Jump): ✅ 13/13 instructions
- Phase 3 (Load/Store): ✅ 13/13 instructions

## KNOWN ISSUES
- `rv32i_pipeline_cache.h` — I/D-Cache integration chain incomplete
- `dynamic_branch_predict.h` — DELETED during code debt cleanup
- `phase3b_complete.cpp` — Compiles but not registered
- `tests/` subdirectory — CMake not wired to main build
- 5-stage pipeline `test_rv32i_pipeline.cpp` — 46 compile errors (needs fix)

## WHERE TO LOOK
| Task | Location |
|------|----------|
| Forwarding logic | `src/forwarding.h` |
| Hazard detection | `src/hazard_detection.h` |
| Branch predictor | `src/branch_predictor_v2.h` |
| Pipeline stages | `src/stages/` |

## PHASE GATES
Follow root Zero-Debt Policy: **编译通过 + 测试覆盖 + 文档同步**. For riscv-mini:
- `.cpp` registered in `CMakeLists.txt` (no untracked binaries)
- `tests/` subdirectory must be wired to main CMake (delete if not needed)
- **No stubs**: empty test files use `SKIP()` or are removed
- **No dead code**: `phase3b_complete.cpp` must be fully integrated or removed
