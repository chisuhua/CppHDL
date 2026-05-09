# ADR-018: 多仿真并发架构

**状态**: ✅ 已采纳
**日期**: 2026-05-08
**决策人**: Sisyphus + 用户

---

## 1. 背景

### 1.1 场景定义

CppHDL 需要支持以下使用场景：

> 同一进程中同时运行多个独立的 CppHDL 仿真。每个仿真拥有自己的 `ch::context` 和 `ch::Simulator`，可能运行在不同的线程中。由顶层编排系统统一驱动每个仿真。

### 1.2 关键术语

| 术语 | 定义 |
|------|------|
| **仿真实例** | 一对 `(context, Simulator)` 构成一个独立的仿真 |
| **编排层** | 用户代码，负责创建、启动、监控和销毁多个仿真实例 |
| **线程安全** | C++ 标准定义：多个线程同时访问共享数据时，不会导致未定义行为 |
| **隔离性** | 一个仿真的状态变化不影响其他仿真 |

### 1.3 关联 ADR

本文档汇总了先前多个 ADR 中涉及多仿真/线程安全的分析，并补充了跨领域的整体评估：

| ADR | 关联内容 | 结论 |
|-----|---------|------|
| ADR-011 | zero/ones_cache 线程安全 | ≤64 无缓存 + >64 thread_local |
| ADR-017 | 双 `in_static_destruction()` — 线程安全 | 统一到原子标记 |
| ADR-012 | IO 端口双 API — `thread_local ctx_curr_` | 多仿真下已验证兼容 |
| ADR-007 D3 | Simulator 生命周期短于 context | 硬约束，多仿真下同 |
| ADR-006 | 错误处理策略 | 多仿真需将 abort 替换为异常 |

---

## 2. 线程安全审计

### 2.1 当前 `thread_local` 分布

代码库中共有 3 处活跃的 `thread_local` 变量和 2 处注释掉的：

| 变量 | 位置 | 用途 | 多仿真是否安全？ |
|------|------|------|----------------|
| `ctx_curr_` | `context.cpp:16` | 当前线程的活跃 context | ✅ 每线程独立 |
| `Component::current_` | `component.h:93` | 当前正在 describe 的组件 | ✅ 每线程独立 |
| `conditional_block::active_block_` | `if_stmt.h:39` | if/switch 语句的当前作用域 | ✅ 每线程独立 |
| `conditional_block::current_branch_idx_` | `if_stmt.h:40` | if/switch 当前分支索引 | ✅ 每线程独立 |
| `zero_cache` | `types.h:176` | 零值缓存（thread_local 被注释掉） | ⚠️ ADR-011 已记录 |
| `ones_cache` | `types.cpp:446` | 全 1 值缓存（thread_local 被注释掉） | ⚠️ ADR-011 已记录 |

### 2.2 全局/静态共享状态审计

| 状态 | 位置 | 类型 | 多线程安全？ | 分析 |
|------|------|------|------------|------|
| `in_static_destruction_flag()` | `destruction_manager.h:139` | `static std::atomic<bool>` | ✅ | 原子操作，无锁 |
| `destruction_manager::instance()` | `destruction_manager.h:22` | Meyer's 单例 | ✅ | C++11 起静态局部初始化线程安全 |
| `std::cout`/`std::cerr` | 运行时库 | 全局流对象 | ⚠️ | 单个字符写线程安全，但行输出会交织 |
| `logger.h` 的日志宏 | `logger.h:106-257` | 无静态状态 | ✅ | 宏内局部变量，无共享 |
| `zero_cache`/`ones_cache` | `types.h:176`, `types.cpp:446` | 非线程安全容器 | ❌ | 无 `thread_local`，无互斥量 |
| Simulator 的 `data_map_` | `simulator.h` | 实例成员 | ✅ | 每仿真实例独立 |
| context 的 `node_storage_` | `context.h:146` | 实例成员 | ✅ | 每仿真实例独立 |

### 2.3 关键 API 的线程安全分析

#### `ch::core::ctx_swap`

```cpp
// context.cpp:24-41
ctx_swap::ctx_swap(context *ctx) : old_ctx(ctx_curr_) { ctx_curr_ = ctx; }
ctx_swap::~ctx_swap() { ctx_curr_ = old_ctx; }
```

**安全性**: 读写 `thread_local ctx_curr_`，不涉及共享数据。线程 A 切换 `ctx_curr_ = ctx1` 不会影响线程 B 的 `ctx_curr_`。

**单线程构造多仿真**: 如果编排系统在单线程中连续构造多个仿真，`ctx_swap` 正确保存/恢复旧 context。

#### `Component::current()`

```cpp
// component.h:39,93
static thread_local Component* current_;
static Component* current() { return current_; }
```

**安全性**: 同样是 `thread_local`，每线程独立。

#### `Simulator::tick()`

```cpp
// simulator.cpp
void Simulator::tick() {
    // 通过 ctx_ 裸指针访问 context
    // ctx_ 是 Simulator 的实例成员，不会跨线程共享
}
```

**安全性**: `Simulator` 无静态状态，所有数据成员是实例级的。裸指针 `ctx_` 指向的 `context` 也是每仿真实例独立的。前提是编排层遵守 ADR-007 D3（Simulator 生命周期短于 context）。

### 2.4 未受保护的共享状态

#### `zero_cache` / `ones_cache`（ADR-011 待实施）

```cpp
// types.h:176 — thread_local 被注释掉
static /*thread_local*/ std::unordered_map<uint32_t, sdata_type> zero_cache;
```

**风险**: 如果多仿真并发创建 `ch_uint<0>` 或 `ch_uint<-1>` 类型，`zero_cache` 和 `ones_cache` 会存在数据竞争。ADR-011 已决议采用混合策略（≤64 无缓存 + >64 thread_local），但尚未实施。

**缓解**: 此风险影响面小（仅 `ch_uint<0>` 和 `ch_uint<-1>` 两个特例），不影响主流仿真。

---

## 3. 架构保证

### 3.1 多仿真场景下的隔离性保证

| 层面 | 保证 | 机制 |
|------|------|------|
| **DAG 节点隔离** | ✅ 每个仿真的节点互相不可见 | `node_storage_` 是 context 实例成员 |
| **仿真数据隔离** | ✅ 每个仿真的运行时数据独立 | `data_map_` 是 Simulator 实例成员 |
| **IO 端口隔离** | ✅ 每个组件的端口独立 | `io_storage_` 是组件实例成员 |
| **上下文隔离** | ✅ 同时构造多个仿真不会乱 | `thread_local ctx_curr_` |
| **日志交织** | ⚠️ 输出混杂但不崩溃 | `std::cout`/`std::cerr` 是进程级共享 |

### 3.2 多仿真场景下的约束（同单仿真）

以下约束在多仿真场景下保持不变：

1. **RAII 顺序**（ADR-007 D3）: Simulator 生命周期必须短于 context
2. **两阶段初始化**（ADR-012 §3.1）: 端口必须在 `build()` 之后（`ctx_curr_` 有效时）构造
3. **单时钟域**（ADR-008）: 每个仿真仍是单时钟域

### 3.3 编排层职责

为了安全支持多仿真，编排层（用户代码）必须：

```
[编排层]
  ├── 创建 context A, Simulator A  →  线程 A: tick()
  ├── 创建 context B, Simulator B  →  线程 B: tick()
  ├── join 所有线程
  ├── 析构所有 Simulator（按创建顺序逆序）
  ├── 析构所有 context（按创建顺序逆序）
  └── 返回 main()
```

**必须遵守的规则**:
1. `main()` 返回前 join 所有仿真线程
2. Simulator 在 context 之前析构（RAII 顺序）
3. 同一线程内连续构造多个仿真时，`ctx_swap` 会自动处理

---

## 4. 跨 ADR 总结

### 4.1 多仿真场景下各议题的状态

| 议题 | 关联 ADR | 原决策 | 多仿真影响 | 是否需要修正？ |
|------|---------|--------|-----------|--------------|
| #1 非对称内存模型 | ADR-007 D1-D4 | RAII 顺序约束 | 验证正确性（`thread_local` 天然隔离） | ❌ 不修正 |
| #2 data_map_ 共享状态 | ADR-007 §6 | 改为 `vector<sdata_type>` | 无关（实例成员） | ❌ 不修正 |
| #3 单时钟域 | ADR-008 | CDC 不支持 | 每个仿真仍是单时钟域 | ❌ 不修正 |
| #4 仿真评估顺序 | ADR-009 | 边沿触发 | 无关（实例状态） | ❌ 不修正 |
| #5 `<<=` 连接语义 | ADR-010 | 统一 set_src | 无关（DAG 操作） | ❌ 不修正 |
| #6 zero/ones 线程安全 | ADR-011 | 混合策略 | **唯一活跃的数据竞争点** | ❌ 决议已覆盖 |
| #7 IO 端口双 API | ADR-012 | 延期 | 验证了 `thread_local ctx_curr_` 的隔离性 | ❌ 维持延期 |
| #8 类型层次设计 | ADR-013 | ch_bool 独立 | 无关 | ❌ 不修正 |
| #9 Bundle 反射系统 | ADR-014 | 10 字段限制修复 | 无关 | ❌ 不修正 |
| #10 拓扑排序单一 IR | ADR-015 | 保持单一 IR | 无关（context 实例成员） | ❌ 不修正 |
| #11 ch_reg 继承设计 | ADR-016 | 保持继承 | 无关 | ❌ 不修正 |
| #12 双 in_static_destruction() | ADR-017 | 统一原子标记 | 验证了原子标记必要 | ✅ 已修正 |

### 4.2 多仿真场景识别的风险项

| 风险 | 严重性 | 状态 | 缓解措施 |
|------|--------|------|---------|
| `zero_cache`/`ones_cache` 数据竞争 | 低（影响面小） | ADR-011 已决议未实施 | 实现 ADR-011 的混合策略 |
| `CHFATAL`/`CHABORT` 杀所有仿真 | 中 | 待处理 | 编排层应捕获异常替换 abort |
| 日志输出交织 | 低 | 可接受 | 如需隔离添加线程 ID 前缀 |
| 编排层违反 RAII 顺序 | 高 | 用户责任 | `ch_device` 封装自动保证 |
| Simulator 的 `ctx_` 裸指针野指针 | 高 | ADR-007 D3 硬约束 | `disconnect()` 提供基本保护 |

### 4.3 议题 #7 维持延期的理由

经多仿真场景评估，议题 #7（IO 端口双 API）的延期决定维持不变：

1. `thread_local ctx_curr_`（`context.cpp:16`）天然提供了多仿真的上下文隔离
2. `io_storage_` 是每个组件实例的成员变量，不存在跨仿真共享
3. `reinterpret_cast` UB 在实际中是 `char*` 别名的最安全形式，多仿真不增加风险
4. ~100 个文件的迁移成本在当前阶段不合理

**下次重新评估触发条件**:
- T002（Verilog 字段名捕获）成为硬需求
- 新增编译器/平台不再支持 `char*` 别名模式
- 出现新的架构需求使某一方案明显更优

---

## 5. 编排层设计建议

### 5.1 推荐的编排模式

```cpp
#include "ch.hpp"
#include "simulator.h"

// 每个仿真在独立线程中运行
void run_simulation(ch::ch_context& ctx, ch::Simulator& sim) {
    for (int cycle = 0; cycle < 100; ++cycle) {
        try {
            sim.tick();
        } catch (const std::exception& e) {
            // 编排层捕获异常，不影响其他仿真
            std::cerr << "Simulation error: " << e.what() << std::endl;
            break;
        }
    }
}

int main() {
    // 创建仿真实例
    ch::ch_device<MyDesign> device1;
    ch::Simulator sim1(device1.context());

    ch::ch_device<MyDesign> device2;
    ch::Simulator sim2(device2.context());

    // 多线程并发仿真
    std::thread t1(run_simulation, std::ref(device1.context()), std::ref(sim1));
    std::thread t2(run_simulation, std::ref(device2.context()), std::ref(sim2));

    // 等待所有仿真完成
    t1.join();
    t2.join();

    // Simulator 析构 → disconnect()（自动）
    // context 析构 → 释放节点（自动，通过 ch_device RAII）
    return 0;
}
```

### 5.2 线程安全检查清单

| 检查项 | 说明 |
|--------|------|
| ✅ 每个仿真有自己的 `context` | 基础要求 |
| ✅ 每个 `Simulator` 只访问自己的 `context` | 无交叉访问 |
| ✅ `main()` 返回前 join 所有线程 | 避免静析期间线程仍在运行 |
| ✅ Simulator 在 context 之前析构 | RAII 顺序约束 |
| ✅ 不使用全局 `ch::Component`/`ch::Simulator` 对象 | 避免静析 UB |

---

## 6. 变更日志

| 日期 | 版本 | 变更 | 作者 |
|------|------|------|------|
| 2026-05-08 | v1.0 | 初始版本，汇总多仿真场景的架构分析和线程安全审计 | Sisyphus + 用户 |

---

**相关链接**:
- `src/core/context.cpp:16` — `thread_local context *ctx_curr_`
- `include/core/context.h:24-34` — `thread_local` 设计理由
- `include/component.h:39-93` — `thread_local Component::current_`
- `include/chlib/if_stmt.h:39-40` — `thread_local` 作用域管理
- `include/core/types.h:176` — `zero_cache`（`thread_local` 被注释掉）
- `src/utils/types.cpp:446` — `ones_cache`（`thread_local` 被注释掉）
- `include/utils/destruction_manager.h:139` — `std::atomic<bool>` 静析标记
- `docs/adr/ADR-007-memory-ownership-model.md` — RAII 顺序约束
- `docs/adr/ADR-011-zero-ones-thread-safety.md` — zero/ones 线程安全决议
- `docs/adr/ADR-012-io-port-dual-api.md` — IO 端口多仿真兼容
- `docs/adr/ADR-017-dual-in-static-destruction.md` — 原子标记统一
- `docs/adr/ADR-DISCUSSION-PLAN.md` — 讨论计划
