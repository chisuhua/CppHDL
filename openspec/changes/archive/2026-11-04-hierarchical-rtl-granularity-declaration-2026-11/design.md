# 设计：层级粒度声明（`hierarchical-rtl-granularity-declaration-2026-11`）

> 决策在 ADR-061（决策①⑤⑥）+ ADR-059 §2.2.1（模块类型名的**四档来源**）；
> 本文件承载**推导与证据**。
> ⚠️ **第 ① 档（类级宏 `CH_HIERARCHY_LAYER("name")`）由 T3-①a 落地** ⇒ ⇒ 本 change
> 只做**②③④ 三档的分派** + `ch_layer` / `ch_inline` / `is_layer()` / 无 parent 装配点。
## 2. 层级粒度（ADR-061 的落地形态）

### 2.1 三种声明形态

```cpp
// ① 类级（组件作者声明「我是一个层」）
class Rv32iPipeline : public ch::Component { CH_HIERARCHY_LAYER("rv32i_pipeline"); ... };

// ② 实例级覆盖（使用方按需指定）—— 两个新 API 名，**不动 ch_module 签名**
ch::ch_layer<Sub>    s{"s"};                   // ② T 已声明成层 ⇒ 继承其类级名
ch::ch_layer<Helper> h{"h", "helper_layer"};  // ③ T 未声明成层 ⇒ MUST 在实例处给名
ch::ch_inline<Sub>   t{"t"};                   // 显式 inline

// ③ emitter 侧总开关（逃生门 + 对比测试用）
enum class Granularity { RespectDeclarations, ForceInline, ForceLayer };
```

⚠️ **为什么不用 tag 参数**：`ch_module` 的构造参数**转发给组件构造函数**
（`include/module.h:60`）⇒ ⇒ tag 参数会要求**每个**组件的 ctor 都接受它 ⇒ ⇒ 破坏现有组件。

### 2.2 inline 的语义（落地）

| 对象 | inline 后（ADR-061 决策② · 两作用域） |
|---|---|
| 组件的**有 driver** 的输入端口 | 父 module 内的 **wire** + `assign`（沿用 `codegen_verilog.cpp:333-360` 的既有机制） |
| 组件的**无 driver** 的输入端口 | **最近成层祖先**的 `input` 端口（祖先 = RTL 顶层 ⇒ 顶层 `input`）+ `CHWARN`（扁平规则本身） |
| 组件的**输出端口** | **最近成层祖先**的 `output` 端口：祖先 = **RTL 顶层** ⇒ 顶层 `output`（⇒ 即使已被父层消费、即使无人消费，扁平里就是这么做的 `:363-370`）；祖先 = **非顶层成层 module** ⇒ 该 module **体内的普通信号**（不上 RTL 顶） |
| 组件的体内节点 | 并入父 module 的 body |

⚠️ **「三处发射点 × module 角色」**（ADR-062 §2.1，oracle R2）：子 module 的 `ch_in`
（**有** driver）⇒ 端口表**有**、`print_decl` **不**声明 wire、`print_logic` **不**发 `assign`；
父 module 为子 `ch_out` 声明 `logic` 网并由实例 `.port(net)` 驱动（**不是** `assign`）。
⇒ ⇒ 现有 `print_decl`（`:430-446`）与 `print_logic`（`:613-663`）**都按 `driver()` 判定**
⇒ 逐 module 复用而不改判据 ⇒ 子 module 的端口会**双声明**（`input` + `logic`）并收到
一条**非法**的 `assign <input> = ...`。

⚠️ **为什么 inline 的输出不降成内部 wire**（自审揪出的矛盾，ADR-061 §2.2 已记）：降成 wire ⇒
无人消费的子输出从端口表消失 ⇒ ⇒ **与扁平产物分叉** ⇒ ⇒ 「全 inline 逐字节一致」不成立。
⇒ ⇒ **端口表收缩是成层子树独有的回报**：被成层组件无人消费的子输出**不**上顶层。

### 2.3 装配点与成层判定**解耦**

「是否成层」由**声明**决定；装配点可以是任意位置，含**无 active parent** 处
（下游 ChipForge 的桥接 plugin：`setup()` 内 `make_unique` + `build(ctx)`）
⇒ ⇒ emitter 通过**组件树**（`children()`）而非 `ch_module` 发现层级
⇒ ⇒ ⇒ **组件树必须正确** ⇒ ⇒ ⇒ **归属修正是硬前置**。

---

### 风险（自 umbrella §7 分派）

| # | 风险 | 对策 |
|---|---|---|
| 3 | 隐式端口方向搞反 ⇒ 错 RTL | 方向矩阵 4 行各一用例（边界 #14/#15/#16） |

---

## 4. 实施期记录（**文档 MUST 与代码一致**）

| # | 设计稿 | 定稿 | 原因 |
|---|---|---|---|
| 1 | 「两个新 API 名，内部薄封装 `ch_module`」（未说怎么把实例级声明传进去） | `ch_module` 加**受保护**的第二个 ctor（接收 `InstanceDecl`）；`ch_layer` / `ch_inline` 派生自 `ch_module<T, Args...>` 并调它 | 公开 ctor **签名零变更**（`ch_module(name, args...)` 仍转发给组件 ctor）；实例级声明 MUST 在 `build()` **之前**写入（否则组件在自己的 `describe()` 里读到错的 `is_layer()`） |
| 2 | 实例级 setter（`set_instance_layer` / `set_instance_module_type_name`） | **private** + `friend ch_module`（模板 friend） | 公开面 MUST 只有 `CH_HIERARCHY_LAYER` / `ch_layer` / `ch_inline` 三个声明入口 ⇒ 少一个「绕过声明直接改成层」的面 |
| 3 | 四档的可判定性 | **编译期**：`requires detail::has_class_layer_name<T>` 分派 + 测试里的 7 条 `static_assert` | 档 ② 的「MUST NOT 另给名字」与档 ③ 的「缺名 MUST 不可表达」都是**语法层**约束 ⇒ 用 `requires` 表达是**编译期**拒绝，MUST NOT 推迟到运行期 |
| 4 | ④ 回退 `sanitize(typeid)` | 回退 `typeid(...)`（**未 sanitize**）+ `CHWARN`（每组件一次） | `sanitize_name` 是 writer 成员函数（`codegen_verilog.h:52`）⇒ `component.h` 用不了；sanitize 归发射侧（T3-④）⇒ 与 ①a 的处理一致 |

### 4.1 本段**不产生产物**（范围界定）

`is_layer()` 的消费者是 **T3-② 分析层**与 **T3-④ 发射器** ⇒ ⇒ 「成层 ⇒ 真的多一个
Verilog module」在本段**尚不可观测**（那是 T3-④ 的判据）。本段的判据面 = 「声明 → 判定 →
模块类型名」这条链。

### 4.2 一条如实登记的**等价变异**

把 `module_type_name()` 的「实例名优先」改成「类级优先」⇒ 测试**恒绿**。
原因：档 ② 永不写实例名（档 ② 的 `ch_layer` 单参形态）、档 ③ 只作用于**无**类级名的类型
⇒ ⇒ 两条分支**互斥** ⇒ ⇒ 优先级顺序**不可观测**（⇒ 不是测试缺口；契约只钉「档 ② MUST
继承类级名」，已由判据覆盖）。
