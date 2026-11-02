# 层次化 RTL 输出 —— 目标架构（需求 · 模块关系 · 结构事实）

> 本文件 = **架构主题文档**：只承载**当前态快照**·**下游需求**·**目标形态的结构事实**
> （模块关系、数据结构、命名规则、已知残留）。
> **决策**在 ADR-058（边界与等价性）· ADR-059（归属与命名）· ADR-060（端口策略与复用）·
> **ADR-061（层级粒度由用户显式控制）**。
> **算法伪码 / 发射流程 / 边界情形矩阵 / 验证策略** ⇒ ⇒ 归**未来 change 的
> `design.md` / `tasks.md`**（见 §7 的落点表）⇒ ⇒ ⇒ **本文件不重复承载**。

- Date: 2026-11-02
- Status: **Draft**（随 ADR-058 / 059 / 060 / 061 拍板后转正）
- 关联决策: `docs/adr/ADR-058-hierarchical-rtl-output.md` ·
  `docs/adr/ADR-059-node-ownership-and-component-naming.md` ·
  `docs/adr/ADR-060-hierarchical-port-policy-and-module-reuse.md` ·
  `docs/adr/ADR-061-hierarchical-granularity-user-control.md`
- 关联台账: `.rddf/roadmap/features/feat-hierarchical-rtl-output.md`（实施路径）·
  `docs/developer_guide/patterns/HIERARCHICAL-RTL-AUTHORING-GUIDE.md`（用户写法）

---

## 1. 当前态快照（2026-11-02 实测）

### 1.1 扁平输出的形态

`ch::toVerilog(filename, context*)`（`include/codegen_verilog.h:36`）**只接收 context**，
固定输出一个 `module top (`（`src/codegen_verilog.cpp:301`）到 `endmodule`（`:409`）。
writer 内**没有任何** `children()` / `child_count()` 遍历，也不发射实例化语句。

⚠️ **Verilator backend 不走 `toVerilog()`**：它直接构造 `verilogwriter(ctx)` 并写死
单个 `top.v` + `module top`（`src/core/verilator_backend.cpp:417, 416-489`）
⇒ ⇒ **框架里没有任何入口能仿真「外部生成的层次化 Verilog」**
⇒ ⇒ 这是 ADR-058 §2.3「backend 只增不改例外」的由来。

### 1.2 归属与命名的**四个**缺陷（**层次化的前置阻塞**）

实测（附录 A 的探针）：

```
top.m.unnamed_input   <- parent=top.m      ✅ 端口归属正确
top.m.r               <- parent=top.m      ❌ 但 r 是 Leaf 的 ch_reg 成员
top.top.m.lf.*                             ❌ 深度 ≥3 前缀重复
```

| # | 缺陷 | 根因 | 位置 |
|---|---|---|---|
| ① | **成员节点归到父组件** | 建节点时用 `Component::current()`，而**成员构造早于 `build()`** | `src/core/lnodeimpl.cpp:25-31` |
| ② | **成员 `ch_module` 挂祖父** | 同上 ⇒ 组件树失真 | `include/module.h:42` |
| ③ | **深度 ≥3 前缀重复** | `build_hierarchical_name()` 用 `hierarchical_name()`，后者又拼一次已是全路径的 `name_` | `include/module.h:21` + `include/component.h:61-73` |
| ④ | **成员 `ch_module` 因 `context()==nullptr` 早退** | `Component::ctx_` 在 ctor 期为 `nullptr`，只在 `build()` 赋值 ⇒ 今日靠「挂祖父」恰好掩盖 | `src/component.cpp:13, 105-118` + `include/module.h:49-53` |

⇒ ⇒ **修法与取舍见 ADR-059**；本文件 §3 之后的一切都**假设** ADR-059 已落地。

### 1.3 已排除的岔路

⇒ ⇒ 三个岔路（后处理切分 / 跨 `ch_device` 连线 / 逐层手写例化）的**否决理由**
⇒ ⇒ ⇒ 属**决策内容** ⇒ ⇒ ⇒ 见 **ADR-058 §3 选项 3 / 4 / 5** ⇒ ⇒ ⇒ 本文件不重复。

---

## 2. 下游需求清单（N1–N7）

> **来源**：① 下游 **ChipForge** 的开放核工作（其 ADR-091 v1.15）；
> ② 下游向上游提的 openspec change `component-framework-hardening-2026-10`
> （**位于本仓** `openspec/changes/`，由 ChipForge 提出）。

| # | 需求 | 证据 | 对本方案的影响 |
|---|---|---|---|
| **N1** | ★ **层 2 IP 单元（`ch::Component`）MUST 能成为独立 Verilog module**（可单独复用 / 交付 / 综合） | ChipForge ADR-091 §D3a.8「层次化 = 采纳」· §D3a.7 两层范式 | ⇒ 这是本能力的**目的** |
| **N2** | ★ **装配点可能没有 active parent**（桥接 plugin 在 `setup()` 内 `make_unique` + `build(ctx)`）⇒ ⇒ `ch_module` 不可用 ⇒ ⇒ **「成层声明」与「装配点」MUST 解耦** | ChipForge ADR-091 v1.15 §D3a.7 三层真实理由 ① | ⇒ ⇒ **ADR-061 决策 ⑥**（成层由声明决定 · 装配点任意）⇒ ⇒ ⇒ **ADR-059 的组件树正确性从「层次化前置」升级为「下游 Plugin 框架前置」** |
| **N3** | ★ **Component MUST 共用 ctx**（子 ctx ⇒ node-id aliasing） | ChipForge ADR-091 §G-1p | ⇒ ⇒ 层次化分析的前提成立（单 ctx ⇒ 单节点表） |
| **N4** | ★ **顶层端口名 MUST NOT 与内部 `ch_reg` / `ch_node` 同名**（否则唯一化会改**端口**名 ⇒ 两后端端口名不一致） | ChipForge ADR-091 §G-1o | ⇒ ⇒ 层次化 emitter MUST NOT 引入新的改名 ⇒ ⇒ 命名沿用全局表（§4.2）⇒ ⇒ **天然满足** |
| **N5** | ★ **节点名唯一性 opt-in 检测**（解释器侧不改名 / Verilator 侧改名 ⇒ 按名解析会**静默采到错节点**） | `openspec/changes/component-framework-hardening-2026-10` §Why.2 + U-1 | ⇒ ⇒ 层次化**不解决**它（不改名语义）⇒ ⇒ ⇒ 但层次化的全局命名表让「改名只发生在 codegen 侧」这一既有事实**更显眼** ⇒ ⇒ ⇒ 与 U-1 互补 |
| **N6** | ★ **公共头 ODR 安全**（`module.h:13` 的自由函数缺 `inline` ⇒ 2+ TU include 即链接失败） | 同上 §Why.1 + U-4；ChipForge ADR-091 §G-1b | ⇒ ⇒ **层次化 emitter 是库级代码** ⇒ ⇒ ⇒ 多 TU 项目必踩 ⇒ ⇒ ⇒ **U-4 是层次化的前置**（已在同仓 change 内立项） |
| **N8** | ★ **层 2 单元的接口类型 MUST 是「结构化端口」** —— 下游的 `Payload<T>` 是 host 侧 key-value（cell 是 `std::any`、store 由 `PipeNode` 持有）⇒ ⇒ **不产生端口、也不构成接口声明** ⇒ 若拿它当单元接口 ⇒ 单元的端口集合**由实现细节偶然决定** ⇒ 「可单独交付/综合」直接失败 | `include/cf/plugin/payload.h:48-180` · ChipForge ADR-091 **G-1q** · ADR-062 | ⇒ ⇒ **ADR-062 决策①②**（`Payload` 不可作接口；成层组件的全部端口都成端口） |
| **N7** | ★ **按 ctx 级节点名解析观测点**（`resolve_node_id(ctx, name)` 精确匹配 `node->name()`） | `tools/parity_runner/parity_sim_runner.cpp:249-255` | ⇒ ⇒ **顶层端口名 MUST 与 ctx 节点名一致** ⇒ ⇒ ⇒ **命名权威 MUST 是 ctx 级全局表**（§4.2）⇒ ⇒ ⇒ **per-module 局部命名被否决** |

⇒ ⇒ **需求 → 决策的映射**：

| 需求 | 落点 |
|---|---|
| N1 | ADR-058 §2.1（模块边界 = 所有权边界）+ ADR-061（成层声明） |
| N2 | ADR-061 决策 ⑥ + ADR-059（组件树正确性） |
| N3 | ADR-059 §2.3（单 context） |
| N4 / N7 | ADR-061 决策 ④（命名沿用 ctx 级全局表） |
| N8 | **ADR-062**（接口类型 = 结构化端口；`Payload<T>` 不可作接口；成层组件端口规则按粒度分叉） |
| N5 | 本仓既有 change `component-framework-hardening-2026-10`（U-1）—— **不在本方案范围内** |
| N6 | 本仓既有 change `component-framework-hardening-2026-10`（U-4）—— **前置依赖** |

---

### 2.1 下游接口类型的调研结论（2026-11-02）

**问题**：层 2 单元（`ch::Component`）的接口，能不能用 `Payload<T>`？有没有更优雅的做法？

| # | 候选载体 | 类型安全 | **对 codegen 可见（能成为端口）** | 时序 | 结论 |
|---|---|---|---|---|---|
| 1 | `Payload<T>` | ✅ 编译期 key 类型检查 | ❌ **不是 port 节点**（key/store 都在 host 侧，cell 是 `std::any`） | ❌ 写 `elaborate()` / 读 `build()` | **❌ 不可作接口**（可作层 1 内部 + 层 2 输出发布） |
| 2 | `ch_stream<T>` | ✅ | ❌ **成员不是端口**（`stream_bundle.h:19-24` 三个成员） | ✅ 本体**无 FIFO、零延迟** | ❌ **bundle ⇒ 成员不是端口**（⚠️ **不能**引 ADR-092 §D4 当理由 —— 它的分界是「核内 stage 间 vs IP 间」，**没有「核内部件」这一档**；「带 FIFO 有延迟」也不成立，ADR-062 v1.1 已订正） |
| 3 | bundle（`CH_BUNDLE_FIELDS`） | ✅ | ⚠️ 跨层 ⇒ **隐式端口 + `CHWARN`**；作 IO **不自动成端口** | ✅ | ❌ 把显式契约换成隐式 |
| 4 | **显式端口**（`__io` 的 `ch_in`/`ch_out`） | ✅（按 struct 字段） | ✅ **就是端口** | ✅ 组合直连 | ✅ **采纳**（ADR-062 决策①） |
| 5 | **单一声明派生**（一份字段表 ⇒ 端口 + 桥接 + payload key） | ✅ | ✅（派生出的仍是端口） | ✅ | ⭐ **建议**（消除「同一字段写四遍」） |

⚠️ **决定性的一条不是时序、是「契约缺失」**：`Payload<T>` 既不产生 port 节点、也不携带
方向/宽度/端口名这些结构元数据 ⇒ ⇒ **它不构成接口声明** ⇒ ⇒ 一个「只用 `Payload` 读写」
的单元，其端口集合**由实现细节偶然决定** ⇒ **不可交付/综合/复用**（N1 失败）。
⚠️ **不要说成「0 端口 module」**：CH_MEM 模式下 cell 可以装 `ch_out` 节点
（下游 `regfile_bridge_chmem.h:203-210` 实测）⇒ 端口数可能 > 0 ⇒ ⇒ 该说法**没有判别力**。
⇒ ⇒ 这条**独立于**下游 G-1q 的时序理由（⇒ 即使时序问题被修掉，这条仍然成立）。

⚠️ **下游的「同一字段写四遍」实测**（`ip/cpu/core/regfile_bridge_chmem.h`）：
单元的 `__io(...)` · 桥接的 `Ports`（`ch_logic_in`）· `ObsPorts`（`ch_out`）· payload key 与其
`put/get` ⇒ ⇒ 这正是决策④「单一声明派生」要消掉的成本。

---

## 3. 目标形态的结构事实

### 3.1 语义定义（**权威在 ADR-058 §2.1 / ADR-061 §2**）

> **模块边界 = 所有权边界**；`owner(n)` = 节点归属的组件实例。
> **层级粒度由用户显式声明，默认 inline** ⇒ ⇒ ⇒
> **RTL module 树 ≡ 「被声明为成层的」组件子树**（被 inline 的组件不出现在 RTL 树里）。

⚠️ **为什么不用「图割」**：图割对**死逻辑**与**归属错误**都判不准 ⇒ 需两套规则并存
⇒ 不可预测 ⇒ ⇒ 归属修好后所有权规则**更简单且全覆盖**（论证见 ADR-058 §2.1）。

⚠️ **inline 的端口规则 = 扁平规则本身**（ADR-061 决策②）：
无 driver 的 `ch_in` ⇒ 顶层 `input`；**任意** `ch_out` ⇒ 顶层 `output`（含已被父层消费的）。
⇒ ⇒ ⇒ **端口表收缩（无人消费的子输出不上顶层）只属于成层子树** ⇒
这使「全 inline ⇒ 与扁平逐字节一致」成为**推论**而非额外承诺。

### 3.2 模块关系（目标）

```
                    ┌────────────────────────────────────┐
                    │  ch::Component（组件实例）          │
                    │  ├─ children()      实例集合          │
                    │  ├─ local_name()    实例局部名（新增）  │
                    │  ├─ module_type_name() 模块类型名（新增）│
                    │  └─ is_layer()      成层声明（新增，ADR-061）│
                    └──────────────┬─────────────────────┘
                                   │ 拥有（owner）
                    ┌──────────────▼─────────────────────┐
                    │  lnodeimpl（含 owner 指针）          │
                    │  ├─ type_input / type_output  → 端口 │
                    │  ├─ type_reg / type_mem        → 体内 │
                    │  └─ type_op / type_proxy / …         │
                    └──────────────┬─────────────────────┘
                                   │ 被分析
                    ┌──────────────▼─────────────────────┐
                    │  HierarchyAnalysis（新增）           │
                    │  ├─ ModuleDef[]（只含成层组件）       │
                    │  ├─ InstanceDef[]                   │
                    │  └─ 隐式端口 / 冒泡端口识别           │
                    └──────────────┬─────────────────────┘
                                   │ 发射
                    ┌──────────────▼─────────────────────┐
                    │  ModuleEmitter（从 verilogwriter 拆出）│
                    │  → 逐 module 的 RTL                  │
                    └────────────────────────────────────┘

  ch::core::context ──► 节点全集与拓扑序（get_eval_list）
  src/codegen_verilog.cpp ──► print_* 系列（算子/宽度/always_ff/mem）⇒ 复用
  src/core/verilator_backend.cpp ──► 既有 initialize(ctx,…) **不动**
                                  （新增一个只增不改入口，见 ADR-058 §2.3）
  src/simulator*.cpp ──► **不动**（仿真语义与 RTL 形态无关）
```

### 3.3 目标数据结构

```cpp
struct ModulePort {
    std::string name;      // **ctx 级全局名**（sanitize 后）⇒ 顶层端口 MUST 与扁平逐字相同
    unsigned    width;
    bool        is_input;
    bool        is_clock;  // ⚠️ 新增维度（2026-11-02，critic SF）：clock/reset 不是普通端口
    bool        is_reset;
    lnodeimpl*  node;      // 合成端口（`__if_` / `__x_`）时为 nullptr
};

struct InstanceDef {
    Component*  comp;
    std::string inst_name;    // comp->local_name()
    std::string module_name;  // comp->module_type_name()
    std::vector<std::pair<std::string,std::string>> port_map; // (.port, 父侧信号名)
};

struct ModuleDef {
    std::string              name;
    std::vector<ModulePort>  ports;     // ⚠️ 顺序 MUST 复刻扁平 print_header：
                                        //   clock → reset → 无 driver 的 input → output
                                        //   （src/codegen_verilog.cpp:333-362）⇒ 否则
                                        //   「全 inline 逐字节一致」当场失败
    std::vector<lnodeimpl*>  body;      // 按 ctx->get_eval_list() 顺序过滤（拓扑序）
    std::vector<InstanceDef> children;  // 按 children() 的构造顺序
    bool                     needs_clock; // ⚠️ clock / reset **分开判**（一元 has_seq 不够）
    bool                     needs_reset;
};
```

⚠️ **clock / reset MUST 分开判**（oracle R3/R4 + critic R4 订正）：
**clock** ⇒ 含 `ch_reg` **或** `ch_mem` **写端口**（`src/codegen_verilog.cpp:1310`）**或** `sread`；
**reset** ⇒ 含 `ch_reg` **或** `sread` —— ⚠️ mem 写端口的 `always_ff` **没有 reset 分支**
⇒ 纯 RAM 写模块 MUST **只发 clock**（否则得到一根永不使用的 reset 端口）。
`aread`（0-cycle）两者都不需要。

⚠️ **根 module 例外**：**无条件**发射 `default_clock` / `default_reset`
（扁平 `print_header` 就是无条件的，`src/codegen_verilog.cpp:333-343`；
`context::init()` 恒定创建二者 ⇒ **纯组合设计的扁平产物也有这两个端口**）
⇒ ⇒ 只有**非根** module 才按 `needs_clock` / `needs_reset` **分别**决定 ⇒ ⇒ 否则 ForceInline 产物少两个端口。

---

## 4. 归属 ↔ 语义的耦合（**承重 · 实施期必查**）

⚠️ **归属不是纯命名属性** —— 它被用于**语义分流**：

| 消费点 | 用法 | 影响 |
|---|---|---|
| `src/simulator_init.cpp:95` | `!node->get_parent()` ⇒ 分到 `input_instr_list_` / `combinational_instr_list_` | **归属真源变更 ⇒ 指令表分流可能变** ⇒ ⇒ 仿真行为**不得**假定逐位不变 ⇒ 须逐项回归（ADR-059 D7） |
| `include/core/io.h:360/385/410/432` | 4 处 `CHREQUIRE` 用 `get_parent() == current_component()` 判连线方向 | 归属真源变更 ⇒ **守卫触发面可能变** ⇒ ⇒ `CHREQUIRE` **只打日志** ⇒ ⇒ 漂移**静默** ⇒ 须逐条回归 |
| `src/core/lnodeimpl.cpp:26-31` | `parent_->name()` 拼 `name_` ⇒ **所有信号名的来源** | 归属真源变更 ⇒ **扁平产物文本会变**（ADR-059 D5 的对象就是它）⇒ ⇒ 这是「文本变化面」需要逐条登记的**根因** |
| `src/codegen_verilog.cpp:333-370` / `:432-499` | 按 `driver()` / 类型筛端口与声明：`type_output` **恒为端口**、`print_decl` **无 wire 分支** | ⚠️ 层次化后同一个 `type_output` 有三种角色（自身端口 / 隐式输出端口 / **体内 wire**）⇒ ⇒ `print_decl` MUST 新增 wire 分支（ADR-061 决策②） |

⇒ ⇒ ⇒ **ADR-059 的「仿真逐位不变」是待验证的验收项，不是可假定的结论。**

---

## 5. 命名规则

⚠️ **命名权威 = ctx 级全局表**（`verilogwriter::node_names_`，按 eval list 顺序全局 `_N` 去重，
`src/codegen_verilog.cpp:59-88`）⇒ ⇒ ⇒ **层次化 MUST NOT 发明新命名**。

| 对象 | 规则 | 例 |
|---|---|---|
| module 名 | `comp->module_type_name()`（默认 sanitize(`typeid(*this).name()`)，可由 `CH_MODULE_NAME("...")` 覆盖） | `rv32i_pipeline` |
| 实例名 | `comp->local_name()`（**新增字段**，ADR-059 D4） | `u_pipeline` |
| 模块内信号 | **ctx 级全局名**（去掉 owner 前缀会丢信息 ⇒ ⇒ **保留完整路径**） | `top_p_c_r` |
| 隐式端口（**只出现在非顶层 module**） | `__if_<field>`（bundle 字段）/ `__x_<n>`（其它跨层边） | `__if_payload` |
| 冒泡端口（悬空子输入） | **ctx 级全局名**（⇒ 与扁平产物逐字相同） | `top_m_lf_unnamed_input` |

⚠️ **为什么保留完整路径前缀**（而不是压平）：
① 下游按 ctx 级名解析（N7）⇒ ⇒ 压平会**打断**它；
② 「默认全 inline ⇒ 与扁平**逐字节一致**」这条判据（ADR-061 决策④）**要求**同名；
③ 名字里保留「这根线是谁的」线索 ⇒ ⇒ ⇒ 代价只是名字偏长。

⚠️ **端口节点的原始名恒为 `unnamed_input` / `unnamed_output`**
（`include/core/io.h:141-158`）⇒ ⇒ 扁平侧靠**全局** `_N` 后缀消歧
⇒ ⇒ ⇒ **父子共享同一套后缀名** ⇒ ⇒ ⇒ 命名权威与求值顺序 MUST 写进
P1 change 的 `design.md`（先定子模块端口名 → 再由父层实例化引用同一后缀名）。

---

## 6. 与既有文档的关系

| 文档 | 关系 |
|---|---|
| `docs/developer_guide/architecture/verilog-codegen-architecture.md` §7.1 | ⚠️ **本文取代其 §7.1** ⇒ ⇒ 那份文档改为一行指针 |
| `docs/usage_guide/05-component-design.md` §5 | **面向用户的现状说明** ⇒ 由 P5 change 同步 |
| `docs/developer_guide/patterns/HIERARCHICAL-RTL-AUTHORING-GUIDE.md` | **用户写法指南**（含粒度选择 + 方案的用户面审查）⇒ 与本文互补 |
| `docs/adr/ADR-042-bundle-field-first-class-nodes.md` | bundle 成员跨层是**已声明支持**的用法 ⇒ ⇒ ADR-060 的隐式端口策略必须覆盖它 |
| `openspec/changes/component-framework-hardening-2026-10` | 下游提出的**三处框架硬化**（ODR / const io / 节点名唯一性）⇒ ⇒ **U-4 是本方案的前置依赖**；U-1 与本方案互补 |

---

## 7. 已知残留（本方案**不做 / 未决**）

| # | 残留 | 状态 / 归属 |
|---|---|---|
| 1 | **扁平产物仍是 Verilator backend 的唯一输入** ⇒ ⇒ 长期存在**两套真值** ⇒ ⇒ **二者同源校验未建立** | ⚠️ 未决 ⇒ **独立立项**（ADR-058 §4 风险 3） |
| 2 | **跨模块 `ch_mem`**（父层调 `child.mem.aread()`） | ✅ **已裁决**（ADR-060 §2.1 第四行）⇒ **响亮拒绝** + 两条替代路径 |
| 3 | **不属于任何组件的节点**（`owner == nullptr`：context 级 clock/reset + 用户在 `main()` 里不经组件直接建的节点）的 module 归属 | ✅ **已裁决**（ADR-058 §2.1 v2.3）：**归顶层 module** + `CHWARN` ⇒ **MUST NOT** 拒绝（否则含寄存器的设计全部不可层次化，且与扁平行为分叉） |
| 4 | **层次化产物不被 Verilator backend 消费** ⇒ ⇒ 框架的「双后端 parity」能力对它**不可用** | 与残留 1 同源 ⇒ ⇒ 已裁决「backend 加只增不改入口」 |
| 5 | **端口命名契约**（顶层无框架前缀 · `__if_`/`__x_` 只出现在非顶层 module）**无静态护栏** | ⚠️ 无 ⇒ **独立 change** 加 `scripts/verify_*.sh` |
| 6 | **参数化 module**（`InstrTCM<12,32>` 共用一份带参 module） | 不做 ⇒ 先用「每类型一份 module」 |
| 7 | **多时钟域** | 不做 ⇒ 框架本身单时钟域（ADR-008） |
| 8 | **`ch_device` 多 root / 黑盒 IP 集成** | 不做 ⇒ ADR-024 已评估，另案 |
| 9 | **>64-bit 端口的 DUT 无法进入 Verilator 等价性验证**（ADR-035 的 cap） | ⚠️ 已知 ⇒ 等价性验证的 DUT 集合 MUST **排除**这类设计 |
| 10 | 层次化产物的**综合 / 布局效果评估** | 不做 ⇒ 需真实综合工具，超出本仓范围 |
| 14 | **下游装配形态与本方案的第二格冲突** —— `make_unique + build(ctx)` 不 `add_child` ⇒ 被拒绝；下游 MUST 补 `add_child` 或改用 `ch_layer`（后者以 U-4 为前置） | ⚠️ 已登记（ADR-062 决策⑤）⇒ 需下游同步 |
| 12 | **结构同构的 RTL 树提取工具未验证** —— `--xml-only` 在仓内 verilator **5.048** 已被删除（`Changes:228`）⇒ 只能走 `--json-only` + `.tree.meta.json` 指针 join | ⚠️ **P0 之前的阻塞 spike**（见 change 的 `design.md` §6.2）⇒ 提不出 ⇒ 判据重设计 |
| 13 | **Verilator backend 的「只增不改入口」远不止加一个函数** —— 端口表 / `emit_sim_main_postlude` / 单文件编译 + 缓存 key 都按「ctx 全量 = 顶层端口」写死 | ⚠️ 工作量重估 ~4 天 → **~7–9 天**（ADR-058 §2.3 / design §6） |
| 11 | **用户看不到自己的设计是否「层次友好」**（诊断 API）· **用户拿不到自己 DUT 的等价性反馈**（公共 helper） | ⚠️ 用户视角审查提出 ⇒ **待用户拍板**（路径图「用户视角审查新增的两个产品缺口」） |

---

## 8. 落点表（本文内容 → 未来文档）

| 内容 | 落点 |
|---|---|
| 分析算法伪码 · 发射流程 · 时钟复位贯通 | P1/P3 change 的 `design.md` |
| 边界情形矩阵（逐条） · 可勾选任务 | P3 change 的 `tasks.md` |
| 等价性测试设计 · 映射表格式 | P4 change 的 `design.md` |
| 护栏设计 | 对应 change 的 `design.md` + `scripts/verify_*.sh` |
| **决策** | ADR-058 / 059 / 060 / 061（已落盘） |
| **排期 · 候选 change · 验收** | `.rddf/roadmap/features/feat-hierarchical-rtl-output.md`（已落盘） |
| **实现载体** | `openspec/changes/hierarchical-rtl-output-2026-11/`（proposal / design / tasks / specs delta） |

---

## 9. 附录 A —— 可复算的归属探针

⚠️ **证据必须可复算**（本仓反复吃过「探针在 `/tmp` ⇒ 审稿无法核验」的亏）
⇒ ⇒ ⇒ 探针源码**入仓**在此 ⇒ ⇒ P0 change MUST 把它固化为
`tests/test_component_ownership.cpp` 的断言。

```cpp
// 探针：打印「节点 → 归属组件」与「组件树」
// 用法（在仓根）：g++ -std=c++23 -I include -I include/core -I include/ast
//   -I include/utils -I external/inipp <此文件> build/libcpphdl.a \
//   $(llvm-config-18 --ldflags --libs --system-libs) -o /tmp/probe && /tmp/probe
#include "ch.hpp"
#include "simulator.h"
#include <iostream>
using namespace ch::core;

class Leaf : public ch::Component {
public:
    __io(ch_in<ch_uint<8>> a; ch_out<ch_uint<8>> y;)
    Leaf(ch::Component* p=nullptr, const std::string& n="leaf") : ch::Component(p,n) {}
    void create_ports() override { new (io_storage_) io_type; }
    ch_reg<ch_uint<8>> r{0_d, "r"};          // 成员 ⇒ 归属应 = Leaf
    void describe() override { r->next = io().a; io().y <<= r; }
};
class Mid : public ch::Component {
public:
    __io(ch_in<ch_uint<8>> in; ch_out<ch_uint<8>> out;)
    Mid(ch::Component* p=nullptr, const std::string& n="mid") : ch::Component(p,n) {}
    void create_ports() override { new (io_storage_) io_type; }
    void describe() override {
        ch::ch_module<Leaf> lf{"lf"};        // describe 内局部 ⇒ 归属应 = Mid
        lf.io().a <<= io().in;
        io().out <<= lf.io().y;
    }
};
class Top : public ch::Component {
public:
    __io(ch_in<ch_uint<8>> a; ch_out<ch_uint<8>> y;)
    Top(ch::Component* p=nullptr, const std::string& n="top") : ch::Component(p,n) {}
    void create_ports() override { new (io_storage_) io_type; }
    void describe() override {
        ch::ch_module<Mid> m{"m"};
        m.io().in <<= io().a;
        io().y <<= m.io().out;
    }
};

int main(){
    ch::ch_device<Top> d;
    auto* ctx = d.context();
    for (auto& np : ctx->get_nodes()) {
        auto* n = np.get();
        auto* p = n->get_parent();
        std::cout << "  " << n->name() << "  <- parent="
                  << (p ? p->name() : "(null)") << "\n";
    }
}
```

**实测输出（缺陷态，2026-11-02；已实测可复现，上面给的编译命令可直接跑）**：

```
  default_clock            <- parent=(null)
  default_reset            <- parent=(null)
  top.unnamed_input        <- parent=top
  top.m.unnamed_input      <- parent=top.m
  top.m.ch_lit             <- parent=top.m
  top.m.r                  <- parent=top.m           ← ❌ 缺陷 ①（r 实属 Leaf）
  top.m._r                 <- parent=top.m
  top.top.m.lf.unnamed_input  <- parent=top.top.m.lf  ← ❌ 缺陷 ③（前缀重复）
  top.top.m.lf.unnamed_output <- parent=top.top.m.lf
```

⚠️ 注意 `default_clock` / `default_reset` 的 parent = **`(null)`** ⇒ ⇒ ⇒
**context 级节点不归属任何组件**（ADR-059 §2.3）⇒ ⇒ emitter 必须对它们**单独处理**。

⇒ ⇒ ⇒ **P0 落地后的期望**：`r` 的 parent = `top.m.lf`；`lf` 的路径 = `top.m.lf`（无重复）。
