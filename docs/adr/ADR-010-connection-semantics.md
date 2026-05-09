# ADR-010: `<<=` DAG 连接语义

**状态**: ✅ 已采纳  
**日期**: 2026-05-06  
**决策人**: Sisyphus + 用户  

---

## 1. 背景

`operator<<=` 是 CppHDL 中建立硬件连接的核心操作符。当前有 8 个重载，覆盖 4 种端口方向组合和 4 种非端口连接组合。存在以下问题：
- `set_driver()` vs `set_src()` 两种 DAG 操作方法语义不统一
- 大量使用 `dynamic_cast` 进行运行时分发
- 4 处 `TODO: check the port is module port` 表明验证缺失
- `operator&&` / `operator||` 在端口上的使用被标记为反模式

## 2. Q1: `set_driver()` vs `set_src()` 统一

### 2.1 分析

`set_driver()` 仅在 `inputimpl` 上定义（`ast_nodes.h:157`）：

```cpp
void set_driver(lnodeimpl *drv) {
    driver_ = drv;       // 冗余保存 driver 指针
    add_src(drv);        // 通过 add_src 建立 DAG 边 + 反向引用
}
```

`set_src()` 在基类 `lnodeimpl` 上定义（`lnodeimpl.h:119`）：

```cpp
void set_src(uint32_t index, lnodeimpl *src) {
    srcs_[index] = src;
    src->add_user(this);
}
```

**核心发现**: 两者在 DAG 层面执行的**完全相同的操作**——建立一条从 driver 到 receiver 的有向边。唯一的区别是 `set_driver()` 额外保存了一个 `driver_` 指针（`srcs_[0]` 的冗余副本）。

这是历史遗留设计，`driver_` 字段在当前的 `inputimpl` 指令实现（`instr_input::eval()`）中并未使用——`instr_io.cpp:10` 直接从 `src_` 指针读取值。

### 2.2 8 个重载的成因

8 个重载来自三层语义映射：

| 层次 | 机制 | 说明 |
|------|------|------|
| 编译期方向标签 | `port<T, Dir>` | `input_direction` / `output_direction`，C++ 类型安全 |
| 运行时实现类型 | `inputimpl` / `outputimpl` | 实际节点类型，通过虚函数表确定 |
| DAG 操作 | `set_driver()` / `set_src()` | 建立 DAG 边 |

4 种方向组合对应的实际场景：

```
receiver <<= driver       实际场景
─────────────────────────────────
out      <<= in           内部信号驱动顶层输出（最常见）
in       <<= in           顶层输入穿透到子模块输入
out      <<= out          子模块输出穿透到顶层输出
in       <<= out          内部逻辑输出驱动子模块输入
```

每种方向组合 + 非端口（表达式/字面量）变体 = 8 个重载。

### 2.3 决策

**决议**: ✅ **统一为 `add_src()` / `set_src()` 建立 DAG 边**。

具体方案：
1. 所有方向组合统一使用基类 `set_src()` / `add_src()` 建立 DAG 边
2. `set_driver()` 保留为 `inputimpl` 的兼容方法，实现改为 `add_src(drv)` 的别名
3. `driver_` 字段标记为已废弃但暂不删除（避免破坏现有二进制兼容性）
4. 消除 `dynamic_cast`：`port<T, output_direction>::impl()` 一定是 `outputimpl`，`port<T, input_direction>::impl()` 一定是 `inputimpl`，不需要运行时检查

**理由**:
1. DAG 语义统一：`set_driver()` 和 `set_src()` 做的是同一件事
2. 减少 `dynamic_cast`：编译期方向标签已经提供了足够信息
3. `driver_` 冗余字段未被使用（`instr_input::eval()` 直接从 `src_` 读取）

---

## 3. Q2: `dynamic_cast` 分发

### 3.1 分析

`port` 的编译期方向标签直接确定了运行时实现类型：

| 方向标签 | 实现类型 | 证据 |
|---------|---------|------|
| `port<T, output_direction>` | `outputimpl*` | `io.h:107` — `ch_logic_out::impl()` 返回 `output_node_` |
| `port<T, input_direction>` | `inputimpl*` | `io.h:183` — `ch_logic_in::impl()` 返回 `input_node_` |

当前代码使用 `dynamic_cast` 做运行时检查。在编译期类型已知的前提下，`dynamic_cast` 是冗余的——它不可能失败（除非有 bug 导致方向标签与实现类型不一致）。

### 3.2 决策

**决议**: ✅ **将 `dynamic_cast` 替换为 `static_cast` + `assert`**。

具体方案：
1. 所有 8 个重载中的 `dynamic_cast` 改为 `static_cast`
2. 增加 `assert(receiver_impl != nullptr)` 作为 debug 安全检查
3. release 模式零开销，debug 模式保持防御性

**理由**:
1. 编译期方向标签提供了完整的类型信息
2. `dynamic_cast` 在 release 模式仍有 RTTI 开销（虽然不在热路径上）
3. `static_cast` + `assert` 兼顾安全和性能

---

## 4. Q3: 端口验证 TODO

### 4.1 分析

`io.h` 中存在 4 处 `// TODO: check the port is module port` 注释（第 694、725、753、777 行），表明端口连接的有效性验证缺失。

不同方向组合需要不同的验证规则：

| 场景 | receiver 应在 | driver 应在 |
|------|--------------|------------|
| `out <<= in` | 当前模块 | 内部信号 |
| `in <<= in`（穿透） | 子模块 | 当前模块 |
| `out <<= out`（穿透） | 当前模块 | 子模块 |
| `in <<= out` | 子模块 | 内部逻辑 |

但当前 `port` 没有直接存储所属模块信息。`lnodeimpl::get_parent()` 返回创建该节点的 Component 实例指针，在部分场景下可用于验证。

### 4.2 决策

**决议**: ✅ **用 `CHREQUIRE` 断言替换 TODO 注释**。

具体实现：
1. 删除 4 处 `TODO` 注释
2. 在连接建立前增加 `CHREQUIRE(receiver.impl() != nullptr && driver.impl() != nullptr, ...)` 检查
3. 不实现完整的模块端口归属验证——当前 `get_parent()` 信息不足以在所有场景下验证
4. 如果后续需要更严格的验证，可作为独立议题

**理由**:
1. `TODO` 注释已存在 >6 个月未处理，说明这不是阻塞性问题
2. `nullptr` 检查是最基础的防御——捕获最常见的错误（未初始化的端口）
3. 模块归属验证需要更复杂的组件追踪基础设施，当前优先级不够

---

## 5. Q4: `operator&&` / `operator||` 反模式

### 5.1 分析

AGENTS.md 标记 `&&` 在 IO 端口上为反模式，推荐使用 `select()` 替代。实际实现（`io.h:379-392`）已经正确地产生了硬件 AND/OR 门——它调用 `get_lnode(lhs) & get_lnode(rhs)`，不是 C++ 短路求值。

问题在于：
1. `&&` 的 C++ 语义暗示短路求值，但硬件中两个输入都应该被计算
2. 用户可能混淆 `&&`（逻辑与）和 `&`（按位与），虽然对 1-bit 信号效果相同
3. `select()` 是 CppHDL 中条件逻辑的推荐方式

### 5.2 决策

**决议**: ✅ **保留操作符，添加 TODO 注释标记为已废弃方向**。

具体方案：
1. 保留 `operator&&` / `operator||` 在端口上的重载（向后兼容）
2. 在定义处添加 TODO 注释说明应使用 `select()`
3. AGENTS.md 中已有反模式声明，不做额外修改

**理由**:
1. 删除会破坏现有代码
2. 实现本身是正确的（产生硬件 AND/OR 门）
3. TODO 注释为未来清理提供指引

---

## 6. 变更日志

| 日期 | 版本 | 变更 | 作者 |
|------|------|------|------|
| 2026-05-06 | v1.0 | 初始版本，记录 Q1-Q4 分析与决议 | Sisyphus + 用户 |

---

**相关链接**:
- `include/core/io.h:680-853` — 8 个 `operator<<=` 重载
- `include/ast/ast_nodes.h:147-194` — `inputimpl` / `outputimpl` 定义
- `include/core/lnodeimpl.h:109-131` — `add_src()` / `set_src()` 基类实现
- `src/ast/instr_io.cpp:8-20` — IO 指令 eval 实现
