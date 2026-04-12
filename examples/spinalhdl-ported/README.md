# SpinalHDL 移植示例

> **状态**: 17 个移植全部完成 ✅  
> **最后更新**: 2026-04-12

本目录包含从 SpinalHDL 移植到 CppHDL 的经典示例。每个示例都包含完整的代码、仿真测试和 Verilog 生成功能。

---

## 📁 目录结构

```
examples/spinalhdl-ported/
├── README.md                     # 本文件
├── testbench_template.cpp        # 测试平台模板
├── counter/                      # ✅ 计数器（2 个版本）
├── fifo/                         # ✅ 流式 FIFO
├── uart/                         # ✅ UART 发送器
├── uart_rx/                      # ✅ UART 接收器
├── pwm/                          # ✅ PWM 调制器
├── gpio/                         # ✅ 通用 IO + 中断
├── spi/                          # ✅ SPI 控制器
├── i2c/                          # ✅ I2C 控制器
├── timer/                        # ✅ 可编程定时器
├── vga/                          # ✅ VGA 控制器 (640x480)
├── ws2812/                       # ✅ WS2812 LED 控制器
├── quadrature_encoder/           # ✅ 正交编码器
├── sigma_delta_dac/              # ✅ Σ-Delta DAC
├── width_adapter/                # ✅ 位宽适配器
├── assert/                       # ✅ 断言系统示例
└── phase1c/                      # ✅ 集成测试
```

---

## 📊 移植清单

| # | 模块 | 复杂度 | 质量 | Catch2 测试 | 说明 |
|---|------|--------|------|-------------|------|
| 1 | counter_simple | ⭐ | ✅ 稳定 | ❌ | 自由运行计数器 |
| 2 | counter | ⭐⭐ | ✅ 稳定 | ❌ | 带 enable/clear |
| 3 | fifo | ⭐⭐⭐ | ✅ 稳定 | ❌ | StreamFifo，带反压 |
| 4 | uart_tx | ⭐⭐⭐ | ✅ 稳定 | ❌ | UART 发送器，状态机 |
| 5 | uart_rx | ⭐⭐⭐ | ✅ 稳定 | ❌ | UART 接收器 |
| 6 | pwm | ⭐⭐ | ✅ 稳定 | ❌ | PWM，占空比可调 |
| 7 | gpio | ⭐⭐ | ✅ 稳定 | ❌ | 8 位 GPIO + 边沿检测 |
| 8 | spi | ⭐⭐⭐⭐ | ✅ 稳定 | ❌ | SPI 主控制器，状态机 |
| 9 | i2c | ⭐⭐⭐ | ⚠️ 简化 | ❌ | I2C 基础时序 |
| 10 | timer | ⭐⭐ | ✅ 稳定 | ❌ | 预分频 + 自动重装载 |
| 11 | vga | ⭐⭐⭐ | ✅ 稳定 | ❌ | 640x480@60Hz 时序 |
| 12 | ws2812 | ⭐⭐⭐⭐ | ✅ 稳定 | ❌ | WS2812B LED 驱动 |
| 13 | quadrature_encoder | ⭐⭐⭐ | ✅ 稳定 | ❌ | AB 相 4 倍频解码 |
| 14 | sigma_delta_dac | ⭐⭐ | ✅ 稳定 | ❌ | 一阶 Σ-Delta 调制 |
| 15 | width_adapter | ⭐⭐ | ✅ 稳定 | ✅ 已注册 | Stream 位宽转换 |
| 16 | assert | ⭐ | ✅ 稳定 | ✅ CTest | 断言系统演示 |
| 17 | phase1c | ⭐⭐ | ✅ 稳定 | ✅ CTest | 集成测试 |

### AXI4 总线示例 (`examples/axi4/`)

| # | 模块 | 复杂度 | 质量 | CTest | 说明 |
|---|------|--------|------|-------|------|
| 18 | AXI4-Lite | ⭐⭐⭐ | ✅ 稳定 | ✅ CTest | 最简 AXI-Lite 时序验证 |
| 19 | AXI4 Full | ⭐⭐⭐⭐ | ✅ 稳定 | ✅ CTest | 全功能 AXI4，突发传输 |
| 20 | AXI SoC | ⭐⭐⭐⭐ | ✅ 稳定 | ✅ CTest | AXI 矩阵 + GPIO + UART + Timer |
| 21 | Phase 3A | ⭐⭐⭐⭐ | ✅ 稳定 | ✅ CTest | Interconnect + AXI-to-Lite 转换器 |

### RISC-V Mini 示例 (`examples/riscv-mini/`)

| # | 模块 | 复杂度 | 质量 | CTest | 说明 |
|---|------|--------|------|-------|------|
| 22 | RV32I Phase 1 | ⭐⭐ | ✅ 稳定 | ✅ CTest | RegFile + ALU + 解码器 |
| 23 | RV32I Phase 2 | ⭐⭐ | ✅ 稳定 | ✅ CTest | 分支 + 跳转指令 (13/13) |
| 24 | RV32I Phase 3 | ⭐⭐ | ✅ 稳定 | ✅ CTest | Load/Store 指令 (13/13) |

> ⚠️ RISC-V Mini 代码债务见下方「已知问题」

---

## 🚀 快速开始

### 构建所有示例

```bash
cd build && cmake .. && make -j$(nproc)
```

### 运行单个示例

```bash
./examples/spinalhdl-ported/pwm/pwm_example          # PWM
./examples/spinalhdl-ported/gpio/gpio_example        # GPIO
./examples/spinalhdl-ported/spi/spi_controller_example  # SPI
```

### 运行测试

```bash
cd build && ctest --output-on-failure
```

---

## ✅ 测试通过标准

每个移植示例必须满足：
1. **编译通过** — 无编译错误和警告
2. **仿真正常** — `main()` 中的自测试输出正确
3. **Verilog 生成** — `toVerilog()` 成功生成有效代码
4. **Catch2 测试** — 独立的单元测试通过（逐步补全中）

---

## 📐 使用模板

创建新示例时，复制 `testbench_template.cpp` 并修改：

```cpp
// 1. 定义被测模块
class MyModule : public ch::Component {
public:
    __io(
        ch_in<ch_bool> enable;
        ch_out<ch_uint<8>> output;
    )
    
    void describe() override {
        // 实现逻辑
    }
};

// 2. 在 Top 中实例化（使用 ch::ch_module<T>）
class Top : public ch::Component {
    void describe() override {
        ch::ch_module<MyModule> inst{"my_inst"};
        inst.io().enable <<= io().enable;
        io().output <<= inst.io().output;
    }
};
```

> ⚠️ 使用 `ch::ch_module<T>` 而非 `CH_MODULE` 宏（后者不支持模板类）

---

## ⚠️ 已知问题

### RISC-V Mini 代码债务 (`examples/riscv-mini/`)

| 问题 | 文件 | 严重程度 | 说明 |
|------|------|---------|------|
| 已禁用文件 | `rv32i_minimal.cpp`, `phase3b_minimal.cpp` | 🔴 | `ForwardingMux` / `RV32ICore` 类名不存在，API 变更 |
| 未编译代码 | `phase3b_complete.cpp` | 🟡 | 存在但未在 CMakeLists.txt 中注册 |
| 未编译代码 | `rv32i_test.cpp`, `rv32i_phase3_test.cpp` 外的测试 | 🟡 | `rv32i_test.cpp` 引用已废弃头文件 |
| 孤立 tests/ | `tests/` 目录 CMakeLists.txt 未被主 CMakeLists 引用 | 🟡 | 9 个测试文件无法构建 |
| stages/ 头文件 | `src/stages/*.h` | 🟡 | IF/ID/EX/MEM/WB 阶段完整但未被活跃测试使用 |
| 流水线 Cache | `src/rv32i_pipeline_cache.h` | 🟡 | 集成 I/D-Cache 但依赖链不完整 |
| 分支预测器 | `src/branch_predictor_v2.h`, `dynamic_branch_predict.h` | 🟡 | 两个版本共存，v2 有测试，dynamic 无 |

---

## 🔗 参考资料

- [CppHDL 架构差距分析](../../docs/architecture/2026-03-30-cpphdl-architecture-gap-analysis.md)
- [SpinalHDL 移植架构](../../docs/architecture/2026-03-30-spinalhdl-port-architecture.md)
- [Phase 架构总览](../../docs/architecture/phases/README.md)
- [官方 Counter 示例](../../samples/counter.cpp)
