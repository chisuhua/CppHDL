# Simulator API 使用指南

**文档编号**: USAGE-04  
**版本**: v2.0  
**最后更新**: 2026-04-09  
**目标读者**: CppHDL 用户（硬件设计者）  
**前置知识**: CppHDL 基础，C++20基础  

---

## 🎯 本章目标

学完本章，你将能够：

1. ✅ 使用 Simulator API 设置/读取信号值
2. ✅ 直接访问 Bundle 字段进行 fine-grained 控制
3. ✅ 编写高效的 testbench 代码
4. ✅ 避免常见的使用错误

---

## 📖 快速开始

### 5 分钟上手

```cpp
#include "ch.hpp"
#include "simulator.h"

using namespace ch::core;

int main() {
    // 1. 创建上下文
    auto ctx = std::make_unique<ch::core::context>("test");
    ch::core::ctx_swap ctx_guard(ctx.get());
    
    // 2. 创建信号
    ch_uint<8> data(0_d);   // 8 位数据信号
    ch_uint<16> addr(0_d);  // 16 位地址信号
    
    // 3. 创建 Simulator
    Simulator sim(ctx.get());
    
    // 4. Phase 4 新增功能：直接设置值
    sim.set_input_value(data, 0xA5);      // 设置 8 位值
    sim.set_input_value(addr, 0x1234);   // 设置 16 位值
    
    // 5. 读取值
    auto d = sim.get_input_value(data);   // 返回 0xA5
    auto a = sim.get_input_value(addr);   // 返回 0x1234
    
    return 0;
}
```

---

## 📚 核心 API 参考

### API 一览表

| API | 输入类型 | 功能 | Phase |
|-----|---------|------|-------|
| `set_input_value(signal, value)` | `ch_uint<N>` | 设置无符号整数 | 4+ |
| `get_input_value(signal)` | `ch_uint<N>` | 读取无符号整数 | 4+ |
| `get_input_value(port)` | `ch_in<T>` | 读取输入端口 | 4+ |
| `set_value(signal, value)` | `ch_bool` | 设置布尔值 | 3+ |
| `set_bundle_value(bundle, val)` | Bundle | 设置整个 Bundle | 3+ |

---

### 1. set_input_value() - 设置信号值

#### A. ch_uint<N> 版本 (Phase 4+)

```cpp
template <unsigned N>
void set_input_value(const ch_uint<N>& signal, uint64_t value);
```

**使用示例**:

```cpp
// ✅ 推荐：独立信号
ch_uint<8> data(0_d);
sim.set_input_value(data, 0xA5);

// ✅ 推荐：Bundle 字段访问
struct MyBundle : bundle_base<MyBundle> {
    ch_uint<8> data;
    ch_bool valid;
    CH_BUNDLE_FIELDS_T(data, valid)
};

ch::ch_device<TestComponent> dev;
sim.set_input_value(dev.instance().io().bundle.data, 0xAA);
sim.set_input_value(dev.instance().io().bundle.valid, 1);
```

**支持的位宽**:
- 4-bit: `ch_uint<4>`
- 8-bit: `ch_uint<8>`
- 16-bit: `ch_uint<16>`
- 32-bit: `ch_uint<32>`
- 64-bit: `ch_uint<64>`

**注意事项**:
```cpp
// ⚠️ 错误：值超出位宽
ch_uint<8> data(0_d);
sim.set_input_value(data, 0x12345);  // 只有低 8 位 0x45 会被保留

// ✅ 正确：使用合适的位宽
ch_uint<24> data24(0_d);
sim.set_input_value(data24, 0x12345);  // 完整保留
```

---

#### B. ch_in<T> 版本 (Phase 4+)

```cpp
template <typename T>
void set_input_value(const ch::core::ch_in<T>& port, uint64_t value);
```

**使用示例**:

```cpp
class MyComponent : public Component {
public:
    __io(ch_in<ch_uint<8>> data_in; ch_out<ch_uint<8>> data_out;);
    
    void create_ports() override { new (this->io_storage_) io_type; }
    
    void describe() override {
        data_out <<= data_in;  // 简单连接
    }
};

ch::ch_device<MyComponent> dev;
Simulator sim(dev.context());

// 设置输入端口值
sim.set_input_value(dev.instance().io().data_in, 0x42);
```

---

### 2. get_input_value() - 读取信号值

#### A. ch_uint<N> 版本

```cpp
template <unsigned N>
sdata_type get_input_value(const ch_uint<N>& signal) const;
```

**使用示例**:

```cpp
ch_uint<8> data(0_d);
sim.set_input_value(data, 0xA5);

// 读取值
auto val = sim.get_input_value(data);  // 返回 sdata_type

// 转换为整数
uint64_t int_val = static_cast<uint64_t>(val);
REQUIRE(int_val == 0xA5);
```

#### B. ch_in<T> 版本

```cpp
template <typename T>
sdata_type get_input_value(const ch::core::ch_in<T>& port) const;
```

**使用示例**:

```cpp
auto port_val = sim.get_input_value(dev.instance().io().data_in);
REQUIRE(static_cast<uint64_t>(port_val) == 0x42);
```

---

## 🎭 使用场景

### 场景 1: FIFO 测试

```cpp
ch::ch_device<FiFo<ch_uint<8>, 16>> fifo_dev;
Simulator sim(fifo_dev.context());

auto& io = fifo_dev.instance().io();

// 写操作
sim.set_input_value(io.push, 1);     // 写使能
sim.set_input_value(io.din, 0x42);  // 数据
sim.tick();

// 读操作
sim.set_input_value(io.pop, 1);      // 读使能
sim.tick();

// 验证输出
auto dout = sim.get_input_value(io.dout);
auto empty = sim.get_input_value(io.empty);
auto full = sim.get_input_value(io.full);
```

---

### 场景 2: 状态机测试

```cpp
ch::ch_device<TestStateMachine> sm_dev;
Simulator sim(sm_dev.context());

auto& io = sm_dev.instance().io();

// 设置控制信号
sim.set_input_value(io.start, 1);
sim.set_input_value(io.reset, 0);

// 运行多个周期
for (int i = 0; i < 10; i++) {
    sim.tick();
    auto state = sim.get_input_value(io.current_state);
    auto done = sim.get_input_value(io.done);
    
    if (static_cast<uint64_t>(done) == 1) {
        break;
    }
}
```

---

### 场景 3: 位宽适配器测试

```cpp
// 测试不同位宽
ch_uint<4> nibble(0_d);
ch_uint<8> byte(0_d);
ch_uint<32> word(0_d);

Simulator sim(ctx.get());

// 设置不同值
sim.set_input_value(nibble, 0xA);
sim.set_input_value(byte, 0xA5);
sim.set_input_value(word, 0xA5A5A5A5);

// 验证
REQUIRE(sim.get_input_value(nibble) == 0xA);
REQUIRE(sim.get_input_value(byte) == 0xA5);
REQUIRE(sim.get_input_value(word) == 0xA5A5A5A5);
```

---

### 场景 4: AXI 总线测试

```cpp
ch::ch_device<AxiLiteSpi<8, 16>> spi_dev;
Simulator sim(spi_dev.context());

auto& io = spi_dev.instance().io();

// AXI 写事务
sim.set_input_value(io.awaddr, 0x10);    // 地址
sim.set_input_value(io.awprot, 0);       // 保护位
sim.set_input_value(io.awvalid, 1);      // 地址有效

// 等待握手
while (sim.get_input_value(io.awready) == 0) {
    sim.tick();
}

sim.tick();
sim.set_input_value(io.awvalid, 0);  // 撤销
```

---

## ⚠️ 注意事项与最佳实践

### 1. 推荐模式

```cpp
// ✅ 推荐：独立 ch_uint 变量
ch_uint<8> signal(0_d);
sim.set_input_value(signal, value);  // 100% 可靠

// ✅ 推荐：使用 get_input_value 读取
auto val = sim.get_input_value(signal);
```

### 2. Component IO 字段注意事项

```cpp
// ⚠️ 注意：Component IO 字段可能无法访问
sim.set_input_value(dev.io().field, value);  // 可能失败

// 原因：Component IO 在 Simulator 初始化时可能未被追踪
// 解决方案：确保 Component build() 已调用，或使用独立变量
```

### 3. 初始化检查

```cpp
Simulator sim(ctx.get());

// ✅ 推荐：检查初始化状态
if (sim.initialized()) {
    sim.set_input_value(signal, value);
}

// ⚠️ 避免：未检查就使用
// sim.set_input_value(signal, value);  // 可能初始化失败
```

### 4. 位宽匹配

```cpp
// ✅ 推荐：位宽匹配
ch_uint<8> data(0_d);
sim.set_input_value(data, 0xA5);  // 8-bit 值

// ⚠️ 警告：超出位宽
sim.set_input_value(data, 0x12345);  // 只有 0x45 被保留
```

---

## 🔍 故障排除

### 问题 1: "Signal implementation is null"

**症状**:
```
[FATAL] ABORT: Signal implementation is null
```

**原因**:
- 信号未正确初始化
- Simulator 未找到信号的节点 ID

**解决方案**:
```cpp
// ✅ 解决：确保信号已初始化
ch_uint<8> signal(0_d);  // 添加 (0_d) 初始化
Simulator sim(ctx.get());
sim.set_input_value(signal, value);  // 现在可用
```

---

### 问题 2: 读取的值始终为 0

**症状**: 设置的值无法读取

**原因**: Simulator 未 tick() 或信号未连接到 eval_list

**解决方案**:
```cpp
sim.set_input_value(signal, value);
sim.tick();  // 确保运行至少一个 tick
auto val = sim.get_input_value(signal);
```

---

### 问题 3: Component IO 字段访问失败

**症状**: Component 的 IO 字段无法访问

**原因**: Component 未调用 build() 或节点未注册

**解决方案**:
```cpp
// ✅ 方法 1：使用 ch_device（自动调用 build()）
ch::ch_device<MyComponent> dev;  // 构造时自动 build()
Simulator sim(dev.context());

// ✅ 方法 2：手动调用 build()
MyComponent comp(nullptr, "comp");
comp.build();  // 显式调用
Simulator sim2(comp.context());
```

---

## 📊 API 选择指南

```
你需要操作什么类型？
│
├─ ch_uint<N> (独立变量)
│   └─ 使用：set_input_value(get_input_value)
│
├─ ch_in<T> (输入端口)
│   └─ 使用：set_input_value/input_value(port)
│
├─ ch_out<T> (输出端口)
│   └─ 使用：get_value(port)
│
├─ ch_bool
│   └─ 使用：set_value/get_value(signal)
│
└─ Bundle 整体
    └─ 使用：set_bundle_value/get_bundle_value(bundle)
```

---

## 📖 参考资源

### 相关文档

| 文档 | 说明 |
|------|------|
| [T001-simulator-api.md](../developer_guide/api-reference/T001-simulator-api.md) | API 技术规格 |
| [T001-analysis.md](../developer_guide/tech-reports/T001-analysis.md) | 技术分析报告 |
| [simulator-api-quickref.md](../skills/simulator-api-quickref.md) | 速查手册 |

### 代码示例

| 示例 | 位置 |
|------|------|
| test_simulator_bundle.cpp | tests/ |
| test_fifo.cpp | tests/ |
| test_state_machine.cpp | tests/ |

---

**维护**: AI Agent  
**版本**: v2.0  
**下次审查**: 2026-05-09
