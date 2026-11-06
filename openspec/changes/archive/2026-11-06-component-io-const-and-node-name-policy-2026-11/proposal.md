# Proposal: `io() const` + opt-in 节点名唯一性（`component-io-const-and-node-name-policy-2026-11`）

## Why

### U-2 —— `io()` **无 const 重载**（实测）

`__io(...)` 宏生成的 `io()` **只有**非 const 重载（`include/core/io.h`）
⇒ ⇒ const 对象 / const 引用**读不到 IO** ⇒ ⇒ 下游只能在 const 接口里放弃 const 正确性。

⇒ ⇒ 修法 = **加一个 const 重载**（`const io_type& io() const`）⇒ ⇒ **1 行**。

### U-1 —— ctx 内的节点名**不唯一**（实测）

解释器侧**不改名**（同名节点共存），而 Verilator backend **自动改名**（加 `_N` 后缀）
⇒ ⇒ 两后端看到**不同的节点名** ⇒ ⇒ 「按名匹配观测点」的契约在两后端**不等价**
⇒ ⇒ 这是一处**真实的两后端分歧前提**（本仓「同构缺陷恒被 parity 掩盖」的反面：
这里是**异构**分歧 ⇒ parity **看得见**但没人写判据）。

⇒ ⇒ 修法 = ctx 提供 **opt-in** 的唯一性策略（默认**宽松** ⇒ **零回归**），
开启后同名节点在**创建点** fail-fast。

## What Changes

1. **U-2**：`__io` 宏加 **const 重载**（`const io_type& io() const`）。
2. **U-1**：`ch::core::context` 加**可显式开启**的节点名唯一性策略
   （`kAllowDuplicates`（**默认**）/ `kRejectDuplicates`）⇒ ⇒ 开启后 `create_input` /
   `create_output` 等**创建点**检测到同名 ⇒ ⇒ **抛异常**（MUST NOT 只打日志）。
3. **ctx MUST NOT 自动改名**（既不自动加后缀，也不静默替换）⇒ ⇒ 名字冲突只由
   「放行」或「报错」两种方式处理。

## 不做（Non-Goals）

- **不**改默认行为（唯一性策略**默认宽松**）⇒ ⇒ 零回归。
- **不**把唯一性策略**默认打开**（会让仓内现有设计大面积重名报错 ⇒ ⇒
  实测 `include/` 的 **641** 个 `ch_in` 声明**零**显式名字 ⇒ ⇒ 那是**规格预期**，不是缺陷）。
- **不**做层次化相关的事 ⇒ ⇒ 本 change 与层次化主线**无依赖**。

## Acceptance

- [ ] **U-2**：const 上下文 MUST 可读 IO（`impl()` / `name()` / `is_valid()`）⇒
      **编译期断言**核验；移除 const 重载 ⇒ 该断言**编译失败**（判别性对照）
- [ ] **U-1**：开启策略后同名节点 MUST **抛异常**且错误信息含**冲突的节点名**；
      **默认策略 MUST 保持宽松**（不抛、允许同名共存）
- [ ] **U-1**：ctx MUST **不改名**
- [ ] **两层**护栏：① 行为层（开启 ⇒ 抛）② 默认层（默认 ⇒ 不抛）
- [ ] **零回归**：`ctest -LE perf` / samples / examples 全绿 · 计数锚点同步

## Capabilities

| capability | delta |
|---|---|
| `component-io-const-access` | **ADDED ×1**（`__io` 生成的 IO 访问器 MUST 提供 const 版本） |
| `core-ctx-node-name-uniqueness` | **ADDED ×1**（ctx MUST 提供 opt-in 的节点名唯一性策略） |

⇒ 归档**禁** `--skip-specs`。

---

## 附录：原 `component-framework-hardening-2026-10` 的实测证据

> 原 change 含 3 项（U-1 / U-2 / U-4）⇒ ⇒ U-4 已单独实施并归档
> （`header-odr-inline-fix-2026-11`）⇒ ⇒ 此处保留 U-1 / U-2 的**实测证据**。

### 2. ★ctx 内的节点名**不唯一**，且**只有 Verilog 后端**会改名

`src/core/context.cpp:200-237` 的 `create_input` / `create_output` **不做**名称唯一化
⇒ **同名节点可以并存**（ctx 里有两个叫 `reg_1` 的节点是完全合法的）。

唯一化只发生在 **Verilog codegen**（`src/codegen_verilog.cpp:64-76` 建 `node_names_` map
⇒ 同名者加 `_N` 后缀）⇒ **解释器后端（Simulator）不改名**。

⇒ **后果（两后端看到不同的节点名）**：

| 后端 | 同名时的表现 |
|---|---|
| Verilator | 端口被改名（`reg_1` → `reg_1_1`）⇒ **按契约名匹配失败** |
| 解释器 | **不改名** ⇒ 按契约名匹配**会命中** ⇒ **静默采到错的那个** |

⇒ 下游若按名字解析观测点（ChipForge 的 parity runner 就是这么做的）⇒ ★
**解释器侧会静默采到内部节点** ⇒ **读到貌似合理的错值** ⇒ **假绿**
（比「读不到」更坏：「读不到」至少会报错）。

⇒ **上游自己的先例就是踩坑现场**：`include/cpu/pipeline/rv32i_regfile.h`
`create_ports()` 写 `new (io_storage_) io_type;`（默认构造 ⇒ 端口名 `unnamed_input`）
且 `describe()` 里用 **32 个局部 `ch_reg<ch_uint<32>> x0(0_d)`…`x31`**（`ch_reg` 默认名
`"reg"` ⇒ **32 个同名节点**）⇒ ⇒ **若默认开启唯一性检测，上游自己的这个组件会立刻炸**
⇒ ⇒ **检测 MUST 是 opt-in**。

### 3. `__io` 宏**无 const 版 `io()`**

`include/core/io.h:312-319` 的 `__io` 宏只生成**非 const** 的 `io()`：

```cpp
[[nodiscard]] io_type &io() { return *reinterpret_cast<io_type *>(io_storage_); }
```

⇒ **后果**：**const 上下文无法读 Component 的 IO** ⇒ 下游若想在
`const` 方法 / `const` 引用里观测组件输出 ⇒ **只能放弃 const 或加 mutable**
⇒ 且 `io_storage_` 是 `char[]` ⇒ 加 const 重载是**零破坏**的纯新增：

```cpp
[[nodiscard]] const io_type &io() const { return *reinterpret_cast<const io_type *>(io_storage_); }
```
