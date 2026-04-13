# riscv-mini SoC 架构设计文档

**版本**: 1.1  
**创建日期**: 2026-04-13  
**评审日期**: 2026-04-14  
**状态**: 评审后修订  
**关联文档**: [01-specification.md](01-specification.md)

---

## 1. 架构概述

### 1.1 设计目标

设计一个支持 FreeRTOS 运行的最小 RISC-V SoC，满足以下目标:

1. **功能完整性**: 支持 RV32I + SYSTEM 指令，CLINT 中断，内存映射外设
2. **仿真友好**: C++ 仿真环境，无需 FPGA，可加载 ELF/HEX 固件
3. **可扩展性**: AXI4-Lite 总线架构，支持未来添加更多外设
4. **零债务**: 每个模块有独立测试，编译 0 警告，文档同步

### 1.2 架构分层

```
┌─────────────────────────────────────────────────────────┐
│                    Application Layer                    │
│              (FreeRTOS + User Tasks)                    │
└─────────────────────────────────────────────────────────┘
                          │
┌─────────────────────────────────────────────────────────┐
│                   Software Stack Layer                  │
│    (RISC-V Port, Trap Handler, Driver Layer)            │
└─────────────────────────────────────────────────────────┘
                          │
┌─────────────────────────────────────────────────────────┐
│                    Hardware Abstraction                 │
│         (Memory Map, CSR, Interrupt Controller)         │
└─────────────────────────────────────────────────────────┘
                          │
┌─────────────────────────────────────────────────────────┐
│                   SoC Interconnect Layer                │
│            (AXI4-Lite, Address Decoder)                 │
└─────────────────────────────────────────────────────────┘
                          │
┌─────────────────────────────────────────────────────────┐
│                    Component Layer                      │
│   (CPU, TCM, CLINT, UART, GPIO, Firmware Loader)        │
└─────────────────────────────────────────────────────────┘
```

---

## 2. 组件详细设计

### 2.1 RV32I CPU (5-Stage Pipeline)

#### 2.1.1 流水线级

| 级 | 模块 | 功能 | 文件 |
|----|------|------|------|
| IF | `IfStage` | PC 更新，指令 fetch，branch/jump 目标 | `stages/if_stage.h` |
| ID | `IdStage` | 指令译码，寄存器读，立即数生成 | `stages/id_stage.h` |
| EX | `ExStage` | ALU 运算，branch 条件判断 | `stages/ex_stage.h` |
| MEM | `MemStage` | Load/Store 地址计算，数据访问 | `stages/mem_stage.h` |
| WB | `WbStage` | 写回选择，x0 保护 | `stages/wb_stage.h` |

#### 2.1.2 关键信号流

```
┌──────────────────────────────────────────────────────────┐
│                        IF Stage                          │
│  PC_reg ──► I-TCM ──► instr ──► IF/ID pipeline register │
│     ▲                              │                     │
│     │◄───── branch_target ◄────────┘ (from EX stage)    │
│     │                              │                     │
│     └───── PC+4 / branch / jump ◄──┘                    │
└──────────────────────────────────────────────────────────┘
                          │ IF/ID
                          ▼
┌──────────────────────────────────────────────────────────┐
│                        ID Stage                          │
│  instr ──► Decoder ──► control_signals                   │
│          ──► RegFile (rs1, rs2 read)                     │
│          ──► ImmGen ──► immediate                        │
│                          │                               │
│                          │ ID/EX pipeline register       │
└──────────────────────────────────────────────────────────┘
                          │
                          ▼
┌──────────────────────────────────────────────────────────┐
│                        EX Stage                          │
│  rs1_data, rs2_data ──► Forwarding MUX                   │
│                       ──► ALU ──► result                 │
│                       ──► Branch Comparator              │
│                          │                               │
│                          │ EX/MEM pipeline register      │
└──────────────────────────────────────────────────────────┘
                          │
                          ▼
┌──────────────────────────────────────────────────────────┐
│                       MEM Stage                          │
│  addr ──► D-TCM (load/store)                             │
│  alu_result ──► MEM/WB pipeline register                 │
└──────────────────────────────────────────────────────────┘
                          │
                          ▼
┌──────────────────────────────────────────────────────────┐
│                        WB Stage                          │
│  MEM_result / ALU_result ──► Wb MUX                      │
│                            ──► RegFile (write)           │
│                            ──► x0 hardwire protection    │
└──────────────────────────────────────────────────────────┘
```

#### 2.1.3 冒险处理

```cpp
// HazardUnit 输入
struct HazardInputs {
    ch_uint<5> id_rs1_addr, id_rs2_addr;  // ID 阶段读寄存器地址
    ch_uint<5> ex_rd_addr, mem_rd_addr, wb_rd_addr;  // 各阶段目标
    ch_bool ex_reg_write, mem_reg_write, wb_reg_write; // 写使能
    ch_bool mem_is_load;  // Load-Use 检测
    ch_bool ex_branch, ex_branch_taken;  // 分支控制
};

// HazardUnit 输出
struct HazardOutputs {
    ch_uint<2> forward_a, forward_b;  // RS1/RS2 前推源选择
    ch_bool stall;  // Load-Use 停顿
    ch_bool flush;  // 分支错误冲刷
    ch_bool pc_src; // PC 源选择
};
```

### 2.2 CSR 单元 (Phase A 新增)

#### 2.2.1 CSR 寄存器组

```cpp
// rv32i_csr.h
struct CsrBank {
    ch_uint<32> mstatus;    // MIE, MPIE, MPP
    ch_uint<32> mie;        // MTIE, MEIE
    ch_uint<32> mtvec;      // Trap base
    ch_uint<32> mscratch;   // Temporary
    ch_uint<32> mepc;       // Exception PC
    ch_uint<32> mcause;     // Exception cause
    ch_uint<32> mtval;      // Exception value
    ch_uint<32> mip;        // Interrupt pending
    ch_uint<64> mtime;      // CLINT counter
    ch_uint<64> mtimecmp;   // CLINT compare
};
```

#### 2.2.2 SYSTEM 指令执行

```cpp
// rv32i_system.h
void executeSystemInstr(SystemInstr instr, CsrBank& csr, ch_uint<32>& rd) {
    switch (instr.opcode) {
        case OP_CSRRW:
            rd = csr.read(instr.csr_addr);
            csr.write(instr.csr_addr, instr.rs1_data);
            break;
        case OP_CSRRS:
            rd = csr.read(instr.csr_addr);
            csr.write(instr.csr_addr, rd | instr.rs1_data);
            break;
        case OP_CSRRC:
            rd = csr.read(instr.csr_addr);
            csr.write(instr.csr_addr, rd & ~instr.rs1_data);
            break;
        case OP_MRET:
            pc = csr.mepc;
            csr.mstatus.MIE = csr.mstatus.MPIE;
            csr.mstatus.MPIE = 1;
            break;
    }
}
```

### 2.3 CLINT (Core-Local Interruptor)

#### 2.3.1 架构

```
┌───────────────────────────────────────┐
│                CLINT                  │
│                                       │
│  mtime (64-bit counter)               │
│    │                                  │
│    ▼                                  │
│  Comparator (mtime >= mtimecmp?)      │
│    │                                  │
│    ▼                                  │
│  mtip (Machine Timer Interrupt Pend)  │
│    │                                  │
└────┼──────────────────────────────────┘
     │
     │ (IRQ pin to CPU)
     ▼
┌───────────────────────────────────────┐
│              CPU Core                 │
│                                       │
│  irq_pin ──► InterruptChecker         │
│              │                        │
│              ▼ (if mie.MTIE && mstatus.MIE)
│              ExceptionUnit            │
│                │                      │
│                ▼                      │
│              mcause = 0x80000007      │
│              mepc = PC                │
│              mstatus.MPIE = MIE       │
│              mstatus.MIE = 0          │
│              PC = mtvec               │
└───────────────────────────────────────┘
```

#### 2.3.2 寄存器映射

| 地址 | 寄存器 | 位宽 | 读写 | 复位值 |
|------|--------|------|------|--------|
| `0x40000000` | `mtime[31:0]` | 32 | R/W | 0 |
| `0x40000004` | `mtime[63:32]` | 32 | R/W | 0 |
| `0x40000008` | `mtimecmp[31:0]` | 32 | R/W | 0xFFFFFFFF |
| `0x4000000C` | `mtimecmp[63:32]` | 32 | R/W | 0xFFFFFFFF |

### 2.4 SoC 互连 (直接 MMIO 地址解码)

> **修订 v1.1**: Metis 建议初期使用直接 MMIO 地址解码代替 AXI4-Lite。只有 1 master + 3 slaves，不需要总线仲裁器。

#### 2.4.1 地址解码

```cpp
// apb_interconnect.h
struct AddressDecoder {
    static constexpr uint32_t I_TCM_BASE  = 0x20000000;
    static constexpr uint32_t D_TCM_BASE  = 0x20010000;
    static constexpr uint32_t CLINT_BASE  = 0x40000000;
    static constexpr uint32_t UART_BASE   = 0x40001000;
    static constexpr uint32_t GPIO_BASE   = 0x40002000;
    
    Slave selectSlave(uint32_t addr) {
        if (addr >= I_TCM_BASE && addr < 0x20010000) return Slave::ITCM;
        if (addr >= D_TCM_BASE && addr < 0x20020000) return Slave::DTCM;
        if (addr >= CLINT_BASE && addr < 0x40001000)  return Slave::CLINT;
        if (addr >= UART_BASE && addr < 0x40002000)   return Slave::UART;
        if (addr >= GPIO_BASE && addr < 0x40003000)   return Slave::GPIO;
        return Slave::ERROR;
    }
};
```

#### 2.4.2 互连时序

```
Cycle 1: CPU发出读请求 (addr, read_en)
Cycle 2: AddressDecoder 选中 slave
Cycle 3: Slave 读取数据
Cycle 4: 数据返回 CPU

Cycle 1: CPU 发出写请求 (addr, data, write_en, strbe)
Cycle 2: AddressDecoder 选中 slave
Cycle 3: Slave 写入数据
Cycle 4: 写完成响应
```

### 2.5 外设模块

#### 2.5.1 UART 16550 (简化)

```cpp
// uart_16550.h
struct Uart16550 {
    __io(
        ch_in<ch_uint<32>> addr;       // AXI 地址
        ch_in<ch_bool> read_en;        // AXI 读使能
        ch_in<ch_bool> write_en;       // AXI 写使能
        ch_in<ch_uint<32>> write_data; // AXI 写数据
        ch_out<ch_uint<32>> read_data; // AXI 读数据响应
        
        ch_out<ch_bool> irq;           // TX/RX 中断
    )
    
    // 内部寄存器
    ch_uint<8> tx_fifo[16];  // TX FIFO 16 级
    ch_uint<8> rx_fifo[16];  // RX FIFO 16 级
    ch_uint<5> tx_ptr, rx_ptr;
};
```

#### 2.5.2 GPIO

```cpp
// axi_gpio.h
struct Ax_gpio {
    __io(
        ch_in<ch_uint<32>> addr;
        ch_in<ch_bool> read_en, write_en;
        ch_in<ch_uint<32>> write_data;
        ch_out<ch_uint<32>> read_data;
        ch_out<ch_uint<32>> gpio_out;  // 32-bit GPIO 输出
        ch_in<ch_uint<32>>  gpio_in;   // 32-bit GPIO 输入
    )
};
```

### 2.6 Firmware Loader

#### 2.6.1 ELF 解析

```cpp
// firmware_loader.h
struct ElfProgramHeader {
    uint32_t p_type;
    uint32_t p_offset;
    uint32_t p_vaddr;
    uint32_t p_filesz;
    uint32_t p_memsz;
};

class FirmwareLoader {
public:
    static ElfBinary loadELF(const std::string& path) {
        // 1. 解析 ELF header
        // 2. 读取 program headers
        // 3. 加载 .text/.data/.rodata segments
        // 4. 返回 entry point
    }
};
```

#### 2.6.2 HEX 格式支持

```
// Intel HEX 格式示例
:1020000013000000130000001300000013000000D4
:1020100063400000634000006340000063400000AA
:00000001FF  // EOF
```

---

## 3. 接口定义

### 3.1 CPU 顶层接口

```cpp
// rv32i_cpu.h
struct Rv32iCpuIO {
    // 系统控制
    ch_in<ch_bool>  rst;
    ch_in<ch_bool>  clk;
    
    // 中断输入
    ch_in<ch_bool>  mtip;  // Machine Timer
    ch_in<ch_bool>  meip;  // Machine External
    
    // 指令接口 (I-TCM)
    ch_out<ch_uint<32>> instr_addr;
    ch_in<ch_uint<32>>  instr_data;
    ch_in<ch_bool>      instr_ready;
    
    // 数据接口 (D-TCM + 外设)
    ch_out<ch_uint<32>> data_addr;
    ch_out<ch_uint<32>> data_write_data;
    ch_out<ch_uint<4>>  data_strbe;
    ch_out<ch_bool>     data_write_en;
    ch_in<ch_uint<32>>  data_read_data;
    ch_in<ch_bool>      data_ready;
};
```

### 3.2 SoC 顶层接口

```cpp
// rv32i_soc.h
struct Rv32iSocIO {
    // 系统
    ch_in<ch_bool> rst;
    ch_in<ch_bool> clk;
    
    // UART (外部输出)
    ch_out<ch_bool> uart_tx;
    ch_in<ch_bool>  uart_rx;
    
    // GPIO
    ch_out<ch_uint<32>> gpio_out;
    ch_in<ch_uint<32>>  gpio_in;
};
```

---

## 4. 仿真模型

### 4.1 测试平台架构

```
┌─────────────────────────────────────────────────────────┐
│                   C++ Testbench                         │
│                                                         │
│  int main() {                                           │
│    // 1. 创建 Context                                   │
│    ch::core::context ctx("soc_test");                   │
│                                                         │
│    // 2. 创建 SoC                                       │
│    Rv32iSoc soc;                                        │
│                                                         │
│    // 3. 加载固件                                       │
│    FirmwareLoader::loadELF(soc, "demo.elf");            │
│                                                         │
│    // 4. 创建仿真器                                     │
│    ch::Simulator sim(&ctx);                             │
│                                                         │
│    // 5. 复位序列                                       │
│    sim.set_input_value(soc.io().rst, 1);                │
│    sim.tick();                                          │
│    sim.set_input_value(soc.io().rst, 0);                │
│                                                         │
│    // 6. 运行循环                                       │
│    for (uint64_t i = 0; i < MAX_TICKS; i++) {           │
│      sim.tick();                                        │
│      soc.clint.tick();  // 更新 mtime                   │
│      check_uart_output();                               │
│    }                                                    │
│  }                                                      │
│                                                         │
└─────────────────────────────────────────────────────────┘
```

### 4.2 仿真时序

```
Cycle 0:  rst=1 (复位)
Cycle 1:  rst=0 (释放复位), PC=0x20000000
Cycle 2:  IF fetch instr from I-TCM[0]
Cycle 3:  ID decode
Cycle 4:  EX execute
Cycle 5:  MEM access
Cycle 6:  WB writeback
...
Cycle N:  mtime >= mtimecmp → mtip=1
Cycle N+1: CPU 检测中断 → trap entry
Cycle N+4: 执行 trap handler (mret 后返回)
```

---

## 5. 关键设计决策

### 5.1 Cache 跳过

**决策**: 不使用 I-Cache / D-Cache  
**理由**:
1. FreeRTOS + Demo ≤ 8KB，TCM 足够
2. Cache 增加复杂度 (hit/miss, LRU, 相干性)
3. 仿真性能影响小 (TCM 单周期)
4. 参考：VexRiscv Murax SoC 片上 8KB RAM 无 Cache

### 5.2 指令集选择

**决策**: RV32I + SYSTEM (51 条指令)  
**排除**: M (乘除), F/D (浮点), A (原子)  
**理由**:
1. FreeRTOS 不需要硬件乘除 (软件模拟)
2. 无浮点运算需求
3. 单核 M-mode 无需原子操作

### 5.3 总线选择

**决策**: AXI4-Lite (简化 APB 语义)  
**排除**: Wishbone, TileLink  
**理由**:
1. 项目已有 AXI4-Lite 基础设施
2. 与 chlib 外设兼容
3. Wishbone 需要新的互连逻辑

---

## 6. 验证计划

| 模块 | 测试文件 | 覆盖项 | 状态 |
|------|---------|-------|------|
| CSR | `test_rv32i_csr.cpp` | 读写/位操作/mret | ❌ |
| Pipeline | `test_pipeline_full.cpp` | 20+ 指令序列 | ❌ |
| CLINT | `test_clint_interrupt.cpp` | MTIP 触发/返回 | ❌ |
| Interconnect | `test_apb_decode.cpp` | 地址映射/片选 | ❌ |
| UART | `test_uart_loopback.cpp` | TX/RX FIFO | ❌ |
| Firmware | `test_elf_loader.cpp` | ELF 解析/加载 | ❌ |
| FreeRTOS | `test_freertos_boot.cpp` | 双任务切换 | ❌ |

---

## 7. 文件组织

```
examples/riscv-mini/
├── src/
│   ├── rv32i_csr.h           # Phase A
│   ├── rv32i_system.h        # Phase A
│   ├── rv32i_exception.h     # Phase C
│   ├── rv32i_pipeline.h      # Phase B (重写)
│   ├── rv32i_cpu.h           # Phase B
│   ├── rv32i_soc.h           # Phase D
│   ├── apb_interconnect.h    # Phase D
│   ├── uart_16550.h          # Phase D
│   └── clint.h               # Phase C
├── tests/
│   ├── test_rv32i_system.cpp       # Phase A
│   ├── test_pipeline_execution.cpp # Phase B
│   ├── test_timer_interrupt.cpp    # Phase C
│   ├── test_soc_integration.cpp    # Phase D
│   └── test_freertos_boot.cpp      # Phase E
├── freertos/
│   ├── config/FreeRTOSConfig.h
│   ├── port/port.c
│   ├── port/portASM.S
│   └── demo/demo_main.c
└── firmware/
    └── Makefile             # 交叉编译脚本
```

---

## 8. 修订历史

| 版本 | 日期 | 作者 | 变更说明 |
|------|------|------|---------|
| 0.1 | 2026-04-13 | CppHDL | 初始草案 |
| 1.0 | 待评审 | - | 评审后发布 |

