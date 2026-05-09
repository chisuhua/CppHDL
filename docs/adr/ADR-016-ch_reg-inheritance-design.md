# ADR-016: `ch_reg<T>` 继承设计

**状态**: ✅ 已采纳
**日期**: 2026-05-07
**决策人**: Sisyphus + 用户（参考 Oracle 分析）

---

## 1. 背景

`ch_reg<T>` 是寄存器封装模板，通过继承持有数据类型 `T`（如 `ch_uint<8>`、`ch_bool`），并通过 `std::unique_ptr<next_type>` 支持流水线级联的非阻塞赋值。

**关键文件**：
- `include/core/reg.h:33-62` — `ch_reg` 类定义
- `include/lnode/reg.tpp` — 构造和实现细节

**当前设计**：
```cpp
template <typename T> class ch_reg : public T {
    std::unique_ptr<next_type> __next__;   // 流水线链
    lnodeimpl *regimpl_node_ = nullptr;       // 寄存器节点
};
```

---

## 2. Q1: 继承设计分析

### 2.1 代码证据

**两个不同节点的语义分工**（`include/lnode/reg.tpp:40-65`）：

| 指针 | 初始化位置 | 指向 | 用途 |
|------|-----------|------|------|
| `logic_buffer<T>::node_impl_`（继承自 `T`） | `reg.tpp:53` — `static_cast<logic_buffer<T>*>(this)->node_impl_ = proxy_node;` | `proxy_node`（代理节点） | 寄存器**当前输出值**，下游逻辑读取 |
| `regimpl_node_`（ch_reg 自有） | `reg.tpp:57` — `regimpl_node_ = reg_node;` | `reg_node`（寄存器节点） | 寄存器**本身**，用于 `->next = ...` 赋值 |

**关键发现**：两者是**不同节点，服务不同语义实体，无需同步**。
- 当执行 `reg + 1_d`，继承自 `T` 的 `operator+` 调用 `this->impl()` → `node_impl_`（代理节点）
- 当执行 `reg->next = value`，使用 `regimpl_node_` 通过 `__next__` → `next_assignment_proxy::operator=`

**使用证据**：
- `tests/test_reg.cpp:129-132` — 静态断言确认 `is_base_of_v<ch_uint<8>, ch_reg<ch_uint<8>>>`（替代性测试）
- `tests/test_reg_timing.cpp:32` — `counter->next = counter + 1_d;` — `counter` 在 `+` 中作为 `ch_uint<4>`，在 `->next` 中作为 `ch_reg`
- `include/chlib/stream_pipeline.h:36` — `payload_reg->next = select(..., payload_reg)` — `payload_reg` 同时用于 `select`（作为 `T`）和 `->next`（作为 `ch_reg`）

### 2.2 风险分析

| 风险 | 评估 |
|------|------|
| **Diamond 继承** | 低 — `T`（如 `ch_uint<N>`）继承层次简单，无虚继承 |
| **构造函数脆弱性** | 中 — 必须将 `this` 强制转换为 `logic_buffer<T>*` 设置 `node_impl_`（`reg.tpp:53`）；若 `T` 的构造函数层次变化可能失效 |
| **双重指针误用** | 低 — `node_impl_`（代理）和 `regimpl_node_`（寄存器）语义明确，注释已充分说明 |

### 2.3 结论

**保持继承设计，无需修改。** `ch_reg<T>` IS-A `T`，可完全替代使用，这是最符合硬件语义的设计。组合方案需要手动转发 20+ 操作符，成本极高且风险大。

---

## 3. Q2: `unique_ptr<next_type>` 分析

### 3.1 代码证据

**`__next__` 从不形成链表**：
- 每个 `ch_reg` 只有一个 `next_proxy` 对象，构造函数总是初始化（`reg.tpp:62,84`）：
  ```cpp
  __next__ = std::make_unique<next_type>(regimpl_node_);
  ```
- `__next__` 从不在运行时遍历，仅通过 `operator->()` 解引用一次
- `operator<<=` 中的 `if (__next__)` 检查是不必要的防御性代码

**使用模式**（全代码库 525 处 `->next =`）：
```cpp
// 唯一使用方式：单层赋值
reg->next = value;  // ch_reg::operator->() 返回 __next__.get()
```

### 3.2 改进方案

将 `std::unique_ptr<next_type>` 替换为直接成员：

```cpp
// reg.h:59-61 — 当前
std::unique_ptr<next_type> __next__;

// 改进后
next_type next_proxy_;  // 直接成员，无堆分配
```

```cpp
// reg.h:50 — 当前
const next_type *operator->() const { return __next__.get(); }

// 改进后
const next_type *operator->() const { return &next_proxy_; }

// reg.h:53-57 — 当前
template <typename U> void operator<<=(const U &value) const {
    if (__next__) { __next__->next = value; }  // 不必要的 null 检查
}

// 改进后
template <typename U> void operator<<=(const U &value) const {
    next_proxy_.next = value;  // 无 null 检查
}
```

### 3.3 决策

**Q2 改进延后至最低优先级**。仅在其他所有优化任务完成后才考虑。

**当前状态可接受**：虽然 `unique_ptr` 引入不必要的堆分配，但：
- 每个寄存器的 `next_proxy` 对象大小极小（仅包含一个 `lnodeimpl*`）
- 当前代码工作正常，无已知 bug
- 改进收益与成本（重构风险）不成比例

---

## 4. Q3: 组合替代方案分析

### 4.1 组合设计

```cpp
template <typename T> class ch_reg {
    T value_;                    // 组合而非继承
    next_type next_proxy_;       // 直接成员
    lnodeimpl *regimpl_node_;
};
```

### 4.2 组合会破坏的功能

| 功能 | 破坏原因 | 所需修复 |
|------|---------|---------|
| `reg + 1_d` | 失去继承的 `operator+` | 手动转发所有算术操作符 |
| `reg[3]` | 失去继承的 `operator[]` | 手动转发比特选择 |
| `reg(7, 0)` | 失去继承的 `operator()` | 手动转发位切片 |
| `reg == other` | 失去继承的比较操作符 | 手动转发所有比较 |
| `reg.as<16>()` | 失去继承的 `as<>` | 手动转发类型转换 |
| `reg.msb()` / `reg.lsb()` | 失去继承方法 | 手动转发所有方法 |
| `lnode<T> ln = reg;` | 失去隐式转换 | 添加 `operator lnode<T>()` |
| `select(cond, reg, other)` | `select` 期望 `ValidOperand`（T 或 lnode） | 添加转换操作符 |
| `io().out = reg;` | IO 赋值期望 `T` 或 `lnode` | 添加赋值操作符 |

### 4.3 结论

**不推荐组合方案**。需要手动转发约 20+ 操作符和方法，成本 3+ 天，且极易引入遗漏错误。当前继承模式工作良好，与 SpinalHDL 的 `Reg[T]` 语义一致。

---

## 5. 决议总结

| 问题 | 决策 | 优先级 | 工作量 |
|------|------|--------|--------|
| **Q1** 继承设计 | **保持** — 语义清晰，替代成本高 | — | — |
| **Q2** `unique_ptr` 优化 | **延后** — 仅在其他优化完成后考虑 | 🟢 最低 | 1-2 小时 |
| **Q3** 组合替代 | **不推荐** — 转发成本极高，风险大 | — | 3+ 天 |

---

## 6. 变更记录

| 日期 | 版本 | 变更 | 作者 |
|------|------|------|------|
| 2026-05-07 | v1.0 | 初始版本，记录 Oracle Q1-Q3 分析与决议 | Sisyphus + 用户 |

---

**相关链接**:
- `include/core/reg.h:33-62` — `ch_reg` 类定义
- `include/lnode/reg.tpp` — 构造函数实现，双节点语义
- `include/chlib/stream_pipeline.h:31-32` — 流水线寄存器使用模式
- `tests/test_reg.cpp:129-132` — 替代性测试
- `tests/test_reg_timing.cpp:32` — `counter->next = counter + 1_d` 模式
- `docs/adr/ADR-DISCUSSION-PLAN.md` — 议题 #11