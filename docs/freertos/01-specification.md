# riscv-mini FreeRTOS 支持技术规格说明书

**版本**: 1.1  
**创建日期**: 2026-04-13  
**评审日期**: 2026-04-14  
**状态**: 评审后修订  
**作者**: CppHDL Team

---

## 1. 概述

### 1.1 目标

在 CppHDL C++ 仿真环境中实现 FreeRTOS RTOS 运行能力，为 riscv-mini RV32I CPU 提供完整的多任务调度、中断处理和外设访问能力。

> **注意**：FreeRTOS 是最终演示目标。CPU 正确性通过 bare-metal "Hello World" + riscv-tests ISA 套件验证，**不依赖** FreeRTOS 作为 Gate。

### 1.2 范围

- **包含**: RV32I 基础指令集 + SYSTEM 指令扩展、CLINT 中断控制器、SoC 互连（直接 MMIO 地址解码）、UART/Timer 外设、FreeRTOS porting layer
- **不包含**: Cache、M/F/D/A 扩展、Linux 支持、FPGA 综合、Debug/JTAG

### 1.3 设计约束

| 约束项 | 要求 |
|--------|------|
| 仿真环境 | C++ simulation only (Verilator 模式) |
| 内存模型 | TCM (Tightly Coupled Memory)，无 Cache |
| 指令集 | RV32I (40 条) + SYSTEM (11 条) = 51 条 |
| 中断模式 | Machine Mode Only (M-mode) |
| 外设总线 | 直接 MMIO 地址解码 (初期)，AXI4-Lite 可后续叠加 |
| 固件格式 | Intel HEX 优先 (Phase D)，ELF 延后到 Phase F |

### 1.4 评审发现修订记录

| # | 问题 | 来源 | 修订 |
|---|------|------|------|
| 1 | mstatus 复位值错误 | 自我评审 | `0x00001880` → `0x00000080` |
| 2 | mtime/mtimecmp 不应在 CSR Bank | RISC-V Spec v1.11 | 从 CSR map 移除，归入 CLINT MMIO |
| 3 | mtvec MODE 位未说明 | 自我评审 | 添加 [1:0] 模式位说明 |
| 4 | misa CSR 缺失 | 自我评审 | 添加到 CSR 映射 |
| 5 | SYSTEM 指令 funct12 编码缺失 | 自我评审 | 补充完整编码表 |
| 6 | ELF loader 过复杂 | Momus/Metis | 改用 HEX/Binary 优先，ELF 延后 Phase F |
| 7 | FreeRTOS 不应为 Gate | Metis | 重设 Gate 为 bare-metal hello world |

---

## 2. 系统架构

### 2.1 顶层框图

```
┌─────────────────────────────────────────────────────────────────┐
│                         riscv-mini SoC                          │
│                                                                 │
│  ┌─────────────┐    ┌─────────────┐    ┌─────────────────────┐ │
│  │   RV32I     │ I$ │    I-TCM    │    │   AXI4-Lite         │ │
│  │  5-Stage    │◄───┤   64KB      │    │   Interconnect      │ │
│  │  Pipeline   │   │  (ROM)      │    │   (1 Master /       │ │
│  │             │   │             │    │    N Slaves)        │ │
│  │  - IF       │   └─────────────┘    │                     │ │
│  │  - ID       │                      │  ┌───────────────┐  │ │
│  │  - EX       │   ┌─────────────┐    │  │    CLINT      │  │ │
│  │  - MEM      │ D$│    D-TCM    │    │  │  0x40000000   │  │ │
│  │  - WB       │──►│   64KB      │    │  │  - mtime      │  │ │
│  └─────────────┘   │  (RAM)      │    │  │  - mtimecmp   │  │ │
│        ▲           │             │    │  └───────────────┘  │ │
│        │           └─────────────┘    │  ┌───────────────┐  │ │
│        │                              │  │    UART       │  │ │
│        └─────────────────────────────►│  │  0x40001000   │  │ │
│                                       │  │  - TX FIFO    │  │ │
│                                       │  │  - RX FIFO    │  │ │
│                                       │  └───────────────┘  │ │
│                                       │  ┌───────────────┐  │ │
│                                       │  │    GPIO       │  │ │
│                                       │  │  0x40002000   │  │ │
│                                       │  └───────────────┘  │ │
│                                       └─────────────────────┘ │
└─────────────────────────────────────────────────────────────────┘
```

### 2.2 内存映射

| 起始地址 | 结束地址 | 大小 | 组件 | 说明 |
|---------|---------|------|------|------|
| `0x00000000` | `0x1FFFFFFF` | 512MB | NULL | 非法访问 → trap |
| `0x20000000` | `0x2000FFFF` | 64KB | I-TCM (ROM) | 固件代码段 |
| `0x20010000` | `0x2001FFFF` | 64KB | D-TCM (RAM) | 数据段 + Heap + Stack |
| `0x40000000` | `0x40000FFF` | 4KB | CLINT | mtime/mtimecmp |
| `0x40001000` | `0x40001FFF` | 4KB | UART | Console I/O |
| `0x40002000` | `0x40002FFF` | 4KB | GPIO | 32-bit GPIO |
| `0x80000000` | `0xFFFFFFFF` | 2GB | 保留 | 未來扩展 |

### 2.3 复位向量

- **Cold Reset PC**: `0x20000000` (I-TCM 基址)
- **Trap Entry (mtvec)**: 可配置，默认 `0x20000100` (向量表基址)

---

## 3. CPU 指令集规格

### 3.1 支持的指令子集

| 扩展 | 指令数 | 说明 | 状态 |
|------|-------|------|------|
| RV32I Base | 40 | 基础整数指令 | ✅ Phase 1-3 完成 |
| SYSTEM | 11 | CSR 读写 + 异常控制 | ❌ Phase A 待实现 |
| **总计** | **51** | FreeRTOS 最小集 | |

### 3.2 SYSTEM 指令清单

> **修订 v1.1**: 补充 funct12 完整编码 (RISC-V Privileged Spec v1.11)

| 指令 | 编码格式 | funct3 | funct12 | 操作 | FreeRTOS 用途 |
|------|---------|--------|---------|------|--------------|
| `csrrw rd, csr, rs1` | I-type | 001 | 0b000000000001 | `tmp=CSR[csr]; CSR[csr]=rs1; rd=tmp` | 读/写 mstatus/mie |
| `csrrs rd, csr, rs1` | I-type | 010 | 0b000000000010 | `tmp=CSR[csr]; CSR[csr]|=rs1; rd=tmp` | 设置中断使能 |
| `csrrc rd, csr, rs1` | I-type | 011 | 0b000000000011 | `tmp=CSR[csr]; CSR[csr]&=~rs1; rd=tmp` | 清除中断标志 |
| `csrrwi rd, csr, uimm` | I-type | 101 | 0b000000000101 | `tmp=CSR[csr]; CSR[csr]=uimm; rd=tmp` | 写立即数 |
| `csrrsi rd, csr, uimm` | I-type | 110 | 0b000000000110 | `tmp=CSR[csr]; CSR[csr]|=uimm; rd=tmp` | 设置立即数 |
| `csrrci rd, csr, uimm` | I-type | 111 | 0b000000000111 | `tmp=CSR[csr]; CSR[csr]&=~uimm; rd=tmp` | 清除立即数 |
| `mret` | I-type | 000 | 0b001100000010 | `PC←mepc; mstatus.MIE←mstatus.MPIE` | 中断返回 |
| `ecall` | I-type | 000 | 0b000000000000 | 触发异常 (environment call) | FreeRTOS 系统调用 |
| `ebreak` | I-type | 000 | 0b000000000001 | 触发断点异常 | Debug |
| `wfi` | I-type | 000 | 0b000100000101 | Wait for Interrupt | 低功耗 (可选) |

### 3.3 CSR 寄存器映射

> **修订 v1.1**: mtime/mtimecmp 已从 CSR 表中移除。它们是 CLINT 内存映射寄存器，不是标准 CSR (RISC-V Spec v1.11 §3.1.21)。地址 `0x700` 保留给 `mcountinhibit` (v1.10+)。

| CSR 地址 | 名称 | 位宽 | 读写 | 复位值 | 说明 |
|---------|------|------|------|--------|------|
| `0x300` | `mstatus` | 32 | R/W | `0x00000080` | MIE=0, MPIE=1, MPP=0 |
| `0x301` | `misa` | 32 | R/W | `0x40101101` | RV32I (I-ext=bit 8, M-mode=bit 12,13) |
| `0x304` | `mie` | 32 | R/W | `0x00000000` | MTIE(bit 7), MEIE(bit 11) |
| `0x305` | `mtvec` | 32 | R/W | `0x20000100` | Trap 向量基址 + MODE[1:0] |
| `0x340` | `mscratch` | 32 | R/W | `0x00000000` | 临时寄存器 (trap entry 用) |
| `0x341` | `mepc` | 32 | R/W | `0x00000000` | 异常返回地址 |
| `0x342` | `mcause` | 32 | R/W | `0x00000000` | 异常原因编码 |
| `0x343` | `mtval` | 32 | R/W | `0x00000000` | 异常附加信息 |
| `0x344` | `mip` | 32 | R/W | `0x00000000` | 中断挂起标志 MTIP(bit 7), MEIP(bit 11) |

**mtvec MODE 位说明**: `mtvec[1:0]` 决定 trap 处理模式：
- `MODE=0` (direct): 所有 trap 跳转到 mtvec[31:2]
- `MODE=1` (vectored): 同步异常跳转到 mtvec[31:2]，中断跳转到 mtvec[31:2] + cause×4
- FreeRTOS 通常使用 direct mode (MODE=0)

### 3.4 mcause 编码

| mcause 值 | 异常类型 | 触发条件 |
|----------|---------|---------|
| `0x80000007` | Machine Timer Interrupt | `mtime >= mtimecmp && mie.MTIE && mstatus.MIE` |
| `0x8000000B` | Machine External Interrupt | `meip asserted` |
| `0x00000003` | Instruction Address Misaligned | PC not 2-aligned |
| `0x00000001` | Instruction Access Fault | I-TCM 非法访问 |
| `0x00000005` | Load Access Fault | D-TCM 非法访问 |
| `0x00000007` | Store/AMO Access Fault | D-TCM 非法访问 |
| `0x00000008` | Environment Call (ecall) | `ecall` 指令 |
| `0x00000009` | Environment Call (ebreak) | `ebreak` 指令 |

---

## 4. CLINT (Core-Local Interrupt Controller) 规格

### 4.1 寄存器映射

| 偏移 | 寄存器 | 位宽 | 说明 |
|------|--------|------|------|
| `0x000` | `mtime` (低 32 位) | 32 | 时间计数器 [31:0] |
| `0x004` | `mtime` (高 32 位) | 32 | 时间计数器 [63:32] |
| `0x008` | `mtimecmp` (低 32 位) | 32 | 比较值 [31:0] |
| `0x00C` | `mtimecmp` (高 32 位) | 32 | 比较值 [63:32] |

### 4.2 中断生成逻辑

```cpp
// CLINT tick 函数 (每个 CPU cycle 调用)
void Clint::tick() {
    mtime += 1;
    if (mtime >= mtimecmp) {
        mtip = true;  // Machine Timer Interrupt Pending
    } else {
        mtip = false;
    }
}

// CPU 中断采样 (每个 cycle 检查)
bool interrupt_pending = mtip && mie.MTIE && mstatus.MIE;
```

### 4.3 中断响应延迟

- **检测延迟**: 1 cycle (当前 cycle 检测)
- **入口延迟**: 3 cycles (mstatus/mepc/mcause 保存)
- **总延迟**: 4 cycles 后执行 trap handler 第一条指令

---

## 5. 仿真接口规格

### 5.1 C++ 仿真测试框架

```cpp
// 标准测试模板
#include "ch.hpp"
#include "simulator.h"
#include "rv32i_soc.h"

TEST_CASE("FreeRTOS Boot Test", "[freertos][boot]") {
    ch::core::context ctx("freertos_test");
    ch::core::ctx_swap swap(&ctx);
    
    // 1. 创建 SoC
    Rv32iSoc soc;
    
    // 2. 加载固件 ELF
    FirmwareLoader::loadELF(soc, "freertos/build/demo.elf");
    
    // 3. 初始化仿真器
    ch::Simulator sim(&ctx);
    sim.set_input_value(soc.cpu.io().rst, 1);
    sim.tick();
    sim.set_input_value(soc.cpu.io().rst, 0);
    
    // 4. 运行仿真
    uint64_t max_ticks = 10'000'000;
    for (uint64_t tick = 0; tick < max_ticks; tick++) {
        sim.tick();
        soc.clint.tick();  // 更新 mtime
        
        // 5. 检查 UART 输出
        if (soc.uart.txReady()) {
            char c = soc.uart.readTx();
            std::cout << c;
            
            if (check_pass_marker(c)) break;
        }
    }
    
    REQUIRE(freertos_booted);
}
```

### 5.2 固件加载 API

```cpp
// firmware_loader.h
struct ElfBinary {
    uint32_t entry_point;
    std::vector<uint8_t> code;
    uint32_t load_addr;
};

class FirmwareLoader {
public:
    static ElfBinary loadELF(const std::string& path);
    static void loadHex(Rv32iSoc& soc, const std::string& path);
};
```

### 5.3 UART 输出捕获

```cpp
// 标准输出期望
// FreeRTOS boot 成功后打印:
// "[FreeRTOS] Task 1: counter = 0\n"
// "[FreeRTOS] Task 2: tick = 0\n"
// "PASS\n"
```

---

## 6. FreeRTOS 配置规格

### 6.1 FreeRTOSConfig.h 关键宏

```c
#define configMTIME_BASE_ADDRESS         (0x40000000UL)
#define configMTIMECMP_BASE_ADDRESS      (0x40000008UL)
#define configCPU_CLOCK_HZ               (50000000UL)  // 50MHz
#define configTICK_RATE_HZ               (1000UL)      // 1kHz tick
#define configMAX_SYSCALL_INTERRUPT_PRIORITY (5)
#define configNUM_PRIORITIES             (5)
#define configMINIMAL_STACK_SIZE         (128)         // words
#define configTOTAL_HEAP_SIZE            (4096)        // bytes
```

### 6.2 必需的 FreeRTOS 文件

| 文件 | 路径 | 说明 |
|------|------|------|
| `FreeRTOSConfig.h` | `freertos/config/` | 配置宏 |
| `port.c` | `freertos/port/` | RISC-V port 实现 |
| `portASM.S` | `freertos/port/` | 汇编 trap handler |
| `portmacro.h` | `freertos/port/` | 类型定义 |
| `demo_main.c` | `freertos/demo/` | 演示任务 |

---

## 7. 验收标准

### 7.1 Phase Gate 定义

| Phase | Gate 标准 | 验证方式 |
|-------|----------|---------|
| A: CSR+SYSTEM | CSR 读写测试通过，mret 返回正确 | `test_rv32i_system.cpp` |
| B: Pipeline | 5-stage 全集成，执行 20+ 指令序列 | `test_pipeline_execution.cpp` |
| C: CLINT | MTIP 中断触发→trap 入口→MRET 返回 | `test_timer_interrupt.cpp` |
| D: SoC | ELF 加载成功，UART 打印 Hello | `test_soc_integration.cpp` |
| E: FreeRTOS | 2 任务切换，UART 输出 tick | `test_freertos_boot.cpp` |

### 7.2 性能指标

| 指标 | 目标值 | 测量方式 |
|------|-------|---------|
| CPU 频率 | ≥ 50 MHz | 仿真 cycle count |
| FreeRTOS tick | 1 kHz | UART 输出间隔 |
| 中断延迟 | ≤ 10 cycles | `mtime` 精度测量 |
| 上下文切换 | ≤ 100 cycles | GPIO toggle 测量 |
| 内存占用 | ≤ 8 KB (代码 + 数据) | ELF size 统计 |

---

## 8. 风险与缓解

| 风险 | 影响 | 概率 | 缓解措施 |
|------|------|------|---------|
| Pipeline wiring 复杂度高 | Phase B 延期 | 高 | 分模块测试，每 stage 单独验证 |
| CSR 与 exception 耦合 | Phase A/C 互相阻塞 | 中 | Phase A 只做 CSR 读写，C 做异常流 |
| FreeRTOS port 兼容性 | Phase E 无法运行 | 中 | 参考 VexRiscv port 实现 |
| ELF 解析复杂 | Phase D 延期 | 低 | 先用简单 HEX 格式，后支持 ELF |

---

## 9. 参考资料

1. **RISC-V Privileged Spec v1.11**: https://riscv.org/specifications/privileged-isa/
2. **FreeRTOS RISC-V Port**: https://www.freertos.org/Using-FreeRTOS-on-RISC-V/
3. **VexRiscv Murax SoC**: https://github.com/SpinalHDL/VexRiscv
4. **fpga-3-softcores**: https://github.com/rschlaikjer/fpga-3-softcores
5. **riscv-gnu-toolchain**: https://github.com/riscv/riscv-gnu-toolchain

---

## 10. 修订历史

| 版本 | 日期 | 作者 | 变更说明 |
|------|------|------|---------|
| 0.1 | 2026-04-13 | CppHDL | 初始草案 |
| 1.0 | 2026-04-13 | CppHDL | 评审后正式发布 |

