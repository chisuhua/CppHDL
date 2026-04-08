好的！我们将以你提供的 `Counter<N>` 示例为基础，**详细展开 `describe()` 调用期间构建 IR 节点的完整过程**，包括：

- 每一行用户代码触发了哪些内部函数
- 创建了哪些类型的 `lnodeimpl` 节点
- 每个节点的构造逻辑和关键代码位置

---

## 🧩 用户代码回顾

```cpp
template <unsigned N>
struct Counter {
  __io ((__out (ch_uint<N>) out));
  void describe() {
    ch_reg<ch_uint<N>> reg(2);      // ← 行 A
    reg->next = reg + 1;            // ← 行 B
    io.out = reg;                   // ← 行 C
  }
};
```

我们逐行分析。

---

## 📌 前提：上下文已激活

在 `ch_device<Counter<4>>::load()` 中，已调用：

```cpp
// src/core/deviceimpl.cpp
bool deviceimpl::begin() {
  is_opened_ = true;
  old_ctx_ = ctx_swap(ctx_);  // ← 切换到当前 device 的 context
  return (instance_ != 0);
}
```

> ✅ 此时 `ch::internal::ctx_curr()` 返回 `Counter` 的专属 `context*`，所有后续建模操作都注册到该 `context`。

---

## 🔹 行 A：`ch_reg<ch_uint<N>> reg(2);`

### 1. 调用 `ch_reg` 构造函数

#### 代码位置：`include/reg.h`（模板展开后）

```cpp
// ch_reg_impl 构造（见 reg.h）
explicit ch_reg_impl(CH_SRC_INFO)
: base(make_logic_buffer(createRegNode(
    ch_width_v<T>, srcinfo.name(), srcinfo.sloc()
))) {
  __next__ = std::make_unique<next_t>(getRegNextNode(get_lnode(*this)));
}
```

### 2. 调用 `createRegNode(unsigned size, ...)`（无初始值重载）

但用户传入了 `2`，所以实际调用的是：

```cpp
// include/reg.h
lnodeimpl* createRegNode(const lnode& init, const std::string& name, const source_location& sloc);
```

### 3. `createRegNode(const lnode& init, ...)` 实现

#### 代码位置：`src/ast/regimpl.cpp`

```cpp
lnodeimpl* ch::internal::createRegNode(const lnode& init,
                                       const std::string& name,
                                       const source_location& sloc) {
  auto ctx = ctx_curr();
  auto cd = ctx->current_cd(sloc);
  auto rst = ctx->current_reset(sloc);
  auto init_impl = init.impl();  // ← 字面量节点（litimpl）
  auto reg = ctx->create_node<regimpl>(
      init.size(), 1, cd, rst, nullptr, nullptr, init_impl, name, sloc);
  return ctx->create_node<proxyimpl>(reg, name, sloc);
}
```

### 4. 创建的节点类型

| 节点类型 | 说明 | 代码位置 |
|--------|------|--------|
| `litimpl` | 字面量 `2` | `src/ast/litimpl.cpp` |
| `regimpl` | 寄存器本体（含初始值） | `src/ast/regimpl.cpp` |
| `proxyimpl` | 寄存器的代理（用于赋值、切片等） | `src/ast/proxyimpl.cpp` |

> ✅ **`reg` 变量实际是 `proxyimpl` 的 wrapper（通过 `logic_buffer`）**

---

## 🔹 行 B：`reg->next = reg + 1;`

### 1. `reg + 1` → 触发 `operator+`

#### 代码位置：`include/bitbase.h`（或 `numbase.h`）

```cpp
template <typename T, typename U>
auto operator+(const T& lhs, const U& rhs) {
  return ch_add(lhs, rhs);  // ← 通用加法
}
```

### 2. `ch_add(...)` → 创建加法操作节点

#### 代码位置：`src/ast/opimpl.cpp`

```cpp
lnodeimpl* ch::internal::createOpNode(ch_op op,
                                      uint32_t size,
                                      bool is_signed,
                                      lnodeimpl* lhs,
                                      lnodeimpl* rhs,
                                      const std::string& name,
                                      const source_location& sloc) {
  auto ctx = ctx_curr();
  return ctx->create_node<opimpl>(ctx, op, size, is_signed, lhs, rhs, name, sloc);
}
```

> 其中 `ch_op::add`，`size = N`，`lhs = reg.impl()`（proxyimpl），`rhs = litimpl(1)`

### 3. 创建的节点类型

| 节点类型 | 说明 |
|--------|------|
| `litimpl` | 字面量 `1` |
| `opimpl` | 加法操作节点（`reg + 1`）|

### 4. `reg->next = ...` → 设置寄存器 next 值

#### 代码位置：`include/reg.h`（`next_t::operator=`）

```cpp
template <typename U>
void operator=(const U& value) {
  auto next_node = to_lnode<T>(value, srcinfo);  // ← 已是 opimpl
  auto reg_node = getRegNextNode(reg_);
  reg_node->set_next(next_node.impl());  // ← 关键：建立 next 连接
}
```

#### 实际调用 `regimpl::set_next(...)`

```cpp
// src/ast/regimpl.cpp
void regimpl::set_next(lnodeimpl* next) {
  this->add_src(next);  // ← next 成为 regimpl 的第 0 个源（src(0)）
}
```

> ✅ **`regimpl` 的 `src(0)` 指向 `opimpl`**

---

## 🔹 行 C：`io.out = reg;`

### 1. `io.out` 是 `ch_logic_out<ch_uint<N>>`

由 `__io` 宏展开生成，其底层是 `outputimpl`（输出端口）

### 2. 赋值操作 → 创建输出驱动

#### 代码位置：`include/ioport.h`（`ch_logic_out::operator=`）

```cpp
template <typename U>
void operator=(const U& src) {
  auto node = get_lnode(src);  // ← reg 的 proxyimpl
  auto output = ctx_curr()->get_output(name_);  // ← 查找或创建 outputimpl
  output->src(0) = node;  // ← 驱动源
}
```

#### 实际调用 `context::create_output(...)`

```cpp
// src/core/context.cpp
outputimpl* context::create_output(uint32_t size,
                                   const std::string& name,
                                   const source_location& sloc) {
  auto src = this->create_node<proxyimpl>(size, '_' + name, sloc);  // ← 代理
  auto value = smart_ptr<sdata_type>::make(size);
  return this->create_node<outputimpl>(size, src, value, name, sloc);
}
```

### 3. 创建的节点类型

| 节点类型 | 说明 |
|--------|------|
| `proxyimpl` | 输出端口的内部代理（用于驱动）|
| `outputimpl` | 输出端口本体 |

> ✅ **`outputimpl` 的 `src(0)` 指向 `reg` 的 `proxyimpl`**

---

## 🧾 最终 IR 节点图（按创建顺序）

| 节点 ID | 类型 | 作用 | 源节点（src） |
|--------|------|------|-------------|
| #0 | `litimpl` | 常量 `2` | — |
| #1 | `regimpl` | 寄存器 | src(0)=#3（next），init=#0 |
| #2 | `proxyimpl` | reg 变量 | src(0)=#1 |
| #3 | `litimpl` | 常量 `1` | — |
| #4 | `opimpl` | `#2 + #3` | src(0)=#2, src(1)=#3 |
| #5 | `proxyimpl` | output 代理 | src(0)=#2 |
| #6 | `outputimpl` | `io.out` | src(0)=#5 |

> 🔗 **数据流**：  
> `#2 (reg)` → `#4 (reg+1)` → `#1.next`  
> `#2 (reg)` → `#5` → `#6 (io.out)`

---

## 📁 关键代码文件汇总

| 功能 | 文件 |
|------|------|
| `ch_reg` 构造 | `include/reg.h` |
| `createRegNode` | `src/ast/regimpl.cpp` |
| `litimpl` | `src/ast/litimpl.cpp` |
| `opimpl`（加法）| `src/ast/opimpl.cpp` |
| `outputimpl` | `src/ast/ioimpl.cpp` |
| `proxyimpl` | `src/ast/proxyimpl.cpp` |
| `context` 管理 | `src/core/context.cpp` |
| `deviceimpl::load` | `src/core/deviceimpl.cpp` |

---

## ✅ 总结

`describe()` 中的每一行 C++ 代码，都会：

1. **立即执行**（非延迟）
2. **调用工厂函数**（如 `createRegNode`, `createOpNode`）
3. **在当前 `context` 中创建 `lnodeimpl` 子类节点**
4. **建立节点间的连接（`add_src`）**
5. **最终形成完整的数据流图（IR）**

整个过程**完全发生在 `ch_device` 构造期间**，**不依赖仿真或代码生成**。
