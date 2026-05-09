# ADR-007: 非对称内存所有权模型

**状态**: ✅ 已采纳  
**日期**: 2026-05-06  
**决策人**: Sisyphus + 用户  
**评审记录**: 详见下方讨论过程  

---

## 1. 背景

CppHDL 使用**非对称（asymmetric）内存所有权模型**：context 通过 `unique_ptr` 拥有所有 `lnodeimpl` 节点，但 DAG 边通过裸指针（`srcs_`、`users_`）双向引用。这种设计不同于 `shared_ptr` 自动管理的图结构，也不同于 Arena 分配器的一次性释放。

该模型是隐含的——从未通过 ADR 记录其设计理由、安全边界和约束条件。本文档记录该模型的架构决策、讨论过程和最终决议。

## 2. 当前设计

### 2.1 节点所有权

```cpp
// include/core/context.h:146
std::vector<std::unique_ptr<lnodeimpl>> node_storage_;
```

- context 是每个 `lnodeimpl` 的**唯一所有者**
- 节点按创建顺序存储在 `vector<unique_ptr>` 中
- context 析构时：`clear_sources()` → 反向迭代 `reset()` → `clear()`

### 2.2 DAG 边

```cpp
// include/core/lnodeimpl.h:207-208
std::vector<lnodeimpl *> srcs_;   // 指向输入的源节点
std::vector<lnodeimpl *> users_;  // 指向输出目标节点
```

- `add_src()` 同时建立 `srcs_` 和反向的 `users_`
- `set_src()` 更新源时维护旧关系的移除
- **无引用计数**、无 `shared_ptr`、无侵入式指针

### 2.3 安全保障

| 保护机制 | 位置 | 状态 |
|---------|------|------|
| `Simulator::disconnect()` | `simulator.cpp:284-314` | 活跃 — 清除 `ctx_` 和所有缓存 |
| `destruction_manager` 注册 | `context.cpp` 注释块 | 已移除（见 D1） |
| `disconnected_` 标志守卫 | ~15 处检查 | 活跃 |

---

## 3. 决策过程

### D1: `destruction_manager` 的去留

**问题**: `include/utils/destruction_manager.h` 定义了单例，用于在 `atexit` 时按依赖顺序清理。但注册/注销代码在 `context.cpp` 中被注释掉，同时存在两个不连接的 `in_static_destruction()` 函数。

**讨论过程**:

| 方案 | 描述 | 工作量 | 风险 |
|------|------|--------|------|
| A: 修复合用 | 取消注释注册，统一 `in_static_destruction()` | 2-4h | 中 — 静态析构顺序平台相关 |
| B: 彻底移除 | 删除 `destruction_manager`，只依赖 RAII 顺序 | 1h | 低 — 简化设计 |
| C: 简化保留 | 保留但仅用于 debug 断言 | 1h | 低 |

**决议**: ✅ **方案 B — 彻底移除**

**理由**:
1. CppHDL 已有清晰的 RAII 生命周期模式（`ctx_swap`、`ch_device` 析构顺序）
2. `destruction_manager` 的注册代码已被注释掉，说明实际未使用
3. `atexit` 处理器在不同编译器/平台上的行为不一致
4. 两套 `in_static_destruction()` 函数的不一致性本身就是 bug 来源

**执行**:
- 删除 `include/utils/destruction_manager.h`
- 移除 `context.cpp` 中所有相关的注释代码
- 统一 `in_static_destruction()` 定义（保留 `logger.h` 版本）
- 完全依赖 RAII 析构顺序

---

### D2: `clear_sources()` 是否应同时清理 `users_`

**问题**: context 析构时（`context.cpp:122-137`），`clear_sources()` 只清空了 `srcs_`，没有清理 `users_`。这意味着节点 B 的 `users_` 中可能包含指向已释放节点 A 的野指针。

**讨论过程**:

| 方案 | 描述 | 影响 |
|------|------|------|
| A: 保持现状 | 只清 `srcs_`，信任 `users_` 在析构期间不会被访问 | 零改动 |
| B: 同时清 `users_` | `clear_sources()` 中也清空 `users_` | 5 分钟修改 |
| C: 加断言 | 在关键路径加 `destructing_` 断言 | 1h |

**决议**: ✅ **方案 B — 同时清理 `users_`**

**理由**:
1. 虽然当前析构时无人遍历 `users_`，但 `clone()` 和 `set_src()` 路径在特定条件下可能访问
2. 修改成本极低（5 分钟），提供了防御性安全
3. `clear_sources()` 的语义从"清理源引用"扩展为"清理所有 DAG 引用"，语义更完整

**执行**:
- `lnodeimpl.h:197`: `clear_sources()` 中同时调用 `srcs_.clear()` 和 `users_.clear()`

---

### D3: "Simulator 生命周期短于 context" 作为硬约束

**问题**: 当前设计假定 Simulator 的生命周期总是短于 context。如果用户手动提前销毁 context 或出现异常路径，裸指针可能变成野指针。

**讨论过程**:

| 方案 | 描述 | 影响 |
|------|------|------|
| A: 接受为硬约束 | 记录该规则作为 API 契约，不添加运行时保护 | RAII 保证 |
| B: 引入 `shared_ptr` | Simulator 通过 `shared_ptr` 持有 context 引用 | 引入原子操作开销 |
| C: 弱引用 + 断言 | 保持裸指针，但在关键路径检查 context 是否存活 | 运行时检查开销 |

**决议**: ✅ **方案 A — 接受为硬约束**

**理由**:
1. `ch_device` 的 RAII 设计天然保证了 `context` 存活时间长于 `Simulator`
2. `disconnect()` 已在 `~Simulator()` 中自动调用，提供了基本保护
3. CppHDL 是单线程的，不存在"另一个线程析构 context"的场景
4. 引入 `shared_ptr` 会削弱所有权模型的清晰性

**约束条件**:
- 用户必须确保 `Simulator` 在 `context` 之前析构
- `ch_device` 的使用方式自动满足该约束
- 在单元测试中直接构造 `context` + `Simulator` 时，必须注意析构顺序

---

### D4: 异常安全要求

**问题**: context/Simulator 在构造期间抛出异常时的状态未定义。

**决议**: 当前不添加特殊处理。C++ 标准保证：构造函数抛出异常时，已构造的成员变量会被正确析构。我们的 `vector<unique_ptr>` 和 `unordered_map` 都是 RAII 类型，会自动处理。

---

## 4. 最终设计的生命周期时序图

```
✅ 正确路径（RAII 保证）:

ch_device<MyDesign> device;       // 1. 构造 context + node_storage_
Simulator sim(device.context());  // 2. Simulator 引用 context（裸指针）
sim.tick();                       // 3. 仿真（通过裸指针访问）
~Simulator();                     // 4. Simulator 析构 → disconnect()（自动）
~ch_device();                     // 5. context 析构 → clear_sources() → 销毁节点

❌ 错误路径（违反约束）:

context* ctx = new context();
Simulator sim(ctx);
delete ctx;                       // 6. context 先销毁 → node_storage_ 释放
sim.tick();                       // 7. use-after-free!（disconnected_ 未保护 instr 内部的裸指针）
```

---

## 5. 架构影响

| 影响领域 | 说明 |
|---------|------|
| **Simulator 实现** | 所有通过 `ctx_` 访问节点的方法必须在入口检查 `disconnected_` |
| **JIT 编译器** | JIT 数据缓冲区（`data_buffer_`）是独立的 `uint64_t*`，**不依赖** context 生命周期 |
| **测试编写** | 测试必须遵循 `ch_device` → `Simulator` 顺序，确保 RAII 析构 |
| **异常安全** | 构造函数抛出异常时，`unique_ptr` 和 `unordered_map` 自动处理 |
| **未来扩展** | 如果要支持 context 热替换或动态重配置，需要重新审视 D3 约束 |

---

## 6. 变更日志

| 日期 | 版本 | 变更 | 作者 |
|------|------|------|------|
| 2026-05-06 | v1.0 | 初始版本，记录讨论过程和四项决策 | Sisyphus + 用户 |

---

**相关链接**:
- `include/core/lnodeimpl.h` — `clear_sources()` 实现
- `include/core/context.h` — `node_storage_` 所有权定义
- `src/simulator.cpp` — `disconnect()` 实现
- `ADR-004` — 数据所有权和生命周期（覆盖 context/Simulator 关系）
- `ADR-005` — 双模式仿真设计（覆盖 JIT 缓冲区独立性）
