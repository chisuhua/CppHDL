好的！我们将基于你提供的 `req2.md` 内容，**详细描述该框架中数据流图（IR）的仿真执行过程**，包括：

- 仿真如何启动
- 如何遍历 IR 节点
- 如何生成执行指令（`instr_*`）
- 如何按节拍（tick）推进仿真
- 关键数据结构与代码位置

---

## 🧱 一、前提：IR 已构建完成

在 `ch_device<T>` 构造期间，`T::describe()` 已执行，构建了完整的 **节点图（`lnodeimpl` 子类组成的 DAG）**，存储在 `context` 中。

例如 `Counter<4>` 的 IR 包含：
- `litimpl`（常量 2、1）
- `regimpl`（寄存器）
- `opimpl`（加法）
- `proxyimpl`（代理）
- `outputimpl`（输出端口）

这些节点通过 `src(i)` 相互连接，形成数据流图。

---

## ⚙️ 二、仿真启动：`ch_tracer` / `ch_simulator` 初始化

### 1. 创建 `tracerimpl`（继承自 `simulatorimpl`）

```cpp
ch_tracer::ch_tracer(const std::vector<device_base>& devices)
  : ch_simulator(new tracerimpl(devices)) {
  impl_->initialize();  // ← 关键初始化
}
```

### 2. `tracerimpl::initialize()` 做了什么？

#### 代码位置：`src/sim/tracerimpl.cpp`

```cpp
void tracerimpl::initialize() {
  simulatorimpl::initialize();  // ← 核心：构建执行计划
  // 添加信号到 trace 列表（用于 VCD/打印）
  for (auto node : eval_ctx_->outputs()) {
    signals_.emplace_back(reinterpret_cast<ioportimpl*>(node));
  }
}
```

---

## 🔧 三、核心：`simulatorimpl::initialize()` —— 构建执行计划（Lowering）

这是**将 IR 节点图转换为可执行指令序列**的关键步骤。

### 1. 遍历所有节点，按类型生成 `instr_*` 对象

#### 代码位置：`src/sim/simulatorimpl.cpp`

```cpp
for (auto node : eval_list) {
  instr_base* instr = nullptr;
  switch (node->type()) {
    case type_reg:
      instr = instr_reg_base::create(...);
      break;
    case type_mem:
      // ...
    case type_op:
      instr = instr_op_base::create(...);
      break;
    case type_proxy:
      instr = instr_proxy_base::create(...);
      break;
    case type_input:
      instr = instr_input_base::create(...);
      break;
    case type_output:
      instr = instr_output_base::create(...);
      break;
    // ...
  }
  instr_map[node->id()] = instr;
}
```

> ✅ **每个 `lnodeimpl` 节点 → 对应一个 `instr_*` 执行对象**

---

### 2. 指令对象的作用

| 节点类型 | 指令类型 | 作用 |
|--------|--------|------|
| `regimpl` | `instr_reg` | 在 `eval()` 中：`if (enable) dst = next` |
| `opimpl` | `instr_op` | 执行加法、比较等操作 |
| `proxyimpl` | `instr_proxy` | 复制位段（`bv_copy`） |
| `outputimpl` | `instr_output` | 将值写入输出缓冲区 |
| `inputimpl` | `instr_input` | 从输入缓冲区读取值 |

这些指令内部持有指向**仿真数据缓冲区**（`block_t*`）的指针。

---

### 3. 数据缓冲区分配

在 `instr_*::create()` 中：

```cpp
// 为节点分配仿真存储空间
auto dst = (block_type*)buf_cur;
map[node->id()] = dst;  // data_map_t
bv_init(dst, dst_size);
```

> ✅ 所有节点的仿真值都存储在连续内存中，通过 `data_map` 映射 `node->id()` → `block_t*`

---

## ▶️ 四、仿真执行：`simulatorimpl::eval()`

每次调用 `eval()`，执行一个**仿真节拍（tick）**。

### 1. 执行顺序：按拓扑排序（`eval_list`）

`eval_list` 是在 `initialize()` 中通过**拓扑排序**生成的节点列表，确保：
- 组合逻辑先于寄存器更新
- 依赖关系满足

### 2. 执行每个指令的 `eval()`

```cpp
void simulatorimpl::eval() {
  for (auto node : eval_list) {
    auto instr = instr_map.at(node->id());
    instr->eval();  // ← 执行具体操作
  }
}
```

#### 示例：`instr_reg::eval()`

```cpp
void eval() override {
  if constexpr (has_enable) {
    if (!static_cast<bool>(enable_[0])) return;
  }
  bv_copy_vector(dst_, next_, size_);  // reg = next
}
```

#### 示例：`instr_op::eval()`（加法）

```cpp
void eval() override {
  bv_add(dst_, lhs_, rhs_, size_);  // dst = lhs + rhs
}
```

---

## 🕒 五、仿真节拍推进（`tracer.run()`）

用户代码：

```cpp
tracer.run([&](ch_tick t)->bool {
  std::cout << "t" << t << ": out=" << counter.io.out << std::endl;
  return t != 20;
}, 2);
```

### 内部流程：

1. `t = 0`：调用 `eval()` → 执行所有指令（包括寄存器初始化）
2. 打印 `counter.io.out`（从 `outputimpl` 的缓冲区读取）
3. `t = 1`：再次 `eval()` → 组合逻辑计算 `reg+1`，寄存器更新
4. 重复直到返回 `false`

> ✅ **每个 `eval()` 调用 = 1 个时钟周期**

---

## 📦 六、关键数据结构

| 结构 | 作用 | 代码位置 |
|------|------|--------|
| `lnodeimpl` | IR 节点基类 | `include/lnodeimpl.h` |
| `context` | 存储所有节点 | `src/core/context.cpp` |
| `instr_base` | 仿真指令基类 | `src/sim/instr.h` |
| `data_map_t` | `node_id → block_t*` | `simulatorimpl.cpp` |
| `eval_list` | 拓扑排序的执行列表 | `simulatorimpl.cpp` |

---

## 🔁 七、仿真执行流程图

```
[IR 节点图]
     ↓
simulatorimpl::initialize()
     ↓
遍历节点 → 生成 instr_* 对象
     ↓
分配仿真缓冲区（data_map）
     ↓
拓扑排序 → eval_list
     ↓
每 tick 调用 simulatorimpl::eval()
     ↓
for (node in eval_list):
    instr = instr_map[node.id]
    instr->eval()  // 更新缓冲区
     ↓
用户读取 io.out（从 outputimpl 缓冲区）
```

---

## ✅ 总结

该框架的仿真执行过程是典型的 **“编译式仿真器”** 设计：

1. **建模期**：构建静态 IR（节点图）
2. **初始化期**：将 IR “编译”为指令序列（`instr_*`） + 分配数据缓冲区
3. **运行期**：按节拍执行指令，更新缓冲区值

> **不依赖外部工具（如 Verilator）**，纯 C++ 原生执行，但也可生成 Verilog 供外部仿真。

这种设计兼顾了**仿真速度**和**代码生成灵活性**。
