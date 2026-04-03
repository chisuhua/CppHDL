# Phase 3 规划 - AXI4 SoC 与 RISC-V 集成

**创建日期**: 2026-04-01  
**阶段目标**: 实现完整 SoC 系统，支持 RISC-V 处理器集成  
**预计工期**: 2 周 (2026-04-02 ~ 2026-04-15)  
**当前状态**: 🟢 Phase 3A 完成 + Phase 3B T305 完成 (60%) ✅

---

## 📊 当前基础

### ✅ 已有组件

| 组件 | 位置 | 状态 | 说明 |
|------|------|------|------|
| AXI4-Lite Slave | `include/axi4/axi4_lite_slave.h` | ✅ | 基础从设备 |
| AXI4-Lite Matrix | `include/axi4/axi4_lite_matrix.h` | ✅ | 2x2 矩阵 |
| AXI GPIO | `include/axi4/peripherals/axi_gpio.h` | ✅ | 8 位 GPIO |
| AXI UART | `include/axi4/peripherals/axi_uart.h` | ✅ | UART 控制器 |
| AXI Timer | `include/axi4/peripherals/axi_timer.h` | ✅ | 定时器 |
| AXI4-Lite 示例 | `examples/axi4/axi4_lite_example.cpp` | ✅ | 基础验证 |
| AXI SoC 示例 | `examples/axi4/axi_soc_example.cpp` | ✅ | SoC 集成 |

### ⚠️ 待完善

| 组件 | 优先级 | 工作量 | 说明 |
|------|--------|--------|------|
| AXI4 全功能支持 | 🔴 高 | 2 天 | AW/W/B/AR/R 通道完整实现 |
| AXI Interconnect | 🔴 高 | 2 天 | 可扩展矩阵 (支持 4x4/8x8) |
| RISC-V 处理器 | 🔴 高 | 3 天 | RV32I 基础指令集 |
| 中断控制器 | 🟡 中 | 1 天 | PLIC 简化版 |
| DMA 控制器 | 🟡 中 | 2 天 | 简单 DMA 引擎 |
| DDR 控制器 | 🟢 低 | 2 天 | 简化版 DDR 模型 |

---

## 🎯 Phase 3 目标

### Phase 3A: AXI4 总线系统 (3 天)

| Task ID | 任务 | 状态 | 预计工时 | 验收标准 |
|---------|------|------|---------|---------|
| T301 | AXI4 全功能支持 | ✅ | 8h | AW/W/B/AR/R 通道完整 |
| T302 | AXI Interconnect 4x4 | ✅ | 8h | 支持 4 主 4 从 |
| T303 | AXI 到 AXI-Lite 转换器 | ⏳ | 4h | 全功能→精简转换 |
| T304 | AXI 示例验证 | ⏳ | 4h | 仿真 + Verilog 生成 |

**交付物**:
- ✅ `include/axi4/axi4_full.h` - AXI4 全功能从设备
- ⏳ `include/axi4/axi_interconnect_4x4.h` - 4x4 矩阵
- ⏳ `include/axi4/axi_to_axilite.h` - 协议转换器
- ✅ `examples/axi4/axi4_full_example.cpp` - 验证示例

---

### Phase 3B: RISC-V 处理器集成 (4 天)

| Task ID | 任务 | 状态 | 预计工时 | 验收标准 |
|---------|------|------|---------|---------|
| T305 | RV32I 核心实现 | ✅ | 16h | 40 条基础指令 |
| T306 | 指令存储器 (I-TCM) | ⏳ | 4h | 零等待 ROM |
| T307 | 数据存储器 (D-TCM) | ⏳ | 4h | 单周期 SRAM |
| T308 | AXI4 总线接口 | ⏳ | 8h | 指令 + 数据端口 |
| T309 | 简单调试模块 | ⏳ | 4h | 断点 + 单步 |

**交付物**:
- ✅ `include/riscv/rv32i_core.h` - RV32I 核心
- ✅ `include/riscv/rv32i_decoder.h` - 指令译码
- ✅ `include/riscv/rv32i_alu.h` - 执行单元
- ✅ `examples/riscv/rv32i_minimal.cpp` - 最小系统

---

### Phase 3C: SoC 集成 (3 天)

| Task ID | 任务 | 预计工时 | 验收标准 |
|---------|------|---------|---------|
| T310 | 中断控制器 (PLIC) | 8h | 支持 8 个中断源 |
| T311 | SoC 顶层设计 | 8h | RISC-V + AXI + 外设 |
| T312 | 启动代码 (Boot ROM) | 4h | 复位向量 + 跳转 |
| T313 | Hello World 示例 | 4h | UART 输出验证 |

**交付物**:
- `include/soc/cpptl_soc.h` - SoC 顶层
- `include/soc/plic.h` - 中断控制器
- `examples/soc/hello_world.cpp` - Hello World
- `examples/soc/gpio_interrupt.cpp` - GPIO 中断

---

### Phase 3D: 高级功能 (可选，2 天)

| Task ID | 任务 | 预计工时 | 验收标准 |
|---------|------|---------|---------|
| T314 | DMA 控制器 | 8h | 内存到内存传输 |
| T315 | PWM 控制器 | 4h | 多通道 PWM |
| T316 | SPI 控制器 (AXI) | 4h | AXI 接口 SPI |
| T317 | I2C 控制器 (AXI) | 4h | AXI 接口 I2C |

**交付物**:
- `include/soc/dma_controller.h` - DMA 引擎
- `include/axi4/peripherals/axi_pwm.h` - AXI PWM
- `include/axi4/peripherals/axi_spi.h` - AXI SPI
- `include/axi4/peripherals/axi_i2c.h` - AXI I2C

---

## 📐 系统架构

### Phase 3A: AXI4 总线系统

```
                  ┌─────────────────────────┐
                  │   AXI Interconnect 4x4  │
                  │                         │
  Master 0 ───────│                         │───▶ Slave 0 (GPIO @ 0x0000_0000)
  (RISC-V I)      │                         │
  Master 1 ───────│                         │───▶ Slave 1 (Timer @ 0x1000_0000)
  (RISC-V D)      │                         │
  Master 2 ───────│                         │───▶ Slave 2 (UART @ 0x2000_0000)
  (DMA)           │                         │
  Master 3 ───────│                         │───▶ Slave 3 (Reserved @ 0x8000_0000)
  (Reserved)      │                         │
                  └─────────────────────────┘
```

### Phase 3B: RISC-V 最小系统

```
     ┌──────────────┐
     │   RV32I Core │
     │              │
     │  ┌────────┐  │
     │  │ IFetch │  │───────▶ I-TCM (Instruction Memory)
     │  └────────┘  │
     │  ┌────────┐  │
     │  │  ALU   │  │
     │  └────────┘  │
     │  ┌────────┐  │
     │  │  Load  │  │───────▶ D-TCM (Data Memory)
     │  │  Store │  │
     │  └────────┘  │
     └──────────────┘
```

### Phase 3C: 完整 SoC

```
                    ┌─────────────────────────────────┐
                    │         AXI Interconnect        │
                    │             4x4 Matrix          │
                    └─────────────────────────────────┘
           │              │              │              │
           ▼              ▼              ▼              ▼
    ┌────────────┐ ┌────────────┐ ┌────────────┐ ┌────────────┐
    │  RV32I     │ │   PLIC     │ │   UART     │ │   GPIO     │
    │  Core      │ │ Interrupt  │ │ Controller │ │ Controller │
    │ (Master)   │ │ Controller │ │ (Slave)    │ │ (Slave)    │
    └────────────┘ └────────────┘ └────────────┘ └────────────┘
           │
           ▼
    ┌────────────┐
    │   Timer    │
    │ Controller │
    │ (Slave)    │
    └────────────┘
```

---

## 📅 执行计划

### Week 1 (2026-04-02 ~ 2026-04-08)

| 日期 | 任务 | 交付物 |
|------|------|--------|
| Thu 04-02 | T301: AXI4 全功能支持 | `axi4_full.h` |
| Fri 04-03 | T302: AXI Interconnect 4x4 | `axi_interconnect_4x4.h` |
| Sat 04-04 | T303: AXI 到 AXI-Lite 转换器 | `axi_to_axilite.h` |
| Sun 04-05 | T304: AXI 示例验证 | `axi4_full_example.cpp` |
| Mon 04-06 | T305: RV32I 核心实现 (1/2) | `rv32i_core.h` (部分) |
| Tue 04-07 | T305: RV32I 核心实现 (2/2) | `rv32i_core.h` (完整) |
| Wed 04-08 | T306-T307: 存储器系统 | `rv32i_tcm.h` |

### Week 2 (2026-04-09 ~ 2026-04-15)

| 日期 | 任务 | 交付物 |
|------|------|--------|
| Thu 04-09 | T308: AXI4 总线接口 | `rv32i_axi_interface.h` |
| Fri 04-10 | T309: 调试模块 | `rv32i_debug.h` |
| Sat 04-11 | T310: 中断控制器 | `plic.h` |
| Sun 04-12 | T311: SoC 顶层设计 | `cpptl_soc.h` |
| Mon 04-13 | T312-T313: 启动代码 + Hello World | `hello_world.cpp` |
| Tue 04-14 | 系统验证 + 调试 | 测试报告 |
| Wed 04-15 | Phase 3 评审 + 文档 | Phase 3 报告 |

---

## ✅ 验收标准

### Phase 3A: AXI4 总线系统

- [ ] AW/W/B/AR/R 通道完整实现
- [ ] 4x4 Interconnect 可配置
- [ ] AXI 到 AXI-Lite 转换正确
- [ ] 仿真测试通过
- [ ] Verilog 生成可综合

### Phase 3B: RISC-V 处理器

- [ ] RV32I 40 条指令完整支持
- [ ] 指令/数据存储器可访问
- [ ] AXI4 总线接口正常工作
- [ ] 简单程序可执行（如 LED 闪烁）

### Phase 3C: SoC 集成

- [ ] RISC-V + AXI + 外设集成
- [ ] 中断控制器正常工作
- [ ] Hello World UART 输出
- [ ] GPIO 中断响应

---

## 🔧 技术要点

### AXI4 协议关键点

1. **5 个独立通道**:
   - AW (Write Address)
   - W (Write Data)
   - B (Write Response)
   - AR (Read Address)
   - R (Read Response)

2. **握手协议**:
   - VALID/READY 握手机制
   - 每个通道独立握手

3. **突发传输**:
   - 支持 INCR 突发
   - 支持 FIXED 突发
   - 最大突发长度：16

### RISC-V RV32I 指令集

| 类型 | 指令 | 数量 |
|------|------|------|
| LUI | LUI | 1 |
| AUIPC | AUIPC | 1 |
| JAL | JAL | 1 |
| JALR | JALR | 1 |
| BRANCH | BEQ, BNE, BLT, BGE, BLTU, BGEU | 6 |
| LOAD | LB, LH, LW, LBU, LHU | 5 |
| STORE | SB, SH, SW | 3 |
| OP-IMM | ADDI, SLTI, SLTIU, XORI, ORI, ANDI, SLLI, SRLI, SRAI | 9 |
| OP | ADD, SUB, SLL, SLT, SLTU, XOR, SRL, SRA, OR, AND | 10 |
| **总计** | | **40** |

---

## 📊 风险评估

| 风险 | 概率 | 影响 | 缓解措施 |
|------|------|------|---------|
| AXI4 协议复杂 | 中 | 高 | 参考 AMBA 规范，分步实现 |
| RISC-V 指令译码 | 中 | 中 | 使用查表法简化译码 |
| 时序收敛困难 | 高 | 中 | 添加流水线级 |
| 进度延期 | 中 | 中 | 优先完成核心功能 |

---

## 📝 参考文档

- [AMBA® AXI and ACE Protocol Specification](https://developer.arm.com/documentation/ihi0022/latest/)
- [RISC-V Instruction Set Manual](https://riscv.org/technical/specification/)
- [SiFive E21 Core Complex](https://www.sifive.com/cores/e21)
- [Western Digital SweRV Core](https://github.com/westerndigitalcorporation/swerv_core)

---

**创建人**: DevMate  
**创建日期**: 2026-04-01  
**下次更新**: 2026-04-02 (Phase 3A 开始)
