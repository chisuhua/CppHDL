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

## 6. `data_map_` 架构决策

> **关联议题 2**: 本节记录 `data_map_` 作为仿真引擎中心数据存储的设计决策。

### 6.1 背景

`data_map_` 是仿真引擎的核心数据存储，位于 `Simulator` 中。它通过 node_id 索引存储所有仿真节点的运行时值。三个子系统共享该数据：解释器（直接读写）、JIT（通过批量同步）、DAG 代码生成器（拷贝引用）。

### 6.2 当前设计

```cpp
// include/ast/instr_base.h:16
struct data_map_t : public std::unordered_map<uint32_t, ch::core::sdata_type> {};
```

- 所有节点 ID 从 0 开始单调递增（`context.h:147`）
- 初始化时一次性填充（`simulator.cpp:543-587`）
- 指令通过 `create_instruction()` 将 `sdata_type*` 指针存入指令对象，`eval()` 热路径通过指针直接访问

### 6.3 Q1: `unordered_map` 是否应改为 `vector`

**问题**: `unordered_map` 的哈希查找在热点路径（JIT sync、VCD trace、用户 API 调用）引入了不必要的常数开销。初始化时已经建立了 ID 连续递增的假设，但未在容器类型中体现。

**分析过程**:

关键发现之一是**指令执行的热路径（`instr->eval()`）已经使用 `sdata_type*` 指针**，不受容器类型影响。`data_map_` 的实际使用场景是：

| 使用场景 | 频率 | 当前操作 |
|---------|------|---------|
| 初始化赋值 | 1 次 | `data_map_[node_id] = value` |
| `create_instruction()` 获取指针 | 1 次/节点 | `&data_map.at(id())` |
| JIT sync_to/from_buffer | 每次 tick | 遍历整个容器 |
| VCD trace 读值 | 每次 tick | `data_map_.at(signal_id)` |
| get_port_value/set_input_value | 用户调用 | `data_map_.find(node_id)` |

**指针稳定性分析**:
- `create_instruction()` 将 `&data_map_[id()]` 存储为指令成员，所有指令类型都如此（`instr_op.h:21-23`, `instr_reg.h:18-20`, `muximpl.cpp:9-12`）
- `vector` 在 `reserve()` + 不 `push_back` 的前提下的指针稳定
- 当前初始化流程是一次性填充（`simulator.cpp:553-587`），满足此条件

**方案对比**:

| 方案 | 优势 | 劣势 | 工作量 |
|------|------|------|--------|
| A: 保持 `unordered_map` | 灵活性高，支持稀疏 ID | JIT sync 有 O(N) 哈希查找，`sync_from_buffer` 中每槽位调用 `find()` | 0 |
| B: 改用 `vector<sdata_type>` | O(1) 索引访问，消除所有哈希查找；连续内存缓存友好；减少 32 字节/条目开销 | 不支持稀疏 ID（当前不需要）；需确保初始化后不重新分配 | 4-6h |
| C: 混合 `vector` + 边界检查 | 安全 + 性能 | 少量运行时检查 | 5h |

**决议**: ✅ **方案 B — 改用 `vector<sdata_type>`**

**理由**:
1. **安全前提满足**: 节点 ID 从 0 开始连续递增（已验证全代码库），不存在稀疏 ID 场景
2. **指针稳定性**: `initialize()` 中一次性 `resize(max_id+1)`，后续永不重新分配，所有已存储指针稳定
3. **JIT sync 加速**: `sync_from_buffer` 消除 N 次 `find()` 调用（`jit_compiler.cpp:189-194`），`sync_to_buffer` 直接索引
4. **VCD trace 加速**: `data_map_.at(signal_id)` 变为 `data_map_[signal_id]`
5. **内存减少**: 约 32 字节/条目的 map 开销消除，连续内存提升缓存局部性
6. **代码简化**: 消除 ~30 行 `.find() → .at() → .end()` 守卫代码

**实施计划**:

| 步骤 | 内容 | 影响范围 |
|------|------|---------|
| 1 | `data_map_t` 从 `unordered_map` 改为 `vector<sdata_type>`，保留 `using` 别名 | `instr_base.h` |
| 2 | `initialize()` 中 `resize(max_id+1)` 替换 `map` 填充逻辑 | `simulator.cpp:543-587` |
| 3 | `reinitialize()` 中 `assign(max_id+1, sdata_type())` | `simulator.cpp:316-324` |
| 4 | 替换 `&data_map.at(id())` → `&data_map_[id()]` | `ast_nodes.cpp`, `muximpl.cpp` |
| 5 | 替换 `data_map.find(id)` → `data_map_[id]` | `instr_mem.cpp` (11 处) |
| 6 | 简化 `sync_to/from_buffer`，消除 `find()` | `jit_compiler.cpp:174-195` |
| 7 | VCD trace 中的 `.at()` 改为索引 | `simulator.cpp:949-1053` |
| 8 | `disconnect()` 中 `.clear()` 保留 | `simulator.cpp:306` |
| 9 | `codegen_dag.cpp` 中的拷贝参考更新 | `codegen_dag.cpp:146-288` |

**边界条件**:
- `vector<bool>` 特化问题：不相关，我们存储 `sdata_type`
- 拷贝语义：`vector` 拷贝是深拷贝（每个元素拷贝构造），与当前 `unordered_map` 拷贝行为一致
- 异常安全：`resize` 在 `bad_alloc` 时抛出，与当前行为一致

### 6.4 Q2: JIT `sync_to/from_buffer` 批量拷贝策略

**问题**: 当前 JIT 每次 tick 执行 `sync_to_buffer`（遍历 `data_map` 全部 N 个条目写入 `data_buffer_`） + JIT 函数执行 + `sync_from_buffer`（遍历 `data_buffer_` 全部 N 个槽位写回）。每 tick 4 次遍历（comb sync_to + sync_from + seq sync_to + sync_from）。

**分析过程**:

| 方案 | 描述 | 复杂度 | 风险 |
|------|------|--------|------|
| A: 接受当前开销 + Q1 改进 | Q1 `vector` 已消除 N 次 `find()` 哈希查找。保留 4 次全量遍历 | 低 | 大设计 (100000 节点) 可能 ~200μs/tick |
| B: 脏节点追踪 | 只同步 JIT 实际修改的节点 | 中 | JIT 执行覆盖率 >80%，脏追踪开销可能抵消收益 |
| C: 统一内存布局 (PRD T15) | 消除双缓冲，`data_buffer_` 直接指向 `data_map_` | 高 | 全架构改动，独立议题 |

经过量化分析：Q1 `vector` 后 4N 操作在现代 CPU 上对于 1000 节点约 2μs，对于 10000 节点约 20μs。方案 B 的脏追踪在 JIT 覆盖 >80% 节点的场景下收益有限。方案 C 应作为 PRD T15 独立规划。

**决议**: ✅ **方案 A + 轻量快速跳过优化**

接受 O(4N) 的同步开销作为当前阶段的设计约束。在 `set_input_value()` / `set_value()` 入口设置脏标志，允许 `sync_to_buffer` 在无输入变更时跳过拷贝。

**影响**:
- Q1 的 `vector` 改造完成后，`sync_to/from_buffer` 直接使用索引访问（`jit_compiler.cpp:174-195`）
- 增加 `data_map_dirty_` 标志，在 `set_input_value()` 中设为 `true`，`sync_to_buffer` 后设为 `false`
- PRD T15 优先级提升，作为统一内存布局的长期解决方案

### 6.5 Q3: `data_map_` 生命周期绑定

**问题**: `data_map_` 是 `Simulator` 的成员变量。当 context 在 Simulator 活跃期间修改时，或在 `reinitialize()` 重建时，外部代码持有的引用可能失效。

**分析过程**:
- ADR-007 D3 已接受"Simulator 生命周期短于 context"为硬约束
- Q1 将 `data_map_` 改为 `vector`，拷贝语义不变
- `codegen_dag.cpp:146` 通过 `simulator.data_map()` 获取的是**容器拷贝**，不是引用
- 当前没有其他外部代码持有 `data_map_` 的内部指针

**方案对比**:

| 方案 | 描述 | 工作量 |
|------|------|--------|
| A: 保持现状 | `data_map_` 与 Simulator 绑定，依赖 ADR-007 D3 的 RAII 约束 | 0 |
| B: 引用计数 | `shared_ptr<vector<sdata_type>>` | 2h |
| C: 事件回调 | Simulator 销毁时通知持有者 | 3h |

**决议**: ✅ **方案 A — 保持现状**

`data_map_` 的生命周期绑定到 Simulator，由 ADR-007 D3 的 RAII 顺序约束保护。所有外部访问都是拷贝语义或临时引用，不存在野指针风险。

### 6.6 Q4: DAG 代码生成器对 `data_map_` 的耦合

**问题**: `codegen_dag` 通过 `simulator.data_map()` 拷贝 `data_map_` 用于可视化调试（在节点标签中标注运行时值）。这是不必要的类型耦合和数据耦合。

**分析过程**: DAG 用于可视化调试，需要运行时值来标注节点标签中的信号状态。Q1 将 `data_map_` 改为 `vector` 后拷贝成本自然降低（连续内存 vs unordered_map 逐元素），解耦改造的收益进一步减小。

**决议**: ✅ **方案 A — 保持现状**

DAG 可视化调试有明确的用途需求。Q1 的 `vector` 改造已降低拷贝成本，在明确 DAG 代码生成器的长期定位之前，不做额外的解耦工作。

---

## 7. 变更日志

| 日期 | 版本 | 变更 | 作者 |
|------|------|------|------|
| 2026-05-06 | v1.0 | 初始版本，记录讨论过程和四项决策 | Sisyphus + 用户 |
| 2026-05-06 | v1.1 | 新增 §6 `data_map_` 架构决策，记录 Q1 分析和决议 | Sisyphus + 用户 |

---

**相关链接**:
- `include/core/lnodeimpl.h` — `clear_sources()` 实现
- `include/core/context.h` — `node_storage_` 所有权定义
- `src/simulator.cpp` — `disconnect()` 实现
- `ADR-004` — 数据所有权和生命周期（覆盖 context/Simulator 关系）
- `ADR-005` — 双模式仿真设计（覆盖 JIT 缓冲区独立性）
