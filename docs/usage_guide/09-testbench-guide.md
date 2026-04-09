# Testbench 编写指南

**文档编号**: USAGE-09  
**版本**: v2.0  
**最后更新**: 2026-04-09  
**目标读者**: CppHDL 用户（硬件设计者）  
**前置知识**: [Simulator API 使用指南](04-simulator-api.md)

---

## 🎯 本章目标

学完本章，你将能够：

1. ✅ 使用 CppHDL 测试框架
2. ✅ 编写 Component testbench
3. ✅ 使用 Bundle 进行测试
4. ✅ 运行和验证测试
5. ✅ 避免常见测试错误

---

## 📖 测试框架概述

CppHDL 使用 **Catch2 v3.7.0** 测试框架，集成到 CMake/CTest 构建系统。

### 测试组织

| 类别 | 说明 | 标签 |
|------|------|------|
| 基础测试 | CppHDL 核心功能测试 | `[base]` |
| chlib 测试 | chlib 库组件测试 | `[chlib]` |
| 示例测试 | 使用示例验证功能 | 按需 |

---

## 🔧 Testbench 基本结构

### 标准模板

```cpp
// tests/test_my_component.cpp
#include "catch_amalgamated.hpp"
#include "ch.hpp"
#include "simulator.h"
#include "my_component.h"  // 被测模块

using namespace ch::core;

TEST_CASE("MyComponent - Functionality Test", "[my_component][testbench]") {
    // 1. 创建 ch_device（自动调用 build()）
    ch::ch_device<MyComponent> dev;
    
    // 2. 创建 Simulator
    Simulator sim(dev.context());
    
    // 3. 获取 IO 引用
    auto& io = dev.instance().io();
    
    // 4. 设置输入值
    sim.set_input_value(io.data_in, 0x42);
    sim.set_input_value(io.valid, 1);
    
    // 5. 运行仿真
    sim.tick();
    
    // 6. 验证输出
    auto output = sim.get_input_value(io.data_out);
    REQUIRE(static_cast<uint64_t>(output) == 0x42);
}
```

---

### 组成部分说明

**1. 头文件包含**:
```cpp
#include "catch_amalgamated.hpp"  // Catch2 框架
#include "ch.hpp"                 // CppHDL 核心
#include "simulator.h"            // Simulator
#include "my_component.h"         // 被测模块
```

**2. TEST_CASE 宏**:
```cpp
TEST_CASE("测试名称", "[标签 1][标签 2]")
```

**3. ch_device**:
```cpp
ch::ch_device<MyComponent> dev;  // 自动调用 build()
```

**4. Simulator**:
```cpp
Simulator sim(dev.context());    // 初始化仿真
```

**5. 断言**:
```cpp
REQUIRE(condition);              // 验证条件
CHECK(condition);                // 非致命验证
```

---

## 🎭 测试模式

### 模式 1: 独立信号测试

```cpp
TEST_CASE("Basic Signal Test", "[signal][basic]") {
    auto ctx = std::make_unique<ch::core::context>("test");
    ch::core::ctx_swap ctx_guard(ctx.get());
    
    ch_uint<8> signal(0_d);
    Simulator sim(ctx.get());
    
    // 设置和验证
    sim.set_input_value(signal, 0xA5);
    REQUIRE(sim.get_input_value(signal) == 0xA5);
}
```

**适用场景**:
- 基础 API 验证
- 独立信号操作
- 快速原型测试

---

### 模式 2: Component 功能测试

```cpp
TEST_CASE("Counter - Increment Function", "[counter][increment]") {
    ch::ch_device<Counter<8>> counter_dev;
    Simulator sim(counter_dev.context());
    
    auto& io = counter_dev.instance().io();
    
    // 设置初始值
    sim.set_input_value(io.reset, 1);
    sim.tick();
    
    sim.set_input_value(io.reset, 0);
    sim.set_input_value(io.enable, 1);
    
    // 运行多个周期
    for (int i = 0; i < 5; i++) {
        sim.tick();
    }
    
    // 验证计数值
    auto count = sim.get_input_value(io.count);
    REQUIRE(static_cast<uint64_t>(count) == 5);
}
```

**适用场景**:
- 模块功能验证
- 时序行为测试
- 状态机测试

---

### 模式 3: Bundle 测试

```cpp
TEST_CASE("Stream - Data Transfer", "[stream][bundle]") {
    ch::ch_device<StreamProducer<ch_uint<32>>> producer_dev;
    ch::ch_device<StreamConsumer<ch_uint<32>>> consumer_dev;
    
    Simulator sim_prod(producer_dev.context());
    Simulator sim_cons(consumer_dev.context());
    
    auto& prod_io = producer_dev.instance().io();
    auto& cons_io = consumer_dev.instance().io();
    
    // 设置主从方向
    prod_io.stream_out.as_master();
    cons_io.stream_in.as_slave();
    
    // 连接 Bundle（需要在同一个 context 中）
    // 注：实际测试中通常将 producer 和 consumer 放在顶层模块中
    
    // 设置测试数据
    sim_prod.set_input_value(prod_io.stream_out.payload, 0x12345678);
    sim_prod.set_input_value(prod_io.stream_out.valid, 1);
    
    sim_prod.tick();
    
    // 验证消费者接收
    auto payload = sim_cons.get_input_value(cons_io.stream_in.payload);
    REQUIRE(static_cast<uint64_t>(payload) == 0x12345678);
}
```

**适用场景**:
- Stream/Flow 测试
- AXI 总线测试
- 接口协议验证

---

### 模式 4: AXI 总线测试

```cpp
TEST_CASE("AXI Lite - Register Write/Read", "[axi][register]") {
    ch::ch_device<AxiLiteSlave> slave_dev;
    Simulator sim(slave_dev.context());
    
    auto& io = slave_dev.instance().io();
    
    // AXI 写事务
    sim.set_input_value(io.awaddr, 0x10);
    sim.set_input_value(io.awprot, 0);
    sim.set_input_value(io.awvalid, 1);
    
    // 等待握手
    while (sim.get_input_value(io.awready) == 0) {
        sim.tick();
    }
    
    sim.tick();
    sim.set_input_value(io.awvalid, 0);
    
    // 写数据
    sim.set_input_value(io.wdata, 0xDEADBEEF);
    sim.set_input_value(io.wstrb, 0xF);
    sim.set_input_value(io.wvalid, 1);
    
    while (sim.get_input_value(io.wready) == 0) {
        sim.tick();
    }
    
    sim.tick();
    sim.set_input_value(io.wvalid, 0);
    
    // 验证写入（通过内部状态或后续读操作）
    // ...
}
```

**适用场景**:
- AXI 协议验证
- 寄存器访问测试
- 总线互连测试

---

## ⚡ 测试技巧

### 技巧 1: 使用 SECTION 组织测试

```cpp
TEST_CASE("FIFO - Multiple Operations", "[fifo]") {
    ch::ch_device<FiFo<ch_uint<8>, 16>> fifo_dev;
    Simulator sim(fifo_dev.context());
    auto& io = fifo_dev.instance().io();
    
    SECTION("Empty FIFO") {
        REQUIRE(sim.get_input_value(io.empty) == 1);
        REQUIRE(sim.get_input_value(io.full) == 0);
    }
    
    SECTION("Write Operation") {
        sim.set_input_value(io.push, 1);
        sim.set_input_value(io.din, 0x42);
        sim.tick();
        
        REQUIRE(sim.get_input_value(io.empty) == 0);
    }
    
    SECTION("Read Operation") {
        // 先写入
        sim.set_input_value(io.push, 1);
        sim.set_input_value(io.din, 0x55);
        sim.tick();
        
        // 再读出
        sim.set_input_value(io.pop, 1);
        sim.tick();
        
        auto dout = sim.get_input_value(io.dout);
        REQUIRE(static_cast<uint64_t>(dout) == 0x55);
    }
}
```

**优点**:
- 逻辑清晰的测试分组
- 共享 setup 代码
- 独立验证各个功能点

---

### 技巧 2: 参数化测试

```cpp
TEST_CASE("ALU - Arithmetic Operations", "[alu]") {
    ch::ch_device<ALU<32>> alu_dev;
    Simulator sim(alu_dev.context());
    auto& io = alu_dev.instance().io();
    
    struct TestCase {
        uint32_t a, b;
        int op;
        uint32_t expected_result;
    };
    
    std::vector<TestCase> test_cases = {
        {10, 20, 0, 30},   // ADD
        {100, 50, 1, 50},  // SUB
        {7, 8, 2, 56},     // MUL
        {100, 3, 3, 33},   // DIV
    };
    
    for (const auto& tc : test_cases) {
        SECTION(std::to_string(tc.op)) {
            sim.set_input_value(io.a, tc.a);
            sim.set_input_value(io.b, tc.b);
            sim.set_input_value(io.op, tc.op);
            
            sim.tick();
            
            auto result = sim.get_input_value(io.result);
            REQUIRE(static_cast<uint32_t>(result) == tc.expected_result);
        }
    }
}
```

**优点**:
- 减少代码重复
- 易于添加新测试用例
- 清晰的测试覆盖

---

### 技巧 3: 使用 tick() 控制时序

```cpp
TEST_CASE("State Machine - Sequence Test", "[fsm]") {
    ch::ch_device<TestFSM> fsm_dev;
    Simulator sim(fsm_dev.context());
    auto& io = fsm_dev.instance().io();
    
    // 初始状态
    sim.set_input_value(io.start, 0);
    sim.set_input_value(io.reset, 1);
    sim.tick();  // 复位
    
    // 开始状态机
    sim.set_input_value(io.reset, 0);
    sim.set_input_value(io.start, 1);
    sim.tick();  // 启动
    
    // 等待完成
    int max_ticks = 100;
    for (int i = 0; i < max_ticks; i++) {
        sim.tick();
        auto done = sim.get_input_value(io.done);
        if (static_cast<uint64_t>(done) == 1) {
            break;
        }
    }
    
    // 验证完成状态
    REQUIRE(sim.get_input_value(io.done) == 1);
}
```

---

## ⚠️ 注意事项

### 1. 初始化问题

```cpp
// ✅ 推荐：使用 ch_device（自动 build）
ch::ch_device<MyComponent> dev;
Simulator sim(dev.context());

// ⚠️ 谨慎：手动创建 Component
MyComponent comp(nullptr, "comp");
comp.build();  // 必须显式调用
Simulator sim2(comp.context());
```

---

### 2. Context 管理

```cpp
// ✅ 推荐：ch_device 自动管理 context
ch::ch_device<MyComponent> dev;  // 自动创建 top_ctx

// ✅ 推荐：显式 context（需要 ctx_swap）
auto ctx = std::make_unique<ch::core::context>("test");
ch::core::ctx_swap ctx_guard(ctx.get());
```

---

### 3. 断言选择

| 断言 | 用途 | 失败行为 |
|------|------|---------|
| `REQUIRE(condition)` | 关键验证 | 立即终止测试 |
| `CHECK(condition)` | 次要验证 | 继续执行测试 |
| `STATIC_REQUIRE(condition)` | 编译时验证 | 编译失败 |

**最佳实践**:
```cpp
REQUIRE(output == expected);  // 功能正确性
CHECK(warning_count == 0);    // 代码质量
```

---

## 🔍 故障排除

### 问题 1: 测试崩溃（Segmentation Fault）

**症状**: 测试运行时崩溃

**常见原因**:
- 未调用 `comp.build()`
- Context 未正确切换
- Simulator 未初始化

**解决**:
```cpp
// ✅ 使用 ch_device
ch::ch_device<MyComponent> dev;  // 自动 build()
Simulator sim(dev.context());    // 正确初始化
```

---

### 问题 2: 断言失败，值始终为 0

**症状**: `REQUIRE(value != 0)` 失败

**常见原因**:
- 未调用 `sim.tick()`
- 输入值未正确设置
- 信号未连接

**解决**:
```cpp
sim.set_input_value(signal, value);
sim.tick();  // ✅ 确保仿真运行
auto val = sim.get_input_value(signal);
REQUIRE(val != 0);
```

---

### 问题 3: Bundle 字段访问失败

**症状**: `sim.get_input_value(bundle.field)` 失败

**解决**: 参考 [Simulator API 使用指南](04-simulator-api.md) 的注意事项章节

---

## 📊 测试运行

### 运行所有测试

```bash
cd build
ctest  # 运行所有测试
```

### 按标签运行

```bash
# 只运行基础测试
ctest -L base

# 只运行 chlib 测试
ctest -L chlib

# 运行特定标签
ctest -L fifo
ctest -L axi
```

### 运行单个测试

```bash
# 运行特定测试文件
./tests/test_fifo

# 运行特定 SECTION
./tests/test_fifo "[fifo][write]"
```

### 详细输出

```bash
# 输出失败的测试详情
ctest --output-on-failure

# 详细模式
ctest -V

# 只运行失败的测试
ctest --rerun-failed
```

---

## 📖 参考资源

### 相关文档

| 文档 | 说明 |
|------|------|
| [04-simulator-api.md](04-simulator-api.md) | Simulator API 使用 |
| [06-bundle-patterns.md](06-bundle-patterns.md) | Bundle 使用模式 |

### 示例代码

| 示例 | 位置 |
|------|------|
| FIFO 测试 | `tests/test_fifo.cpp` |
| Stream 测试 | `tests/test_stream*.cpp` |
| 状态机测试 | `tests/test_state_machine.cpp` |
| AXI 测试 | `tests/test_axi*.cpp` |

### 外部资源

- [Catch2 文档](https://github.com/catchorg/Catch2/blob/devel/docs/tutorial.md)
- [CTest 文档](https://cmake.org/cmake/help/latest/manual/ctest.1.html)

---

**维护**: AI Agent  
**版本**: v2.0  
**下次审查**: 2026-05-09
