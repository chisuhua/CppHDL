# CppHDL Bundle 连接模式指南 - 技能文档

**技能 ID**: `cpphdl-bundle-patterns`  
**版本**: v1.0  
**创建日期**: 2026-04-09  
**优先级**: Critical

---

## 📚 概述

本技能文档总结 CppHDL 框架中 Bundle 类型的正确使用模式，特别适用于 Stream、Flow 和自定义 Bundle 的连接。

---

## ✅ 三种正确的 Bundle 模式

### 模式 1: Bundle 作为 Class 成员（推荐用于流接口）

**适用场景**: 组件外部接口、Stream 流处理、数据输入输出

**特点**: Bundle 直接作为类的成员变量，设置方向后直接访问成员

**示例代码**:

```cpp
// samples/bundle_top_example.cpp
template <typename T>
class Producer : public ch::Component {
public:
    ch::ch_stream<T> stream_out;  // ✅ Bundle 作为成员变量

    Producer(ch::Component* parent = nullptr, 
             const std::string& name = "producer")
        : ch::Component(parent, name), stream_out() {
        stream_out.as_master();  // ✅ 设置方向（Master 输出）
    }

    void describe() override {
        ch_reg<T> counter(0_d, "counter");
        auto fire = stream_out.valid && stream_out.ready;
        counter->next = select(fire, counter + 1_b, counter);
        
        // ✅ 直接访问 Bundle 成员
        stream_out.payload = counter;
        stream_out.valid = 1_b;
    }
};

// Consumer 组件
template <typename T>
class Consumer : public ch::Component {
public:
    ch::ch_stream<T> stream_in;   // ✅ Bundle 作为成员变量（Slave）
    ch::ch_flow<T> out_flow;      // ✅ Flow 作为成员变量

    Consumer(...) : ch::Component(parent, name), stream_in(), out_flow() {
        stream_in.as_slave();     // ✅ 设置方向（Slave 输入）
        out_flow.as_master();
    }

    void describe() override {
        // ✅ 直接访问 Bundle 成员
        stream_in.ready = 1_b;
        out_flow.payload = received_count;
        out_flow.valid = stream_in.valid;
    }
};
```

**关键要点**:
- Bundle 成员在构造函数中初始化
- 使用 `as_master()` 或 `as_slave()` 设置方向
- `describe()` 中直接访问 `bundle.payload`、`bundle.valid`、`bundle.ready`
- 可用于顶层外部接口，Simulator 使用 `set_bundle_value()` / `get_bundle_value()`

---

### 模式 2: __io() 宏定义端口（传统模式）

**适用场景**: 简单 IO 信号、控制信号、非 Bundle 类型

**特点**: 使用 `__io()` 宏定义端口，通过 `io().port` 访问

**示例代码**:

```cpp
// samples/fifo_example.cpp
template <typename T, unsigned N>
class FiFo : public ch::Component {
public:
    __io(
        ch_in<T> din;        // ✅ 基本类型输入
        ch_in<ch_bool> push;
        ch_in<ch_bool> pop;
        ch_out<T> dout;      // ✅ 基本类型输出
        ch_out<ch_bool> empty;
        ch_out<ch_bool> full;
    )

    FiFo(ch::Component* parent = nullptr, const std::string& name = "fifo")
        : ch::Component(parent, name) {}

    void create_ports() override { new (io_storage_) io_type; }

    void describe() override {
        // ✅ 使用 io() 访问端口
        auto writing = io().push;
        rd_ptr->next = select(writing, rd_ptr + 1_b, rd_ptr);
        
        io().dout = data_out;
        io().empty = (rd_ptr == wr_ptr);
    }
};
```

**关键要点**:
- `__io()` 支持基本类型：`ch_in<T>`, `ch_out<T>`, `ch_in<ch_bool>`, `ch_out<ch_bool>`
- 必须实现 `create_ports()`
- `describe()` 中使用 `io().port` 访问
- ❌ 不支持 `ch_out<SomeBundle>` 作为端口类型

---

### 模式 3: 函数式接口返回 Bundle 结构体

**适用场景**: 组件库函数、可复用逻辑（如 Arbiter、Mux、FIFO）

**特点**: 函数返回包含 Bundle 的结构体，而非使用组件 IO 端口

**示例代码**:

```cpp
// include/chlib/stream_arbiter.h
template <typename T, unsigned N_INPUTS>
struct StreamArbiterLockResult {
    std::array<ch_stream<T>, N_INPUTS> input_streams;
    ch_stream<T> output_stream;
    ch_uint<compute_idx_width(N_INPUTS)> selected;
};

// ✅ 函数返回 Bundle 结构体
template <typename T, unsigned N_INPUTS>
StreamArbiterLockResult<T, N_INPUTS>
stream_arbiter_lock(std::array<ch_stream<T>, N_INPUTS> input_streams) {
    StreamArbiterLockResult<T, N_INPUTS> result;

    // ✅ 访问返回结构体的 Bundle 成员
    result.output_stream.payload = selected_payload;
    result.output_stream.valid = valid_vector != 0_d;

    for (unsigned i = 0; i < N_INPUTS; i++) {
        ch_bool is_selected = (selected_idx == make_uint<...>(i));
        result.input_streams[i].ready = is_selected && result.output_stream.ready;
    }

    return result;
}

// 使用示例
auto arbiter_result = stream_arbiter_lock<ch_uint<8>, 4>(input_streams);
top_out_data <<= arbiter_result.output_stream.payload;
top_out_valid = arbiter_result.output_stream.valid;
```

**关键要点**:
- 结构体包含 `ch_stream<T>` 或 `ch_flow<T>` Bundle
- 函数内部直接访问 Bundle 成员
- 调用者获取返回值后访问 `.output_stream`、`.input_streams[i]`
- 参考 `include/chlib/forwarding.h` 使用 `enum class` 定义控制编码

---

## ❌ 错误模式（禁止使用）

### Bundle 结构体作为 __io() 端口类型

```cpp
// ❌ 错误：Bundle 结构体作为 IO 端口
class HazardUnit : public ch::Component {
public:
    struct HazardControl {
        ch_bool if_stall;
        ch_bool id_stall;
    };

    __io(
        ch_out<HazardControl> hazard_ctrl;  // ❌ 编译错误！
    )

    void describe() override {
        io().hazard_ctrl.if_stall = ...;  // ❌ 错误：port 类型没有成员
    }
};
```

**编译错误**:
```
error: 'ch::core::ch_out<HazardControl>' has no member named 'if_stall'
```

### 正确的替代方案

```cpp
// ✅ 正确：使用单独的控制信号端口
class HazardUnit : public ch::Component {
public:
    __io(
        ch_out<ch_bool> if_stall;      // ✅ 单独 ch_bool 端口
        ch_out<ch_bool> id_stall;
        ch_out<ch_bool> flush;
        ch_out<ch_uint<2>> forward_a;  // ✅ 单独 ch_uint 端口
        ch_out<ch_uint<2>> forward_b;
    )

    void describe() override {
        // ✅ 直接赋值
        io().if_stall = load_use_hazard;
        io().forward_a = select(condition, ch_uint<2>(1_d), ch_uint<2>(0_d));
    }
};
```

**参考文件**: 
- `include/chlib/forwarding.h` - ForwardingUnit 实现
- `include/riscv/hazard_unit.h` - HazardUnit 实现（已修复）

---

## 🔌 Bundle 连接模式

### 模式 A: <<<= 操作符连接

```cpp
// samples/bundle_top_example.cpp
class Top : public ch::Component {
public:
    ch::ch_stream<ch_uint<8>> ext_stream;
    ch::ch_flow<ch_uint<8>> result1;

    void describe() override {
        ch::ch_module<Producer<ch_uint<8>>> producer_inst{"producer"};
        ch::ch_module<Consumer<ch_uint<8>>> consumer1_inst{"consumer1"};

        // ✅ 使用 <<= 连接 Bundle
        consumer1_inst.instance().stream_in <<= producer_inst.instance().stream_out;
        result1 <<= consumer1_inst.instance().out_flow;
    }
};
```

### 模式 B: Simulator 设置 Bundle 值

```cpp
// samples/bundle_top_example.cpp
int main() {
    ch::ch_device<Top> top_device;
    ch::Simulator sim(top_device.context());

    // ✅ 设置 Bundle 值（payload=5, valid=1）
    uint64_t ext_value = (1ULL << 8) | 5ULL;  // valid=1, payload=5
    sim.set_bundle_value(top_device.instance().ext_stream, ext_value);

    // ✅ 获取 Bundle 值
    uint64_t r1 = sim.get_bundle_value(top_device.instance().result1);
    uint64_t payload1 = r1 & 0xff;
    uint64_t valid1 = (r1 >> 8) & 0x1;
}
```

### 模式 C: 函数式接口的 Bundle 连接

```cpp
// include/chlib/stream.h
template <typename T, unsigned DEPTH>
StreamFifoResult<T, DEPTH> stream_fifo(ch_stream<T> input_stream) {
    StreamFifoResult<T, DEPTH> result;

    // ✅ 连接 push_stream
    input_stream.ready = result.push_stream.ready;
    result.push_stream.valid = input_stream.valid;
    result.push_stream.payload = input_stream.payload;

    // ✅ 连接 pop_stream
    result.pop_stream.ready = ...;

    return result;
}

// 使用
auto fifo_result = stream_fifo<ch_uint<8>, 16>(input_stream);
output_data <<= fifo_result.pop_stream.payload;
```

---

## 📋 检查清单

### 设计阶段

- [ ] 确定 Bundle 使用场景（外部接口 vs 内部信号）
- [ ] 选择正确模式：
  - 外部接口/流处理 → **模式 1: Class 成员**
  - 简单 IO 信号 → **模式 2: __io() 端口**
  - 组件库函数 → **模式 3: 函数式接口**

### 实现阶段

- [ ] Class 成员模式：
  - [ ] 在构造函数中初始化 Bundle 成员
  - [ ] 调用 `as_master()` 或 `as_slave()` 设置方向
  - [ ] `describe()` 中直接访问 Bundle 成员

- [ ] __io() 端口模式：
  - [ ] 使用基本类型（`ch_in<T>`, `ch_out<ch_bool>`）
  - [ ] 实现 `create_ports()`
  - [ ] ❌ 不使用 `ch_out<SomeBundle>`

- [ ] 函数式接口模式：
  - [ ] 定义 Result 结构体包含 Bundle 成员
  - [ ] 函数返回 Result 结构体
  - [ ] 参考 `forwarding.h` 使用 `enum class`

### 验证阶段

- [ ] 编译通过
- [ ] Simluator 可以设置/获取 Bundle 值
- [ ] Verilog 生成成功

---

## 📁 参考文件清单

### 示例代码

| 文件 | 模式 | 说明 |
|------|------|------|
| `samples/bundle_top_example.cpp` | 模式 1 | Bundle 成员最佳实践 |
| `samples/fifo_example.cpp` | 模式 2 | __io() 传统模式 |
| `samples/spinalhdl_component_function_area_example.cpp` | 混合 | 组件函数混合模式 |

### 库实现

| 文件 | 模式 | 说明 |
|------|------|------|
| `include/chlib/stream_arbiter.h` | 模式 3 | 流仲裁器函数式接口 |
| `include/chlib/forwarding.h` | 模式 3 | 前推检测单元（参考） |
| `include/chlib/stream.h` | 模式 3 | 流 FIFO 函数式接口 |
| `include/riscv/hazard_unit.h` | 修复后 | 冒险检测单元（已修复） |

### 测试代码

| 文件 | 测试内容 |
|------|---------|
| `tests/chlib/test_forwarding.cpp` | ForwardingUnit 测试 |
| `tests/chlib/test_stream.cpp` | Stream 操作测试 |

---

## 🔧 常见错误及修复

### 错误 1: Bundle 作为 __io() 端口

**错误代码**:
```cpp
__io(
    ch_out<MyBundle> bundle_port;
)
io().bundle_port.member = ...;  // ❌ 编译错误
```

**修复**:
```cpp
// 方案 A: 改用 Class 成员
ch::ch_stream<T> my_bundle;  // 成员变量
my_bundle.as_master();
my_bundle.payload = ...;

// 方案 B: 改用单独端口
__io(
    ch_out<ch_uint<32>> payload;
    ch_out<ch_bool> valid;
)
io().payload = ...;
io().valid = ...;
```

### 错误 2: 忘记设置 Bundle 方向

**错误代码**:
```cpp
ch::ch_stream<T> stream_out;  // ❌ 忘记 as_master()
```

**修复**:
```cpp
ch::ch_stream<T> stream_out;
stream_out.as_master();  // ✅ 设置方向
```

### 错误 3: Simulator API 使用错误

**错误代码**:
```cpp
sim.set_port_value(device.instance().stream_out, value);  // ❌ 错误的 API
```

**修复**:
```cpp
sim.set_bundle_value(device.instance().stream_out, value);  // ✅ 正确的 API
```

---

## 💡 最佳实践建议

1. **外部接口优先使用 Class 成员模式**
   - 代码更清晰
   - 直接访问 Bundle 成员
   - Simulator API 支持更好

2. **控制信号使用 __io() 单独端口**
   - 参考 `forwarding.h` 模式
   - 使用 `ch_uint<2>` 编码而非 Bundle

3. **组件库函数使用函数式接口**
   - 返回包含 Bundle 的结构体
   - 便于组合和复用

4. **避免混合使用模式**
   - 一个组件尽量使用单一模式
   - 如需混合，确保边界清晰

---

**维护人**: DevMate  
**下次更新**: 发现新模式或框架变更时
