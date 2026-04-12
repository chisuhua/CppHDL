# CppHDL Agent 开发指南

> **定位**: SpinalHDL 示例移植到 CppHDL 的开发规范  
> **核心原则**: 先学习现有代码 → 再实现新功能 | 使用正确 IO 模式 | 验证后再提交

---

## 1. 移植前必学：现有示例和测试

### 1.1 必须阅读的示例代码

在开始任何移植任务前，**必须**先阅读以下示例理解正确模式：

| 示例 | 文件 | 学习重点 |
|------|------|---------|
| **FIFO** | `samples/fifo_example.cpp` | IO 端口使用、模块实例化、仿真 API |
| **移位寄存器** | `samples/shift_register.cpp` | 移位寄存器模式、concat 拼接 |
| **计数器** | `samples/counter.cpp` | 简单计数器模式、Verilog 生成 |
| **简单 IO** | `samples/simple_io.cpp` | 基础 IO 操作 |
| **Stream 示例** | `samples/spinalhdl_stream_example.cpp` | Stream 模式、流处理 |

### 1.2 必须阅读的测试用例

| 测试 | 文件 | 学习重点 |
|------|------|---------|
| FIFO 测试 | `tests/test_fifo.cpp`, `tests/test_fifo_example.cpp` | FIFO 验证方法 |
| Stream 测试 | `tests/test_stream*.cpp` | Stream 操作验证 |
| 状态机测试 | `tests/test_state_machine.cpp` | 状态机验证方法 |
| 位宽适配器 | `tests/test_stream_width_adapter_complete.cpp` | 编译时验证 |

### 1.3 关键代码模式速查

#### IO 端口使用（❗最重要）

```cpp
// ✅ 正确：Testbench 中使用 Simulator API
sim.set_input_value(device.instance().io().push, true);
auto val = sim.get_port_value(device.instance().io().dout);

// ✅ 正确：describe() 中直接使用 IO 端口
void describe() override {
    auto writing = io().push;  // 无需转换
    rd_ptr->next = select(writing, rd_ptr + 1_b, rd_ptr);
}

// ❌ 错误：不能直接赋值
io().push = ch_bool(true);  // 编译错误！

// ❌ 错误：不能用 && 连接 IO 端口
ch_bool x = io().push && io().ready;  // 编译错误！
// ✅ 正确：使用 select()
ch_bool x = select(io().push, io().ready, ch_bool(false));
```

#### 模块实例化

```cpp
// ✅ 正确：使用 ch::ch_module
ch::ch_module<FiFo<ch_uint<8>, 16>> fifo_inst{"fifo_inst"};
fifo_inst.io().din <<= io().din;
io().dout <<= fifo_inst.io().dout;

// ❌ 错误：CH_MODULE 宏可能不工作
CH_MODULE(FiFo<8, 16>, fifo_inst);  // 可能编译失败
```

#### 移位寄存器模式

```cpp
// 多字节移位寄存器（用于位宽转换）
ch_reg<ch_uint<8>> byte0(0_d);
ch_reg<ch_uint<8>> byte1(0_d);

byte0->next = select(shift_en, io().din, byte0);
byte1->next = select(shift_en, byte0, byte1);

// 拼接为宽输出
auto tmp = concat(byte1, byte0);  // 16-bit
```

#### 状态机模式

```cpp
// 使用状态机 DSL
ch_state_machine<StateEnum, N> sm;

sm.state(StateEnum::IDLE)
  .on_active([this, &sm]() {
      if (io().start == ch_bool(true))
          sm.transition_to(StateEnum::RUNNING);
  });

sm.set_entry(StateEnum::IDLE);
sm.build();
```

---

## 2. 移植工作流程

### 2.1 标准流程

```
1. 学习阶段
   ├── 阅读 SpinalHDL 原代码
   ├── 查找 CppHDL 现有类似示例
   ├── 阅读相关测试用例
   └── 理解 IO 端口和模块模式

2. 实现阶段
   ├── 创建示例框架（参考现有示例）
   ├── 实现核心逻辑（使用 select() 代替 &&）
   ├── 编写测试平台（使用 sim.set_input_value()）
   └── 生成 Verilog 验证

3. 验证阶段
   ├── 编译通过
   ├── 仿真运行正常
   ├── Verilog 生成成功
   └── 输出符合预期
```

### 2.2 检查清单

在提交任何移植代码前，必须确认：

- [ ] 已阅读 `samples/fifo_example.cpp` 理解 IO 模式
- [ ] 已阅读相关现有示例（如移位寄存器、计数器等）
- [ ] Testbench 使用 `sim.set_input_value()` 而非直接赋值
- [ ] `describe()` 中使用 `select()` 而非 `&&` 处理 IO 端口
- [ ] 模块实例化使用 `ch::ch_module<...>`
- [ ] 编译无错误
- [ ] 仿真运行正常
- [ ] Verilog 生成成功

---

## 3. 常见错误及修复

### 3.1 IO 端口类型转换错误

**错误**:
```
error: conversion from 'ch::core::ch_in<ch::core::ch_bool>' 
       to non-scalar type 'ch::core::ch_bool' requested
```

**原因**: 尝试将 IO 端口直接赋值给 `ch_bool` 变量

**修复**:
```cpp
// ❌ 错误
ch_bool x = io().push;

// ✅ 正确 - 直接使用，无需转换
auto x = io().push;
// 或者在 select() 中直接使用
result = select(io().push, val1, val0);
```

### 3.2 && 运算符错误

**错误**:
```
error: no match for 'operator&&' 
       (operand types are 'ch::core::ch_bool' and 'ch::core::ch_in<...>')
```

**原因**: 尝试用 `&&` 连接 IO 端口

**修复**:
```cpp
// ❌ 错误
ch_bool will_write = wren && !is_full(count);

// ✅ 正确
ch_bool will_write = select(wren, ch_bool(!is_full(count)), ch_bool(false));
```

### 3.3 模块实例化错误

**错误**:
```
error: template argument 1 is invalid
```

**原因**: `CH_MODULE` 宏不支持模板参数

**修复**:
```cpp
// ❌ 错误
CH_MODULE(StreamFifoModule<8, 16>, fifo_inst);

// ✅ 正确
ch::ch_module<StreamFifoModule<8, 16>> fifo_inst{"fifo_inst"};
```

### 3.4 Bundle IO 端口错误（新增）

**错误**:
```
error: 'ch::core::ch_out<HazardControl>' has no member named 'if_stall'
```

**原因**: Bundle 结构体不能作为 `__io()` 端口类型

**修复**:
```cpp
// ❌ 错误：Bundle 结构体作为 IO 端口
struct HazardControl {
    ch_bool if_stall;
    ch_bool id_stall;
};

__io(
    ch_out<HazardControl> hazard_ctrl;  // ❌ 编译错误！
)
io().hazard_ctrl.if_stall = ...;

// ✅ 正确：使用单独的 ch_bool/ch_uint 端口
__io(
    ch_out<ch_bool> if_stall;      // ✅ 单独端口
    ch_out<ch_bool> id_stall;
    ch_out<ch_uint<2>> forward_a;  // ✅ 单独 ch_uint 端口
)
io().if_stall = load_use_hazard;
io().forward_a = select(condition, ch_uint<2>(1_d), ch_uint<2>(0_d));

// ✅ 或使用 Class 成员模式（推荐用于流接口）
class HazardUnit : public ch::Component {
public:
    ch::ch_stream<ch_uint<32>> stream_out;  // ✅ Bundle 作为成员

    HazardUnit(...) : ch::Component(parent, name), stream_out() {
        stream_out.as_master();  // ✅ 设置方向
    }

    void describe() override {
        stream_out.payload = counter;  // ✅ 直接访问成员
        stream_out.valid = 1_b;
    }
};
```

**参考文件**:
- `samples/bundle_top_example.cpp` - Bundle 成员模式（推荐）
- `include/chlib/forwarding.h` - 单独端口模式（参考）
- `skills/cpphdl-bundle-patterns/SKILL.md` - 完整 Bundle 模式指南

---

### 3.5 Testbench Simulator API 错误（新增）

**错误**:
```
error: no matching function for call to 'ch::Simulator::set_port_value(ch::core::ch_in<...>&, ch::core::ch_bool)'
```

**原因**: `set_port_value()` 需要 `uint64_t` 值参数，不能使用 `ch_bool()` 或 `ch_uint<>()` 包装

**修复**:
```cpp
// ❌ 错误：使用 ch_bool/ch_uint 包装
sim.set_port_value(hazard.io().ex_reg_write, ch_bool(true));
sim.set_port_value(hazard.io().id_rs1_addr, ch_uint<5>(5_d));

// ✅ 正确：直接使用整数
sim.set_port_value(hazard.io().ex_reg_write, 1);
sim.set_port_value(hazard.io().id_rs1_addr, 5);
sim.set_port_value(hazard.io().ex_alu_result, 42);  // ch_uint<32>
```

**错误 2**:
```
error: 'class ch::Simulator' has no member named 'run_for'
```

**原因**: Simulator 没有 `run_for()` 方法，应该使用 `tick()`

**修复**:
```cpp
// ❌ 错误
sim.run_for(10'000'000'000ULL);

// ✅ 正确
sim.tick();  // 运行一个时钟周期
```

**错误 3**:
```
error: cannot convert 'const ch::core::sdata_type' to 'uint64_t'
```

**原因**: `get_port_value()` 返回 `sdata_type`，需要转换

**修复**:
```cpp
// ❌ 错误
uint64_t stall = sim.get_port_value(hazard.io().stall);

// ✅ 正确：隐式转换
auto stall = sim.get_port_value(hazard.io().stall);
REQUIRE(stall == 1);  // C++ 自动转换

// ✅ 正确：显式转换
auto stall = sim.get_port_value(hazard.io().stall);
uint64_t stall_u64 = stall;
REQUIRE(stall_u64 == 1);
```

**参考**:
- `skills/cpphdl-simulator-port-value/SKILL.md` - Simulator API 快速参考
- `tests/riscv/test_hazard_compile.cpp` - 实战示例

---

## 4. 参考资源

### 4.1 示例代码位置

```
samples/
├── fifo_example.cpp          # FIFO 实现（必读）
├── shift_register.cpp        # 移位寄存器（必读）
├── counter.cpp               # 计数器（必读）
├── simple_io.cpp             # 简单 IO
├── spinalhdl_stream_example.cpp  # Stream 示例
└── ...
```

### 4.2 测试用例位置

```
tests/
├── test_fifo.cpp             # FIFO 测试
├── test_fifo_example.cpp     # FIFO 示例测试
├── test_stream*.cpp          # Stream 测试
├── test_state_machine.cpp    # 状态机测试
└── ...
```

### 4.3 库头文件位置

```
include/chlib/
├── fifo.h                    # FIFO 函数
├── stream.h                  # Stream 定义
├── stream_width_adapter.h    # 位宽适配器
├── state_machine.h           # 状态机 DSL
└── ...
```

### 4.4 文档位置

```
docs/architecture/
├── 2026-03-30-cpphdl-architecture-gap-analysis.md  # 架构差距分析
├── 2026-03-30-state-machine-dsl-design.md          # 状态机设计
└── ...
```

---

## 5. 已移植示例清单

> **最后更新**: 2026-04-12 | 17 个移植全部完成

| # | 示例 | 状态 | 文件 | Verilog | Catch2 测试 |
|---|------|------|------|---------|------------|
| 1 | Counter (simple) | ✅ | `examples/spinalhdl-ported/counter/counter_simple.cpp` | ✅ | ❌ |
| 2 | Counter (full) | ✅ | `examples/spinalhdl-ported/counter/counter.cpp` | ✅ | ❌ |
| 3 | UART TX | ✅ | `examples/spinalhdl-ported/uart/uart_tx.cpp` | ✅ | ❌ |
| 4 | UART RX | ✅ | `examples/spinalhdl-ported/uart_rx/uart_rx.cpp` | ✅ | ❌ |
| 5 | FIFO | ✅ | `examples/spinalhdl-ported/fifo/stream_fifo_example.cpp` | ✅ | ❌ |
| 6 | PWM | ✅ | `examples/spinalhdl-ported/pwm/pwm.cpp` | ✅ | ❌ |
| 7 | GPIO | ✅ | `examples/spinalhdl-ported/gpio/gpio.cpp` | ✅ | ❌ |
| 8 | SPI Controller | ✅ | `examples/spinalhdl-ported/spi/spi_controller.cpp` | ✅ | ❌ |
| 9 | I2C Controller | ✅ | `examples/spinalhdl-ported/i2c/i2c_controller.cpp` | ✅ | ❌ |
| 10 | Timer | ✅ | `examples/spinalhdl-ported/timer/timer.cpp` | ✅ | ❌ |
| 11 | VGA Controller | ✅ | `examples/spinalhdl-ported/vga/vga_controller.cpp` | ✅ | ❌ |
| 12 | WS2812 Controller | ✅ | `examples/spinalhdl-ported/ws2812/ws2812_controller.cpp` | ✅ | ❌ |
| 13 | Quadrature Encoder | ✅ | `examples/spinalhdl-ported/quadrature_encoder/quadrature_encoder.cpp` | ✅ | ❌ |
| 14 | Sigma-Delta DAC | ✅ | `examples/spinalhdl-ported/sigma_delta_dac/sigma_delta_dac.cpp` | ✅ | ❌ |
| 15 | Width Adapter | ✅ | `examples/spinalhdl-ported/width_adapter/stream_width_adapter_example.cpp` | ✅ | ✅ |
| 16 | Assert | ✅ | `examples/spinalhdl-ported/assert/assert_example.cpp` | ✅ | ❌ |
| 17 | Phase 1C 集成测试 | ✅ | `examples/spinalhdl-ported/phase1c/phase1c_test.cpp` | N/A | ❌ |

---

## 6. 当前技术债与待完善项

> 以下为已移植代码需要进一步完善的工作，**不是**新的移植任务。

| 优先级 | 项目 | 说明 | 预计工时 |
|--------|------|------|---------|
| P0 | Catch2 单元测试 | 16 个端口仅有 `main()` 自测，需编写独立测试 | ~4h |
| P0 | CTest 注册 | 所有示例需通过 `add_test()` 注册到 CTest | ~1h |
| P1 | I2C 完整实现 | 当前为简化版，缺少 ACK 处理和完整时序 | ~3h |
| P1 | API 合规审查 | 部分代码使用已废弃 API（如 `get_value()` vs `get_port_value()`） | ~2h |
| P2 | 模块文档 | 每个端口配套 detailed-design.md | ~4h |
| P2 | 代码/测试分离 | 当前模块+testbench+main() 在同一文件 | 架构决策 |

---

## 7. 关键原则总结

> **原则 1**: 移植前先学习 - 阅读现有示例和测试，理解正确模式

> **原则 2**: IO 端口不是变量 - 使用 `sim.set_input_value()` 在 testbench 中设置，使用 `select()` 在 `describe()` 中处理

> **原则 3**: 模块实例化用 `ch::ch_module` - 不要用 `CH_MODULE` 宏处理模板类

> **原则 4**: 验证后再提交 - 编译 + 仿真 + Verilog 生成全部通过

---

**版本**: v1.0  
**创建时间**: 2026-03-30  
**维护人**: DevMate  
**下次更新**: 发现新模式或修复常见错误时

---

**版本**: v1.0  
**创建时间**: 2026-03-30  
**维护人**: DevMate  
**下次更新**: 发现新模式或修复常见错误时
