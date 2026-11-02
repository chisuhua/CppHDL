# ADR-062: 层次化单元的**接口类型** —— 结构化端口（`Payload<T>` 不可充当接口）

- Status: **Proposed**（2026-11-02 · 待用户拍板 · **v1.3**：下游调研产出 + critic 5 轮收敛）
- Date: 2026-11-02
- Author: build-agent（architect role）
- Parent: 无（新增决策）；**约束** ADR-060 §2.1（成层组件的端口角色）·
  **补强** ADR-061 决策②（inline 与成层的端口规则分界）
- 关联 capability: `rtl-hierarchical-output`（本 ADR 的主张会作为其 Requirement 落地）
- 关联台账: `docs/architecture/2026-11-02-hierarchical-rtl-output.md` §2（需求 N1–N8）·
  `openspec/changes/hierarchical-rtl-output-2026-11/`
- 下游依据: ChipForge `docs/architecture/adr/ADR-091-chmem-open-core-port-architecture.md`
  §D3a.7（两层范式）· 硬约束 **G-1q**（层 2 输入 MUST NOT 走 `Payload<T>`）·
  `include/cf/plugin/payload.h` · `ip/cpu/core/regfile_bridge_chmem.h`

---

## 1. 背景（**只陈述决策所需的事实**）

### 1.1 下游的现状：两层范式

ChipForge 的 ADR-091 §D3a.7 把可复用单元分成两层：

| 层 | 载体 | 通道 |
|---|---|---|
| **层 1 · 核内 stage 间** | `cf::plugin::PluginBase` + `at_stage` 闭包 | **`Payload<T>`**（全局静态对象作 key，每 `PipeNode` 一个 `PayloadStore`） |
| **层 2 · 可复用 IP 单元** | `ch::Component` 子类 + `__io(...)` 的 `io()` struct | 桥接 plugin：单元 `io()` ← 组合端口；单元输出 → 发布成 `Payload<T>` |

### 1.2 实测事实

| # | 事实 | 证据 |
|---|---|---|
| ① | `Payload<T>` 的 key 与 store **都在 host 侧**：`PayloadKeyBase` 只持 `type_info` + 名字，`PayloadStore` 的 cell 是 `std::any` | `include/cf/plugin/payload.h:48-71`（key）· `:112-180`（TLM 模式 store） |
| ② | CH_MEM 模式下 cell 装的是 `ch_uint<N>` / `ch_bool` **句柄** ⇒ 背后的 lnode 在 ctx 里，但 **`Payload` 本身不是 `lnodeimpl`、更不是 port 节点** | `include/cf/plugin/payload.h:9,16-18,95-110` |
| ③ | `PayloadStore` 的实例由 **`PipeNode` 持有** ⇒ **独立 `ch_device<Comp>` 场景根本没有 `PayloadStore`** | ADR-091 v1.15 §D3a.7 第 2 条「分层根」 |
| ④ | `Payload` 的写入发生在 **`at_stage` 闭包**（`elaborate()` 期），而层 2 的 `describe()` 在 **`build()`** 期跑 ⇒ 早读 ⇒ `has()==false` ⇒ **永久 fail-open** | ADR-091 **G-1q**（A4-3 实测） |
| ⑤ | 层 2 的实例化点是**桥接 plugin 的 `setup()`**：`make_unique<RegFileComponent>(nullptr,"regfile")` + `build(pb.ctx())` ⇒ **无 active parent、未登记进组件树** | `ip/cpu/core/regfile_bridge_chmem.h:124-125` |
| ⑥ | 同一个字段组今天要**手写四遍**：单元的 `__io(...)`、桥接的 `Ports`（`ch_logic_in`）、`ObsPorts`（`ch_out`）、payload key 及其 `put/get` | 同上文件 `:102-140`（`__io`）· `:99-118`（`ObsPorts`）· `:130-140`（逐字段 `<<=`） |
| ⑦ | **层次化 emitter 从「端口节点」推导 module 端口** —— 不是 port 节点的一切结构信息对它**不可见** | ADR-058 §2.1（模块边界 = 归属边界）· ADR-060 §2.1（端口表来源） |
| ⑧ | ChipForge 已自裁：层 2 **输入** MUST 走组合节点、`Payload<T>` **只用于层 1 内部**（G-1q） | ADR-091 v1.14 §D3a.7 行「层 2 输入的驱动源」 |

### 1.3 问题（**精确表述**）

ADR-091 的 §D3a.7 表格同一节里有两句互相收紧的话：
① 「**接口载体 = 纯 `Payload<T>`**」（用户裁决 B，2026-10-29）；
② 「层 2 的**输入** MUST NOT 用 `Payload<T>`」（G-1q，2026-10-30，A4-3 实测）。

⇒ ⇒ 两句合起来只能读成「**`Payload<T>` 单向（只作输出通道）**」——
但 §D3a.7 没有把这个结论写成契约，且**没有回答**「输入方向既然不能用 Payload，
那**用什么**」。⇒ ⇒ 本 ADR 回答这个问题，并给出**上游侧的等价约束**。

---

## 2. Decision（**裁决**）

### 2.1 决策 ① —— 层次化单元的接口 MUST 是**结构化端口**

> 一个单元的对外接口 = 它的 `ch::Component` 子类上 **`ch_in` / `ch_out` 端口节点**
> （即 `__io(...)` 的 `io()` struct 的**端口字段**）。
> ⇒ ⇒ **单元是「成层」的 ⇒ 它的全部 `ch_in` / `ch_out` MUST 成为该 module 的端口**
> （**不论是否已被父层驱动**）。

⚠️ **这一条与扁平规则的差异（承重）**：扁平 codegen 把**有 driver 的 `ch_in` 过滤成内部 wire**
（`src/codegen_verilog.cpp:333-360`）⇒ ⇒ 若把这个规则照搬到**成层**组件 ⇒ 它的输入端口会
**全部消失** ⇒ 父层的驱动表达式落在**另一个 module 的体内** ⇒ 跨 module 赋值 ⇒ **非法 Verilog**。

⇒ ⇒ 故端口规则**必须按粒度分叉**（ADR-061 决策② 的两作用域表 + 本条）：

> ⚠️ **本表的适用范围是「**成层且非根**」与「inline」两类**非根** module**
> ⇒ ⇒ **RTL 顶层 module 整行交回扁平规则**（见下方单独一段）
> ⇒ ⇒ 「`⊇` 自身 `ch_in` ∪ `ch_out`」这条判据**只对成层且非根的 module 成立**
> （critic 第 3 轮 B1：对根也要求它 ⇒ 与「全 inline ⇒ `cmp` 0」**互斥**）。

| 组件粒度 | `ch_in`（**有** driver） | `ch_in`（**无** driver） | `ch_out` | **父侧落点** |
|---|---|---|---|---|
| **RTL 顶层 module**（根，**永远按扁平规则**） | 体内 **wire**（`codegen_verilog.cpp:349-358`） | 顶层 `input` | 顶层 `output`（**含全部 inline 后代的 `ch_out`**） | —（无父） |
| **inline**（在非根 module 内） | 该 module 内 **wire** | 该 module 的 `input`（无人驱动 ⇒ 冒泡） | 该 module 体内的**普通信号**（**不**上端口表） | —（同 module 内） |
| **成层**（非根） | **该 module 的 `input` 端口** | 该 module 的 `input` 端口（⇒ 向上冒泡） | 该 module 的 `output` 端口 | `ch_out` ⇒ **父 module MUST 声明一根 `logic` 网**并写 `.port(net)`（Verilog 不允许把实例输出直接接表达式）；`ch_in` ⇒ **父侧可直接接表达式**（⇒ MUST NOT 强制造网，critic SF） |

⚠️ **根 module 必须单列**（oracle R1/R9）：它是唯一「MUST 与扁平逐字节一致」的那个
⇒ ⇒ 它的端口规则**就是扁平规则本身**（clock → reset → 无 driver 的 `input` → **全部** `ch_out`，
含 inline 后代的）⇒ ⇒ **MUST NOT** 套用「成层组件的全部 `ch_in` 都成端口」这条
（否则根 module 的端口表与扁平不同 ⇒ 「逐字节一致」当场破产）。

⚠️ **「三处发射点 × module 角色」—— 实现 MUST 按角色重写，不能沿用现有判据**（oracle R2）：

| 对象 | `print_header`（端口表） | `print_decl`（声明） | `print_logic`（assign） |
|---|---|---|---|
| **成层** module 的 `ch_in`（**有** driver） | ✅ 是端口 | ❌ **不**声明 wire | ❌ **不**发 `assign`（⇒ 否则在子 module 内对 `input` 赋值 = 非法；在父 module 内 = 跨 module `assign`） |
| **成层** module 的 `ch_in`（无 driver） | ✅ 是端口 | ❌ | ❌ |
| **父 module** 内的子 `ch_out` 连接网 | ❌ | ✅ **`logic`** | 由**实例化语句**的 `.port(net)` 驱动（⇒ **不是** `assign`） |
| **inline** 组件的 `ch_in`（有 driver） | ❌ | ✅ `logic` | ✅ `assign`（扁平规则） |

⇒ ⇒ ⚠️ 现有 `print_decl` 的 `type_input` 分支（`:430-446`）与 `print_logic` 的 `type_input` 分支
（`:613-663`）**都按 `driver() != nullptr` 决定行为** ⇒ ⇒ 若逐 module 复用而不改判据
⇒ 子 module 的端口会被**同时**声明成 `input` 与 `logic`（双声明），且被发一条
`assign <input> = <driver>`（**非法**）。⇒ ⇒ 这两个分支 MUST 改成「**按 module 角色**」判定。

⚠️ **自驱动 `ch_in`（driver 在**它自己**的体内）—— 仅「成层且非根」—— MUST 响亮拒绝**
（⇒ **根 module 例外**：它按扁平规则，被驱动的 `ch_in` 就是**体内 wire**，合法）：
若一个**成层**组件自己驱动自己的 `ch_in`（driver 节点 `owner` == 它自己）
⇒ ⇒ 「子 module 内不发 `assign`」与「父层用那个 driver 生成实例连接」**两条都无定义**
（⇒ driver 在子体内、父层引用不到；而在子 module 内对 `input` 赋值 = **非法 Verilog**）
⇒ ⇒ 故：**成层组件的 `ch_in` 的 driver 若在本 module 内 ⇒ `CHFATAL_EXCEPTION`**
（⇒ 提示改成内部 wire 或改用 `ch_out` 中转）。

⇒ ⇒ **实现要点**：对成层组件的 `ch_in`，emitter MUST **忽略**它的 `inputimpl::driver()`
（⇒ 只在**父层**用那个 driver 生成实例连接），**MUST NOT** 调用 `set_driver` 改 IR
（ADR-060 §2.1 的「只许发射期虚拟重定向」）。

### 2.2 决策 ② —— `Payload<T>` **不可**充当层次化单元的接口

| # | 理由 | 证据 |
|---|---|---|
| **R1** | **`Payload` 本身不产生端口、也不携带「该单元需要这个口」的契约** ⇒ 单元对外接口**完全由实现是否恰好有 `ch_in`/`ch_out` 决定** ⇒ 两种失败态：① 输入方向无 `ch_in` ⇒ **无口可连**（值只从 host 侧来）；② 输出方向**有**口但**契约不可见**（名/方向/宽度由实现随手定） | 事实①②⑦ |
| **R2** | **读取依赖 `PayloadStore` 实例**（⇒ 该实例由 `PipeNode` 持有，是**框架接线**属性而非 `Payload` 类型属性）⇒ 单元离开 PipeNode（独立 `ch_device` / 独立 module）就**读不到** | 事实③ |
| **R3** | **时序错位**：写发生在 `elaborate()`、读发生在 `build()` ⇒ fail-open | 事实④ |
| **R4** | 接口**无方向 / 无宽度 / 无端口名**这些结构元数据 ⇒ emitter 无从发射 | 事实① |

⚠️⚠️ **R1 的精确表述**（oracle R5 订正）：CH_MEM 模式下 `PayloadStore` 的 cell 装的是
`ch_uint<N>` **句柄** ⇒ 背后的 lnode **在 ctx 里**，且下游实测 cell 里装的**正是**
`type_output` 节点（`regfile_bridge_chmem.h:203-210` 把 `io().rs1_data` 写进 payload）
⇒ ⇒ **所以不能说「用了 Payload 就一定 0 端口」**：输出方向反而**必须先有** `ch_out` 作源。
⇒ ⇒ 正确的否决理由是：**Payload 不构成接口声明** —— 它既不新增端口、也不声明「需要哪些口」
⇒ ⇒ 一个「只用 Payload 读写」的单元，其端口集合**由实现细节偶然决定** ⇒
**不可交付、不可综合、不可复用**（N1 失败）。

⇒ ⇒ **保留**：`Payload<T>` **仍然是层 1 内部（stage 间）的通道**，也**仍然是层 2 → 层 1 的
输出发布通道**（经桥接 plugin）—— 这两条与 G-1q 一致，**不推翻**。

### 2.3 决策 ③ —— 层 2 的输入方向：**组合端口直连**（采纳下游 G-1q 并给出理由）

> 层 2 单元的输入 MUST 由**组合节点**驱动（顶层 `ch_out` / 另一单元的 `ch_in` / 表达式），
> **MUST NOT** 经由 `Payload<T>` 读取。

⇒ ⇒ 上游侧的理由（**与下游 G-1q 的时序理由独立，且更强**）：
**Payload 不是端口 ⇒ 路径上没有任何 module 端口 ⇒ 层次化产物里这条依赖会「跨 module 消失」**
（R1）⇒ ⇒ 即使时序问题（R3）被下游的两阶段 `describe()` 修掉，**R1/R2/R4 仍然成立**
⇒ ⇒ **这是「结构性」而不是「时序性」的否决**。

### 2.4 决策 ④ —— **更优雅的做法**：单一声明派生（**建议**，非强制）

今天同一个字段组要手写四遍（事实⑥）。建议（**下游可选采纳**）：

```cpp
// 一个字段表 ⇒ 派生三样东西（编译期）
CH_INTERFACE(RegFileIface,
    IN (ch_uint<5>,  rs1_addr),
    IN (ch_uint<5>,  rs2_addr),
    IN (ch_uint<5>,  rd_addr),
    IN (ch_uint<32>, rd_data),
    IN (ch_bool,     rd_we),
    OUT(ch_uint<32>, rs1_data),
    OUT(ch_uint<32>, rs2_data))
```

⇒ ⇒ **派生 ①（上游）**：单元 `__io(...)` 的端口声明 + 一张**编译期字段表**
（⇒ 层次化 emitter 直接可见；字段表可供 `static_assert` 自检）
⇒ ⇒ **派生 ②（下游）**：桥接 plugin 的 `Ports` / `ObsPorts` 结构（同名字段同类型）
⇒ ⇒ **派生 ③（下游）**：payload key 的绑定（`at_stage` 里的 `put`/`get`）

⚠️ **派生 ③ MUST NOT 划给上游**（oracle R6）：`cf::plugin::Payload` 在**下游**
（`payload.h:35` 只 include `<ch.hpp>`，反向不成立）⇒ 上游生成它 = **分层倒置**。

#### 规范段（**MUST**，义务方写明）

| # | 条款 | 义务方 |
|---|---|---|
| N-1 | **payload key MUST 是 per-instance** —— `Payload` 按**静态对象地址**比身份、`PayloadStore` 是 **per-PipeNode**（`payload.h:48-71, 112-180`）⇒ 同类型两实例共用一份静态 key ⇒ 同一 stage 内**静默串味** | **下游** |
| N-2 | **端口名唯一性 MUST 由「实例路径名」保证** —— 即 ctor 拿到的名字 MUST 含父路径（见决策⑤）；⚠️ **MUST NOT** 要求「端口名里带**字段名**」（⇒ `__io` 只把字段声明内联（`include/core/io.h:312-319`），**字段名对框架不可见**，C++ 无反射 ⇒ 除非上 `CH_INTERFACE`，否则该要求**无机制可实现**，critic SF） | 上游（命名）+ 下游（装配） |
| N-3 | 接口字段名与顺序 MUST 稳定（⇒ 桥接按**同名同序**映射；`create_ports()` 的 aggregate 初始化**不区分类型** ⇒ 错序=**静默接错线**，`regfile_chmem.h:130-138`） | 上游 |

#### 可选段（**MAY**）—— `CH_INTERFACE` 宏本身

⚠️ 采纳 `CH_INTERFACE` 的**唯一**额外收益是「字段名可读 + 三处派生不漂移」；
⇒ ⇒ **不采纳也必须守 N-1/N-2/N-3**（critic SF：两条硬约束曾被塞进「建议」里）。

⚠️ **`CH_INTERFACE` 若是宏，两个已知坑**：字段类型里的逗号会拆参数（须括号保护）；
`create_ports()` 的 aggregate 初始化**按成员顺序且不做类型区分**
（下游 `regfile_chmem.h:130-138` 已注明）⇒ 字段错序会**静默接错线**
⇒ ⇒ 故 MUST 有 `static_assert` 的字段名表自检。

⚠️ **为什么不是「bundle 作接口」**：bundle 成员跨层会走 **ADR-060 §2.1 的隐式端口**
（+ `CHWARN`），且 **bundle 作顶层/单元 IO 不会自动成为端口**
（根 `AGENTS.md` 的 ANTI-PATTERNS）⇒ ⇒ 用 bundle 当接口 = 把「显式端口」换成
「隐式端口 + 告警」⇒ **更贵不更好**。

⚠️ **为什么不采用「纯 `Payload<T>` 接口」**：见决策②。

### 2.5 决策 ⑤ —— 下游的装配形态 MUST 追加 `add_child`

> `make_unique<Unit>() + build(ctx)` 的单元 **MUST 被登记进组件树**，否则层次化 emitter
> **响亮拒绝**（ADR-058 §2.1 第二格）。
> ⚠️ **「登记先于 build」单独**解决不了**命名**（critic 第 3 轮 B6）**：
> `add_child` 只 `push_back`、**不碰 `name_`**（`src/component.cpp:161-169`），
> 而 `name_` 由 **ctor 实参逐字**决定（`src/component.cpp:13`），节点名 = `parent_->name() + "." + name_`
> （`src/core/lnodeimpl.cpp:25-28`）⇒ ⇒ **顺序换了也不产生父路径前缀**。
> ⇒ ⇒ **真正决定前缀的是「ctor 拿到的名字字符串」**：
> ① 用 `ch_module` / `ch_layer<T>` ⇒ 它调 `build_hierarchical_name(parent, local)`
>    生成**全路径**名字再交给 ctor（`include/module.h:56`）⇒ ✅ 名字带前缀；
> ② 手工 `make_unique<Unit>(nullptr, "regfile")` ⇒ 名字**就是** `"regfile"`
>    ⇒ ❌ 两个同名实例**同名**（⇒ 节点名靠全局 `_N` 去重 ⇒ 但那是**改名**，
>    破坏「顶层端口名与扁平逐字一致」与下游按名解析）。
> ⇒ ⇒ ⇒ **裁决**：无 active parent 的装配点 MUST **自己算全路径名**
> （`parent->name() + "." + local`，或复用 `build_hierarchical_name`）后传给 ctor；
> **MUST NOT** 依赖「`add_child` 会补前缀」。
> ⇒ ⇒ ⚠️ **`add_child` 仍 MUST 先于 `build`**（⇒ `parent_` 必须在建节点前就位，
> 否则节点名取不到父）；**且** `add_child` 返回 `shared_ptr`（`include/component.h:76`）
> ⇒ ⇒ 下游 MUST 用返回的 `shared_ptr` 续存（**MUST NOT** 继续持有原 `unique_ptr` ⇒ 双重管理）。

⇒ ⇒ ⚠️ **这是对下游现有代码的一处真实改动**（`regfile_bridge_chmem.h:124-125` 的形态）。
⇒ ⇒ 替代路径：用 **ADR-061 的 `ch_layer<T>`** 装配 ⇒ ⇒ 它同时解决三件事：
① 自动登记进组件树；② 自动成层；③ 自动共用 env ctx（满足 G-1p）。
⇒ ⇒ **但** `ch_layer` 的可用性仍以 **U-4（`module.h:13` 的 ODR）** 为前置。

### 2.6 接口**里能放什么**（白名单与三条遗漏裁决）

| 项 | 裁决 | 理由 |
|---|---|---|
| `ch_uint<N>` / `ch_bool` | ✅ **允许** | 有端口形态、有宽度元数据 |
| **`ch_mem`** | ❌ **禁止** | 数组无端口形态；且跨模块 mem 已被 ADR-060 §2.1 第四行**响亮拒绝** |
| **`ch_reg`** | ❌ **禁止** | 寄存器是**状态**不是接口；跨模块写别人的寄存器属 ADR-060 §2.1 的隐式端口范畴 |
| **bundle / `ch_stream`** | ❌ **禁止** | 成员不是端口（见 §3 选项 3/4） |
| **clock / reset** | ❌ **禁止声明** | 框架按「clock / reset **分开判**」供给（ADR-058 §5）；声明会造成**重复端口** |
| **`inout`** | ❌ **不支持** | 框架单方向端口模型（`include/core/io.h`）；MUST 明确拒绝而非静默 |
| 数组字段（`ch_out<T> x[N]`） | ✅ 允许，但有**命名规则** | 端口名 MUST 是确定性派生（`x_<i>`）⇒ 否则「同一设计两次生成字节相同」不成立 |

⚠️⚠️ **观测端口（`ObsPorts` 型）—— v1.3 订正：原裁决是错的，已撤回**（critic 第 5 轮）

**原裁决（v1.1）**：「观测端口归创建它的 module ⇒ 单元 MUST **不**因为有人读它的内部节点
而长出端口 ⇒ 端口合成在**父**侧」。
**为什么错**：Verilog 里**父层根本无法直接读子模块的内部信号**（可综合子集不接受层次引用）
⇒ ⇒ 要让父层读到子内部的 `ch_reg`，**唯一**路径就是**子 module 的 output 端口**
⇒ ⇒ ADR-060 §2.1 第 2 行（子→父 ⇒ 合成**子侧** output 隐式端口）是**必要的**，
«合成在父侧»在物理上不可能。

⇒ ⇒ **正确裁决（三段）**：

| # | 情形 | 裁决 |
|---|---|---|
| ① | 父层读**子内部节点**（非子端口） | **按 ADR-060 §2.1 行 2**：在**子侧**合成 **output 隐式端口** + `CHWARN`（⇒ 这是 Verilog 的唯一路径，MUST NOT 拒绝） |
| ② | 某个内部节点是**稳定的交付观测面**（⇒ 下游那 32 个 `reg_*`、`memsig`、`tohost`） | **MUST 由单元自己声明成 `ch_out`**（⇒ 进入接口 ⇒ 端口签名稳定 ⇒ 可单独交付/综合） |
| ③ | 把「父层临时建一个 `ch_out` 去读子内部」当作**交付接口** | **MUST NOT** —— 它只是**调试胶合**，会给子 module 长隐式端口，且端口集合随使用方变化 |

⇒ ⇒ **为什么原裁决的动机已经消失**：它当时的理由是「与 ADR-060 §2.3 的 module 复用冲突」
⇒ ⇒ 而 **module 复用已被改判为 MUST NOT**（§2.6 下半）⇒ ⇒ ⇒ **冲突不再存在**
⇒ ⇒ 故「① 合成子侧 output」是可接受的代价（⇒ 换来了**可综合性**）。

⚠️ **代价（如实登记）**：若下游不改 ②，则它的单元 module 会多出 ~32 个隐式 output
⇒ ⇒ 接口膨胀；⇒ ⇒ **正确做法是 ②**（把观测面写成单元的 `ch_out`）。

⚠️ **同类型多实例 vs module 复用（未闭合，登记为残留）**：接口必须逐字相同，但
**端口名含实例路径** ⇒ ⇒ 端口签名（名字序列）**必然不同** ⇒ ⇒ ADR-060 §2.3 的复用条款
在当前命名规则下**不可达**。⇒ ⇒ 二者 MUST 择一：
① **端口名用相对名** ⇒ 复用可达，但与「命名权威 = ctx 级全局表」（ADR-061 决策④）冲突；
② **放弃复用**（每实例一份 module）⇒ 与全局命名自洽。
⇒ ⇒ **本 ADR 取 ②**（⇒ 复用是**优化**，命名一致性是**契约**）⇒ ⇒ ⇒ **契约表述改为正向**：
**同类型多实例 MUST NOT 复用同一 module 定义**（每实例一份），
**module 名 MUST 是 `<module_type_name>_<按实例路径排序的序号>`**（⇒ 确定性、无重名）。

⚠️ **这不是「加注」能解决的**（critic B5）：ADR-060 §5 有一条 MUST「同类型 + 同端口 + 同初值
⇒ **MUST 复用**」⇒ ⇒ 与本节**互斥** ⇒ ⇒ 那一行 MUST **随本 ADR 改判**（已同步 ADR-060 v1.5）。

---

## 3. 选项对比（**逐条含理由**）

| # | 接口载体 | 结论 | 理由 |
|---|---|---|---|
| 1 | **纯 `Payload<T>`**（ADR-091 §D3a.7 原口径） | ❌ **否决**（作单元接口） | ① **不构成接口声明**（⇒ 端口集合由实现细节偶然决定，R1）；② 读取绑 `PayloadStore`（R2）；③ 时序 fail-open（R3）；④ 无结构元数据（R4）。**⇒ 但它仍是层 1 内部与「层 2→层 1 输出」的正确载体** |
| 2 | **结构化端口**（`__io` 的 `ch_in`/`ch_out`） | ✅ **采纳** | ① 端口节点在 ctx 里 ⇒ emitter 直接可见；② 天然满足「可单独交付/综合」（N1）；③ 与 G-1q 一致；④ 与既有扁平 codegen 同源（算子/宽度/`always_ff` 全部复用） |
| 3 | **`ch_stream<T>`** | ❌ 否决 | ① 它是 **bundle** ⇒ **成员不是端口**（`include/bundle/stream_bundle.h:19-24` 只有 `payload`/`valid`/`ready` 三个成员）⇒ 与选项 4 同一条否决理由；② ⚠️ **不能引用 ADR-092 §D4 当理由**（oracle R8）：那条的分界是「核内 stage 间（Payload）vs IP 间/核↔从设备（ch_stream）」，**没有「核内部件」这一档** ⇒ 「IP 单元用 ch_stream」在该条下**并不被禁止** ⇒ 本条是**新增决策**；③ ⚠️ 「`stream_fifo` 引入延迟」**不成立**：`ch_stream` 本体**无 FIFO、零延迟**，延迟只在用 `stream_fifo` / pipe 时出现 |
| 4 | **bundle 作接口**（`CH_BUNDLE_FIELDS`） | ❌ 否决 | ① 跨层走**隐式端口 + `CHWARN`**（ADR-060 §2.1）；② bundle 作 IO **不会自动成端口**（根 `AGENTS.md` ANTI-PATTERNS）⇒ 把显式契约换成隐式⇒ 更贵不更好 |
| 5 | **单一声明派生**（决策④：`CH_INTERFACE`） | ⭐ **建议采纳**（下游可选） | ① 消除「同一字段写四遍」（事实⑥）⇒ 字段增删改只在**一处**；② 派生物**结构上不可能漂移**；③ 代价 = 一个新的宏/代码生成点（⇒ 必须自带「派生正确性」判据） |
| 6 | **「端口 + payload 双写」但手工保持同步** | ❌ 否决 | 事实⑥ 就是它的运行结果：**四处手写**必然漂移（本仓与下游都反复吃过这个亏） |

---

## 4. 后果 / 代价

### 正面
- **N1 才真正可达**：单元有端口 ⇒ 才能成为可单独交付/综合的 module；
- 下游的「层 2 输入」规则（G-1q）从**时序性约束**升级为**结构性约束**（更难被误改回去）；
- 端口规则**按粒度分叉**之后，`inline` 与 `成层` 各自的判据**互不干扰**
  （inline 保「逐字节一致」，成层保「端口不丢」）；
- 若采纳决策④ ⇒ 事实⑥ 的「四处手写」降到一处。

### 负面
- **成层组件的 `ch_in` 即使有 driver 也 MUST 是端口** ⇒ emitter 必须**忽略** `driver()`，
  与扁平路径**行为不同**（⇒ 这正是「两套端口规则」的成本，且无法靠参数抹平）；
- 下游现有装配形态（`make_unique` + `build`）**MUST 改**（补 `add_child` 或改 `ch_layer`）；
- 决策④若落地 ⇒ 新宏 + 新代码生成点 ⇒ 需要**自己的**护栏（否则「派生出错」比「手写漂移」更难查）。

### 风险

| # | 风险 | 对策 |
|---|---|---|
| 1 | 实施者把「扁平的有 driver `ch_in` ⇒ wire」照搬到成层组件 ⇒ **输入端口全丢**、跨 module 赋值 | ADR-060 §5 增判据：**成层组件的每个 `ch_in` MUST 出现在其 module 端口表**；本 ADR 决策① 的两粒度表逐行有判据 |
| 2 | 下游继续用 `Payload` 当接口 ⇒ **接口由实现细节偶然决定** | 判据 = 「**单元体内 MUST NOT 引用 `owner` 既不是自己、也不是自己祖先的端口节点**」（⇒ 这正是「值经 host 通道进来」的形态；⇒ 已写成 capability 的 Requirement，见 spec）。⚠️ **MUST NOT 用「端口数 > 0」** —— `Payload` 的 cell 可以装 `ch_out` 节点 ⇒ 端口数可能 > 0 ⇒ 该判据**没有判别力**（§5） |
| 3 | 决策④ 的宏过于聪明 ⇒ 生成结果不可预测 | 若采纳 MUST 先出「派生结果可读化」的判据（`static_assert` 出字段表）⇒ 否则**不要采纳** |
| 4 | 改下游装配形态引发回归 | 两条路径并存（`add_child` / `ch_layer`）⇒ MUST 各有用例；`ch_layer` 以 U-4 为前置 |

---

## 5. Verification（**前瞻**验收判据）

- [ ] **端口完整性（`⊇` + 五类来源）**：对任一**成层且非根**的 module，
      其端口集合 MUST **⊇** 它自己的 `ch_in` ∪ `ch_out`（**driven 与否不影响**）
      ⇒ ⇒ **且每个端口 MUST 可追溯到五类来源之一**：
      ① 自身 `ch_in` / `ch_out`；② **inline 后代**的 `ch_out`（⇒ **仅根 module 允许**）；
      ③ ADR-060 §2.1 的**隐式端口**；④ ADR-060 §2.2 的**冒泡端口**；
      ⑤ **时序基础设施** `default_clock` / `default_reset`（⇒ 它们**不是** `ch_in`/`ch_out`，
      由 `has_seq` 供给；⇒ 漏掉这一类 ⇒ **任何含寄存器的设计**都过不了本条，critic B2）。
      ⚠️ **MUST NOT 写成「等于」** —— ③④ 是**必然存在**的额外端口，
      写成「等于」⇒ 实现为让判据绿只能砍掉 ADR-060 的三条裁决（oracle R1）
- [ ] **RTL 顶层 module 的端口表 MUST 等于扁平产物的端口表**（逐字逐序）
      ⇒ ⇒ 含 clock → reset → 无 driver 的 input → **全部** `ch_out`（含 inline 后代的）
      ⇒ ⇒ 这条**取代**「⊇ 自身 ch_in ∪ ch_out」在根上的适用（⇒ 二者对根互斥，critic B1）
- [ ] **非根 module MUST NOT 把 inline 后代的 `ch_out` 上端口表**（⇒ 它们是体内普通信号，
      ADR-061 决策② 行 2）⇒ ⇒ 判据：任一行 2 形态的 module 端口表里**不含**这些信号
- [ ] **子 module 内 MUST NOT 出现 `assign <ch_in> = ...`**（⇒ 对 `input` 赋值非法）
      ⇒ ⇒ 判据：`verilator --lint-only -Wall` 零 error（现有 `print_logic` 会发它，oracle R2）
- [ ] **父 module MUST 为子 `ch_out` 声明连接网** ⇒ ⇒ 判据：**接 output 的那个**
      `.port(...)` 实参 MUST 是该 module 内**已声明**的 `logic`
      （⚠️ **接 input 的实参可以是表达式** ⇒ MUST NOT 强制造网，critic SF）
- [ ] **观测/跨层读取（情形①②③）**：父层读**子内部节点** ⇒ 子 module MUST 有对应的
      **output** 端口（⇒ 隐式端口 + `CHWARN`）；而**稳定的交付观测面** MUST 由单元自己
      声明成 `ch_out`（⇒ 判据：交付用的观测信号 MUST 出现在单元的 `ch_out` 集合里，
      MUST NOT 只存在于「父层临时建的 `ch_out`」里）
- [ ] **自驱动 `ch_in` MUST 被拒绝**：成层组件自己驱动自己的 `ch_in` ⇒
      emitter MUST `CHFATAL_EXCEPTION`（⇒ 两条既有裁决都无定义，且对 `input` 赋值非法）
- [ ] **inline 不受本条影响**：`Granularity::ForceInline` 产物 MUST 仍与扁平**逐字节一致**
      （`cmp` 退出码 0）⇒ ⇒ 这是「按粒度分叉」没有误伤默认路径的判据
- [ ] **driven `ch_in` 的父层连接**：成层组件的 `ch_in` 由父层表达式驱动时
      ⇒ 父层的驱动 MUST 成为**实例端口连接**（MUST NOT 变成跨 module 的 `assign`）
      ⇒ ⇒ 判据：产物里**零**跨 module 边界的 `assign`（ADR-058 §5 已有一条，此处是它的**新形态**）
- [ ] **`Payload` 不可作接口（判别性对照，oracle R5 订正）**：把某单元的**输入**改造成
      「只从 `Payload` 读、不声明 `ch_in`」⇒ ⇒ 判据 (b) **MUST 转红**：
      「单元体内 MUST NOT 引用 `owner` 既不是自己、也不是自己祖先的端口节点」
      （⇒ 这正是「值经 host 通道进来」的形态）
      ⚠️ **MUST NOT 用「端口数变 0」当判据** —— 输出方向必须**先有** `ch_out` 作源
      ⇒ 端口数不会变 0 ⇒ 该判据**没有判别力**
- [ ] **下游装配形态**：`make_unique` + `build(ctx)` **不** `add_child` ⇒ MUST 被拒绝；
      补 `add_child`（或改用 `ch_layer`）⇒ MUST 正常产出且该单元**按其声明成层**
- [ ] **决策④（若采纳）**：`CH_INTERFACE` 的派生结果 MUST 与手写声明**逐字段相等**
      ⇒ ⇒ 判据 = 派生出的字段表 `static_assert` 打印 + 与手写版做编译期比较

---

## 6. 变更记录

| 日期 | 变更 |
|---|---|
| 2026-11-02 | **创建**（调研产出：下游 ADR-091 §D3a.7 + G-1q + `payload.h` + 桥接 plugin 实测。裁决：① 层次化单元接口 = **结构化端口**，且**成层组件的全部 `ch_in`/`ch_out` 都成端口**（不论是否被驱动）② `Payload<T>` **不可**作接口（4 条理由，其中 R1「codegen 不可见」不依赖时序）③ 层 2 输入 MUST 走组合端口（采纳 G-1q 并给出**结构性**理由）④ 建议「单一声明派生」消除四处手写 ⑤ 下游装配 MUST 补 `add_child` 或改用 `ch_layer`） |
| 2026-11-02 | **v1.1**（据 oracle 第 2 轮 11 条）：① **端口判据从「等于」改「⊇ + 四类来源可追溯」**（⇒ 「四类」已在 v1.2 扩为**五类**） —— 「等于」被 ADR-060 的隐式端口 / 冒泡端口 / 根 module 的 inline 后代 output **三条既有裁决直接证伪**；② **新增「三处发射点 × module 角色」表** —— 子 module 的 `ch_in` MUST **不**声明 wire、**不**发 `assign`（现有 `print_decl` / `print_logic` 都按 `driver()` 判 ⇒ 会双声明 + 非法赋值）；③ **根 module 单列**（= 扁平规则本身）；④ **父侧落点**：子 `ch_out` 在父 module MUST 有 `logic` 连接网；⑤ **R1 重写** —— `Payload` 的 cell **可以**装 `ch_out` 节点 ⇒ 不能说「用 Payload 就 0 端口」；正确理由 = **不构成接口声明**；⑥ **决策④ 拆归属**（派生③ 归下游，否则分层倒置）+ 端口名实例前缀 + per-instance key；⑦ **决策⑤ 补顺序**（`add_child` MUST 先于 `build`，否则 `name_` 永久缺父路径）；⑧ **`ch_stream` 否决理由改正**（bundle ⇒ 成员不是端口；ADR-092 §D4 无「核内部件」一档）；⑨ **新增 §2.6 接口白名单**（禁 `ch_mem`/`ch_reg`/bundle/clock-reset/inout）+ **观测端口归创建它的 module** + **放弃 module 复用**（端口名含实例路径 ⇒ 签名恒不同） ⚠️ **其中「观测端口归创建它的 module」已于 v1.3 撤回**（见 §2.6） |
| 2026-11-02 | **v1.2**（据 critic 第 3/4 轮）：① **`⊇` 限定为「成层且非根」**（⇒ 对根套用会与「全 inline ⇒ `cmp` 0」**互斥**），根 module 单列走扁平规则；② **五类来源**（补**时序基础设施** clock/reset）；③ **自驱动 `ch_in` 的拒绝限定为非根**（根上被驱动的 `ch_in` 就是体内 wire）；④ **决策⑤ 机制订正** —— `add_child` **不补名字前缀**（`name_` 由 ctor 实参逐字决定）⇒ 无 parent 装配点 MUST **自己算全路径名**；⑤ **决策④ 拆规范段/可选段**（并明确 **MUST NOT** 要求端口名带字段名）；⑥ **复用改判为 MUST NOT**；⑦ `ch_stream` 否决理由改为「bundle ⇒ 成员不是端口」 |
| 2026-11-02 | **v1.3**（据 critic 第 5 轮，**撤回一处错裁**）：原「观测端口归创建它的 module ⇒ 端口合成在**父**侧」**物理上不可能**（Verilog 父层无法直读子内部信号）⇒ ⇒ 改为三段裁决：① 父读子内部 ⇒ **按 ADR-060 §2.1 行 2 合成子侧 output 隐式端口**；② 稳定交付观测面 MUST 由单元自己声明成 `ch_out`；③ MUST NOT 把「父层临时 `ch_out`」当交付接口。⇒ 原裁决的动机（与 module 复用冲突）**已随「复用改判 MUST NOT」消失**。并在 §5 补对应可勾选判据 |
