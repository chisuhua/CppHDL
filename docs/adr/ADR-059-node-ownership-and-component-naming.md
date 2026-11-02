# ADR-059: 节点归属与组件命名 —— 用「构造作用域」修正挂错层的缺陷

- Status: **Proposed**（2026-11-02 · 待用户拍板 · **v2**：据对抗审稿重写栈纪律与 context 来源）
- Date: 2026-11-02
- Author: build-agent（architect role）
- Parent: **无**（新增决策）；**服务于** ADR-058（层次化 RTL 输出的硬前置）
- 关联 capability: 待建 `component-ownership-and-naming`
- 关联 change: 待建（见 `.rddf/roadmap/features/feat-hierarchical-rtl-output.md`）
- 关联台账: `docs/architecture/2026-11-02-hierarchical-rtl-output.md` §1.2（四个归属缺陷）· 附录 A（可复算探针）·
  `examples/removed_examples.txt:26`（`ch_device<Rv32iPipeline>` 结构性不可实现的根因）·
  `examples/riscv-mini/tests/test_pipeline_debug.cpp:26-31`（已登记的「挂祖父」注释）
- ⚠️ 本 ADR 同时修掉**四个**已登记缺陷（成员节点挂父 · 成员 `ch_module` 挂祖父 ·
  深度≥3 前缀重复 · 成员 `ch_module` 因 `context()==nullptr` 而早退）⇒ ⇒ ⇒
  它有**独立于 ADR-058 的价值** ⇒ ⇒ ⇒ 必须**单独成 change 先落**。

---

## 1. 背景（**只陈述决策所需的事实**）

### 1.1 实测事实（2026-11-02，全部可复算；探针见架构主题文档附录 A）

| # | 事实 | 证据 |
|---|---|---|
| ① | 节点归属由**建节点时**的 `Component::current()` 决定 | `src/core/lnodeimpl.cpp:25-31` |
| ② | **成员构造早于 `build()`** ⇒ 成员的 `ch_reg`/`ch_mem`/bundle 字段建节点时 `current_` 仍是**父组件** ⇒ 归属错（实测 `top.m.r` 实属 Leaf） | C++ 语义（基类先于成员）+ 探针 |
| ③ | 成员 `ch_module` 的 parent 取 `Component::current()` ⇒ 挂到**祖父** ⇒ 组件树错、实例名重名 | `include/module.h:42` + 探针（`top.children = 4`，两个 `top.leaf`） |
| ④ | `build_hierarchical_name()` 用 `parent->hierarchical_name()`，后者又拼一次已是全路径的 `name_` ⇒ **深度 ≥3 前缀重复**（实测 `top.top.m.lf`） | `include/module.h:21` + `include/component.h:61-73` |
| ⑤ | `Component::ctx_` 在**构造函数里是 `nullptr`**，只在 `build()` 里赋值 ⇒ ⇒ 成员形态 `ch_module` 的「新父组件」在构造期**没有 context** ⇒ ⇒ `include/module.h:49-53` 早退 ⇒ ⇒ **子组件直接消失**（今日靠「挂祖父」恰好掩盖） | `src/component.cpp:13, 105-118` + `include/module.h:49-53` |
| ⑥ | 端口节点归属**正确**（端口在 `create_ports()` 建，那时 `current_` 是本组件）⇒ ⇒ 缺陷只影响「成员形态」的节点 | 探针：`top.m.unnamed_input <- parent=top.m` ✅ |
| ⑦ | **归属被用于语义分流**：`src/simulator_init.cpp:95` 用 `!node->get_parent()` 把节点分到 `input_instr_list_` / `combinational_instr_list_`；`include/core/io.h:360/385/410/432` 的 4 处 `CHREQUIRE` 用 `get_parent() == current_component()` 判连线方向 ⇒ ⇒ **归属真源变更不是纯命名变更** | 源码 |
| ⑧ | `CHREQUIRE` **只打日志不抛异常**（`include/utils/logger.h:192-200`）⇒ ⇒ 归属错**静默**；仓内唯一抛异常的宏是 `CHFATAL_EXCEPTION`（`:235-244`） | 源码 |
| ⑨ | ★ **下游 ChipForge 的层 2 IP 单元在「无 active parent」处装配**：桥接 plugin 在 `setup()` 内 `make_unique` + `build(ctx)` ⇒ ⇒ ⇒ 该点**不在任何 `describe()` 内** ⇒ ⇒ ⇒ **组件树若失真 ⇒ ⇒ ⇒ 层 2 单元的层级归属就错** ⇒ ⇒ ⇒ **本 ADR 不仅是「层次化输出」的前置，更是「下游 Plugin 框架」的前置** | ChipForge ADR-091 v1.15 §D3a.7 · `include/cf/plugin/pipe_builder.h` |

### 1.2 问题（**精确表述**）

1. 「一个节点属于哪个组件」在**成员形态**下是**错的** ⇒ ⇒ ⇒ 任何按归属切分的工具
   （层次化 emitter、层次化 trace、可读性报告）都会**把子组件的状态划进父组件**，
   且**错得很安静**（⑦⑧）。
2. 「组件实例叫什么」在**深度 ≥3** 时**重复** ⇒ ⇒ ⇒ 模块/信号名不可读、不可作切分依据。
3. 「成员 `ch_module` 挂祖父」使**组件树本身**失真 ⇒ ⇒ ⇒ 任何基于树的遍历都错。
4. 「成员 `ch_module` 因 `context()==nullptr` 早退」（⑤）⇒ ⇒ ⇒ **任何把 parent 改成
   「正在构造的那个组件」的修法，若不同时解决 context 来源，都会让旗舰 DUT 的子模块全部消失**。

---

## 2. Decision（**裁决**）

### 2.1 核心裁决

> **引入「构造作用域」**：`Component` 的构造函数进入一个作用域、`build_internal()`
> 进入另一个作用域；**节点归属与 `ch_module` 的 parent 统一取「最内层作用域的组件」**；
> 作用域为空时回落到 `Component::current()`。

```cpp
// include/component.h
class Component {
    static thread_local std::vector<Component*> scope_;   // 作用域栈
public:
    static Component* node_owner() {          // 节点归属的唯一真源
        return scope_.empty() ? current_ : scope_.back();
    }
};
```

⇒ ⇒ **为什么可行**：C++ 保证**基类子对象先于成员构造** ⇒ ⇒ ⇒ `Component` 的构造函数
**一定**早于该组件的所有成员构造 ⇒ ⇒ ⇒ 成员建节点时作用域栈顶**就是正在构造的那个组件**。

⇒ ⇒ **为什么必须配对作用域**（v1 稿的「ctor 入栈 / build 出栈」是**错的**）：
`build()` 可能**被调用两次**（`built_` 早退，`src/component.cpp:99-103`）⇒ 出栈会多弹一帧；
`create_child` **从不 build**（`include/component.h:43-59`）⇒ 该组件**活着期间**长期占据栈顶
⇒ ⇒ ⇒ 后续**每一个**节点的归属都被污染。⇒ ⇒ ⇒ **作用域必须 ctor 进 / ctor 出（配对）**。

| 时点 | 作用域栈顶 | 归属结果 |
|---|---|---|
| `Top` 构造期（成员 `ch_reg`） | `Top`（CONSTRUCT 作用域） | ✅ `Top` |
| `Mid` 构造期（成员 `ch_reg`） | `Mid` | ✅ `Mid` |
| `Mid::describe()` 期间 | `Mid`（BUILD 作用域，`build_internal` 压入） | ✅ `Mid` |
| 栈空（`describe()` 之外、组件外） | 回落 `current_` | ✅ 与今天一致 |

### 2.2 配套裁决（逐条）

| # | 裁决 |
|---|---|
| **D1** | 新增 `Component::node_owner()`；`lnodeimpl` 的 `parent_` 改用它 |
| **D2** | `ch_module` 的 parent 取 `Component::node_owner()`（不再取 `current()`）⇒ ⇒ ⇒ **成员 `ch_module` 挂到正确的父组件** |
| **D2′** | **（v2 新增 · 承重）** `ch_module` 的 **build context 取「环境 context」**（`ch::core::ctx_curr_`），**不再取 `parent_component->context()`** ⇒ ⇒ ⇒ 成员形态的父组件在构造期 `ctx_ == nullptr`（事实⑤）不再导致早退。⚠️ **环境 context 为空时 MUST NOT 硬失败**（v2.2 订正，oracle R8）：下游装配形态是 `make_unique<Unit>()` **然后** `build(ctx)` ⇒ Unit 的 ctor 跑在 `build` **之前** ⇒ `ctx_curr_` 常为 **nullptr** ⇒ ⇒ 若此处硬失败，「含子模块的单元无法在无 active parent 处装配」⇒ ⇒ ⇒ **N2 只对叶子单元成立 = 「装配点任意」是空话**。⇒ ⇒ 裁决：`ch_module` / `Component` ctor **MUST 接受显式 ctx 来源**（构造实参或 `Component::build(ctx)` 前的一次绑定）；取不到**任何** ctx 时才 `CHFATAL_EXCEPTION`，且错误信息 MUST 说明两种合法投喂方式 |
| **D2″** | **（v2.2 新增 · oracle R9）** **跨 ctx 守卫**：若子组件按 env ctx 建进 `ctx1`、宿主之后 `build(ctx2)`（`src/component.cpp:107-118` 直接用 `external_ctx`）⇒ ⇒ 子模块节点留在 `ctx1`、宿主端口在 `ctx2` ⇒ ⇒ `ctx2->get_eval_list()` **看不见子模块** ⇒ **整块逻辑静默消失**，且等价性三条判据**全看不见**（子模块不在树里）。今日不可达（成员 `ch_module` 早退），**D2′ 落地后可达** ⇒ ⇒ MUST 断言「`build(ctx)` 的 ctx == 构造期 env ctx」，不等则 `CHFATAL_EXCEPTION` |
| **D3** | `build_hierarchical_name()` 改用 `parent->name()`（**不是** `hierarchical_name()`）⇒ ⇒ ⇒ **深度 ≥3 不再重复前缀** |
| **D4** | `Component::name()` 语义**不变**（仍是**全路径**）；**新增** `local_name_`（实例局部名）与 `module_type_name()`（模块类型名，可由 `CH_MODULE_NAME("...")` 覆盖；缺省时回退 sanitize(`typeid(*this).name()`)）⇒ ⇒ ⇒ **单一命名真源**：`name()` = 全路径；`local_name()` = 局部。⚠️ **确定性（v2.2 订正，oracle R15）**：`typeid` 的拼写**是实现相关的**（Itanium vs MSVC）⇒ ⇒ 本仓有平台矩阵（macOS-14 已启用、Windows-2022 计划中）⇒ ⇒ ⇒ 「同一设计两次生成字节相同」**只在同一编译器/ABI 内成立**。⇒ ⇒ **成层组件 MUST 显式 `CH_MODULE_NAME`**（缺省 ⇒ 该组件成层时**硬错**，`CHFATAL_EXCEPTION`）⇒ ⇒ 撤销本 ADR 早期「退化可用、MUST NOT 编译失败」的措辞；跨平台 golden 的比较范围 MUST 在契约里写明「同 ABI」 |
| **D5** | **不改** `toVerilog()` 的**语义**（端口筛选、算子映射、always_ff、mem 一律不动）⇒ ⇒ ⇒ 只改信号名 ⇒ ⇒ ⇒ 扁平产物**语义等价、文本有变** |
| **D6** | 作用域纪律：① `Component::Component()` 压入 CONSTRUCT 作用域，**构造函数出口**弹出（配对 ⇒ 无泄漏 ⇒ **无需**析构兜底）；② `build_internal()` 压入 BUILD 作用域，出口弹出；③ **`create_child` 路径不进入任何作用域** ⇒ ⇒ 三条都 MUST 有断言。⚠️ **D6③ 的原始理由已被订正**（v2.1，critic SF）：原文写「它不 build ⇒ **不产生节点**」**是错的** —— `create_child`（`include/component.h:43-59`）会 `make_shared<T>` ⇒ **T 的构造函数照常跑** ⇒ 成员 `ch_reg` / bundle 字段**照样建节点**（架构文档附录 A 的 `top.m.r` 就是 ctor 期建的）。真实情况是「**有节点、无端口**」（`create_ports()` 只在 `build_internal` 里跑，`src/component.cpp:129-140`）⇒ ⇒ ⇒ **层次化 emitter 对「未 build 的子组件」MUST 响亮拒绝**（否则会发出一个无端口 module，或把它的节点当成空气） |
| **D8** | **`ch_module` 的「无 active parent」分支 MUST 从 `CHERROR` 升格为 `CHFATAL_EXCEPTION`**（v2.1，critic SF）：现状 `include/module.h:42-46` 只打日志后 `return`（⇒ 后续 `io()` 解引用空指针 ⇒ SIGSEGV，本仓 `AGENTS.md` 的 ANTI-PATTERNS 已登记该形态）⇒ ⇒ 与本 ADR「响亮失败 MUST 用 `CHFATAL_EXCEPTION`」的**自身原则**不一致 ⇒ ⇒ 同批升格 |
| **D9** | **（v2.2 新增 · 承重 · oracle R7）** `Component::add_child()` MUST 补绑 `child->parent_ = this->weak_from_this()`（同类内可写 protected 成员）。**根因**：`Component` ctor 用 `parent->weak_from_this()`（`src/component.cpp:24`），而成员形态 `ch_module` 先 `make_unique<T>`（`include/module.h:60`）再 `add_child`（`:64`）才转 `shared_ptr` ⇒ ⇒ ctor 期 `weak_from_this()` **为空** ⇒ ⇒ **成员子组件的 `parent_` 永久为空**。⇒ ⇒ 后果：`Rv32iPipeline` 的 7 个成员 `ch_module`（`include/cpu/pipeline/rv32i_pipeline.h:114-123`）改好 parent 之后，`children()` 里**有**它们，但 `parent().lock()` **是 nullptr** ⇒ ⇒ 若 `subtree` / 归属判定按**向上** parent 链实现 ⇒ ⇒ **旗舰 DUT 被 `owner ∉ subtree(top)` 误杀**，且错误信息还叫人去 `add_child`（已经调过）。⇒ ⇒ **同时裁**：`subtree` / 「`owner` 是否在树里」**MUST 只向下走 `children()`** |
| **D10** | **（v2.2 新增 · oracle R11）** `context::init()` 建 `default_clock` / `default_reset`（`src/core/context.cpp:100-116`）MUST 用「**无归属**」哨兵 ⇒ ⇒ 否则某组件在**作用域内**调 `build()`（无 external ctx，`src/component.cpp:111-118`）时，新 ctx 的 clock/reset 会拿到 `owner != nullptr` ⇒ 直接违反 §2.3「context 级节点不归属任何组件」⇒ ⇒ emitter 的 clock/reset 特判失效 |
| **D7** | **归属 ≠ 纯命名**（事实⑦）：归属真源变更 MUST 逐条回归 `src/simulator_init.cpp:95` 的指令表分流与 `include/core/io.h` 的 4 处方向 `CHREQUIRE` ⇒ ⇒ ⇒ **「仿真逐位不变」不是可假定的结论，而是待验证的验收项**（见 §5）。⚠️ **（v2.2 补，oracle R10）** 那 4 处守卫（`include/core/io.h:360/385/410/432`）目前判的是 `lnodeimpl::current_component()`（`src/core/lnodeimpl.cpp:34-36` ⇒ `Component::current()`）⇒ ⇒ **与归属真源分叉** ⇒ 在 ctor 期接线的组件上会**误报**；而 `CHREQUIRE` **只打日志不抛** ⇒ ⇒ 判据「触发面逐项一致」**无法收敛**（新误报让 diff 永远不等）。⇒ ⇒ 裁决：4 处守卫**改用 `node_owner()`**，且判据改为「**改前改后均无触发**」（而不是「触发面相同」） |

### 2.3 归属定义的**唯一性**

> **组件级节点**（建在某个组件的作用域内）归属于该组件；
> **context 级节点**（`default_clock` / `default_reset`，建在 `context::init()`，
> `src/core/context.cpp:106-115`，**早于任何组件**）**不归属任何组件** ⇒ ⇒ ⇒
> emitter 对它们**单独处理**（含时序逻辑的模块各自合成一对 clock/reset 端口）。

⚠️ **不设「事后归属规则」**（不写「`parent_ == nullptr` ⇒ 归顶层」）⇒ ⇒ ⇒
归属**只由作用域栈决定**，**不需要任何事后修正**（不改名、不改指针）⇒ ⇒ ⇒ 代价低、可预测。

---

## 3. 选项对比（**逐条含理由**）

| # | 选项 | 结论 | 理由 |
|---|---|---|---|
| 1 | **配对作用域**（本文）：ctor 进/ctor 出 + `build_internal` 压 BUILD 作用域，归属取最内层 | ✅ **采纳** | ① **零 API 变更**（`ch_module` / `ch_device` 用法一字不变）；② **不改构造顺序** ⇒ ⇒ 节点创建序不变 ⇒ ⇒ 爆炸半径最小；③ 归属由 C++ 构造顺序**唯一确定** ⇒ ⇒ 无歧义；④ 配对 ⇒ **无泄漏**（v1 稿的「ctor 进/build 出」在 `build()` 二次调用与 `create_child` 路径上都会失衡） |
| 2 | **延迟构造子组件**（`ch_module` 延后到父 `build()` 内、`describe()` 之前才建） | 🟡 **仅作 D2′ 的备选** | ① 它能解决「成员形态 context 未就绪」；② 但 **D2′（context 取环境值）以更小的改动达到同一效果** ⇒ ⇒ ⇒ 首选 D2′；③ 保留此选项是因为：若 D2′ 被否决（环境 context 不可靠），它就是退路；④ 代价：改节点创建顺序 ⇒ ⇒ 扁平产物文本变（与 D5 已接受的方向一致，但面更大） |
| 3 | **事后修正**：遍历节点、按前缀/时间戳反推归属 | ❌ 否决 | ① 归属变成**推断**而非**记录** ⇒ 不可靠（同名 / 跨层 / 复用节点都误判）；② 需要额外全局注册表 ⇒ 又是状态；③ 静默出错 ⇒ 与本仓「假绿」病症同型 |
| 4 | **不改归属，只在 emitter 侧用图割兜底** | ❌ 否决 | ① 图割对**死逻辑**与**成员节点**都判不准 ⇒ 需两套规则并存 ⇒ 不可预测；② **组件树仍然失真** ⇒ ⇒ 任何基于树的遍历都错 ⇒ ⇒ ⇒ **问题被推迟，不是被解决** |
| 5 | **只改命名、不改归属**（D3 单独成 change） | ✅ **采纳为独立子项** | ① 它只修 ④（深度≥3 前缀重复）⇒ **零语义变更**、可独立验收；② 但**不**修 ②③ ⇒ ⇒ 层次化 emitter 仍无法工作 ⇒ ⇒ ⇒ **必须**做选项 1 ⇒ ⇒ ⇒ 实施时 D3 单独成 change（见路径图 ①a） |

---

## 4. 后果 / 代价

### 正面
- **节点归属**正确且可判定 ⇒ ⇒ ⇒ 层次化 emitter / 层次化 trace / 可读性报告有了可靠基础；
- **组件树**恢复真实 ⇒ ⇒ ⇒ 「成员 `ch_module` 挂祖父」缺陷消除 ⇒ ⇒ ⇒
  **配合 D2′**，成员形态组件（如 `Rv32iPipeline`）**首次可作为顶层实例化**
  ⇒ ⇒ ⇒ `examples/removed_examples.txt:26` 的归位条件**解除**；
- **深度 ≥3 的实例路径**不再重复 ⇒ ⇒ ⇒ 日志、trace、Verilog 名**可读**；
- **零 API 变更**、**不改构造顺序** ⇒ ⇒ ⇒ 回归面只剩「名字变了」与「归属变了」两类。

### 负面
- **Verilog 文本会变**（信号名）⇒ ⇒ ⇒ Verilator 缓存 key 失效 ⇒ ⇒ 一次冷编译；
- **归属变了 ⇒ 语义分流可能变**（事实⑦）⇒ ⇒ ⇒ 「仿真逐位不变」**必须当作待验证项**（D7），
  **不得**当作假定；
- `Component` 的生命周期多了一条**作用域纪律**（D6）⇒ ⇒ ⇒ 新增一个必须被测试覆盖的不变式；
- 新增 `local_name_` ⇒ ⇒ ⇒ `Component` 的内存布局变化（对 `sizeof` 敏感处需复核）。

### 风险

| # | 风险 | 对策 |
|---|---|---|
| 1 | 作用域**不配平** ⇒ 后续节点归属整体偏移（静默） | D6 三条纪律 + **MUST 用 `CHFATAL_EXCEPTION`** 断言（`CHREQUIRE` 不抛异常 ⇒ 抓不住） |
| 2 | D2′ 让 `ch_module` 在**环境 context 为空**时构造子组件 | MUST `CHFATAL_EXCEPTION`（响亮）⇒ **MUST NOT** 静默自建子 context（那会让节点分裂到另一个 ctx） |
| 3 | 归属真源变更 ⇒ `simulator_init` 的指令表分流与 `io.h` 的方向守卫语义漂移 | D7 ⇒ P0 内**逐条回归**（`type_input` 节点的分类 + 4 处 `CHREQUIRE` 的触发面） |
| 4 | 名字变更打破**依赖具体名字**的脚本/测试 | ⚠️ **实测**存在 4 处（见 §5「文本变化面」）⇒ MUST **逐条复核**，**不得**整体豁免 |
| 5 | `create_child` 路径不进入作用域 ⇒ 其**构造期建出的节点**归属取栈顶（= 父组件）⇒ ⇒ ⇒ 若该子组件**永不 `build`**，它「**有节点、无端口**」⇒ ⇒ 层次化 emitter 会发出无端口 module（或把它的节点当成空气） | D6③ 明确「不进入作用域」⇒ ⇒ 且 **emitter MUST 对「未 build 的子组件」响亮拒绝**（§5 判据）。⚠️ **不要**把本条读成「无暴露面」—— `create_child` 会 `make_shared<T>` ⇒ 构造函数照常跑（v2.1 订正） |

---

## 5. Verification（**前瞻**验收判据，MUST 可勾选 —— 裁决落地后适用）

- [ ] **效果判据**：三层设计（Top → Mid → Leaf）⇒ **Leaf 的 `ch_reg` 成员的
      `get_parent()` MUST 是 Leaf**（不是 Mid）⇒ ⇒ 其 Verilog 名 MUST 含 `leaf` 段
- [ ] **效果判据**：**成员形态**的 `ch_module` ⇒ MUST 挂到**直接父组件**
      （`top.m.children()` MUST 含该实例）⇒ ⇒ MUST NOT 出现在祖父的 `children()` 里
- [ ] **效果判据**：**成员形态** `ch_module` 的子组件 MUST **可 `io()`** 且
      `instance()` 非空 ⇒ ⇒ ⇒ 覆盖事实⑤的早退路径
- [ ] **效果判据（下游场景）**：在**无 active parent** 处装配的组件（事实⑨）⇒ ⇒
      其 `children()` MUST **正确登记**在它自己下面 ⇒ ⇒ ⇒ 且其节点的 `get_parent()`
      MUST 是它 ⇒ ⇒ ⇒ （这条直接决定 ChipForge 的层 2 IP 单元能否成层）
- [ ] **效果判据**：**顶层 `ch_device<Pipeline>`**（成员形态组件）MUST **可实例化且不 SIGSEGV**
      ⇒ ⇒ ⇒ `examples/removed_examples.txt:26` 的归位条件**解除**
- [ ] **效果判据**：**深度 ≥3** 的实例路径 MUST **无重复前缀**（`top.m.lf` 形态）
- [ ] **效果判据**：**归属 ⇒ 语义分流**（事实⑦）⇒ `type_input` 节点在
      `input_instr_list_` / `combinational_instr_list_` 的归属 MUST 与变更前**逐项一致**
      ⇒ ⇒ 且 `include/core/io.h` 的 4 处方向 `CHREQUIRE` 的**触发面** MUST 不变
      （⇒ 以「变更前后各跑一次并 diff 触发日志」为判据）
- [ ] **结构判据**：作用域 MUST **配平** ⇒ ⇒ 任意构造/析构序列后作用域栈 MUST 为空
      ⇒ ⇒ MUST 有「故意 `create_child` 不 build」的用例（栈不得被污染）
- [ ] **结构判据（异常路径）**：**构造函数抛出**时作用域 MUST 由 RAII **自动回退**到
      进入前深度 ⇒ ⇒ 且此后新建节点的归属 MUST 仍正确
      （⚠️ 不写「栈非空 ⇒ 抛异常」—— 配对作用域下它**不可触发**，见 change spec）
- [ ] **结构判据**：**未 build 的子组件**（`create_child` 后从不 `build`）⇒ ⇒
      `create_ports()` 从未跑 ⇒ **有节点、无端口** ⇒ ⇒ 层次化 emitter MUST **响亮拒绝**
- [ ] **结构判据**：`ch_module` 的「无 active parent」分支 MUST 抛异常
      （`CHFATAL_EXCEPTION`）⇒ MUST NOT 只打日志后返回
- [ ] **结构判据**：`ch_module` 的 parent MUST 取 `node_owner()`；其 build context
      MUST 取环境 context ⇒ ⇒ MUST NOT 取 `parent_component->context()`
- [ ] **结构判据（D9 · oracle R7）**：**成员形态** `ch_module` 的 `parent().lock()` MUST **非空**
      ⇒ ⇒ 判据 = `ch_device<Rv32iPipeline>` 下每个成员子组件的 `parent().lock() == 父组件`
      （⚠️ 按 `children()` 反查 `children()[i]->parent().lock()`）
- [ ] **结构判据（D9）**：`subtree` / 归属判定 MUST **只向下走 `children()`**
      ⇒ ⇒ MUST NOT 用 parent 链（否则成员子组件（`parent_` 空）被误杀）
- [ ] **结构判据（D2″ · oracle R9）**：`build(ctx)` 的 ctx MUST == 构造期 env ctx
      ⇒ ⇒ 不等 MUST `CHFATAL_EXCEPTION`（否则子模块节点留在另一个 ctx ⇒ 逻辑静默消失）
- [ ] **结构判据（D2′ · oracle R8）**：**含成员 `ch_module` 的组件** MUST 能在
      `make_unique` + `build(ctx)` 形态下装配成功（⇒ `ctx_curr_` 在 ctor 期为 null 也 MUST NOT 硬失败）
- [ ] **结构判据（D10 · oracle R11）**：`context::init()` 建的 clock/reset MUST `owner == nullptr`
      ⇒ ⇒ 即使在**作用域内**调 `build()` 也 MUST 保持无归属
- [ ] **结构判据**：`build_hierarchical_name()` MUST 用 `parent->name()`
      ⇒ ⇒ MUST NOT 用 `hierarchical_name()`
- [ ] **结构判据**：归属断言 MUST 用 `CHFATAL_EXCEPTION`（**MUST NOT** 用 `CHREQUIRE`）
- [ ] **零回归判据**：`ctest -LE perf` 全绿；`run_all_samples.sh` /
      `run_all_ported_tests.sh` 全绿；`scripts/verify_test_counts.sh` 三锚点同步
- [ ] **零回归判据**：**文本变化面 MUST 逐条登记** ⇒ 至少覆盖这 4 处实测依赖：
      `tests/test_axi_spi.cpp:213-214`（断言生成 Verilog 含 `top_unnamed_output` /
      `top_unnamed_input`）· `tests/benchmark/verilator_harness_tc07.cpp:33` 与
      `tc08.cpp:51`（引用 Vtop 字段 `top->top_unnamed_output`）·
      `tests/test_verilator_backend.cpp`（断言 `module top` / `default_clock` / 端口名）
      ⇒ ⇒ **MUST NOT** 用「仓内零处断言具体名字」这种未复算的结论豁免

---

## 6. 变更记录

| 日期 | 变更 |
|---|---|
| 2026-11-02 | **创建**（裁决：引入「构造作用域」· 归属与 `ch_module` parent 统一取最内层作用域 · `build_hierarchical_name` 改用 `name()` · 新增 `local_name_` / `module_type_name()` · 扁平语义不变） |
| 2026-11-02 | **v2**（据对抗审稿）：① 「ctor 进/build 出」改为**配对作用域**（ctor 进/ctor 出 + `build_internal` 压 BUILD 作用域）—— 原方案在 `build()` 二次调用与 `create_child` 不 build 两条路径上失衡；② **新增 D2′**：`ch_module` 的 build context 取**环境 context**（否则事实⑤会让成员形态子组件**直接消失**）⇒ 并据此保留「顶层 `ch_device<Pipeline>` 可实例化」这条承诺；③ **新增 D7**：归属真源变更**耦合**到 simulator 指令表分流与 `io.h` 方向守卫 ⇒ 「仿真逐位不变」降级为**待验证项**；④ 「仓内零处断言具体名字」的错误 grep 结论改为**4 处实测清单**；⑤ 探针证据改为指向架构主题文档**附录 A**（可复算） |
| 2026-11-02 | **v2.1**（据 critic 第 1 轮 SF）：① **订正 D6③ 的事实错误** —— `create_child` 会构造组件 ⇒ 成员节点**照样建**（原写「不产生节点」）；真实缺陷是「有节点、无端口」⇒ ⇒ **新增：未 build 的子组件 MUST 被 emitter 响亮拒绝**；② **新增 D8** —— `ch_module` 的「无 active parent」分支 MUST 由 `CHERROR` 升格为 `CHFATAL_EXCEPTION`（与本 ADR 自身原则一致）；③ 验收判据补「异常路径的栈回退」与「未 build 子组件」 |
| 2026-11-02 | **v2.2**（据 oracle 实测，5 条）：① **D2′ 改判** —— env ctx 为空时 **MUST NOT 硬失败**（否则「含子模块的单元」无法在无 active parent 处装配 ⇒ N2 只对叶子成立）；② **新增 D2″** 跨 ctx 守卫（`build(ctx)` 的 ctx 必须等于构造期 env ctx，否则子模块逻辑**静默消失**）；③ **新增 D9** —— `add_child` MUST 补绑 `child->parent_`，且 `subtree` 判定 MUST **只向下走 `children()`**（根因：成员 `ch_module` 在 `make_unique` 期 `weak_from_this()` 为空 ⇒ `parent_` 永久空 ⇒ 旗舰 DUT 会被误杀）；④ **新增 D10** —— context 级 clock/reset MUST 用「无归属」哨兵；⑤ **D4 加确定性约束** —— `typeid` 拼写实现相关 ⇒ 成层组件 MUST 显式 `CH_MODULE_NAME`，跨平台 golden 限定「同 ABI」；⑥ **D7 加** —— `io.h` 四处守卫改 `node_owner()`，判据改「改前改后均无触发」 |
