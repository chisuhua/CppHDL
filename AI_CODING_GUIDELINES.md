# CppHDL AI辅助编码规则和提示词指南

## 概述

本文档为AI助手提供了在CppHDL项目中编写代码的具体规范和最佳实践。遵循这些指南可以确保生成的代码与项目架构、编码风格和设计模式保持一致。

## 通用编码规范

### 1. C++ 语言标准
- 使用 C++20 标准
- 利用现代 C++ 特性，如概念(Concepts)、范围(Ranges)、模块化等
- 遵循 RAII 原则进行资源管理

### 2. 命名约定
- 使用驼峰命名法：变量和函数使用小驼峰 `camelCase`
- 类名使用大驼峰 `PascalCase`
- 常量使用全大写加下划线 `CONSTANT_CASE`

### 3. 代码格式
- 使用 4 个空格缩进，禁用制表符
- 遵循 Google C++ Style Guide
- 行长度不超过 100 个字符
- 使用 clang-format 统一代码风格

## CppHDL 特定规范

### 1. 硬件类型定义
- 使用 `ch_uint<N>` 表示 N 位无符号整数
- 使用 `ch_bool` 表示布尔类型
- 使用 `ch_reg<T>` 表示寄存器类型
- 使用 `ch_in<T>`, `ch_out<T>`, `ch_inout<T>` 表示端口类型

### 2. 字面量规范
- 使用 `_d` 后缀表示十进制字面量，如 `12_d`
- 使用 `_b` 后缀表示二进制字面量，如 `1100_b`（**注意：不要在二进制字面量前添加 `0b` 前缀**）
- 使用 `_h` 后缀表示十六进制字面量，如 `0xC_h`
- 使用 `_o` 后缀表示八进制字面量，如 `14_o`
- 确保字面量的位宽不超过目标 `ch_uint<N>` 的宽度
- 优先使用`make_literal(value, width)`工厂函数创建指定宽度的运行时字面量信号
- 禁止直接使用`ch_literal_runtime`构造函数

### 3. 布尔值使用规范
- **禁止在select语句或其他硬件操作中使用C++内置的`true`/`false`**
- **必须使用CppHDL类型表示法：`1_b`表示真值，`0_b`表示假值**
- **对于ch_bool类型的变量，应使用`ch_bool(1_b)`或`ch_bool(0_b)`创建**

**正确示例**：
```cpp
// 在select语句中使用CppHDL字面量
ch_bool result = select(condition, 1_b, 0_b);

// 初始化ch_bool寄存器
ch_reg<ch_bool> reg(0_b, "name");

// 在条件赋值中使用
reg->next = select(rst, 0_b, signal);
```

**错误示例**：
```cpp
// 不要使用C++内置的true/false
ch_bool result = select(condition, true, false);

// 不要直接使用true/false初始化
ch_reg<ch_bool> reg(false, "name");

// 避免混用类型
reg->next = select(rst, false, signal);
```

### 4. 循环变量规范
- 在循环中创建表示索引或其他运行期值的硬件节点时，应优先使用`make_uint<WIDTH>(i)`而非`make_literal(i)`
- `make_uint`用于创建指定位宽的无符号整数信号，更适合循环变量场景
- `make_literal`主要用于常量值的字面量构造，一般不适用于运行期变化的循环变量
- **例外情况**：当[make_literal(i)](file:///mnt/ubuntu/chisuhua/github/CppHDL/include/core/literal.h#L163-L165)仅在赋值语句中作为右操作数使用时，是允许的例外，如 `ch_uint<N> value = make_literal(i);`
- **禁止用法**：将[make_literal(i)](file:///mnt/ubuntu/chisuhua/github/CppHDL/include/core/literal.h#L163-L165)用于比较操作或作为函数参数传递，如 `if (some_signal == make_literal(i))` 或 `auto value = make_uint<WIDTH>(make_literal(i));`
- 正确用法示例：
  ```cpp
  for (unsigned i = 0; i < N; ++i) {
      ch_uint<WIDTH> current_value = make_uint<WIDTH>(i);
      // ... 其他操作
  }
  ```
- 错误用法示例：
  ```cpp
  for (unsigned i = 0; i < N; ++i) {
      ch_uint<WIDTH> current_value = make_literal(i);  // 不推荐
      // ... 其他操作
  }
  ```

### 5. 组件定义
- 使用 `__io` 宏定义组件接口
- 继承自 `ch::Component` 创建新组件
- 实现 `create_ports()` 和 `describe()` 方法
- 在构造函数中初始化组件名称和父组件

### 6. 操作符使用
- 优先使用框架提供的自由函数而非自定义函数
- 使用 `zext` 进行零扩展，而非 `zero_extend`
- 使用 `bits<MSB, LSB>(value)` 提取位段，而非成员函数调用
- 位宽计算规则：提取WIDTH位从LSB开始，则MSB = LSB + WIDTH - 1

### 7. 仿真与测试
- 使用 `ch::Simulator` 进行仿真
- 使用 `sim.tick()` 驱动时钟，而非手动翻转时钟信号
- 使用 Catch2 框架编写测试用例
- 为 `ch_reg` 提供初始值

### 8. 模板参数使用规范
- **核心原则**：模板参数必须是编译期可确定的常量表达式
- **禁止将运行时变量（如循环变量、函数参数等）作为模板参数使用**
- **所有模板实参必须在编译时具有确定值**

**常见错误示例**：
- 在`make_uint<compute_bit_width(width)>(width)`中，如果[width](file:///mnt/ubuntu/chisuhua/github/CppHDL/include/core/literal.h#L75-L75)是函数参数（运行时值），则`compute_bit_width(width)`无法作为模板参数，因为[width](file:///mnt/ubuntu/chisuhua/github/CppHDL/include/core/literal.h#L75-L75)不是编译期常量
- 在`bits<MSB, LSB>()`、`make_literal<Value, Width>()`等模板中使用运行时变量

**正确实践**：
1. **编译期计算**：对于依赖于变量的位宽或其他类型参数，应先通过constexpr函数或模板在编译期计算出结果
2. **运行时场景**：当参数是运行时值时，应使用运行时函数而非模板函数，如使用循环和字面量操作来处理运行时值
3. **常量提取**：将复杂的表达式简化为编译期常量，避免在模板参数中直接使用算术运算
4. **替代方案**：当需要运行时行为时，考虑使用函数参数而非模板参数，或采用标签分发（tag dispatching）等技术

**设计优势**：
- 确保代码可通过编译，避免"non-type template parameter is not a constant expression"等错误
- 提高类型安全性，保证生成的硬件结构在编译期就已确定
- 符合CppHDL框架对静态类型和编译期优化的要求

### 9. 运行时值处理规范
- **运行时值处理**：当需要处理运行时值（如函数参数）时，不能直接将其用作模板参数
- **替代方法**：使用循环和字面量操作来处理运行时值，而不是依赖模板计算
- **字面量操作**：使用如`1_d`等字面量操作来构建运行时计算逻辑

**示例**：
```cpp
// 错误：试图使用运行时值作为模板参数
ch_uint<N> mask = (ch_uint<N>(1_d) << make_uint<compute_bit_width(width)>(width)) - 1_d;

// 正确：使用循环和字面量操作处理运行时值
ch_uint<N> mask_val = 1_d;
for(unsigned i = 1; i < width; ++i) {
    mask_val = (mask_val << 1_d) + 1_d;
}
```

## 代码结构规范

### 1. 头文件包含顺序
```
1. 对应的头文件（如果适用）
2. C 系统头文件
3. C++ 系统头文件
4. 其他库头文件
5. 项目头文件
```

### 2. 类和函数组织
- 在头文件中声明，在源文件中定义实现
- 将公共接口放在类的开头
- 使用 RAII 模式管理资源
- 避免在头文件中包含不必要的实现细节

## 错误处理和调试

### 1. 断言和检查
- 使用 `CHREQUIRE` 进行前置条件检查
- 使用 `CHASSERT` 进行运行时断言
- 提供有意义的错误消息

### 2. 调试输出
- 使用 `CHDBG` 进行调试输出
- 包含有意义的上下文信息
- 包含类型信息或状态信息以提升诊断效率

## 测试规范

### 1. 测试用例编写
- 使用 Catch2 的 SECTION 机制组织测试
- 测试应覆盖边界情况
- 使用适当的测试名称和标签
- 优先使用公共 API 进行测试，而非内部实现

### 2. 验证方法
- 使用 `REQUIRE` 和 `CHECK` 进行断言
- 使用 `sim.get_value()` 获取仿真值
- 验证硬件行为符合预期

## 性能优化

### 1. 编译时优化
- 利用模板和 constexpr 进行编译时计算
- 避免不必要的运行时开销
- 使用适当的算法和数据结构

### 2. 硬件优化
- 避免创建不必要的中间节点
- 重用已创建的节点以减少重复
- 遵循硬件语义，一个物理连接点对应一个唯一的节点表示

## 文档规范

### 1. 注释风格
- 使用 Doxygen 风格的注释
- 为公共接口提供详细文档
- 解释复杂算法和设计决策

### 2. 代码文档
- 在函数和类定义前添加文档注释
- 说明参数、返回值和异常情况
- 提供使用示例

## AI 特定指导

### 1. 模板使用
- 当不确定时，参考现有实现而非自行发明
- 优先使用框架提供的公共 API
- 避免使用未在项目中验证的 C++ 特性

### 2. 代码生成
- 确保生成的代码符合上述所有规范
- 生成的代码应易于理解和维护
- 提供适当的错误处理和边界检查

### 3. 问题修复
- 修复问题时保持与现有架构一致
- 优先使用框架提供的标准操作符进行测试
- 验证修改后的代码符合预期语义

## 提示词编写模板

### 1. 功能实现提示词模板
```
请根据CppHDL框架规范实现[功能描述]，遵循以下要求：
- 使用正确的CppHDL API和语法
- 遵循自由函数优先原则
- 使用正确的模板参数语法
- 包含必要的头文件
- 遵循CppHDL_UsageGuide.md中的最佳实践
```

### 2. 问题修复提示词模板
```
修复以下CppHDL代码中的错误：
[错误代码或错误信息]

要求：
- 识别错误原因，特别是CppHDL特有的问题
- 提供符合CppHDL规范的修复方案
- 确保修复后的代码遵循文档规范
- 解释修复的原因和影响
```

### 3. 代码优化提示词模板
```
优化以下CppHDL代码以提高性能和可读性：
[待优化代码]

要求：
- 遵循CppHDL_UsageGuide.md中的最佳实践
- 使用更高效的CppHDL API
- 保持代码的可维护性和类型安全性
- 符合函数式编程风格推荐
```

## 常见错误和避免方式

### 1. 模板参数错误
- 错误：`zext<N>(other)`导致的模板参数推导失败
- 正确：确认模板参数顺序和类型，如`zext<ch_uint<M>, N>(other)`

### 2. 函数调用错误
- 错误：使用不存在的成员方法如`ch_uint.slice()`
- 正确：使用自由函数如`bits<MSB, LSB>()`

### 3. 字面量构造错误
- 错误：直接使用构造函数创建字面量
- 正确：使用`make_literal(value, width)`工厂函数

### 4. 布尔值使用错误
- 错误：在select语句中使用C++内置的true/false
- 正确：使用CppHDL字面量如1_b/0_b或ch_bool类型

## 检查清单

在AI辅助编码时，请确保：
- [ ] 所有代码遵循CppHDL_UsageGuide.md中的规范
- [ ] 模板函数使用正确的语法
- [ ] 位操作使用标准自由函数
- [ ] 组件生命周期方法使用正确
- [ ] 字面量构造使用工厂函数
- [ ] 使用CppHDL类型（1_b/0_b）替代C++内置true/false
- [ ] 代码风格与项目保持一致
- [ ] 包含了适当的错误处理和验证