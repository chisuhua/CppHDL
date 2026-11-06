# 层次化 RTL 输出 —— 目标架构（需求 · 模块关系 · 结构事实）

> 本文件 = **架构主题文档**：只承载**当前态快照**·**下游需求**·**目标形态的结构事实**
> （模块关系、数据结构、命名规则、已知残留）。
> **决策**在 ADR-058（边界与等价性）· ADR-059（归属与命名）· ADR-060（端口策略与复用）·
> **ADR-061（层级粒度由用户显式控制）**· **ADR-062（单元接口类型）**。
> **算法伪码 / 发射流程 / 边界情形矩阵 / 验证策略** ⇒ ⇒ 归**未来 change 的
> `design.md` / `tasks.md`**（见 §7 的落点表）⇒ ⇒ ⇒ **本文件不重复承载**。

- Date: 2026-11-02（**2026-11-04 架构评审修订** ⇒ 见 §10）
- Status: **Draft**（随 ADR-058 / 059 / 060 / 061 / 062 拍板后转正）
- 关联决策: `docs/adr/ADR-058-hierarchical-rtl-output.md` ·
  `docs/adr/ADR-059-node-ownership-and-component-naming.md` ·
  `docs/adr/ADR-060-hierarchical-port-policy-and-module-reuse.md` ·
  `docs/adr/ADR-061-hierarchical-granularity-user-control.md` ·
  `docs/adr/ADR-062-hierarchical-unit-interface-typing.md`
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
| module 名 | `comp->module_type_name()` —— **成层**者按 ADR-059 §2.2.1 **四档**取（① 类级 `CH_HIERARCHY_LAYER("name")` · ② `ch_layer<T>` 继承 · ③ `ch_layer<Helper>` 实例处给名 ⇒ ⇒ **①②③ 缺名编译期不可表达** · ④ `ForceLayer` 回退 `sanitize(typeid)` + `CHWARN`）；**非成层**者不参与 module 命名 | `rv32i_pipeline` |
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

> **2026-11-06 修订（architect · T3-005 实施后审查）**：残留 1 拆为两条（**用户面缺口** ⇒ P8 · **语料级回归缺口** ⇒ 独立立项）⇒ 残留 4 改为「**已收口**」⇒ 措辞按 oracle 建议收紧（「同源校验已建立」→「『层次化 ≡ 扁平』的逐拍一致性判据已建立」+ 判别力边界点名）⇒ ⇒ 记录 = `.rddf/reviews/T3-005-equivalence-debt-review.md`

| # | 残留 | 状态 / 归属 |
|---|---|---|
| 1 | **用户面缺口**：层次化产物的等价性 harness 仍在 `tests/` 内部 ⇒ ⇒ **用户自己的设计拿不到**「层次化后行为不变」的逐拍反馈（⇒ ⇒ 这才是 P8 的原始痛点） | ⚠️ 未决 ⇒ 归 **P8**（公共 helper 立项） |
| 1b | **语料级回归缺口**：层次化产物在全仓测试语料上的行为覆盖 = 仅 T3-005 的 **5 个专用 DUT**（`DUAL_BACKEND_TEST_PAIR` **不参数化粒度** ⇒ 既有语料**一律只跑扁平**）⇒ ⇒ 层次化路径在**既有语料**上零覆盖 | ⚠️ 未决 ⇒ **独立立项** ⇒ ⚠️ **MUST NOT** 并入 P8（P8 的交付物是「公共 helper」，**不含**给既有语料补层次化覆盖 ⇒ 并进去 ⇒ 按 P8 的 done 判据交付后该缺口**静默留存**） |
| 2 | **跨模块 `ch_mem`**（父层调 `child.mem.aread()`） | ✅ **已裁决**（ADR-060 §2.1 第四行）⇒ **响亮拒绝** + 两条替代路径 |
| 3 | **不属于任何组件的节点**（`owner == nullptr`：context 级 clock/reset + 用户在 `main()` 里不经组件直接建的节点）的 module 归属 | ✅ **已裁决**（ADR-058 §2.1 v2.3）：**归顶层 module** + `CHWARN` ⇒ **MUST NOT** 拒绝（否则含寄存器的设计全部不可层次化，且与扁平行为分叉） |
| 4 | ~~**层次化产物不被 Verilator backend 消费**~~ ⇒ ⇒ **T3-005 已收口**（2026-11-06）：backend **已可**消费层次化产物（§2.3 E1 新入口 + 由 emitter 产物派生的端口集合）⇒ ⇒ **「层次化 ≡ 扁平」的逐拍一致性判据已建立**（比较面 = **两份 Verilog 产物** × 同一 Verilator backend ⇒ 判别「层次化**变换**」引入的行为分叉）⇒ ⇒ ⚠️ **判据边界**：它**不**判别「扁平/层次化**共享的发射底座**算错」（两产物同源 ⇒ 同构缺陷仍会一起错）⇒ ⇒ 后者仍归**既有** parity + 手推期望值义务（parity 对该族缺陷**结构性失明**） | ✅ 已收口（T3-005）⇒ ⇒ 收口记录 = `.rddf/reviews/T3-005-equivalence-debt-review.md` |
| 5 | **端口命名契约**（顶层无框架前缀 · `__if_`/`__x_` 只出现在非顶层 module）**无静态护栏** | ⚠️ 无 ⇒ **独立 change** 加 `scripts/verify_*.sh` |
| 6 | **参数化 module**（`InstrTCM<12,32>` 共用一份带参 module） | 不做 ⇒ 先用「每类型一份 module」 |
| 7 | **多时钟域** | 不做 ⇒ 框架本身单时钟域（ADR-008） |
| 8 | **`ch_device` 多 root / 黑盒 IP 集成** | 不做 ⇒ ADR-024 已评估，另案 |
| 9 | **>64-bit 端口的 DUT 无法进入 Verilator 等价性验证**（ADR-035 的 cap） | ⚠️ 已知 ⇒ 等价性验证的 DUT 集合 MUST **排除**这类设计 |
| 10 | 层次化产物的**综合 / 布局效果评估** | 不做 ⇒ 需真实综合工具，超出本仓范围 |
| 14 | **下游装配形态与本方案的第二格冲突** —— `make_unique + build(ctx)` 不 `add_child` ⇒ 被拒绝；下游 MUST 补 `add_child` 或改用 `ch_layer`（后者以 U-4 为前置） | ⚠️ 已登记（ADR-062 决策⑤）⇒ 需下游同步 |
| 12 | ~~**结构同构的 RTL 树提取工具未验证**~~ | ✅ **已解决**（2026-11-02 · C0-0 spike）：`verilator --json-only` 的 `.tree.json` 里 `modulesp` 是**模块列表**、CELL 的 `modp` 可**一次查表** ⇒ ⇒ 提取器 `scripts/lib/verilog_tree_extract.py` + 护栏（含负向对照）+ CI 挂载已落地 |
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

---

## 10. 架构评审结论（2026-11-04 · architect 角色）

> ⚠️ **本节是评审产物**，记录**事实**（drift / 缺口）与**重整后的落点** ⇒ ⇒ ⇒
> **决策**仍在 ADR；**实现步骤与验收**仍在各 change 的 `design.md` / `tasks.md`。
> ⚠️ 本节**不新增**任何裁决 ⇒ ⇒ 凡涉及**取舍**的项一律列进 §10.5「待拍板」，由用户裁决。

### 10.1 治理状态（**开工前的前置**）

| # | 事实 | 证据 | 现状（2026-11-04 评审后） |
|---|---|---|---|
| ① | **5 份 ADR 原为 Status = `Proposed`**（2026-11-02 · 「待用户拍板」）⇒ ⇒ 而 `hierarchical-rtl-output-2026-11` 已按它们起草完毕 | `docs/adr/ADR-058..062` 的 Status 行 | ✅ **已转正为 `Accepted`**（用户拍板 · 各 ADR §6 已记版本号） |
| ② | 本仓惯例是「**先立 ADR（Accepted）⇒ 再立 change**」⇒ ⇒ 原状是「**ADR 未 Accepted ⇒ change 已就绪**」 | 先例 `get-lnode-entry-set-2026-10` 引用 ADR-053 | ✅ 已消解 |
| ③ | 其中**部分**条目已由用户**局部拍板**（ADR-058 v2.1「backend 只增不改例外」· ADR-060 v2.1「跨模块 `ch_mem` 拒绝 + 默认策略」· ADR-061 v1.2「装配点任意但 MUST 登记」）⇒ ⇒ 但**整体 Status 未转正** ⇒ ⇒ 「局部拍板」与「ADR 生效」在文档上**不可区分** | 各 ADR §6 变更记录 | ✅ 整体转正后不再有「局部 vs 整体」的歧义 |
| ④ | `component-framework-hardening-2026-10` **无 ADR**（proposal 自陈「本 proposal 即决策输入」）⇒ ⇒ 它**新建 3 个 capability** ⇒ ⇒ 一个**无 ADR 背书**的 change 却是大工程的**硬前置** | 该 change 的 `.openspec.yaml` + proposal §Why | ⚠️ **仍开放** ⇒ ⇒ 见 §10.4：拆成 **T2a**（U-4 = 1 词）与 **T2b**（U-1 + U-2）⇒ ⇒ **T2a 的决策面小到不需要独立 ADR** ⇒ ⇒ **T2b 才需要**（它新建 2 个 capability） |

### 10.2 跨 ADR 矛盾（**本轮已消解 5 处**）

| # | 矛盾 | 处置 |
|---|---|---|
| ① | **ADR-058 §4 风险 5 / §5 护栏**（「`CH_MODULE_NAME` 缺失 ⇒ MUST 回退、MUST NOT 编译失败」）**与 ADR-059 D4 v2.2**（「成层组件 MUST 显式声明、缺省**硬错**」）**互斥** ⇒ ⇒ 两条 Verification 判据**不可同时满足** ⇒ ⇒ ⚠️ **进一步**：裁决 B（合并宏）后，「运行期硬错」这条兜底**本身也不需要了**，但「合并宏」只封**类级**路径 ⇒ ⇒ 成层还可由 `ch_layer<T>` / `ForceLayer` 产生 ⇒ ⇒ 原「回退无害」的论证**被证伪** | ✅ **已消解**（ADR-058 v2.8 · ADR-059 D4 v2.3 + **§2.2.1 四档来源** · ADR-061 v1.6/v1.6.1）⇒ ①–③ 用户显式给名（缺名**编译期**不可表达）· ④ `ForceLayer` 回退 + `CHWARN`（仅「同 ABI」可复算）⇒ ⇒ 两份 capability delta 已同步（critic B1 收口） |
| ② | ADR-059 D4 已确立「两次生成字节相同」**只在同一编译器/ABI 内**成立 ⇒ ⇒ ADR-058 §5 确定性判据**未带该限定** | ✅ **已消解**（ADR-058 v2.7）⇒ 补「同 ABI」限定 + 注明 2026-11-03「CI 只做 Ubuntu」后**仓内无跨平台对照证据** |
| ③ | ADR-058 §5 的效果判据列表里**仍留着** `hier.top_ports ⊆ flat.top_ports` ⇒ ⇒ 该形态**已被 v2.5 撤销** ⇒ ⇒ 与本 ADR 自己的 §2.2 ③ **互斥** | ✅ **已消解**（ADR-058 v2.8）⇒ 改为引用 §2.2 ③ 的两条精确判据（输入**相等** + 输出**等于精确期望集**） |
| ④ | **ADR-061 §5 的一条 Verification 未限定作用域**（「被 inline 的组件…任意 `output` ⇒ 顶层 `output`」）⇒ ⇒ 与决策② 的**两作用域表**冲突 ⇒ ⇒ ⚠️ **决策段一直是自洽的**（v1.2 已定两作用域表）⇒ ⇒ **只有 Verification 段漏了限定** ⇒ ⇒ change 侧（delta / design / tasks）**全部按正确读法落地** ⇒ ⇒ 漂移**只在 ADR** | ✅ **已消解**（ADR-061 v1.6）⇒ 补作用域限定 |
| ⑤ | ADR-062 §5 的「观测/跨层读取（情形①②③）」**未标注适用范围** ⇒ ⇒ 而 change 的 tasks C2.8 **单方面豁免**了②③ ⇒ ⇒ 「任务层豁免一条 ADR MUST」 | ✅ **已消解**（ADR-062 v1.4）⇒ 明示「**① 在本仓可测**；**②③ 的判据落在下游** ⇒ **本仓不适用**」⇒ ⇒ tasks 的豁免表述随之与 ADR 对齐 |

### 10.3 契约载体映射（**ADR 裁决 ⇒ capability Requirement**）

⚠️ **判据**：ADR 里每一条**规范性义务**（MUST / MUST NOT）⇒ ⇒ MUST 有**契约载体**
（capability 的 Requirement / Scenario）⇒ ⇒ 否则归档后**该义务即消失** ⇒ ⇒ 后人可重新引入静默失效。

| ADR | 裁决 | 契约载体（**编号 = 该 capability delta 文件内 Requirement 的出现顺序**） | 状态（评审后） |
|---|---|---|---|
| ADR-058 §2.1 | 模块边界 = 所有权边界 · `owner==nullptr` 归顶层 | `rtl-hierarchical-output` R2 / R7 | ✅ |
| ADR-058 §2.2 | 等价性四条（映射表四元组 · 输入相等 · 输出=精确期望集 · 观测逐拍全等） | R9 | ✅ |
| ADR-058 §2.3 | 独立公开入口 + 错误模型（校验先于 I/O · 临时**文件**+rename · 不落 `catch(...)` · 返回码兜底）+ **单文件多 module**（v2.9：撤销「多文件布局」）| **R13**（评审新增）| ✅ **已补**（⚠️ **R13 在主 spec 不存在** —— 悬空引用属**既有**问题 ⇒ ⇒ 记 backlog，**不进**本轮返工队列）|
| ADR-058 §5 | **零**跨 module 边界的 `assign` | R2 的 Scenario | ✅ |
| ADR-059 D1 / D3 | 构造作用域定归属 · `name()` 拼路径 | `component-ownership-and-naming` R1 / R2 | ✅ |
| ADR-059 **D2′ / D2″** | build context 取**显式来源** · env ctx 空时 MUST NOT 硬失败 · **跨 ctx 守卫（基准分两支）** | **R5 + R6**（评审新增） | ✅ **已补** |
| ADR-059 **D4** | `local_name_` / `module_type_name()` · **模块类型名的四档来源** · 「同 ABI」限定 | **R3 + R4**（评审新增并改写） | ✅ **已补** |
| ADR-059 **D7** | 归属 ↔ 语义分流（`io.h` 四处守卫改用 `node_owner()`）· 判据改「改前改后均无触发」 | — | 🔴 **仍无载体**（内部约束 · §10.6 ⑨） |
| ADR-059 **D8** | `ch_module` 无 active parent ⇒ `CHFATAL_EXCEPTION`（非 `CHERROR`）；根组件/手工装配 **不适用** | **R7**（评审新增） | ✅ **已补** |
| ADR-059 **D9** | `add_child` MUST 补绑 `parent_` · 归属判定 MUST NOT 误杀成员子组件（策略属 ADR） | **R8**（评审新增） | ✅ **已补** |
| ADR-059 **D10** | context 级 clock/reset MUST 用「无归属」哨兵 | — | 🔴 **仍无载体**（内部义务 · §10.6 ⑨） |
| ADR-060 §2.1 | 隐式端口**按方向**分列 · 跨模块 `ch_mem` 响亮拒绝 · **MUST NOT 改写 IR** · 第三轴（driver 的 owner 在更上层 ⇒ 逐级 pass-through） | R5 / R6（**第三轴的 Scenario 为评审新增**） | ✅ **已补第三轴** |
| ADR-060 §2.2 | 悬空输入**冒泡**（复用 ctx 级名） | R6 | ✅ |
| ADR-060 §2.3 | **MUST NOT 复用** module（每实例一份 · `<type>_<序号>`） | R3 的 Scenario | ✅ |
| ADR-060 §2.4 | 两个**线性枚举**（`PortPolicy` / `UndrivenPolicy`）+ 默认值（`SynthesizeWarn` / `Bubble`）· MUST NOT 布尔 | R13（**只覆盖「用线性枚举」这一条**） | 🟡 **枚举名与默认值仍无载体**（§10.6 ⑫） |
| ADR-061 决策① / ⑤ | 显式声明（含**模块类型名**）· 默认 inline · `Granularity` 三档 | R1 | ✅ |
| ADR-061 决策② | **四角色**端口表（自身端口 / 隐式输出端口 / 体内 wire / 子端口连接网） | R11（隐含） | 🟡 **无独立 Requirement**（§10.6 ⑬） |
| ADR-061 决策④ | 命名权威 = ctx 级全局表 | R3 | ✅ |
| ADR-061 §5 | inline **不产生隐式端口** | — | 🔴 **无载体**（§10.6 ⑭） |
| ADR-062 §2.1 | 层次化单元接口 MUST 是**结构化端口** · 成层组件全部端口成端口（不论是否被驱动） | R11 / R12 | ✅ |
| ADR-062 §2.6 | **接口白名单** + 数组字段命名 `x_<i>` | **R14**（评审新增 · 用户拍板） | ✅ **已补** |
| ADR-062 §2.6 | 观测/跨层读取**三段裁决** | R5 的 Scenario（情形①） | ✅ ②③ = **下游义务** ⇒ ADR-062 v1.4 已明示「本仓不适用」 |
| ADR-062 决策⑤ | 下游装配 MUST 补 `add_child`（或改 `ch_layer`）· MUST 自算全路径名 | R8 | ✅ |
| ADR-062 规范段 N-3 | 接口字段名与顺序 MUST 稳定（义务方 = **上游**） | — | 🔴 **无载体**（§10.6 ⑮） |

⇒ ⇒ **统计（评审后）**：契约载体 **2 → 8 条 Requirement / 3 → 16 Scenario**（`component-ownership-and-naming`）
· **12 → 14 条 / 23 → 30**（`rtl-hierarchical-output`）⇒ ⇒ **原 7 条「无载体」已补 5 条**
⇒ ⇒ **余 4 条**（ADR-059 D7 / D10 · ADR-060 枚举名与默认值 · ADR-061 四角色 / inline 不产生隐式端口 · ADR-062 N-3）
⇒ ⇒ ⇒ 这 4 条的性质 = **内部实现约束** 或 **下游义务** ⇒ ⇒ **不是「响亮失败」类** ⇒ ⇒ 优先级低于已补的 5 条
⇒ ⇒ ⇒ **最集中的一条已解**：`component-ownership-and-naming` 原只有 **2 条 Requirement** 覆盖 ADR-059 的 **D1–D10**
⇒ ⇒ ⇒ 归属决策的「失败模式」面（D2′/D2″/D4/D8/D9）**已全部有载体**。
⇒ ⇒ **4 条 🟡 部分/隐含**（ADR-058 错误模型 · ADR-060 第三轴 · ADR-060 复用 · ADR-061 四角色）
⇒ ⇒ ⇒ **最集中的一条**：`component-ownership-and-naming` 只有 **2 条 Requirement**，
而 ADR-059 有 **D1–D10**（10 条裁决）⇒ ⇒ ⇒ **归属决策的「失败模式」面几乎裸奔**
（响亮失败 / 跨 ctx 守卫 / 组件树登记 / 命名确定性）⇒ ⇒ 这些恰恰是**静默失效**的高发区。

### 10.4 重整后的落点（**3 个 change ⇒ 10 个**）

⚠️ **重整的判据**（沿用 feature 文件的既有裁决：「①a/①b/①c MUST 拆开 ⇒ 捆在一起 ⇒ 回归变红**无法归因**」）：
**一个 change = 一组回归判据** ⇒ ⇒ 判据不同的裁决 MUST 分开 ⇒ ⇒ 且**前置依赖**要显式化。

| # | change | 内容 | 前置 | 契约载体 | 规模 |
|---|---|---|---|---|---|
| **T2a** | `header-odr-inline-fix-2026-11` | **U-4 only**：`module.h:13` 加 `inline` + multi-source 链接 fixture | 无 | `core-header-odr-safety` | **1 词 + 1 fixture** |
| **T2b** | `component-io-const-and-node-name-policy-2026-11` | **U-2**（const `io()`）+ **U-1**（opt-in 节点名唯一性） | 无 | `component-io-const-access` · `core-ctx-node-name-uniqueness` | 小 |
| **T3-①a** | `hierarchical-rtl-naming-fix-2026-11` | **D3**（`build_hierarchical_name` 用 `name()`）+ **D4**（`local_name_` / `module_type_name()` · **成层声明与模块命名合并为一个宏** `CH_HIERARCHY_LAYER("name")`） | T2a | `component-ownership-and-naming` | 小 · **零语义变更** |
| **T3-①b** | `hierarchical-rtl-ownership-fix-2026-11` | **D1 / D2′ / D2″ / D6 / D7 / D8 / D9 / D10**（归属真源） | T3-①a | `component-ownership-and-naming` | 中 · **耦合 simulator 分流 + `io.h` 守卫** |
| **T3-①c** | `hierarchical-rtl-granularity-declaration-2026-11` | ADR-061 决策①⑤⑥（`CH_HIERARCHY_LAYER` / `ch_layer` / `ch_inline` / `is_layer()` / 无 parent 装配点） | T3-①b | `rtl-hierarchical-output` | 中 |
| **T3-②** | `hierarchical-rtl-analysis-2026-11` | 分析层 + golden 文本 | T3-①c | `rtl-hierarchical-output` | 中 |
| **T3-③** | `hierarchical-rtl-emitter-split-2026-11` | emitter 拆分（**含时钟名参数化**） | T3-①b | `rtl-hierarchical-output` | 中 |
| **T3-④** | `hierarchical-rtl-emission-2026-11` | 层次发射 · 隐式端口 · 冒泡 · 拒绝 | T3-② + T3-③ | `rtl-hierarchical-output` | 大 |
| **T3-⑤** | `hierarchical-rtl-equivalence-2026-11` | backend 只增不改入口 + ≥5 DUT 等价 | T3-④ | `rtl-hierarchical-output` | **~7–9 天** |
| **T3-⑥** | `hierarchical-rtl-docs-2026-11` | 文档同步 | T3-⑤ | — | 小 |

```
T2a（1 词 · 硬前置）
  └─▶ T3-①a（纯命名 · 零语义变更）
        └─▶ T3-①b（归属真源 · 耦合仿真分流 ⇒ 独立回归面）
              ├─▶ T3-①c（粒度声明）
              │     └─▶ T3-②（分析层 + golden）
              └─▶ T3-③（emitter 拆分）───┐
                                          └─▶ T3-④（层次发射）
                                                └─▶ T3-⑤（等价性）
                                                      └─▶ T3-⑥（文档）

T2b（U-1 + U-2）—— **与主线无依赖** ⇒ 可全程并行
```

⚠️ **与现状的差异（3 处，都是「把隐含的依赖显式化」）**：
① **T2 拆成 T2a / T2b** ⇒ ⇒ **只有 U-4 是硬前置**（ADR-062 决策⑤ 只点名 U-4）
⇒ ⇒ U-1 / U-2 与层次化**无依赖** ⇒ ⇒ 不该挂在主线前面；
② **C1 拆成 T3-①a / T3-①b** ⇒ ⇒ 依据 = feature 的既有裁决「①a/①b/①c MUST 拆开」
⇒ ⇒ 而现状 change 把 ADR-059 的 **D1–D10 全揉进 C1** ⇒ ⇒ **违反 feature 自己的裁决**；
③ **T3-①c（粒度）与 T3-②（分析）之间**插入依赖 `T3-①c`（现状 change 的 C2 → C3 已有此依赖 ✓）。

### 10.5 裁决（**2026-11-04 · 用户已拍板**）

| # | 裁决点 | 裁决 | 落地 |
|---|---|---|---|
| **A** | 5 份 ADR 的 Status | ✅ **全部转正为 `Accepted`** | ADR-058 v2.8 · 059 v2.3 · 060 v1.6 · 061 v1.6 · 062 v1.4（各 §6 已记） |
| **B** | 「成层声明」与「模块命名」是否合并为一个宏 | ✅ **合并为 `CH_HIERARCHY_LAYER("name")`** ⇒ ⇒ 「成层但无模块名」**编译期即不可表示** ⇒ ⇒ `CH_MODULE_NAME` **不单独存在** | ADR-059 D4 v2.3 · ADR-061 决策① v1.6 · capability `component-ownership-and-naming` R3/R4（已改写） |
| **C** | 缺名的硬错**触发时机** | ✅ **随 B 落在声明时（编译期）** ⇒ ⇒ 「写这个宏就必须给名字」⇒ ⇒ `ForceInline` 模式**不误伤** | 同上 |
| **D** | 「不复制每个 `ch_out`」的范围 | ✅ **维持现状 = 仅成层子树**（ADR-061 决策②）⇒ ⇒ ⚠️ 但契约面 MUST **显式写明**（否则「顶层端口表 = 扁平端口表」与「成层子树不复制」两条会打架） | capability `rtl-hierarchical-output` R11 + R2（顶层单列的 Scenario）已写明 |
| **E** | ADR-062 §2.6 **接口白名单**是否进 capability | ✅ **进**（新增 Requirement）⇒ ⇒ 否则 tasks C2.7 的交付物**归档即消失** | capability `rtl-hierarchical-output` **R14**（已加） |

### 10.6 change 文档修订清单（**builder 执行**）

⚠️ **本清单是 architect 对 change 的评审产出** ⇒ ⇒ **不是** architect 自己的待办
⇒ ⇒ ⇒ 落笔在 `openspec/changes/hierarchical-rtl-output-2026-11/{proposal,design,tasks}.md`
⇒ ⇒ ⇒ **MUST NOT** 与 §10.7 的结构问题混为一谈。

#### A. 口径统一（**同一 change 内三份文件口径不一**）

| # | 位置 | 现状 | 应为 |
|---|---|---|---|
| ① | `proposal.md` §3 | 「子端口由父层表达式驱动 ⇒ **图重写**」 | 「⇒ **发射期虚拟重定向**（**MUST NOT** 改 IR · ADR-060 §2.1）」 |
| ② | `proposal.md` §4 | 「`hier.top_ports ⊆ flat.top_ports`」 | ADR-058 v2.8 的**四条**口径（输入**相等** · 输出**= 精确期望集** · 映射表 · 观测逐拍全等）⇒ ⇒ `⊆` **单独不是有效判据** |
| ③ | `design.md` §3 伪码 ④ / §5 矩阵 #16 / `tasks.md` C5.3 | 「**图重写**」 | 同 ① |
| ④ | `design.md` §6 验证策略表 | 「`hier.top_ports ⊆ flat.top_ports`」 | 同 ② |
| ⑤ | `proposal.md` Acceptance「归属 ↔ 语义分流」 | 「触发面与变更前**逐项一致**」 | 「**改前改后均无触发**」（ADR-059 D7 v2.2 已改判 ⇒ ⇒ proposal 用的是**改判前**的措辞 ⇒ ⇒ 实施者会为「触发面相同」而被新误报卡死） |
| ⑥ | `tasks.md` C1.4 | 「默认 sanitize(`typeid(*this).name()`)」 | 按 **裁决 B/C**：模块类型名的**四档来源**（ADR-059 §2.2.1）⇒ ⇒ 成层者缺名**编译期**不可表达 |
| ⑥b | **合并宏的签名面**（critic SF3 · 共 **5 处**仍是无参 / 旧形态） | `design.md:56`（`CH_HIERARCHY_LAYER;` 无参）· `design.md:59`（`ch_layer<Sub> s{"s"}` 未体现 ③）· `tasks.md:54-55` · `proposal.md:59-60` · `docs/developer_guide/patterns/HIERARCHICAL-RTL-AUTHORING-GUIDE.md:236`（仍写「只能靠 `CH_MODULE_NAME` 宏」） | 全部改为 `CH_HIERARCHY_LAYER("name")` 形态；helper 提升写 `ch_layer<Helper> h{"h","helper_layer"}`；用户面指南删掉 `CH_MODULE_NAME`（该宏**不存在**） |

#### B. 判据落地缺口（**ADR 有判据 · tasks 无验收项**）

⚠️ oracle 全量核对：5 份 ADR 共 **22 条** Verification 判据在 `tasks.md` **无对应验收项**
⇒ ⇒ 下列为**最关键**的 9 条（其余按 §10.3 映射处理）：

| # | 缺失的验收项 | ADR 出处 | 为什么要紧 |
|---|---|---|---|
| ⑦ | **输入端口集合 MUST 相等** + **输出端口集合 MUST 等于精确期望集**（逐项配对） | ADR-058 §2.2 ③ | ⭐ 这是 ADR-058 v2.5 用来替换空洞 `⊆` 的**核心裁决** ⇒ ⇒ 现在只活在 design 与 spec 里 ⇒ ⇒ **tasks 零命中** ⇒ ⇒ 漏发一半端口**无人抓** |
| ⑧ | **零**跨 module 边界的 `assign`（所有形态） | ADR-058 §5 | 「结构正确」的**唯一**可判定形式 ⇒ ⇒ 无它 ⇒ module 边界失守也可通过 |
| ⑨ | **顶层 `ch_device<Pipeline>` 可实例化且不 SIGSEGV**（= `examples/removed_examples.txt:26` 归位条件） | ADR-059 §5 | 旗舰判据 ⇒ ⇒ proposal / feature **都有** ⇒ ⇒ **tasks C1.5 的探针清单里没有** |
| ⑭ | **inline 组件 MUST 不产生隐式端口** | ADR-061 §5 | 「inline 对产物不可见」的另一半 ⇒ ⇒ 漏了 ⇒ ⇒ inline 组件被误判为跨模块时**无探针** |
| ⑮ | **ForceInline vs ForceLayer 逐拍全等** | ADR-061 §5 / ADR-058 §5 | 「inline ≡ 成层语义等价」⇒ ⇒ 这条**只在** ADR 与 design 里 ⇒ ⇒ tasks 无 |
| ⑯ | **接口白名单的判据**（违反 ⇒ `CHFATAL_EXCEPTION` + 点名端口） | ADR-062 §2.6 | tasks C2.7 有交付物但**无判据** ⇒ ⇒ 违约不转红 |
| ⑰ | **映射表 MUST 覆盖被观测的全部信号** | ADR-058 §5 | 「映射表被产出」≠「映射表够用」⇒ ⇒ 缺覆盖子句 ⇒ ⇒ 等价性测试可能只映射了一半 |
| ⑱ | **实例端口名的宽度 MUST 一致** | ADR-058 §5 | 现只判「名字 ∈ 端口集」⇒ ⇒ 宽度错配不被抓 |
| ⑲ | **`node_names_` 一致性**：两份产物的**信号名集合交集 MUST 完全一致** | ADR-061 §5 | C4.3 只有**机制**要求（ctx 级只读快照）⇒ ⇒ 缺**可判定**的验收 |

#### C. 任务粒度与精度（**tasks 内部**）

| # | 位置 | 问题 | 修法 |
|---|---|---|---|
| ⑳ | `tasks.md` C1.2 | 把 **D2′**（ctx 来源）与 **D8**（无 active parent ⇒ 抛）**合并成一条** ⇒ ⇒ 两者触发条件不同（**无 ctx** vs **无 parent 组件**）⇒ ⇒ 实施者可能把 D2′ 的「MUST NOT 硬失败」读成覆盖 D8 | 拆成 1.2（D2′：ctx 来源）与 1.2a（D8：无 active parent ⇒ 抛） |
| ㉑ | `tasks.md` C1.2b | D2″ 以「构造期 env ctx」为基准 ⇒ ⇒ 但 **D2′ 允许 env ctx 为空** ⇒ ⇒ 「MUST == 构造期 env ctx」在 env ctx == null 时**无定义** | 补「env ctx 为 null ⇒ 记为无约束；**首次** `build(ctx)` 绑定的 ctx 即基准」 |
| ㉒ | `tasks.md` C1.7 | 「文本变化面登记」把 ①a 的**信号名变化**与 ①b 的**归属变化**混在同一份 ⇒ ⇒ **归因链断裂**（ADR-058 §5 的零回归基准 = ADR-059 落地后的产物） | 随 C1 拆分，两份登记各归其 change |
| ㉓ | `tasks.md` C5.7 | 只定义 `Granularity` ⇒ ⇒ **两个线性枚举**（`PortPolicy` / `UndrivenPolicy`）与**默认值**未创建 ⇒ ⇒ 且 C5.3/C5.4 沿用 **boolean 味的 `strict` 措辞** ⇒ ⇒ ADR-060 §2.4「MUST NOT 留给实现者自行发明」被绕过 | 扩为定义三个枚举 + 默认值（`SynthesizeWarn` / `Bubble`）⇒ ⇒ `strict` 措辞改成枚举值 |
| ㉔ | `tasks.md` 计数 | proposal / tasks 自称 `ADDED ×1` ⇒ ⇒ delta 实为 **×7**（ownership）与 **×14**（hierarchical） | 同步 proposal §Capabilities 与 tasks 的计数 |
| ㉕ | `tasks.md` C3 | **ADR-059 D7**（归属 ↔ 语义分流：`io.h` 四处守卫改用 `node_owner()` · 判据「改前改后均无触发」）与 **D10**（context 级 clock/reset 用「无归属」哨兵）**无任务** ⇒ ⇒ 这两条是「归属真源变更 ⇒ 仿真分流可能变」的**兜底** | 补入 C1（①b）的验收项 |

---

### 10.7 change 层结构问题（**与 §10.6 分开**）

| # | 问题 | 证据 | 处置 |
|---|---|---|---|
| ① | **C2.5（接口与端口完整性）排在 emitter 之前 ⇒ 顺序倒置**：C2.6 要求「成层且非根 module 的端口 ⊇ 自身 `ch_in`∪`ch_out`」⇒ ⇒ **需要 emitter 产出的端口表**才能判；C2.9 要求「把某单元输入改成只从 host 侧通道读 ⇒ 判据 MUST 转红」⇒ ⇒ **需要 emitter**才能观察 | `tasks.md` C2.5 · feature 把「端口完整性判据」放在 **④** | **拆**：C2.5a（接口类型约定 + 白名单 ⇒ 留在 C2 后）· **C2.5b（端口完整性判据 ⇒ 移入 C5）** |
| ② | **依赖图缺节点**：`tasks.md` 的依赖图只有 C0–C4 / C5–C8 ⇒ ⇒ **C2.5 无人负责** | `tasks.md` 依赖图 | 随 ① 一并补节点 |
| ③ | **C1 把 ①a（纯命名）与 ①b（归属真源）合并** ⇒ ⇒ **违反 feature 自身的裁决**（「①a/①b/①c MUST 拆开 ⇒ 捆在一起 ⇒『回归变红』无法归因」）⇒ ⇒ 连带 ⇒ capability delta **无法表达**「①a ADDED ×N → ①b MODIFIED ×M」⇒ ⇒ **①a 的增量不可单独归档 / 单独回退** | `tasks.md` C1 · feature 的候选 change 表 | ✅ **已裁决**（§10.5）⇒ 按 §10.4 拆成 **T3-①a / T3-①b** |
| ④ | **feature 文件残留旧措辞**：「允许的差异 **MUST 逐条列明**」⇒ ⇒ 与 ADR-058 v2.4「**允许差异 = 空集**（`cmp` 退出码 0）」冲突 ⇒ ⇒ 若实施者以 feature 为准 ⇒ ⇒ 「逐字节一致」被**平凡满足** | `feat-hierarchical-rtl-output.md` 验收段 | ⚠️ **feature 不在 `arch-docs.json` 的 `arch_paths`** ⇒ ⇒ **architect 不改** ⇒ ⇒ 交 **planner / builder** 订正 |
| ⑤ | **feature 的「候选 change」表与 §10.4 的 10 个 change 不一致** | feature 的候选 change 表（①a–⑥） | 同 ④ ⇒ ⇒ 交 planner 同步 |

---

### 10.8 本轮（2026-11-04 · architect）已落地的改动

| 面 | 改动 | 验证 |
|---|---|---|
| **ADR** | 5 份 Status → `Accepted`（058 v2.8 · 059 v2.3 · 060 v1.6 · 061 v1.6 · 062 v1.4）· 消解 §10.2 的 5 处矛盾 | — |
| **capability delta** | `component-ownership-and-naming` **2 → 8** 条 Requirement / 3 → 16 Scenario · `rtl-hierarchical-output` **12 → 14** 条 / 23 → 30 Scenario | `openspec validate --strict` **valid**（exit 0）· `verify_openspec_delta.sh` **4/4 绿** |
| **架构主题文档** | 本 §10（评审结论 + 落点重整 + 裁决记录 + builder 修订清单）· 补 ADR-062 到「关联决策」· 订正命名规则表的模块名行 | — |

⚠️ **未落地（不在 architect 权限内）**：`tasks.md` 的改写（§10.6 / §10.7）· `proposal.md` / `design.md` 的口径统一
⇒ ⇒ **归 builder**；feature 文件的台账订正（§10.7 ④⑤）⇒ ⇒ **归 planner**。

---

## 11. 实施期缺口复核的裁决（2026-11-04 · architect 角色 · 第二轮）

> ⚠️ 本轮是 **T3-④ 第一部分落地后**的复核 ⇒ 输入 = 实施期新查明的 6 个事实（F1–F6）
> ⇒ ⇒ 裁决只覆盖**已暴露的缺口**，**不新增**架构能力。

### 11.1 四项待拍板的裁决

| # | 事项 | 裁决 | 落点 |
|---|---|---|---|
| **A1** | `PortPolicy::SynthesizeFatal` 的语义（ADR-060 §2.4 此前**只定义枚举、未定义语义**） | **= 「先合成完、再失败」** ⇒ 失败时 MUST 列出**全部**被合成的边 + 合成端口名 ⇒ ⇒ 与 `Reject` 的区别是「**诊断完整度**」不是「严格度」⇒ ⇒ **执行时机承重**（失败 MUST 在合成完成之后） | **ADR-060 v1.7** §2.4.1 |
| **A2** | 单文件 vs 一 module 一文件 | **单文件唯一模式** ⇒ ⇒ 撤销 `out_dir` / `one_file_per_module`（Zero-Debt）⇒ ⇒ 理由：N1 由「module 独立」满足（**不要求文件独立**）· T3-⑤ 的 backend 编译**单文件** ⇒ 多文件要**再翻倍**成本 · 错误模型简化 ⇒ ⇒ 「同名不同物」由「**文件名由调用方显式指定**」消解 ⇒ 层次化产物路径 MUST NOT 与扁平产物同名 | **ADR-058 v2.9** §2.3 + **ADR-061 v1.6.2** §2.5 |
| **A3** | `Granularity` 的权威位置 | **= 全局**（`Component::granularity()` + RAII `GranularityScope`）⇒ ⇒ 选项结构 **MUST NOT** 再放 `granularity` 字段 ⇒ ⇒ ⚠️ **理由已于 v1.6.3 订正**（原写的「装配期烘焙 ⇒ per-emit 字段无法生效 ⇒ 是必然被忽略的旋钮」经 oracle 实测**证伪**）⇒ ⇒ 现理由 = **避免双权威的优先级歧义**（见 §11.5 **O1**）⇒ ⇒ 唯一入口 = `GranularityScope`（`Component::set_granularity()` MUST 删除）⇒ ⇒ `GranularityScope` MUST **同时覆盖构造与发射**（两个读取时刻）⇒ ⇒ `ForceInline` / `ForceLayer` MUST 在**装配前**设 | **ADR-061 v1.6.3** §2.5.1 |
| **A4** | **P6**（层次友好度诊断 API）/ **P8**（等价性 harness 公共 helper） | **二者均「独立立项」** ⇒ ⇒ **不进** T3-④ / T3-⑤ 的实施范围（⇒ ⇒ MUST NOT 静默塞入）⇒ ⇒ 但**各带一条形态约束**（见 §11.2）⇒ ⇒ **不为此二项立新 ADR**（API 尚未设计 ⇒ 为未设计的 API 写 ADR = 过早固化） | 本节 §11.2 + feature 的 backlog 段（归 planner） |

### 11.2 P6 / P8 的形态约束（**给后续 change 的边界**，不是实施）

| # | 形态约束 | 理由 |
|---|---|---|
| **P6** | MUST 照搬 `ch::core::diagnose_drive` 的形态：**单枚举 + 单一判定实现 + 护栏** ⇒ ⇒ 报告：隐式端口数 / 悬空输入数 / 未消费子输出数 / 跨模块 `mem` 访问数 / **各层规模** | ⇒ 既有 `diagnose_drive` 是仓内「可判定的诊断 API」的**唯一既有范式** ⇒ ⇒ 另起一套 ⇒ ⇒ 两套判据必然漂移 |
| **P8** | T3-⑤ 的 harness MUST 写成「**可公共化**」的形态（内部实现与公共包装**分离**）⇒ ⇒ 但 T3-⑤ **MUST NOT** 暴露公共 API（⇒ ⇒ 公共化 = 独立 change） | ⇒ ⇒ 避免 T3-⑤ 被 API 面绑架（⇒ ⇒ 一旦暴露公共 API ⇒ ⇒ 语义承诺 + 兼容负担 ⇒ ⇒ 落在等价性 change 里不合适） |

### 11.3 顺带裁决：feature 边界表的两个「未裁决」项

| # | 事项 | 裁决 |
|---|---|---|
| **B1** | 根组件自己写 `CH_HIERARCHY_LAYER("x")` | **MUST 是 no-op**（根**永远**是 RTL 顶层 module）⇒ ⇒ 顶层名**只**由 `top_module_name` 决定 ⇒ ⇒ **不告警**（避免恒响）⇒ ⇒ **ADR-061 v1.6.2 §2.5.2** |
| **B4** | ADR-059 §5 仍写「触发面 MUST 不变」 | **订正为「改前改后均无触发」**（= 执行既有 D7 **v2.2**，**不是**新增决策）⇒ ⇒ **ADR-059 v2.3.2** |

### 11.4 本轮**没有**裁决的（**明确留待后续**）

| # | 事项 | 归属 |
|---|---|---|
| 1 | F2（成层组件的悬空 `ch_in` **不冒泡**）· F3（非顶层 module 的 inline 后代 `ch_out` **上了端口表** + `print_decl` 无 `type_output`-as-wire 分支） | ⇒ ⇒ **不是决策缺口，是实现缺口** ⇒ ⇒ 既有 ADR 的口径**已经正确** ⇒ ⇒ **无需改 ADR** ⇒ ⇒ 归 **builder**（T3-④ 的 B2 / B3） |
| 2 | T3-⑤ 的「观测面」实现路径（backend 显式端口集合 vs 其它候选） | ⇒ ⇒ 预估 **~7–9 天** ⇒ ⇒ 归 **planner**（T3-④ 归档后单独修订一次） |
| 3 | 层次化产物文件名 MUST NOT 与扁平产物同名 ⇒ **护栏形态**（在 T3-④ 的 `verify_hierarchical_rtl.sh` 里，还是独立判据） | ⇒ ⇒ 归 **planner + builder**（T3-④ 的 B4 护栏内） |

### 11.5 oracle 实测复核的处置（2026-11-04 · architect 角色 · 第二轮）

> ⚠️ 本轮 architect 的 4 项裁决经 **oracle** 复核 ⇒ **1 项的「理由」被实测证伪**（结论保留、理由更换）
> ⇒ ⇒ 另有 **3 项工程性发现**需要落到后续段。

| # | oracle 的发现 | 处置 |
|---|---|---|
| **O1** | **A3 的理由错了**：`is_layer()` 是**调用时**读全局（`src/component.cpp:132-141`），分析层**发射时**才调（`src/hierarchy_analysis.cpp:56`）⇒ ⇒ per-emit 粒度字段**技术上会生效** ⇒ ⇒ 「装配期烘焙 ⇒ 无法生效」**不成立** | ⇒ ⇒ **ADR-061 v1.6.3** 订正理由（保留「全局唯一权威」结论，换成「**避免双权威的优先级歧义**」—— 与 ADR-060 §2.4 否决布尔开关同病因）⇒ ⇒ 并补**优先级**（全局 > 实例级 > 类级）· **读取时刻两处** · **作用域 MUST 覆盖构造 + 发射** · **判别性判据**（只包构造 ⇒ 产物 MUST = `RespectDeclarations` 形态） |
| **O2** | `Component::set_granularity()` **零调用点** ⇒ ⇒ 是「绕过 RAII 的永久泄漏通道」 | ⇒ ⇒ **ADR-061 v1.6.3** 裁决**删除**（Zero-Debt）⇒ ⇒ 唯一入口 = `GranularityScope` ⇒ ⇒ 归 **builder**（T3-④ 的 B1 一并做） |
| **O3** | `SynthesizeFatal` 需要**collect 模式**（现发射循环**首错即返**，`src/hierarchy_verilog.cpp:307-314`）⇒ ⇒ 而 collect 化与 F2/F3 **改的是同一个循环** | ⇒ ⇒ **排期顺序**：**先 collect 化、再 F2/F3** ⇒ ⇒ 否则二次返工 ⇒ ⇒ 归 **builder**（T3-④ 的 B2 首步） |
| **O4** | 「同名不同物」**护栏空窗**是活的 footgun（API 侧无自卫 ⇒ `rename` 静默覆盖扁平基线） | ⇒ ⇒ **ADR-058 v2.9** 补写该约束 ⇒ ⇒ 护栏 MUST 含「层次化产物路径 ≠ 既有扁平产物路径」⇒ ⇒ 归 **builder**（T3-④ 的 B4 护栏） |
| **O5** | N1「层 2 单元可单独交付/综合」在**单文件**下**技术上成立**（module 自包含 ⇒ 可 `out_of_context` elaborate），但：① 端口名带**完整实例路径前缀** ⇒ ⇒ 抽出后**端口名与父设计耦合**；② module 名按**每设计**编号 ⇒ ⇒ 两份设计的产物**拼接**时 `module top` 会撞名 | ⇒ ⇒ 这两条是**命名契约**的代价（ADR-062 §2.6 已接受），**与文件模式无关** ⇒ ⇒ ⚠️ **C1 闭合后二者的定性 = 「已接受的契约代价」**（**不是**待补的缺口）⇒ ⇒ ① 由下游集成时按名重导出消化（**理由陈述 · 非判据承重** ⇒ ⇒ 不作为 MUST 条款）⇒ ⇒ ② 的补救与 C2 的轴② 同解（产物级前缀选项 · 预裁见 §11.6）⇒ ⇒ ⚠️ **MUST NOT** 把①当 bug 重开 |

### 11.6 新增的**下游确认项**（不阻塞主线 · 但越晚确认越贵）

> ⚠️ **C1 已闭合**（2026-11-05 · 用户拍板）⇒ ⇒ **C2 仍未答** ⇒ ⇒ 但 C2 **不**影响
> 「单文件唯一模式」的裁决。
> ⚠️ **C2 的「拼接撞名」有**两轴**（oracle 复核订正 —— 原先只覆盖轴① ⇒ overclaim）：
> ① **顶层 module 名** ⇒ ⇒ **已可配置**（`top_module_name`）⇒ ⇒ **有解**；
> ② **内部 module 名**（`module_type_name` + 按每设计编号）⇒ ⇒ 两份产物各含名为 `alu` 的
>    layer 时**照撞** ⇒ ⇒ ⇒ **当前无机制**。
> ⇒ ⇒ **预裁（现在定口径，实施留到 C2 有答案或 T3-⑥）**：若 C2 答「拼接」⇒ ⇒ 解 =
> **产物级 module 名前缀选项**（默认**空**前缀 · **纯增量** · **非格式变更** ⇒ ⇒ **任何时点可加**
> ⇒ ⇒ 现在**不实现**（不为未证实需求付成本）。
> ⚠️ **该前缀的**作用面 MUST 是「**非根 module**」**（根由 `top_module_name` 管辖）**
> ⇒ ⇒ 否则会出现「两个旋钮管同一个根名」的优先级歧义（critic NIT）。
>
> ⭐ **「文件级切分 = 纯下游动作」的实测背书**（2026-11-05 · critic 建议后实测）：
> 把三层层次化产物**按 module 边界**切成 3 个 `.sv`（`l3_unit.sv` / `m3_unit.sv` / `top.sv`）
> ⇒ ⇒ `verilator --lint-only --top-module top l3_unit.sv m3_unit.sv top.sv` **exit 0**
> （**无 Error / 无 Warning**）⇒ ⇒ ⇒ 「module 边界是合法切分点」由**论证**升级为**实测事实**
> ⇒ ⇒ ⇒ ADR-058 v2.10 的「不构成重开条件」由此有据。
> ⚠️ **C1 的拍板出处**：**用户在本次会话中直接拍板**（2026-11-05 · 「C1: 接受单文件多module」）
> ⇒ ⇒ 仓内**无独立的决策留痕文件**（本行 + ADR-058 v2.10 即其载体）⇒ ⇒ 后续若需复核
> 「C1 到底答了什么」，以这两处为准。

| # | 待确认 | 归属 | 若答「是」的代价 |
|---|---|---|---|
| **C1** | ChipForge 的交付管线是否接受「**单文件多 module**」？还是必须**文件级切分**（每个 module 一个 `.sv`）？ | ✅ **已答**（2026-11-05 · 用户拍板）：**接受单文件多 module** ⇒ ⇒ ⇒ 「多文件模式」**不重开** ⇒ ⇒ ⇒ ADR-058 **v2.10**（§2.3 的条件条款**撤销**）⇒ ⇒ ⇒ T3-⑤ 的 backend **不需要**文件列表 + 缓存 key 改造 ⇒ ⇒ ⇒ **成本不翻倍** |
| **C2** | 下游是否会**拼接多份层次化产物**？若是 ⇒ 撞名有**两轴**（见本节开头的两轴表述） | 下游 | ⇒ ⇒ 轴①（顶层名）**已可配置** ⇒ ⇒ 轴②（内部 module 名）**当前无机制** ⇒ ⇒ **预裁** = 产物级 module 名前缀选项（默认空 · 纯增量 · 事后可加）⇒ ⇒ **不阻塞主线** ⇒ ⇒ ⚠️ **MUST NOT** 被列为 T3-⑤ 的前置（它与「编译单文件」的 backend 假设**无关**） |

### 11.7 T3-④ 实施期的架构裁决（2026-11-05 · architect 角色）

> 触发：B2b-1 / B2b-2 落地后，实施期实测出**四处决策缺口** ⇒ ⇒ 全部经 **oracle → critic**
> 双审后落入 ADR ⇒ ⇒ 本节只留**结论 + 指针**（推导不进决策文档）。

| # | 缺口（实测事实） | 裁决 | 落点 |
|---|---|---|---|
| **A1** | `child.instance().sum <<= x`（父层用 `<<=` 改写子模块内部**运算节点的操作数**）⇒ 实测**可达**且产生跨 module 边 | **拒绝**（`CHFATAL_EXCEPTION`）⇒ ⇒ **破坏模块边界**（ADR-058 §2.1）+ 方向物理不可实现 + 替代路径 = 驱动子模块输入端口（⇒ 完整论证见 ADR-060 §2.1 第七格；本表**只留结论**） | ADR-060 §2.1「第七格」 |
| **A2** | 合成端口名 `__if_<field>` 与实现（`__if_<n>`）不符 | **订正 ADR 措辞**（隐式端点无 field 名；`n` = 该 module 内出现序，与 `__x_<n>` 对称） | ADR-060 §2.2 |
| **A3** | 未消费的子 `ch_out` 不接 ⇒ Verilator `PINMISSING`（lint exit 1）；接空连接 ⇒ exit 0 | **父层 MUST 以空连接 `.port()` 接上**；**子 input MUST NOT** 用空连接（未驱动的子 input 必须冒泡） | ADR-061 决策② |
| **A4** | `SynthesizeWarn` 的**名字**暗示「警告后继续产出」，而矩阵外形态必须始终拒绝、未实现的合成必须 fail-closed | **区分「合成不适用」与「合成未实现」**：前者始终拒绝（与策略无关）；后者 MUST fail-closed ⇒ ⇒ **`SynthesizeWarn` 不授权静默产出错误产物** | ADR-060 §2.4.1 |

⚠️ **A4 顺带产出一条归档前置判据**：**T3-④ 归档前**，默认策略 `SynthesizeWarn` 在
**全部可达形态**上 MUST 符合 ADR-060 §2.4.1（可达 = §2.1 矩阵内的行 + 第三轴 + 兄弟边；
矩阵外形态由「拒绝」覆盖）⇒ ⇒ 「部分合成 + 错误产物」**不是**合法中间态。

⚠️ **A1 的对照事实（同一探针）**：`child.instance().sum = x`（`operator=`）**不**产生边
（本地句柄重绑定）；bundle 字段（proxy，字段节点已建）**可达且产生边** ⇒ ⇒ 必须合成
（复用寄存器形态的节点级重定向）。⇒ ⇒ 「行 1 的子形态」最终清单 =
**寄存器 next ✅ / proxy ✅ / op ❌ 拒绝 / `operator=` 无处理**。

### 11.8 B2b-3 实施期的架构裁决（2026-11-05 · architect 角色）

> 触发：B2b-3（proxy 子形态）落地后，实施期实测出**两处契约缺口** ⇒ ⇒ 落入 ADR-060 **v1.10**
> ⇒ ⇒ 本节只留**结论 + 指针**。

| # | 缺口（实测事实） | 裁决 | 落点 |
|---|---|---|---|
| **W1** | 合成端口宽度只能取**子侧** ⇒ ⇒ 父侧宽度不同 ⇒ ⇒ Verilog 自动扩展/截断（**合法** ⇒ 不报）⇒ ⇒ 且产物关掉 `WIDTHTRUNC` ⇒ ⇒ **完全静默** | **跨层边两端宽度 MUST 一致；不一致 ⇒ 响亮拒绝**（点名前后端 + 两个宽度）⇒ ⇒ 与「冒泡」不同（冒泡两端同源 ⇒ 天然一致） | ADR-060 §2.1 |
| **W2** | 被重定向节点的 `src(0)` 可能已被**父层**驱动覆盖 ⇒ ⇒ 代理链穿透会引用**父层信号**（该信号在本 module 内未声明 ⇒ 非法 Verilog 且**零告警**） | **两条不变式**：R1 = 重定向目标是**本 module 内的本地 wire** · R2 = **代理链穿透 MUST 停在被重定向节点上** | ADR-060 §2.1 |

⚠️ **W2 是 B2b-3 实测踩到的真缺陷**（`print_output` 的 proxy-chain walk）⇒ ⇒ 它是
**机制的正确性前提**，不是实现细节 ⇒ ⇒ 留在契约里。

🔴 **重构禁令**（critic SF 的对策项 · 落在台账以便实施者可见）：
**任何** emitter 重构若**新增**了「读 `src(0)` / 穿透节点」的路径 ⇒ ⇒
该路径 MUST **先查重定向表**（或等价地：被重定向节点按**本地 wire 名**取值）
⇒ ⇒ 否则会引用**父层信号**（该信号在本 module 内未声明 ⇒ 非法 Verilog）
⇒ ⇒ 且**零告警**（`WIDTHEXPAND` / `WIDTHTRUNC` 均被关）⇒ ⇒ **极难发现**。
⚠️ **MUST NOT** 把该规则泛化成「停在任何有名字的节点上」—— 扁平路径下 `print_output`
今天**穿透所有 proxy（含命名的）** ⇒ ⇒ 泛化会改**扁平产物** ⇒ ⇒ 破 ADR-058 §2.3 的
「扁平产物逐字节不变」零回归判据。

⚠️ **W1 的实施落点**：合成**之前**比对两端宽度 ⇒ ⇒ 不一致 ⇒ 进入「合成不适用」的
**拒绝**清单（⇒ ⇒ 与 `PortPolicy` 无关 ⇒ ⇒ 见 ADR-060 §2.4.1 的两分栏）。

### 11.9 B2b-5 / B3 实施前的三条裁决（2026-11-05 · architect 角色）

> 触发：B2b-3 / B2b-4 落地后，B2b-5（第三轴 + 兄弟边）与 B3（冒泡 + 端口完整性）开工前
> ⇒ ⇒ 查出 **2 个决策缺口 + 1 个机制重复** ⇒ ⇒ 全部在**动手前**定死。

| # | 缺口 | 裁决 | 落点 |
|---|---|---|---|
| **G0** | 冒泡（**自下而上**）与第三轴（**自上而下**）方向相反，但**机制同构**（祖先链每级一个同名 `input` 端口 · 名字 = 该节点的 ctx 级全局名）⇒ ⇒ 两套实现必然漂移 | **MUST 共用同一套实现**（一个函数 · 参数 = 起点节点 + 方向）<br>⚠️ **本格的形态已被 §11.9.1 的 G0-1 修正** ⇒ 以「**一个 chain builder + 两种 leaf adapter**」为准 | design.md §7.2.2 + tasks 0.33 |
| **G1** | 同类型多实例的**序号顺序未定义**：现状按 `analysis.modules` 的遍历序（post-order ⇒ 依赖**组件创建顺序**）⇒ ⇒ 与 ADR-060 §2.3「按**实例路径**排序」不符 | **按实例路径的字典序**（确定性 + 与 ctx 节点 ID 分配顺序无关）⇒ ⇒ **MUST 在 B3 加判据之前落地** | design.md §7.1.4 + tasks T0h |
| **G2** | B3 的「**五类**来源可追溯」与分析层的 `HierarchyPortOrigin`（**4 类**）对不上 | 第五类 = **合成端口**（`__if_<n>` / `__x_<n>`）⇒ ⇒ 它**不在分析层**（发射期造的）⇒ ⇒ 来源信息 MUST 由 **`HierarchyEmitResult`** 承载 ⇒ ⇒ 分析层 `to_text()` 与既有字段的顺序/命名 **MUST NOT** 动 ⇒ ⇒ T3-② 的 4 份 golden + 87 断言零改动 | design.md §7.1.4 + B3 |

⚠️ **G1 的时间窗**：改动**会**改变同类型多实例的 module 名 ⇒ ⇒ **先落地再让 B3 加判据**
⇒ ⇒ 否则要动既有判据 ⇒ ⇒ 现状**无**同类型多实例用例 ⇒ ⇒ 改动对现有产物**逐字节无影响**
⇒ ⇒ **现在做最便宜**。

#### 11.9.1 oracle 对三条裁决的修正（2026-11-05 · 同轮）

| # | oracle 的修正 | 落点 |
|---|---|---|
| **G0-1** | **链是同一条**（叶 → 祖先），但**叶端点策略不同**：冒泡的叶端点是**无 driver 的 `type_input`**（origin = `Bubbled`）、第三轴的叶端点可能是 **reg / proxy**（需**重定向**）⇒ ⇒ **只用「起点节点 + 方向」当共同抽象会误标 origin 或漏 `port_drives_node`** ⇒ ⇒ MUST = **一个 chain builder + 两种 leaf adapter** | design.md §7.1.4 · ADR-060 §2.3 |
| **G0-2** | **补 F2**（G0 漏了它）：**成层组件**的无 driver `ch_in` 现状归 `Self` 而**不是**冒泡（`src/hierarchy_analysis.cpp:247-253`）⇒ ⇒ 与 ADR-060 §2.2 不符 ⇒ ⇒ 属 **B3** 的活，但 MUST 用 G0 的 chain builder（不得另写） | design.md §7.1.4 · tasks T0h |
| **G1-1** | 排序键取 **`HierarchyModule.key`**（**既有字段** · 零新增）⇒ ⇒ 「相对实例路径」需另算、且 `.`/`_` 分隔符要另定 ⇒ ⇒ **收益不抵成本** | ADR-060 v1.12 |
| **G1-2** | **MUST NOT 重排 `analysis.modules` 的 post-order 发射序**（序号分配只改**名字**） | ADR-060 v1.12 |
| **G1-3** | **同 base 不同类型 MUST 在编号前判身份** —— 现状只查**生成后**重名 ⇒ ⇒ 两个不同类型若 sanitized 后 base 相同 ⇒ ⇒ 会被**静默**编成 `_0`/`_1` ⇒ ⇒ 违反 §2.3「同名不同类型 ⇒ 响亮拒绝」 | ADR-060 v1.12 · tasks 0.31 |
| **G2-1** | **注入的 clock/reset 不在分析层端口表**（非 root module 的 clock/reset 是**发射期补的**）⇒ ⇒ 只查分析层**必然漏** | ADR-060 v1.12 |
| **G2-2** | **只加 enum 或只查分析层都会漏** ⇒ ⇒ MUST 在**发射期**（`HierarchyEmitResult.port_provenance`，value-only，含注入 clock/reset）做完整性校验（expected = 分析层端口 ∪ 合成端口 ∪ 注入 clock/reset ⇒ actual = `ModuleView.ports` ∪ `synthesized_ports`），失败 ⇒ `ok=false` | ADR-060 v1.12 · tasks T0h/B3 |

### 11.10 B3 开工前的两处契约缺口（2026-11-05 · architect 角色 · ADR-060 v1.13 + ADR-058 v2.12）

> 触发：B2b-5 落地后，B3（冒泡 + 端口完整性 + 拒绝类 + 白名单）开工前复核实现，查出
> **两个契约缺口 + 一个纯实现缺口** ⇒ ⇒ 前两项需要 ADR 裁决（缺了 ⇒ 实施者只能即兴）。

| # | 缺口 | 裁决 | 落点 |
|---|---|---|---|
| **G3** | **F2**：成层组件的悬空 `ch_in` 归 `Self` 而不冒泡（`src/hierarchy_analysis.cpp:247-253`）⇒ 顶层无法驱动。修法落点未定 | 冒泡端口 **MUST 在分析层登记**（否则顶层端口序与扁平发散）；**连接也由分析层生成**（现状连接表只从有 driver 的边生成 ⇒ 中间祖先会空连接 ⇒ 值中途断线且零报错）；顶层 `synthesized_ports` **MUST 恒空**；第三轴仍在发射期但**链条 MUST 同一份实现** | ADR-060 §2.2 G3-1..G3-6 |
| **§2.4.2** | `UndrivenPolicy` **只有枚举、零实现读取**（Zero-Debt） | 定义 `Bubble` / `Reject` 两格语义；**收窄 `Reject` 适用范围** = 只对 inline 折叠后 module 归属为**非根成层 module** 的悬空 `ch_in`（根自己的 / inline 折叠进根的悬空 `ch_in` 在扁平里本就是顶层 input ⇒ 拒绝它们 = 拒绝合法设计） | ADR-060 §2.4.2 |
| **⑤** | 错误载体两套（ADR-062 §2.6 说 `CHFATAL_EXCEPTION`，发射器返 `HierarchyEmitResult`）+ ADR-058 §2.3 签名与实现漂移 | 对外唯一载体 = `HierarchyEmitResult`；**发射器内 MUST 不依赖异常**（异常会落进 `print()` 的 `catch(...)` ⇒ 静默失败）；分析层抛 ⇒ 入口**转换**；各 ADR 的 `CHFATAL_EXCEPTION` 按本口径解释 | ADR-058 §2.3 ⑤ + 签名订正 |
| （不修 ADR） | 直接行 1 / 行 2 **缺宽度校验**（产物关掉 `WIDTHEXPAND`/`WIDTHTRUNC` ⇒ 静默扩展/截断） | v1.10 **W1 的通用措辞已覆盖** ⇒ 判定为**纯实现追赶** ⇒ 归 B3 前的小批（含负向用例） | tasks / B3 |

⚠️ **审稿**：oracle 1 轮 + critic 1 轮（arch 档）⇒ critic `APPROVE_WITH_CONDITIONS`（无 BLOCKER）。
oracle 的**唯一高影响发现**（连接生成的层位歧义）已按原句修进 G3-2；critic 的 SF-1（第三轴层位未明示）
已补 G3-6；SF-2（proposal Acceptance 的「抛」是旧载体措辞）登记为 **B3 的 tasks 待办**。
⇒ ⇒ 完整记录：`.rddf/reviews/ADR-060-v1.13-ADR-058-v2.12.md`。

### 11.11 T3-④ 归档后的全局复核（2026-11-05 · architect 角色）

> 触发：T3-④（层次发射）归档后，对「**已完成的部分**」与「**后续段**」做一次全局复核
> ⇒ ⇒ 目标链是否仍然成立 / 决策与实现是否仍然一致 / 下一段的第一块砖是否踩在实处。
> ⚠️ 本节只承载**复核结论 + 裁决指针**（推导在 ADR-058 v2.13 / ADR-060 v1.14 的版本史）。

#### 11.11.1 目标链复核（结论：**目标未变，链条已闭合到 T3-④**）

| # | 目标（为什么做） | 手段（怎么做） | 为什么这样做 | 当前态 |
|---|---|---|---|---|
| **N1** | 下游 ChipForge 的层 2 IP 单元 MUST 能成为**独立 Verilog module**（可单独复用 / 交付 / 综合）—— 手工 wrapper 的工作量随层次深度**线性增长** | `toVerilogHierarchical()` 独立入口 + `CH_HIERARCHY_LAYER("name")` 显式成层 | **默认 inline** ⇒ 「先写代码、零风险，将来要交付再加一行声明」⇒ 粒度是**用户的领域决策**，框架 MUST NOT 猜 | ✅ 发射侧已落地（T3-④）· ⏳ **等价性证据缺失**（T3-005） |
| **N2** | 下游装配点**可能没有 active parent** | 成层判定与装配点**解耦**（`ch_layer<T>` / `ch_inline<T>`） | 装配形态是下游的自由 ⇒ 不该被框架的 `ch_module` 约束 | ✅ T3-①c |
| **N3 / N4 / N7** | 单 ctx（防 node-id aliasing）· 顶层端口名不与内部节点同名 · 下游按 ctx 级名解析观测点 | 归属由**构造作用域**唯一决定 + 命名权威 = **ctx 级全局表** | 端口名带实例路径前缀 ⇒ 顶层名与扁平**逐字相同** ⇒ 下游按名解析不失效 | ✅ T3-①a/b |
| **N8** | 层 2 接口 MUST 是「结构化端口」 | `ch_uint` / `ch_bool` 白名单（`Payload<T>` **不可**作接口） | `Payload<T>` **不产生 port 节点** ⇒ 端口集合由实现细节偶然决定 ⇒ 不可交付 | ✅ T3-②/④ |
| **N5 / N6** | 节点名唯一性（opt-in）· 公共头 ODR 安全 | `context::node_name_policy`（默认宽松）· `inline` 自由函数 | 与层次化**无依赖** ⇒ 不该挂在主线前面 | ⏳ **T2b**（与主线并行）· N6 已由 T2a 归档 |

⇒ ⇒ **结论**：目标链**未变**，且 T3-④ 把「发射侧」整段闭合 ⇒ ⇒ **剩余唯一承重缺口 =
「等价性证据」**（T3-005）⇒ ⇒ 它是下一段的**第一块砖**，且它的**前置契约**在本节被发现**有洞**
⇒ ⇒ 见 §11.11.2。

#### 11.11.2 复核查出：六处漂移 + 一处契约缺口（⇒ ⇒ 全部已裁决并落档）

| # | 漂移 / 缺口 | 证据 | 裁决 | 落点 |
|---|---|---|---|---|
| **D1** | T3-005 的 `tasks.md` 1.4 要求 backend 支持**多文件** + 缓存 key 纳入全部 source ⇒ ⇒ **已被 ADR-058 v2.10 撤销**（单文件 ⇒ 成本不翻倍）⇒ ⇒ 照做 = 无背书的 backend 改造 | `openspec/changes/hierarchical-rtl-equivalence-2026-11/tasks.md:13` vs ADR-058 v2.10 | **删该任务**（v2.10 已封死多文件模式） | T3-005 的 tasks（**builder** 落地） |
| **D2** | T3-005 `tasks.md:3` 写「ADDED ×3」，实际 delta 只有 **2** 条 Requirement | 该 change 的 `specs/rtl-hierarchical-output/spec.md` + `proposal.md:58` | 计数订正为 **×2** | T3-005（builder） |
| **D3** | T3-005 三处写「`verilator --lint-only -Wall` 零 error」⇒ ⇒ 与 **ADR-060 §2.5**（唯一权威 · **不加** `-Wall`）冲突 | `tasks.md:34` / `proposal.md:50` / `design.md:11` | 改按 ADR-060 §2.5 执行（**ADR-061 §5 / ADR-062 §5 的同款残留已在本轮一并订正**） | T3-005（builder）· ADR-061 v1.6.5 · ADR-062 v1.5 |
| **D4** | **已归档主 spec** 三处与实现/已签批 ADR 冲突：(a) `:190` 的 `-Wall`；(b) `:78/:87/:144/:219` 的「MUST **抛异常**」（实现是 `ok=false`，且与**本 spec 自己的 Purpose** `:18-19` 互斥）；(c) `:126/:161` 的 `strict` 死引用（代码里无此策略） | `openspec/specs/rtl-hierarchical-output/spec.md` | 由 **T3-005 的 delta 以 MODIFIED 顺带修订**（同 capability 连续变更 ⇒ 不新开 change）⇒ ⇒ **MUST 任务化**（5 个 Requirement 标题 ⇒ 漏一处 = 复发）⇒ ⇒ **闭环判据见下（V1 · 可机械执行）** | T3-005（builder）· **architect 已把裁决写进 ADR-058 v2.13 / ADR-060 v1.14** |
| **D5**（**v2.16 订正**） | ADR-058 §2.3 授权的「只增不改入口」**不足以**支撑等价性：backend 的 `sim_main.cpp` 模板硬编码 `#include "Vtop.h"` + `new Vtop{…}` ⇒ ⇒ 而 Verilator 的**顶层类名 / 头文件名**由 **`--prefix` → `--top-module` → 首个输入文件名主干** 决定（`V3Options.cpp:1161-1165`，实测）⇒ ⇒ **约束对象 = 编译输入文件名 MUST 是 `top.v`**（**不是**顶层 module 名）⇒ ⇒ ⚠️ **v2.13 的前提已被实测推翻**：顶层 module 名 ≠ `top`（`my_top`）的产物**照常可编译可仿真**（Verilator 自选「未被实例化」的 module 为顶层）；**文件名**主干 ≠ `top`（`hier.v`）才 ⇒ `Vhier.h` 缺 ⇒ **编译期**失败 | `src/core/verilator_backend.cpp`（`kSimMainPrelude`）+ 实测 + `.rddf/reviews/hierarchical-rtl-equivalence-2026-11-discovery.md` | 入口 MUST 以 **`top.v`** 为编译输入（落地形态 = **复制**外部产物到 `<work_dir>/top.v`）⇒ ⇒ **MUST NOT** 落到编译期才发现；**顶层 module 名 MUST NOT 影响可消费性** ⇒ ⇒ 「backend 只消费默认名」**不再是已知边界**；**保留** `top_module_name`（已落地公开契约） | **ADR-058 v2.16 §2.3 + §5**（原 v2.13 §2.3 + 风险 7 已订正） |
| **D6** | `build_port_access_table` 从 **ctx 全量**推端口 ⇒ ⇒ 层次化顶层端口是**真子集** ⇒ ⇒ 生成的 `case` 引用**不存在的 Vtop 字段** ⇒ ⇒ 编译失败；而 ADR-058 §2.2① 的四元组映射表**无任何实现载体**（`port_provenance` 无 `node_id`） | `src/core/verilator_backend.cpp:594-680` · `include/hierarchy_verilog.h` | 入口 MUST 收**显式端口集合**（⊇ {clock, reset}）· 映射表 MUST 进 `HierarchyEmitResult` 新增**公开字段**（value-only）· `node_id` 有效性域 = 同一 ctx | **ADR-058 v2.13 §2.2 + §2.3 E1/E2/E3** |
| **（oracle 补）** | ① 双 backend 共享 `verilator_work_dir_` ⇒ 互踩；② `initialize_with_verilog` 若沿用 best-effort `CHWARN` ⇒ **backend 静默不可用** ⇒ 套件 SKIP = **恒绿假绿**；③ 端口集合漏 clock/reset ⇒ 寄存器永不更新 ⇒ **逐拍全等恒绿** | `:419,:436,:504,:530` · `:342-344,:368-370` | 全部写进 **ADR-058 v2.13 §2.3 E1/E2/E3**（含「失败 MUST 致命」+「harness MUST 断言真初始化」） | ADR-058 v2.13 |

⚠️ **V1 · D4 的闭环判据（v2.14 补 · 可机械执行 · 正反两面）**：

```bash
S=openspec/specs/rtl-hierarchical-output/spec.md
# 正面：断言面的 7 行必须清零（当前 = 7）
grep -cE -- '-Wall|`strict`|MUST \*\*(抛异常|拒绝)|MUST 改为抛异常' "$S"   # 归档时 MUST = 0
# 反面：合法的「MUST NOT 依赖抛异常」必须仍在（防「为清零而误删」）
grep -c '抛异常' "$S"                                                      # 归档时 MUST = 1（仅 :398）
```

⇒ ⇒ 正面当前命中 `:78 :87 :126 :144 :161 :190 :219` 七行 ⇒ ⇒ 逐行对应 MODIFIED 的 Requirement 标题；
⇒ ⇒ 反面是**反向判据**：`grep` 计数清零这个目标**可以**被「删掉整条禁令」达成 ⇒ ⇒ 第二条堵这一格。
⇒ ⇒ **两条 MUST 进 T3-005 的 tasks 末项（verify 块）** ⇒ ⇒ 不满足 ⇒ **不得归档**。

#### 11.11.3 一个**如实登记**的边界：等价性只覆盖「顶层可见行为」

层次化顶层输出集 = 扁平顶层输出集 − {`ch_out` 的最近成层祖先 ≠ 根}
⇒ ⇒ 被移出顶层的那批 `ch_out` 在层次化产物里**降级为子 module 端口**（或空连接 `.port()`）
⇒ ⇒ 顶层**观测不到** ⇒ ⇒ 「子树内部算错但顶层输出恰好相同」这一类缺陷
**结构性测不出**（lint 也看不见：`UNUSEDSIGNAL` 仅 `-Wall` 才报）⇒ ⇒
⚠️ **不是缺陷 ⇒ 是契约边界** ⇒ ⇒ **MUST 三处同步登记**（ADR-058 §4 风险 6 · capability spec · 用法指南）
⇒ ⇒ **MUST NOT** 假装全覆盖。
⇒ ⇒ ⚠️ **输入方向无盲区**（顶层 input 集合两侧 MUST 相等 ⇒ 激励可完整施加）。

#### 11.11.4 本轮未裁决、明确留待后续

| # | 事项 | 归属 |
|---|---|---|
| 1 | ~~「backend 支持任意顶层 module 名」~~ ⇒ ⇒ **v2.16 已消解**：实测顶层 module 名**不影响**可消费性（约束对象 = 编译输入文件名）⇒ ⇒ **不再作为残留**（ADR-058 v2.13 §4 风险 7 已删除） | ✅ 已消解（ADR-058 v2.16） |
| 2 | **C2**（下游是否拼接多份层次化产物）⇒ ⇒ 预裁 = 产物级 module 名前缀选项（默认空 · 纯增量）⇒ ⇒ 仍未答 | 下游确认（不阻塞主线） |
| 3 | **P6 / P8** 两个用户面产品缺口 | ✅ 已裁决独立立项（**P8** 独立立项 = A4 · 形态约束 = 公共 helper · 本期**不**在 `include/` 暴露）⇒ ⇒ **P8 的归位条件已达成**（T3-005 的 harness 已落地 · 两层形态）⇒ ⇒ **待 planner 立项**；**P6** 仍待 planner 立项 |
| 4 | 「端口命名契约」（顶层无框架前缀 · `__if_`/`__x_` 只在非顶层 module）的**静态护栏** | 独立判据（台账 §7 残留 5）—— **不在** T3-005 范围 |

⚠️ **审稿**：oracle 1 轮（对抗复核：六处漂移的成立性 + 五条裁决的失败模式 + 四条 architect 漏掉的边界）
⇒ ⇒ oracle 修正了 architect 的**一处机制描述**（Verilator 顶层类名的决定因素）⇒ 已按其实测改写。
⇒ ⇒ 完整记录：`.rddf/reviews/ADR-058-v2.13-ADR-060-v1.14.md`。
