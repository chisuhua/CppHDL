# CppHDL Bundle 使用指南

## 1. Bundle 概述

Bundle 是 CppHDL 中用于表示复杂信号集合的机制，允许将多个相关的信号打包成一个逻辑单元。Bundle 在接口定义中特别有用，例如 AXI、Stream 等协议。

## 2. Bundle 的基本定义

Bundle 通过继承 `bundle_base<Derived>` 模板类来定义，需要指定其包含的字段，并实现 `as_master()` 和 `as_slave()` 方法来定义方向。

```cpp
#include "core/bundle/bundle_base.h"
#include "core/bundle/bundle_meta.h"
#include "core/uint.h"
#include "core/bool.h"

template<typename T>
struct stream_bundle : public bundle_base<Stream<T>> {
    using Self = Stream<T>;
    T payload;           // 数据载荷
    ch_bool valid;       // 有效信号
    ch_bool ready;       // 就绪信号

    stream_bundle() = default;
    explicit stream_bundle(const std::string& prefix) {
        this->set_name_prefix(prefix);
    }

    // 使用宏定义bundle字段
    CH_BUNDLE_FIELDS(Self, payload, valid, ready)

    void as_master() override {
        // Master: 输出数据，接收就绪信号
        this->make_output(payload, valid);
        this->make_input(ready);
    }

    void as_slave() override {
        // Slave: 接收数据，发送就绪信号
        this->make_input(payload, valid);
        this->make_output(ready);
    }
};
```

## 3. Bundle 宏定义

CppHDL 提供了 `CH_BUNDLE_FIELDS` 宏来简化 Bundle 字段定义：

```cpp
CH_BUNDLE_FIELDS(Self, field1, field2, field3)
```

该宏会自动创建 `__bundle_fields()` 方法，返回包含所有字段信息的元组。

## 4. Bundle 操作

### 4.1 方向控制

Bundle 提供了 `as_master()` 和 `as_slave()` 方法来控制其内部信号的方向：

- `as_master()`: 指定哪些信号是输出，哪些是输入
- `as_slave()`: 与 `as_master()` 相反的方向设置
- `flip()`: 自动翻转 Bundle 的方向

### 4.2 命名

Bundle 支持批量命名功能：

```cpp
bundle.set_name_prefix("my_prefix");
```

### 4.3 连接

Bundle 支持直接连接操作：

```cpp
template <typename BundleT> 
void connect(BundleT &src, BundleT &dst);
```

### 4.4 有效性检查

Bundle 提供了 `is_valid()` 方法来检查其内部信号是否有效。

## 5. Bundle 操作函数

### 5.1 `bundle_slice`

从现有 Bundle 中提取指定字段子集：

```cpp
template <size_t Start, size_t Count, typename BundleT>
auto bundle_slice(BundleT &bundle) {
    return bundle_slice_view<Start, Count, BundleT>(bundle);
}
```

使用示例：
```cpp
// 从bundle中提取第1-2个字段
auto slice = bundle_slice<1, 2>(my_bundle);
```

### 5.2 `bundle_cat`

连接两个 Bundle，生成一个新的 Bundle：

```cpp
template <typename Bundle1, typename Bundle2>
auto bundle_cat(Bundle1 b1, Bundle2 b2) {
    return bundle_concat<Bundle1, Bundle2>(std::move(b1), std::move(b2));
}
```

使用示例：
```cpp
auto combined = bundle_cat(bundle1, bundle2);
```

## 6. 嵌套 Bundle

Bundle 支持嵌套定义，可以将多个 Bundle 组合成更复杂的 Bundle：

```cpp
template<size_t ADDR_WIDTH, size_t DATA_WIDTH>
struct axi_write_channel : public bundle_base<axi_write_channel<ADDR_WIDTH, DATA_WIDTH>> {
    using Self = axi_write_channel;
    axi_addr_channel<ADDR_WIDTH> aw;      // 地址通道
    axi_write_data_channel<DATA_WIDTH> w; // 数据写通道
    axi_write_resp_channel b;             // 响应通道

    CH_BUNDLE_FIELDS(Self, aw, w, b)

    void as_master() override {
        this->make_output(aw, w);
        this->make_input(b);
    }

    void as_slave() override {
        this->make_input(aw, w);
        this->make_output(b);
    }
};
```

## 7. 实际应用示例

### 7.1 AXI-Lite Bundle

AXI-Lite Bundle 包含读写通道，每个通道又包含多个子通道：

```cpp
template<size_t ADDR_WIDTH, size_t DATA_WIDTH>
struct axi_lite_bundle : public bundle_base<axi_lite_bundle<ADDR_WIDTH, DATA_WIDTH>> {
    using Self = axi_lite_bundle<ADDR_WIDTH,DATA_WIDTH>;
    axi_lite_aw_channel<ADDR_WIDTH> aw;      // 写地址通道
    axi_lite_w_channel<DATA_WIDTH> w;        // 写数据通道
    axi_lite_b_channel b;                    // 写响应通道
    axi_lite_ar_channel<ADDR_WIDTH> ar;      // 读地址通道
    axi_lite_r_channel<DATA_WIDTH> r;        // 读数据通道

    CH_BUNDLE_FIELDS(Self, aw, w, b, ar, r)

    void as_master() override {
        this->make_output(aw, w, ar);
        this->make_input(b, r);
    }

    void as_slave() override {
        this->make_input(aw, w, ar);
        this->make_output(b, r);
    }
};
```

## 8. Bundle 的序列化与反序列化

Bundle 支持转换为位向量和从位向量恢复：

```cpp
// 序列化
template <unsigned W = 0>
ch_uint<W> to_bits() const;

// 反序列化
template <unsigned W = 0>
static Derived from_bits(const ch_uint<W> &bits);
```

## 9. 注意事项

1. Bundle 必须继承自 `bundle_base<Derived>`，其中 `Derived` 是 Bundle 自己的类型
2. 必须使用 `CH_BUNDLE_FIELDS` 宏定义 Bundle 字段
3. 必须实现 `as_master()` 和 `as_slave()` 方法
4. Bundle 支持嵌套，但需要确保所有嵌套类型也符合 Bundle 规范
5. Bundle 可以像普通类型一样使用，支持复制、移动等操作