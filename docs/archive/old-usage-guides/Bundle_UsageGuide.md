# CppHDL Bundle 使用指南

本指南面向使用者，聚焦“如何在模块中正确使用 Bundle 作为接口”。若需要内部实现与元编程细节，请查看 Bundle_DeveloperGuide.md。

## 1. Bundle 概述

Bundle 用于把相关信号打包成一个逻辑单元，常用于 AXI、Stream 等协议接口。CppHDL 推荐将 Bundle **直接作为 Component 的成员变量**，减少代理层与宏展开的复杂度。

## 2. 推荐使用模式（Bundle 作为成员）

| 维度 | `__io()` + `bundle_port_expanded` | **推荐：Bundle 作为直接成员** |
|------|------------------------------------|-----------------------------|
| 定义方式 | 使用宏在 `io_type` 中定义 | 直接声明为成员变量 |
| 字段访问 | 需要 `port` 代理 | 直接访问 `bundle.field` |
| 模块连接 | `port <<= port` | `bundle <<= bundle` |
| 语义清晰性 | port 与 bundle 语义混杂 | Bundle 即 Bundle |
| 仿真交互 | 通过 port 间接读取 | 直接对 bundle 对象操作 |
| 灵活性 | 固定 IO 结构 | 支持多个 bundle 成员 |

## 3. 基本定义（自定义 Bundle）

```cpp
#include "core/bundle/bundle_base.h"
#include "core/bundle/bundle_meta.h"
#include "core/uint.h"
#include "core/bool.h"

template<typename T>
struct stream_bundle : public ch::core::bundle_base<stream_bundle<T>> {
    using Self = stream_bundle<T>;
    T payload;
    ch::core::ch_bool valid;
    ch::core::ch_bool ready;

    stream_bundle() = default;
    explicit stream_bundle(const std::string& prefix) {
        this->set_name_prefix(prefix);
    }

    CH_BUNDLE_FIELDS_T(payload, valid, ready)

    void as_master_direction() {
        this->make_output(payload, valid);
        this->make_input(ready);
    }

    void as_slave_direction() {
        this->make_input(payload, valid);
        this->make_output(ready);
    }
};
```

## 4. 方向与接口

Bundle 通过 `as_master()` / `as_slave()` 设置字段方向：

- `as_master()`：payload/valid 为输出，ready 为输入
- `as_slave()`：payload/valid 为输入，ready 为输出

```cpp
ch::ch_stream<ch_uint<32>> stream;
stream.as_master();
stream.payload = data;
stream.valid = fire;
auto pressure = stream.ready;
```

## 5. 连接方式

### 5.1 直接 Bundle 连接（推荐）

```cpp
consumer.stream_in <<= producer.stream_out;
```

连接会根据字段方向自动匹配，支持嵌套 Bundle。

### 5.2 同类型 Bundle 的 connect

```cpp
ch::core::connect(src_bundle, dst_bundle);
```

`connect` 适用于同类型 Bundle 的字段级别赋值连接。

## 6. 常用内置 Bundle

```cpp
#include "bundle/stream_bundle.h"
#include "bundle/flow_bundle.h"
#include "bundle/axi_lite_bundle.h"
#include "bundle/clock_reset_bundle.h"
```

常见类型：
- `ch::ch_stream<T>` / `ch::ch_flow<T>`
- `ch::axi_lite_bundle<ADDR_WIDTH, DATA_WIDTH>`
- `ch::axi_bundle<ADDR_WIDTH, DATA_WIDTH>`
- `ch::clock_reset_bundle`

## 7. 实战示例

### 7.1 Producer/Consumer Stream

```cpp
template <typename T>
class Producer : public ch::Component {
public:
    ch::ch_stream<T> stream_out;

    Producer(ch::Component *parent = nullptr,
             const std::string &name = "producer")
        : ch::Component(parent, name), stream_out() {
        stream_out.as_master();
    }

    void describe() override {
        ch_reg<T> counter(0_d, "counter");
        auto fire = stream_out.valid && stream_out.ready;
        counter->next = select(fire, counter + 1_b, counter);
        stream_out.payload = counter;
        stream_out.valid = 1_b;
    }
};

template <typename T>
class Consumer : public ch::Component {
public:
    ch::ch_stream<T> stream_in;

    Consumer(ch::Component *parent = nullptr,
             const std::string &name = "consumer")
        : ch::Component(parent, name), stream_in() {
        stream_in.as_slave();
    }

    void describe() override {
        ch_reg<T> received_count(0_d, "received_count");
        auto data_received = stream_in.valid;
        received_count->next =
            select(data_received, received_count + 1_b, received_count);
        stream_in.ready = 1_b;
    }
};
```

### 7.2 FIFO 控制接口

```cpp
class FIFOController : public ch::Component {
public:
    ch::fifo_bundle<ch_uint<32>> port;

    FIFOController(ch::Component *parent = nullptr,
                   const std::string &name = "fifo_ctrl")
        : ch::Component(parent, name), port() {
        port.as_slave();
    }

    void describe() override {
        ch_mem<ch_uint<32>, 256> memory("fifo_mem");
        ch_reg<ch_uint<8>> write_ptr(0_d, "wr_ptr");
        ch_reg<ch_uint<8>> read_ptr(0_d, "rd_ptr");

        auto writing = port.push;
        auto reading = port.pop;
        write_ptr->next = select(writing, write_ptr + 1_b, write_ptr);
        read_ptr->next = select(reading, read_ptr + 1_b, read_ptr);
        port.full = /* ... */;
        port.empty = /* ... */;
    }
};
```

### 7.3 AXI-Lite Bundle（特性与校验）

```cpp
#include "bundle/axi_lite_bundle.h"
#include "bundle/axi_protocol.h"

using AxiLite = ch::axi_lite_bundle<32, 32>;
static_assert(ch::core::is_axi_lite_v<AxiLite>);
static_assert(ch::core::is_axi_lite_write_v<AxiLite>);
static_assert(ch::core::is_axi_lite_read_v<AxiLite>);
```

## 8. 仿真访问

```cpp
Top& top = device.instance();
auto payload_val = top.ext_stream.payload;
auto valid_signal = top.ext_stream.valid;
auto ready_signal = top.ext_stream.ready;
```

## 9. Bundle 操作函数

```cpp
#include "core/bundle/bundle_operations.h"

auto slice = ch::core::bundle_slice<1, 2>(my_bundle);
auto combined = ch::core::bundle_cat(bundle1, bundle2);
```

提示：当前 `bundle_slice` 仅支持最多 3 个字段的切片。

## 10. 命名与有效性

```cpp
bundle.set_name_prefix("my_prefix");
bool ok = bundle.is_valid();
```

## 11. 迁移指南（从 `__io()` 到 Bundle 成员）

1. 将 `io_type` 中的 Bundle 字段移为 Component 成员变量
2. 在构造函数中调用 `as_master()` / `as_slave()`
3. 将 `port <<= port` 替换为 `bundle <<= bundle`
4. 仿真测试中直接访问 `bundle.field`

## 12. 最佳实践

- 用 Bundle 描述清晰接口，避免单一组件承担过多职责
- 使用类型别名（`using DataStream = ch::ch_stream<T>;`）提高可读性
- 在顶层与子模块之间使用 `bundle <<= bundle` 保持一致连接语义
- 对公共接口添加注释（字段含义、握手规则）