# CppHDL 使用指南

## 概述

CppHDL 是一个基于 C++ 的硬件描述语言工具集，它允许用户使用现代 C++ 语法来设计数字电路。CppHDL 提供了类型安全的硬件构造、编译时结构生成、Verilog 代码生成等功能。

## 快速开始

以下是一个简单的 4 位加法器示例：

```cpp
#include "ch.hpp"
#include "component.h"
#include "core/uint.h"
#include "core/literal.h"

using namespace ch::core;

class Adder4 : public ch::Component {
public:
    __io(
        ch_in<ch_uint<4>> a, b;
        ch_out<ch_uint<4>> sum;
    )

    Adder4(ch::Component *parent = nullptr, const std::string &name = "adder4")
        : ch::Component(parent, name) {}

    void create_ports() override {
        new (io_storage_) io_type;
    }

    void describe() override {
        io().sum = io().a + io().b;
    }
};

int main() {
    Adder4 adder;
    adder.create_ports();
    adder.describe();
    
    return 0;
}
```

## 字面量规范

CppHDL 支持多种进制的字面量表示，使用特定后缀来区分：

### 二进制字面量
- 使用 `_b` 后缀表示二进制字面量
- 例如：`1100_b` 表示二进制 1100（十进制 12）
- 注意：**不要在二进制字面量前添加 `0b` 前缀**，应直接使用 `_b` 后缀

### 十进制字面量
- 使用 `_d` 后缀表示十进制字面量
- 例如：`12_d` 表示十进制 12

### 十六进制字面量
- 使用 `_h` 后缀表示十六进制字面量
- 例如：`0xC_h` 或 `12_d` 都表示十进制 12
- 也可以使用 `0xFF_h` 表示十六进制 FF

### 八进制字面量
- 使用 `_o` 后缀表示八进制字面量
- 例如：`14_o` 表示八进制 14（十进制 12）

## 组合逻辑设计

CppHDL支持丰富的组合逻辑运算：

### 算术运算
```cpp
ch_uint<8> a(12_d);
ch_uint<8> b(5_d);

auto add_result = a + b;
auto sub_result = a - b;
auto mul_result = a * b;
```

### 位运算
```cpp
ch_uint<8> a(12_d); // 0b1100
ch_uint<8> b(5_d);  // 0b0101

auto and_result = a & b;
auto or_result = a | b;
auto xor_result = a ^ b;
auto not_result = ~a;
```

### 比较运算
```cpp
ch_uint<8> a(12_d);
ch_uint<8> b(5_d);
ch_uint<8> c(12_d);

auto eq_result = a == c;  // true
auto ne_result = a != b;  // true
auto gt_result = a > b;   // true
auto ge_result = a >= c;  // true
auto lt_result = b < a;   // true
auto le_result = b <= a;  // true
```

### 移位运算
```cpp
ch_uint<8> a(12_d);

auto shl_result = a << 2_d;  // 左移2位
auto shr_result = a >> 1_d;  // 右移1位
```

## 时序逻辑设计

CppHDL通过寄存器（`ch_reg`）来实现时序逻辑：

### D触发器示例
```cpp
class DFlipFlop : public ch::Component {
public:
    __io(
        ch_in<ch_bool> clk, d;
        ch_out<ch_bool> q;
    )

    DFlipFlop(ch::Component *parent = nullptr, const std::string &name = "dff")
        : ch::Component(parent, name) {}

    void create_ports() override { new (io_storage_) io_type; }

    void describe() override {
        ch_reg<ch_bool> reg(0_b);
        reg->next = io().d;
        io().q = reg;
    }
};
```

### 计数器示例
```cpp
template <unsigned N>
class Counter : public ch::Component {
public:
    __io(
        ch_out<ch_uint<N>> out;
    )

    Counter(ch::Component *parent = nullptr, const std::string &name = "counter")
        : ch::Component(parent, name) {}

    void create_ports() override {
        new (io_storage_) io_type;
    }

    void describe() override {
        ch_reg<ch_uint<N>> reg(0_d);
        reg->next = reg + 1_d;
        io().out = reg;
    }
};
```

### 带复位的计数器
```cpp
class CounterWithReset : public ch::Component {
public:
    __io(
        ch_in<ch_bool> clk, rst;
        ch_out<ch_uint<8>> count;
    )

    CounterWithReset(ch::Component *parent = nullptr, 
                     const std::string &name = "counter_reset")
        : ch::Component(parent, name) {}

    void create_ports() override { new (io_storage_) io_type; }

    void describe() override {
        ch_reg<ch_uint<8>> counter(0_d);
        counter->next = io().rst ? 0_d : (counter + 1_d);
        io().count = counter;
    }
};
```

## 调试和仿真

CppHDL 提供了仿真器来测试硬件设计：

```cpp
#include "simulator.h"
#include "catch_amalgamated.hpp"
#include "core/context.h"

using namespace ch::core;

TEST_CASE("Example: test hardware component", "[example]") {
    auto ctx = std::make_unique<context>("test");
    ctx_swap ctx_swapper(ctx.get());

    // 创建硬件组件
    ch_uint<4> a(12_d);
    ch_uint<4> b(3_d);
    ch_uint<4> result = a + b;

    // 创建仿真器
    ch::Simulator sim(ctx.get());
    sim.tick();  // 执行一个仿真周期

    // 验证结果
    REQUIRE(sim.get_value(result) == 15);
}
```

## 最佳实践

1. **类型安全**：始终使用 `ch_uint<N>` 指定明确的位宽
2. **字面量使用**：使用适当的后缀（`_b`, `_d`, `_h`, `_o`）创建字面量
3. **组件设计**：使用 `__io` 宏定义组件接口
4. **命名空间**：使用 `using namespace ch::core` 以简化代码
5. **上下文管理**：使用 `ctx_swap` 确保正确的上下文环境

## 常见错误

1. **位宽不匹配**：确保字面量的位宽不超过目标 `ch_uint<N>` 的宽度
2. **上下文缺失**：在测试和仿真中始终使用正确的上下文
3. **未初始化寄存器**：为 `ch_reg` 提供初始值
4. **错误的字面量格式**：使用正确的后缀而不是传统的 C 风格前缀
