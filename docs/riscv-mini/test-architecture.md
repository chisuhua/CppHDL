# riscv-mini 集成测试架构方案

**版本**: 2.0  
**创建日期**: 2026-04-14  
**状态**: 执行中  

---

## 目录

1. [架构总览](#1-架构总览)
2. [测试分层模型](#2-测试分层模型)
3. [Layer 1: ISA 层 (riscv-tests)](#3-layer-1-isa-层-riscv-tests)
4. [Layer 2: 中断与异常层](#4-layer-2-中断与异常层)
5. [Layer 3: SoC 集成层](#5-layer-3-soc-集成层)
6. [Layer 4: FreeRTOS OS 语义层](#6-layer-4-freertos-os-语义层)
7. [测试基础设施](#7-测试基础设施)
8. [CI/CD 集成方案](#8-cicd-集成方案)
9. [实施计划与里程碑](#9-实施计划与里程碑)
10. [文件目录结构](#10-文件目录结构)

---

## 1. 架构总览

### 1.1 设计理念

riscv-mini 是一个运行在 C++ 模拟器中的 RV32I CPU，测试架构遵循 **从底层 ISA 到上层 OS 语义** 的渐进式验证策略：

```
┌─────────────────────────────────────────────────────────────┐
│  Layer 4: FreeRTOS OS 语义层                                  │  ← x86_64 Catch2 测试加载 FreeRTOS 编译的 ELF
│  任务调度 · 上下文切换 · 队列/信号量 · 定时器 ISR             │  ← UART 输出解析验证 ("SUCCESS" 关键字)
├─────────────────────────────────────────────────────────────┤
│  Layer 3: SoC 集成层                                         │  ← x86_64 Catch2 测试
│  UART MMIO · CLINT 计时器 · 地址解码 · 固件加载              │  ← 内存映射 IO 验证
├─────────────────────────────────────────────────────────────┤
│  Layer 2: 中断与异常层                                       │  ← x86_64 Catch2 测试
│  CLINT 定时中断 · 异常 Trap · mret 恢复                      │  ← CSR/PC 流验证
├─────────────────────────────────────────────────────────────┤
│  Layer 1: ISA 层 (riscv-tests)                               │  ← x86_64 Catch2 测试加载 .bin
│  41 个 RV32UI 指令集测试 · tohost@0x80001000 机制            │  ← 解释器或 SoC 执行
├─────────────────────────────────────────────────────────────┤
│  Layer 0: 模块单元测试                                       │  ← x86_64 Catch2 测试
│  ALU · RegFile · Decoder · 流水线组件                        │  ← 单模块验证
└─────────────────────────────────────────────────────────────┘
```

### 1.2 关键执行模型

| 层次 | 测试运行位置 | 被测固件 | 验证方式 |
|------|------------|---------|---------|
| Layer 0 | x86_64 (Catch2) | 无固件 | C++ 对象方法调用 |
| Layer 1 | x86_64 (Catch2) | RV32I .bin (riscv32-unknown-elf-gcc 编译) | tohost MMIO 写入 |
| Layer 2 | x86_64 (Catch2) | 手写汇编/小 C 程序 | CSR/中断流断言 |
| Layer 3 | x86_64 (Catch2) | C 编译的裸机固件 | UART 输出捕获 |
| Layer 4 | x86_64 (Catch2) | FreeRTOS 编译的 ELF | "SUCCESS" 关键字匹配 |

**所有测试都运行在 x86_64 主机上**，被测代码是在 RISC-V 工具链下编译的固件。模拟器 (Rv32iSoc) 逐周期执行 RISC-V 指令。

**决策记录 (2026-04-14)**:
- riscv-tests 执行方式: **仅 Rv32iSoc Pipeline 模式** (CI Gate), Interpreter 保留为开发调试工具
- FreeRTOS 源码来源: **从 FreeRTOS 官方仓库 clone + 源码编译** (riscv32-unknown-elf-gcc)

### 1.3 riscv-tests 与 FreeRTOS 测试的关系

```
riscv-tests (Layer 1)                    FreeRTOS Tests (Layer 4)
─────────────────                        ────────────────────────
目的: 证明 CPU 正确执行指令               目的: 证明 OS 语义正确
验证: 单条指令行为                        验证: 多任务调度/同步
方法: 写 tohost → 读结果                  方法: UART 输出解析
依赖: ALU/RegFile/L/SW                    依赖: Layer 1+2+3 全部通过
状态: 41 .bin 已编译                      状态: Phase E, 待实施
```

**两者正交** — Layer 1 确保指令正确，Layer 4 确保 OS 利用这些正确指令实现了正确的调度语义。

---

## 2. 测试分层模型

### 2.1 层次依赖图

```
Layer 4 (FreeRTOS)
    │  依赖: Layer 1 (CSR/SYSTEM) Layer 2 (中断) Layer 3 (SoC: UART+CLINT)
    │
    ├── test_freertos_blinky.cpp    ← 最小 FreeRTOS 验证
    ├── test_freertos_full.cpp      ← 全面 OS 功能验证
    └── test_context_switch.cpp     ← 寄存器级上下文切换验证
    │
Layer 3 (SoC 集成)
    │  依赖: Layer 1 (CSR) Layer 2 (CLINT/异常)
    │
    ├── test_soc_hello_world.cpp    ← UART 打印验证
    ├── test_firmware_loader.cpp    ← ELF/BIN/HEX 加载
    ├── test_uart_mmio.cpp          ← 内存映射 IO
    └── test_address_decoder.cpp    ← 6 区域地址映射
    │
Layer 2 (中断与异常)
    │  依赖: Layer 1 (CSR/SYSTEM/mret)
    │
    ├── test_interrupt_flow.cpp     ← 定时中断完整流程
    ├── test_exception_trap.cpp     ← 异常入口/恢复
    └── test_mret_recovery.cpp      ← mret PC 恢复
    │
Layer 1 (ISA riscv-tests)
    │  依赖: Layer 0 (ALU/RegFile/解码/L-SW/Branch)
    │
    ├── test_riscv_tests.cpp        ← riscv-tests 官方套件
    └── test_rv32i_bare.cpp         ← 基本 ISA 子集
    │
Layer 0 (模块单元)
    ├── rv32i_phase1_test.cpp       ← RegFile/ALU/Decoder
    ├── rv32i_phase2_test.cpp       ← Branch/Jump
    └── rv32i_phase3_test.cpp       ← Load/Store
```

### 2.2 Gate 矩阵

```
Phase 0 (Layer 0) → Phase A (CSR) → Phase B (流水线)
      │                                    ↓
      │                              Phase C (中断)
      │                                    ↓
      │                              Phase D (SoC)
      │                                    ↓
      └──────────────────────→ Phase D.5 (riscv-tests)
                                    │
                                    ↓
                              Phase E (FreeRTOS)
```

每层 Gate 标准：

| Layer | Gate 条件 | Pass 标准 |
|-------|----------|-----------|
| L0 | 编译 0 警告 | 所有现有测试 pass |
| L1 | riscv-tests 41/41 | tohost=1 ≥ 90% (CSR 相关测试除外) |
| L2 | CLINT 中断触发 | 中断延迟 ≤ 10 cycles |
| L3 | UART 输出正确 | "Hello World" 在 ≤ 10000 cycles 内打印 |
| L4 | FreeRTOS 启动 | "SUCCESS" 在 ≤ 60s 内输出 |

---

## 3. Layer 1: ISA 层 (riscv-tests)

### 3.1 测试概述

riscv-tests 是 RISC-V 官方 ISA 验证套件。我们在 Phase D.5 已经完成了：
- ✅ riscv-tests 仓库克隆到 `/workspace/project/riscv-tests/`
- ✅ riscv32-unknown-elf-gcc 工具链 (v15.2.0) 位于 `/workspace/project/opt/riscv/bin/`
- ✅ 41 个 RV32UI 测试编译为 .bin 文件 (位于 `build/rv32ui/`)
- ✅ Rv32iInterpreter 解释器框架创建
- ✅ 4 个手写测试通过 (add, load/store, branch, jal)

### 3.2 测试分类

| 类别 | 测试名 | 指令验证 | CSR 需要 | 预期状态 |
|------|--------|---------|---------|---------|
| **ALU 算术** | add, addi | ADD, ADDI | ❌ | ✅ 应 pass |
| **ALU 算术** | sub, lui | SUB, LUI | ❌ | ✅ 应 pass |
| **ALU 逻辑** | and, andi | AND, ANDI | ❌ | ✅ 应 pass |
| **ALU 逻辑** | or, ori | OR, ORI | ❌ | ✅ 应 pass |
| **ALU 逻辑** | xor, xori | XOR, XORI | ❌ | ✅ 应 pass |
| **移位** | sll, slli | SLL, SLLI | ❌ | ✅ 应 pass |
| **移位** | srl, srli | SRL, SRLI | ❌ | ✅ 应 pass |
| **移位** | sra, srai | SRA, SRAI | ❌ | ✅ 应 pass |
| **比较** | slt, slti | SLT, SLTI | ❌ | ✅ 应 pass |
| **比较** | sltu, sltiu | SLTU, SLTIU | ❌ | ✅ 应 pass |
| **分支** | beq, bne | BEQ, BNE | ❌ | ✅ 应 pass |
| **分支** | blt, bltu | BLT, BLTU | ❌ | ✅ 应 pass |
| **分支** | bge, bgeu | BGE, BGEU | ❌ | ✅ 应 pass |
| **跳转** | jal | JAL | ❌ | ✅ 应 pass |
| **跳转** | jalr | JALR | ❌ | ✅ 应 pass |
| **跳转** | auipc | AUIPC | ❌ | ✅ 应 pass |
| **加载** | lb, lbu | LB, LBU | ❌ | ✅ 应 pass |
| **加载** | lh, lhu | LH, LHU | ❌ | ✅ 应 pass |
| **加载** | lw | LW | ❌ | ✅ 应 pass |
| **存储** | sb | SB | ❌ | ✅ 应 pass |
| **存储** | sh | SH | ❌ | ✅ 应 pass |
| **存储** | sw | SW | ❌ | ✅ 应 pass |
| **综合** | st_ld | LW/SW 混合 | ❌ | ✅ 应 pass |
| **综合** | ma_data | 内存对齐 | ❌ | ✅ 应 pass |
| **综合** | simple | 基础验证 | ❌ | ✅ 应 pass |
| **综合** | fence_i | 指令屏障 | ❌ | ✅ 应 pass |
| **特殊*** | (需要 CSR) | 各种 | ✅ | ⏸️ 需 CSR 支持 |

### 3.3 riscv-tests 运行机制

```
┌─────────────────────────────────────────────────┐
│  test_riscv_tests.cpp (Catch2 测试)               │
│                                                   │
│  1. 加载 .bin 文件到 0x80000000                    │
│  2. 从 _start 入口点开始执行                       │
│  3. 监控 0x80001000 (tohost) 的写入                │
│     - 写入 1 → PASS                               │
│     - 写入 0 → FAIL                               │
│  4. 超时 (默认 10000 cycles) → TIMEOUT             │
│                                                   
│  执行方式:                                         │
│  ├── Interpreter (当前)                           │
│  │   └── 逐指令 RISC-V 模拟                        │
│  │   └── 缺 CSR/Trap → CSR 测试 fail               │
│  │                                                 │
│  └── Rv32iSoc Pipeline (目标)                     │
│      └── 完整 5 级流水线模拟                       │
│      └── CSR/异常完整支持                          │
└─────────────────────────────────────────────────┘
```

### 3.4 riscv-tests 运行机制

```
┌─────────────────────────────────────────────────┐
│  test_riscv_tests_soc.cpp (Catch2 测试)            │
│                                                   │
│  1. 加载 .bin 文件到 0x80000000                    │
│  2. 从 _start 入口点开始执行 via Rv32iSoc          │
│  3. 监控 0x80001000 (tohost) 的写入                │
│     - 写入 1 → PASS                               │
│     - 写入 0 → FAIL                               │
│  4. 超时 (默认 10000 cycles) → TIMEOUT             │
│                                                   
│  执行方式 (决策):                                  │
│  ├── CI Gate: Rv32iSoc Pipeline (唯一正式模式)     │
│  │   └── 完整 5 级流水线模拟                       │
│  │   └── CSR/异常完整支持                          │
│  │                                                 │
│  └── 开发调试: Interpreter (辅助工具)              │
│      └── 逐指令 RISC-V 模拟                        │
│      └── 快速迭代，不纳入 CI                       │
└─────────────────────────────────────────────────┘
```

### 3.5 CMake 集成方案

add_executable(test_riscv_tests_interpreter
    tests/test_riscv_tests.cpp 
    ${CMAKE_CURRENT_SOURCE_DIR}/../../tests/catch_amalgamated.cpp)
target_link_libraries(test_riscv_tests_interpreter PRIVATE cpphdl)
target_compile_definitions(test_riscv_tests_interpreter PRIVATE
    RISCV_TESTS_BIN_DIR="${RISCV_TESTS_BIN_DIR}")
add_test(NAME riscv_tests_interpreter COMMAND test_riscv_tests_interpreter)
set_tests_properties(riscv_tests_interpreter PROPERTIES LABELS "riscv-tests;interpreter")

add_executable(test_riscv_tests_soc
    tests/test_riscv_tests_soc.cpp 
    ${CMAKE_CURRENT_SOURCE_DIR}/../../tests/catch_amalgamated.cpp)
target_link_libraries(test_riscv_tests_soc PRIVATE cpphdl)
target_compile_definitions(test_riscv_tests_soc PRIVATE
    RISCV_TESTS_BIN_DIR="${RISCV_TESTS_BIN_DIR}")
add_test(NAME riscv_tests_soc COMMAND test_riscv_tests_soc)
set_tests_properties(riscv_tests_soc PROPERTIES LABELS "riscv-tests;soc")
```

### 3.5 路径管理策略

当前 `test_riscv_tests.cpp` 中硬编码路径：
```cpp
const char* RISCV_TESTS_PATH = "/workspace/project/riscv-tests/build/rv32ui/";
```

改为 CMake 编译宏传入：
```cpp
#ifndef RISCV_TESTS_BIN_DIR
#define RISCV_TESTS_BIN_DIR "/workspace/project/riscv-tests/build/rv32ui/"
#endif
```

CMakeLists.txt 中：
```cmake
target_compile_definitions(test_riscv_tests PRIVATE
    RISCV_TESTS_BIN_DIR="${RISCV_TESTS_BIN_DIR}")
```

支持运行时覆盖环境变量：
```cmake
# 如果设置了 RISCV_TESTS_PATH 环境变量，优先使用
if(DEFINED ENV{RISCV_TESTS_PATH})
    set(RISCV_TESTS_BIN_DIR "$ENV{RISCV_TESTS_PATH}")
endif()
```

---

## 4. Layer 2: 中断与异常层

### 4.1 测试覆盖矩阵

| 测试文件 | 测试名 | 验证内容 | 依赖 | 状态 |
|---------|--------|---------|------|------|
| `test_interrupt_flow.cpp` (已存在) | CLINT 定时中断 | `mtime` 递增 → `mtimecmp` 触发 → `mtip` 置位 | CLINT, CSR | ✅ |
| `test_interrupt_flow.cpp` (已存在) | 中断 trap 入口 | PC 跳转 → `mcause`=7 → `mepc` 保存 | CSR, Pipeline | ✅ |
| `test_interrupt_flow.cpp` (已存在) | `mret` 返回 | `mepc` → PC 恢复 → 继续执行 | CSR, SYSTEM | ✅ |
| `test_exception_trap.cpp` (新增) | 非法指令异常 | 执行非法 opcode → trap → `mcause`=2 | 异常单元 | ❌ 新增 |
| `test_exception_trap.cpp` (新增) | 断点异常 | `EBREAK` → trap → `mcause`=3 | SYSTEM, CSR | ❌ 新增 |
| `test_exception_trap.cpp` (新增) | 环境调用 | `ECALL` → trap → `mcause`=11 | SYSTEM, CSR | ❌ 新增 |
| `test_exception_trap.cpp` (新增) | 对齐错误 | 非对齐 LW → trap → `mcause`=4 | 内存控制器 | ❌ 新增 |
| `test_timer_precision.cpp` (新增) | 中断延迟 | `mtimecmp` 设置 → cycles 到中断 ≤ 10 | CLINT, Pipeline | ❌ 新增 |

### 4.2 中断验证流程

```cpp
// test_interrupt_flow.cpp 中的核心验证模式
TEST_CASE("Interrupt: Complete CLINT Timer Interrupt Flow", "[interrupt][clint]") {
    context ctx("clint_timer");
    ctx_swap swap(&ctx);
    
    Rv32iSoc soc(ctx);
    
    // 1. 设置中断向量
    soc.csr.write(CSR_MTVEC, 0x80000200);
    
    // 2. 使能中断
    soc.csr.write(CSR_MSTATUS, 0x8);  // MIE=1
    soc.csr.write(CSR_MIE, 0x80);      // MTIE=1
    
    // 3. 安装中断处理程序到 0x80000200
    // (手写汇编: mret 回跳)
    
    // 4. 运行到中断触发
    uint64_t tick_before = soc.clint.read_mtime();
    sim.run_until_interrupt(tick_before, max_cycles);
    
    // 5. 验证
    REQUIRE(soc.csr.read(CSR_MCAUSE) == 0x80000007); // Timer interrupt
    REQUIRE(soc.pipeline.pc() == 0x80000200);         // PC 跳入 trap handler
    
    // 6. mret 恢复
    sim.tick_one_mret();
    REQUIRE(soc.pipeline.pc() == original_pc);       // PC 恢复
}
```

---

## 5. Layer 3: SoC 集成层

### 5.1 测试覆盖矩阵

| 测试文件 | 测试名 | 验证内容 | 依赖 | 状态 |
|---------|--------|---------|------|------|
| `test_soc_hello_world.cpp` (已存在) | UART 打印 | "Hello World" 字符串输出 | UART MMIO, Loader | ✅ |
| `test_address_decoder.cpp` (新增) | 6 区域映射 | I-TCM/D-TCM/CLINT/UART/ROM/未映射 | 地址解码器 | ❌ 新增 |
| `test_firmware_loader.cpp` (新增) | BIN/HEX 加载 | 固件正确加载到 I-TCM | 固件加载器 | ❌ 新增 |
| `test_uart_mmio.cpp` (新增) | MMIO 读写 | 数据寄存器/状态寄存器 | UART MMIO | ❌ 新增 |
| `test_soc_memory.cpp` (新增) | 内存映射完整性 | 跨区域读写正确 | 地址解码器 | ❌ 新增 |

### 5.2 SoC 地址映射

```
地址范围              大小     组件
─────────────────────────────────────────
0x0000_0000 - 0x0000_FFFF  64KB   I-TCM (指令内存)
0x0001_0000 - 0x0001_FFFF  64KB   D-TCM (数据内存)
0x1000_0000 - 0x1000_0FFF   4KB   CLINT (mtime/mtimecmp)
0x1000_1000 - 0x1000_1FFF   4KB   UART MMIO
0x8000_0000 - 0x8000_0FFF   4KB   ROM (启动代码)
其他地址                    —     未映射 (读回 0)
```

---

## 6. Layer 4: FreeRTOS OS 语义层

### 6.1 概述

Phase E 的 FreeRTOS 测试验证操作系统语义正确性。参考 FreeRTOS 官方 [RISC-V_RV32_QEMU_VIRT_GCC](https://github.com/FreeRTOS/FreeRTOS/tree/main/FreeRTOS/Demo/RISC-V_RV32_QEMU_VIRT_GCC) demo。

### 6.2 测试设计

#### 6.2.1 `test_freertos_blinky.cpp` — 最小验证

验证 FreeRTOS 能够：
1. 正确启动并初始化调度器
2. 运行 2 个任务 (producer/consumer) 通过队列通信
3. 软件定时器回调被触发

**输出验证**：
```
[FreeRTOS] Queue message received from task 1
[FreeRTOS] Software timer callback invoked
[FreeRTOS] FreeRTOS Demo SUCCESS : 1234
```

**Pass 条件**：10000 cycles 内输出包含 `"FreeRTOS Demo SUCCESS"`

#### 6.2.2 `test_freertos_full.cpp` — 全面功能验证

验证 FreeRTOS 的所有 OS 功能：
- Queue (Generic Queue Test)
- Semaphore (Binary/Counting)
- Mutex (Recursive Mutex)
- Event Groups
- Task Notifications
- Stream/Message Buffers
- Timer Demo

**Check Task 模式** (FreeRTOS 官方做法)：
```c
// prvCheckTask 周期性地检查所有其他任务是否仍在运行
static void prvCheckTask(void *pvParameters) {
    static const char *pcMessage = "FreeRTOS Demo SUCCESS";
    
    for (;;) {
        vTaskDelayUntil(&xPreviousWakeTime, xTaskPeriod);
        
        // 检查所有测试任务
        if (xAreSemaphoreTasksStillRunning() != pdPASS) {
            pcMessage = "ERROR: xAreSemaphoreTasksStillRunning";
        }
        // ... 检查其他任务
        
        printf("[FreeRTOS] %s : %u\n", pcMessage, (uint32_t)xTaskGetTickCount());
    }
}
```

#### 6.2.3 `test_context_switch.cpp` — 寄存器级验证

验证任务切换时所有 RISC-V 寄存器值被正确保存/恢复：

```assembly
# RegTest.S - 官方模式
reg_test_1:
    li t0, 0x11111111     # 填充寄存器
    li t1, 0x22222222
    li t2, 0x33333333
    # ... 填充 a0-a7, s1-s11
    
    # 让出 CPU (触发上下文切换)
    ecall  # 或直接调用 taskYIELD
    
    # 回来后验证寄存器
    li t3, 0x11111111
    beq t0, t3, 1f
    sw zero, error, x0    # 出错写入特定地址
1:
    j reg_test_1
```

### 6.3 FreeRTOS 测试依赖

```
FreeRTOS 测试
    ├── FreeRTOSConfig.h    ← 平台配置 (必须正确)
    ├── port.c              ← RISC-V 端口实现
    ├── portASM.S           ← 上下文切换汇编
    ├── clint_interrupt.S   ← CLINT tick ISR
    ├── blinky_demo.c       ← blinky demo 代码
    ├── full_demo.c         ← full demo 代码
    └── reg_test.S          ← 寄存器测试汇编
```

### 6.4 UART 输出验证机制

```cpp
// 通用验证框架
class FreeRtosOutputParser {
public:
    void feed(char c);
    bool success_detected() const;
    bool error_detected() const;
    std::string error_message() const;
    
    static constexpr const char* SUCCESS_PATTERN = "FreeRTOS Demo SUCCESS";
    static constexpr const char* ERROR_PATTERN = "FreeRTOS Demo ERROR";
};

// 在测试中使用
TEST_CASE("FreeRTOS Blinky: Basic verification", "[freertos][blinky]") {
    context ctx("freertos_blinky");
    ctx_swap swap(&ctx);
    
    Rv32iSoc soc(ctx);
    load_firmware(soc, "freertos_blinky.bin");
    
    UartCapture uart_capture;
    FreeRtosOutputParser parser;
    
    for (uint64_t tick = 0; tick < MAX_TICKS; tick++) {
        sim.tick();
        soc.clint.tick();
        
        while (soc.uart.txReady()) {
            char c = soc.uart.readTx();
            uart_capture.append(c);
            parser.feed(c);
            
            if (parser.success_detected()) {
                INFO("FreeRTOS started successfully");
                return;  // PASS
            }
            if (parser.error_detected()) {
                FAIL("FreeRTOS error: " << parser.error_message());
            }
        }
    }
    
    FAIL("Timeout: FreeRTOS did not start within " << MAX_TICKS << " cycles");
}
```

---

## 7. 测试基础设施

### 7.1 构建系统

```cmake
# 整体 cmake/TestingInfrastructure.cmake
# riscv-tests 二进制管理
set(RISCV_TESTS_BIN_DIR "${CMAKE_CURRENT_BINARY_DIR}/riscv-tests"
    CACHE PATH "Path to riscv-tests .bin files")

# 如果启用了 riscv-tests 交叉编译
option(RISCV_TESTS_BUILD "Build riscv-tests from source" OFF)
if(RISCV_TESTS_BUILD)
    find_program(RISCV32_GCC riscv32-unknown-elf-gcc)
    if(NOT RISCV32_GCC)
        message(FATAL_ERROR "riscv32-unknown-elf-gcc not found in PATH")
    endif()
    # 自定义命令编译 .S -> .elf -> .bin
    add_subdirectory(third_party/riscv-tests)
endif()

# riscv-tests 测试注册
function(add_riscv_tests test_name binary_file)
    add_test(
        NAME "riscv-${test_name}"
        COMMAND $<TARGET_FILE:test_riscv_tests_soc>
        --test-name "${test_name}"
        --binary "${RISCV_TESTS_BIN_DIR}/${binary_file}"
    )
    set_tests_properties("riscv-${test_name}" PROPERTIES
        LABELS "riscv-tests;soc"
        TIMEOUT 30
    )
endfunction()

# 批量注册 41 个测试
add_riscv_tests("add" "add.bin")
add_riscv_tests("addi" "addi.bin")
# ... 所有 41 个测试
```

### 7.2 测试标签体系

```
ctest -L <label>

标签层级:
├── unit          ← Layer 0 模块测试
├── csr           ← Layer 1 CSR 测试
├── riscv-tests   ← riscv-tests 套件
│   ├── interpreter   ← 解释器模式
│   └── soc           ← SoC 流水线模式
├── interrupt     ← Layer 2 中断测试
├── soc           ← Layer 3 SoC 集成测试
└── freertos      ← Layer 4 FreeRTOS 测试
    ├── blinky        ← 基础验证
    ├── full          ← 全面功能
    └── regtest       ← 寄存器切换
```

运行命令：
```bash
# 运行所有测试
ctest --output-on-failure

# 只运行 riscv-tests
ctest -L soc

# 只运行 FreeRTOS 测试
ctest -L freertos

# 运行 riscv-tests 的子集 (28 个应通过的测试)
ctest -R "riscv-(add|addi|and|sub|...)" --output-on-failure
```

### 7.3 重试机制

```cmake
# CTest 重试配置 (适用于 timing-dependent 测试)
set_tests_properties(test_freertos_blinky PROPERTIES
    PASS_REGULAR_EXPRESSION "FreeRTOS Demo SUCCESS"
    FAIL_REGULAR_EXPRESSION "FreeRTOS Demo ERROR"
    TIMEOUT 60
    ENVIRONMENT "MAX_TICKS=50000000"
    RETRIES 3
)
```

```cpp
// Catch2 重试支持
#define CATCH_CONFIG_FAST_COMPILE
#define CATCH_CONFIG_ALLOW_RERUNNER
```

---

## 8. CI/CD 集成方案

### 8.1 本地 CI 脚本

```bash
#!/bin/bash
# run_all_freertos_tests.sh

set -e

echo "=== riscv-mini Full Test Suite ==="

# 1. 构建
cmake -B build -DRISCV_TESTS_PATH=/workspace/project/riscv-tests/build/rv32ui
cmake --build build -j$(nproc)

# 2. 单元+集成测试
echo "--- Running base tests ---"
ctest -L "unit|csr|interrupt" --output-on-failure

# 3. riscv-tests (解释器模式)
echo "--- Running riscv-tests (interpreter) ---"
ctest -L "interpreter" --output-on-failure

# 4. riscv-tests (SoC 模式)
echo "--- Running riscv-tests (SoC) ---"
ctest -L "soc" --output-on-failure

# 5. FreeRTOS (如果存在)
if [ -f build/tests/test_freertos_blinky ]; then
    echo "--- Running FreeRTOS tests ---"
    ctest -L "freertos" --output-on-failure
fi

echo "=== All tests passed ==="
```

### 8.2 测试分类与执行策略

```
阶段 1: 快速验证 (≤ 30s)
  ├── Layer 0: 3 个 Phase 测试
  ├── Layer 1: 28 个 ALU/逻辑/分支测试 (解释器)
  └── Layer 2: 已存在的中断测试
  
→ 如果失败，立即停止
  
阶段 2: 完整验证 (≤ 5min)
  ├── Layer 1: 全部 41 riscv-tests
  ├── Layer 3: SoC 集成测试
  └── Layer 4: FreeRTOS blinky (如果有)
  
阶段 3: 深度验证 (≤ 30min)
  ├── Layer 4: FreeRTOS full (所有 15+ check tasks)
  └── Layer 4: RegTest (寄存器切换压力测试)
```

---

## 9. 实施计划与里程碑

### 9.1 当前状态

| Layer | 文件 | 状态 | 备注 |
|-------|------|------|------|
| L0 | 已完整 | ✅ | rv32i_phase{1,2,3}_test.cpp |
| L1 | 部分完成 | ⚠️ | 41 .bin 编译好，解释器创建，4 测试通过 |
| L2 | 部分完成 | ⚠️ | test_interrupt_flow.cpp 已存在 |
| L3 | 部分完成 | ⚠️ | test_soc_hello_world.cpp 已存在 |
| L4 | 未开始 | ❌ | Phase E |

### 9.2 下一步计划 (按优先级)

#### Phase F: riscv-tests 全量执行 (预计 2-3 天)

**目标**: 41/41 riscv-tests 全部通过 (或明确标记 CSR 依赖)

| 步骤 | 工作 | 产出 | 验收 |
|------|------|------|------|
| F.1 | 修复解释器 CSR 支持 | CSR/Trap 在解释器中工作 | test_csr_unit.cpp pass |
| F.2 | 创建 test_riscv_tests_soc.cpp | SoC 模式的测试 | 41 .bin 加载成功 |
| F.3 | CMake 路径管理 | 硬编码路径移除 | CI 可用相对路径 |
| F.4 | 批量执行 + 分类 | 28 pass / ? CSR 依赖 / ? fail | 分类报告 |
| F.5 | 修复 fail 的指令 | 流水线 bug 修复 | 全部 pass |

#### Phase G: 新增测试文件 (预计 2 天)

| 测试文件 | 新增测试数 | 验证内容 |
|---------|----------|---------|
| `test_address_decoder.cpp` | 6 | 6 区域地址映射 |
| `test_firmware_loader.cpp` | 8 | BIN/HEX 加载 |
| `test_uart_mmio.cpp` | 5 | UART MMIO 读写 |
| `test_soc_memory.cpp` | 4 | 跨区域内存访问 |
| `test_exception_trap.cpp` | 5 | 异常类型 |
| `test_timer_precision.cpp` | 2 | 中断延迟 |

#### Phase E: FreeRTOS Port (预计 7-14 天)

在 F 和 G 完成后实施，所有前置依赖已满足。

---

## 10. 文件目录结构

```
CppHDL/
├── examples/riscv-mini/
│   ├── CMakeLists.txt                          # 构建配置
│   ├── tests/
│   │   ├── rv32i_phase1_test.cpp               # Layer 0: RegFile/ALU/Decoder
│   │   ├── rv32i_phase2_test.cpp               # Layer 0: Branch/Jump
│   │   ├── rv32i_phase3_test.cpp               # Layer 0: Load/Store
│   │   │
│   │   ├── test_csr_unit.cpp                   # Layer 1: CSR 寄存器测试
│   │   ├── test_interrupt_flow.cpp             # Layer 2: 中断流测试
│   │   ├── test_soc_hello_world.cpp            # Layer 3: SoC 集成 (UART 打印)
│   │   │
│   │   ├── test_riscv_tests.cpp                # Phase F: riscv-tests 解释器模式
│   │   ├── test_riscv_tests_soc.cpp            # Phase F: riscv-tests SoC 模式 [新增]
│   │   │
│   │   ├── test_address_decoder.cpp            # Phase G: 地址映射 [新增]
│   │   ├── test_firmware_loader.cpp            # Phase G: 固件加载 [新增]
│   │   ├── test_uart_mmio.cpp                  # Phase G: UART MMIO [新增]
│   │   ├── test_soc_memory.cpp                 # Phase G: 内存集成 [新增]
│   │   ├── test_exception_trap.cpp             # Phase G: 异常类型 [新增]
│   │   ├── test_timer_precision.cpp            # Phase G: 中断延迟 [新增]
│   │   │
│   │   ├── test_freertos_blinky.cpp            # Phase E: FreeRTOS 最小验证 [新增]
│   │   ├── test_freertos_full.cpp              # Phase E: FreeRTOS 全面验证 [新增]
│   │   └── test_context_switch.cpp             # Phase E: 寄存器切换 [新增]
│   │
│   └── freertos/                               # Phase E: FreeRTOS 源码
│       ├── FreeRTOSConfig.h                    # 平台配置
│       ├── port.c                              # RISC-V 端口
│       ├── portASM.S                           # 上下文切换汇编
│       ├── clint_interrupt.S                   # CLINT tick ISR
│       ├── blinky_demo.c                       # blinky 入口
│       └── full_demo.c                         # full demo 入口
│
├── third_party/
│   └── riscv-tests/ → /workspace/project/riscv-tests/  # symlink 或 submodule
│
├── cmake/
│   └── FetchRiscvTests.cmake                   # riscv-tests CMake 集成
│
└── docs/riscv-mini/
    ├── freertos_roadmap.md                     # 路线图
    └── test-architecture.md                    # 本文档
```

---

## 附录 A: riscv-tests 41 个测试分类

### 纯 ALU 测试 (20 个，不依赖 CSR)

| 测试 | 验证指令 | 预期 | 风险 |
|------|---------|------|------|
| add | ADD | ✅ | 低 |
| addi | ADDI | ✅ | 低 |
| sub | SUB | ✅ | 低 |
| and | AND | ✅ | 低 |
| andi | ANDI | ✅ | 低 |
| or | OR | ✅ | 低 |
| ori | ORI | ✅ | 低 |
| xor | XOR | ✅ | 低 |
| xori | XORI | ✅ | 低 |
| sll | SLL | ✅ | 低 |
| slli | SLLI | ✅ | 低 |
| srl | SRL | ✅ | 低 |
| srli | SRLI | ✅ | 低 |
| sra | SRA | ✅ | 低 |
| srai | SRAI | ✅ | 低 |
| slt | SLT | ✅ | 低 |
| slti | SLTI | ✅ | 低 |
| sltu | SLTU | ✅ | 低 |
| sltiu | SLTIU | ✅ | 低 |
| lui | LUI | ✅ | 低 |

### 分支/跳转测试 (9 个，不依赖 CSR)

| 测试 | 验证指令 | 预期 | 风险 |
|------|---------|------|------|
| beq | BEQ | ✅ | 低 |
| bne | BNE | ✅ | 低 |
| blt | BLT | ✅ | 低 |
| bltu | BLTU | ✅ | 低 |
| bge | BGE | ✅ | 低 |
| bgeu | BGEU | ✅ | 低 |
| jal | JAL | ✅ | 低 |
| jalr | JALR | ✅ | 低 |
| auipc | AUIPC | ✅ | 低 |

### 访存测试 (9 个，不依赖 CSR)

| 测试 | 验证指令 | 预期 | 风险 |
|------|---------|------|------|
| lb | LB | ✅ | 中 |
| lbu | LBU | ✅ | 中 |
| lh | LH | ✅ | 中 |
| lhu | LHU | ✅ | 中 |
| lw | LW | ✅ | 中 |
| sb | SB | ✅ | 中 |
| sh | SH | ✅ | 中 |
| sw | SW | ✅ | 中 |
| fence_i | FENCE.I | ✅ | 低 (空操作) |

### 综合测试 (3 个)

| 测试 | 验证内容 | 预期 | 风险 |
|------|---------|------|------|
| simple | 基础算术 + 跳转 | ✅ | 低 |
| st_ld | 存储加载混合 | ✅ | 中 |
| ma_data | 内存对齐数据 | ✅ | 中 |

**总计**: 41 个测试，全部不依赖 CSR。如果解释器正确实现 riscv-tests 入口框架 (使用 AUIPC+ADDI 而非 CSR 设置 tohost 指针)，应当全部通过。

---

## 附录 B: 与 FreeRTOS 官方测试对比

| FreeRTOS 官方 | CppHDL riscv-mini | 匹配度 |
|---------------|-------------------|--------|
| QEMU RISC-V virt machine | C++ 模拟器 Rv32iSoc | ✅ 对应 |
| `main_blinky.c` | `test_freertos_blinky.cpp` | ✅ 对应 |
| `main_full.c` | `test_freertos_full.cpp` | ✅ 对应 |
| `build/gcc/RegTest.S` | `test_context_switch.cpp` | ✅ 对应 |
| `executable-monitor.py` | UART 输出 "SUCCESS" 匹配 | ✅ 对应 |
| 60s timeout | 50M cycles timeout | ✅ 对应 |
| 3 retry attempts | CTest RETRIES 3 | ✅ 对应 |
| "FreeRTOS Demo SUCCESS: <tick>" | Same pattern | ✅ 对应 |
| "FreeRTOS Demo ERROR: <test>" | Same pattern | ✅ 对应 |
| `xAreXXXStillRunning()` | Counter increment + check | ✅ 对应 |

---

*文档结束*
*下一步：执行 Phase F (riscv-tests 全量执行) → Phase G (新增测试文件) → Phase E (FreeRTOS Port)*
