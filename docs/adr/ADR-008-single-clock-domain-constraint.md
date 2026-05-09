# ADR-008: 单时钟域约束

**状态**: ✅ 已采纳  
**日期**: 2026-05-06  
**决策人**: Sisyphus + 用户  
**优先级**: P2（影响 Phase 4 规划）

---

## 1. 背景

CppHDL 的时钟系统隐含了**单时钟域 + 单复位域**的假设。当前架构不支持多时钟设计。对于硬件描述语言而言，这是重要的功能限制——SpinalHDL、Chisel、Verilog 都原生支持多时钟域。

本文档记录为什么 CppHDL 当前保持单时钟域约束，以及在什么条件下可以解除该约束。

## 2. 当前设计

### 2.1 时钟表示

时钟（`clockimpl`）和复位（`resetimpl`）是 `lnodeimpl` 的子类，作为普通 1-bit 数据节点存在于 DAG 中：

```cpp
// include/ast/clockimpl.h
class clockimpl : public lnodeimpl {
    bool is_posedge_;  // 上升沿有效
    bool is_negedge_;  // 下降沿有效
};

// include/ast/resetimpl.h
class resetimpl : public lnodeimpl {
    enum reset_type { sync_high, sync_low, async_high, async_low };
};
```

### 2.2 单域假设

```cpp
// include/core/context.h:148-157
clockimpl *current_clock_ = nullptr;     // 当前活跃时钟（唯一）
resetimpl *current_reset_ = nullptr;     // 当前活跃复位（唯一）
clockimpl *default_clock_ = nullptr;     // 默认时钟
resetimpl *default_reset_ = nullptr;     // 默认复位
```

- context 创建时自动生成默认时钟和默认复位
- 所有寄存器在创建时通过 `node_builder.h:177-178` 隐式连接到默认时钟/复位
- 不存在将不同寄存器连接到不同时钟的 API

### 2.3 缺少的基础设施

| 设施 | SpinalHDL | CppHDL | 状态 |
|------|-----------|--------|------|
| ClockDomain 对象 | `ClockDomain(clock, reset, enable)` | ❌ 无 | 缺失 |
| 多域绑定 | `reg.setClockDomain(cd)` | ❌ 无 | 缺失 |
| 同步器原语 | `BufferCC(信号, 2)`（双 Flip-Flop） | ❌ 无 | 缺失 |
| CDC 交叉 | `Crossing` 参数类型 | ❌ 无 | 缺失 |
| 异步 FIFO | `StreamFifoCC(gen, depth, pushCd, popCd)` | ❌ 注释掉 | 禁用 |

## 3. 决策

### D1: CppHDL 当前不支持多时钟域

**决议**: ✅ **不接受多时钟域设计**。CppHDL 当前仅支持单时钟域，所有寄存器共享一个时钟和一个复位。

**理由**:
1. **复杂度太高**：完整的 CDC 支持需要 ClockDomain 对象、同步器原语、格雷码计数器、双时钟 FIFO，预计 3-5 天
2. **优先级不够**：P0/P1 问题（时钟边沿检测、64 位掩码、JIT 有符号算术）尚未修复，不应优先启动 CDC
3. **当前设计不兼容**：`context` 只有单时钟单复位指针，寄存器通过 `default_clock_` 隐式连接。引入多域需重新设计 context 的时钟管理
4. **PRD 定位正确**：PRD Phase 4 将 CDC 规划在稳定性修复和性能优化之后

**影响**:
- 所有设计必须使用单时钟
- async FIFO 保持注释状态
- 不提供跨时钟域同步原语

**恢复条件**（何时可以解除约束）:
1. PRD Phase 1（P0 修复）全部完成
2. PRD Phase 2（JIT 性能优化）全部完成
3. 有明确的 CDC 实现方案（推荐参考 SpinalHDL 的 ClockDomain + StreamFifoCC 架构）
4. 分配了 3-5 天的连续开发时间

### D2: Async FIFO 不启用

**决议**: ✅ Async FIFO 保持注释状态，不提供"伪 CDC"实现。

**理由**:
1. 没有真正 CDC 基础设施的 async FIFO 是**静默错误**——数据跨时钟域时可能随机损坏，用户无法调试
2. 注释掉加明确说明比一个错误运行但结果不可信的实现在工程上更负责任
3. 恢复条件：CDC 基础设施（同步器 + 格雷码计数器 + ClockDomain 对象）就绪后，可基于 SpinalHDL 的 `StreamFifoCC` 模式重新实现

### D3: 明确文档化单时钟域约束

**决议**: ✅ 在以下位置添加明确的"CDC 不支持"标注：
1. `include/core/context.h` — 在 `default_clock_`/`default_reset_` 声明处
2. `include/chlib/fifo.h` — 在 async FIFO 注释块处
3. `AGENTS.md` — 添加到反模式或已知限制部分

---

## 4. 架构影响

| 影响领域 | 说明 |
|---------|------|
| **用户设计** | 所有设计必须使用单时钟。CPU、流处理、FIFO 等都在单时钟下工作 |
| **Async FIFO** | 不可用。需要使用单时钟同步 FIFO（`sync_fifo` / `fwft_fifo`） |
| **未来扩展** | CDC 支持需从 ClockDomain 对象 + 同步器原语开始，逐步实现 |
| **代码生成** | Verilog 代码生成器只输出单时钟模块，无需多时钟逻辑 |
| **测试** | 不需要编写多时钟测试用例 |

---

## 5. 相关文档

- [PRD §3.1 — 能力矩阵](PRD.md) — CDC 标记为 ❌ 缺失
- [PRD §9.4 — Phase 4 路线图](PRD.md) — CDC 在 Phase 4 规划中
- `include/ast/clockimpl.h` — 时钟节点实现
- `include/ast/resetimpl.h` — 复位节点实现
- `include/chlib/fifo.h` — 已注释的 async FIFO

---

## 6. 变更日志

| 日期 | 版本 | 变更 | 作者 |
|------|------|------|------|
| 2026-05-06 | v1.0 | 初始版本，记录单时钟域约束决策和 CDC 不支持 | Sisyphus + 用户 |

---

**决策人**: AI Agent  
**审阅人**: 用户  
**状态**: ✅ 已采纳
