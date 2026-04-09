# Simulator API 速查表

**文档类型**: 快速参考技能  
**适用版本**: Phase 4+  
**最后更新**: 2026-04-09

---

## 🚀 快速参考

### 设置值

```cpp
ch_uint<N> signal(0_d);
sim.set_input_value(signal, value);  // 通用 API
```

### 读取值

```cpp
auto val = sim.get_input_value(signal);  // ch_uint<N>
auto port = sim.get_input_value(input_port);  // ch_in<T>
```

---

## 📝 完整示例

```cpp
#include "ch.hpp"
#include "simulator.h"

using namespace ch::core;

TEST_CASE("QuickRef - Simulator API") {
    auto ctx = std::make_unique<ch::core::context>("test");
    ch::core::ctx_swap ctx_guard(ctx.get());
    
    ch_uint<8> data(0_d);
    Simulator sim(ctx.get());
    
    // 设置值
    sim.set_input_value(data, 0xA5);
    
    // 读取值
    REQUIRE(sim.get_input_value(data) == 0xA5);
}
```

---

## ⚡ 位宽参考

| 类型 | 位宽 | 范围 |
|------|------|------|
| `ch_uint<4>` | 4-bit | 0-15 |
| `ch_uint<8>` | 8-bit | 0-255 |
| `ch_uint<16>` | 16-bit | 0-65535 |
| `ch_uint<32>` | 32-bit | 0-4294967295 |

---

## ⚠️ 注意事项

1. **初始化检查**: 确保 `sim.initialized()` 为 true
2. **位宽匹配**: 值不应超过信号位宽
3. **Component IO**: 可能无法直接访问，优先使用独立变量

---

**完整文档**: [usage_guide/04-simulator-api.md](../usage_guide/04-simulator-api.md)
