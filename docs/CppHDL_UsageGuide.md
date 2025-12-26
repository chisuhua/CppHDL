# CppHDL 使用指南

CppHDL是一个基于C++的硬件描述语言框架，旨在提供一种现代化、类型安全且高效的方式来描述数字电路。它利用C++的模板元编程和运算符重载等特性，使硬件设计更加直观和可维护。

## 目录
1. [简介](#简介)
2. [安装与构建](#安装与构建)
3. [基本概念](#基本概念)
4. [快速入门](#快速入门)
5. [组合逻辑设计](#组合逻辑设计)
6. [时序逻辑设计](#时序逻辑设计)
7. [运行时循环设计](#运行时循环设计)
8. [运算符操作](#运算符操作)
9. [测试与仿真](#测试与仿真)
10. [代码生成](#代码生成)
11. [最佳实践](#最佳实践)
12. [示例参考](#示例参考)

## 简介

CppHDL将C++作为硬件描述语言，允许设计者使用熟悉的C++语法来描述数字电路。与传统HDL相比，CppHDL具有以下优势：

- **类型安全**：利用C++的强类型系统，减少设计错误
- **现代C++特性**：支持模板、lambda等现代C++特性
- **可组合性**：易于构建复杂的模块化设计
- **可测试性**：直接利用C++测试框架进行单元测试
- **性能**：编译后的仿真器通常比解释型HDL仿真器更快

## 安装与构建

### 依赖
- C++20或更高版本的编译器
- CMake 3.14+
- 可选：Doxygen（用于生成文档）

### 构建步骤
1. 克隆仓库
   ```bash
   git clone <repository_url>
   cd CppHDL
   ```
2. 创建构建目录
   ```bash
   mkdir build && cd build
   ```
3. 配置项目
   ```bash
   cmake ..
   ```
4. 编译项目
   ```bash
   make
   ```

## 基本概念

### 信号类型
CppHDL中主要有以下几种信号类型：
- `ch_uint<N>`: N位无符号整数类型
- `ch_bool`: 布尔类型（1位）
- `ch_reg<T>`: 寄存器类型，用于时序逻辑

### 字面量
CppHDL提供多种字面量后缀用于创建不同类型的值：
- `_d`: 十进制字面量
- `_b`: 二进制字面量
- `_h`: 十六进制字面量
- `_o`: 八进制字面量

### 端口
- `ch_in<T>`: 输入端口
- `ch_out<T>`: 输出端口
- 使用`__io()`宏定义端口结构

### 组件（Component）
硬件设计的基本构建块，包含输入、输出和内部逻辑。

## 快速入门

以下是一个简单的例子，展示如何使用CppHDL创建一个4位加法器：

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
ch_uint<8> a(12_d); // 0b00001100
ch_uint<8> b(5_d);  // 0b00000101

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


## 运算符操作

CppHDL提供了丰富的运算符操作，用于构建复杂的硬件逻辑。

### 位选择操作 (bit_select)

`bit_select`用于从位向量中选择特定位置的位：

```cpp
ch_uint<8> data(0b10110101_d);
ch_bool bit2 = bit_select(data, 2);  // 选择第2位，结果为true(1)
```

### 条件选择操作 (select)

`select`用于根据条件选择两个值中的一个：

```cpp
ch_bool condition(true);
ch_uint<8> true_val(10_d);
ch_uint<8> false_val(20_d);

ch_uint<8> result = select(condition, true_val, false_val);  // 结果为10
```

### 逻辑运算符

CppHDL支持标准的逻辑运算符：

```cpp
ch_bool a(true);
ch_bool b(false);

ch_bool and_result = a && b;  // 逻辑AND
ch_bool or_result = a || b;   // 逻辑OR
ch_bool not_result = !a;      // 逻辑NOT
```

### 比较运算符

CppHDL支持标准的比较运算符：

```cpp
ch_uint<8> a(10_d);
ch_uint<8> b(20_d);

ch_bool eq = (a == b);  // 等于
ch_bool ne = (a != b);  // 不等于
ch_bool gt = (a > b);   // 大于
ch_bool ge = (a >= b);  // 大于等于
ch_bool lt = (a < b);   // 小于
ch_bool le = (a <= b);  // 小于等于
```

### 位运算符

CppHDL支持标准的位运算符：

```cpp
ch_uint<8> a(0b1100_d);
ch_uint<8> b(0b1010_d);

ch_uint<8> and_result = a & b;  // 位AND
ch_uint<8> or_result = a | b;   // 位OR
ch_uint<8> xor_result = a ^ b;  // 位XOR
ch_uint<8> not_result = ~a;     // 位NOT
```

### 移位运算符

CppHDL支持左移和右移运算符：

```cpp
ch_uint<8> value(0b00001100_d);  // 12

ch_uint<8> left_shifted = value << 2_d;  // 左移2位: 0b00110000 (48)
ch_uint<8> right_shifted = value >> 1_d; // 右移1位: 0b00000110 (6)
```

### 函数式用法

CppHDL支持函数式编程风格，允许将硬件操作封装为函数：

```cpp
// 定义一个函数来实现某种硬件功能
template <unsigned N>
ch_uint<N> reverse_bits(ch_uint<N> input) {
    ch_uint<N> result = 0_d;
    
    for (unsigned i = 0; i < N; ++i) {
        ch_bool bit_at_i = bit_select(input, i);
        ch_uint<N> shifted_bit = ch_uint<N>(bit_at_i) << (N - 1 - i)_d;
        result = result | shifted_bit;
    }
    
    return result;
}

// 在模块中使用
class BitReverser : public ch::Component {
public:
    __io(
        ch_in<ch_uint<8>> input;
        ch_out<ch_uint<8>> output;
    )

    BitReverser(ch::Component *parent = nullptr, const std::string &name = "bit_reverser")
        : ch::Component(parent, name) {}

    void create_ports() override { new (io_storage_) io_type; }

    void describe() override {
        io().output = reverse_bits<8>(io().input);
    }
};
```

### 子模块用法

CppHDL允许将复杂功能封装为子模块并复用：

```cpp
class MyTopModule : public ch::Component {
public:
    __io(
        ch_in<ch_uint<4>> a, b;
        ch_out<ch_uint<4>> result;
    )

    MyTopModule(ch::Component *parent = nullptr, const std::string &name = "top")
        : ch::Component(parent, name) {}

    void create_ports() override { new (io_storage_) io_type; }

    void describe() override {
        // 创建子模块实例
        CH_MODULE(Adder4, adder1);
        CH_MODULE(Counter<4>, counter1);
        
        // 连接子模块
        adder1.io().a = io().a;
        adder1.io().b = io().b;
        
        // 使用子模块的输出
        io().result = adder1.io().sum;
    }
};
```

### 使用运行时for循环实现OneHot解码器

```cpp
void describe() override {
    if constexpr (N == 1) {
        io().out = ch_uint<OUTPUT_WIDTH>(0_d);
    } else {
        ch_uint<OUTPUT_WIDTH> result = 0_d;

        // 循环遍历每一位
        for (unsigned i = 0; i < N; ++i) {
            // 创建一个位选择操作
            ch_bool bit_at_i = bit_select(io().in, i);

            // 创建当前索引的字面量值
            ch_uint<OUTPUT_WIDTH> current_value = make_literal(i);

            // 使用select操作创建新的result节点
            result = select(bit_at_i, current_value, result);
        }

        io().out = result;
    }
}
```

### 使用运行时for循环实现OneHot编码器

```cpp
void describe() override {
    ch_uint<N> result = 0_d;

    for (unsigned i = 0; i < N; ++i) {
        ch_bool idx_matches = (ch_uint<INPUT_WIDTH>(io().in.impl()) ==
                               make_uint<INPUT_WIDTH>(i));
        ch_uint<N> one_hot_val = make_literal<1, N>()
                                 << make_uint<INPUT_WIDTH>(i);
        result = select(idx_matches, one_hot_val, result);
    }

    io().out = result;
}
```

### 使用static_assert进行编译时检查

```cpp
template <unsigned N> class onehot_dec {
public:
    static_assert(N > 0, "OneHotDecoder must have at least 1 bit");
    static constexpr unsigned OUTPUT_WIDTH =
        (N > 1) ? compute_bit_width(N - 1) : 1;
    // ...
};
```

### 使用if constexpr进行编译时条件分支

```cpp
void describe() override {
    if constexpr (N == 1) {
        io().out = ch_uint<OUTPUT_WIDTH>(0_d);
    } else {
        // 复杂逻辑
        ch_uint<OUTPUT_WIDTH> result = 0_d;
        // ...
    }
}
```

## 测试与仿真

CppHDL提供了完整的测试框架，可以使用Catch2测试框架进行验证：

### 基本测试
```cpp
#include "catch_amalgamated.hpp"
#include "ch.hpp"
#include "component.h"
#include "core/uint.h"
#include "core/literal.h"
#include "simulator.h"

using namespace ch::core;

class TestAdder : public ch::Component {
public:
    __io(
        ch_in<ch_uint<4>> a, b;
        ch_out<ch_uint<4>> sum;
    )

    TestAdder(ch::Component *parent = nullptr, const std::string &name = "test_adder")
        : ch::Component(parent, name) {}

    void create_ports() override { new (io_storage_) io_type; }

    void describe() override {
        io().sum = io().a + io().b;
    }
};

TEST_CASE("Adder4: basic functionality", "[adder][basic]") {
    auto ctx = std::make_unique<ch::core::context>("test_ctx");
    ch::core::ctx_swap ctx_swapper(ctx.get());

    TestAdder adder;
    adder.create_ports();
    adder.describe();

    ch::Simulator sim(ctx.get());

    // 设置输入并运行仿真
    adder.io().a = 5_d;
    adder.io().b = 3_d;
    sim.tick();

    // 验证输出
    REQUIRE(simulator.get_value(adder.io().sum) == 8);
}
```

### 时序逻辑测试
```cpp
TEST_CASE("Counter: basic functionality", "[counter][timing]") {
    auto ctx = std::make_unique<ch::core::context>("test_ctx");
    ch::core::ctx_swap ctx_swapper(ctx.get());

    Counter<4> counter;
    counter.create_ports();
    counter.describe();

    ch::Simulator sim(ctx.get());

    for (int i = 0; i < 10; ++i) {
        sim.tick();
        // 检查计数值
        REQUIRE(simulator.get_value(counter.io().out) == i % 16);
    }
}
```

## 代码生成

CppHDL支持将设计转换为Verilog代码：

```cpp
#include "ch.hpp"
#include "codegen_verilog.h"
#include "component.h"

using namespace ch;

int main() {
    // 创建组件实例
    Adder4 adder;
    adder.create_ports();
    adder.describe();

    // 生成Verilog代码
    ch::toVerilog("adder4.v", adder.context());

    return 0;
}
```

## 最佳实践

1. **组件设计**：
   - 继承`ch::Component`类定义硬件模块
   - 使用`__io()`宏声明输入输出端口
   - 在`describe()`方法中实现连接逻辑

2. **命名规范**：
   - 使用有意义的变量名和模块名
   - 遵循一致的命名约定（如小写字母+下划线）

3. **字面量使用**：
   - 用`_d`后缀表示十进制字面量
   - 用`_b`后缀表示二进制字面量
   - 用`_h`后缀表示十六进制字面量

4. **寄存器使用**：
   - 用`ch_reg<T>`创建寄存器
   - 通过`->next`设置下一个时钟周期的值

5. **运行时循环使用**：
   - 当需要对多个位进行相似操作时，使用运行时for循环
   - 在循环中使用bit_select来访问特定位
   - 使用select操作来实现条件选择逻辑
   - 结合if constexpr进行编译时优化

6. **运算符使用**：
   - 使用bit_select访问特定位
   - 使用select实现条件选择
   - 使用标准算术、位运算和比较运算符
   - 遵循移位操作规范（字面量作为右操作数）

7. **函数式设计**：
   - 将可复用的硬件功能封装为函数
   - 使用模板参数实现参数化功能
   - 避免创建不必要的组件实例

8. **模块化设计**：
   - 使用CH_MODULE宏创建子模块
   - 通过端口连接模块间信号
   - 保持模块功能单一且可复用

9. **编译时检查**：
   - 使用static_assert进行编译时验证
   - 使用if constexpr进行编译时分支
   - 确保类型安全和参数有效性

10. **测试覆盖**：
    - 为每个模块编写全面的单元测试
    - 使用Catch2框架进行测试验证

## 示例参考

CppHDL仓库中的samples目录包含以下示例：

- `counter.cpp` - 计数器实现
- `simple_io.cpp` - 简单IO端口使用
- `bundle_demo.cpp` - Bundle的使用示例
- `fifo.cpp` - FIFO设计示例

tests目录包含单元测试，用于验证各种功能：

- `test_basic.cpp` - 基本功能测试
- `test_operator.cpp` - 操作符功能测试
- `test_operator_advanced.cpp` - 高级操作符功能测试
- `test_reg.cpp` - 寄存器功能测试
- `test_bundle.cpp` - Bundle功能测试
- `test_literal.cpp` - 字面量功能测试

这些示例和测试代码展示了CppHDL的各种用法，从基本的信号操作到复杂的模块设计，是学习CppHDL的重要参考资料。