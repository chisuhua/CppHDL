# ADR-061: 层级粒度由**用户显式**控制 —— 默认 inline，成层须声明

- Status: **Proposed**（2026-11-02 · 待用户拍板 · **v1**：用户提出诉求 + 下游 ChipForge 实证驱动）
- Date: 2026-11-02
- Author: build-agent（architect role）
- Parent: **无**（新增决策）；**修订** ADR-058 §2.1（模块边界的措辞）与 ADR-060（端口策略的作用面）
- 关联 capability: 待建 `rtl-hierarchical-output`（与 ADR-058 共用）
- 关联 change: 待建 `hierarchical-rtl-output-2026-11`（见 `.rddf/roadmap/features/feat-hierarchical-rtl-output.md`）
- 关联台账: `docs/architecture/2026-11-02-hierarchical-rtl-output.md`（需求清单 N1–N7）·
  `docs/developer_guide/patterns/HIERARCHICAL-RTL-AUTHORING-GUIDE.md`（用户写法）

---

## 1. 背景（**只陈述决策所需的事实**）

### 1.1 用户诉求（2026-11-02）

> 「可否让用户在写代码时**选择**不创建新层次、或创建新层次？
> 特别是**小的模块**，对用户来说**默认不创建新层级**，只对**大的模块**用户选择创建新层级。」

### 1.2 实测事实

| # | 事实 | 证据 |
|---|---|---|
| ① | ADR-058 v2 的口径是「**每个组件实例 = 一个 module**」⇒ ⇒ ⇒ 小组合子（`chlib` 的 mux / 编码器 / 断言器 / 前推 mux）**全部各成一 module** ⇒ ⇒ ⇒ 产物变成**例化清单的噪声**，用户真正关心的层被淹没 | ADR-058 §2.1 · 仓内 `chlib` 组合子共 **27** 个对外暴露者 |
| ② | 「一个组件是不是一层」取决于**用户想在哪一级看 RTL**（调试波形 / 交接外部综合 / 时序收敛）⇒ ⇒ 这是**领域知识**，框架无从推断 | — |
| ③ | 下游 **ChipForge** 的层 2 IP 单元（`ch::Component`）**需要**成为独立 module（其 ADR-091 §D3a.8「层次化 = 采纳」）⇒ ⇒ ⇒ 但它的装配点是**桥接 plugin 在 `setup()` 内** `make_unique` + `build(ctx)` ⇒ ⇒ **该点无 active parent** ⇒ ⇒ `ch_module` 不可用（`module.h:42-46` 早退）⇒ ⇒ ⇒ **「成层声明」与「装配点」必须解耦** | ChipForge ADR-091 v1.15 §D3a.7 · `include/cf/plugin/pipe_builder.h` |
| ④ | 下游的 parity runner 用 `resolve_node_id(ctx, name)` 按 **ctx 级节点名精确匹配**（`node->name()`）⇒ ⇒ ⇒ **顶层端口名 MUST 与 ctx 节点名一致** ⇒ ⇒ ⇒ 层次化 emitter **MUST 复用** codegen 既有的**全局**命名表（`node_names_`）⇒ ⇒ ⇒ **MUST NOT** 发明 per-module 命名 | `tools/parity_runner/parity_sim_runner.cpp:249-255` |
| ⑤ | 扁平 codegen 已有「**有 driver 的 `ch_in` ⇒ 内部 `logic` wire + assign**」的机制，且**任意层 `ch_out` 都上顶层 `output`** ⇒ ⇒ ⇒ **inline 的端口处理 = 照抄扁平规则** ⇒ **零新增机制、零新语义** | `src/codegen_verilog.cpp:333-370` |
| ⑥ | 全局命名表保证**端口名与内部节点名不同名**（同名者后者加 `_N`）⇒ ⇒ ⇒ 下游 ADR-091 §G-1o「顶层端口名 MUST NOT 与内部节点同名」**天然满足** | `src/codegen_verilog.cpp:64-78` |

### 1.3 问题（**精确表述**）

1. ADR-058 的「每组件一层」把**层次粒度**替用户决定了 ⇒ ⇒ ⇒ 小组件成层是**纯成本**（多文件、多端口表、多例化噪声）；
2. 但粒度**不能自动推断**（端口数？节点数？ ⇒ ⇒ **不可预测** ⇒ ⇒ 平台替用户猜 = 本仓反复吃过的亏）；
3. ⇒ ⇒ 必须**让用户显式声明**，且**默认取「不成层」**；
4. 且该声明必须**与装配点解耦**（事实③）⇒ ⇒ ⇒ 否则下游的层 2 单元**无法成层**。

---

## 2. Decision（**裁决**）

### 2.1 决策 ① —— 层级的判定是**显式**的，默认 **inline**

> **组件的「成层」与否由声明决定；未声明者一律 inline（不产生 module）。**
> 声明有两级：**类级默认** + **实例级覆盖**。

```cpp
// ① 类级默认：组件作者声明「我是一个层」
class Rv32iPipeline : public ch::Component {
    CH_HIERARCHY_LAYER;                 // ← 成层；未写此宏者 = helper ⇒ inline
};

// ② 实例级覆盖：使用方按需指定
ch::ch_layer<Sub>   s{"s"};             // 显式成层（即使 Sub 是 helper）
ch::ch_inline<Sub>  t{"t"};             // 显式 inline（即使 Sub 是 layer）
```

⚠️ **为什么用两个新 API 名，而不是给 `ch_module` 加一个参数**：
`ch_module` 的构造参数是**转发给组件构造函数**的（`include/module.h:60`）
⇒ ⇒ 用 tag 参数会要求**每个**组件的 ctor 都接受它 ⇒ ⇒ ⇒ 破坏现有组件
⇒ ⇒ ⇒ 故新增**两个薄封装**（内部复用 `ch_module` 的机制）⇒ ⇒ **零破坏**。

### 2.2 决策 ② —— inline 的组件：**端口照扁平规则**，体内节点**并入父 module**

> 被 inline 的组件 `c`：
> ① `c` 的**输入端口**在父 module 内成为**内部 wire**（沿用事实⑤的既有机制）；
> ② `c` 的**输出端口**：**按扁平规则**处理 ⇒ ⇒ **成为「最近的成层祖先 module」的
>    `output` 端口**（⇒ 即使它已被该 module 内消费；⇒ 即使无人消费）⇒ ⇒ ⇒ 见下方 ⚠️。
> ③ `c` 的**体内节点**并入父 module 的 body。

ⓘ **`c` 的输入端口**同样**按扁平规则**：有 driver ⇒ 该 module 内 `wire` + `assign`
（沿用 `src/codegen_verilog.cpp:333-360` 的既有机制）；无 driver ⇒ **该 module 的 `input` 端口**
（见决策 ③）。

⚠️⚠️ **`type_output` 在同一个 module 内有三种角色**（v1.3，oracle R5）⇒ ⇒
现有的 `print_decl`（`src/codegen_verilog.cpp:432-499`）**只有「端口」一种**：
`type_output` 直接进 `outputs` 列表、**没有 wire 分支** ⇒ ⇒ 若按本决策把 inline 子组件的
`ch_out` 当体内信号 ⇒ ⇒ 它会得到 `assign <name> = ...`（`:586-621`）却**无声明**
⇒ ⇒ Verilator 报 undeclared identifier ⇒ ⇒ **「`print_*` 原样复用」不成立**。

| 角色 | 何时 | 需要什么 |
|---|---|---|
| **① 自身端口** | 该 `ch_out` 的 `owner` == 本 module 的组件 | 现有 `print_output`（端口表 + 声明） |
| **② 隐式输出端口** | ADR-060 判定的跨模块边（子→父） | 新形态：端口表 + 声明（名字是合成名） |
| **③ 体内 wire** | `owner` 是**被 inline 的**子组件、且本 module 是**非顶层**成层 module | **新增** `print_decl` 的 `type_output`-as-**wire** 分支 |
| **④ 子端口的连接网** | 本 module **实例化**了一个成层子组件，且要消费它的 `ch_out` | **新增**：父 module 内一根 `logic` 网（由实例化语句的 `.port(net)` 驱动，**不是** `assign`）⇒ ⇒ Verilog **不允许**把实例输出直接接表达式（oracle R3） |

⇒ ⇒ 实现 MUST 先做「端口角色判定表」，再让 `print_header` / `print_decl` / `print_logic`
**按角色**发射 ⇒ ⇒ MUST NOT 依赖「`type_output` 就一定是端口」这条现状假设。

⚠️ **「成层」组件的端口规则**（v1.4，ADR-062 决策①）：**它的全部 `ch_in` / `ch_out`
MUST 成为该 module 的端口 —— 不论是否已被父层驱动** ⇒ ⇒ 父层用**实例端口映射**连接，
MUST NOT 把它当内部 wire。⇒ ⇒ 这与 inline 的「有 driver 的 `ch_in` ⇒ 父内 wire」
**故意不同**（⇒ 否则成层组件的输入端口会全部消失，父层驱动落在别的 module 体内
⇒ 跨 module `assign` ⇒ 非法 Verilog）。

⚠️ **「最近的成层祖先」的两个作用域必须分开说**（v1.1 订正，critic SF）：

| 最近成层祖先 | inline 子组件端口的落点 | 与扁平的关系 |
|---|---|---|
| **RTL 顶层**（`Granularity::ForceInline` 时**必然**是这种） | 无 driver 的 `ch_in` ⇒ **顶层 `input`**；**任意** `ch_out` ⇒ **顶层 `output`** | **逐字相同**（⇒ 「逐字节一致」判据成立） |
| **非顶层成层 module `M`** | 有 driver 的 `ch_in` ⇒ `M` 内 wire；无 driver ⇒ `M` 的 `input`；`ch_out` ⇒ **`M` 体内的普通信号**（**不**上 RTL 顶） | `M` 的端口表由 ADR-060 决定 ⇒ ⇒ 该 `ch_out` **不**出现在 RTL 顶 ⇒ 与扁平**不同**（⇒ 这正是 `hier.top_ports ⊆ flat.top_ports` 的**真子集**来源） |

⇒ ⇒ ⇒ **为什么必须这样分**：若「非顶层成层 module」里的 inline 子输出也一路上 RTL 顶
⇒ ⇒ 那「成层子树端口收缩」**永远不可能发生**（⇒ 收缩的全部价值消失，且
`⊆` 退化为 `=`）⇒ ⇒ ⇒ 而若「顶层」也不上 ⇒ ⇒ 「逐字节一致」当场不成立。
⇒ ⇒ 两格**各自**由一条判据钉住，互不干扰。

⇒ ⇒ ⇒ **inline 不产生任何 module、任何实例化语句**
⇒ ⇒ ⇒ **inline 的组件不适用 ADR-060 的隐式端口合成 / 冒泡规则**（那两条只对**成层**组件生效）
⇒ ⇒ ⇒ inline 的端口**完全按扁平规则**处理。

⚠️⚠️ **为什么 inline 的输出也必须上顶层（而不是降成内部 wire）—— 这是一个被自审揪出来的矛盾**：
「一个组件 inline ⇒ 它对 RTL 产物**不可见**」与「inline 会**改变端口表**」是**互斥**的
⇒ ⇒ 若 inline 的输出降成内部 wire ⇒ ⇒ **无人消费的子输出会从端口表消失**
⇒ ⇒ ⇒ 而扁平产物里它**是**端口（usage guide §5.3 / `codegen_verilog.cpp:363-370` 只过滤 input）
⇒ ⇒ ⇒ **「默认全 inline ⇒ 与扁平逐字节一致」这条判据当场不成立**（ADR-058 §2.2 ③ 又要求
层次化的顶层端口是扁平的**子集**，两条口径必须对齐）。
⇒ ⇒ ⇒ **裁决**：**inline ⇒ 输出照扁平规则上顶层**；**成层** ⇒ 该子树的端口变成真 module 端口
⇒ ⇒ 其无人消费的输出**不再**上顶层（⇒ 这才是端口表收缩的**唯一**来源）。

⇒ ⇒ ⇒ 结果：两条判据**互为推论**而不是并列要求 ——
「全 inline ⇒ 逐字节一致」由「inline 完全照扁平规则」**直接推出**；
「混合设计 ⇒ `hier.top_ports ⊆ flat.top_ports`」由「成层子树的端口不上顶层」**直接推出**。
⇒ ⇒ 且**端口表收缩这条"好处"只属于成层子树** —— 这是诚实的代价：**想让顶层接口干净，
就必须把边界显式声明出来**。

### 2.3 决策 ③ —— inline 的**悬空输入**：抬成**父的顶层 `input`** + `CHWARN`

> 被 inline 的组件若有**无 driver 的输入端口** ⇒ ⇒ **抬成顶层 `input`**
> （= 扁平规则本身，事实⑤）⇒ ⇒ 并打 `CHWARN` 点名路径；
> `strict` 策略下 MUST 改为 `CHFATAL_EXCEPTION`。

⇒ ⇒ 理由：**扁平产物里它本来就是顶层 `input`** ⇒ ⇒ 不抬 ⇒ ⇒ 该信号在层次化产物里
既不是端口、也没有驱动 ⇒ ⇒ 读值是未定义值 ⇒ ⇒ ⇒ **与扁平产物分叉** ⇒ ⇒
**「全 inline 逐字节一致」直接不成立**（⇒ 这条判据是**推论**，不是并列要求）。

⚠️ **`CHWARN` 不影响「逐字节一致」**：判据比的是**产物文件**；`CHWARN` 是**stderr 诊断**。
⇒ ⇒ 但它**必须存在** —— 「某个层的输入忘了驱动」在多层的扁平产物里表现为**顶层凭空多出
一个 input**（usage guide §5.3 的推论），而那**恰恰是最容易被忽略的一类错误**。

### 2.4 决策 ④ —— 命名：**沿用 ctx 级全局命名表**，层次化 MUST NOT 发明新命名

> 层次化 emitter MUST 复用 `verilogwriter` 的 `node_names_`（按 eval list 顺序全局 `_N` 去重）
> ⇒ ⇒ ⇒ **顶层端口名 == 扁平端口名（逐字）** ⇒ ⇒ ⇒ 下游的按名解析（事实④）不受影响
> ⇒ ⇒ ⇒ 下游 ADR-091 §G-1o 天然满足（事实⑥）。

⇒ ⇒ **推论（承重）**：**默认（全 inline）的层次化产物 MUST 与扁平产物逐字节一致**
⇒ ⇒ ⇒ 这条**零成本、判别力极强**（同时验证 inline 实现与 module 实现）
⇒ ⇒ ⇒ 也是「默认路径零回归」的**技术保证**。

⚠️ **代价**：模块内信号名带**完整路径前缀**（如 `top_p_c_r`）⇒ ⇒ 名字偏长
⇒ ⇒ ⇒ 但**换来了与扁平逐字一致** ⇒ ⇒ ⇒ 且保留「这根线是谁的」线索 ⇒ ⇒ ⇒ **取此舍彼**。

⚠️⚠️ **`node_names_` 的形态是承重的**（v1.3，oracle R6）：`_N` 后缀由「ctx 全局
`get_eval_list()` 顺序」决定（`src/codegen_verilog.cpp:59-88`），而
`declared_nodes_` / `printed_logic_nodes_` / `node_uses_` 都是 writer 的成员状态。
⇒ ⇒ **逐 module 新建 writer** ⇒ `_N` 后缀与扁平不一致 ⇒ 「全 inline 逐字节一致」红 +
下游按名解析（事实④）断掉；**逐 module 复用同一 writer** ⇒ 成员状态未重置 ⇒ 漏声明 / 重声明。
⇒ ⇒ 裁决：`node_names_` MUST 由调用方传一份 **ctx 级只读快照**（一次建好、所有 module 共用）；
writer 的**其余**成员状态 MUST **每 module 显式重置**（或改成无状态、按参数传递）。

### 2.5 决策 ⑤ —— emitter 侧总开关（逃生门 + 对比测试用）

```cpp
enum class Granularity { RespectDeclarations, ForceInline, ForceLayer };
struct VerilogHierOptions {
    std::string out_dir;
    bool          one_file_per_module = true;
    Granularity   granularity = Granularity::RespectDeclarations;
    PortPolicy    port_policy     = PortPolicy::SynthesizeWarn;      // ADR-060
    UndrivenPolicy undriven_policy = UndrivenPolicy::Bubble;          // ADR-060
};
```
⇒ ⇒ `ForceInline` 用于「与扁平逐字节对照」；`ForceLayer` 用于「全成层的极端形态」。

### 2.6 决策 ⑥ —— 成层判定与**装配点解耦**

> 「是否成层」由**声明**决定；**装配点**可以是任意位置
> —— 包括**没有 active parent** 的位置（下游 ChipForge 的桥接 plugin 场景，事实③）
> ⇒ ⇒ ⇒ emitter 通过**组件树**（`children()`）而非 `ch_module` 发现层级
> ⇒ ⇒ ⇒ **组件树必须正确** ⇒ ⇒ ⇒ **ADR-059 是本决策的硬前置**。

⚠️ **「装配点任意」的精确含义（v1.1 订正，critic BLOCKER B2）**：
装配点**可以没有 active parent**，但该组件 **MUST 被登记进组件树**
（三个合法来源：`ch_device<T>` 根 · `ch_module<T>` · 手工 `top.add_child(std::move(u))`）。
⇒ ⇒ **未登记**（`make_unique` + `build(ctx)` 而不 `add_child`）⇒ ⇒
`owner ∉ subtree(top)` ⇒ ⇒ emitter MUST **响亮拒绝**（`CHFATAL_EXCEPTION` + `add_child` 修法）
⇒ ⇒ **MUST NOT** 静默丢弃（否则整块逻辑消失且三条判据**结构性失明** —— ADR-058 §2.1 第二格）。

### 2.7 决策 ⑦ —— 「结构同构」不变式改写

> **RTL module 树 ≡ 「被声明为成层的」组件子树**
> （被 inline 的组件**不出现在** RTL 树里）⇒ ⇒ ⇒ 仍是**可机械判定**的。

---

## 3. 选项对比（**逐条含理由**）

| # | 选项 | 结论 | 理由 |
|---|---|---|---|
| 1 | **默认 inline + 显式成层**（本文） | ✅ **采纳** | ① 默认路径**逐字节不变**（决策④）⇒ ⇒ **零回归**；② 小组件不产生噪声；③ 粒度是**用户的领域决策**（事实②）⇒ ⇒ 框架不猜；④ 下游可**渐进**采用（大单元先成层） |
| 2 | **默认全成层**（ADR-058 v2 原口径） | ❌ 否决 | ① 小组合子全部成层 ⇒ 产物是例化清单的噪声（事实①）；② 用户要**逐个**声明「不成层」⇒ ⇒ ⇒ **默认值让绝大多数用户受益**才合理；③ 默认路径**字节变化** ⇒ ⇒ 全部既有 golden / perf harness 要重基线 |
| 3 | **按端口数 / 节点数自动推断**成层 | ❌ 否决 | ① **不可预测** ⇒ ⇒ 用户无法预测产物长什么样（写法指南 §3 的判据直接失败）；② 平台替用户猜 = 本仓反复吃过的亏；③ 阈值随实现漂移 ⇒ ⇒ ⇒ 产物不可复算 |
| 4 | **类级默认 + 实例级覆盖**（本文） | ✅ **采纳** | ① 组件作者声明一次（大多数用户**不需要**操心）；② 使用方按需覆盖（同一类型在不同位置不同粒度）；③ 两个新 API 名 ⇒ 零破坏 |
| 5 | **仅实例级**（无类级声明） | ❌ 否决 | ① 每个实例化点都要写 ⇒ ⇒ ⇒ 冗长且易漏；② 组件作者无法表达「我天然是一层」 |
| 6 | **仅全局开关**（emitter 侧统一 inline 或统一成层） | ❌ 否决 | ① 粒度是**每个组件**的属性 ⇒ ⇒ 全局开关表达不了；② 但它作为**逃生门**有用 ⇒ ⇒ ⇒ 保留为 `Granularity` 枚举（决策⑤） |
| 7 | **命名沿用 ctx 级全局表**（本文） | ✅ **采纳** | ① 下游按 ctx 级名解析（事实④）⇒ ⇒ per-module 命名会**打断**它；② 顶层端口名与扁平逐字一致 ⇒ ⇒ 下游 G-1o 天然满足；③ 支撑「逐字节一致」判据 |
| 8 | **per-module 局部命名**（压平路径前缀） | ❌ 否决 | ① 与事实④冲突 ⇒ ⇒ 下游解析失败；② 破坏「逐字节一致」⇒ ⇒ 零回归承诺失效；③ 名字不再能看出信号来自哪个组件 |
| 9 | **inline 的悬空端口：抬成父顶层端口**（本文） | ✅ **采纳** | ① 与扁平行为一致 ⇒ ⇒ 等价性判据成立；② 否则该信号在产物里**既不是端口也没驱动** ⇒ ⇒ 读值未定义 ⇒ ⇒ ⇒ 行为分叉 |
| 10 | **inline 的悬空端口：报 `CHWARN` 悬空 / 接 0** | ❌ 否决 | ① 「接 0」= 静默改行为（扁平里它**可驱动**）⇒ ⇒ 分叉；② 「只告警不抬」⇒ ⇒ 顶层不可驱动 ⇒ ⇒ 与扁平不一致 ⇒ ⇒ ⇒ 两者都不可接受 |
| 11 | **inline 的输出降成内部 wire**（→ 端口表收缩） | ❌ 否决 | ① 与**扁平规则**冲突（扁平里任意层 `ch_out` 都是顶层 output）⇒ ⇒ **「全 inline 逐字节一致」当场不成立**；② 「inline 对产物不可见」与「inline 改变端口表」互斥 ③ 端口表收缩是**成层子树**的回报 ⇒ inline 去蹭它 ⇒ ⇒ 用户永远不会去声明成层 |

---

## 4. 后果 / 代价

### 正面
- **默认路径零行为变化**：全 inline 时产物与扁平**逐字节一致** ⇒ ⇒ 既有 golden /
  perf harness / 外部基线**全部不受影响** ⇒ ⇒ ⇒ **整个项目的最大风险被消掉**；
- 小组件**不产生** module / 例化 / 端口表 ⇒ ⇒ 产物回到「人写的 RTL」的样子；
- 粒度是**用户的领域决策** ⇒ ⇒ 框架不猜 ⇒ ⇒ 产物可预测；
- 下游的层 2 IP 单元**可成层**，且**装配点不受 active parent 限制**（事实③）；
- 命名沿用全局表 ⇒ ⇒ 下游按名解析与 G-1o **均不受影响**（事实④⑥）。

### 负面
- 需要**三个** API 名（`ch_module` / `ch_layer` / `ch_inline`）⇒ ⇒ 认知负担；
- 「默认 inline」⇒ ⇒ 层次化能力**默认不被使用** ⇒ ⇒ ⇒ 需要**诊断 API**（用户视角审查 P6）
  + 写法指南来引导；
- 成层判定与装配点**解耦** ⇒ ⇒ ⇒ **强依赖组件树正确**（ADR-059）⇒ ⇒ ⇒
  ⇒ ADR-059 从「层次化的前置」升级为「**下游 Plugin 框架的前置**」；
- **inline 不吃端口表收缩这条好处**（决策②）⇒ ⇒ 想让顶层接口干净，MUST 显式声明成层
  ⇒ ⇒ ⇒ 这是「零回归」的**代价**（持平：换来默认路径逐字节不变 + 好处归属清晰）；
- 模块内信号名带**完整路径前缀** ⇒ ⇒ 名字偏长（决策④的代价，已被接受）。

### 风险

| # | 风险 | 对策 |
|---|---|---|
| 1 | 「默认 inline」被理解为「层次化功能没做完」⇒ ⇒ 用户不用 | MUST 在写法指南给出**成层的判据**（何时该成层）+ 诊断 API 报告「本设计有几层」 |
| 2 | 组件树失真（ADR-059 未修）⇒ ⇒ 成层判定错 ⇒ ⇒ module 划分错 | ADR-059 为**硬前置**（决策⑥）⇒ ⇒ 路径图上 P0 先行 |
| 3 | inline 的输入**上游没驱动** ⇒ 悬空 ⇒ 未定义值 | 决策③（= 扁平规则：抬成顶层 `input` + `CHWARN`）⇒ ⇒ 且等价性判据覆盖 |
| 4 | 「逐字节一致」判据被「这些都算注释差异」平凡满足 | **允许差异 = 空集**（可执行定义 = `cmp` 退出码 0）⇒ ⇒ 若实现**必须**产 banner ⇒ MUST 先改 ADR，MUST NOT 在实现里默默引入 |
| 5 | 用户把**所有**组件都声明成层 ⇒ ⇒ 回到「碎片化」 | 诊断 API 报告每层规模（端口数/节点数）⇒ ⇒ 供用户自查；**MUST NOT** 用启发式替用户裁剪 |

---

## 5. Verification（**前瞻**验收判据，MUST 可勾选 —— 裁决落地后适用）

- [ ] **效果判据（最强）**：**默认（全 inline）** 的层次化产物 MUST 与扁平产物
      **逐字节一致**（同名、同序、同端口表）⇒ ⇒ **允许差异 = 空集**
      （可执行定义 = `cmp` 退出码 0；全 inline 只产 1 个文件）
- [ ] **效果判据**：同一设计 `ForceInline` vs `ForceLayer` ⇒ ⇒ 对显式观测集合
      **逐拍全等**（⇒ ⇒ inline 与成层**语义等价**）
- [ ] **效果判据**：类级 / 实例级三种组合（helper 类 + helper 实例 /
      layer 类 + helper 实例 / helper 类 + layer 实例）⇒ ⇒ 各自的成层结果
      MUST 与声明一致
- [ ] **结构判据**：被 inline 的组件 MUST **不产生** module / 实例化语句 /
      隐式端口 ⇒ ⇒ 其端口 MUST **完全按扁平规则**出现：
      **无 driver 的 input ⇒ 顶层 `input`** · **任意 `output` ⇒ 顶层 `output`**
      （⚠️ 包括**已被父层消费**的输出 —— 见决策②）⇒ ⇒ MUST NOT 降成纯内部 wire
- [ ] **结构判据**：**端口表收缩只发生在成层子树** —— 被成层组件**无人消费的
      子输出** MUST NOT 上 RTL 顶；被 inline 组件**无人消费的子输出** MUST 出现在
      **其最近成层祖先**的端口表里（⇒ 当该祖先是 **RTL 顶层**时即顶层 `output`，
      与扁平一致；当祖先是**非顶层成层 module** 时它是该 module 体内的普通信号）
- [ ] **结构判据**：`make_unique` + `build(ctx)` 但**未 `add_child`** 的组件 ⇒ ⇒
      emitter MUST 抛异常（MUST NOT 静默丢弃）⇒ ⇒ `add_child` 后 MUST 正常产出
- [ ] **结构判据**：被 inline 的组件若有**悬空输入** ⇒ ⇒ 该信号 MUST 出现在
      **顶层端口表** ⇒ ⇒ 且 MUST 有 `CHWARN` ⇒ ⇒ `strict` 下 MUST 抛异常
- [ ] **结构判据**：层次化 MUST NOT 改变**任何端口名** ⇒ ⇒ 顶层端口名 MUST 与
      扁平产物**逐字相同**（⇒ ⇒ 下游按名解析不受影响）
- [ ] **结构判据（`type_output` 三角色 · oracle R5）**：① 自身端口 ⇒ 在端口表；
      ② 隐式输出端口 ⇒ 在端口表且名字是合成名；③ **体内 wire** ⇒ **有 `logic` 声明**
      且**不在**端口表 ⇒ ⇒ 缺声明 MUST 被 `verilator --lint-only -Wall` 抓到
- [ ] **结构判据（`node_names_` · oracle R6）**：全 inline（单 module）与层次化（多 module）
      两条路径产出的**同一节点名 MUST 逐字相同** ⇒ ⇒ 判据 = 两份产物的信号名集合交集
      MUST 完全一致
- [ ] **结构判据**：在**无 active parent** 处装配的组件，MUST 仍能按其声明成层
      （⇒ ⇒ 下游 ChipForge 桥接 plugin 场景）
- [ ] **零回归判据**：`ctest -LE perf` / `run_all_samples.sh` /
      `run_all_ported_tests.sh` 全绿 · 计数锚点同步

---

## 6. 变更记录

| 日期 | 变更 |
|---|---|
| 2026-11-02 | **创建**（用户诉求 + 下游 ChipForge 实证驱动；裁决：层级判定**显式** · 默认 inline · 类级默认 + 实例级覆盖（`ch_layer` / `ch_inline` 两个新 API 名）· **inline 的端口完全按扁平规则**（悬空 input ⇒ 顶层 input；任意 output ⇒ 顶层 output）· 命名沿用 ctx 级全局表（⇒ 默认全 inline 与扁平逐字节一致）· `Granularity` 枚举逃生门 · 成层判定与装配点解耦 · 「结构同构」改写为「成层组件子树」） |
| 2026-11-02 | **v1.2**（据 critic 第 1 轮 1 BLOCKER + 1 SF）：① 决策⑥ 精确化为「**装配点任意，但 MUST 登记进组件树**」⇒ 未登记 ⇒ 响亮拒绝（ADR-058 §2.1 第二格）；② 决策② 的「父 module / 顶层」**二义**消解 ⇒ 明确「最近成层祖先」的**两个作用域**（RTL 顶层 vs 非顶层成层 module）各自落点与判据 |
| 2026-11-02 | **v1.3**（据 oracle 实测）：① **`type_output` 在 module 内有三种角色**（自身端口 / 隐式输出端口 / **体内 wire**），而现状 `print_decl` **只实现第一种** ⇒ 「`print_*` 原样复用」**不成立** ⇒ 新增角色判定表 + `type_output`-as-wire 声明分支；② **`node_names_` MUST 是 ctx 级只读快照**（逐 module 新建 writer 会毁掉 `_N` 一致性 ⇒ 「逐字节一致」红 + 下游按名解析断；复用同一 writer 则成员状态未重置 ⇒ 漏/重声明） |
| 2026-11-02 | **v1.4**（据 ADR-062）：补「**成层**组件的全部 `ch_in`/`ch_out` MUST 成为 module 端口（不论是否被父层驱动）」⇒ 与 inline 的扁平规则**显式分叉**，并给出后果（否则跨 module `assign`） |
| 2026-11-02 | **v1.5**（据 oracle 第 2 轮）：`type_output` 的三角色表补**第四角色** —— 「子端口的连接网」（父 module 内 `logic` + 实例 `.port(net)`，**不是** `assign`）⇒ 否则父层消费子输出会引用未声明标识符 |
| 2026-11-02 | **v1.1**（自审揪出的矛盾修正）：原决策②写「inline 的输出降成父内 wire」，但扁平产物里**任意层** `ch_out` 都是顶层 `output`（`codegen_verilog.cpp:363-370`）⇒ ⇒ ⇒ 「全 inline 逐字节一致」**当场不成立**，且「inline 对产物不可见」与「inline 改变端口表」互斥 ⇒ ⇒ **改裁**：inline **完全照扁平规则**；**端口表收缩是「成层子树」独有的回报**（⇒ 想顶层接口干净 MUST 显式声明成层）⇒ 两条判据从「并列要求」变成「互为推论」（选项表新增 #11 记录被否决的形态） |
