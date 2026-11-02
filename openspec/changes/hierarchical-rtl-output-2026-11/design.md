# 设计：层次化 RTL 输出（`hierarchical-rtl-output-2026-11`）

> 本文件承载**推导与证据**（分析算法 · 发射流程 · 边界情形矩阵 · 验证策略）；
> **决策**在 ADR-058 / 059 / 060 / 061；**当前态与需求**在
> `docs/architecture/2026-11-02-hierarchical-rtl-output.md`。

---

## 1. 为什么先修归属（推导）

### 1.1 归属错误的三个可观测形态（实测）

| 形态 | 实测 | 根因 |
|---|---|---|
| 成员的 `ch_reg` 归**父**组件 | `top.m.r <- parent=top.m`（r 实属 Leaf） | 建节点时 `Component::current()` 还是父组件（成员构造早于 `build()`） |
| 成员 `ch_module` 挂**祖父** | `top.children = 4`，含两个同名 `top.leaf` | 同上 |
| 深度 ≥3 **前缀重复** | `top.top.m.lf.*` | `build_hierarchical_name()` 用 `hierarchical_name()`（后者又拼一次已是全路径的 `name_`） |

### 1.2 为什么「ctor 进 / build 出」是错的（推导）

| 路径 | `ctor 进 / build 出` 的后果 |
|---|---|
| `build()` 被调用两次（`built_` 早退，`src/component.cpp:99-103`） | 第二次会**多弹一帧** ⇒ 静默错归属 |
| `create_child` **从不 build**（`include/component.h:43-59`） | 该组件**活着期间**长期占据栈顶 ⇒ 后续**每一个**节点归属被污染 |
| `describe()` 期间（祖先尚未 build） | 栈**非空** ⇒ 「回落 `current_`」这条分支**不触发** ⇒ 归属取到构造中的祖先 ⇒ **恰好把要修的错重新引入** |

⇒ ⇒ **配对作用域**（ctor 进 / ctor 出）+ `build_internal` 另压一层 BUILD 作用域
⇒ ⇒ 四种形态全部正确，且**无泄漏**（⇒ 不需要析构兜底）。

### 1.3 为什么 `ch_module` 的 context 必须取环境值（推导）

`Component::ctx_` 在**构造函数里是 `nullptr`**（`src/component.cpp:13`），只在 `build()` 里赋值
⇒ ⇒ 成员形态 `ch_module` 的「新父组件」在构造期**没有 context**
⇒ ⇒ `include/module.h:49-53` 早退 ⇒ ⇒ **子组件直接消失** ⇒ ⇒ `io()` 时 `CHFATAL` ⇒ SIGSEGV。

⇒ ⇒ 今日之所以没炸，是因为 `Component::current()` 在那个窗口指向**祖父**（祖父已 build ⇒ 有 context）
⇒ ⇒ ⇒ **「挂祖父」这个缺陷恰好掩盖了「context 未就绪」** ⇒ ⇒ ⇒
⇒ ⇒ 改 parent 而不改 context 来源 ⇒ ⇒ **把「挂错层」升级成「整个子模块不存在」**（更严重且同样静默）。

⇒ ⇒ **修法**：`ch_module` 的 build context 取**环境 context**（`ch::core::ctx_curr_`）⇒ ⇒
- `describe()` 内创建时 = 父的 context（**与今天相同** ⇒ 零行为变化）；
- 成员形态时 = 环境 context（父的构造发生在祖父的 ctx 作用域内 ⇒ 仍是**同一个** context）
  ⇒ ⇒ **成员形态可用**，且**不需要延迟构造**（不改变节点创建顺序 ⇒ 爆炸半径最小）。

⇒ ⇒ **为什么不需要「延迟构造」**：延迟构造也能解决 context 问题，但它会**改节点创建顺序**
⇒ ⇒ 全部 Verilog 文本变 ⇒ ⇒ 「扁平产物逐字节不变」判据失效 ⇒ ⇒ ⇒ 故取环境 context 方案。

---

## 2. 层级粒度（ADR-061 的落地形态）

### 2.1 三种声明形态

```cpp
// ① 类级（组件作者声明「我是一个层」）
class Rv32iPipeline : public ch::Component { CH_HIERARCHY_LAYER; ... };

// ② 实例级覆盖（使用方按需指定）—— 两个新 API 名，**不动 ch_module 签名**
ch::ch_layer<Sub>  s{"s"};     // 显式成层
ch::ch_inline<Sub> t{"t"};     // 显式 inline

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

## 3. 分析层（伪码）

```
analysis(root):
  ctx  = root.context()
  all  = ctx->get_nodes()
  order= ctx->get_eval_list()                 # 拓扑序（regs 不穿透 next ⇒ 单独处理）

  # ① 成层判定（ADR-061）：类级默认 + 实例级覆盖 + emitter 侧 Granularity
  layered(c) = decl(c) 或 override(c) 或 granularity==ForceLayer
  (ForceInline ⇒ layered(c)=false for all c)

  # ② 组件树 → 模块清单（自底向上）
  walk(c):
      for ch in c.children(): walk(ch)
      if not layered(c): return                # inline ⇒ 不产生 module
      ports[c] = { n ∈ all | owner(n)==c ∧ n.type ∈ {input,output} }
      body[c]  = { n ∈ order | owner(n)==c ∧ n ∉ ports[c] }

  # ③ inline 的组件：其节点并入**最近的成层祖先**
  for c in tree: if not layered(c): owner'(n) = nearest layered ancestor of c

  # ④ 跨模块边 → 端口提升（ADR-060 的方向矩阵）
  for each edge (u→v):
      cu, cv = owner'(u), owner'(v)
      if cu == cv: continue
      if cv is cu 的祖先:      # 父→子
          if v ∈ ports[cv]: 例化映射
          else:              合成子侧 input 隐式端口（+CHWARN）
      elif cu is cv 的祖先:    # 子→父
          if u ∈ ports[cu]: 父层 wire 中转
          else:              合成子侧 output 隐式端口（+CHWARN）
      else:                   # 兄弟→兄弟
          父层 wire 中转（两端都必须是端口）
  # ⚠️ 第三格（子端口由父层表达式驱动）⇒ 图重写：把驱动搬进子模块

  # ⑤ 悬空输入冒泡（ADR-060）
  for each 子端口 p with driver(p)==nullptr: 逐级向上冒泡 ⇒ 顶层 input（名字 = ctx 级名）

  # ⑥ 跨模块 ch_mem ⇒ 响亮拒绝（ADR-060；⚠️ 按 **inline 折叠后** 的 module 归属判，
  #    否则被 inline wrapper 包住的 chlib::memory 会被误拒 —— oracle R16）

  # ⑦ 跨模块边的第三轴（ADR-060 v1.4）：driver 的 owner 在更上层（含 nullptr ⇒ 顶层）
  #    ⇒ 沿祖先链逐级合成 pass-through input 端口（否则父 module 引用不到顶层网）
  # ⑧ 前置校验（三条响亮拒绝，MUST 在分析**之前**跑 —— 诊断要指向根因）
  for n in all:
      ow = owner'(n)
      if ow != nullptr and not in_subtree(ow, root):
          fatal("组件 <ow> 未登记进组件树；请 top.add_child(std::move(unit))")   # B2
  for c in tree:                       # 已登记但从未 build ⇒ 有节点、无端口（ADR-059 D6③）
      # ⚠️ 判据是 `not built()` —— build() 先置 built_ 再调 create_ports()（src/component.cpp:99-140）
      # 若写成 `built() and not ports_created()` ⇒ 恒假 ⇒ 这条拒绝永不触发（critic R2 B1）
      if not c.built():
          fatal("未 build 的子组件 <path>：它没有端口，无法成为 module")
  for c in tree:                       # 成层组件自己驱动自己的 ch_in ⇒ 无定义（ADR-062）
      for p in c.ports(ch_in) where driver(p) in c.body:
          fatal("成层组件自驱动 ch_in <path>：子 module 内对 input 赋值非法，父层又引用不到")
  for n in all:                        # 归属空洞（ADR-058 §2.1）
      if owner'(n) == nullptr and n not in {default_clock, default_reset}:
          warn("节点 <name> 不属于任何组件 ⇒ 归顶层 module")                     # clock/reset 不告警
```

⚠️ **`owner == nullptr` 的节点**（context 级 clock/reset + 用户在 `main()` 里不经组件直接建的节点）
⇒ **已裁决**（ADR-058 §2.1 v2.3）：归**顶层 module** + `CHWARN` ⇒ **MUST NOT** 拒绝
⇒ ⇒ 分析层把它们放进根 `ModuleDef` 的 `body`（clock/reset 则进端口表）。
⚠️ 这条**不是**「兜底」而是**必要条件**：拒绝它们 ⇒ 任何含 `ch_reg` 的设计都不可层次化；
且扁平产物把它们作顶层端口发射 ⇒ 不归顶层就无法满足「默认全 inline 逐字节一致」。

---

## 4. 命名权威（推导）

下游按 **ctx 级节点名**解析观测点（`resolve_node_id(ctx, name)` 精确匹配 `node->name()`）
⇒ ⇒ 若层次化用 per-module 命名 ⇒ ⇒ 下游**解析不到** ⇒ ⇒ ⇒ 破下游的 parity 门禁。

⇒ ⇒ **命名权威 MUST 是 ctx 级全局表**（`verilogwriter::node_names_`）
⇒ ⇒ ⇒ 三个推论：
① 顶层端口名 MUST 与扁平产物**逐字相同**；
② **默认（全 inline）产物 MUST 与扁平逐字节一致**（最强的零成本判据）；
③ 下游 ADR-091 §G-1o「顶层端口名不得与内部节点同名」**天然满足**
（全局唯一化保证端口与内部节点不同名）⇒ ⇒ 层次化 MUST NOT 引入新的改名。

⇒ ⇒ **代价**：模块内信号名带**完整路径前缀**（`top_p_c_r`）⇒ ⇒ 名字偏长
⇒ ⇒ ⇒ 但换来「与扁平逐字一致」+「保留来源线索」⇒ ⇒ ⇒ **取此舍彼**。

---

## 5. 边界情形矩阵（实现时逐条要有判据）

| # | 情况 | 期望 |
|---|---|---|
| 1 | 子输入由父端口驱动 | 例化 port map 直连 |
| 2 | 子输入由父层表达式驱动 | 父层 wire + assign，再连 |
| 3 | 子输入由兄弟输出驱动 | 父层 wire 中转（兄弟间**不能**直连） |
| 4 | 子输入由常量驱动 | 直接写字面量 |
| 5 | 子输入悬空 | 冒泡成顶层 input + `CHWARN`（`strict` ⇒ 抛） |
| 6 | 同一输入被驱动两次 | 复刻 last-wins（只用 `driver()`） |
| 7 | 子输出无人消费 | 留在实例端口上（**不**冒泡）⇒ ⇒ 顶层端口集合比扁平**少** |
| 8 | 子输出 → 父输出 | 直连 |
| 9 | 3+ 层嵌套 | 递归，模块自底向上 |
| 10 | 同类型多实例 | **各发一份** module（名 `<type>_<序号>`）⇒ ⇒ ⚠️ 复用**不触发**（端口名含实例路径 ⇒ 签名恒不同；ADR-062 §2.6） |
| 11 | 同类型不同初值 | **不复用** ⇒ 各发一份（否则初值串味） |
| 12 | 同名不同类型 | **响亮拒绝**（MUST NOT 静默合并） |
| 13 | `ch_mem` 位于多层 | mem 与使用者**同层**；跨模块访问 ⇒ 响亮拒绝 |
| 14 | bundle 成员跨层 | 合成隐式端口（按方向）+ `CHWARN` |
| 15 | 父层直接改子层 `reg->next` | 合成隐式端口（子侧 input）；父子双写 = last-wins（行为未定义 ⇒ 文档登记） |
| 16 | 子端口由父层表达式驱动 | **图重写**（把驱动搬进子模块）⇒ 子输出**不是**恒 0 |
| 17 | 无子模块的叶子 | 普通单 module |
| 18 | 空 `describe()`（纯端口） | 空 body module |
| 19 | 顶层无 `__io`（只用成员接口） | 顶层 module 无端口（合法） |
| 20 | 端口宽度 > 64 | RTL 侧允许（64-bit cap 仅为 Verilator 侧约束）⇒ ⇒ **不可**进入等价性验证的 DUT 集合 |
| 21 | 含 `ch_mem` 但只用 `aread` | 两者都不需要。⚠️ **clock / reset MUST 分开判**：clock ⇒ `reg` ∨ 写端口 ∨ `sread`；reset ⇒ `reg` ∨ `sread`（⇒ mem 写端口的 `always_ff` 无 reset 分支，`codegen_verilog.cpp:1310`） |
| 22 | 被 inline 的组件含 `ch_reg` | 父 module 必须有 clock/reset 端口（时钟需求**向上传导**） |
| 23 | 同名实例 | 实例名去重 `u_x_2`（端口名/信号名同步去重） |
| 24 | 跨 module 组合环 | ctx 全局拓扑序仍能检出 ⇒ 但诊断 MUST 指明 module 边界 |
| 25 | **组件已 build 但未登记进组件树**（`make_unique`+`build(ctx)` 不 `add_child`） | **响亮拒绝** + `add_child` 修法（⇒ 否则整块逻辑静默消失，三条判据结构性失明） |
| 26 | **组件已登记但从未 build**（`create_child` 后不 build） | **响亮拒绝**（⇒ 有节点、无端口） |
| 27 | **inline 组件位于非顶层成层 module 内** | 其 `ch_out` 是**该 module 体内**的普通信号（不上 RTL 顶；该 module 的端口表由 ADR-060 决定） |
| 28 | **inline 组件位于 RTL 顶层** | 其 `ch_out` ⇒ **顶层 output**（⇒ 扁平规则 ⇒ 支撑「全 inline 逐字节一致」） |

---

## 6. 验证策略

| 层次 | 判据 |
|---|---|
| **最强 · 零成本** | **默认（全 inline）** 产物与扁平产物**逐字节一致** ⇒ **允许差异 = 空集**（`cmp` 退出码 0） |
| **语义（承重）** | 映射表声明的观测集合 `O`（**非空** + **至少一个随激励变化**）上 `flat[O] ≡ hier[O]` **逐拍全等**；且 `hier.top_ports ⊆ flat.top_ports` |
| **判别力自证** | 故意破坏层次化（删连线 / 改隐式端口方向）⇒ 等价性测试 MUST **转红** ⇒ 还原复绿 |
| **结构** | 实例端口名 ∈ 目标 module 端口集合；宽度一致；**零**未声明引用（`verilator --lint-only -Wall` 零 error） |
| **粒度** | 类级 × 实例级三组合的成层结果与声明一致；`ForceInline` vs `ForceLayer` 逐拍等价 |
| **归属** | 归属 ↔ 语义分流回归（`type_input` 分类 + `io.h` 四处守卫触发面**逐项一致**） |
| **确定性** | 同一设计两次生成**字节相同** |
| **零回归** | `ctest -LE perf` · `run_all_samples.sh` · `run_all_ported_tests.sh` 全绿 · 计数锚点同步 |

⚠️ **等价性测试的载体**：**backend 只增不改**的 `initialize_with_verilog(...)`
⇒ ⇒ 不另起「自建多文件 harness」。

⚠️⚠️ **但这个「只增不改入口」远不止加一个函数**（oracle R13）：
① `build_port_access_table` 从 **ctx 全量**推端口表，按 **node id** 取 `data_map_`、
按 **Vtop 字段名**取端口（`src/core/verilator_backend.cpp:594-660`）⇒ 层次化下子组件输出
**不再是顶层端口** ⇒ 生成的 `case` 引用不存在的字段；
② `emit_sim_main_postlude` 同样从 `ctx->get_eval_list()` 全量生成（`:193-231`）；
③ `invoke_verilator` 编译**单文件** `top.v`（`:490-497`），缓存 key 只哈希**一份** source。
⇒ ⇒ 新入口 MUST 接收**显式端口集合**（`node_id` + 顶层端口名 + 宽度 + 方向）
⇒ ⇒ ⇒ **工作量重估：~4 天 → ~7–9 天**（含多文件编译 + 缓存 key 改造）。

### 6.1 三条判据的**可执行形态**（critic 第 1 轮 SF 收口）

| 判据 | 可执行定义 | 反「自证」措施 |
|---|---|---|
| **「逐字节一致」= 允许差异** **空集** | `cmp <flat.v> <hier_forceinline.v>` 退出码 **0**（⇒ 全 inline 只产 1 个文件，其内容 MUST 与 `toVerilog()` 相同） | 不写「注释不算」—— ⇒ 任何差异即红；若实现**必须**产 banner ⇒ 先改 ADR |
| **结构同构** | 「RTL module 树」**独立提取**：**`verilator --json-only`**（`.tree.json` + `.tree.meta.json`，`bin/verilator:446`）或 `--dump-tree` ⇒ ⚠️ **`--xml-only` 已作废**（仓内 verilator **5.048** 已删除，`Changes:228`）⇒ 提取器形态以 §6.2 的 spike 结论为准 | **MUST NOT** 复用分析层的 `ModuleDef`（⇒ 否则分析错则两边一起错，「树形错但计数对」永远测不出） |
| **映射表 + 观测集合** | 映射表由 emitter 产出（**四元组**：`node_id` → 名字 → 扁平位置 → 层次化位置集合，含 `hier_top_port_name`）；`O` 由测试**手写声明**（≥1 个随激励变化的信号） | `O` 里若含恒 0 信号 ⇒ 测试 MUST 有「该信号确实变化过」的断言 |
| **端口集合** | ① **输入集合 MUST 相等**（名字+宽度逐项配对）；② **输出集合 MUST 等于精确期望集**（flat 输出集 − {`ch_out` 的**最近成层祖先 ≠ 根**} 的全部节点，**含成层子树里被 inline 的后代**） | ⚠️ **不用 `⊆`**（丢端口正是子集方向 ⇒ 漏发一半也能过） |

---

## 7. 风险与对策

| # | 风险 | 对策 |
|---|---|---|
| 1 | 归属修错方向 ⇒ 整棵子树归错父（**比现状更难定位**） | ①b 的判据（归属↔语义分流回归）+ 附录 A 探针入仓为断言 |
| 2 | `default_clock` / `default_reset` **硬编码** ⇒ 子模块 `always_ff` 引用不存在的信号 | ② 的 emitter 拆分 MUST **参数化时钟名**（⇒ 不是「纯搬移」） |
| 3 | 隐式端口方向搞反 ⇒ 错 RTL | 方向矩阵 4 行各一用例（边界 #14/#15/#16） |
| 4 | 「逐字节一致」被「这算注释差异」平凡满足 | **允许差异 = 空集**（`cmp` 退出码 0）⇒ 要放宽 MUST 先改 ADR |
| 5 | 用户把所有组件都声明成层 ⇒ 回到碎片化 | 诊断 API（路径图 P6）报告每层规模；**MUST NOT** 用启发式替用户裁剪 |
| 6 | 下游 ODR 缺陷未修 ⇒ 多 TU 项目链接失败 | 前置依赖：`component-framework-hardening-2026-10`（U-4） |

### 6.2 P0 之前的 **spike**（阻塞项 · oracle R1）

「结构同构」的 RTL 树提取 **`--xml-only` 在本仓不可用**（verilator **5.048** 已删除该选项，
`third_party/verilator/Changes:228`）⇒ ⇒ **P0 之前 MUST 先花半天验证**可用路径：

- 候选 ①：`verilator --json-only`（`bin/verilator:446`）⇒ `.tree.json` + `.tree.meta.json`
  ⇒ 用 `ptrNames` 把 `AstCell::m_modp` 解成 module 名（`src/V3Ast.cpp:1496-1505`）；
- 候选 ②：`--dump-tree`（会打 `-> modp()`）；
- **出口判据**：能机械产出「父子 module + 实例名 + module 名」三元组 ⇒ 判据成立；
  **提不出** ⇒ ⇒ 结构同构判据 MUST 重新设计（C3/C6 的排期与成本**都不成立**）。
- ⚠️ 输入 MUST 用**手写**的 3 层 SV（不依赖 emitter）⇒ 否则这个 spike 自己就成了「自证」。
