# ADR-004: 数据所有权和生命周期管理

**状态**: 草稿（待评审）  
**日期**: 2026-05-06  
**决策人**: QoderWork Architecture Review  
**优先级**: P0  
**相关文档**: `AGENTS.md` (COMPONENT LIFECYCLE PATTERNS), `include/core/context.h`, `include/simulator.h`, `docs/adr/ADR-003-jit-compiler-architecture.md`

---

## 1. 背景

CppHDL 的组件生命周期管理是开发中最常踩的坑。项目中存在三种不同的组件包装器（`ch_device`、`ch_module`、`CH_MODULE`），每种有不同的生命周期和所有权语义。同时，`Simulator` 和 `context` 之间的所有权关系也不清晰，导致多次 SIGSEGV 和内存泄漏问题。

本文档记录当前的所有权规则、生命周期管理策略，以及为什么采用这些设计。

---

## 2. 架构决策

### D1: 三种组件包装器的使用场景

**决策**: 根据使用场景区分三种组件包装器：

| 包装器 | 使用场景 | 所有权 | 生命周期 |
|--------|---------|--------|---------|
| `ch::ch_device<T>` | **独立测试**、顶层设备 | 拥有组件 | 构造时创建 context，析构时销毁 |
| `ch::ch_module<T>` | **Component::describe()** 内部 | 属于父 Component | 依赖父 Component 存活 |
| `CH_MODULE` | **旧代码兼容**（不推荐） | 属于父 Component | 同上，但不支持模板 |

**理由**:
- **`ch_device`** 适合独立测试场景，因为它自包含 context 和管理自己的生命周期
- **`ch_module`** 适合在 Component 层次结构中嵌套使用，它需要在 `Component::describe()` 中创建，依赖 `Component::current()` 返回有效的父组件
- **`CH_MODULE`** 是遗留宏，不支持模板参数，应该逐渐淘汰

**CRITICAL 规则**:
```cpp
// ✅ CORRECT: 独立测试使用 ch_device
TEST_CASE("HazardUnit test", "[hazard]") {
    ch::ch_device<HazardUnit> hazard;  // 自包含 context
    ch::Simulator sim(hazard.context());
    sim.set_input_value(hazard.instance().io().rs1_data_raw, 0);
    sim.tick();
}

// ✅ CORRECT: ch_module 在 Component::describe() 内部使用
class MyTop : public ch::Component {
    void describe() override {
        ch::ch_module<HazardUnit> hazard{"hazard"};  // 有父 Component
        // ...
    }
};

// ❌ WRONG: ch_module 在独立测试中使用 - SIGSEGV
TEST_CASE("Bad pattern", "[bad]") {
    ch::ch_module<HazardUnit> hazard{"hazard"};  // 崩溃 - 没有父 Component
    // "Child component has been destroyed unexpectedly in io()!"
}
```

### D2: context 所有权采用 thread_local + ctx_swap 模式

**决策**: `context` 通过 `thread_local ctx_curr_` 全局指针和 `ctx_swap` RAII 守卫管理。

**理由**:
1. **线程隔离性**: 每个线程都有独立的变量副本，互不干扰
2. **上下文切换**: 在线程内串行执行不同的 context，`thread_local` 能正确维护当前状态
3. **性能优势**: 无需锁机制，访问速度快
4. **简化设计**: 避免了复杂的线程同步代码

**生命周期**:
```
context ctx("name")           // 1. 创建 context
ctx_swap swap(&ctx)           // 2. 切换到当前 context（RAII）
  // 在此构建 HDL 图...       // 3. 所有节点创建在当前 context
Simulator sim(&ctx)           // 4. Simulator 引用 context
sim.tick()                    // 5. 执行仿真
                              // 6. swap 析构，恢复旧 context
                              // 7. ctx 析构，销毁所有节点
```

**CRITICAL 规则**:
- 在创建任何 HDL 节点（`ch_logic`、`ch_uint`、`ch_reg` 等）之前，必须有活跃的 `ctx_swap`
- 没有 `ctx_swap` 时创建的节点会挂在错误的 context 上，导致仿真时 segfault
- `context` 必须在 `Simulator` 之前销毁（通过 RAII 顺序保证）

### D3: Simulator 使用裸指针引用 context，必须实现 disconnect() 防止 use-after-free

> **说明**: 本决策合并了原 D3（不拥有所有权）和 D4（disconnect 保护），两者是同一设计选择的两个方面。

**决策**: `Simulator` 构造函数接受 `context*`（裸指针），不拥有 context 的所有权。同时提供 `disconnect()` 方法，在 context 销毁前断开所有引用。

**理由**:
- **所有权清晰**: context 的生命周期由 `ch_device` 管理，Simulator 只是消费者
- **双重释放风险**: 使用 `std::unique_ptr<context>` 会导致 Simulator 和 device 都尝试销毁
- **use-after-free 防护**: Simulator 保存了 `ctx_` 指针和 `eval_list_` 副本，如果 context 先于 Simulator 销毁，访问这些指针会导致崩溃

**实现**:
```cpp
// Simulator 构造函数 — 裸指针，不拥有
explicit Simulator(ch::core::context* ctx) : ctx_(ctx) {
    // 只保存指针，不接管所有权
}

// disconnect() — 在 context 销毁前调用
void Simulator::disconnect() {
    disconnected_ = true;
    ctx_ = nullptr;
    eval_list_.clear();
    instr_map_.clear();
    instr_cache_.clear();
    data_map_.clear();
}
```

**CRITICAL 规则**:
```cpp
// ✅ CORRECT: context 由 device 管理，Simulator 只引用
ch::ch_device<MyDesign> device;
ch::Simulator sim(device.context());  // 裸指针引用
sim.tick();
// Simulator 先析构（自动或在 ~Simulator 中调用 disconnect()）
// device 再析构，销毁 context

// ❌ WRONG: Simulator 不应该拥有 context
ch::Simulator sim(std::make_unique<ch::core::context>());  // 双重释放风险

// ❌ WRONG: context 先于 Simulator 销毁但未调用 disconnect()
ch::ch_device<MyDesign> device;
ch::Simulator sim(device.context());
device.~ch_device();  // context 被销毁，sim.ctx_ 成为野指针
sim.tick();           // use-after-free! 崩溃
```

**生命周期保证**:
- **最佳实践**: 通过 RAII 顺序保证（Simulator 在 context 之前析构）
- **异常情况**: 如果必须提前销毁 context，先调用 `sim.disconnect()` 再销毁
- **未来改进**: 考虑在 `~Simulator()` 中自动调用 `disconnect()`，降低遗漏风险

### D4: 节点存储采用 unique_ptr 在 context 中

**决策**: `context` 使用 `std::vector<std::unique_ptr<lnodeimpl>>` 存储所有节点。

**理由**:
- context 是节点的**唯一所有者**
- 节点之间通过裸指针相互引用（`src()`、`user()` 关系）
- context 析构时自动销毁所有节点，无需手动管理
- 裸指针引用避免了循环依赖（`unique_ptr` 环会导致内存泄漏）

**所有权图**:
```
context
├── node_storage_ (vector<unique_ptr<lnodeimpl>>)  ← 唯一所有权
│   ├── inputimpl (节点 0)
│   ├── litimpl (节点 1)
│   ├── opimpl (节点 2)  ← src(0) → 节点 0 (裸指针引用)
│   └── regimpl (节点 3) ← src(0) → 节点 2 (裸指针引用)
│
└── default_clock_ (裸指针 → node_storage_ 中的节点)
```

**CRITICAL 规则**:
- 不要手动 `delete` 任何 `lnodeimpl` 指针
- 不要将 `unique_ptr<lnodeimpl>` 移出 `context`
- 节点间的引用（src/user）是裸指针，生命周期由 context 保证

### D5: JIT 数据缓冲区与 Simulator data_map 双写同步

> **说明**: 本节内容已迁移至 ADR-005 作为 D6。此处分阶段保持引用，避免破坏现有交叉引用，但完整内容以 ADR-005 为准。

**决策要点**: JIT 编译后的函数使用独立的 `data_buffer_`（`uint64_t*`），需要通过 `sync_to_buffer()` 和 `sync_from_buffer()` 与 Simulator 的 `data_map_` 同步。

**原因**: JIT 函数需要连续内存布局，而 `data_map_` 是 `unordered_map` 不支持直接指针访问。

**已知问题**: 同步开销是 JIT 性能瓶颈之一（PRD T15 任务）。

→ **完整内容请参见**: [ADR-005 D5: 数据缓冲区同步策略](ADR-005-dual-mode-simulation.md)

---

## 3. 生命周期时序图

### 3.1 独立测试场景（ch_device）

```
main()
├── ch_device<MyDesign> device    // 1. 创建 device + context
│   └── MyDesign::describe()      // 2. 构建 HDL 图
├── Simulator sim(device.context())  // 3. 创建 Simulator（引用 context）
├── sim.tick()                    // 4. 执行仿真
├── ~Simulator()                  // 5. Simulator 析构（调用 disconnect()）
└── ~ch_device()                  // 6. 销毁 context + 所有节点
```

### 3.2 Component 层次结构（ch_module）

```
main()
├── ch_device<TopLevel> top       // 1. 创建设备
│   └── TopLevel::describe()
│       ├── ch_module<SubA> a{"a"}  // 2. 创建子模块（依赖 TopLevel::current()）
│       │   └── SubA::describe()
│       └── ch_module<SubB> b{"b"}  // 3. 创建另一个子模块
│           └── SubB::describe()
├── Simulator sim(top.context())  // 4. 创建 Simulator
├── sim.tick()                    // 5. 执行仿真
├── ~Simulator()                  // 6. Simulator 析构
└── ~ch_device()                  // 7. 销毁 TopLevel context + 所有子模块
```

### 3.3 错误场景（ch_module 在独立测试中使用）

```
TEST_CASE("Bad", "[bad]")
├── ch_module<MyDesign> design{"design"}  // 1. 尝试创建
│   └── Component::current() → nullptr    // 2. 没有父 Component
│       → 静默失败 / SIGSEGV              // 3. 创建失败
└── design.io()  → "Child component has been destroyed unexpectedly!"
```

---

## 4. 已知问题和规则总结

### 4.1 CRITICAL 规则（必须遵守）

| 规则 | 违反后果 | 检查方法 |
|------|---------|---------|
| `ch_module` 只能在 `Component::describe()` 中使用 | SIGSEGV / "Child component destroyed" | 代码审查 |
| 独立测试必须使用 `ch_device` | SIGSEGV | 代码审查 |
| 创建节点前必须有 `ctx_swap` | 节点挂在错误的 context 上 | `ctx_curr_ != nullptr` 检查 |
| `Simulator` 不能拥有 `context` | 双重释放 | 类型审查（`context*` vs `unique_ptr`） |
| context 销毁前必须 `disconnect()` | use-after-free | RAII 顺序或手动调用 |
| 不要手动 `delete lnodeimpl` | 双重释放 | 代码审查 |
| 不要将 `unique_ptr<lnodeimpl>` 移出 context | 生命周期混乱 | 代码审查 |

### 4.2 技术债务

| 债务 | 影响 | 修复计划 | 参考 |
|------|------|---------|------|
| `CH_MODULE` 宏不支持模板 | 限制组件复用 | 逐渐迁移到 `ch_module<T>` | — |
| 裸指针引用缺乏生命周期标记 | 容易误用 | 考虑引入 `observer_ptr` 或注释标记 | — |
| `disconnect()` 需要手动调用 | 容易被遗漏 | 考虑在 `~Simulator()` 中自动调用 | PRD T2 |
| JIT 双缓冲区同步开销 | 抵消 JIT 优势 | PRD T15: 统一内存布局 | ADR-005 D5 |

---

## 5. 决策影响

### 对开发者的影响

1. **测试编写**: 独立测试函数必须使用 `ch_device<T>`，不能使用 `ch_module<T>`
2. **组件层次结构**: 嵌套组件必须在 `Component::describe()` 中创建
3. **内存管理**: 不要手动管理 `lnodeimpl` 的生命周期，由 context 统一管理
4. **Simulator 使用**: 确保 Simulator 在 context 之前析构（通过 RAII 顺序）

### 对用户的影響

1. **API 一致性**: `ch_device` 和 `ch_module` 有不同的使用模式，需要文档清晰说明
2. **错误信息**: SIGSEGV 通常是因为错误使用了组件包装器，错误信息应更友好
3. **学习曲线**: 需要理解 Component 层次结构和 context 生命周期

---

## 6. 变更日志

| 日期 | 版本 | 变更 | 作者 |
|------|------|------|------|
| 2026-05-06 | v1.0 | 初始版本，记录组件生命周期和所有权规则 | QoderWork |
| 2026-05-06 | v1.1 | 合并 D3/D4，D6 迁移至 ADR-005，添加 PRD 交叉引用 | QoderWork Review |

---

**相关链接**:
- [AGENTS.md - COMPONENT LIFECYCLE PATTERNS](../../AGENTS.md)
- [ADR-003: JIT 编译器架构](ADR-003-jit-compiler-architecture.md)
- [ADR-005: 双模式仿真设计](ADR-005-dual-mode-simulation.md) — D5 数据同步策略（迁出目标）
- [PRD - 已知架构债务](../adr/PRD.md) — P0/P1 债务列表
- `include/core/context.h` — context 和 ctx_swap 定义
- `include/simulator.h` — Simulator 定义
- `docs/developer_guide/patterns/COMPONENT-LIFECYCLE-GUIDE.md` — 完整生命周期指南

---

**决策人**: AI Agent  
**审阅人**: ________________  
**状态**: 草稿（待评审）
