# SpinalHDL Stream 连接功能与 CppHDL ch_stream 对比

## 概述

本文档详细列出了 SpinalHDL Stream 提供的连接功能，并与 CppHDL 的 ch_stream 连接操作进行全面对比。

## 1. SpinalHDL Stream 连接操作符

SpinalHDL 提供了丰富的连接操作符，用于不同的流水线和时序需求。

### 1.1 基本连接操作符

| 操作符 | 别名 | 流水线阶段 | 组合路径 | 延迟 | 描述 |
|--------|------|------------|----------|------|------|
| `<<` | `<--` | 无 | 完全组合 | 0 周期 | 直接连接，所有信号都是组合逻辑 |
| `>>` | `-->` | 无 | 完全组合 | 0 周期 | 直接连接（反向语法） |
| `<-<` | `<--<` | valid/payload | 半流水线 | 1 周期 | master-to-slave 流水线（m2sPipe） |
| `>->` | `>-->` | valid/payload | 半流水线 | 1 周期 | master-to-slave 流水线（反向语法） |
| `</<` | `<--/<` | ready | 半流水线 | 0 周期 | slave-to-master 流水线（s2mPipe） |
| `>/>` | `>--/>` | ready | 半流水线 | 0 周期 | slave-to-master 流水线（反向语法） |
| `<-/<` | `<--/<` | valid/payload/ready | 全流水线 | 1 周期 | 双向流水线（m2sPipe + s2mPipe） |
| `>/->`| `>--/->` | valid/payload/ready | 全流水线 | 1 周期 | 双向流水线（反向语法） |

### 1.2 连接操作符详细说明

#### 1.2.1 直接连接 `<<` / `>>`

```scala
// SpinalHDL 示例
val source = Stream(UInt(8 bits))
val sink = Stream(UInt(8 bits))

sink << source  // 直接连接，完全组合
// 等价于
source >> sink
```

**特点：**
- 零延迟
- 所有信号（valid、ready、payload）都是组合逻辑
- 可能导致长组合路径
- 适合同一模块内的短连接

#### 1.2.2 Master-to-Slave 流水线 `<-<` / `>->`

```scala
// SpinalHDL 示例
sink <-< source  // 在 valid 和 payload 上插入寄存器
// 等价于
source >-> sink
// 等价于
sink << source.m2sPipe()
// 等价于
sink << source.stage()
```

**特点：**
- 1 周期延迟
- 在 valid 和 payload 信号上插入寄存器
- ready 信号仍然是组合路径
- 打破 master 到 slave 的组合路径
- 适合需要流水线化数据路径的场景

#### 1.2.3 Slave-to-Master 流水线 `</<` / `>/>`

```scala
// SpinalHDL 示例
sink </< source  // 在 ready 上插入寄存器
// 等价于
source >/> sink
// 等价于
sink << source.s2mPipe()
```

**特点：**
- 0 周期数据延迟（valid/payload 是组合的）
- 在 ready 信号上插入寄存器
- 打破 slave 到 master 的反压组合路径
- 适合需要打破反压路径的场景

#### 1.2.4 完全流水线 `<-/<` / `>/->`

```scala
// SpinalHDL 示例
sink <-/< source  // 双向流水线
// 等价于
source >/-> sink
// 等价于
sink << source.m2sPipe().s2mPipe()
```

**特点：**
- 1 周期延迟
- 所有握手信号都插入寄存器
- 打破所有组合路径
- 最适合跨时钟域或长距离连接

### 1.3 SpinalHDL Stream 连接操作符总结

**优点：**
- 语法简洁直观
- 提供多种流水线选项
- 编译器自动处理信号连接
- 类型安全

**缺点：**
- 需要学习特殊操作符
- 操作符语义可能不够清晰（对新手）

## 2. SpinalHDL Stream 流水线方法

除了连接操作符，SpinalHDL 还提供了显式的流水线方法：

### 2.1 流水线方法列表

| 方法 | 等价操作符 | 描述 |
|------|------------|------|
| `.m2sPipe()` | `<-<` | Master-to-Slave 流水线寄存器 |
| `.stage()` | `<-<` | 同 m2sPipe，更语义化的名称 |
| `.s2mPipe()` | `</<` | Slave-to-Master 流水线寄存器 |
| `.halfPipe()` | - | 半带宽流水线（所有信号寄存器化但降低带宽） |
| `.queue(depth)` | - | 添加 FIFO 缓冲 |

### 2.2 流水线方法示例

```scala
// SpinalHDL 示例
val pipelined1 = source.m2sPipe()
val pipelined2 = source.stage()  // 语义化名称
val pipelined3 = source.s2mPipe()
val pipelined4 = source.m2sPipe().s2mPipe()  // 完全流水线
val buffered = source.queue(16)  // 添加16深度的FIFO
```

## 3. SpinalHDL Stream 实用工具

SpinalHDL 提供了丰富的 Stream 操作工具：

### 3.1 缓冲与队列

| 工具 | 描述 |
|------|------|
| `StreamFifo(dataType, depth)` | 同步 FIFO |
| `StreamFifoCC(dataType, depth, pushClk, popClk)` | 跨时钟域 FIFO |
| `stream.queue(depth)` | 快速添加队列 |

### 3.2 流拓扑操作

| 工具 | 描述 |
|------|------|
| `StreamFork(input, outputCount)` | 将一个流复制到多个输出 |
| `StreamFork2(input, sync)` | 将流分叉为2个输出 |
| `StreamJoin(inputs)` | 等待所有输入流就绪后传输 |
| `StreamArbiter(policy, inputs)` | 仲裁多个输入流 |
| `StreamArbiterFactory` | 仲裁器工厂（支持 roundRobin、lowerFirst 等） |

### 3.3 流选择与路由

| 工具 | 描述 |
|------|------|
| `StreamMux(select, inputs)` | 多路选择器 |
| `StreamDemux(input, select, outputCount)` | 解复用器 |

### 3.4 流转换与控制

| 方法 | 描述 |
|------|------|
| `.haltWhen(condition)` | 当条件为真时暂停流 |
| `.throwWhen(condition)` | 当条件为真时丢弃数据 |
| `.continueWhen(condition)` | 仅在条件为真时继续 |
| `.takeWhen(condition)` | 仅在条件为真时获取数据 |
| `.translateWith(newPayload)` | 转换 payload 类型 |
| `.map(func)` | 映射 payload |
| `.filter(condition)` | 过滤流 |

### 3.5 位宽适配

| 工具 | 描述 |
|------|------|
| `StreamWidthAdapter` | 位宽适配器 |
| 流转换工具 | 在不同位宽之间转换 |

### 3.6 跨时钟域

| 工具 | 描述 |
|------|------|
| `StreamCCByToggle(input, outputClk)` | 基于切换信号的跨时钟域传输 |
| `StreamFifoCC(...)` | 跨时钟域 FIFO |

### 3.7 状态查询方法

| 方法 | 描述 |
|------|------|
| `.fire` | 检查是否发生传输（valid && ready） |
| `.isStall` | 检查是否停滞（valid && !ready） |
| `.isFree` | 检查是否空闲（!valid 或 ready） |

## 4. CppHDL ch_stream 连接操作

CppHDL 提供了类似但不同的 Stream 操作方式。

### 4.1 CppHDL 连接方式

CppHDL **不使用操作符重载**进行连接，而是通过以下方式：

#### 4.1.1 直接赋值连接

```cpp
// CppHDL 示例
ch_stream<ch_uint<8>> source;
ch_stream<ch_uint<8>> sink;

// 直接赋值连接（组合逻辑）
sink = source;
// 或者字段级连接
sink.payload = source.payload;
sink.valid = source.valid;
source.ready = sink.ready;
```

#### 4.1.2 函数式连接

CppHDL 通过函数调用而非操作符来实现流水线和处理：

```cpp
// CppHDL 流水线示例
auto pipelined_stream = stream_fifo<ch_uint<8>, 1>(source);
```

### 4.2 CppHDL Stream 工具函数

CppHDL 提供了以下 Stream 操作工具：

#### 4.2.1 缓冲与队列

| 工具 | SpinalHDL 对应 | 描述 |
|------|----------------|------|
| `stream_fifo<T, DEPTH>(input)` | `StreamFifo` | 同步 FIFO |
| `stream_queue<T, DEPTH>(input)` | `.queue(depth)` | 队列（别名） |

```cpp
// CppHDL 示例
auto fifo_result = stream_fifo<ch_uint<8>, 16>(input_stream);
// 访问
auto push = fifo_result.push_stream;
auto pop = fifo_result.pop_stream;
auto full = fifo_result.full;
auto empty = fifo_result.empty;
```

#### 4.2.2 流拓扑操作

| 工具 | SpinalHDL 对应 | 描述 |
|------|----------------|------|
| `stream_fork<T, N>(input, sync)` | `StreamFork` | 流分叉 |
| `stream_join<T, N>(inputs)` | `StreamJoin` | 流合并 |
| `stream_arbiter_round_robin<T, N>(clk, rst, inputs)` | `StreamArbiter.roundRobin` | 轮询仲裁 |
| `stream_arbiter_priority<T, N>(inputs)` | `StreamArbiter.lowerFirst` | 优先级仲裁 |

```cpp
// CppHDL 示例
// Fork
auto fork_result = stream_fork<ch_uint<8>, 2>(input_stream, false);
auto out1 = fork_result.output_streams[0];
auto out2 = fork_result.output_streams[1];

// Join
std::array<ch_stream<ch_uint<8>>, 2> inputs = {stream1, stream2};
auto join_result = stream_join<ch_uint<8>, 2>(inputs);

// Arbiter
auto arb_result = stream_arbiter_round_robin<ch_uint<8>, 4>(clk, rst, inputs);
```

#### 4.2.3 流选择与路由

| 工具 | SpinalHDL 对应 | 描述 |
|------|----------------|------|
| `stream_mux<N, T>(inputs, select)` | `StreamMux` | 多路选择器 |
| `stream_demux<T, N>(input, select)` | `StreamDemux` | 解复用器 |

```cpp
// CppHDL 示例
// Mux
auto mux_result = stream_mux<4, ch_uint<8>>(input_streams, select);
stream_mux_connect_ready(mux_result);  // 连接 ready 信号

// Demux
auto demux_result = stream_demux<ch_uint<8>, 4>(input_stream, select);
```

#### 4.2.4 流转换与控制

| 工具 | SpinalHDL 对应 | 描述 |
|------|----------------|------|
| `stream_halt_when<T>(input, halt)` | `.haltWhen()` | 暂停流 |
| `stream_throw_when<T>(input, condition)` | `.throwWhen()` | 丢弃数据 |
| `stream_continue_when<T>(input, condition)` | `.continueWhen()` | 条件继续 |
| `stream_take_when<T>(input, condition)` | `.takeWhen()` | 条件获取 |
| `stream_translate_with<TOut, TIn>(input, transform)` | `.translateWith()` | 转换 payload |
| `stream_map<TOut, TIn>(input, func)` | `.map()` | 映射函数 |
| `stream_transpose<TOut, TIn>(input)` | - | 位宽转换 |

```cpp
// CppHDL 示例
// Halt when
auto halted = stream_halt_when(input_stream, halt_signal);

// Throw when
auto filtered = stream_throw_when(input_stream, drop_condition);

// Map
auto mapped = stream_map<ch_uint<16>, ch_uint<8>>(
    input_stream, 
    [](ch_uint<8> x) { return ch_uint<16>(x) * 2_d; }
);

// Translate with
auto translated = stream_translate_with<ch_uint<16>, ch_uint<8>>(
    input_stream,
    [](ch_uint<8> x) { return ch_uint<16>(x) << 1; }
);
```

#### 4.2.5 流组合

| 工具 | SpinalHDL 对应 | 描述 |
|------|----------------|------|
| `stream_combine_with<T1, T2>(input1, input2)` | - | 组合两个流 |

```cpp
// CppHDL 示例
auto combined = stream_combine_with(stream1, stream2);
// 访问组合的 payload
auto data1 = combined.output_stream.payload._1;
auto data2 = combined.output_stream.payload._2;
```

#### 4.2.6 状态查询方法

| 方法 | SpinalHDL 对应 | 描述 |
|------|---------------|------|
| `.fire()` | `.fire` | 检查传输发生（valid && ready） |
| `.isStall()` | `.isStall` | 检查停滞（valid && !ready） |

```cpp
// CppHDL 示例
ch_stream<ch_uint<8>> stream;
auto is_firing = stream.fire();
auto is_stalled = stream.isStall();
```

### 4.3 CppHDL 目前缺少的功能

以下是 SpinalHDL 有但 CppHDL 当前版本尚未提供的功能：

| 功能类别 | SpinalHDL 功能 | CppHDL 状态 |
|---------|----------------|------------|
| **连接操作符** | `<<`, `>>`, `<-<`, `>->`, `</<`, `>/>` 等 | ❌ 不支持（使用赋值或函数） |
| **流水线方法** | `.m2sPipe()`, `.stage()`, `.s2mPipe()` | ⚠️ 需通过 FIFO 或手动实现 |
| **半带宽流水线** | `.halfPipe()` | ❌ 未实现 |
| **跨时钟域** | `StreamFifoCC`, `StreamCCByToggle` | ❌ 需手动实现 |
| **位宽适配器** | `StreamWidthAdapter` | ❌ 需手动实现 |
| **过滤器** | `.filter()` | ⚠️ 可通过 `stream_take_when` 实现 |
| **仲裁策略** | 多种策略（priority、roundRobin、lock 等） | ⚠️ 仅支持 roundRobin 和 priority |

## 5. 详细对比表

### 5.1 连接方式对比

| 特性 | SpinalHDL | CppHDL |
|------|-----------|--------|
| **基本连接** | `sink << source` | `sink = source` 或字段级赋值 |
| **流水线连接** | `sink <-< source` | 需使用 `stream_fifo<T,1>(...)` |
| **反压流水线** | `sink </< source` | 需手动实现 |
| **完全流水线** | `sink <-/< source` | 需使用 FIFO 或手动实现 |
| **语法风格** | 操作符重载（DSL 风格） | 函数调用（标准 C++） |
| **类型安全** | 编译时检查 | 编译时检查 |
| **学习曲线** | 需学习特殊操作符 | 标准 C++ 函数调用 |

### 5.2 功能完整性对比

| 功能 | SpinalHDL | CppHDL | 备注 |
|------|-----------|--------|------|
| **FIFO/Queue** | ✅ | ✅ | 都支持 |
| **Fork** | ✅ | ✅ | 都支持同步/异步模式 |
| **Join** | ✅ | ✅ | 都支持 |
| **Arbiter** | ✅ 多种策略 | ⚠️ 部分支持 | CppHDL 支持 roundRobin 和 priority |
| **Mux/Demux** | ✅ | ✅ | 都支持 |
| **haltWhen** | ✅ | ✅ | 都支持 |
| **throwWhen** | ✅ | ✅ | 都支持 |
| **continueWhen** | ✅ | ✅ | 都支持 |
| **takeWhen** | ✅ | ✅ | 都支持 |
| **translateWith** | ✅ | ✅ | 都支持 |
| **map** | ✅ | ✅ | CppHDL 支持 lambda |
| **filter** | ✅ | ⚠️ 通过 takeWhen | 功能等价 |
| **combineWith** | ⚠️ 通过其他方式 | ✅ | CppHDL 提供专门函数 |
| **跨时钟域** | ✅ | ❌ | SpinalHDL 有专门工具 |
| **位宽适配** | ✅ | ❌ | SpinalHDL 有专门工具 |
| **连接操作符** | ✅ | ❌ | SpinalHDL 独有 |

### 5.3 使用风格对比

#### SpinalHDL 风格

```scala
// SpinalHDL 示例：流水线数据路径
val source = Stream(UInt(8 bits))
val stage1 = source.m2sPipe()  // 流水线阶段1
val stage2 = stage1.queue(4)    // 添加FIFO
val stage3 = stage2.haltWhen(busy)  // 条件控制
val sink = stage3.m2sPipe()    // 流水线阶段2

// 或者使用连接操作符
sink <-< stage3
stage3 << stage2.haltWhen(busy)
stage2 << stage1.queue(4)
stage1 <-< source
```

**特点：**
- 链式调用风格
- 操作符连接简洁
- DSL 语法优雅
- 适合快速构建流水线

#### CppHDL 风格

```cpp
// CppHDL 示例：流水线数据路径
ch_stream<ch_uint<8>> source;

// 流水线阶段1（使用1深度FIFO）
auto stage1_result = stream_fifo<ch_uint<8>, 1>(source);
auto stage1 = stage1_result.pop_stream;

// 添加FIFO
auto stage2_result = stream_fifo<ch_uint<8>, 4>(stage1);
auto stage2 = stage2_result.pop_stream;

// 条件控制
auto stage3 = stream_halt_when(stage2, busy);

// 流水线阶段2
auto sink_result = stream_fifo<ch_uint<8>, 1>(stage3);
auto sink = sink_result.pop_stream;
```

**特点：**
- 显式函数调用
- 标准 C++ 语法
- 中间结果可访问（如 full、empty 信号）
- 更详细的控制

## 6. 设计哲学对比

### 6.1 SpinalHDL 设计哲学

**优点：**
- **DSL 风格**：专门为硬件设计优化的语法
- **操作符丰富**：直观的连接和流水线操作符
- **高度抽象**：隐藏底层细节
- **链式调用**：流畅的 API 设计
- **工具完善**：丰富的内置工具和组件

**适用场景：**
- 快速原型开发
- 复杂数据流设计
- 需要跨时钟域处理
- 团队熟悉 Scala

### 6.2 CppHDL 设计哲学

**优点：**
- **标准 C++**：无需学习特殊语法
- **显式控制**：所有操作都是明确的函数调用
- **灵活性高**：可以轻松扩展和定制
- **类型安全**：利用 C++ 模板系统
- **性能优化**：可以进行底层优化

**适用场景：**
- C++ 项目集成
- 需要精细控制的设计
- 性能关键应用
- 团队熟悉 C++

## 7. 代码示例对比

### 7.1 简单流水线

#### SpinalHDL

```scala
class SimplePipeline extends Component {
  val io = new Bundle {
    val input = slave Stream(UInt(8 bits))
    val output = master Stream(UInt(8 bits))
  }
  
  // 三级流水线，每级之间都有寄存器
  io.output <-< io.input.m2sPipe().m2sPipe()
}
```

#### CppHDL

```cpp
struct SimplePipeline {
    ch_stream<ch_uint<8>> process(ch_stream<ch_uint<8>> input) {
        // 三级流水线，每级之间都有寄存器
        auto stage1 = stream_fifo<ch_uint<8>, 1>(input).pop_stream;
        auto stage2 = stream_fifo<ch_uint<8>, 1>(stage1).pop_stream;
        auto stage3 = stream_fifo<ch_uint<8>, 1>(stage2).pop_stream;
        return stage3;
    }
};
```

### 7.2 流分叉和仲裁

#### SpinalHDL

```scala
class ForkAndArbitrate extends Component {
  val io = new Bundle {
    val input = slave Stream(UInt(8 bits))
    val output = master Stream(UInt(8 bits))
  }
  
  // 分叉为两路
  val (fork1, fork2) = StreamFork2(io.input)
  
  // 各自处理
  val processed1 = fork1.queue(4)
  val processed2 = fork2.queue(4)
  
  // 仲裁合并
  val arbiter = StreamArbiterFactory.roundRobin.on(Seq(processed1, processed2))
  io.output << arbiter
}
```

#### CppHDL

```cpp
struct ForkAndArbitrate {
    ch_stream<ch_uint<8>> process(
        ch_bool clk, ch_bool rst,
        ch_stream<ch_uint<8>> input
    ) {
        // 分叉为两路
        auto fork_result = stream_fork<ch_uint<8>, 2>(input, false);
        
        // 各自处理（添加队列）
        auto processed1 = stream_fifo<ch_uint<8>, 4>(
            fork_result.output_streams[0]
        ).pop_stream;
        auto processed2 = stream_fifo<ch_uint<8>, 4>(
            fork_result.output_streams[1]
        ).pop_stream;
        
        // 仲裁合并
        std::array<ch_stream<ch_uint<8>>, 2> arb_inputs = {
            processed1, processed2
        };
        auto arbiter = stream_arbiter_round_robin<ch_uint<8>, 2>(
            clk, rst, arb_inputs
        );
        
        return arbiter.output_stream;
    }
};
```

### 7.3 条件过滤和映射

#### SpinalHDL

```scala
class FilterAndMap extends Component {
  val io = new Bundle {
    val input = slave Stream(UInt(8 bits))
    val output = master Stream(UInt(16 bits))
  }
  
  val filtered = io.input.throwWhen(io.input.payload < 10)
  val mapped = filtered.map(x => x.resize(16 bits) * 2)
  io.output << mapped
}
```

#### CppHDL

```cpp
struct FilterAndMap {
    ch_stream<ch_uint<16>> process(ch_stream<ch_uint<8>> input) {
        // 过滤：丢弃小于10的值
        auto filtered = stream_throw_when(input, input.payload < 10_d);
        
        // 映射：转换为16位并乘以2
        auto mapped = stream_map<ch_uint<16>, ch_uint<8>>(
            filtered,
            [](ch_uint<8> x) { 
                return ch_uint<16>(x) * 2_d; 
            }
        );
        
        return mapped;
    }
};
```

## 8. 总结

### 8.1 SpinalHDL Stream 连接功能总结

**连接操作符：**
- ✅ 8种连接操作符（`<<`, `>>`, `<-<`, `>->`, `</<`, `>/>`, `<-/<`, `>/->`）
- ✅ 自动流水线插入
- ✅ 语法简洁直观

**流操作工具：**
- ✅ 丰富的 FIFO 和队列工具
- ✅ 完善的 Fork/Join/Arbiter
- ✅ 多种条件控制方法
- ✅ 跨时钟域支持
- ✅ 位宽适配器

**优势：**
- DSL 风格，专为硬件设计优化
- 工具链完善，开箱即用
- 社区活跃，文档丰富

**局限：**
- 需要学习 Scala 和特殊操作符
- 固定的抽象层次
- 某些场景下可能过度抽象

### 8.2 CppHDL ch_stream 连接功能总结

**连接方式：**
- ✅ 标准 C++ 赋值和函数调用
- ⚠️ 无专用连接操作符（可视为优点或缺点）
- ✅ 显式、可预测的行为

**流操作工具：**
- ✅ 核心 Stream 操作（FIFO、Fork、Join、Arbiter）
- ✅ 条件控制方法（halt、throw、continue、take）
- ✅ 转换和映射函数
- ⚠️ 部分高级功能需要手动实现（跨时钟域、位宽适配）

**优势：**
- 标准 C++，无需学习新语法
- 灵活性高，易于扩展
- 与 C++ 生态系统无缝集成
- 精细的控制能力

**局限：**
- 缺少特殊连接操作符（代码可能更冗长）
- 某些高级功能尚未实现
- 需要更多的显式代码

### 8.3 选择建议

**选择 SpinalHDL 如果：**
- 需要快速开发复杂数据流设计
- 团队熟悉 Scala 或愿意学习
- 需要跨时钟域等高级功能
- 偏好 DSL 风格的硬件描述

**选择 CppHDL 如果：**
- 项目已经使用 C++
- 团队熟悉 C++
- 需要与 C++ 代码深度集成
- 需要精细的底层控制
- 偏好显式、标准的语法

### 8.4 未来发展方向

**CppHDL 可以考虑添加：**
1. 操作符重载支持（可选）：
   ```cpp
   // 可能的语法
   sink <<= source;  // 直接连接
   sink <<= stream_pipe(source);  // 流水线连接
   ```

2. 跨时钟域支持：
   ```cpp
   auto cdc_result = stream_cdc<ch_uint<8>>(input, output_clk);
   ```

3. 位宽适配器：
   ```cpp
   auto adapted = stream_width_adapter<ch_uint<16>, ch_uint<8>>(input);
   ```

4. 更多仲裁策略：
   ```cpp
   auto arb = stream_arbiter_lock<T, N>(...);  // 锁定仲裁
   auto arb = stream_arbiter_sequence<T, N>(...);  // 序列仲裁
   ```

5. 链式调用支持（fluent API）：
   ```cpp
   auto result = input
       .queue(4)
       .haltWhen(busy)
       .map(transform)
       .throwWhen(invalid);
   ```

## 9. 参考资料

### SpinalHDL 文档
- [SpinalHDL Stream 官方文档](https://spinalhdl.github.io/SpinalDoc-RTD/master/SpinalHDL/Libraries/stream.html)
- [SpinalHDL Stream, Flow, and Fragment | DeepWiki](https://deepwiki.com/SpinalHDL/SpinalHDL/5.1-stream-flow-and-fragment)
- [SpinalHDL Stream.scala 源码](https://github.com/SpinalHDL/SpinalHDL/blob/dev/lib/src/main/scala/spinal/lib/Stream.scala)

### CppHDL 文档
- [CppHDL vs SpinalHDL Stream/Flow 使用对比](./CppHDL_vs_SpinalHDL_Stream_Flow_Usage.md)
- [CppHDL Stream 实现](../include/chlib/stream.h)
- [CppHDL Stream Bundle 定义](../include/bundle/stream_bundle.h)

### 博客和教程
- [SpinalHDL之Stream - CSDN博客](https://blog.csdn.net/m0_59092412/article/details/140492621)
- [Notes on SpinalHDL Pipeline API](https://blog.hai-hs.in/posts/2023-02-23-notes-on-spinal-hdl-pipeline-api/)

---

**文档版本：** 1.0  
**最后更新：** 2026-03-04
**作者：** CppHDL 开发团队
