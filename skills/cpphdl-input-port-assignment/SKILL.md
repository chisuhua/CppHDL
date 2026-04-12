# CppHDL Input Port 赋值 — 正确使用 set_input_value

**技能 ID**: `cpphdl-input-port-assignment`  
**版本**: v1.0  
**创建日期**: 2026-04-12  
**优先级**: 🔴 Critical  
**适用场景**: 所有使用 Component IO 端口的仿真测试

---

## ⚠️ 根因发现

**症状**: 仿真中计数器不工作、状态机不响应、使能信号无效，所有信号值始终为 0。

**根因**: 直接使用输入端口赋值 `io().enable = true` **什么都不做**！

`port<T, input_direction>::operator=` 的实现只打印错误消息，**不会更新模拟器的 data_map_**。

```cpp
// io.h 中 port<T, input_direction> 的 operator= 实现:
template <typename U> void operator=(const U &value) {
    static_assert(is_input_v<input_direction>, "Cannot assign to input port!");
    CHERROR("Cannot assign value to input port!");  // ← 只打印错误，不更新值
}
```

---

## ✅ 正确 API

### 规则：所有 Top 层输入端口，必须使用 `simulator.set_input_value()`

| 操作 | ❌ 错误写法 | ✅ 正确写法 |
|------|-----------|-----------|
| 设置 ch_bool | `device.io().clena = true;` | `sim.set_input_value(device.io().clear, 1);` |
| 设置 ch_uint\<N\> | `device.io().data = 0xAB;` | `sim.set_input_value(device.io().data, 0xAB);` |
| 读取输出 | `auto v = device.io().value;` | `auto v = sim.get_value(device.io().value);` |

### 完整测试模板

```cpp
#include "simulator.h"

int main() {
    ch::ch_device<Top> device;
    ch::Simulator sim(device.context());
    
    // ❌ 错误：直接赋值对 input 端口无效
    device.instance().io().enable = true;
    device.instance().io().clear = false;
    
    // ✅ 正确：使用 set_input_value
    sim.set_input_value(device.instance().io().enable, 1);
    sim.set_input_value(device.instance().io().clear, 0);
    
    // 运行仿真
    sim.tick();
    
    // 读取输出
    auto val = sim.get_value(device.instance().io().value);
    std::cout << "value = " << static_cast<uint64_t>(val) << std::endl;
}
```

---

## 🔍 为什么之前的一些代码能用？

### 情况对比

| 文件 | 使用方式 | 是否有效 | 原因 |
|------|---------|:--------:|------|
| `fifo/stream_fifo_example.cpp` | `sim.set_input_value()` | ✅ | 正确 API |
| `counter/counter.cpp` | `device.io().enable = true` | ❌ | 错误 API（本次修复） |
| `pwm/pwm.cpp` | `sim.set_input_value()` | ✅ | 正确 API |
| `gpio/gpio.cpp` | `sim.set_input_value()` | ✅ | 正确 API |
| **所有 SpinalHDL 移植** | 应统一使用 `sim.set_input_value()` | — | 见下方检查清单 |

### ch_out 端口赋值有效（但仅限输出）

`ch_out<T>` 的 `operator=` 确实可以工作（它会创建赋值节点）。但 `ch_in<T>` 不行。

```cpp
// ✅ ch_out 赋值有效（在 describe() 内部）
io().value = counter_reg;  // 输出端口，可以赋值

// ❌ ch_in 赋值无效（在 testbench 测试代码中）
device.io().enable = true;  // 输入端口，赋值无效！
```

---

## ⚙️ 内部机制

### 数据流路径

```
[Simulator::set_input_value()]
      ↓
  data_map_[node_id] = value
      ↓
  [Simulator::eval_combinational()]
      ↓
  for (input_instr : input_instr_list_)
      input_instr->eval()  //  *dst_ = *src_
      ↓
  [MUX 指令读取 cond 值] → 结果正确
```

### 错误路径（直接赋值）

```
[device.io().enable = true]
      ↓
  port<T, input>::operator=(true)
      ↓
  CHERROR("Cannot assign to input port!")  // ← 只打印错误，返回
      ↓
  data_map_ 未更新  →  MUX 始终读 0 → 计数器不工作
```

---

## 📋 修复检查清单

修复现有测试时，搜索以下模式：

```bash
# 搜索所有直接赋值给 io() 输入端口的代码
grep -rn '\.io()\.\<[a-z_]\+\s*=' examples/ tests/ | grep -v '<<=' | grep -v '='
```

**修复步骤**:
1. 找到 `device.io().<port> = <value>` 模式
2. 替换为 `sim.set_input_value(device.io().<port>, <int_value>)`
3. `true` → `1`, `false` → `0`
4. 编译验证
5. 运行验证输出值

---

## 🔗 相关资源

- [Simulator set_port_value 技能](../cpphdl-simulator-port-value/SKILL.md)
- [AGENTS.md 常见错误 Section 3](../../AGENTS.md#3-常见错误及修复)
- [include/core/io.h](../../include/core/io.h) — port 赋值实现

---

**版本**: v1.0  
**创建时间**: 2026-04-12  
**维护人**: DevMate  
**Bug 调查耗时**: ~45 min + 4 Agent 协作  
**下次更新**: 发现新赋值模式或 API 变更时
