# riscv-mini 测试验证策略

**版本**: 1.1  
**创建日期**: 2026-04-13  
**评审日期**: 2026-04-14  
**状态**: 评审后修订 (Momus 7 Critical, Metis 战略调整)

---

## 1. 验证层次结构

```
┌──────────────────────────────────────────────────────┐
│              Level 5: System Testing                 │
│         (FreeRTOS Boot, Multi-task Switching)        │
└──────────────────────────────────────────────────────┘
                      │
┌──────────────────────────────────────────────────────┐
│              Level 4: Integration Testing            │
│           (SoC, ELF Loader, UART Output)             │
└──────────────────────────────────────────────────────┘
                      │
┌──────────────────────────────────────────────────────┐
│              Level 3: Component Testing              │
│        (Pipeline, CLINT, Interrupt Flow)             │
└──────────────────────────────────────────────────────┘
                      │
┌──────────────────────────────────────────────────────┐
│              Level 2: Unit Testing                   │
│           (CSR, SYSTEM Instructions)                 │
└──────────────────────────────────────────────────────┘
                      │
┌──────────────────────────────────────────────────────┐
│              Level 1: Module Testing                 │
│           (ALU, Decoder, RegFile)                    │
└──────────────────────────────────────────────────────┘
```

---

## 2. 单元测试 (Level 1-2)

### 2.1 Phase A: CSR + SYSTEM

| 测试文件 | 覆盖项 | Assertions | Gate |
|---------|-------|-----------|------|
| `test_rv32i_csr.cpp` | mstatus 读写，位字段 | 10 | ≥8 pass |
| `test_csrrw.cpp` | csrrw 指令执行 | 5 | ≥5 pass |
| `test_csrrs.cpp` | csrrs 置位，清零 | 5 | ≥5 pass |
| `test_mret.cpp` | MRET 流，mepc/mstatus | 5 | ≥5 pass |

### 2.2 Phase C: Interrupt

| 测试文件 | 覆盖项 | Assertions | Gate |
|---------|-------|-----------|------|
| `test_clint_tick.cpp` | mtime 递增 | 5 | ≥5 pass |
| `test_mtip_trigger.cpp` | mtimecmp 比较 | 5 | ≥5 pass |
| `test_exception_entry.cpp` | trap 入口，mcause 编码 | 10 | ≥8 pass |
| `test_mret_return.cpp` | MRET 返回时序 | 5 | ≥5 pass |

---

## 3. 集成测试 (Level 3-4)

### 3.1 Phase B: Pipeline

```cpp
TEST_CASE("Pipeline: 20-instruction sequence", "[pipeline][integration]") {
    // 程序：20 条指令混合 (add, sub, lw, sw, branch)
    std::vector<uint32_t> program = {
        0x00100093,  // addi x1, x0, 1
        0x00200113,  // addi x2, x0, 2
        0x002081b3,  // add x3, x1, x2
        // ... 17 more instructions
    };
    
    Rv32iPipeline cpu;
    cpu.loadProgram(program);
    
    // 执行 100 cycles
    for (int i = 0; i < 100; i++) {
        sim.tick();
    }
    
    // 验证寄存器结果
    REQUIRE(cpu.gpr(3).value() == 3);  // x3 = 1+2
    REQUIRE(cpu.gpr(4).value() == 10); // x4 = loop counter
}
```

### 3.2 Phase D: SoC

```cpp
TEST_CASE("SoC: ELF loading + UART output", "[soc][elf]") {
    Rv32iSoc soc;
    
    // 加载测试 ELF
    FirmwareLoader::loadELF(soc, "tests/hello_world.elf");
    
    // 复位
    sim.set_input_value(soc.io().rst, 1);
    sim.tick();
    sim.set_input_value(soc.io().rst, 0);
    
    // 运行并捕获 UART
    std::string uart_output;
    for (uint64_t tick = 0; tick < 1'000'000; tick++) {
        sim.tick();
        soc.clint.tick();
        
        if (soc.uart.txReady()) {
            uart_output += soc.uart.readTx();
        }
    }
    
    REQUIRE(uart_output == "Hello from riscv-mini\n");
}
```

---

## 4. 系统测试 (Level 5)

### 4.1 FreeRTOS Boot

```cpp
TEST_CASE("FreeRTOS: Boot and dual-task switch", "[freertos][system]") {
    Rv32iSoc soc;
    FirmwareLoader::loadELF(soc, "freertos/build/demo.elf");
    
    // 运行 10 秒仿真 (50MHz * 10s = 500M cycles)
    uint64_t max_ticks = 500'000'000;
    std::vector<std::string> task_outputs;
    
    for (uint64_t tick = 0; tick < max_ticks; tick++) {
        sim.tick();
        soc.clint.tick();
        
        if (soc.uart.txReady()) {
            char c = soc.uart.readTx();
            std::cout << c;
            
            // 检测 FreeRTOS 启动标记
            if (c == '[') {
                task_outputs.push_back(readLine());
            }
            
            // 检测 PASS 标记
            if (check_pass_marker(c)) {
                break;
            }
        }
    }
    
    // 验证双任务切换
    REQUIRE(task_outputs.size() >= 10);
    REQUIRE(count(task_outputs, "Task 1") >= 5);
    REQUIRE(count(task_outputs, "Task 2") >= 5);
}
```

### 4.2 性能测试

| 测试 | 目标值 | 测量方法 |
|------|-------|---------|
| 中断延迟 | ≤10 cycles | `mtime`精度测量 |
| 上下文切换 | ≤100 cycles | GPIO toggle |
| Tick 频率 | 1kHz ±1% | UART 输出间隔 |
| CPU 频率 | ≥50 MHz | cycle count / 时间 |

---

## 5. CI/CD 流水线

### 5.1 GitHub Actions

```yaml
name: riscv-mini FreeRTOS CI

on: [push, pull_request]

jobs:
  build-and-test:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v4
      
      - name: Configure CMake
        run: cmake -B build
      
      - name: Build
        run: cmake --build build -j$(nproc)
      
      - name: Run Unit Tests
        run: ctest -L unit --output-on-failure
      
      - name: Run Integration Tests
        run: ctest -L integration --output-on-failure
      
      - name: Run FreeRTOS Boot Test
        run: ./build/tests/test_freertos_boot
      
      - name: Upload Test Report
        uses: actions/upload-artifact@v4
        with:
          name: test-report
          path: build/Testing/
```

### 5.2 Gate 标准

| Phase | Gate | CI 要求 |
|-------|------|--------|
| A | CSR 测试 | 单元测试 100% pass |
| B | Pipeline | 集成测试 ≥90% pass |
| C | Interrupt | 中断流测试 pass |
| D | SoC | ELF + UART pass |
| E | FreeRTOS | Boot test pass |

---

## 6. 覆盖率要求

| 类型 | 目标 | 工具 |
|------|------|------|
| 代码覆盖率 | ≥80% | gcov/lcov |
| 分支覆盖率 | ≥70% | gcov |
| 功能覆盖率 | 100% | 手动检查 |
| 断言密度 | ≥1 断言/10 行 | 静态分析 |

---

## 7. 调试策略

### 7.1 仿真调试

```cpp
// 启用详细日志
ch::Simulator sim(&ctx, ch::SimulatorConfig{
    .trace_enabled = true,
    .trace_vcd = "waves.vcd",
    .verbose = true
});

// GTKWave 查看波形
// gtkwave waves.vcd
```

### 7.2 UART Console

```cpp
// FreeRTOS 输出重定向到仿真 stdout
void vPrintString(const char* msg) {
    printf("[FreeRTOS] %s\n", msg);
}
```

---

## 8. 验收清单

### Phase A

- [ ] `test_rv32i_csr.cpp` pass (5 tests)
- [ ] `test_mret.cpp` pass (5 tests)
- [ ] 编译 0 warnings
- [ ] CI green

### Phase B

- [ ] `test_pipeline_execution.cpp` pass (10 tests)
- [ ] 20 条指令序列正确执行
- [ ] 内存读写验证通过

### Phase C

- [ ] `test_exception_flow.cpp` pass
- [ ] MRET 返回后 PC==mepc
- [ ] mcause 编码正确

### Phase D

- [ ] `test_soc_integration.cpp` pass
- [ ] ELF 加载成功
- [ ] UART 打印 "Hello"

### Phase E

- [ ] `test_freertos_boot.cpp` pass
- [ ] 双任务切换 ≥10 次
- [ ] 无崩溃，连续运行 10 秒

