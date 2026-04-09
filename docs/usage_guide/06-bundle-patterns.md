# Bundle 使用模式

**文档编号**: USAGE-06  
**版本**: v2.0  
**最后更新**: 2026-04-09  
**目标读者**: CppHDL 用户（硬件设计者）  
**前置知识**: [Simulator API 使用指南](04-simulator-api.md), Component 设计基础  

---

## 🎯 本章目标

学完本章，你将能够：

1. ✅ 定义自定义 Bundle 类型
2. ✅ 在 Component 中正确使用 Bundle 作为接口
3. ✅ 使用 as_master()/as_slave() 设置方向
4. ✅ 进行 Bundle 级别的连接
5. ✅ 使用常用内置 Bundle（Stream/AXI）

---

## 📖 Bundle 概述

Bundle 用于将相关信号打包成一个逻辑单元，常用于：
- AXI 总线协议
- Stream/Flow 数据流
- 时钟复位组
- 自定义接口

**推荐实践**: 将 Bundle **直接作为 Component 的成员变量**，减少代理层与宏展开的复杂度。

---

## 🔧 Bundle 定义

### 1. 自定义 Bundle 示例

```cpp
#include "core/bundle/bundle_base.h"
#include "core/uint.h"
#include "core/bool.h"

template<typename T>
struct StreamBundle : public ch::core::bundle_base<StreamBundle<T>> {
    using Self = StreamBundle<T>;
    
    T payload;       // 数据载荷
    ch_bool valid;   // 有效信号
    ch_bool ready;   // 就绪信号
    
    StreamBundle() = default;
    
    explicit StreamBundle(const std::string& prefix) {
        this->set_name_prefix(prefix);
    }
    
    // 注册 Bundle 字段（必需）
    CH_BUNDLE_FIELDS_T(payload, valid, ready)
    
    // 设置主设备方向
    void as_master_direction() {
        this->make_output(payload, valid);
        this->make_input(ready);
    }
    
    // 设置从设备方向
    void as_slave_direction() {
        this->make_input(payload, valid);
        this->make_output(ready);
    }
};
```

---

### 2. Bundle 字段注册宏

**CH_BUNDLE_FIELDS_T**: 必需，注册所有字段

```cpp
// 语法
CH_BUNDLE_FIELDS_T(field1, field2, field3, ...)

// 示例
struct MyBundle : bundle_base<MyBundle> {
    ch_uint<8> data;
    ch_bool valid;
    ch_bool ready;
    
    CH_BUNDLE_FIELDS_T(data, valid, ready)  // ✅ 必需
};
```

**注意事项**:
- ✅ 必须注册所有公开字段
- ✅ 顺序不重要，但建议按定义顺序
- ❌ 不注册会导致编译错误

---

## 🎭 方向设置

### as_master() / as_slave()

Bundle 通过这两个方法设置字段方向：

```cpp
ch_stream<ch_uint<32>> stream;

// ✅ 推荐：显式调用
stream.as_master();    // 主设备：payload/valid 输出，ready 输入
stream.as_slave();     // 从设备：payload/valid 输入，ready 输出
```

### 方向对照表

| 信号 | Master 方向 | Slave 方向 |
|------|----------|-----------|
| payload | Output (→) | Input (←) |
| valid | Output (→) | Input (←) |
| ready | Input (←) | Output (→) |

**记忆技巧**:
- Master **产生**数据和有效信号
- Slave **产生**就绪信号

---

## 🔗 Bundle 连接

### 1. 直接 Bundle 连接（推荐）

```cpp
// ✅ 推荐：Bundle 级别连接
consumer.stream_in <<= producer.stream_out;
```

**特点**:
- 自动按字段方向匹配
- 支持嵌套 Bundle
- 代码简洁清晰

---

### 2. 字段级别连接

```cpp
// 当需要精细控制时
consumer.stream_in.data <<= producer.stream_out.data;
consumer.stream_in.valid <<= producer.stream_out.valid;
consumer.stream_in.ready <<= producer.stream_out.ready;
```

**使用场景**:
- 需要插入逻辑（如寄存器、MUX）
- 部分字段 bypass
- 调试和验证

---

### 3. connect 辅助函数

```cpp
// 同类型 Bundle 的字段级别赋值连接
ch::core::connect(src_bundle, dst_bundle);
```

**等效于**:
```cpp
dst_bundle.field1 <<= src_bundle.field1;
dst_bundle.field2 <<= src_bundle.field2;
// ...
```

---

## 📦 常用内置 Bundle

### Stream / Flow

```cpp
#include "chlib/stream.h"

ch_stream<ch_uint<32>> in_stream;
ch_flow<ch_uint<32>> out_flow;

in_stream.as_master();
out_flow.as_master();
```

**区别**:
- Stream: 标准 valid/ready 握手
- Flow: 简化的流控制

---

### AXI4-Lite

```cpp
#include "chlib/axi_lite_bundle.h"

ch_axi_lite_bundle<32, 32> axi_lite;  // 32 位地址，32 位数据

// 访问字段
axi_lite.awaddr <<= 0x1000;
axi_lite.awvalid <<= 1;
auto awready = axi_lite.awready;
```

**完整字段**:
- 写地址通道：awaddr, awprot, awvalid, awready
- 写数据通道：wdata, wstrb, wvalid, wready
- 写响应通道：bresp, bvalid, bready
- 读地址通道：araddr, arprot, arvalid, arready
- 读数据通道：rdata, rresp, rvalid, rready

---

### 时钟复位 Bundle

```cpp
#include "chlib/clock_reset_bundle.h"

ch_clock_reset_bundle clk_rst;

// 访问
clk_rst.clk <<= 1;
clk_rst.rst <<= 0;
```

---

## ⚠️ 最佳实践

### ✅ 推荐模式

```cpp
// 1. 使用 Bundle 作为成员变量（而非 port 代理）
class MyComponent : public Component {
public:
    __io(StreamBundle<ch_uint<32>> stream_in; StreamBundle<ch_uint<32>> stream_out;);
    
    void describe() override {
        // ✅ Bundle 级别连接
        io().stream_out <<= io().stream_in;
    }
};

// 2. 显式设置方向
stream.as_master();  // 清晰表达意图

// 3. 使用内置 Bundle
ch_stream<T> stream;  // 而非自己定义
```

---

### ⚠️ 注意事项

1. **字段注册**: 必须调用 `CH_BUNDLE_FIELDS_T`
2. **方向设置**: 在 `build()` 前调用 `as_master()/as_slave()`
3. **命名**: 使用 `set_name_prefix()` 避免冲突
4. **连接**: 优先 Bundle 级别连接，而非字段级别

---

## 🔍 故障排除

### 问题 1: 编译错误"未注册字段"

**症状**:
```
error: CH_BUNDLE_FIELDS_T not called
```

**解决**:
```cpp
struct MyBundle : bundle_base<MyBundle> {
    ch_uint<8> data;
    
    CH_BUNDLE_FIELDS_T(data)  // ✅ 添加此行
};
```

---

### 问题 2: Bundle 连接失败

**症状**: 方向不匹配导致连接错误

**解决**:
```cpp
// 确保方向正确
producer_stream.as_master();   // 输出数据
consumer_stream.as_slave();    // 接收数据

// 连接
consumer_stream <<= producer_stream;
```

---

### 问题 3: 仿真中无法访问字段值

**症状**: `sim.get_input_value(bundle.field)` 失败

**解决**:
```cpp
// 方法 1: 独立 Bundle 变量
ch_stream<ch_uint<32>> bundle;
sim.set_input_value(bundle.payload, value);  // ✅ 可用

// 方法 2: 确保 Component build() 已调用
ch::ch_device<MyComponent> dev;  // 自动 build()
sim.set_input_value(dev.io().stream.payload, value);  // ✅ 可用
```

参考：[Simulator API 使用指南](04-simulator-api.md)

---

## 📖 进阶阅读

### 开发者文档

- [Bundle 设计模式](../developer_guide/patterns/bundle-design.md) - Bundle 内部实现
- [Bundle 技术规格](../developer_guide/api-reference/T002-bundle-api.md) - API 细节

### 示例代码

- tests/test_bundle.cpp - Bundle 基础测试
- tests/test_bundle_connection.cpp - 连接测试
- samples/stream_example.cpp - Stream 使用示例

---

## 📊 相关文档

| 文档 | 说明 |
|------|------|
| [04-simulator-api.md](04-simulator-api.md) | Simulator API 使用 |
| [05-component-design.md](05-component-design.md) | Component 设计指南 |
| [07-stream-patterns.md](07-stream-patterns.md) | Stream 使用模式 |

---

**维护**: AI Agent  
**版本**: v2.0  
**下次审查**: 2026-05-09
