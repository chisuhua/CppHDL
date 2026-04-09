# cpphdl-type-system-patterns - CppHDL 类型系统模式

**版本**: v1.0
**创建时间**: 2026-04-02
**用途**: 指导 CppHDL 框架中的类型系统正确使用

---

## 触发条件

当在 CppHDL 框架中进行硬件描述时触发，尤其是：
- 需要定义组件输入输出端口（`ch_in<T>`, `ch_out<T>`）
- 需要进行逻辑运算（`ch_bool` 的 `select()` vs `&&`）
- 需要位域提取（`bits<>()` 操作符）
- 需要类型扩展（`sext<>()`, `zext<>()`）
- 遇到类型不匹配编译错误

---

## 使用方法

### 步骤 1: 端口类型定义

CppHDL 使用模板化端口类型进行硬件连接：

```cpp
#include "core/core.h"

namespace ch = ::cpphdl::core;

struct MyComponent : ch::component<MyComponent> {
    // 输入端口
    ch::ch_in<ch::u32> data_in;      // 32 位无符号输入
    ch::ch_in<ch::s16> signed_in;    // 16 位有符号输入
    ch::ch_in<ch_bool> enable;       // 单比特使能
    ch::ch_in<ch_bool> clock;        // 时钟
    ch::ch_in<ch_bool> reset;        // 复位
    
    // 输出端口
    ch::ch_out<ch::u32> data_out;    // 32 位无符号输出
    ch::ch_out<ch_bool> valid;       // 有效信号
    
    void operator()() {
        // 实现逻辑
    }
};
```

### 步骤 2: 类型系统基础

CppHDL 预定义类型列表：

| 类型别名 | 实际类型 | 位宽 | 用途 |
|---------|---------|------|------|
| `ch::u1` / `ch_bool` | `ch::dt::u<1>` | 1 位 | 逻辑信号 |
| `ch::u8` | `ch::dt::u<8>` | 8 位 | 字节数据 |
| `ch::u16` | `ch::dt::u<16>` | 16 位 | 半字数据 |
| `ch::u32` | `ch::dt::u<32>` | 32 位 | 字数据 |
| `ch::u64` | `ch::dt::u<64>` | 64 位 | 双字数据 |
| `ch::s8` | `ch::dt::s<8>` | 8 位 | 有符号字节 |
| `ch::s16` | `ch::dt::s<16>` | 16 位 | 有符号半字 |
| `ch::s32` | `ch::dt::s<32>` | 32 位 | 有符号字 |
| `ch::s64` | `ch::dt::s<64>` | 64 位 | 有符号双字 |

### 步骤 3: 位宽推导

```cpp
// ch_width_v<T> 用于获取类型的位宽
template <typename T>
inline constexpr std::size_t ch_width_v = ch::detail::width_v<T>;

// 示例
static_assert(ch_width_v<ch::u8> == 8);
static_assert(ch_width_v<ch::s32> == 32);

// 在模板元编程中使用
template <typename T>
struct MyGenComponent : ch::component<MyGenComponent<T>> {
    static constexpr auto WIDTH = ch_width_v<T>;
    
    ch::ch_in<T> input;
    ch::ch_out<T> output;
    
    void operator()() {
        // 使用 WIDTH 进行编译期计算
        if constexpr (WIDTH >= 32) {
            // 处理宽类型
        } else {
            // 处理窄类型
        }
    }
};
```

---

## 技术细节

### `ch_in<T>` / `ch_out<T>` 端口访问模式

```cpp
// 基本用法
struct Example : ch::component<Example> {
    ch::ch_in<ch::u32> data;
    ch::ch_out<ch::u32> result;
    
    void operator()() {
        // 读取输入（自动解引用）
        ch::u32 value = data;
        
        // 写入输出（自动赋值）
        result = value + 1;
    }
};

// 组合逻辑示例
struct ALU : ch::component<ALU> {
    ch::ch_in<ch::u32> a;
    ch::ch_in<ch::u32> b;
    ch::ch_in<ch::u2> op;  // 2 位操作码
    ch::ch_out<ch::u32> result;
    
    void operator()() {
        using namespace ch::literals;
        
        result = ch::select(op == 0_u2, a + b,  // ADD
               ch::select(op == 1_u2, a - b,    // SUB
               ch::select(op == 2_u2, a & b,    // AND
               ch::select(op == 3_u2, a | b, a & b))));  // OR, default=AND
    }
};
```

### `ch_bool` 逻辑运算

```cpp
// ❌ 错误：使用 C++ 内置逻辑运算符
ch_bool a, b, c;
ch_bool wrong = a && b;        // 编译错误！
ch_bool also_wrong = a || b;   // 编译错误！
ch_bool not_work = !a;         // 编译错误！

// ✅ 正确：使用 CppHDL 逻辑操作符
ch_bool a, b, c;
ch_bool correct_and = ch::logic_and(a, b);
ch_bool correct_or = ch::logic_or(a, b);
ch_bool correct_not = ch::logic_not(a);

// ✅ 推荐：使用位运算符（已重载）
ch_bool bitwise_and = a & b;
ch_bool bitwise_or = a | b;
ch_bool bitwise_not = ~a;

// ✅ 推荐：使用 select() 条件选择
ch::u32 x, y, result;
ch_bool cond;

// 条件赋值（多路选择器）
result = ch::select(cond, x, y);  // cond ? x : y

// 嵌套 select（多路选择）
result = ch::select(op == 0_u2, a + b,
         ch::select(op == 1_u2, a - b,
         ch::select(op == 2_u2, a & b,
                    a | b)));
```

### `bits<>()` 位域提取操作符

```cpp
// 基本语法：bits<HIGH, LOW>(signal)
// 返回：ch::u<HIGH - LOW + 1> 类型

#include "core/operators.h"

struct BitExtractExample : ch::component<BitExtractExample> {
    ch::ch_in<ch::u32> input;
    ch::ch_out<ch::u8> byte0;
    ch::ch_out<ch::u8> byte1;
    ch::ch_out<ch::u8> byte2;
    ch::ch_out<ch::u8> byte3;
    
    void operator()() {
        using namespace ch::literals;
        
        // 提取 4 个字节
        byte0 = bits<7, 0>(input);    // 最低字节
        byte1 = bits<15, 8>(input);
        byte2 = bits<23, 16>(input);
        byte3 = bits<31, 24>(input);  // 最高字节
        
        // 提取单个位
        ch_bool sign_bit = bits<31, 31>(input);
        
        // 提取半字
        ch::u16 low_half = bits<15, 0>(input);
        ch::u16 high_half = bits<31, 16>(input);
    }
};

// 注意事项：
// 1. HIGH 必须 >= LOW（编译期检查）
// 2. 索引从 0 开始（0 = LSB）
// 3. 返回值类型自动推断
```

### `sext<>()` / `zext<>()` 符号/零扩展

```cpp
#include "core/operators.h"

struct ExtendExample : ch::component<ExtendExample> {
    ch::ch_in<ch::u16> unsigned_in;
    ch::ch_in<ch::s16> signed_in;
    ch::ch_out<ch::u32> unsigned_out;
    ch::ch_out<ch::s32> signed_out;
    
    void operator()() {
        using namespace ch::literals;
        
        // 零扩展：高位补 0
        unsigned_out = zext<32>(unsigned_in);
        // 等价于：unsigned_out = {16'b0, unsigned_in}
        
        // 符号扩展：复制符号位
        signed_out = sext<32>(signed_in);
        // 等价于：signed_out = {{16{signed_in[15]}}, signed_in}
        
        // 常见用例：立即数扩展
        ch::u12 imm12 = bits<11, 0>(instruction);
        
        // 有符号立即数（如分支偏移）
        ch::s32 signed_imm = sext<32>(imm12);
        
        // 无符号立即数（如位掩码）
        ch::u32 unsigned_imm = zext<32>(imm12);
    }
};

// 跨类型运算时的扩展
struct AddMixed : ch::component<AddMixed> {
    ch::ch_in<ch::u8> small;
    ch::ch_in<ch::s16> medium;
    ch::ch_in<ch::s32> large;
    ch::ch_out<ch::s32> result;
    
    void operator()() {
        // 所有操作数扩展到相同位宽
        ch::s32 small_ext = sext<32>(small);
        ch::s32 medium_ext = sext<32>(medium);
        
        result = small_ext + medium_ext + large;
    }
};
```

### `cat()` 位拼接操作符

```cpp
#include "core/operators.h"

struct ConcatExample : ch::component<ConcatExample> {
    ch::ch_in<ch::u16> high;
    ch::ch_in<ch::u16> low;
    ch::ch_out<ch::u32> result;
    
    void operator()() {
        // 拼接两个 16 位为 32 位
        result = cat(high, low);  // {high, low}
        
        // 拼接多个信号
        ch::u1 a, b, c, d;
        ch::u4 nibble = cat(a, b, c, d);  // {a, b, c, d}
        
        // 与字面量拼接
        using namespace ch::literals;
        ch::u5 with_zero = cat(bits<3, 0>(result), 0_b);  // 左移 1 位
    }
};
```

---

## 示例代码

### 完整示例：移位寄存器

```cpp
// shift_register.h
#pragma once
#include "core/core.h"
#include "core/operators.h"

namespace ch = ::cpphdl::core;

struct ShiftRegister : ch::component<ShiftRegister> {
    // 端口定义
    ch::ch_in<ch::u8> data_in;
    ch::ch_in<ch::u2> op;       // 0=hold, 1=load, 2=shift_left, 3=shift_right
    ch::ch_in<ch_bool> enable;
    ch::ch_in<ch_bool> clock;
    ch::ch_in<ch_bool> reset;
    
    ch::ch_out<ch::u8> data_out;
    
    // 内部状态
    ch::u8 register_value;
    
    void operator()() {
        using namespace ch::literals;
        
        // 同步复位
        if (reset) {
            register_value = 0_u8;
        }
        // 使能信号
        else if (enable) {
            // 操作选择
            register_value = ch::select(
                op == 0_u2, register_value,      // HOLD
                ch::select(
                    op == 1_u2, data_in,         // LOAD
                    ch::select(
                        op == 2_u2,              // SHIFT LEFT
                        cat(bits<6, 0>(register_value), 0_b),
                        ch::select(
                            op == 3_u2,          // SHIFT RIGHT
                            cat(0_b, bits<7, 1>(register_value)),
                            register_value       // default
                        )
                    )
                )
            );
        }
        
        // 输出
        data_out = register_value;
    }
};
```

### 示例：优先编码器

```cpp
// priority_encoder.h
#pragma once
#include "core/core.h"
#include "core/operators.h"

namespace ch = ::cpphdl::core;

struct PriorityEncoder : ch::component<PriorityEncoder> {
    ch::ch_in<ch::u8> input;
    ch::ch_out<ch::u3> encoded;
    ch::ch_out<ch_bool> valid;
    
    void operator()() {
        using namespace ch::literals;
        
        // 优先编码：最高位优先
        encoded = ch::select(bits<7, 7>(input), 7_u3,
                  ch::select(bits<6, 6>(input), 6_u3,
                  ch::select(bits<5, 5>(input), 5_u3,
                  ch::select(bits<4, 4>(input), 4_u3,
                  ch::select(bits<3, 3>(input), 3_u3,
                  ch::select(bits<2, 2>(input), 2_u3,
                  ch::select(bits<1, 1>(input), 1_u3,
                  ch::select(bits<0, 0>(input), 0_u3,
                             0_u3))))))));
        
        // 有效信号：任一输入为高
        valid = bits<7, 0>(input) != 0_u8;
    }
};
```

---

## 常见错误

### 错误 1: 使用 C++ 逻辑运算符

```cpp
// ❌ 错误
ch_bool a, b;
ch::u32 result;

if (a && b) {  // 编译错误！
    result = 1;
}

// ✅ 正确
ch_bool cond = ch::logic_and(a, b);  // 或 a & b
result = ch::select(cond, 1_u32, 0_u32);
```

### 错误 2: 类型不匹配未扩展

```cpp
// ❌ 错误：16 位与 32 位直接运算
ch::ch_in<ch::u16> small;
ch::ch_in<ch::u32> large;
ch::ch_out<ch::u32> result;

void operator()() {
    result = small + large;  // 类型不匹配！
}

// ✅ 正确
void operator()() {
    ch::u32 small_ext = zext<32>(small);
    result = small_ext + large;
}
```

### 错误 3: `bits<>()` 索引反向

```cpp
// ❌ 错误
ch::u32 data;
auto wrong = bits<0, 7>(data);  // 编译错误：HIGH < LOW

// ✅ 正确
auto correct = bits<7, 0>(data);
```

### 错误 4: `select()` 类型不一致

```cpp
// ❌ 错误：then 和 else 类型不同
ch_bool cond;
ch::u16 a;
ch::u32 b;
auto wrong = ch::select(cond, a, b);  // 类型不匹配！

// ✅ 正确：扩展到相同类型
auto correct = ch::select(cond, zext<32>(a), b);
```

### 错误 5: 忘记时钟/复位逻辑

```cpp
// ❌ 错误：缺少时序逻辑
struct BadFF : ch::component<BadFF> {
    ch::ch_in<ch::u32> d;
    ch::ch_in<ch_bool> clock;
    ch::ch_out<ch::u32> q;
    
    void operator()() {
        q = d;  // 组合逻辑，不是触发器！
    }
};

// ✅ 正确：添加时钟和复位
struct GoodFF : ch::component<GoodFF> {
    ch::ch_in<ch::u32> d;
    ch::ch_in<ch_bool> clock;
    ch::ch_in<ch_bool> reset;
    ch::ch_out<ch::u32> q;
    
    ch::u32 state;
    
    void operator()() {
        if (reset) {
            state = 0_u32;
        } else if (clock.posedge()) {  // 或使用 clock && posedge 检测
            state = d;
        }
        q = state;
    }
};
```

### 错误 6: 位移操作类型错误

```cpp
// ❌ 错误：位移量>=类型位宽
ch::u32 x;
auto wrong = x << 32;  // 未定义行为

// ✅ 正确：检查边界
auto safe = ch::select(shift >= 32, 0_u32, x << shift);

// 或使用 64 位中间类型
auto safe = (static_cast<ch::u64>(x) << shift) & 0xFFFFFFFF;
```

---

## 参考文档

- CppHDL 核心文档：`/workspace/CppHDL/docs/component_design.md`
- 操作符参考：`/workspace/CppHDL/include/core/operators.h`
- AXI4 示例：`/workspace/CppHDL/include/axi4/axi4_lite_slave.h`
- 类型系统：`/workspace/CppHDL/include/core/literal.h`

---

## 测试

### 单元测试模板

```cpp
// test_type_system.cpp
#include "core/core.h"
#include "core/operators.h"
#include "core/sim.h"
#include <cassert>
#include <type_traits>

// 测试 1: bits<>() 类型推导
void test_bits_type() {
    ch::u32 data = 0x12345678;
    
    auto byte = bits<7, 0>(data);
    static_assert(std::is_same_v<decltype(byte), ch::u8>);
    assert(byte == 0x78);
    
    auto half = bits<15, 0>(data);
    static_assert(std::is_same_v<decltype(half), ch::u16>);
    assert(half == 0x5678);
}

// 测试 2: sext/zext 扩展
void test_extend() {
    ch::u8 unsigned_val = 0x80;
    ch::s8 signed_val = -128;
    
    auto unsigned_ext = zext<32>(unsigned_val);
    assert(unsigned_ext == 0x00000080);
    
    auto signed_ext = sext<32>(signed_val);
    assert(signed_ext == 0xFFFFFF80);
}

// 测试 3: cat() 拼接
void test_concat() {
    ch::u16 high = 0xABCD;
    ch::u16 low = 0xEF01;
    
    auto result = cat(high, low);
    static_assert(std::is_same_v<decltype(result), ch::u32>);
    assert(result == 0xABCD0000 | 0xEF01);  // 注意：实际拼接方式可能不同
}

// 测试 4: select() 类型检查
void test_select() {
    ch_bool cond = true;
    ch::u16 a = 100;
    ch::u16 b = 200;
    
    auto result = ch::select(cond, a, b);
    static_assert(std::is_same_v<decltype(result), ch::u16>);
    assert(result == 100);
}

int main() {
    test_bits_type();
    test_extend();
    test_concat();
    test_select();
    
    printf("All type system tests passed!\n");
    return 0;
}
```

### 编译验证

```bash
# 编译测试
cd /workspace/CppHDL
make test_type_system

# 运行测试
./build/test_type_system
# 预期输出：All type system tests passed!
```

---

**维护者**: DevMate
**许可证**: MIT
