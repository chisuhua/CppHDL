# FreeRTOS Support Implementation Plan for riscv-mini

**Version**: 2.0 — Zero-Debt Enforced  
**Created**: 2026-04-14  
**Revised**: 2026-04-14 (Metis + Momus review applied)  
**Status**: Ready for Execution  
**Total Duration**: 22-27工作日 (Phase A→D.5 mandatory, E optional)

---

## Zero-Debt Policy (MANDATORY)

**EVERY phase MUST exit with ZERO technical debt before advancing:**

| Gate | Criteria | Command | Block? |
|------|----------|---------|--------|
| 🟢 **编译** | 0 errors, 0 warnings on new files | `cmake --build build` clean | Phase cannot proceed |
| 🟢 **测试** | All existing + new tests pass | `ctest --output-on-failure` 100% | Phase cannot proceed |
| 🟢 **文档** | AGENTS.md updated, inline comments for new APIs | Manual review + grep | Phase cannot proceed |
| 🟢 **清洁** | 0 TODO/FIXME in new code | `grep -r "TODO\|FIXME" src/` → empty | Phase cannot proceed |
| 🟢 **沉淀** | Phase artifact saved to `.sisyphus/artifacts/` | File exists with lessons learned | Phase cannot proceed |

**No exceptions.** No "we'll fix it later." No "test coverage pending."

---

## Skill Extraction Framework

After each phase gate passes, produce a summary artifact:

| Phase | Artifact File | Content |
|-------|--------------|---------|
| 0.5 | `.sisyphus/artifacts/debug-infrastructure-pattern.md` | VCD workflow, register snapshot API |
| A | `.sisyphus/artifacts/csr-implementation-pattern.md` | CSR register map, bit-field accessors, test vectors |
| B | `.sisyphus/artifacts/pipeline-integration-pattern.md` | Stage wiring, forwarding paths, hazard debugging |
| C | `.sisyphus/artifacts/trap-handling-pattern.md` | Trap entry/exit sequence, MRET semantics |
| D | `.sisyphus/artifacts/soc-integration-pattern.md` | Address decode, peripheral wiring, SoC testing |
| D.5 | `.sisyphus/artifacts/riscv-tests-integration.md` | Toolchain setup, test suite execution, tohost protocol |
| E | `.sisyphus/artifacts/freertos-riscv-porting.md` | port.c analysis, assembly handlers, config macros |

**Artifact template:**
```markdown
# {Pattern Name}
## Context: when to use
## Structure: components + data flow
## Implementation: step-by-step with code
## Test Vectors: happy path + edge cases
## Known Failure Modes: what went wrong
## Related Patterns: cross-references
```

---

## Context

### User Request Summary
Generate a detailed, step-by-step implementation plan for adding FreeRTOS RTOS support to the riscv-mini RV32I CPU simulator in CppHDL. The plan must align with existing design documentation in `docs/freertos/` and provide actionable tasks with clear dependencies, delegation strategies, and verification criteria.

### Current State Summary

**What EXISTS and PASSES:**
- ✅ Phase 1 (RegFile + ALU + Decoder): 537 lines test_forwarding, 235 lines test_hazard_complete
- ✅ Phase 2 (Branch + Jump): 13/13 instructions passing
- ✅ Phase 3 (Load/Store): 13/13 instructions passing
- ✅ 5-stage pipeline test: 5 tests, 10 assertions in `tests/test_rv32i_pipeline.cpp`
- Source files in `examples/riscv-mini/src/`: 17 header files covering ALU, decoder, regfile, pipeline stages, hazard detection, forwarding, branch predictor, I/D-TCM

**What is MISSING (needs implementation):**
- ❌ CSR register bank (Phase A)
- ❌ SYSTEM instructions (csrrw/csrrs/csrrc/mret/ecall/ebreak/wfi) — 11 instructions
- ❌ Pipeline has 4 critical bugs requiring full redesign (line 121 PC≠instruction, line 131 register data as PC, line 166-168 hazard detection broken, line 147 memory always ready)
- ❌ CLINT interrupt controller (Phase C)
- ❌ Exception/trap handling unit (Phase C)
- ❌ Address decoder for MMIO (Phase D)
- ❌ UART peripheral model (Phase D)
- ❌ HEX/Binary firmware loader (Phase D)
- ❌ SoC integration (Phase D)
- ❌ FreeRTOS port (Phase E — optional)
- ❌ riscv-tests ISA suite integration (Phase D.5 — optional but recommended)

### Key Design Documents Reference

| Document | Purpose | Key Specs |
|----------|---------|-----------|
| `docs/freertos/01-specification.md` | RV32I + SYSTEM instruction spec, CSR map, CLINT, memory map | 51 instructions total, 10 CSR registers, 4-cycle interrupt latency |
| `docs/freertos/02-architecture.md` | SoC architecture: 5-stage pipeline, CSR unit, CLINT, UART | Direct MMIO decode (no AXI4-Lite initially), 64KB I/D-TCM |
| `docs/freertos/03-implementation-plan.md` | 5-phase implementation plan | Phase A: 4-5d, B: 5-7d, C: 3-4d, D: 3-4d, D.5: 1-2d, E: 7-14d (optional) |
| `docs/freertos/04-verification-strategy.md` | 5-level test hierarchy | Unit → Integration → SoC → System → FreeRTOS boot |
| `docs/freertos/05-freertos-porting-guide.md` | FreeRTOSConfig.h, port.c, portASM.S | configCPU_CLOCK_HZ=50MHz, configTICK_RATE_HZ=1kHz |

### Critical Constraints from Design Review

1. **Pipeline Redesign Required**: Phase B needs complete rewrite, not incremental fixes (4 critical bugs identified by Momus review) — **SPLIT into B1a/B1b/B1c sub-tasks with VCD checkpoints**
2. **CSR Independence**: Phase A (CSR) can be developed independently of Phase B (Pipeline)
3. **Phase C Dependency**: C2 requires both A2 (CSR registers) AND B1 (pipeline structure) for trap entry logic
4. **Phase D Gate**: All of A, B, C must be complete
5. **Phase D.5 MANDATORY**: riscv-tests ISA suite is **REQUIRED** (not optional) for CPU correctness proof — Phase E cannot start without D.5 passing
6. **Phase E Optional**: FreeRTOS is demo goal only
7. **HEX Priority**: Use Intel HEX format for Phase D, defer ELF support to Phase F
8. **Debug Infrastructure FIRST**: Phase 0.5 (VCD + register snapshot) must complete before Wave 1
9. **D2 uses existing file**: `include/axi4/peripherals/axi_uart.h` (146 lines) already exists — MODIFY, don't create new
10. **D.5 toolchain prerequisite**: Verify `riscv32-unknown-elf-gcc` installed before starting D.5

---

## Task Dependency Graph

| Task | Depends On | Reason |
|------|------------|--------|
| **G0.5: Debug Infrastructure** | None | VCD dump + register snapshot API — prerequisite for all debugging |
| **A1: CSR Register Bank** | G0.5 | Starting point, pure register definition |
| **A2: mstatus/misa Implementation** | A1, G0.5 | Requires CSR bank structure |
| **A3: Decoder SYSTEM Extension** | A2 | Needs CSR interface defined |
| **A4: SYSTEM Instruction Executor** | A3 | Needs decoder outputs |
| **A5: CSR Unit Tests** | A4 | Tests require working implementation |
| **B1a: Pipeline Skeleton** | G0.5 | IF/ID/EX/MEM/WB stage definitions |
| **B1b: Hazard Unit + Forwarding** | B1a | Requires stage structure for forwarding paths |
| **B1c: Control Signal Wiring** | B1b | Requires hazard unit in place |
| **B2: Control Unit** | B1c | Pipeline wiring foundation |
| **B3: I-TCM Integration** | B1c | Memory interface for IF stage |
| **B4: D-TCM Integration** | B1c | Memory interface for MEM stage |
| **B5: Pipeline Execution Tests** | B2, B3, B4 | All stages must be wired |
| **C1: CLINT Module** | G0.5 | Independent timer/compare logic |
| **C2: Exception Unit** | A2, B1c | Requires CSR registers (mepc, mcause) AND pipeline structure for trap entry |
| **C3: Pipeline IRQ Pin** | B1c | Requires pipeline structure |
| **C4: Interrupt Flow Tests** | C1, C2, C3 | All components required |
| **D1: Address Decoder** | G0.5 | Pure combinational logic |
| **D2: UART Model** | D1 | Requires address decode; **MODIFY** existing `axi_uart.h` (146 lines) |
| **D3: HEX Loader** | None | File parsing utility — moved to Wave 2 (closer to D4) |
| **D4: SoC Integration Tests** | D1, D2, D3 | Full SoC required |
| **D.5: riscv-tests Suite** | D4 | Requires working SoC; **MANDATORY** gate; toolchain prereq: `riscv32-unknown-elf-gcc` |
| **E1: FreeRTOSConfig.h** | None | Configuration only |
| **E2: port.c** | C1 | Requires CLINT timer addresses |
| **E3: portASM.S** | E2 | Trap handler requires context switch logic |
| **E4: Dual-Task Demo** | E3 | Requires working port |
| **E5: FreeRTOS Boot Tests** | D4, E4 | SoC + demo required |

---

## Parallel Execution Graph

### Wave 0 — Phase 0.5: Debug Infrastructure (Day 0, FIRST)

```
└── G0.5: Debug Infrastructure (0.5 day)
    Path: include/chlib/simulator_trace.h (new), examples/riscv-mini/tests/test_trace.cpp
    Category: cpp-pro
    Skills: [cpp-pro, cpp-debug]
    Deliverables:
      - VCD waveform dump capability (ch::SimulatorConfig trace)
      - Register state snapshot API
      - UART console capture utility
    Gate: Generate 10-cycle VCD file, verify with grep/check script
```

> **ZERO-DEBT CHECK**: After Wave 0, verify G0.5 compiles + tests pass before proceeding to Wave 1.

### Wave 1 (Start After G0.5 — 4 Parallel + 1 Sequential)

```
├── A1: CSR Register Bank Definition (1 day)
│   Path: examples/riscv-mini/src/rv32i_csr.h
│   Category: cpp-pro
│   Skills: [cpp-pro]
│   Gate M1: 10 registers accessible, reset values correct, 0 TODO/FIXME
│
├── B1a: Pipeline Skeleton (0.5 day)
│   Path: examples/riscv-mini/src/rv32i_pipeline.h (rewrite start)
│   Category: cpp-pro
│   Skills: [cpp-pro, cpp-architecture]
│   Deliverable: IF/ID/EX/MEM/WB stage definitions with clean interfaces
│   Gate: All 5 stages instantiate, 0 TODO/FIXME
│
├── C1: CLINT Module (1 day)
│   Path: include/chlib/clint.h
│   Category: cpp-pro
│   Skills: [cpp-pro]
│   Gate: mtime increments, mtip triggers at mtimecmp
│
└── D1: Address Decoder (1 day)
    Path: examples/riscv-mini/src/address_decoder.h
    Category: cpp-pro
    Skills: [cpp-pro]
    Gate: 5 address ranges decode correctly, 0 TODO/FIXME
```

> **ZERO-DEBT CHECK**: After Wave 1, run compile + test + doc + clean-sweep gates. Block Wave 2 if ANY fail.

### Wave 2 (After Wave 1 Passes Gates)

```
├── A2: mstatus/misa Bit Fields (0.5 day)
│   Depends: A1
│   Gate: writeMIE(true) → readMIE() == true
│
├── B1b: Hazard Unit + Forwarding (1 day)
│   Depends: B1a
│   Gate: Forwarding paths (EX→EX, MEM→EX, WB→EX) wired, VCD generated
│
├── B1c: Control Signal Wiring (0.5 day)
│   Depends: B1b
│   Gate: Pipeline executes addi sequence, reg_write/alusrc signals correct
│   *** Phase B MICRO-GATE: All 4 critical bugs verified fixed with specific test vectors ***
│     • Bug#1 (PC≠instruction): branch+jump test vector passes
│     • Bug#2 (reg data as PC): forwarding conflict test passes
│     • Bug#3 (hazard broken): load-use stall test passes
│     • Bug#4 (memory always ready): D-TCM wait state test passes
│
├── D2: UART Model (0.5 day) — MODIFY existing axi_uart.h
│   Depends: D1
│   File: include/axi4/peripherals/axi_uart.h (146 lines existing)
│   Delta: Add simplified MMIO wrapper or adapt AXI4-Lite to MMIO address space
│   Gate: Write 'H' → tx_char='H' output
│
└── D3: HEX Loader (1 day) — moved from Wave 1
    Depends: None
    Path: include/chlib/firmware_loader.h
    Gate: Load test.hex → verify I-TCM contents, checksum verified
```

> **ZERO-DEBT CHECK**: After Wave 2, verify ALL Wave 1+2 tasks compile clean, 0 TODO/FIXME in new code.

### Wave 3 (After Wave 2 Passes Gates)

```
├── A3: Decoder SYSTEM Extension (1 day) — Depends: A2
├── C2: Exception Unit (1 day) — Depends: A2, B1c (both CSR AND pipeline)
├── C3: Pipeline IRQ Pin (0.5 day) — Depends: B1c
└── B2: Control Unit (1.5 day) — Depends: B1c
    B3: I-TCM Integration (0.5 day) — Depends: B1c
    B4: D-TCM Integration (0.5 day) — Depends: B1c

    (B2/B3/B4 can run in parallel with A3/C2/C3)
```

```
Parallel Stream 1 (CSR → SYSTEM):
├── A2: mstatus/misa Implementation (0.5 day)
├── A3: Decoder SYSTEM Extension (1 day)
├── A4: SYSTEM Instruction Executor (1 day)
└── A5: CSR Unit Tests (1 day)

Parallel Stream 2 (Pipeline):
├── B2: Control Unit (1.5 days)
├── B3: I-TCM Integration (0.5 day)
├── B4: D-TCM Integration (0.5 day)
└── B5: Pipeline Execution Tests (1.5 days)

Parallel Stream 3 (CLINT → Interrupt):
├── C2: Exception Unit (1 day, requires A2 complete)
├── C3: Pipeline IRQ Pin (0.5 day, requires B1 complete)
└── C4: Interrupt Flow Tests (1 day)

Parallel Stream 4 (SoC):
├── D2: UART Model (0.5 day)
└── D4: SoC Integration Tests (1 day)
```

### Wave 3 (After Wave 2 Complete)

```
├── D.5: riscv-tests ISA Suite (1-2 days)
│   Depends: D4 complete
│   Category: cpp-pro
│   Skills: [cpp-pro]
│
└── E1: FreeRTOSConfig.h (0.5 day)
    Depends: None (can start anytime)
    Category: quick
    Skills: []
```

### Wave 4 (After Wave 3 Complete - Optional Phase E)

```
Parallel Stream 1 (FreeRTOS Port):
├── E2: port.c Implementation (2 days)
├── E3: portASM.S Trap Handler (1.5 days)
└── E4: Dual-Task Demo (1 day)

Parallel Stream 2 (Verification):
└── E5: FreeRTOS Boot Tests (1 day)
    Depends: E4, D4
```

### Wave 5 (Final Tuning - Optional)

```
└── E6: Performance Tuning (2-8 days)
    Tick rate validation, context switch optimization
    Category: cpp-pro
    Skills: [cpp-pro, cpp-debug]
```

---

## Detailed Tasks by Phase

### Phase A: CSR + SYSTEM Instructions (4-5 days)

**Goal**: Implement 10 CSR registers + 11 SYSTEM instructions

#### Task A1: CSR Register Bank Definition

**Description**: Define CSR register bank with all 10 required registers (mstatus, mie, mtvec, mscratch, mepc, mcause, mtval, mip, misa, + placeholder for CLINT interface)

**Files**:
- `examples/riscv-mini/src/rv32i_csr.h` (new file, ~200 lines)

**Implementation Steps**:
1. Create `CsrBank` class inheriting from `ch::Component`
2. Define `__io__` interface: `csr_addr[12]`, `csr_wdata[32]`, `csr_rdata[32]`, `csr_write_en`, `mtip`, `meip`
3. Implement 10 CSR registers as `ch_reg<ch_uint<32>>`:
   - `mstatus` (0x300, reset=0x00000080)
   - `mie` (0x304, reset=0x00000000)
   - `mtvec` (0x305, reset=0x20000100)
   - `mscratch` (0x340, reset=0x00000000)
   - `mepc` (0x341, reset=0x00000000)
   - `mcause` (0x342, reset=0x00000000)
   - `mtval` (0x343, reset=0x00000000)
   - `mip` (0x344, reset=0x00000000)
   - `misa` (0x301, reset=0x40101101)
4. Implement address decoder for CSR access
5. Implement read/write logic using `select()` and `when()`

**Delegation Recommendation**:
- Category: `cpp-pro` - Requires modern C++ HDL design patterns, component definition
- Skills: [`cpp-pro`] - Modern C++20 features, HDL component patterns

**Skills Evaluation**:
- ✅ INCLUDED `cpp-pro`: Core C++ design task, requires component pattern expertise
- ❌ OMITTED `cmake`: CMake changes trivial, part of integration
- ❌ OMITTED `cpp-architecture`: Single component, no complex architecture

**Depends On**: None

**Acceptance Criteria**:
- [ ] `CsrBank` class compiles with 0 warnings
- [ ] All 10 CSR registers accessible via read/write
- [ ] Reset values match spec (mstatus=0x80, misa=0x40101101, others=0)
- [ ] `mtip`/`meip` interrupt inputs present (even if unconnected initially)
- [ ] File registered in `examples/riscv-mini/CMakeLists.txt`

---

#### Task A2: mstatus/misa Implementation

**Description**: Implement bit-field accessors for mstatus (MIE, MPIE, MPP) and misa (extensions)

**Files**:
- `examples/riscv-mini/src/rv32i_csr.h` (modify, add ~30 lines)

**Implementation Steps**:
1. Add bit-field extractors: `mstatus.MIE[3]`, `mstatus.MPIE[7]`, `mstatus.MPP[11:12]`
2. Add CSR accessor methods: `readMIE()`, `writeMIE(bool)`, `readMPIE()`, etc.
3. Implement misa extension check: `isExtension(char ext)` for 'I', 'M', etc.

**Delegation Recommendation**:
- Category: `cpp-pro` - Bit manipulation, type-safe accessors
- Skills: [`cpp-pro`]

**Skills Evaluation**:
- ✅ INCLUDED `cpp-pro`: Bit-field manipulation, modern C++ patterns

**Depends On**: A1

**Acceptance Criteria**:
- [ ] Bit-field accessors compile and work correctly
- [ ] Test: `writeMIE(true)` → `readMIE()` returns true
- [ ] Test: Reset value of MIE is 0, MPIE is 1

---

#### Task A3: Decoder SYSTEM Extension

**Description**: Extend instruction decoder to recognize SYSTEM opcodes (0b1110011) and 11 funct3 variants

**Files**:
- `examples/riscv-mini/src/rv32i_decoder.h` (modify, add ~50 lines)

**Implementation Steps**:
1. Add `enum class SystemFunc3`: CSRRW=1, CSRRS=2, CSRRC=3, CSRRWI=5, CSRRSI=6, CSRRCI=7
2. Add `enum class SystemFunct12`: ECALL=0, EBREAK=1, MRET=0x302, WFI=0x105
3. Extend `decode()` method to detect SYSTEM opcode
4. Add outputs: `is_system`, `system_func3[3]`, `csr_addr[12]`, `system_funct12[12]`

**Delegation Recommendation**:
- Category: `cpp-pro` - Decoder extension, enum safety
- Skills: [`cpp-pro`]

**Skills Evaluation**:
- ✅ INCLUDED `cpp-pro`: Instruction decoding, enum design

**Depends On**: A2 (CSR addresses known)

**Acceptance Criteria**:
- [ ] SYSTEM opcode detection: `instr[6:0] == 0b1110011`
- [ ] funct3 decoding correct for all 7 values
- [ ] funct12 decoding for ECALL/EBREAK/MRET/WFI
- [ ] Test: decode(0x30200073) → is_mret=true

---

#### Task A4: SYSTEM Instruction Executor

**Description**: Implement execution logic for 11 SYSTEM instructions (6 CSR ops + MRET + ECALL + EBREAK + WFI)

**Files**:
- `examples/riscv-mini/src/rv32i_system.h` (new file, ~150 lines)

**Implementation Steps**:
1. Create `SystemExecutor` class inheriting from `ch::Component`
2. Define `__io__`: instruction, `is_system`, `rs1_data`, `rd_wdata`, `rd_write_en`, `mret_pc`, `is_mret`, `csr_addr`, `csr_wdata`, `csr_write_en`, `csr_rdata`
3. Implement CSRRW: `tmp=CSR[addr]; CSR[addr]=rs1; rd=tmp`
4. Implement CSRRS: `tmp=CSR[addr]; CSR[addr]|=rs1; rd=tmp`
5. Implement CSRRC: `tmp=CSR[addr]; CSR[addr]&=~rs1; rd=tmp`
6. Implement CSRRWI/CSRRSI/CSRRCI with immediate
7. Implement MRET: `PC=mepc; mstatus.MIE=mstatus.MPIE; mstatus.MPIE=1`
8. Implement ECALL: trigger exception (stub for Phase C)
9. Implement EBREAK: trigger breakpoint (stub for Phase C)
10. Implement WFI: NOP (optional)

**Delegation Recommendation**:
- Category: `cpp-pro` - Complex instruction execution logic
- Skills: [`cpp-pro`]

**Skills Evaluation**:
- ✅ INCLUDED `cpp-pro`: Instruction semantics, component design

**Depends On**: A3

**Acceptance Criteria**:
- [ ] All 11 SYSTEM instructions compile
- [ ] Test: CSRRW reads old value, writes new value
- [ ] Test: CSRRS sets bits correctly
- [ ] Test: CSRRC clears bits correctly
- [ ] Test: MRET updates PC and mstatus correctly

---

#### Task A5: CSR Unit Tests

**Description**: Write comprehensive unit tests for CSR bank and SYSTEM instructions

**Files**:
- `examples/riscv-mini/tests/test_rv32i_csr.cpp` (new file, ~300 lines)
- `examples/riscv-mini/tests/test_mret.cpp` (new file, ~150 lines)

**Test Cases**:
1. **CSR Read/Write**: Write mstatus, read back verify
2. **CSRRS/Set Bits**: Set MTIE bit in mie, verify
3. **CSRRC/Clear Bits**: Clear MTIE bit, verify
4. **MRET Flow**: Set mepc=0x100, mstatus.MPIE=1, execute MRET, verify PC=0x100 and mstatus.MIE=1
5. **ECALL Trigger**: Execute ECALL, verify exception pending flag

**Delegation Recommendation**:
- Category: `cpp-pro` - Test design, Catch2 patterns
- Skills: [`cpp-pro`]

**Skills Evaluation**:
- ✅ INCLUDED `cpp-pro`: Test patterns, HDL simulation

**Depends On**: A4

**Acceptance Criteria**:
- [ ] 5 test cases, 20+ assertions
- [ ] All tests pass
- [ ] Registered in CMake with `add_test(NAME csr_tests COMMAND ...)`
- [ ] 0 compile warnings

---

### Phase B: Pipeline Full Integration (5-7 days)

**Goal**: Complete 5-stage pipeline rewrite with correct wiring, control signals, and hazard handling

**Critical Note**: This is a **full redesign**, not bug fixes. Existing `rv32i_pipeline.h` has 4 critical bugs (Momus review #6). **SPLIT into 3 sub-tasks** with VCD checkpoints per Metis recommendation.

#### Sub-task B1a: Pipeline Skeleton (0.5 day)

**Description**: Define IF/ID/EX/MEM/WB stage structure with clean interfaces.

**Files**: `examples/riscv-mini/src/rv32i_pipeline.h` (rewrite start)
**Gate**: All 5 stages instantiate correctly, inter-stage registers defined, 0 TODO/FIXME. VCD dump generated.

#### Sub-task B1b: Hazard Unit + Forwarding (1 day)

**Description**: Implement forwarding paths and hazard detection.

**Gate**: 
- Forwarding paths (EX→EX, MEM→EX, WB→EX) all wired
- Load-use stall logic correct (1 cycle max)
- Branch flush logic correct
- VCD dump shows clean signal propagation for 10 cycles
- 0 TODO/FIXME in hazard unit code

#### Sub-task B1c: Control Signal Wiring (0.5 day)

**Description**: Connect ControlUnit outputs to stage inputs, complete memory handshake.

**Gate**:
- ALL 4 critical bugs verified fixed with specific test vectors:
  • Bug#1 (PC≠instruction): branch+jump instruction sequence executes correctly
  • Bug#2 (reg data as PC): forwarding conflict resolved, PC uses branch target
  • Bug#3 (hazard broken): load-use stall → resume → correct result
  • Bug#4 (memory always ready): D-TCM wait states properly modeled
- clang-tidy clean (0 warnings)
- VCD pattern check passes

#### Task B5: Pipeline Execution Tests (1.5 days)

> **TDD Note**: Test file must exist (failing) BEFORE B1a starts. Implementation fills failing tests.

#### Task B1: Pipeline Top Rewrite

**Description**: Rewrite `rv32i_pipeline.h` from scratch with correct stage interconnections

**Files**:
- `examples/riscv-mini/src/rv32i_pipeline.h` (complete rewrite, ~400 lines)

**Critical Fixes Required** (from Momus #6):
1. **Line 121 PC≠instruction**: Fix `id_stage.io().instruction <<= if_stage.io().pc` (should be instruction data)
2. **Line 131 register data as PC**: Remove `ex_stage.io().pc <<= id_stage.io().rs1_data`, use proper IF-stage PC
3. **Line 166-168 hazard detection broken**: Complete rewire of hazard unit with proper inputs
4. **Line 147 memory always ready**: Add proper memory ready handshake

**Implementation Steps**:
1. Define clear stage boundaries: IF → ID → EX → MEM → WB
2. IF stage: PC register, I-TCM interface, branch/jump PC update
3. ID stage: Instruction decode, register read, immediate generation
4. EX stage: ALU, branch comparator, forwarding MUX
5. MEM stage: Load/store address, D-TCM interface
6. WB stage: Result MUX (ALU vs MEM), x0 hardwire
7. Hazard unit: Forwarding paths (EX→EX, MEM→EX, WB→EX), load-use stall, branch flush

**Delegation Recommendation**:
- Category: `cpp-pro` - Complex pipeline integration
- Skills: [`cpp-pro`, `cpp-architecture`] - Needs architecture review for clean stage separation

**Skills Evaluation**:
- ✅ INCLUDED `cpp-pro`: Core implementation task
- ✅ INCLUDED `cpp-architecture`: Critical for stage boundary design, hazard detection architecture
- ❌ OMITTED `cpp-debug`: This is implementation not debugging

**Depends On**: A1 (for CSR interrupt interface placeholder)

**Acceptance Criteria**:
- [ ] All 5 stages instantiate correctly
- [ ] Inter-stage registers (IF/ID, ID/EX, EX/MEM, MEM/WB) defined
- [ ] Hazard unit connected with all forwarding paths
- [ ] Compiles with 0 warnings
- [ ] No TODO comments, no stubs

---

#### Task B2: Control Unit

**Description**: Implement centralized control signal generation for all instruction types

**Files**:
- `examples/riscv-mini/src/rv32i_control.h` (new file, ~100 lines)

**Implementation Steps**:
1. Define control signal struct: `reg_write`, `alu_src`, `mem_read`, `mem_write`, `branch`, `jump`
2. Decode opcode to instruction type: R-type=0110011, I-type=0010011, Load=0000011, Store=0100011, Branch=1100011, JAL=1101111
3. Generate control signals per type using lookup table or combinational logic

**Delegation Recommendation**:
- Category: `cpp-pro` - Control logic design
- Skills: [`cpp-pro`]

**Depends On**: B1

**Acceptance Criteria**:
- [ ] All 6 instruction types recognized
- [ ] Control signals correct per ISA spec
- [ ] Test: decode(0x00100093 addi) → reg_write=1, alu_src=1

---

#### Task B3: I-TCM Integration

**Description**: Integrate I-TCM (64KB ROM) with IF stage

**Files**:
- `examples/riscv-mini/src/i_tcm.h` (modify to fix interface)
- `examples/riscv-mini/src/rv32i_pipeline.h` (connect)

**Implementation Steps**:
1. Create `ITcmMemory` component with AXI-like interface
2. Load program from vector<uint32_t> or HEX
3. Connect IF stage `instr_addr` → I-TCM address
4. Connect I-TCM `instr_data` → IF stage

**Delegation Recommendation**:
- Category: `cpp-pro` - Memory interface
- Skills: [`cpp-pro`]

**Depends On**: B1

**Acceptance Criteria**:
- [ ] I-TCM loads test program correctly
- [ ] IF stage fetches correct instruction each cycle
- [ ] Test: Load program {0x00100093, ...}, verify IF reads instruction[0], instruction[1], etc.

---

#### Task B4: D-TCM Integration

**Description**: Integrate D-TCM (64KB RAM) with MEM stage

**Files**:
- `examples/riscv-mini/src/d_tcm.h` (modify to fix interface)
- `examples/riscv-mini/src/rv32i_pipeline.h` (connect)

**Implementation Steps**:
1. Create `DtcmMemory` component with read/write interface
2. Implement byte-enable (strbe[3:0]) support
3. Connect MEM stage `data_addr`, `data_write_data`, `data_strbe`, `data_write_en` → D-TCM
4. Connect D-TCM `data_read_data` → MEM stage

**Delegation Recommendation**:
- Category: `cpp-pro` - Memory interface
- Skills: [`cpp-pro`]

**Depends On**: B1

**Acceptance Criteria**:
- [ ] D-TCM reads/writes correctly
- [ ] Byte enables work (write single byte)
- [ ] Test: Write 0xDEADBEEF to 0x20010000, read back verify

---

#### Task B5: Pipeline Execution Tests

**Description**: Write comprehensive tests for full pipeline executing instruction sequences

**Files**:
- `examples/riscv-mini/tests/test_pipeline_execution.cpp` (new file, ~400 lines)

**Test Cases**:
1. **Simple Sequence**: Execute 5 instructions (addi, addi, add, sw, lw), verify results
2. **Loop Test**: Execute 20-instruction loop, verify x3=3, x4=10
3. **Forwarding Test**: Execute sequence requiring all 3 forwarding paths (EX→EX, MEM→EX, WB→EX)
4. **Load-Use Stall**: Execute addi → load → add (needs stall), verify correct result
5. **Branch Test**: Execute beq taken/not-taken, verify PC update

**Delegation Recommendation**:
- Category: `cpp-pro` - Integration test design
- Skills: [`cpp-pro`]

**Depends On**: B1, B2, B3, B4

**Acceptance Criteria**:
- [ ] 5 test cases, 25+ assertions
- [ ] All tests pass
- [ ] 20-instruction sequence executes correctly
- [ ] Register results verified
- [ ] Memory results verified (0xDEADBEEF at 0x20010000)

---

### Phase C: CLINT + Interrupts (3-4 days)

**Goal**: Implement MTIP interrupt trigger → trap entry → mret return flow

#### Task C1: CLINT Module

**Description**: Implement Core-Local Interrupt Controller with mtime/mtimecmp registers

**Files**:
- `include/chlib/clint.h` (new file, ~150 lines)

**Implementation Steps**:
1. Create `Clint` class inheriting from `ch::Component`
2. Define `__io__`: `addr[32]`, `write_data[32]`, `write_en`, `read_data[32]`, `read_en`, `mtip`
3. Implement 64-bit mtime counter (two 32-bit registers at 0x0 and 0x4)
4. Implement 64-bit mtimecmp comparator (two 32-bit registers at 0x8 and 0xC)
5. Generate mtip when `mtime >= mtimecmp`
6. Implement `tick()` method to increment mtime

**Delegation Recommendation**:
- Category: `cpp-pro` - Timer/counter design
- Skills: [`cpp-pro`]

**Depends On**: None (independent)

**Acceptance Criteria**:
- [ ] mtime increments every tick
- [ ] mtimecmp triggers mtip when crossed
- [ ] Test: Set mtimecmp=mtime+10, wait 10 ticks, verify mtip=1
- [ ] Compiles with 0 warnings

---

#### Task C2: Exception Unit

**Description**: Implement trap entry logic (save mepc/mcause, update mstatus, jump to mtvec)

**Files**:
- `examples/riscv-mini/src/rv32i_exception.h` (new file, ~200 lines)

**Implementation Steps**:
1. Create `ExceptionUnit` class inheriting from `ch::Component`
2. Define `__io__`: `interrupt_req`, `exception_req`, `mcause`, `current_pc`, `trap_pc`, `mepc_write`, `mcause_write`, etc.
3. Implement trap entry sequence:
   - `mepc = current_pc`
   - `mcause = exception_code`
   - `mstatus.MPIE = mstatus.MIE`
   - `mstatus.MIE = 0`
   - `PC = mtvec` (or mtvec + cause×4 for vectored)
4. Implement exception codes: MTI=0x80000007, MEI=0x8000000B, ECALL=0x00000008, etc.

**Delegation Recommendation**:
- Category: `cpp-pro` - Exception handling logic
- Skills: [`cpp-pro`]

**Depends On**: A2 (CSR interface)

**Acceptance Criteria**:
- [ ] Trap entry within 4 cycles of interrupt
- [ ] mepc, mcause, mstatus saved correctly
- [ ] Test: Trigger MTIP, verify mcause=0x80000007, mepc=trapped_PC, PC=mtvec

---

#### Task C3: Pipeline IRQ Pin

**Description**: Add interrupt detection to pipeline (check mtip && mie.MTIE && mstatus.MIE)

**Files**:
- `examples/riscv-mini/src/rv32i_pipeline.h` (modify, add ~30 lines)

**Implementation Steps**:
1. Add input: `mtip` from CLINT
2. Add interrupt checker: `interrupt_pending = mtip && mie.MTIE && mstatus.MIE`
3. Connect to ExceptionUnit
4. Ensure interrupt checked at end of each cycle

**Delegation Recommendation**:
- Category: `cpp-pro` - Pipeline integration
- Skills: [`cpp-pro`]

**Depends On**: B1 (pipeline structure)

**Acceptance Criteria**:
- [ ] Interrupt pin connected
- [ ] Interrupt checker logic correct
- [ ] Test: Set mtip=1, mie.MTIE=1, mstatus.MIE=1, verify interrupt_pending=1

---

#### Task C4: Interrupt Flow Tests

**Description**: Write tests for complete interrupt flow: trigger → trap → mret return

**Files**:
- `examples/riscv-mini/tests/test_timer_interrupt.cpp` (new file, ~300 lines)

**Test Cases**:
1. **MTIP Trigger**: Set mtimecmp, wait, verify mtip=1
2. **Trap Entry**: Trigger interrupt, verify mcause, mepc, mstatus saved, PC=mtvec
3. **MRET Return**: Execute mret, verify PC=mepc, mstatus.MIE=old_MPIE
4. **Interrupt Handler Flow**: Execute full handler (increment counter, clear mtip, mret)

**Delegation Recommendation**:
- Category: `cpp-pro` - Integration test design
- Skills: [`cpp-pro`]

**Depends On**: C1, C2, C3

**Acceptance Criteria**:
- [ ] 4 test cases, 20+ assertions
- [ ] Interrupt latency ≤ 4 cycles
- [ ] MRET returns to correct PC
- [ ] All tests pass

---

### Phase D: SoC + Bare-metal Hello World (3-4 days)

**Goal**: Complete SoC integration with UART output "Hello from riscv-mini"

#### Task D1: Address Decoder

**Description**: Implement MMIO address decoder to route CPU memory accesses to I-TCM, D-TCM, CLINT, UART, GPIO

**Files**:
- `examples/riscv-mini/src/address_decoder.h` (new file, ~100 lines)

**Implementation Steps**:
1. Define address map constants: I_TCM=0x20000000, D_TCM=0x20010000, CLINT=0x40000000, UART=0x40001000, GPIO=0x40002000
2. Create `AddressDecoder` combinational component
3. Implement slave select logic based on address ranges
4. Generate chip selects: `i_tcm_cs`, `d_tcm_cs`, `clint_cs`, `uart_cs`, `gpio_cs`

**Delegation Recommendation**:
- Category: `cpp-pro` - Address decode logic
- Skills: [`cpp-pro`]

**Depends On**: None

**Acceptance Criteria**:
- [ ] All address ranges decoded correctly
- [ ] Test: addr=0x20000000 → i_tcm_cs=1
- [ ] Test: addr=0x40001000 → uart_cs=1
- [ ] Invalid address → error_cs=1

---

#### Task D2: UART Model (MODIFY existing)

**Description**: Adapt existing UART 16550 model for SoC integration.

**Files**:
- `include/axi4/peripherals/axi_uart.h` (MODIFY existing — 146 lines, `AxiLiteUart` class already present)

> **CRITICAL**: DO NOT create a new file or overwrite existing code. The existing `AxiLiteUart<PRESCALE_WIDTH>` has a complete AXI4-Lite interface. You need to either: (a) add a simplified MMIO wrapper on top of it, or (b) adapt the existing AXI4-Lite signals to work with our address decoder's MMIO protocol.

**Implementation Steps**:
1. **READ** existing `include/axi4/peripherals/axi_uart.h` (146 lines) — understand the `AxiLiteUart` interface
2. Add a simplified MMIO wrapper `AxiUartMmio` or adapt `AxiLiteUart` to respond to MMIO read/write signals
3. The wrapper should map: MMIO `write_en`+`write_data` → UART TX FIFO, MMIO `read_en` → UART read data
4. Ensure address 0x40001000 is handled by the address decoder routing to this peripheral

**Delegation Recommendation**:
- Category: `cpp-pro` - Peripheral design
- Skills: [`cpp-pro`]

**Depends On**: D1 (for address decode)

**Acceptance Criteria**:
- [ ] UART address 0x40001000 decoded
- [ ] Write to TX holding register → tx_char output
- [ ] Test: Write 'H', verify tx_char='H'
- [ ] Test: Write sequence, verify all chars output

---

#### Task D3: HEX Loader

**Description**: Implement Intel HEX file parser to load firmware into I-TCM/D-TCM

**Files**:
- `include/chlib/firmware_loader.h` (new file, ~200 lines)

**Implementation Steps**:
1. Define `HexRecord` struct: type, address, data[16], checksum
2. Implement HEX parser: parse_line(), verify_checksum()
3. Support record types: 00=Data, 01=EOF, 02=Extended Segment Address
4. Load to memory: `loadHex(ITcmMemory&, const string& path)`
5. Optional: Support binary format `.bin`

**Delegation Recommendation**:
- Category: `cpp-pro` - File parsing, memory loading
- Skills: [`cpp-pro`]

**Depends On**: None

**Acceptance Criteria**:
- [ ] HEX format parsed correctly
- [ ] Checksum verified
- [ ] Test: Load test.hex → verify I-TCM[0]=first_instr, I-TCM[1]=second_instr
- [ ] Error handling: invalid checksum → throw exception

---

#### Task D4: SoC Integration Tests

**Description**: Write top-level test for complete SoC running bare-metal "Hello World"

**Files**:
- `examples/riscv-mini/tests/test_soc_integration.cpp` (new file, ~300 lines)

**Test Cases**:
1. **HEX Loading**: Load test.hex, verify I-TCM contents
2. **Boot Test**: Reset CPU, verify PC=0x20000000
3. **Hello World**: Run simulation, capture UART output, verify "Hello from riscv-mini\r\n"
4. **CLINT mtime**: Verify mtime increments during simulation

**Delegation Recommendation**:
- Category: `cpp-pro` - SoC integration testing
- Skills: [`cpp-pro`]

**Depends On**: D1, D2, D3

**Acceptance Criteria**:
- [ ] 4 test cases, 20+ assertions
- [ ] HEX loads correct addresses
- [ ] UART output matches exactly "Hello from riscv-mini\r\n"
- [ ] mtime increments every tick
- [ ] All tests pass

---

### Phase D.5: riscv-tests ISA Suite (MANDATORY — 1-2 days)

> **CHANGED from "optional" to MANDATORY.** CPU correctness MUST be proven via riscv-tests. Phase E (FreeRTOS) cannot start without D.5 passing.

**Goal**: Run riscv-tests RV32I standard test suite for CPU correctness proof

#### Task D.5.1: riscv-tests Integration

**Description**: Compile and load riscv-tests RV32I ISA suite

**Files**:
- `examples/riscv-mini/firmware/riscv-tests/` (new subdir)
- `examples/riscv-mini/tests/test_riscv_tests_suite.cpp` (new file, ~200 lines)

**Implementation Steps**:
1. Clone riscv-tests: `git clone https://github.com/riscv-software-src/riscv-tests`
2. Cross-compile for RV32I: `./configure --with-arch=rv32i --with-abi=ilp32`
3. Compile ISA tests: `make isa-rv32i`
4. Convert ELF to HEX: `riscv32-unknown-elf-objcopy -O ihex test_add.elf test_add.hex`
5. Write test harness to load HEX and check `tohost` variable

**Delegation Recommendation**:
- Category: `cpp-pro` - Test suite integration
- Skills: [`cpp-pro`]

**Depends On**: D4

**Acceptance Criteria**:
- [ ] riscv-tests compiled for RV32I
- [ ] All 40 ISA tests loaded and run
- [ ] tohost=1 (pass) for all tests
- [ ] Coverage report: ≥90% RV32I instructions covered

---

### Phase E: FreeRTOS Port (Optional, 7-14 days)

**Goal**: Port FreeRTOS and run dual-task demo

#### Task E1: FreeRTOSConfig.h

**Description**: Create FreeRTOS configuration header for riscv-mini

**Files**:
- `examples/riscv-mini/freertos/config/FreeRTOSConfig.h` (new file, ~80 lines)

**Implementation Steps**:
1. Define configMTIME_BASE_ADDRESS=0x40000000
2. Define configMTIMECMP_BASE_ADDRESS=0x40000008
3. Define configCPU_CLOCK_HZ=50000000 (50MHz)
4. Define configTICK_RATE_HZ=1000 (1kHz)
5. Define configMAX_PRIORITIES=5, configMINIMAL_STACK_SIZE=128
6. Enable preemption: configUSE_PREEMPTION=1

**Delegation Recommendation**:
- Category: `quick` - Configuration header
- Skills: []

**Depends On**: None

**Acceptance Criteria**:
- [ ] All required macros defined
- [ ] Matches spec values
- [ ] Compiles with FreeRTOS headers

---

#### Task E2: port.c Implementation

**Description**: Implement FreeRTOS RISC-V port layer (context save/restore, scheduler)

**Files**:
- `examples/riscv-mini/freertos/port/port.c` (new file, ~300 lines)

**Implementation Steps**:
1. Implement `pxPortInitialiseStack()` to create initial stack frame
2. Stack layout: x1-x31, mstatus, mepc (33 words total)
3. Implement `xPortStartScheduler()`: enable interrupts, setup timer, first yield
4. Implement `vPortEndScheduler()`: infinite loop (not supported)
5. Implement timer setup: calculate tick interval, write mtimecmp

**Delegation Recommendation**:
- Category: `cpp-pro` - RTOS porting
- Skills: [`cpp-pro`]

**Depends On**: C1 (CLINT interface)

**Acceptance Criteria**:
- [ ] pxPortInitialiseStack creates correct stack frame
- [ ] Initial mstatus=0x1880 (MIE=1, MPIE=1)
- [ ] Initial mepc=task entry point
- [ ] Scheduler starts first task

---

#### Task E3: portASM.S Trap Handler

**Description**: Implement assembly trap handler for context switch

**Files**:
- `examples/riscv-mini/freertos/port/portASM.S` (new file, ~200 lines)

**Implementation Steps**:
1. Define `.global vPortTrapHandler`
2. Save all 32 registers to stack (128 bytes)
3. Save mstatus, mepc (8 bytes)
4. Check mcause: if MTI (0x80000007), call vPortSysTickHandler
5. Call vTaskSwitchContext to select next task
6. Restore all registers
7. MRET to return

**Delegation Recommendation**:
- Category: `cpp-pro` - Assembly, RTOS low-level
- Skills: [`cpp-pro`]

**Depends On**: E2

**Acceptance Criteria**:
- [ ] Trap handler entry saves all registers
- [ ] Context switch selects next task
- [ ] MRET returns to correct task
- [ ] Assembles with riscv32-unknown-elf-as

---

#### Task E4: Dual-Task Demo

**Description**: Create FreeRTOS demo with 2 tasks alternating output

**Files**:
- `examples/riscv-mini/freertos/demo/demo_main.c` (new file, ~100 lines)

**Implementation Steps**:
1. Task 1: Counter task, print "[Task 1] counter=X" every 500ms
2. Task 2: Tick monitor, print "[Task 2] tick=X" every 1000ms
3. Implement putchar using UART TX register
4. Create tasks with xTaskCreate
5. Start scheduler with vTaskStartScheduler

**Delegation Recommendation**:
- Category: `cpp-pro` - FreeRTOS application
- Skills: [`cpp-pro`]

**Depends On**: E3

**Acceptance Criteria**:
- [ ] Both tasks created
- [ ] Task 1 prints every 500ms
- [ ] Task 2 prints every 1000ms
- [ ] Output alternates: Task1, Task2, Task1, Task1, Task2, ...

---

#### Task E5: FreeRTOS Boot Tests

**Description**: Write test for FreeRTOS boot and task switching

**Files**:
- `examples/riscv-mini/freertos/tests/test_freertos_boot.cpp` (new file, ~300 lines)

**Test Cases**:
1. **Boot Test**: Load demo.elf, run 10s, verify FreeRTOS starts
2. **Task Switch**: Verify both tasks output alternating messages
3. **Tick Rate**: Measure tick interval, verify 1kHz ±1% (or degrade to 100Hz)
4. **Stability**: Run 10s continuous, verify no crashes

**Delegation Recommendation**:
- Category: `cpp-pro` - System testing
- Skills: [`cpp-pro`]

**Depends On**: D4, E4

**Acceptance Criteria**:
- [ ] FreeRTOS boots successfully
- [ ] Both tasks output ≥5 messages each
- [ ] Tick rate within tolerance
- [ ] No crashes in 10s runtime
- [ ] All tests pass

---

## Risk Mitigation

| Risk | Impact | Probability | Mitigation Actions |
|------|--------|-------------|-------------------|
| **Pipeline wiring complexity** | Phase B delay | **High** | 1. Divide B1 into sub-tasks: IF-stage, ID-stage, EX-stage, etc. 2. Test each stage independently before integration 3. Use explicit TODO list per stage 4. Daily integration builds |
| **Load-use hazard deadlock** | Pipeline stall never recovers | **High** | 1. Explicit stall/flush signal path tests 2. Add stall counter (max 1 cycle for load-use) 3. VCD waveform analysis for deadlock detection 4. Unit test: addi → load → add sequence |
| **CSR/exception coupling** | Phase A/C mutual blocking | Medium | 1. Phase A: CSR read/write only (no exception flow) 2. Phase C: Exception unit with stub CSR interface 3. Integrate in Phase C 4. Clear boundary: A=registers, C=exception logic |
| **FreeRTOS port compatibility** | Phase E fails | Medium | 1. Reference VexRiscv FreeRTOS port 2. Start with minimal config (2 tasks, 100Hz tick) 3. Incremental: tick interrupt → context switch → full scheduler 4. Test in phases: boot → single task → dual task |
| **Timer clock domain mismatch** | CLINT frequency wrong | **High** | 1. Explicit mtime increment frequency test 2. Configurable tick rate via parameter 3. Start with 100Hz, verify, then increase to 1kHz 4. GPIO toggle measurement for tick period |
| **Toolchain compatibility** | GCC built-ins not available | Medium | 1. Use Clang RISC-V target first 2. Minimize compiler built-ins 3. Implement software alternatives 4. Test with simple program first |
| **No debug infrastructure** | Phase E crash un-debuggable | **High** | 1. Phase 0.5: Add VCD waveform dump (ch::SimulatorConfig trace) 2. UART console capture tool 3. Register state snapshot API 4. Implement before Phase E starts |
| **1kHz tick unverified** | FreeRTOS timing fails | Medium | 1. Test 100Hz first 2. Measure tick interval with mtime 3. Gradually increase: 100Hz → 500Hz → 1kHz 4. Fallback: if 1kHz unstable, use 100Hz |

---

## Commit Strategy

### Atomic Commit Guidelines

Each commit must follow these rules:
1. **Single Responsibility**: One logical change per commit
2. **Compiles**: Every commit compiles with 0 errors
3. **Tests Pass**: Existing tests pass (new tests can be added)
4. **Message Format**: `"[Phase X] Task Y: brief description"`

### Commit Sequence

#### Phase A Commits (5 commits)
```
1. "[Phase A] A1: Add CSR register bank skeleton"
   - rv32i_csr.h: CsrBank class with __io__
   - CMakeLists.txt: register new file
   
2. "[Phase A] A2: Implement mstatus/misa bit fields"
   - rv32i_csr.h: add MIE, MPIE, MPP accessors
   
3. "[Phase A] A3: Extend decoder with SYSTEM opcodes"
   - rv32i_decoder.h: enum SystemFunc3, decode logic
   
4. "[Phase A] A4: Implement SYSTEM instruction executor"
   - rv32i_system.h: SystemExecutor class
   
5. "[Phase A] A5: Add CSR unit tests"
   - tests/test_rv32i_csr.cpp: 5 test cases
   - tests/test_mret.cpp: MRET flow test
   - CMakeLists.txt: add_test commands
```

#### Phase B Commits (5 commits)
```
1. "[Phase B] B1: Rewrite pipeline top module"
   - rv32i_pipeline.h: complete rewrite
   
2. "[Phase B] B2: Add control unit"
   - rv32i_control.h: ControlUnit class
   
3. "[Phase B] B3, B4: Integrate I/D-TCM"
   - i_tcm.h, d_tcm.h: memory interfaces
   
4. "[Phase B] B5: Add pipeline execution tests"
   - tests/test_pipeline_execution.cpp: 5 tests
   
5. "[Phase B] B1-B4 fix: Fix hazard detection wiring"
   - Bug fixes if needed
```

#### Phase C Commits (4 commits)
```
1. "[Phase C] C1: Add CLINT module"
   - include/chlib/clint.h: Clint class
   
2. "[Phase C] C2: Implement exception unit"
   - rv32i_exception.h: ExceptionUnit class
   
3. "[Phase C] C3: Add pipeline IRQ pin"
   - rv32i_pipeline.h: interrupt detection
   
4. "[Phase C] C4: Add interrupt flow tests"
   - tests/test_timer_interrupt.cpp: 4 tests
```

#### Phase D Commits (4-5 commits)
```
1. "[Phase D] D1: Implement address decoder"
   - address_decoder.h: AddressDecoder class
   
2. "[Phase D] D2: Add UART model"
   - axi_uart.h: AxUart class
   
3. "[Phase D] D3: Implement HEX loader"
   - firmware_loader.h: FirmwareLoader class
   
4. "[Phase D] D4: Add SoC integration tests"
   - tests/test_soc_integration.cpp: 4 tests
   
5. "[Phase D] D.5: Optional riscv-tests integration"
   - firmware/riscv-tests/: cloned suite
   - tests/test_riscv_tests_suite.cpp
```

#### Phase E Commits (5-6 commits, optional)
```
1. "[Phase E] E1: Add FreeRTOSConfig.h"
   - freertos/config/FreeRTOSConfig.h
   
2. "[Phase E] E2: Implement port.c"
   - freertos/port/port.c
   
3. "[Phase E] E3: Implement portASM.S"
   - freertos/port/portASM.S
   
4. "[Phase E] E4: Add dual-task demo"
   - freertos/demo/demo_main.c
   
5. "[Phase E] E5: Add FreeRTOS boot tests"
   - freertos/tests/test_freertos_boot.cpp
   
6. "[Phase E] E6: Optional performance tuning"
   - Tick rate optimization
```

---

## Success Criteria

### Wave-to-Wave Gate Enforcement (MANDATORY)

**BEFORE each wave can start, ALL gates from the previous wave MUST pass:**

```
Wave 0 → Wave 1: G0.5 passes all 4 gates (compile/test/doc/clean)
Wave 1 → Wave 2: A1, B1a, C1, D1 pass all 4 gates
Wave 2 → Wave 3: A2, B1b, B1c, D2, D3 pass all 4 gates
Wave 3 → Wave 4: A3, A4, B2/B3/B4, C2, C3 pass all 4 gates
Wave 4 → Wave 5: A5, B5, C4, D4 pass all 4 gates
Wave 5 → Wave E: D.5 passes (MANDATORY)
```

**Gate Enforcement Script** (run at the end of EACH wave):
```bash
cd /workspace/project/CppHDL

# 1. Compile gate
echo "=== COMPILE GATE ==="
cmake --build build -j$(nproc)
BUILD_EXIT=$?
if [ $BUILD_EXIT -ne 0 ]; then echo "FAIL: compile errors"; exit 1; fi
WARNINGS=$(cmake --build build 2>&1 | grep -c "warning:" || true)
if [ $WARNINGS -gt 0 ]; then echo "FAIL: $WARNINGS warnings"; exit 1; fi
echo "✅ COMPILE PASS"

# 2. Test gate
echo "=== TEST GATE ==="
ctest --output-on-failure
CTEST_EXIT=$?
if [ $CTEST_EXIT -ne 0 ]; then echo "FAIL: tests failed"; exit 1; fi
echo "✅ TESTS PASS"

# 3. Doc gate
echo "=== DOC GATE ==="
if ! grep -qE "CSR|CLINT|UART|pipeline|riscv" AGENTS.md; then echo "FAIL: AGENTS.md not updated"; exit 1; fi
echo "✅ DOCS SYNCED"

# 4. Clean gate (NO TODO/FIXME)
echo "=== CLEAN GATE ==="
DEBT=$(grep -r "TODO\|FIXME\|STUB\|placeholder\|skeleton" examples/riscv-mini/src/ include/chlib/ --include="*.h" --include="*.cpp" 2>/dev/null | grep -v ".bak" | wc -l)
if [ $DEBT -gt 0 ]; then echo "FAIL: $DEBT technical debt items found"; grep -r "TODO\|FIXME\|STUB\|placeholder\|skeleton" examples/riscv-mini/src/ include/chlib/ --include="*.h" --include="*.cpp"; exit 1; fi
echo "✅ NO TECH DEBT"

# 5. Skill extraction gate (Phase-end only)
echo "=== SKILL EXTRACTION GATE ==="
if [ ! -f ".sisyphus/artifacts/phase-${PHASE}-summary.md" ]; then
  echo "FAIL: Phase artifact missing (.sisyphus/artifacts/phase-${PHASE}-summary.md)";
  exit 1
fi
echo "✅ ARTIFACT PRODUCED"

echo "=== ALL GATES PASSED — Wave complete ==="
```

> **CRITICAL**: If ANY gate fails, the current wave is INCOMPLETE. Fix before proceeding. No "we'll do it in the next wave."

---

### Phase Gates Verification Commands

Each phase must pass these gates before marking complete:

#### Gate 1: 编译通过 (Compile Pass)
```bash
cd /workspace/project/CppHDL
cmake -B build
cmake --build build -j$(nproc)

# Verify 0 errors, 0 warnings on new files
cmake --build build 2>&1 | grep -E "error:|Error" | grep -v "note:"
cmake --build build 2>&1 | grep -E "warning:" | grep "riscv-mini"
```

**Criteria**: 0 errors, 0 warnings on riscv-mini files

---

#### Gate 2: 测试覆盖 (Test Coverage)
```bash
# Run CTest
cd build
ctest -L riscv --output-on-failure

# Or run specific tests
./examples/riscv-mini/tests/test_rv32i_csr
./examples/riscv-mini/tests/test_pipeline_execution
./examples/riscv-mini/tests/test_timer_interrupt
./examples/riscv-mini/tests/test_soc_integration

# riscv-mini standalone tests
./run_all_ported_tests.sh | grep -E "riscv|pipeline"
```

**Criteria**: All tests pass, assertion counts met

---

#### Gate 3: 文档同步 (Documentation Sync)
```bash
# Verify AGENTS.md updated with new APIs
git diff AGENTS.md | grep -E "CSR|CLINT|UART|FreeRTOS"

# Verify usage-guide updated
git diff docs/usage_guide/riscv-mini.md | head -50

# Verify inline comments in new files
grep -r "TODO\|FIXME" examples/riscv-mini/src/*.h
```

**Criteria**: AGENTS.md has new API section, 0 TODO/FIXME in new code

---

### Final Verification for CPU Correctness (Phase D Gate)

```bash
cd /workspace/project/CppHDL/build

# 1. Compile check
cmake --build . -j$(nproc) 2>&1 | grep -E "error:|Error"
# Expected: no output

# 2. All Phase A-E tests pass
ctest -R "csr|mret|pipeline|interrupt|soc" --output-on-failure
# Expected: 100% tests pass

# 3. riscv-tests ISA suite (Phase D.5)
./tests/test_riscv_tests_suite
# Expected: "All 40 ISA tests pass"

# 4. Hello World output
./tests/test_soc_integration
# Expected: UART output="Hello from riscv-mini\r\n"

# 5. Optional: FreeRTOS boot (if Phase E done)
./tests/test_freertos_boot
# Expected: "FreeRTOS boot successful, 2 tasks switched ≥10 times"
```

---

## File-by-File Implementation Order

### Week 1: Phase A (Days 1-5)
```
Day 1: A1 - rv32i_csr.h (CSR bank skeleton)
Day 2: A2 - rv32i_csr.h (mstatus/misa bit fields)
Day 3: A3 - rv32i_decoder.h (SYSTEM extension)
Day 4: A4 - rv32i_system.h (SYSTEM executor)
Day 5: A5 - tests/test_rv32i_csr.cpp, tests/test_mret.cpp (CSR tests)
```

### Week 2: Phase B Part 1 (Days 6-10)
```
Day 6: B1 - rv32i_pipeline.h (top module rewrite)
Day 7: B1 continued - stages connections
Day 8: B2 - rv32i_control.h (control unit)
Day 9: B3 - i_tcm.h (I-TCM integration)
Day 10: B4 - d_tcm.h (D-TCM integration)
```

### Week 3: Phase B Part 2 + C Part 1 (Days 11-15)
```
Day 11: B5 - tests/test_pipeline_execution.cpp (pipeline tests)
Day 12: B5 continued + bug fixes
Day 13: C1 - include/chlib/clint.h (CLINT module)
Day 14: C2 - rv32i_exception.h (exception unit)
Day 15: C3 - rv32i_pipeline.h (IRQ pin)
```

### Week 4: Phase C Part 2 + Phase D (Days 16-20)
```
Day 16: C4 - tests/test_timer_interrupt.cpp (interrupt tests)
Day 17: D1 - address_decoder.h (address decoder)
Day 18: D2 - axi_uart.h (UART model)
Day 19: D3 - firmware_loader.h (HEX loader)
Day 20: D4 - tests/test_soc_integration.cpp (SoC tests)
```

### Week 5: Phase D.5 + Phase E (Days 21-25, optional)
```
Day 21: D.5 - riscv-tests integration
Day 22: D.5 continued + verification
Day 23: E1 - FreeRTOSConfig.h
Day 24: E2 - port.c
Day 25: E3 - portASM.S
```

### Week 6: Phase E Completion (Days 26-30, optional)
```
Day 26: E4 - demo_main.c
Day 27: E5 - test_freertos_boot.cpp
Day 28: E5 continued + bug fixes
Day 29: E6 - performance tuning
Day 30: Final verification + documentation
```

---

## Delegation Strategy for Subagents

### Parallel Subagent Execution

This plan is structured to maximize parallel delegation. Here's the strategy:

#### Wave 1 Parallel Delegation (Day 1)
Fire these 4 tasks **simultaneously** as independent subagents:

```python
# Subagent 1: Phase A - CSR Bank
task(
    category="cpp-pro",
    load_skills=["cpp-pro"],
    description="A1: CSR Register Bank",
    prompt="Implement CSR register bank as defined in Phase A Task A1. File: examples/riscv-mini/src/rv32i_csr.h. Requirements: 10 CSR registers (mstatus, mie, mtvec, mscratch, mepc, mcause, mtval, mip, misa), __io__ with csr_addr[12], csr_wdata[32], csr_rdata[32], csr_write_en, mtip, meip. Reset values per spec.",
    run_in_background=True
)

# Subagent 2: Phase B - Pipeline Top
task(
    category="cpp-pro",
    load_skills=["cpp-pro", "cpp-architecture"],
    description="B1: Pipeline Top Rewrite",
    prompt="Rewrite pipeline top module from scratch. File: examples/riscv-mini/src/rv32i_pipeline.h. Fix 4 critical bugs: line 121 PC≠instruction, line 131 register data as PC, line 166-168 hazard broken, line 147 memory always ready. Implement clean IF→ID→EX→MEM→WB flow.",
    run_in_background=True
)

# Subagent 3: Phase C - CLINT
task(
    category="cpp-pro",
    load_skills=["cpp-pro"],
    description="C1: CLINT Module",
    prompt="Implement CLINT interrupt controller. File: include/chlib/clint.h. Requirements: 64-bit mtime counter, 64-bit mtimecmp comparator, mtip output when mtime>=mtimecmp, tick() method. Address decode for 0x40000000-0x40000FFF.",
    run_in_background=True
)

# Subagent 4: Phase D - Address Decoder
task(
    category="cpp-pro",
    load_skills=["cpp-pro"],
    description="D1: Address Decoder",
    prompt="Implement MMIO address decoder. File: examples/riscv-mini/src/address_decoder.h. Requirements: decode 5 regions (I-TCM=0x20000000, D-TCM=0x20010000, CLINT=0x40000000, UART=0x40001000, GPIO=0x40002000), generate chip selects.",
    run_in_background=True
)
```

**Rationale**: These 4 tasks are truly independent:
- A1 (CSR): Pure register definition, no dependencies
- B1 (Pipeline): Structural rewrite, needs only placeholder CSR interface
- C1 (CLINT): Independent timer module
- D1 (Address decoder): Pure combinational logic

**Expected Output**: All 4 files compile, can be integrated in next wave.

#### Wave 2 Parallel Delegation (Day 3-4)

After Wave 1 completes, fire next wave:

```python
# Subagent 5: Phase A - Decoder Extension
task(
    category="cpp-pro",
    load_skills=["cpp-pro"],
    description="A3: Decoder SYSTEM",
    prompt="Extend decoder with SYSTEM opcodes. File: examples/riscv-mini/src/rv32i_decoder.h. Add enum SystemFunc3, enum SystemFunct12, detect opcode=0b1110011, outputs: is_system, system_func3[3], csr_addr[12], system_funct12[12].",
    run_in_background=True
)

# Subagent 6: Phase B - Control Unit
task(
    category="cpp-pro",
    load_skills=["cpp-pro"],
    description="B2: Control Unit",
    prompt="Implement control unit. File: examples/riscv-mini/src/rv32i_control.h. Decode opcodes: R-type=0110011, I-type=0010011, Load=0000011, Store=0100011, Branch=1100011, JAL=1101111. Generate reg_write, alu_src, mem_read, mem_write, branch, jump signals.",
    run_in_background=True
)

# Subagent 7: Phase D - UART
task(
    category="cpp-pro",
    load_skills=["cpp-pro"],
    description="D2: UART Model",
    prompt="Implement UART 16550 model. File: include/axi4/peripherals/axi_uart.h. TX FIFO (16 entries), TX holding register, char output on write, address 0x40001000.",
    run_in_background=True
)
```

This pattern continues per wave.

---

## TODO List (ADD THESE)

Use `todowrite` tool to create these tasks and execute by wave.

> **ZERO-DEBT ENFORCEMENT**: Before advancing to the next wave batch, run the Gate Enforcement Script. ALL gates must pass.

### Wave 0: Phase 0.5 — Debug Infrastructure (FIRST — Block ALL other work)

- [ ] **G0.5: Add VCD Waveform Dump + Register Snapshot Infrastructure**
  - What: Add `ch::SimulatorConfig` with trace enable, VCD file output, register state snapshot API, UART console capture
  - Files: `include/chlib/simulator_trace.h` (new), `examples/riscv-mini/tests/test_trace.cpp` (new)
  - Depends: None
  - Blocks: ALL waves (debugging prerequisite)
  - Category: `cpp-pro`
  - Skills: [`cpp-pro`, `cpp-debug`]
  - QA: Generate 10-cycle VCD file, verify file exists and contains signal traces
  - After passing: Create `.sisyphus/artifacts/debug-infrastructure-pattern.md`

---

### Wave 1 (After G0.5 Passes Gate)

- [ ] **A1: CSR Register Bank Definition**
  - What: Create rv32i_csr.h with CsrBank class, 10 CSR registers, __io__ interface
  - Gate M1: clang-check clean, 10 registers read/write, reset values correct, 0 TODO/FIXME
  - Depends: G0.5
  - Blocks: A2, B1a
  - Category: `cpp-pro` / Skills: [`cpp-pro`]
  - QA: `cmake --build build` clean, reset values verified

- [ ] **B1a: Pipeline Skeleton** (0.5 day — split from B1)
  - What: Define IF/ID/EX/MEM/WB stage structure with clean interfaces, inter-stage registers
  - Depends: G0.5
  - Blocks: B1b
  - Category: `cpp-pro` / Skills: [`cpp-pro`, `cpp-architecture`]
  - QA: All 5 stages instantiate, VCD generated, 0 TODO/FIXME

- [ ] **C1: CLINT Module**
  - What: Create include/chlib/clint.h with mtime/mtimecmp logic
  - Depends: G0.5
  - Blocks: C2, E2
  - Category: `cpp-pro` / Skills: [`cpp-pro`]
  - QA: mtime increments every tick, mtip triggers at mtimecmp

- [ ] **D1: Address Decoder**
  - What: Create address_decoder.h with 5-region decode (I-TCM, D-TCM, CLINT, UART, GPIO)
  - Depends: G0.5
  - Blocks: D2
  - Category: `cpp-pro` / Skills: [`cpp-pro`]
  - QA: Test all 5 address ranges decode correctly

> **GATE ENFORCEMENT**: Run Gate Script after Wave 1. Must pass before Wave 2.

---

### Wave 2 (After Wave 1 Passes Gates)

- [ ] **A2: mstatus/misa Bit Fields** — Depends: A1
  - QA: writeMIE(true) → readMIE() returns true, 0 TODO/FIXME

- [ ] **B1b: Hazard Unit + Forwarding** (1 day — split from B1)
  - What: Implement forwarding paths (EX→EX, MEM→EX, WB→EX), load-use stall, branch flush
  - Depends: B1a
  - Blocks: B1c
  - QA: VCD shows clean signal propagation, 0 TODO/FIXME

- [ ] **B1c: Control Signal Wiring** (0.5 day — split from B1)
  - What: Connect ControlUnit outputs, complete memory handshake
  - Depends: B1b
  - Blocks: B2, B3, B4, C2, C3
  - **MICRO-GATE: ALL 4 critical bugs verified fixed with specific test vectors**
  - QA: 4 bug-specific test vectors pass, clang-tidy zero warnings

- [ ] **D2: UART Model** (MODIFY existing axi_uart.h)
  - What: Add MMIO wrapper to existing `axi_uart.h` (146 lines)
  - Depends: D1
  - Blocks: D4
  - QA: Write 'H' → tx_char='H' output

- [ ] **D3: HEX Loader** (moved from Wave 1)
  - What: Create firmware_loader.h with HEX parser
  - Depends: None
  - Blocks: D4
  - QA: Load test.hex → verify I-TCM contents, checksum verified

> **GATE ENFORCEMENT**: Run Gate Script after Wave 2. Must pass before Wave 3.

---

### Wave 3 (After Wave 2 Passes Gates)

- [ ] **A3: Decoder SYSTEM Extension** — Depends: A2
- [ ] **C2: Exception Unit** — Depends: A2, B1c (both CSR AND pipeline)
- [ ] **C3: Pipeline IRQ Pin** — Depends: B1c
- [ ] **B2: Control Unit** — Depends: B1c
- [ ] **B3: I-TCM Integration** — Depends: B1c
- [ ] **B4: D-TCM Integration** — Depends: B1c

> **GATE ENFORCEMENT**: These can run in parallel (A3/C2/C3 stream + B2/B3/B4 stream). Gates checked after all complete.

---

### Wave 4 (After Wave 3 Passes Gates)

- [ ] **A4: SYSTEM Instruction Executor** — Depends: A3
- [ ] **A5: CSR Unit Tests** — Depends: A4
- [ ] **B5: Pipeline Execution Tests** — Depends: B2, B3, B4
- [ ] **C4: Interrupt Flow Tests** — Depends: C2, C3

---

### Wave 5 (After Wave 4 Passes Gates — Phase A/B/C Complete)

- [ ] **D4: SoC Integration Tests** — Depends: D2, D3
- [ ] **D.5: riscv-tests Suite** (MANDATORY) — Depends: D4
  - PREREQUISITE: Verify `riscv32-unknown-elf-gcc --version` works
  - If toolchain missing: install before starting D.5
  - QA: ALL 40 ISA tests pass, tohost=1

---

### Phase-End Skill Extraction (After Each Phase Gate Passes)

- [ ] **Phase 0.5 Artifact** → `.sisyphus/artifacts/debug-infrastructure-pattern.md`
- [ ] **Phase A Artifact** → `.sisyphus/artifacts/csr-implementation-pattern.md`
- [ ] **Phase B Artifact** → `.sisyphus/artifacts/pipeline-integration-pattern.md`
- [ ] **Phase C Artifact** → `.sisyphus/artifacts/trap-handling-pattern.md`
- [ ] **Phase D Artifact** → `.sisyphus/artifacts/soc-integration-pattern.md`
- [ ] **Phase D.5 Artifact** → `.sisyphus/artifacts/riscv-tests-integration.md`
- [ ] **Phase E Artifact** (if done) → `.sisyphus/artifacts/freertos-riscv-porting.md`

Each artifact must satisfy this checklist:
- [ ] Context: when to use this pattern
- [ ] Structure: components + data flow diagram
- [ ] Implementation: step-by-step with C++ code
- [ ] Test Vectors: happy path + edge cases
- [ ] Known Failure Modes: what went wrong during implementation
- [ ] Related Patterns: cross-references

---

## Execution Instructions (REVISED)

### Step 0: Phase 0.5 — Debug Infrastructure (FIRST)
```python
task(category="cpp-pro", load_skills=["cpp-pro", "cpp-debug"], run_in_background=true, description="G0.5: Debug Infrastructure", prompt="Add VCD waveform dump capability and register state snapshot API to the simulator. Files: include/chlib/simulator_trace.h (new). Deliverables: ch::SimulatorConfig with trace_enabled, trace_vcd output, register snapshot API. Gate: Generate 10-cycle VCD, verify. Must have: 0 TODO/FIXME, compiles clean.")
```
**After G0.5 passes**: Run Gate Script → create artifacts directory `.sisyphus/artifacts/`

### Step 1: Wave 1 (4 Parallel — After G0.5 Gate Passes)

> **GATE ENFORCEMENT**: After Wave 1 tasks complete, run Gate Script. ALL 4 gates (compile/test/doc/clean) must pass before Wave 2 starts.

```python
task(category="cpp-pro", load_skills=["cpp-pro"], run_in_background=true, description="A1: CSR Bank", prompt="...")
task(category="cpp-pro", load_skills=["cpp-pro", "cpp-architecture"], run_in_background=true, description="B1a: Pipeline Skeleton", prompt="...")
task(category="cpp-pro", load_skills=["cpp-pro"], run_in_background=true, description="C1: CLINT", prompt="...")
task(category="cpp-pro", load_skills=["cpp-pro"], run_in_background=true, description="D1: Address Decoder", prompt="...")
```

### Step 2: Wave 2 (5 Parallel — After Wave 1 Gate Passes)

> **GATE ENFORCEMENT**: After Wave 2 tasks complete, run Gate Script.

```python
task(category="cpp-pro", load_skills=["cpp-pro"], run_in_background=true, description="A2: mstatus/misa", prompt="...")
task(category="cpp-pro", load_skills=["cpp-pro", "cpp-architecture"], run_in_background=true, description="B1b: Hazard+Forwarding", prompt="...")
task(category="cpp-pro", load_skills=["cpp-pro"], run_in_background=true, description="D2: UART (modify existing axi_uart.h)", prompt="READ existing include/axi4/peripherals/axi_uart.h (146 lines) first. This file ALREADY EXISTS. Do NOT create a new file. Add MMIO wrapper to existing AxiLiteUart class or adapt to MMIO protocol. Verify gate: Write 'H' → tx_char='H'.")
task(category="cpp-pro", load_skills=["cpp-pro"], run_in_background=true, description="D3: HEX Loader", prompt="...")
```

After B1b completes: sequential B1c with micro-gate:
```python
task(category="cpp-pro", load_skills=["cpp-pro", "cpp-architecture"], run_in_background=true, description="B1c: Control Wiring", prompt="Complete control signal wiring. MUST verify ALL 4 critical bugs fixed with specific test vectors.")
```

### Step 3: Wave 3 (6 Parallel — After Wave 2 Gate Passes)

```python
task(category="cpp-pro", load_skills=["cpp-pro"], run_in_background=true, description="A3: Decoder SYSTEM", prompt="...")
task(category="cpp-pro", load_skills=["cpp-pro"], run_in_background=true, description="C2: Exception Unit", prompt="Depends on A2 (CSR) AND B1c (pipeline).")
task(category="cpp-pro", load_skills=["cpp-pro"], run_in_background=true, description="C3: Pipeline IRQ", prompt="...")
task(category="cpp-pro", load_skills=["cpp-pro"], run_in_background=true, description="B2: Control Unit", prompt="...")
task(category="cpp-pro", load_skills=["cpp-pro"], run_in_background=true, description="B3: I-TCM", prompt="...")
task(category="cpp-pro", load_skills=["cpp-pro"], run_in_background=true, description="B4: D-TCM", prompt="...")
```

### Step 4: Wave 4 (4 Parallel)

```python
task(category="cpp-pro", load_skills=["cpp-pro"], run_in_background=true, description="A4: SYSTEM Executor", prompt="...")
task(category="cpp-pro", load_skills=["cpp-pro"], run_in_background=true, description="A5: CSR Tests", prompt="...")
task(category="cpp-pro", load_skills=["cpp-pro"], run_in_background=true, description="B5: Pipeline Tests", prompt="...")
task(category="cpp-pro", load_skills=["cpp-pro"], run_in_background=true, description="C4: Interrupt Tests", prompt="...")
```

### Step 5: Wave 5 (Phase D — D.5 is MANDATORY)

```python
task(category="cpp-pro", load_skills=["cpp-pro"], run_in_background=true, description="D4: SoC Integration", prompt="...")
# After D4 passes Gate → D.5 (MANDATORY)
task(category="cpp-pro", load_skills=["cpp-pro"], run_in_background=true, description="D.5: riscv-tests (MANDATORY)", prompt="PREREQUISITE: Verify riscv32-unknown-elf-gcc is installed. Clone riscv-tests suite, compile for RV32I, load and run via CppHDL test harness. Gate: ALL 40 ISA tests pass, tohost=1.")
```

### Phase-End Skill Extraction Task

After each phase gate passes, create the artifact:
```python
task(category="writing", load_skills=["obsidian-markdown"], run_in_background=false, description="Phase X Artifact", prompt="Create .sisyphus/artifacts/phase-x-summary.md documenting: context, structure, implementation steps, test vectors, known failure modes, related patterns.")
```

---

**Plan Status**: Ready for execution  
**First Action**: Execute Wave 0 (G0.5: Debug Infrastructure)  
**Revised Target**: Day 22-27 (Phase D+D.5 mandatory), Day 34 (Phase E optional)
