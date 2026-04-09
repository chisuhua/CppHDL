# Simulator API 扩展使用指南

**适用版本**: Phase 4 (2026-04-09 后)  
**文档类型**: 开发技能 / API 参考  

---

## 🎯 快速开始

### 新增 API 一览表

| API | 输入类型 | 功能说明 | 状态 |
|-----|---------|---------|------|
| `set_input_value(signal, value)` | `ch_uint<N>` | 设置无符号整数信号值 | ✅ 新增 |
| `get_input_value(signal)` | `ch_uint<N>` | 读取无符号整数信号值 | ✅ 新增 |
| `get_input_value(port)` | `ch_in<T>` | 读取输入端口值 | ✅ 新增 |

---

## 📖 使用场景

### 场景 1: 独立信号操作

```cpp
auto ctx = std::make_unique<ch::core::context>("test_ctx");
ch::core::ctx_swap ctx_guard(ctx.get());

// 创建 ch_uint 变量 (独立信号，非 Component IO)
ch_uint<8> data(0_d);
ch_uint<16> addr(0_d);
ch_uint<32> value(0_d);

Simulator sim(ctx.get());

// ✅ Phase 4 新增：直接设置值
sim.set_input_value(data, 0xA5);
sim.set_input_value(addr, 0x1234);
sim.set_input_value(value, 0xDEADBEEF);

// ✅ Phase 4 新增：便捷读取
auto d = sim.get_input_value(data);   // 返回 0xA5
auto a = sim.get_input_value(addr);   // 返回 0x1234
auto v = sim.get_input_value(value);  // 返回 0xDEADBEEF
```

---

### 场景 2: Bundle 字段访问

```cpp
// 定义 Bundle
struct TestBundle : public bundle_base<TestBundle> {
    ch_uint<8> data;
    ch_bool valid;
    
    TestBundle() = default;
    CH_BUNDLE_FIELDS_T(data, valid)
};

// 创建 Component
class TestComponent : public Component {
public:
    __io(TestBundle bundle_in; TestBundle bundle_out;);
    
    TestComponent(Component *parent = nullptr, const std::string &name = "test")
        : Component(parent, name) {}
    
    void create_ports() override { new (this->io_storage_) io_type; }
    
    void describe() override {
        io().bundle_out.data <<= io().bundle_in.data;
        io().bundle_out.valid <<= io().bundle_in.valid;
    }
};

// ✅ 在 testbench 中使用
ch::ch_device<TestComponent> dev;
Simulator sim(dev.context());

// Phase 4: 可以直接设置 Bundle 字段
sim.set_input_value(dev.instance().io().bundle_in.data, 0xAA);
sim.set_input_value(dev.instance().io().bundle_in.valid, 1);

sim.tick();

// 读取字段值
auto data = sim.get_input_value(dev.instance().io().bundle_out.data);
auto valid = sim.get_input_value(dev.instance().io().bundle_out.valid);
```

---

### 场景 3: 位宽适配器测试

```cpp
// 测试不同位宽的支持
ch_uint<4> nibble(0_d);
ch_uint<8> byte(0_d);
ch_uint<16> half(0_d);
ch_uint<32> word(0_d);
ch_uint<64> dword(0_d);

Simulator sim(ctx.get());

// ✅ 支持所有标准位宽
sim.set_input_value(nibble, 0xA);       // 4-bit
sim.set_input_value(byte, 0xA5);        // 8-bit
sim.set_input_value(half, 0xA5A5);      // 16-bit
sim.set_input_value(word, 0xA5A5A5A5);  // 32-bit
sim.set_input_value(dword, 0xA5A5A5A5A5A5A5A5ULL); // 64-bit

// 验证
REQUIRE(sim.get_input_value(nibble) == 0xA);
REQUIRE(sim.get_input_value(byte) == 0xA5);
REQUIRE(sim.get_input_value(half) == 0xA5A5);
REQUIRE(sim.get_input_value(word) == 0xA5A5A5A5);
```

---

### 场景 4: FIFO 测试

```cpp
ch::ch_device<FiFo<ch_uint<8>, 16>> fifo_dev;
Simulator sim(fifo_dev.context());

auto& io = fifo_dev.instance().io();

// ✅ Phase 4: 可以直接设置 FIFO 控制信号
sim.set_input_value(io.push, 1);    // 写使能
sim.set_input_value(io.din, 0x42);  // 数据输入
sim.set_input_value(io.pop, 0);     // 读使能

sim.tick();

// 读取输出
auto dout = sim.get_input_value(io.dout);
auto full = sim.get_input_value(io.full);
auto empty = sim.get_input_value(io.empty);
```

---

### 场景 5: 状态机测试

```cpp
ch::ch_device<TestStateMachine> sm_dev;
Simulator sim(sm_dev.context());

auto& io = sm_dev.instance().io();

// ✅ 设置控制信号
sim.set_input_value(io.start, 1);
sim.set_input_value(io.reset, 0);
sim.set_input_value(io.clk_en, 1);

sim.tick();

// 读取状态
auto state = sim.get_input_value(io.current_state);
auto done = sim.get_input_value(io.done);
```

---

## ⚠️ 注意事项

### 注意 1: Component IO 字段追踪

**推荐** (保证可用)：
```cpp
// 独立 ch_uint 变量
ch_uint<8> signal(0_d);
sim.set_input_value(signal, value);  // ✅ 100% 可靠
```

**谨慎使用** (需验证)：
```cpp
// Component IO 字段
sim.set_input_value(dev.io().field, value);  // ⚠️ 可能失败
```

**原因**:
- Component IO 字段在 Simulator 初始化时可能未被追踪
- `data_map_` 只包含 eval_list 中的节点

---

### 注意 2: 使用 ch_device 顶层访问

```cpp
// ✅ 推荐：通过 ch_device 访问
ch::ch_device<MyComponent> dev;
Simulator sim(dev.context());

sim.set_input_value(dev.instance().io().field, value);

// ⚠️ 谨慎：直接使用裸 Component
MyComponent comp(nullptr, "comp");
Simulator sim2(comp.context());  // 需确保 comp.build() 已调用
```

---

### 注意 3: 初始化检查

```cpp
Simulator sim(ctx.get());

// ✅ 正确：确保 Simulator 初始化完成
if (sim.initialized()) {
    sim.set_input_value(signal, value);
}

// ⚠️ 错误：未检查初始化状态
// sim.set_input_value(signal, value);  // 可能出错
```

---

## 📊 API 选择指南

### 选择流程图

```
你需要设置/读取什么？
│
├─ ch_uint<N> 变量          → set_input_value/get_input_value
├─ ch_in<T> 输入端口        → set_input_value/get_input_value(port)
├─ ch_out<T> 输出端口       → set_value/get_value
├─ ch_bool 信号             → set_value/get_value
└─ Bundle 整体              → set_bundle_value/get_bundle_value
```

---

### 最佳实践

```cpp
// ✅ 推荐：类型匹配的 API
ch_uint<8> data(0_d);
sim.set_input_value(data, 0xA5);  // 类型匹配

// ✅ 推荐：使用 get_input_value 读取
auto val = sim.get_input_value(data);

// ⚠️ 避免：类型不匹配
ch_bool flag(0_d);
sim.set_input_value(ch_uint<1>, flag);  // ❌ 类型错误

// ⚠️ 避免：混用 API
sim.set_value(data, 0xA5);   // 混用 set_value
sim.get_port_value(data);    // 混用 get_port_value
```

---

## 🔗 参考资源

### 代码示例位置

| 示例 | 文件路径 |
|------|---------|
| T401 基础测试 | `tests/test_simulator_bundle.cpp` |
| FIFO 测试 | `tests/test_fifo.cpp` |
| 状态机测试 | `tests/test_state_machine.cpp` |
| Bundle 测试 | `tests/test_bundle_connection.cpp` |

### 相关文档

| 文档 | 说明 |
|------|------|
| `docs/tech/T401-TECHNICAL-DOC.md` | T401 技术文档 |
| `docs/plans/PHASE4-PLAN-2026-04-09.md` | Phase 4 计划 |
| `include/simulator.h` | Simulator API 实现 |

---

## 📝 更新日志

| 日期 | 版本 | 变更内容 |
|------|------|---------|
| 2026-04-09 | v1.0 | 初始版本，添加 T401 API 支持 |

---

**维护人**: AI Agent  
**下次更新**: 发现新功能或修复时

**技能状态**: ✅ 可用  
**适用项目**: CppHDL Phase 4+
