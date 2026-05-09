# ADR-017: 双 `in_static_destruction()` 静态析构保护

**状态**: ✅ 已采纳
**日期**: 2026-05-08
**决策人**: Sisyphus + 用户（参考 Oracle 分析 + 多仿真场景评估）

---

## 1. 背景

CppHDL 存在两套完全独立的 `in_static_destruction()` 实现，用于检测程序是否已进入静态析构阶段（static destruction phase），以避免在全局对象析构后访问已销毁的资源（如 `std::cout`/`std::cerr`）。

### 1.1 实现 A：简单静态 bool 标记

```cpp
// include/utils/logger.h:82-89
namespace ch { namespace detail {
inline bool &in_static_destruction() {
    static bool flag = false;   // 普通 bool，非原子
    return flag;
}
inline void set_static_destruction() { in_static_destruction() = true; }
}}
```

**作用**：被所有日志宏（`CHLOG`, `CHWARN`, `CHFATAL_EXCEPTION` 等）和 `scope_exit::~scope_exit()` 用作 guard，在静态析构期间跳过日志输出和回调执行。

### 1.2 实现 B：原子标记（destruction_manager 集成）

```cpp
// include/utils/destruction_manager.h:139
std::atomic<bool> &in_static_destruction_flag() {
    static std::atomic<bool> flag{false};
    return flag;
}
```

通过 `ch::in_static_destruction()` 公开（`destruction_manager.h:153-161`）:
```cpp
inline bool in_static_destruction() {
    return detail::destruction_manager::instance().is_in_static_destruction();
}
```

### 1.3 当前代码路径分布

| 使用方 | 调用形式 | 解析到的实现 |
|--------|---------|------------|
| 13 个日志/检查宏（`CHLOG`, `CHCHECK`, `CHABORT` 等） | `ch::detail::in_static_destruction()` | **实现 A**（logger.h 简单 bool） |
| `log_message()` 函数内 | bare `in_static_destruction()`（在 `ch::detail` 命名空间内） | **实现 A**（ADL 查找优先） |
| `scope_exit::~scope_exit()` | `detail::in_static_destruction()`（在 `ch` 命名空间内） | **实现 A**（`detail::` 前缀解析到 `ch::detail`） |
| `destruction_manager` 的 `register_*`/`unregister_*` 方法 | bare `in_static_destruction()`（在 `ch::detail` 内） | **实现 A** |
| **无人调用** | `ch::in_static_destruction()` | **实现 B**（零活跃调用者） |

### 1.4 关联议题

- **ADR-007 D1**（议题 #1）曾决定 "彻底移除 `destruction_manager`，只依赖 RAII 顺序"，并提议 "保留 `logger.h` 版本的 `in_static_destruction()`"
- 本文档基于多仿真场景的需求，**部分修正 ADR-007 D1 的决议**

---

## 2. 初始分析发现

### 2.1 两个实现完全独立，不同步

| 特性 | 实现 A（logger.h） | 实现 B（destruction_manager.h） |
|------|-------------------|-------------------------------|
| 存储类型 | `static bool`（非原子） | `static std::atomic<bool>` |
| 命名空间 | `ch::detail` | 原子标记在 `ch::detail::destruction_manager` 私有 |
| 设置方式 | `set_static_destruction()` | `~destruction_manager()` 调用 `pre_static_destruction()` |
| 是否被设置？ | ❌ `set_static_destruction()` 在 3 处调用点均被注释掉 | ✅ 析构时自动设置 |
| 活跃调用者？ | ✅ 13 个宏 + 2 个函数 | ❌ 零活跃调用者 |
| 是否同步？ | — | ❌ 两标记独立，从不同步 |

### 2.2 `set_static_destruction()` 从未被调用

```cpp
// src/simulator.cpp:276 — 注释掉
// ch::detail::set_static_destruction();

// samples/destruction.cpp:52 — 注释掉
// ch::detail::set_static_destruction();

// samples/counter.cpp:84 — 注释掉
// ch::detail::set_static_destruction();
```

→ **实现 A 的标记永远为 `false`**。所有 `!ch::detail::in_static_destruction()` 守卫条件永远为 `true`，日志和断言在静态析构期间无保护地执行。

### 2.3 `destruction_manager` 的注册机制从未被调用

- `register_context()` / `unregister_context()` — 调用在 `src/core/context.cpp` 中被注释掉（提交 `a6caf3a`）
- `register_component()` / `unregister_component()` — 同上
- `register_simulator()` / `unregister_simulator()` — 无调用证据
- 三个容器 `contexts_`、`components_`、`simulators_` 在运行时**始终为空**

### 2.4 关键正确性风险

当前代码中，日志宏的 guard 检查 `!ch::detail::in_static_destruction()` 永远为 `true`。如果用户代码在静态析构期间触发 `CHLOG`、`CHFATAL_EXCEPTION` 等宏（例如在全局 `Component`/`Simulator` 对象的析构函数中），将发生**未定义行为**：访问已销毁的 `std::cout`/`std::cerr` 对象（C++ 标准 §[basic.start.term]）。

---

## 3. Oracle 风险分析

### 3.1 三种移除方案的风险矩阵

Oracle 对三种方案进行了交叉风险分析：

| 场景 | 日志宏 | `log_message()` | `scope_exit` | `destruction_manager` | 整体风险 |
|------|--------|---------------|--------------|----------------------|---------|
| **当前状态** | Guard 检查实现 A（永 false） | Guard via ADL（实现 A） | Guard via `detail::`（实现 A） | 死代码容器（永为空） | **低**（但保护不生效） |
| **A: 只移除实现 A** | 🔴 **无保护** | ✅ 经 ADL 解析到实现 B | ✅ 经 `detail::` 解析到实现 B | 死代码容器 | **中** |
| **B: 只移除实现 B** | ✅ 实现 A（永 false） | 🔴 **无保护** | 🔴 **无保护** | 原子标记 + 容器 | **中低** |
| **C: 全移除** | 🔴 **无保护** | 🔴 **无保护** | 🔴 **无保护** | 移除 | **高** |

### 3.2 Oracle 初始推荐（单仿真场景）

**"增强版 Scenario A"** — 置信度 85%：
1. 移除实现 A（简单 bool）和 `set_static_destruction()`
2. 所有 guard 改为调用 `ch::in_static_destruction()`（实现 B 的原子标记）
3. 精简 `destruction_manager`：移除所有 `register_*`/容器代码，只保留原子标记
4. 使静态析构保护**首次真正生效**

---

## 4. 多仿真场景分析

### 4.1 场景描述

用户提出以下使用场景：
- 同一进程中同时运行多个独立的 CppHDL 仿真
- 每个仿真拥有自己的 `ch::context` 和 `ch::Simulator`
- 多个仿真可能运行在不同的线程中
- 由顶层编排系统统一驱动每个仿真

### 4.2 对线程安全的影响

| 实现 | 类型 | 多线程安全？ | 分析 |
|------|------|------------|------|
| **实现 A** — `static bool`（logger.h:83） | 非原子 | ❌ **data race → UB** | 并发读取/写入非原子 bool 是 C++ 未定义行为 |
| **实现 B** — `std::atomic<bool>`（destruction_manager.h:139） | 原子 | ✅ **安全** | `std::atomic<bool>` 提供无锁原子读/写 |
| **`scope_exit::~scope_exit()`**（logger.h:391） | 读取非原子 bool | ❌ 从任意线程调用时，可能与其他线程的写入构成 data race | |

### 4.3 对 destruction_manager 的影响

| 组件 | 当前状态 | 多仿真场景下的评估 | 建议 |
|------|---------|-----------------|------|
| 原子标记 | 静态 `atomic<bool>` | ✅ 进程级标记，适合检测静态析构 | **保留** |
| 注册容器 | 3 个 `unordered_set`，无 `mutex` | ⚠️ 如需激活需加锁，且注册调用已全部注释掉 | **移除** |
| 生命周期协调 | 无活跃功能 | 顶层编排系统更适合负责 | **不扩展** |

### 4.4 对 ADR-007 D1 的重新评估

| 原决议（ADR-007 D1） | 多仿真场景下的修正 |
|---------------------|------------------|
| 彻底移除 `destruction_manager` | **修正**：保留原子标记部分，移除注册代码 |
| 保留 logger.h 版本的 `in_static_destruction()` | **修正**：移除 logger.h 版本的简单 bool，改用原子标记 |
| 完全依赖 RAII 析构顺序 | **保留**：作为基本约束，但增加原子标记作为额外保护 |

**修正理由**：
1. 多仿真并发场景下，非原子 bool 存在 data race，必须用 `std::atomic<bool>`
2. 当前 logger.h 的简单 bool 保护从未生效（`set_static_destruction()` 被注释掉）
3. 原子标记在 `~destruction_manager()` 析构时自动设置，提供真实的保护
4. `destruction_manager` 的注册容器无需保留——它们是死代码，且多仿真生命周期应由编排层管理

### 4.5 多仿真场景的额外约束

| 注意点 | 风险 | 缓解措施 |
|--------|------|---------|
| **日志交织** | 多线程写 `cout`/`cerr` 输出混杂 | 可接受；如需隔离可添加线程 ID 前缀或每仿真日志文件 |
| **进程级 abort** | 一个仿真的 `CHABORT`/`CHFATAL` 杀死所有仿真 | 编排层应使用异常替换 `abort()` 调用 |
| **卸载时机** | `main()` 返回后静析开始，其他仿真线程可能仍在运行 | 编排层必须在 `main()` 返回前 join 所有仿真线程 |
| **标记生效时机** | 原子标记在 `~destruction_manager()` 时才设置 | 编排层应在普通对象析构前完成显式清理 |

---

## 5. 最终决议

### 5.1 决议内容

**采用"多仿真增强版 Scenario A"**：统一到原子标记，精简 destruction_manager。

| 步骤 | 内容 | 原因 |
|------|------|------|
| **1. 移除** | `ch::detail::in_static_destruction()` 和 `set_static_destruction()`（logger.h:82-89） | 非原子 bool 在多仿真并发下是 data race |
| **2. 迁移** | 所有日志宏 guard 从 `ch::detail::in_static_destruction()` → `ch::in_static_destruction()` | 使用线程安全的原子标记 |
| **3. 迁移** | `scope_exit::~scope_exit()` 改为显式调用 `ch::in_static_destruction()` | 消除从任意线程读取非原子 bool 的 UB |
| **4. 精简** | 移除 `destruction_manager` 的 `register_*`/`unregister_*` 方法和容器（`contexts_`、`components_`、`simulators_`） | 死代码清理，从未被调用 |
| **5. 保留** | `destruction_manager` 的 `std::atomic<bool>` 标记 + `~destruction_manager()` 析构时设置 | 提供真实的、线程安全的静态析构保护 |

### 5.2 修正 ADR-007 D1

本文档**部分修正** ADR-007 D1 的以下决议：

| 原决议 | 新决议 | 说明 |
|--------|--------|------|
| 彻底移除 `destruction_manager` | 精简保留（只保留原子标记） | 需要原子标记的线程安全特性 |
| 保留 `logger.h` 版本的 `in_static_destruction()` | 移除 `logger.h` 版本，统一到原子标记 | 多仿真并发需要原子操作 |

其余 ADR-007 D1 决议（RAII 顺序约束、注册代码移除）保持不变。

### 5.3 不采纳的方案及其理由

| 方案 | 不采纳理由 |
|------|-----------|
| **完全移除所有保护** | 静态析构期间 `CHLOG`/`CHFATAL_EXCEPTION` 等宏可能访问已销毁的全局对象（`std::cout`/`std::cerr`），导致 UB |
| **激活 destruction_manager 注册机制** | `register_*` 调用已在提交 `a6caf3a` 中注释掉，恢复需要加 `mutex` 保护容器，与 RAII 约束叠加增加复杂度。多仿真生命周期应由编排层管理 |
| **同时保留两套实现** | 维护两套独立、不同步的实现增加认知复杂度，且简单 bool 版本在多仿真并发下有 data race |

---

## 6. 讨论过程记录

### 6.1 初始分析（Sisyphus）

- 发现两套独立的 `in_static_destruction()` 实现
- 确认 `set_static_destruction()` 从未被调用（3 处注释）
- 确认 `destruction_manager` 注册从未被激活（提交 `a6caf3a` 注释掉）
- 提出 Q1/Q2/Q3 三个决策问题

### 6.2 Oracle 初步分析（单仿真场景）

**输出**：完整风险矩阵 + "增强版 Scenario A" 推荐（置信度 85%）
- 移除实现 A，统一到实现 B 的原子标记
- 精简 `destruction_manager` 为纯标记持有者

### 6.3 多仿真场景约束引入（用户）

用户提出多仿真并发场景后重新评估：
- 发现简单 bool 存在 data race（多线程并发读/写非原子变量）
- `std::atomic<bool>` 在多仿真场景下成为**必要条件**而非可选优化
- 结论与 Oracle 推荐一致，但多仿真场景加强了使用原子标记的理由

### 6.4 Oracle 二次分析（多仿真场景）

**输出**：
- 确认实现 A 在并发场景下是 UB
- 确认实现 B 线程安全
- 推荐 "Option 2 — 统一到原子标记"
- 明确生命周期协调应由编排层负责，不需要完整的 destruction_manager

### 6.5 最终决策（用户 + Sisyphus）

用户确认同意了最终方案。

---

## 7. 执行计划

| 步骤 | 内容 | 涉及文件 | 工作量 |
|------|------|---------|--------|
| 1 | 移除 `ch::detail::in_static_destruction()` 和 `set_static_destruction()` | `logger.h:82-89` | 10 min |
| 2 | 日志宏 guard 迁移：`ch::detail::` → `ch::` | `logger.h:108-250` | 15 min |
| 3 | `scope_exit::~scope_exit()` guard 迁移：`detail::` → `ch::` | `logger.h:391` | 5 min |
| 4 | 移除 `destruction_manager` 的注册方法 + 容器 | `destruction_manager.h:27-84`, `destruction_manager.h:134-136` | 15 min |
| 5 | 移除外部的 `destruction_manager.h` 引用 | `simulator.h:13`, `component.cpp:5`, `context.h:20`, `samples/destruction.cpp:6` | 10 min |
| 6 | 清理注释掉的旧调用 | `simulator.cpp:275-288`, `samples/destruction.cpp:49-52`, `samples/counter.cpp:84` | 10 min |
| 7 | 更新 `ADR-DISCUSSION-PLAN.md` | — | 5 min |

**注意**：此执行计划为讨论阶段识别，代码修改仅在用户明确要求时启动。

---

## 8. 变更日志

| 日期 | 版本 | 变更 | 作者 |
|------|------|------|------|
| 2026-05-08 | v1.0 | 初始版本，记录双 `in_static_destruction()` 分析、Oracle 风险分析、多仿真场景评估和最终决议 | Sisyphus + 用户 |

---

**相关链接**:
- `include/utils/logger.h:82-89` — 实现 A（简单 bool，将移除）
- `include/utils/logger.h:106-257` — 日志/检查宏（将迁移 guard）
- `include/utils/logger.h:380-401` — `scope_exit` 类（将迁移 guard）
- `include/utils/destruction_manager.h:19-143` — destruction_manager 类（将精简）
- `include/utils/destruction_manager.h:153-161` — 实现 B（原子标记，将保留）
- `src/simulator.cpp:275-288` — 注释掉的旧调用
- `src/core/context.cpp` — 注释掉的注册调用（提交 `a6caf3a`）
- `docs/adr/ADR-007-memory-ownership-model.md` — 原决议（D1 被本文档部分修正）
- `docs/adr/ADR-DISCUSSION-PLAN.md` — 议题 #12
