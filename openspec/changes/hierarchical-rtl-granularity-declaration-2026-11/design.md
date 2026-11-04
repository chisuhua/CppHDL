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
