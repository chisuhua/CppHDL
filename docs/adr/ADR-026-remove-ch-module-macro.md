# ADR-026: 移除 CH_MODULE 宏

**状态**: ✅ 已采纳（已执行）
**日期**: 2026-05-08
**决策人**: Sisyphus + 用户

---

## 1. 背景

议题 #20 评估 `CH_MODULE` 宏的去留。AGENTS.md 和 README.md 已标注为反模式，推荐使用 `ch::ch_module<T>`。

### 1.1 宏的缺陷

```cpp
#define CH_MODULE(type, name, ...) ch::ch_module<type> name(#name, ##__VA_ARGS__)

// ❌ 模板参数中的逗号导致预处理器解析错误
CH_MODULE(Counter<8>, counter1);  // 预处理器认为 3 个参数（Counter<8 和 8> 和 counter1）

// ✅ ch::ch_module<T> 无此问题
ch::ch_module<Counter<8>> counter1{"counter1"};
```

### 1.2 使用分布

| 形式 | 使用数 | 说明 |
|------|--------|------|
| `ch::ch_module<T>` | **34 文件** | 事实标准 |
| `CH_MODULE` 宏 | **2 文件** | `counter_simple.cpp`, `uart_tx.cpp` |

---

## 2. 决议

**直接删除宏 + 替换 2 处使用**（已执行）：

| 操作 | 文件 | 旧代码 | 新代码 |
|------|------|--------|--------|
| 删除宏定义 | `include/module.h:99` | `#define CH_MODULE(...)` | 已移除 |
| 替换使用 | `counter_simple.cpp:84` | `CH_MODULE(Counter<8>, counter1);` | `ch::ch_module<Counter<8>> counter1{"counter1"};` |
| 替换使用 | `uart_tx.cpp:169` | `CH_MODULE(UartTx, uart_tx_inst);` | `ch::ch_module<UartTx> uart_tx_inst{"uart_tx_inst"};` |

---

## 3. 变更日志

| 日期 | 版本 | 变更 | 作者 |
|------|------|------|------|
| 2026-05-08 | v1.0 | 初始版本：移除 CH_MODULE 宏，替换 2 处使用 | Sisyphus |

---

**相关链接**:
- `include/module.h` — 宏定义删除位置
- `examples/spinalhdl-ported/counter/counter_simple.cpp:84` — 替换使用
- `examples/spinalhdl-ported/uart/uart_tx.cpp:169` — 替换使用
- `include/AGENTS.md` — 已标注 CH_MODULE 为反模式
- `examples/spinalhdl-ported/README.md` — 已标注 CH_MODULE 为不推荐
- `docs/adr/ADR-DISCUSSION-PLAN.md` — 议题 #20
