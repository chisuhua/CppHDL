# SpinalHDL Stream 操作符在 CppHDL 中的实现

**文档版本**: 1.0  
**最后更新**: 2026-03-04  
**作者**: CppHDL 开发团队

## 概述

本文档描述了如何在 CppHDL 中实现类似 SpinalHDL 的 Stream 操作符和功能。基于 `docs/SpinalHDL_Stream_Connection_Features_Comparison.md` 中的比较分析，我们实现了 SpinalHDL Stream 的核心功能，同时考虑了 C++ 语言的限制。

## 实现的功能

### 1. 管道原语 (Pipeline Primitives)

实现了三种管道原语，对应 SpinalHDL 的管道操作：

| 功能 | CppHDL 函数 | SpinalHDL 对应 | 描述 |
|------|-------------|----------------|------|
| Master-to-Slave 管道 | `stream_m2s_pipe<T>(input)` | `m2sPipe()` | 1 周期延迟在 valid 和 payload 信号上 |
| Slave-to-Master 管道 | `stream_s2m_pipe<T>(input)` | `s2mPipe()` | 0 周期 payload 延迟，1 周期 ready 延迟 |
| 半带宽管道 | `stream_half_pipe<T>(input)` | `halfPipe()` | 所有信号寄存器化，带宽减半 |

**示例**:
```cpp
// 创建 M2S 管道
auto m2s_result = stream_m2s_pipe<ch_uint<32>>(input_stream);

// 创建 S2M 管道  
auto s2m_result = stream_s2m_pipe<ch_uint<32>>(input_stream);

// 创建半带宽管道
auto half_result = stream_half_pipe<ch_uint<32>>(input_stream);
```

### 2. 成员函数别名 (Member Function Aliases)

为 `ch_stream<T>` 类添加了成员函数别名，提供更流畅的 API：

| 成员函数 | 对应自由函数 | 描述 |
|----------|--------------|------|
| `.m2sPipe()` | `stream_m2s_pipe(*this)` | Master-to-Slave 管道 |
| `.stage()` | `stream_m2s_pipe(*this)` | `.m2sPipe()` 的别名 |
| `.s2mPipe()` | `stream_s2m_pipe(*this)` | Slave-to-Master 管道 |
| `.halfPipe()` | `stream_half_pipe(*this)` | 半带宽管道 |

**示例**:
```cpp
ch_stream<ch_uint<32>> input_stream;

// 使用成员函数
auto m2s_result = input_stream.m2sPipe();
auto stage_result = input_stream.stage();  // 与 m2sPipe() 相同
auto s2m_result = input_stream.s2mPipe();
auto half_result = input_stream.halfPipe();
```

### 3. 连接操作符 (Connection Operators)

由于 C++ 不支持自定义操作符序列（如 `<<=<`），我们实现了替代方案：

| 功能 | CppHDL 实现 | SpinalHDL 对应 | 描述 |
|------|-------------|----------------|------|
| 直接连接 | `sink <<= source` | `sink << source` | 0 周期延迟，组合逻辑 |
| 反向连接 | `source >>= sink` | `source >> sink` | 0 周期延迟，组合逻辑 |
| M2S 管道连接 | `sink <<= m2s_pipe(source).output` | `sink <<=< source` | 1 周期延迟连接 |
| S2M 管道连接 | `sink <<= s2m_pipe(source).output` | `sink <<</< source` | 0 周期数据延迟连接 |
| 完整管道连接 | `sink <<= m2s_pipe(s2m_pipe(source).output).output` | `sink <<=</< source` | M2S + S2M 管道 |

**示例**:
```cpp
ch_stream<ch_uint<32>> source, sink;

// 直接连接
sink <<= source;

// M2S 管道连接（等效于 SpinalHDL 的 sink <<=< source）
sink <<= stream_m2s_pipe(source).output;

// S2M 管道连接（等效于 SpinalHDL 的 sink <<</< source）  
sink <<= stream_s2m_pipe(source).output;

// 完整管道连接（等效于 SpinalHDL 的 sink <<=</< source）
sink <<= stream_m2s_pipe(stream_s2m_pipe(source).output).output;
```

### 4. 流畅 API 构建器 (Fluent API Builder)

实现了 `StreamBuilder<T>` 类，提供链式操作 API：

**可用方法**:
- `.queue<DEPTH>()` - 添加 FIFO 队列（深度必须是编译时常量）
- `.haltWhen(condition)` - 条件暂停
- `.map(function)` - 映射转换
- `.throwWhen(condition)` - 条件抛出
- `.takeWhen(condition)` - 条件获取
- `.continueWhen(condition)` - 条件继续
- `.build()` - 构建最终流

**示例**:
```cpp
ch_stream<ch_uint<32>> input_stream;
ch_bool busy_condition;
auto transform = [](ch_uint<32> x) { return x * 2; };

// 链式操作
auto result = StreamBuilder(input_stream)
    .queue<4>()                    // 添加深度为 4 的 FIFO
    .haltWhen(busy_condition)      // 当 busy 时暂停
    .map(transform)                // 应用转换函数
    .build();                      // 构建最终流
```

### 5. 额外仲裁策略 (Additional Arbitration Strategies)

在现有的轮询仲裁和优先级仲裁基础上，添加了两种新的仲裁策略：

| 仲裁器 | 函数 | 描述 |
|--------|------|------|
| 锁定仲裁器 | `stream_arbiter_lock<T, N>()` | 一旦选中某个输入，保持选中直到事务完成 |
| 顺序仲裁器 | `stream_arbiter_sequence<T, N>()` | 固定顺序仲裁，循环选择 |

**示例**:
```cpp
// 创建锁定仲裁器（2 个输入）
auto lock_arbiter = stream_arbiter_lock<ch_uint<32>, 2>();

// 创建顺序仲裁器（3 个输入）  
auto seq_arbiter = stream_arbiter_sequence<ch_uint<32>, 3>();
```

### 6. 位宽适配器 (Width Adapters)

实现了两种位宽适配器，用于不同位宽流之间的转换：

| 适配器 | 函数 | 描述 |
|--------|------|------|
| 窄到宽适配器 | `stream_narrow_to_wide<TOut, TIn, N>()` | 组合 N 个窄输入节拍为 1 个宽输出节拍 |
| 宽到窄适配器 | `stream_wide_to_narrow<TOut, TIn, N>()` | 拆分 1 个宽输入节拍为 N 个窄输出节拍 |

**示例**:
```cpp
// 8-bit 到 32-bit 适配器（4:1 比例）
auto narrow_to_wide = stream_narrow_to_wide<ch_uint<32>, ch_uint<8>, 4>();

// 32-bit 到 8-bit 适配器（1:4 比例）
auto wide_to_narrow = stream_wide_to_narrow<ch_uint<8>, ch_uint<32>, 4>();
```

## C++ 限制与解决方案

### 1. 自定义操作符序列的限制
**问题**: C++ 不支持自定义操作符序列如 `<<=<`, `<<</<`, `<<=</<`  
**解决方案**: 使用 `sink <<= pipe_result.output` 模式替代

### 2. 模板深度限制
**问题**: FIFO 深度必须是编译时常量  
**解决方案**: 使用模板参数 `.queue<DEPTH>()` 而不是运行时参数

### 3. Lambda 表达式类型推导
**问题**: Lambda 表达式需要明确的返回类型  
**解决方案**: 使用通用模板和 `decltype` 推导

## 文件结构

### 新增头文件
```
include/chlib/
├── stream_pipeline.h      # 管道原语
├── stream_operators.h     # 连接操作符
├── stream_builder.h       # 流畅 API 构建器
├── stream_arbiter.h       # 额外仲裁策略
└── stream_width_adapter.h # 位宽适配器
```

### 修改的头文件
```
include/bundle/
└── stream_bundle.h        # 添加了成员函数别名
```

### 测试文件
```
tests/
├── test_stream_pipeline.cpp      # 管道原语测试
├── test_stream_operators.cpp     # 操作符测试
├── test_stream_builder.cpp       # 流畅 API 测试
├── test_stream_arbiter.cpp       # 仲裁器测试
└── test_stream_width_adapter.cpp # 位宽适配器测试
```

### 示例文件
```
samples/
└── spinalhdl_stream_example.cpp  # 综合示例
```

## 使用示例

完整的示例代码请参考 `samples/spinalhdl_stream_example.cpp`，其中包含：

1. **Stream FIFO 示例** - 对应 SpinalHDL 的 `StreamFifo`
2. **Stream Fork 示例** - 对应 SpinalHDL 的 `StreamFork`
3. **Stream Join 示例** - 对应 SpinalHDL 的 `StreamJoin`
4. **Stream Arbiter 示例** - 对应 SpinalHDL 的 `StreamArbiter`
5. **Stream Mux/Demux 示例** - 对应 SpinalHDL 的 `StreamMux`/`StreamDemux`
6. **管道操作示例** - 展示 M2S、S2M 和 halfPipe 的使用
7. **操作符连接示例** - 展示 `<<=` 操作符的使用
8. **流畅 API 示例** - 展示 `StreamBuilder` 的使用

## 验证状态

所有实现都通过了单元测试：

- ✅ 管道原语测试：31 个断言，6 个测试用例
- ✅ 操作符测试：36 个断言，10 个测试用例  
- ✅ 流畅 API 测试：6 个断言，4 个测试用例
- ✅ 仲裁器测试：9 个断言，5 个测试用例
- ✅ 位宽适配器测试：8 个断言，4 个测试用例

## 与 SpinalHDL 的差异

| 特性 | SpinalHDL | CppHDL | 原因 |
|------|-----------|--------|------|
| 操作符语法 | `<<`, `<<=<`, `<<</<` | `<<=`, `sink <<= pipe().output` | C++ 操作符限制 |
| 管道方法 | `.m2sPipe()`, `.stage()` | `.m2sPipe()`, `.stage()` | 相同 |
| FIFO 深度 | 运行时参数 | 编译时常量模板参数 | C++ 模板限制 |
| Lambda 支持 | 完全支持 | 需要类型推导 | C++ 类型系统 |

## 总结

通过本次实现，CppHDL 现在提供了与 SpinalHDL 类似的 Stream 操作功能，包括：

1. **完整的管道原语** - M2S、S2M、halfPipe
2. **流畅的成员函数 API** - `.m2sPipe()`, `.stage()`, `.s2mPipe()`, `.halfPipe()`
3. **连接操作符** - `<<=` 和 `>>=` 操作符
4. **链式操作构建器** - `StreamBuilder` 流畅 API
5. **丰富的仲裁策略** - 锁定仲裁、顺序仲裁
6. **位宽适配器** - 窄到宽、宽到窄转换

虽然由于 C++ 语言限制，某些语法与 SpinalHDL 有所不同，但功能上是等价的。所有实现都遵循 CppHDL 的编码约定，并与现有代码库完全兼容。