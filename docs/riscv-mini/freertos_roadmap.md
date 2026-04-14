# FreeRTOS on riscv-mini — Industrialization Roadmap

**Target:** Run FreeRTOS on rv32i-mini CPU within C++ simulation environment  
**Scope:** Pure C++ simulation only — no FPGA, no Linux, no hardware

---

## 0. Minimal ISA Subset

FreeRTOS requires **RV32I** + **SYSTEM extension**:

| Category | Instructions | Count | Status |
|----------|-------------|-------|--------|
| RV32I Base | All 40 instructions | 40 | ✅ Phases 1-3 pass |
| SYSTEM | `csrrw`, `csrrs`, `csrrc` | 3 | ❌ Not implemented |
| SYSTEM | `csrrwi`, `csrrsi`, `csrrci` | 3 | ❌ Not implemented |
| SYSTEM | `mret` | 1 | ❌ Not implemented |
| SYSTEM | `ebreak` | 1 | ❌ Not implemented |
| SYSTEM | `ecall` | 1 | ❌ Not implemented |
| SYSTEM | `wfi` | 1 | ❌ Skip (optional) |
| **Total Active** | | **44** | |

### What CAN Be Skipped (Confirmed)
- **M extension** — Not needed, FreeRTOS works with sw mul/div
- **A extension** — Not needed, disable/enable IRQ for critical sections
- **F/D extension** — Not needed, no floating point
- **I/D-Cache** — FreeRTOS fits in 64KB TCM
- **Supervisor Mode** — Bare-metal M-mode only
- **PMP** — Simulation, no protection
- **Branch Predictor** — Correctness > performance
- **WFI instruction** — Not required for functionality
- **Debug Module** — Headless simulation

---

## 1. Phase Diagram & Dependencies

```
Phase A (CSR) ──→ Phase C (Interrupts) ──→ Phase E (FreeRTOS)
                   ↑
Phase B (Pipeline) ─┘
                   ↓
Phase D (SoC+Peripherals) ──→ Phase E (FreeRTOS)
```

---

## 2. Phase A: CSR + SYSTEM Instructions

**Goal:** CPU can read/write CSR registers and execute SYSTEM instructions  
**Estimated Effort:** 2-3 days

### Deliverables

| File | Description |
|------|-------------|
| `examples/riscv-mini/src/rv32i_csr.h` | 16-entry CSR register bank: mstatus, mie, mtvec, mscratch, mepc, mcause, mtval, mip |
| `examples/riscv-mini/src/rv32i_system.h` | SYSTEM instruction executor: csrrw/csrrs/csrrc variants, mret, ecall, ebreak |
| `examples/riscv-mini/src/rv32i_decoder.h` (modify) | Add SYSTEM opcode (0b1110011) handling |
| `examples/riscv-mini/rv32i_system_test.cpp` | CSR read/write tests, MRET entry/return test |

### Critical CSR Registers

| CSR | Address | Purpose | FreeRTOS Use |
|-----|---------|---------|-------------|
| mstatus | 0x300 | Machine status (MIE/GIE) | Interrupt enable/disable |
| mie | 0x304 | Machine interrupt enable | Enable MTIP |
| mtvec | 0x305 | Trap vector base | Point to handler |
| mepc | 0x341 | Exception PC | Save/restore on trap |
| mcause | 0x342 | Trap cause code | Identify interrupt type |
| mtval | 0x343 | Trap value | Exception address |
| mip | 0x344 | Machine interrupt pending | Check MTIP pending |
| mscratch | 0x340 | Scratch register | Context save temp |

---

## 3. Phase B: Pipeline Integration — Full 5-Stage Execution

**Goal:** Complete Rv32iPipeline — all 5 stages wired into working simulation  
**Estimated Effort:** 3-4 days

### Current State Analysis

The existing `Rv32iPipeline` in `examples/riscv-mini/src/rv32i_pipeline.h` has:
- ✅ All 5 stage components defined (IfStage, IdStage, ExStage, MemStage, WbStage)
- ✅ Forwarding, hazard detection, branch predictor
- ❌ TODO stubs in describe() — e.g., `mem_stage.io().mem_valid <<= ch_bool(true)`
- ❌ No control unit — opcode→signal mapping missing
- ❌ MemStage mem_read_data is constant 0
- ❌ No integration with TCM (instruction/data memory)

### Deliverables

| File | Description |
|------|-------------|
| `examples/riscv-mini/src/rv32i_pipeline.h` (rewrite) | Complete IF→ID→EX→MEM→WB wiring, all signals connected |
| `examples/riscv-mini/src/rv32i_control.h` | Main control unit: opcode → reg_write, alu_src, mem_read, mem_write, branch, jump |
| `examples/riscv-mini/src/rv32i_tcm.h` (modify) | Add `loadProgram(vector<uint32_t>)` API, firmware initialization |
| `examples/riscv-mini/src/rv32i_cpu.h` | Top-level CPU: pipeline + I-TCM + D-TCM, `step()` function |
| `examples/riscv-mini/tests/test_pipeline_execution.cpp` | Load raw instructions, tick N cycles, verify results |

### Pipeline Wiring Checklist
- [x] IF stage: PC register, instruction fetch, branch target selection
- [x] ID stage: Register file read, immediate generation, control signals
- [x] EX stage: ALU with forwarding, branch condition, PC target
- [ ] MEM stage: Connect to D-TCM memory, load/store byte/half/word, sign/zero-extend
- [ ] WB stage: Connect write back to register file, mux ALU/mem/JAL
- [ ] Control: Main decoder generating all control signals per instruction
- [ ] SYSTEM instructions: Route to CSR unit in EX/MEM stages
- [ ] Exception: MRET PC override, trap vector injection

---

## 4. Phase C: Interrupt Controller + Exception Handling

**Goal:** CPU handles timer interrupts (MTIP) with proper trap entry/exit  
**Estimated Effort:** 3-4 days

### Deliverables

| File | Description |
|------|-------------|
| `include/chlib/clint.h` | CLINT model: 64-bit mtime counter, mtimecmp, generates MTIP signal |
| `examples/riscv-mini/src/rv32i_exception.h` | Exception detection, cause encoding, mtvec dispatch |
| `examples/riscv-mini/src/rv32i_pipeline.h` (modify) | Add `ch_in<ch_bool> mtip` IRQ input pin |
| `examples/riscv-mini/tests/test_exception_flow.cpp` | Trap entry → verify mepc/mcause → MRET → verify PC restored |
| `examples/riscv-mini/tests/test_timer_interrupt.cpp` | CLINT triggers MTIP → CPU catches → executes handler → MRET returns |

### Exception Flow Specification

**Exception Entry (when IRQ && mstatus.MIE && mie.MTIE):**
```
Phase MEM/WB boundary:
  1. mepc ← PC of interrupted instruction
  2. mcause ← 0x80000007 (machine timer interrupt, bit 31 = interrupt flag)
  3. mtval ← 0 (timer IRQ has no fault address)
  4. mstatus.MPIE ← mstatus.MIE
  5. mstatus.MIE ← 0 (disable further interrupts)
  6. PC ← mtvec[31:2] << 2  (trap vector base, direct mode)
```

**MRET (return from trap):**
```
  1. PC ← mepc
  2. mstatus.MIE ← mstatus.MPIE
  3. mstatus.MPIE ← 1
```

---

## 5. Phase D: SoC Interconnect + Peripherals + Firmware Loader

**Goal:** Memory-mapped SoC ready for firmware execution  
**Estimated Effort:** 3-4 days

### Memory Map

| Address Range | Component | Size |
|--------------|-----------|------|
| 0x0000_0000 - 0x1FFF_FFFF | NULL (access fault) | 512MB |
| 0x2000_0000 - 0x200F_FFFF | Boot ROM | 1MB |
| 0x4000_0000 - 0x4000_0FFF | CLINT | 4KB |
| 0x4000_1000 - 0x4000_1FFF | UART | 4KB |
| 0x4000_2000 - 0x7FFF_FFFF | Reserved | — |
| 0x8000_0000 - 0x800F_FFFF | RAM | 1MB |

### Deliverables

| File | Description |
|------|-------------|
| `examples/riscv-mini/src/apb_interconnect.h` | Address decoder, CPU → multiple peripherals |
| `examples/riscv-mini/src/uart_16550.h` | Simplified UART (tx buffer, rx buffer) |
| `examples/riscv-mini/src/rv32i_soc.h` | SoC top-level: CPU + ROM + RAM + CLINT + UART |
| `include/chlib/firmware_loader.h` | Parse RISC-V ELF/.hex → load into ch_mem vectors |
| `docs/memory_map.md` | Memory map documentation |
| `examples/riscv-mini/tests/test_soc_integration.cpp` | Load firmware, tick simulation, verify UART output |

### Testbench Architecture

```
main() → loadFirmware("demo.hex") → create SoC → while(running):
  sim.tick()              → Clock edge
  soc.clint.tick()        → Increment mtime, check mtimecmp
  soc.uart.tick()         → Drain tx buffer
  
  Check exit conditions:
    - PC halt address (0xDEAD_DEAD)
    - UART output matches expected
    - Max tick limit reached
```

---

## 6. Phase E: FreeRTOS Port + Boot Demo

**Goal:** Compile FreeRTOS for RV32I, load into simulation, verify tick  
**Estimated Effort:** 3-4 days

### Deliverables

| File | Description |
|------|-------------|
| `examples/riscv-mini/freertos/FreeRTOSConfig.h` | configMTIME_BASE=0x40000000, configMTIMECMP_BASE=0x40000008 |
| `examples/riscv-mini/freertos/port.c` | RISC-V port: context save/restore, tick handler |
| `examples/riscv-mini/freertos/portASM.S` | Trap handler, mret, register save/restore |
| `examples/riscv-mini/freertos/demo_main.c` | 2 tasks: tick counter + UART blink |
| `examples/riscv-mini/freertos/Makefile` | riscv32-unknown-elf-gcc cross-compile → .elf → .hex |
| `examples/riscv-mini/freertos/run_freertos_demo.cpp` | Load, simulate, verify UART output |

### Key FreeRTOS Configuration

```c
// FreeRTOSConfig.h (minimal)
#define configMTIME_BASE_ADDRESS    (0x40000000UL)
#define configMTIMECMP_BASE_ADDRESS (0x40000008UL)
#define configCPU_CLOCK_HZ          (1000000UL)  // 1 MHz simulated
#define configTICK_RATE_HZ          (1000)       // 1 kHz tick
#define configMAX_PRIORITIES        (5)
#define configMINIMAL_STACK_SIZE    (256)
#define configTOTAL_HEAP_SIZE       (16 * 1024)  // 16 KB
```

---

## 7. Critical Dependencies & Risks

### Blocking Dependencies
1. **Phase A → Phase C**: CSR registers required before interrupt handling
2. **Phase B → Phase C**: Pipeline IRQ pins needed for CLINT integration
3. **Phase C → Phase E**: Interrupt mechanism required for FreeRTOS tick

### Parallel Opportunities
- Phase B and Phase D can proceed in parallel (SoC interconnect doesn't need full pipeline)
- Phase E preparation (FreeRTOS config) can start during Phase D

### Risks
| Risk | Impact | Mitigation |
|------|--------|------------|
| Pipeline wiring complexity | High risk of subtle bugs | Start with non-pipelined single-cycle CPU first, then add pipeline |
| SRA arithmetic right shift | Known TODO in ALU | Fix ALU SRA during Phase B |
| ELF parsing complexity | High if full ELF loader | Skip full ELF, use simple .hex format initially |
| FreeRTOS port complexity | RISC-V port requires precise register handling | Use official FreeRTOS RISC-V port as reference |

---

## 8. Total Effort Estimate

| Phase | Days | Cumulative |
|-------|------|------------|
| A: CSR + SYSTEM | 2-3 | 2-3 |
| B: Pipeline Integration | 3-4 | 5-7 |
| C: Interrupts + CLINT | 3-4 | 8-11 |
| D: SoC + Peripherals | 3-4 | 11-15 |
| E: FreeRTOS Port + Demo | 3-4 | 14-19 |
| **Total** | **14-19 days** | |

---

## 9. Success Criteria

FreeRTOS is "running" when:
1. ✅ Firmware boots (PC starts at reset vector, not crashing)
2. ✅ CLINT generates timer interrupt at expected interval
3. ✅ CPU catches interrupt → trap handler → MRET (round trip)
4. ✅ FreeRTOS tick counter increments (visible in simulation)
5. ✅ At least 2 tasks scheduled and running (verified via UART output)

---

_Generated: 2026-04-13_  
_Status: Ready for execution — Phase A first_
