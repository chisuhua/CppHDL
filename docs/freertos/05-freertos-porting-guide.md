# riscv-mini FreeRTOS Porting 指南

**版本**: 1.1  
**创建日期**: 2026-04-13  
**评审日期**: 2026-04-14  
**状态**: 评审后修订

---

## 1. 概述

本文档指导如何将 FreeRTOS RTOS 移植到 riscv-mini RV32I CPU，包括：
- FreeRTOS 配置
- RISC-V port实现 (port.c, portASM.S)
- 演示程序编写
- 交叉编译工具链

---

## 2. 工具链安装

### 2.1 RISC-V GCC

```bash
# 克隆工具链
git clone https://github.com/riscv/riscv-gnu-toolchain
cd riscv-gnu-toolchain

# 配置 (RV32I only, no M/F/D extensions)
./configure --with-arch=rv32i --with-abi=ilp32 --prefix=/opt/riscv32i

# 编译 (约 30 分钟)
make -j$(nproc)

# 添加到 PATH
export PATH="/opt/riscv32i/bin:$PATH"
```

### 2.2 验证安装

```bash
riscv32-unknown-elf-gcc --version
# 预期输出：riscv32-unknown-elf-gcc 13.x.x
```

---

## 3. FreeRTOS 源码获取

```bash
# 克隆 FreeRTOS Kernel
git clone https://github.com/FreeRTOS/FreeRTOS-Kernel
cd FreeRTOS-Kernel

# 切换到稳定版本
git checkout V10.5.1
```

---

## 4. 目录结构

```
examples/riscv-mini/freertos/
├── config/
│   └── FreeRTOSConfig.h       # FreeRTOS 配置
├── port/
│   ├── port.c                 # RISC-V port 实现
│   ├── portASM.S              # 汇编 trap handler
│   └── portmacro.h            # 类型/宏定义
├── demo/
│   └── demo_main.c            # 演示任务
├── Makefile                    # 交叉编译脚本
└── linker.ld                   # 链接脚本
```

---

## 5. FreeRTOSConfig.h

```c
#ifndef FREERTOS_CONFIG_H
#define FREERTOS_CONFIG_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>

/* ============= 硬件抽象配置 ============= */

// CLINT 寄存器地址 (匹配 SoC 内存映射)
#define configMTIME_BASE_ADDRESS              (0x40000000UL)
#define configMTIMECMP_BASE_ADDRESS           (0x40000008UL)

// CPU 频率 (50MHz)
#define configCPU_CLOCK_HZ                    (50000000UL)

// Tick 频率 (1kHz)
#define configTICK_RATE_HZ                    (1000UL)

/* ============= 调度器配置 ============= */

// 优先级数量
#define configMAX_PRIORITIES                  (5)

// 最小栈深 (words)
#define configMINIMAL_STACK_SIZE              (128)

// Heap 大小 (bytes)
#define configTOTAL_HEAP_SIZE                 (4096)

// 时间片长度 (ticks)
#define configTICK_RATE_HZ                    (1000)

/* ============= 系统配置 ============= */

// 使用抢占式调度
#define configUSE_PREEMPTION                  1

// 使用时间片轮转
#define configUSE_TIME_SLICING                1

// 空闲任务栈深
#define configMINIMAL_STACK_SIZE              (128)

// 最大任务名长度
#define configMAX_TASK_NAME_LEN               (16)

// 使用 16-bit tick
#define configUSE_16_BIT_TICKS                0

// IDLE hook
#define configIDLE_HOOK                       0

// Tick hook
#define configUSE_TICK_HOOK                   0

/* ============= 内存管理 ============= */

// 使用 heap_4.c (带有碎片合并)
#define configFRTOS_MEMORY_SCHEME             4

/* ============= 中断配置 ============= */

// RISC-V M-mode 中断优先级
#define configKERNEL_INTERRUPT_PRIORITY       7
#define configMAX_SYSCALL_INTERRUPT_PRIORITY  5

/* ============= 调试 ============= */

// 栈溢出检测
#define configCHECK_FOR_STACK_OVERFLOW        2

// 运行时统计
#define configGENERATE_RUN_TIME_STATS         0

#ifdef __cplusplus
}
#endif

#endif /* FREERTOS_CONFIG_H */
```

---

## 6. portmacro.h

```c
#ifndef PORTMACRO_H
#define PORTMACRO_H

#ifdef __cplusplus
extern "C" {
#endif

/* ============= 类型定义 ============= */

#define portCHAR                    char
#define portFLOAT                   float
#define portDOUBLE                  double
#define portLONG                    long
#define portSHORT                   short
#define portSTACK_TYPE              uint32_t
#define portBASE_TYPE               long

typedef portSTACK_TYPE              StackType_t;
typedef long                        BaseType_t;
typedef unsigned long               UBaseType_t;

#if(configUSE_16_BIT_TICKS == 1)
    typedef uint16_t                TickType_t;
    #define portMAX_DELAY           (TickType_t)0xffff
#else
    typedef uint32_t                TickType_t;
    #define portMAX_DELAY           (TickType_t)0xffffffffUL
#endif

/* ============= 架构特性 ============= */

// 临界区栈增长方向 (向下)
#define portSTACK_GROWTH            (-1)

// 软件模拟中断
#define portSOFTWARE_TICK           0

// 字节对齐 (4 字节)
#define portBYTE_ALIGNMENT          4

/* ============= 临界区 ============= */

#define portENTER_CRITICAL()        __asm volatile("csrc mstatus, %0" ::"i"(8))
#define portEXIT_CRITICAL()         __asm volatile("csrs mstatus, %0" ::"i"(8))

#define portENTER_CRITICAL_FROM_ISR()   portENTER_CRITICAL()
#define portEXIT_CRITICAL_FROM_ISR()    portEXIT_CRITICAL()

/* ============= 调度器 ============= */

#define portYIELD()                 __asm volatile("ecall")
#define portYIELD_FROM_ISR(x)       if(x) portYIELD()

#define portNOP()                   __asm volatile("nop")

/* ============= 任务 ============= */

#define portTASK_FUNCTION_PROTO(vFunction, pvParameters) void vFunction(void *pvParameters)
#define portTASK_FUNCTION(vFunction, pvParameters) void vFunction(void *pvParameters)

/* ============= 上下文切换 ============= */

// 初始栈帧设置 (在 pxPortInitialiseStack 中实现)
StackType_t *pxPortInitialiseStack(StackType_t *pxTopOfStack, 
                                   TaskFunction_t pxCode, 
                                   void *pvParameters);

/* ============= 汇编 ============= */

// 汇编 trap handler
extern void vPortTrapHandler(void);
extern void vPortContextSwitch(void);

#ifdef __cplusplus
}
#endif

#endif /* PORTMACRO_H */
```

---

## 7. port.c

```c
#include <stdint.h>
#include "FreeRTOS.h"
#include "task.h"
#include "portmacro.h"

/* ============= 寄存器定义 ============= */

#define portINITIAL_MSTATUS       (0x00001880UL)  // MIE=1, MPIE=1
#define portINITIAL_PC            (0x20000000UL)  // ROM base

/* ============= 全局变量 ============= */

// Tick 计数器
static volatile uint64_t *portMTIME = (volatile uint64_t*)configMTIME_BASE_ADDRESS;
static volatile uint64_t *portMTIMECMP = (volatile uint64_t*)configMTIMECMP_BASE_ADDRESS;

/* ============= 栈初始化 ============= */

StackType_t *pxPortInitialiseStack(StackType_t *pxTopOfStack, 
                                   TaskFunction_t pxCode, 
                                   void *pvParameters) {
    // RISC-V 异常帧布局:
    // x1 (ra), x2 (sp), x3 (gp), ... x31 (t2)
    // mstatus, mepc
    
    pxTopOfStack--;
    *pxTopOfStack = (StackType_t)0x00000000;  // x1 (ra)
    pxTopOfStack--;
    *pxTopOfStack = (StackType_t)0x00000000;  // x2 (sp)
    pxTopOfStack--;
    *pxTopOfStack = (StackType_t)0x00000000;  // x3 (gp)
    pxTopOfStack--;
    *pxTopOfStack = (StackType_t)0x00000000;  // x4 (tp)
    
    // x5-x10 (t0-t5, a0)
    for (int i = 0; i < 6; i++) {
        pxTopOfStack--;
        *pxTopOfStack = (StackType_t)0x00000000;
    }
    
    // a0 = pvParameters
    pxTopOfStack--;
    *pxTopOfStack = (StackType_t)pvParameters;
    
    // a1-a7
    for (int i = 0; i < 7; i++) {
        pxTopOfStack--;
        *pxTopOfStack = (StackType_t)0x00000000;
    }
    
    // s0-s11 (x8-x9, x18-x27)
    for (int i = 0; i < 12; i++) {
        pxTopOfStack--;
        *pxTopOfStack = (StackType_t)0x00000000;
    }
    
    // t0-t6 (x5-x7, x28-x31)
    for (int i = 0; i < 7; i++) {
        pxTopOfStack--;
        *pxTopOfStack = (StackType_t)0x00000000;
    }
    
    // mstatus
    pxTopOfStack--;
    *pxTopOfStack = portINITIAL_MSTATUS;
    
    // mepc = task entry point
    pxTopOfStack--;
    *pxTopOfStack = (StackType_t)pxCode;
    
    return pxTopOfStack;
}

/* ============= 调度器实现 ============= */

static void vPortSetupTimerInterrupt(void) {
    // 计算下一个 tick 的 mtimecmp 值
    uint64_t timer_freq = configCPU_CLOCK_HZ / configTICK_RATE_HZ;
    *portMTIMECMP = *portMTIME + timer_freq;
    
    // 启用 MTIE (Machine Timer Interrupt Enable)
    __asm volatile("csrs mie, %0" ::"i"(0x80));
}

BaseType_t xPortStartScheduler(void) {
    // 启用全局中断
    __asm volatile("csrs mstatus, %0" ::"i"(8));
    
    // 设置定时器中断
    vPortSetupTimerInterrupt();
    
    // 启动第一个任务 (portYIELD 触发上下文切换)
    portYIELD();
    
    // 不应到达这里
    for(;;);
    return pdFALSE;
}

void vPortEndScheduler(void) {
    // 不支持
    for(;;);
}

/* ============= 中断 handler ============= */

// 在 portASM.S 中实现
extern void vPortTrapHandler(void);

// Trap handler 安装到 mtvec
void vPortInstallTrapHandler(void) {
    uintptr_t trap_handler_addr = (uintptr_t)vPortTrapHandler;
    __asm volatile("csrw mtvec, %0" ::"r"(trap_handler_addr));
}
```

---

## 8. portASM.S

```asm
    .section .text
    .global vPortTrapHandler
    .global vPortContextSwitch
    .global pxCurrentTCB

/* ============= Trap Handler ============= */

vPortTrapHandler:
    // 保存 all registers to stack
    addi sp, sp, -132           // Allocate stack frame (32 regs + mstatus + mepc)
    
    sw ra, 4(sp)
    sw gp, 8(sp)
    // ... save all x1-x31
    
    // 读取 mcause
    csrr t0, mcause
    csrr t1, mepc
    
    // 检查是否为 timer interrupt (mcause bit 31 set, cause=7)
    li t2, 0x80000007
    bne t0, t2, 1f
    
    // Timer interrupt - 清除 pending
    li t3, 0
    sw t3, configMTIMECMP_BASE_ADDRESS - configMTIME_BASE_ADDRESS(t0)
    
    // 调用 FreeRTOS tick handler
    call vPortSysTickHandler
    
    j 2f
    
1:
    // 其他中断/异常 - 暂时不处理
    
2:
    // 恢复所有寄存器
    lw ra, 4(sp)
    lw gp, 8(sp)
    // ... restore all
    
    addi sp, sp, 132
    
    // MRET
    mret

/* ============= Context Switch ============= */

vPortContextSwitch:
    // 保存当前任务上下文
    la t0, pxCurrentTCB
    lw t0, 0(t0)              // t0 = pxCurrentTCB->pxStack
    
    // 保存所有寄存器到当前任务栈
    sw ra, 0(t0)
    sw gp, 4(t0)
    // ... save all
    
    // 保存到 TCB
    sw sp, (t0)
    
    // 选择下一个任务
    call vTaskSwitchContext
    
    // 恢复下一个任务上下文
    la t0, pxCurrentTCB
    lw t0, 0(t0)
    lw sp, 0(t0)              // sp = pxNextTCB->pxStack
    
    // 恢复所有寄存器
    lw ra, 0(sp)
    lw gp, 4(sp)
    // ... restore all
    
    mret
```

---

## 9. demo_main.c

```c
#include "FreeRTOS.h"
#include "task.h"
#include <stdio.h>

// UART 寄存器地址
#define UART_BASE     0x40001000
#define UART_TX       (*(volatile uint32_t*)(UART_BASE + 0))

// 简单 putchar
int putchar(int c) {
    while ((UART_TX & 0x1) == 0);  // Wait TX empty
    UART_TX = c;
    return c;
}

/* ============= Task 1: Counter ============= */

static void vTaskCounter(void *pvParameters) {
    uint32_t counter = 0;
    
    for (;;) {
        printf("[Task 1] counter = %u\n", counter++);
        vTaskDelay(pdMS_TO_TICKS(500));  // 500ms
    }
}

/* ============= Task 2: Tick Monitor ============= */

static void vTaskTickMonitor(void *pvParameters) {
    TickType_t lastWakeTime = xTaskGetTickCount();
    uint32_t ticks = 0;
    
    for (;;) {
        printf("[Task 2] tick = %u\n", ticks++);
        vTaskDelayUntil(&lastWakeTime, pdMS_TO_TICKS(1000));  // 1000ms
    }
}

/* ============= Main ============= */

int main(void) {
    printf("\n=== riscv-mini FreeRTOS Demo ===\n\n");
    
    // 创建 Task 1
    xTaskCreate(vTaskCounter, "Counter", 128, NULL, 1, NULL);
    
    // 创建 Task 2
    xTaskCreate(vTaskTickMonitor, "Ticks", 128, NULL, 2, NULL);
    
    // 启动调度器
    vTaskStartScheduler();
    
    // 不应到达这里
    for (;;) {}
    
    return 0;
}
```

---

## 10. Makefile

```makefile
# Cross-compiler
CROSS_COMPILE = riscv32-unknown-elf-
CC = $(CROSS_COMPILE)gcc
AS = $(CROSS_COMPILE)as
LD = $(CROSS_COMPILE)ld
OBJCOPY = $(CROSS_COMPILE)objcopy
OBJDUMP = $(CROSS_COMPILE)objdump

# Architecture
ARCH = rv32i
ABI = ilp32

# Flags
CFLAGS = -march=$(ARCH) -mabi=$(ABI) -Os -ffunction-sections -fdata-sections
CFLAGS += -ffreestanding -nostdlib -Wall -Wextra
CFLAGS += -I./config -I./port -I../../FreeRTOS-Kernel/include -I../../FreeRTOS-Kernel/portable/GCC/RISC-V

# Output
BUILD_DIR = build
ELF = $(BUILD_DIR)/demo.elf
HEX = $(BUILD_DIR)/demo.hex

# Sources
SOURCES = demo/demo_main.c port/port.c port/portASM.S ../../FreeRTOS-Kernel/tasks.c ../../FreeRTOS-Kernel/list.c ../../FreeRTOS-Kernel/queue.c ../../FreeRTOS-Kernel/portable/MemMang/heap_4.c

# Targets
all: $(BUILD_DIR) $(ELF) $(HEX)

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

$(ELF): $(SOURCES)
	$(CC) $(CFLAGS) -Tlinker.ld -o $@ $(SOURCES)

$(HEX): $(ELF)
	$(OBJCOPY) -O ihex $< $@

clean:
	rm -rf $(BUILD_DIR)

dump: $(ELF)
	$(OBJDUMP) -d $(ELF) | less

.PHONY: all clean dump
```

---

## 11. linker.ld

```ld
MEMORY
{
    ROM (rx)      : ORIGIN = 0x20000000, LENGTH = 64K
    RAM (xrw)     : ORIGIN = 0x20010000, LENGTH = 64K
}

SECTIONS
{
    .text : {
        _stext = .;
        *(.text.start)
        *(.text)
        *(.text.*)
        *(.rodata)
        _etext = .;
    } > ROM

    .data : {
        _sdata = .;
        *(.data)
        *(.data.*)
        _edata = .;
    } > RAM

    .bss : {
        _sbss = .;
        *(.bss)
        *(.bss.*)
        *(COMMON)
        _ebss = .;
    } > RAM

    _heap_start = .;
    _heap_end = ORIGIN(RAM) + LENGTH(RAM);
}
```

---

## 12. 构建与运行

```bash
# 进入目录
cd examples/riscv-mini/freertos

# 构建
make clean && make

# 输出:
#   build/demo.elf  (ELF 二进制)
#   build/demo.hex  (Intel HEX 格式)

# 在 C++ 测试中加载
# (见 test_freertos_boot.cpp)
```

---

## 13. 调试

### 13.1 ELF 分析

```bash
riscv32-unknown-elf-objdump -d build/demo.elf | less

# 查看内存布局
riscv32-unknown-elf-size build/demo.elf
```

### 13.2 仿真波形

```cpp
// test_freertos_boot.cpp
ch::Simulator sim(&ctx, ch::SimulatorConfig{
    .trace_enabled = true,
    .trace_vcd = "freertos_boot.vcd"
});

// gtkwave freertos_boot.vcd
```

---

## 14. 常见问题

| 问题 | 原因 | 解决 |
|------|------|------|
| FreeRTOS 不启动 | mtvec 未设置 | 调用 `vPortInstallTrapHandler()` |
| 立即 crash | 栈初始化错误 | 检查 `pxPortInitialiseStack()` 寄存器顺序 |
| 无 UART 输出 | 地址映射错误 | 确认 `0x40001000` 是正确 UART 地址 |
| Tick 不触发 | CLINT 未 tick | 在仿真 loop 中调用 `soc.clint.tick()` |

