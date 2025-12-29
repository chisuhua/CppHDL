# CppHDL AI辅助编码规则和提示词指南

## 概述

本文档为AI助手提供编码规范和提示词编写指南，确保AI在辅助开发CppHDL项目时遵循正确的实践和规范。

## CppHDL核心编码规范

### 1. 模板函数使用规范
- 对于位操作函数如[zext](file:///mnt/ubuntu/chisuhua/github/CppHDL/include/core/lnodeimpl.h#L65-L65)、[bits](file:///mnt/ubuntu/chisuhua/github/CppHDL/include/core/operators.h#L538-L546)等，必须使用正确的模板参数语法
- 正确用法：`zext<NewWidth>(operand)` 或 `bits<MSB, LSB>(operand)`
- 避免虚构不存在的方法，如`ch_uint`的`slice`方法

### 2. 字面量构造规范
- 优先使用`make_literal(value, width)`工厂函数创建指定宽度的运行时字面量信号
- 禁止直接使用`ch_literal_runtime`构造函数
- 使用字面量后缀：`_d`(十进制)、`_b`(二进制)、`_h`(十六进制)、`_o`(八进制)

### 3. 位操作规范
- 使用`bits<MSB, LSB>()`提取指定位段
- 使用`zext<NewWidth>()`进行零扩展
- 使用`sext<NewWidth>()`进行符号扩展
- 使用`bit_select(data, index)`访问特定位

### 4. 组件生命周期规范
- `create_ports()`和`describe()`方法由框架自动调用，禁止在用户代码中手动调用
- 使用`ctx->get_default_clk()`和`ctx->get_default_rst()`获取默认时钟和复位信号
- 使用`simulator.tick()`驱动仿真时钟

### 5. 运行时循环设计规范
- 当需要对多个位进行相似操作时，使用运行时for循环
- 在循环中使用`bit_select`访问特定位
- 使用`select`操作实现条件选择逻辑
- 结合`if constexpr`进行编译时优化

## AI助手行为规范

### 1. 代码生成规范
- 生成的代码必须遵循CppHDL的API规范
- 优先使用自由函数而非虚构的成员方法
- 确保模板参数的正确使用

### 2. 错误诊断规范
- 识别CppHDL特有的编译错误，如模板参数推导失败
- 提供符合CppHDL架构的修复建议
- 避免建议使用不存在的API或方法

### 3. 文档引用规范
- 在提供CppHDL相关建议时，引用[CppHDL_UsageGuide.md](file:///mnt/ubuntu/chisuhua/github/CppHDL/docs/CppHDL_UsageGuide.md)中的相关内容
- 确保所提建议与文档中的最佳实践一致

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

## 检查清单

在AI辅助编码时，请确保：
- [ ] 所有代码遵循CppHDL_UsageGuide.md中的规范
- [ ] 模板函数使用正确的语法
- [ ] 位操作使用标准自由函数
- [ ] 组件生命周期方法使用正确
- [ ] 字面量构造使用工厂函数
- [ ] 代码风格与项目保持一致
- [ ] 包含了适当的错误处理和验证