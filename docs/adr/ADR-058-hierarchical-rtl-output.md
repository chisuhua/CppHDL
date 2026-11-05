# ADR-058: 真层次化 RTL 输出 —— 语义边界与等价性

- Status: **Accepted**（2026-11-05 · 用户拍板 · **当前 v2.14** · 版本史见 §6）
- Date: 2026-11-02
- Author: build-agent（architect role）
- Parent: **无**（新增决策）；**前置** = ADR-059（节点归属与组件命名）·
  **配套** = ADR-061（层级粒度由用户显式控制）
- 关联 capability: `rtl-hierarchical-output`
- 关联 change: 已归档 `hierarchical-rtl-emission-2026-11`（T3-④）·
  在建 `hierarchical-rtl-equivalence-2026-11`（T3-005）· `hierarchical-rtl-docs-2026-11`（T3-006）
  （见 `.rddf/roadmap/features/feat-hierarchical-rtl-output.md`）
- 关联台账: `docs/architecture/2026-11-02-hierarchical-rtl-output.md`（详细规格）·
  `.rddf/roadmap/features/feat-hierarchical-rtl-output.md`（实施路径）
- 审稿报告: `.rddf/reviews/ADR-058-v2.13-ADR-060-v1.14.md`（oracle 1 + critic 1；
  其 2 SHOULD_FIX + 2 NIT 已在 **v2.14** 落实 ⇒ 决策内容零变更）
- ⚠️ **配套决策**：**ADR-060**（层次化输出的**端口策略**与 **module 复用**）——
  本 ADR 只承载「**边界是什么 / 等价性怎么定义 / API 边界在哪**」三件事。

---

## 1. 背景（**只陈述决策所需的事实**）

### 1.1 实测事实（2026-11-02，全部可复算）

| # | 事实 | 证据 |
|---|---|---|
| ① | 唯一的 RTL 出口是 `toVerilog(filename, context*)` ⇒ **没有 Component 参数**；固定输出 `module top (` → `endmodule` | `include/codegen_verilog.h:36` · `src/codegen_verilog.cpp:301, 409` |
| ② | writer 内**零** `children()` / `child_count()` 遍历，**零**实例化语句发射 | 全文件 grep 为空 |
| ③ | 层级只体现在信号名前缀（`.` → `_`）⇒ 且深度 ≥3 时**前缀重复** | `src/codegen_verilog.cpp:59-88` · `include/module.h:21` |
| ④ | **任意层**的 `ch_out` 都成为顶层 output 端口（只过滤 input）⇒ 端口表随层次变长 | `src/codegen_verilog.cpp:363-370` |
| ⑤ | 被驱动的子 `ch_in` **不是**顶层端口 ⇒ 降级为内部 `logic` + `assign` | `src/codegen_verilog.cpp:333-360` |
| ⑥ | 成员节点归属**父组件** · 成员 `ch_module` 挂**祖父** ⇒ 组件树与归属都失真 | ADR-059 §1.1 事实②③ |
| ⑦ | 跨 `ch_device` 连线：仿真**偶然能算**（`get_eval_list()` 跨 context DFS），但 `toVerilog` 产出**非法 Verilog**（Verilator 报 `Can't find definition of variable`） | `src/core/context.cpp:353-368` · `src/codegen_verilog.cpp:423-460, 512-520` |
| ⑧ | **Verilator backend 不调用 `toVerilog()`**：它直接构造 `verilogwriter(ctx)` 并写死单个 `top.v` + `module top` ⇒ ⇒ **框架里没有任何入口能把「外部生成的层次化 Verilog」喂进仿真器** | `src/core/verilator_backend.cpp:417, 416-489` |

### 1.2 问题（**精确表述**）

1. 「输出**层次化** Verilog」当前**零支持** ⇒ ⇒ 每次都要「手写 wrapper + 手写顶层例化」
   ⇒ ⇒ **手工量随层次深度线性增长**。
2. 若直接开写，会**先撞上归属缺陷**（⑥）⇒ ⇒ **emit 器写对了也会产出错 RTL**。
3. 需要先裁决三件事：
   ① **边界**：什么算一个 module？
   ② **等价性**：与现有扁平输出的关系如何定义、如何**机械判定**？
   ③ **API 边界**：新能力放在哪、以什么形态暴露、既有路径受不受影响？

---

## 2. Decision（**裁决**）

### 2.1 决策 ① —— 语义边界：**模块边界 = 所有权边界**

> 组件实例 `c` 对应 RTL 里的**一个 module 实例**（父 module 里的一条例化语句）；
> 它的 module **定义**（`module xxx (...) … endmodule` 那份）= `端口表(c)` ∪ `体内节点(c)`，
> 其中 `owner(n)` = 节点归属的组件实例（ADR-059 落地后 = 真实归属）。
> **子组件的端口与体内节点都不属于父组件的 module** ⇒ ⇒ ⇒
> **父 module 只通过「实例化 + 端口映射」与子 module 交互。**

⚠️ **「一个组件 = 一个 module」只对「被声明为成层的组件」成立**（**ADR-061**）：
层级粒度由**用户显式**声明，**默认 inline** ⇒ ⇒ ⇒
**被 inline 的组件不出现在 RTL 树里** ⇒ ⇒ 它的端口**完全按扁平规则**出现
（无 driver 的 `ch_in` ⇒ 顶层 `input`；任意 `ch_out` ⇒ 顶层 `output` —— ADR-061 决策②）。

⚠️ **module 定义数 = 成层组件实例数**（**每实例一份**）：端口名含**实例路径前缀**
⇒ ⇒ 端口签名恒不同 ⇒ ⇒ **复用不触发**（ADR-060 §5 已改判 · ADR-062 §2.6）
⇒ ⇒ ⇒ **MUST NOT** 假定「module 份数 ≤ 实例数」。

⚠️ **已裁决（2026-11-02）**：`owner(n) == nullptr` 的节点（`default_clock` /
`default_reset` 这类 **context 级**节点，以及用户在 `main()` 里**不经组件**直接建的节点）
⇒ ⇒ **归顶层 module**（并打 `CHWARN` 点名），**MUST NOT** 响亮拒绝。

⇒ ⇒ **理由（三条，任一条即足以否决「拒绝」）**：
1. **`default_clock` / `default_reset` 是 context 级基础设施** ⇒ ⇒ 拒绝它们 ⇒
   **任何含寄存器的设计**都无法层次化 ⇒ ⇒ 能力直接不可用；
2. **与扁平行为一致** ⇒ ⇒ 扁平产物里它们本就是顶层端口
   （`src/codegen_verilog.cpp` 的 `print_header` 无条件发射二者）⇒ ⇒
   若在层次化侧拒绝 ⇒ ⇒ 「默认全 inline 与扁平逐字节一致」这条最强判据**不可能成立**；
3. **它们在语义上确实是顶层胶合逻辑**（用户在 `main()` 里建的节点不在任何组件内
   ⇒ ⇒ 唯一合理的放置点就是根）。

⚠️ **`CHWARN` 的理由**：这类节点是「**归属空洞**」的**信号** —— 架构文档 §4 的
「归属 ↔ 语义」耦合要求这类节点**可被观测**（而不是静默并入）。
⇒ ⇒ 若将来某个节点**本应**属于某组件却落到顶层 ⇒ ⇒ 该 `CHWARN` 是**唯一的线索**。

⚠️ **`CHWARN` 的例外（v2.4 订正）**：`default_clock` / `default_reset` **不**告警 ——
它们是 context 级基础设施、**按设计**不归属任何组件（ADR-059 §2.3 由各模块自合成）
⇒ ⇒ 对它们告警会让**每一个**含寄存器的设计恒响 ⇒ ⇒ 告警变噪声。
⇒ ⇒ 该 `CHWARN` 只对「**用户在 `main()` 里直接建、且不在任何组件内**」的节点生效。

⚠️⚠️ **第二格：`owner(n) != nullptr` 但 `owner(n)` **不在** `subtree(top)` 里**
（v2.4 新增 —— critic BLOCKER B2）：
⇒ ⇒ **必然发生** —— 下游的装配形态是 `make_unique<Unit>()` + `build(ctx)`（**不** `add_child`）
⇒ ⇒ 该组件的 `parent_` 为空、**不**是 `top` 的子节点 ⇒ ⇒ ⇒ emitter 用 `children()`
**找不到它**。

⇒ ⇒ **裁决**：**响亮拒绝**（`CHFATAL_EXCEPTION`），错误信息 MUST 给出修法
（「把该单元登记为 `top` 的子组件：`top.add_child(std::move(unit))`」）。
⇒ ⇒ **MUST NOT** 静默把它当「顶层节点」并入 ⇒ ⇒ 否则**整块逻辑消失**，而三条判据都看不见：
① `hier.top_ports ⊆ flat.top_ports` 只会**更宽**（丢端口正是子集方向）；
② 观测集合 `O` 由测试作者声明 ⇒ 天然排除掉消失的信号；
③ 结构同构比的是「组件树 vs module 树」⇒ 而该组件**不在这两棵树里** ⇒ 两边一起少。
⇒ ⇒ ⇒ 这三条**结构性失明**是**拒绝**（而不是「尽力处理」）的唯一充分理由。

⚠️ **「登记」的三个合法来源**（都调用 `Component::add_child`）：`ch_device<T>`（根）·
`ch_module<T>`（describe 内）· 手工 `add_child`（**无 active parent 的装配点**用它）。
⇒ ⇒ ⇒ 这条把 ADR-061 决策⑥ 的「装配点任意」**精确化**为
「**装配点任意，但 MUST 登记进组件树**」（ADR-061 已同步）。

⚠️ **为什么是「所有权」而不是「图割」**：图割（输出端口反向可达集）也能切模块，
但它对**死逻辑**与**归属错误**都判不准 ⇒ 需要两套规则并存 ⇒ 不可预测。
⇒ ⇒ **归属必须先修（ADR-059）** ⇒ ⇒ **本裁决以「归属修好」为前提**。

⚠️ **附带的不变式（承重）**：
> **结构同构**：RTL module 树 ≡ 「**被声明为成层的**」组件子树
> （被 inline 的组件**不出现在** RTL 树里 —— ADR-061）。

⇒ ⇒ 这是「C++ 组件树 ⇄ RTL 模块树」同构的**唯一**可判定表述 ⇒ ⇒
⇒ **计数式判据（「3 个 module + 2 条实例化」）不足以证明它**（树形错但计数对也能过）。

### 2.2 决策 ② —— 等价性：**观测集合 + 映射表 + 非空洞**

> **等价性判据（三条同时成立）**：
> ① **映射表**：emitter MUST 产出一张「设计级信号名 ↔ 扁平端口名 ↔ 层次化 (module, port)」
>    的**三元映射表**（⇒ 两种命名体系不同 ⇒ 无映射则无法机械比对）；
> ② **观测集合 `O`**：对**显式声明**的观测集合 `O`（`O` ⊆ 层次化顶层输出集，
>    且 `O` ⊆ 扁平顶层输出集），`flat[O] ≡ hier[O]` **逐拍全等**；
>    ⇒ ⇒ `O` MUST **非空**，且 MUST **至少含一个随激励变化的信号**（⇒ 防「两边都恒 0」的假绿）；
> ③ **端口集合两条判据（v2.5 改写，据 oracle R2）**：
>    ① **输入端口集合 MUST 相等**（名字 + 宽度，逐项配对）——
>       扁平顶层 input = 无 driver 的 `ch_in` + clock + reset；层次化顶层 input =
>       各层未驱动 `ch_in` **冒泡**（复用 ctx 名）+ clock + reset
>       ⇒ ⇒ **两侧应当完全相等**，不是「包含」；
>    ② **输出端口集合 MUST 等于「精确期望集」** = 扁平输出集 − {**`ch_out` 的最近成层
>       祖先 ≠ 根** 的全部节点} ⇒ ⇒ MUST 逐项枚举后比对（**MUST NOT** 只写 `⊆`）。
>
> ⚠️ **扣减必须按「最近成层祖先」算，不能只减「成层子组件的端口」**（critic 第 3 轮 B3）：
> **成层子树里被 inline 的后代**的 `ch_out` 同样是体内信号、同样不上 hier 顶层
> ⇒ ⇒ 漏减 ⇒ **正确实现必红**。实测可达：`include/cpu/pipeline/rv32i_pipeline.h:123`
> 的 `ch_module<chlib::ForwardingMux<32>>` 未声明成层 ⇒ inline，而
> `include/chlib/forwarding.h:94-95,215-216` 有 **4** 个 `ch_out` ⇒ ⇒ 一旦把
> `Rv32iPipeline` 声明成层，期望集就会多出 4 个端口。

⚠️ **为什么必须改写「全部顶层输出逐拍全等」**：扁平产物把**任意层**的 `ch_out` 抬成顶层
output（事实④）⇒ ⇒ 层次化的顶层输出集合是扁平的**子集**（**全 inline 时相等**，
有至少一个成层子树时**真子集**）⇒ ⇒ 「全部顶层输出全等」

⚠️ **「`⊆`」单独**不是**有效判据**（oracle R2）：**丢端口正是子集方向** ⇒ ⇒ 漏发一半端口
也能满足 `⊆`。故 ③ 拆成「输入**相等** + 输出**等于精确期望集**」两条。
要么**不可比**（集合不同），要么**退化为只比交集**（判据空洞）。

⚠️ **为什么 MUST 有映射表**（v2.4 订正）：命名**权威**两侧相同（ADR-061 决策④ 的 ctx 级
全局表）⇒ ⇒ 名字**不需要**翻译；需要映射的是「**一个信号在层次化产物里的位置集合**」
（扁平 1 处 ⇒ 层次化可能多处：实例端口连接 + 子 module 端口 + 体内线网）
⇒ ⇒ 否则等价性测试无法确定「扁平的那个端口该读哪个 `(module, port)`」。
⇒ ⇒ 映射表 MUST 是**四元组**：「**`node_id`** → 设计级信号名 → 扁平位置 →
**层次化位置集合（含 `hier_top_port_name`）**」—— **`node_id` 与 `hier_top_port_name`
是承重列**（oracle R13）：Verilator backend 的 `build_port_access_table` 按 **node id**
取 `data_map_` 槽位、按 **Vtop 字段名**取端口（`src/core/verilator_backend.cpp:594-660`）
⇒ ⇒ 只有名字对照表它**用不了**。

⚠️ **映射表的载体（v2.13 · 承重）**：映射表 MUST 由 **emitter 产出**（不得由测试自建
⇒ ⇒ 自建表与 emitter 真相脱钩 ⇒ ⇒ 表错则两错相抵 ⇒ 正是本仓「parity 盖章同构缺陷」的翻版）
⇒ ⇒ 载体 = **`HierarchyEmitResult` 的新增公开字段**（value-only：`uint32_t node_id` +
`std::string`；**MUST NOT** 带 `lnodeimpl*` ⇒ 同 `port_provenance` 的 value-only 纪律）
⇒ ⇒ **不另开 `toVerilogHierarchicalManifest()` 入口**（两次调用之间存在非确定性窗口
⇒ ⇒ 违背「映射表由 emitter 产出」⇒ ⇒ 第二个调用点必然漂移）。
⇒ ⇒ **同一份产物 = 同一个真相源** ⇒ ⇒ §2.3 **E1 的显式端口集合 MUST 同样由它派生**
（映射表 ∪ `port_provenance`）⇒ ⇒ **MUST NOT** 由 harness / backend 独立撰写或手工列举
（独立撰写 ⇒ ⇒ 与 emitter 真相脱钩 ⇒ ⇒ 表错则两错相抵 ⇒ ⇒ 与本条同一条纪律）。
⚠️ **schema MUST 允许「该信号不在层次化顶层」**：此时 `hier_top_port_name` 允许为空
⇒ ⇒ 「层次化位置集合」= 该信号在层次化产物里出现的全部位置（含**子 module 端口**与
**体内线网**）⇒ ⇒ 否则「被移出顶层的那批 `ch_out`」在表里无处落账 ⇒ ⇒ §4 风险 6 的
盲区**没有机械载体**。

⚠️ **`node_id` 的有效性域（v2.13 · 承重）**：`node_id` 是 **ctx 私有**的自增编号
⇒ ⇒ **两个独立 build 的 ctx 之间无可比性契约**（同一设计 build 两次 ⇒ id 分配受建图顺序
影响 ⇒ 相同纯属巧合）⇒ ⇒ 「同一 `node_id`」这个前提**只在「同一个 ctx 上做两次 emit」时成立**
⇒ ⇒ 即：扁平产物与层次化产物 MUST 由**同一个 `ch_device`（同一 ctx）**产生
⇒ ⇒ 跨 ctx 比对 MUST 退化到「设计级信号名」维度（命名权威两侧相同 ⇒ 名字可比
⇒ ⇒ ADR-061 决策④）⇒ ⇒ 等价性 harness MUST 显式断言「两次 emit 共享同一 ctx」。

⚠️ **判别力自证（必须）**：故意破坏层次化产物（删一条连线 / 改一个隐式端口方向）⇒ ⇒
等价性测试 MUST **转红** ⇒ ⇒ （仓内既有惯例：mutation + self-test）。

### 2.3 决策 ③ —— API 边界：**独立入口 + 既有路径逐字节不变**

> 新增**独立函数** `toVerilogHierarchical()`；
> `toVerilog()` 的**行为**与**文本** MUST **逐字节不变**；
> Verilator backend / Simulator / JIT **既有入口**一律不动。

```cpp
// ⚠️ 选项结构的**唯一权威定义**在 ADR-061 §2.5（含 port_policy / undriven_policy /
//    top_module_name）。本 ADR 不重复列出全部字段（⇒ 三份副本必然漂移 —— critic NIT）。
// ⚠️ 返回类型不是 void（oracle R14）：三条「响亮拒绝」必须能被调用方观测。
// ⚠️ v2.12 订正：签名按**已落地形态**写（原文 `std::expected<HierManifest, HierError>`
//    与 `VerilogHierOptions` 与实现漂移 ⇒ 按实现订正，**语义契约不变**）。
struct HierarchyEmitOptions {
    PortPolicy port_policy;            // 默认 SynthesizeWarn
    UndrivenPolicy undriven_policy;    // 默认 Bubble
    std::string top_module_name = "top";
};
struct HierarchyEmitResult {
    bool ok;
    std::string error;                 // ok == false 时非空
    std::vector<std::string> files;    // ok == true 时恰一个
    std::vector<std::string> warnings;
    // ⚠️ v2.14：G2 端口清单（§2.3 的「G2 端口清单字段」段）与 §2.2 的**四元组映射表**
    //    **两个字段并存**（前者不含 `node_id` ⇒ 不能顶替后者）
    std::vector<PortProvenance> port_provenance;
    std::vector<SignalMapEntry> signal_map;
};
HierarchyEmitResult
toVerilogHierarchical(const std::string& path, ch::Component& top,
                      const HierarchyEmitOptions& opts = {});
```

⚠️ **G2 端口清单字段**（v2.14 订正：**已落地** · `include/hierarchy_verilog.h:48-69`）：
`HierarchyEmitResult::port_provenance`（`std::vector<PortProvenance>` · **value-only** ·
覆盖**全部** module 的端口，含合成端口与**注入的 clock/reset** ⇒ ⇒ 顺序 = module 发射序 →
端口发射序 ⇒ **确定性**）
⇒ ⇒ 用途 = 端口完整性校验（`expected ⊆ actual`）与端口清单
⇒ ⇒ 它**不改变**本节的语义契约（失败可观测 · 诊断可承载 · 单文件产物）。
⚠️ **它不含 `node_id`** ⇒ ⇒ §2.2 的四元组映射表**不能**由它顶替 ⇒ ⇒ `node_id` 列 MUST 由
§2.2 要求的新增字段承担（§2.3 E1 消费）—— 两者**并存**，不是同一件东西。

⚠️ **§2.2 的四元组映射表字段**（**已落地** · `include/hierarchy_verilog.h` ·
`SignalMapEntry` / `SignalLocation` · **value-only**）：
`HierarchyEmitResult::signal_map` = 「`node_id` → 设计级信号名 → **扁平位置** →
**层次化位置集合**」⇒ ⇒ `hier_top_port_name` **允许为空**（= 该信号不在层次化顶层）
⇒ ⇒ 载体与 `port_provenance` **并存**（前者带 `node_id` · 后者不带）。
⚠️ **发射器自校验**：表 MUST 在**第一次 I/O 之前**通过 `check_signal_map`
（`src/hierarchy_signal_map_check.h`）—— 顶层端口全覆盖 · `flat.is_port` ⇔ 顶层端口名非空 ·
「声称在顶层 ⇒ MUST 真的有那个位置」· 「顶层端口名为空 ⇒ MUST 真的不在顶层」
⇒ ⇒ 不满足 ⇒ `ok=false` + 点名 ⇒ **不产出文件**（fail-closed）。

⚠️ **错误模型（oracle R14，承重）**：
① **所有校验 MUST 在第一次 I/O 之前完成** ⇒ ⇒ 否则留下**半套产物**（`cmp` / lint 会读到它）；
② 写出 MUST 走「**临时文件 → 成功后 rename**」⇒ ⇒ 失败**不留下**任何新文件，
   且失败路径 MUST NOT 触碰既有同名文件（rename 只在成功后发生 ⇒ ⇒ 旧产物不被破坏）；
③ 校验 MUST NOT 落在任何 `catch(...)` 作用域内 —— 现状 `print()` header/body/footer 各自
`catch(...)`（`src/codegen_verilog.cpp:389-410`）、`toVerilog` 自己也 catch（`:39-45`）
⇒ ⇒ **三条 `CHFATAL_EXCEPTION` 会被静默吞掉**；
④ `CHFATAL_EXCEPTION` 在 static destruction 期**不抛**（`include/utils/logger.h:235-244`）
⇒ ⇒ 该路径 MUST 另有返回码兜底（`HierError`）。

⑤ **错误载体统一口径（v2.12 · 承重）**：对外**唯一**载体 = `HierarchyEmitResult`
（`ok=false` + `error` 点名；`files` 为空）。
- **发射器内**的校验失败（端口策略 / 跨层宽度 / 接口白名单 / 端口完整性 / module 命名冲突）
  MUST 走**错误结果** ⇒ MUST NOT 依赖异常 —— 异常会落进 `print()` 的 `catch(...)`（③）
  ⇒ ⇒ 变成「静默失败 + 看起来成功」；
- **分析层前置校验**（跨模块 `ch_mem` · 成层组件自驱动 `ch_in` · 已 build 未 `add_child`）
  允许 `CHFATAL_EXCEPTION` ⇒ 但入口 MUST 在**第一次 I/O 之前**用
  `catch (const std::exception&)` **转换**成 `error` ⇒ ⇒ 这是**转换**，不是吞掉
  （对比 `toVerilog()` 的 `catch(...)` 静默）；
- **各 ADR 中出现的 `CHFATAL_EXCEPTION` 字样**（ADR-060 §2.1 / §2.2 / §2.4.2、ADR-062 §2.6 等）
  MUST 按本口径解释为「**响亮拒绝**」，可观测形态由**所在层**决定：
  发射器路径 ⇒ 错误结果 + 点名；分析层前置校验 ⇒ 抛 + 入口转换
  ⇒ ⇒ **MUST NOT** 为同一判据引入第二套错误通道。

⚠️ **受限例外（为使等价性可执行）**：允许给 `VerilatorBackend` 增加一个**只增不改**的入口
`initialize_with_verilog(path, data_map)`（既有 `initialize(ctx, …)` 行为**逐字节不变**）
⇒ ⇒ 理由：事实⑧ ⇒ 框架里**没有任何**入口能仿真「外部生成的层次化 Verilog」
⇒ ⇒ 若不允许该例外，等价性测试必须**自建**一套「**外部 RTL 编译 + 双模型对拍**」harness
⇒ ⇒ ⇒ 成本远超预估 ⇒ ⇒ ⇒ **该例外是 P4 可执行性的前提**。
⇒ ⇒ ✅ **已裁决（2026-11-02 · 用户拍板）**：**允许**该只增不改入口。

⚠️ **该入口的三条硬约束（v2.13 · 据 T3-④ 归档后的实施期实测补齐）** ——
上面的裁决给了「允许加口」，但**未给**「口长什么样」⇒ ⇒ 实施者只能即兴 ⇒ ⇒ 写死：

| # | 约束 | 为什么（**实测**） |
|---|---|---|
| **E1** | 该入口 MUST 接收**显式端口集合**（`node_id` + 顶层端口名 + 宽度 + 方向）⇒ ⇒ MUST **⊇ {clock, reset}**；`build_port_access_table` / `emit_sim_main_postlude` MUST 走该集合，**MUST NOT** 再从 ctx 全量推 ⇒ ⇒ 且该集合 MUST 由 **emitter 产物**（§2.2 映射表 ∪ `port_provenance`）**派生** —— **MUST NOT** 由 harness / backend 独立撰写 / 手工列举（独立撰写 ⇒ 与 emitter 真相脱钩 ⇒ 表错则两错相抵） | 层次化产物的顶层端口集合是 ctx 全量的**真子集**（成层子树的 `ch_out` 留在子 module 端口上）⇒ ⇒ 从 ctx 全量生成的 `case` 会引用**不存在的 Vtop 字段** ⇒ ⇒ 编译/链接失败（`src/core/verilator_backend.cpp:594-680`）⇒ ⇒ 且集合漏 clock/reset ⇒ `pulse_reset()` 与 `clock_field_ptr_` 空转 ⇒ ⇒ 寄存器永不更新 ⇒ ⇒ **逐拍全等恒绿**（最危险的假绿） |
| **E2** | 该入口 MUST **物理隔离**于既有 `initialize(ctx, …)` 路径（后者行为**逐字节不变**）⇒ ⇒ 等价性 harness MUST 用**两个独立 work_dir** | 两条路径共享 `verilator_work_dir_` ⇒ ⇒ `top.v` / `sim_main.cpp` / `obj_dir` 互踩（`:419,:436,:504,:530`）⇒ ⇒ 任何「重新 initialize」的顺序假设都会错 |
| **E3** | 该入口的**失败语义 MUST 是致命错误**（返回 `VerilatorError`）⇒ ⇒ **MUST NOT** 沿用 `try_initialize` 的 best-effort 惯例（编译失败仅 `CHWARN` 后继续 · `:342-344`；dlopen 失败仅 `CHWARN` 后继续 · `:368-370`）⇒ ⇒ harness MUST 断言 backend **真初始化成功**（非 SKIP 即过） | best-effort ⇒ backend **静默不可用** ⇒ ⇒ 等价性套件若按 dual-backend fixture 惯例把「verilator 不可用」当 SKIP ⇒ ⇒ **整个套件静默跳过 = 恒绿假绿**（本仓反复出现的失败模式） |

⚠️ **顶层 module 名的约束（v2.13 · 实测订正措辞）**：backend 生成的 `sim_main.cpp`
模板硬编码 `#include "Vtop.h"` 与 `new Vtop{…}`（`src/core/verilator_backend.cpp:446,:456`）
⇒ ⇒ 而 Verilator 生成的顶层类名由 **`--top-module` 或输入文件名**决定
（`third_party/verilator/src/V3Options.cpp:1161-1165`；实测：`--prefix` 亦可覆盖）
⇒ ⇒ 层次化产物若**顶层 module 名 ≠ `top`** ⇒ ⇒ **编译期**失败（`VerilatorCompile`），
不是运行期 ⇒ ⇒ 该入口 MUST **显式校验**顶层名并在不符时**致命拒绝 + 点名**
⇒ ⇒ 把编译期失败**提前到入口**（fail-early）。
⚠️ **为什么保留 `top_module_name` 可配置**（不删选项）：它已是**已落地公开契约**
（capability `rtl-hierarchical-output` 的 Scenario「顶层 module 名可配置」）⇒ ⇒ 删 = breaking
⇒ ⇒ 本期裁决 = **保留选项 + 约束消费路径**（backend 只消费默认名 `top` 的产物）⇒ ⇒
「backend 消费非默认名产物」记为**已知边界**（本期不做，见 §4 风险 7）。

⚠️ **为什么不给 `toVerilog()` 加参数**：既有调用点与 Verilator backend 全部承担新分支风险
⇒ ⇒ 且「默认扁平」会退化成**隐式行为**，回归时无法区分「走扁平」与「走层次化」。

⚠️ **产物形态 = 单文件多 module（v2.9 裁决，取代此前的「目录模式」措辞）**：
层次化产物是**一个文件**、内含**多个 module**（子在前、根在后）⇒ ⇒
`verilator --top-module <root_module>` 可直接吃 ⇒ ⇒ **不实现**「一 module 一文件」
（`out_dir` / `one_file_per_module` 两个选项一并**撤销** ⇒ 见 ADR-061 v1.6.2）。
⇒ ⇒ **为什么撤销目录模式（逐条理由）**：① 下游需求 **N1**（层 2 IP 单元可单独复用 / 交付 / 综合）
由「**module 独立**」满足 ⇒ ⇒ **不要求文件独立** ⇒ ⇒ 目录模式**无承重需求**
⇒ ⇒ ⚠️ **本前提已闭合（v2.10 · 2026-11-05 · 用户拍板）**：下游交付管线**接受「单文件多 module」**
（⇒ 台账 `docs/architecture/2026-11-02-hierarchical-rtl-output.md` §11.6 **C1** ⇒ ✅ 已答）
⇒ ⇒ ⇒ **本裁决不再是条件条款** ⇒ ⇒ 「多文件模式」**不重开**（⇒ backend 的文件列表 +
缓存 key 改造 **不再需要** ⇒ ⇒ T3-⑤ 成本**不翻倍**）。
⚠️ **为什么将来也不必重开（封住「条款仍可被重开」的唯一解读空间）**：单文件多 module 的
Verilog 里，**文件级切分是纯下游工具链动作** —— 按 module 边界把一个文件切成若干 `.sv`
（Verilog 语义合法，module 边界本就是合法切分点）⇒ ⇒ **上游格式与 API 零改动**
⇒ ⇒ ⇒ **文件级切分需求不构成本 ADR 的重开条件**。
⚠️ **该论证已有实测背书**（2026-11-05）：把三层层次化产物**按 module 边界**切成 3 个 `.sv`
⇒ ⇒ `verilator --lint-only --top-module top l3_unit.sv m3_unit.sv top.sv` **exit 0**
（无 Error / 无 Warning）⇒ ⇒ ⇒ 「module 边界是合法切分点」是**实测事实**，不是论证。
② 等价性（T3-⑤）的 Verilator backend 编译**单文件**（`invoke_verilator` 只哈希一份 source）
⇒ ⇒ 多文件要**再改一次**backend 的编译与缓存 key ⇒ ⇒ 成本**翻倍**；
③ Zero-Debt ⇒ ⇒ **MUST NOT** 留下「有字段但不实现」的选项；
④ 错误模型随之简化（**一个**临时文件 → rename ⇒ ⇒ 原子性天然成立）。

⚠️ **同名不同物的消解**：层次化产物的顶层 module 默认名仍为 `top`（⇒ ⇒ 便于与扁平产物 A/B 比对）
⇒ ⇒ 与扁平产物的 `top.v` **同名不同物** ⇒ ⇒ 消解手段 = **文件名由调用方显式指定**（入口收 `path`）
⇒ ⇒ **约束**：层次化产物的路径 MUST NOT 与扁平产物同名（⇒ 由用法指南 + 护栏守）。
⚠️ **API 侧没有自卫**（实测）：`toVerilogHierarchical("top.v", …)` 会经 rename **静默覆盖**
既有的扁平基线文件 ⇒ ⇒ ⇒ 该约束**只能**由「护栏 + 用法指南」承担 ⇒ ⇒ **护栏 MUST 包含**
「层次化产物路径 ≠ 既有扁平产物路径」这条检查（⇒ 归 T3-④ 的 `verify_hierarchical_rtl.sh`）。

### 2.4 边界情形与端口策略 → **ADR-060**

「跨模块非端口边 ⇒ 隐式端口」「悬空输入 ⇒ 冒泡」「module 复用判据」
这三项各有独立选项空间 ⇒ ⇒ ⇒ 按「一文档一决策」拆到 **ADR-060**。

### 2.5 前置裁决

> 本 ADR 的实施 **MUST 以 ADR-059 落地为前置** ⇒ ⇒ ⇒
> **归属未修之前不得开工 emitter**（否则产出错 RTL 且难以定位）。

---

## 3. 选项对比（**逐条含理由**）

| # | 选项 | 结论 | 理由 |
|---|---|---|---|
| 1 | **新立 ADR-058 + 独立新 API `toVerilogHierarchical()`**（本文） | ✅ **采纳** | ① 「层次化输出」是**独立决策点** ⇒ 独立成篇；② 新入口 ⇒ 既有调用点与 Verilator backend **零风险**；③ 等价性被写成**机械可判定**的三条判据（§2.2）⇒ ⇒ 不会退回「看起来像层次化」 |
| 2 | **改 `toVerilog()` 增加 `hierarchical` 开关** | ❌ 否决 | ① 既有调用点 + backend 全部承担新分支风险；② 扁平与层次化**共享同一入口** ⇒ 「默认扁平」退化为隐式行为 ⇒ ⇒ 回归时无法区分两条路径；③ 与 ADR-035 的 64-bit port cap、Verilator 缓存 key 等既有约束纠缠 |
| 3 | **在扁平产物上做后处理切分**（按信号名前缀） | ❌ 否决 | ① 成员节点名字挂父前缀 + 深度≥3 前缀重复 ⇒ **切分边界本来就错**；② 后处理**无法还原端口的方向与宽度** ⇒ ⇒ 只能靠名字猜 ⇒ ⇒ 不可行；③ 产物不可复算（同输入不同产物） |
| 4 | **每层一个 `ch_device` + 跨 device 连线** | ❌ 否决 | ① 跨 context 连线**不受支持**：仿真偶然能算但 `toVerilog` 产出**非法 Verilog**（事实⑦）；② 两个 context 各有 `default_clock`/`default_reset` ⇒ 时钟语义分裂 |
| 5 | **每层一个 `ch_device` 单独出文件 + 手写顶层例化** | 🟡 **仅作过渡手段** | ① 零框架改动 ⇒ 短期可用；② 但 `ch_device<Sub>` 把整棵子树 build 进同一 ctx ⇒ 子模块体内含孙逻辑 ⇒ **不能**与层次化并存；③ 对「成员 `ch_module`」的组件直接崩；④ **归位条件**：层次化 API 落地 ⇒ **本条作废**（否则它会像 `fwft_fifo` 一样长期以「临时手段」身份留在文档里） |
| 6 | **用「图割」定义模块边界** | ❌ 否决 | ① 对**死逻辑**与**归属错误**都判不准 ⇒ 需两套规则并存 ⇒ 不可预测；② 归属修好后所有权规则**更简单且全覆盖** ⇒ ⇒ 图割是「缺陷未修时的权宜」，不是终态 |
| 7 | **错误载体：两套并存**（分析层抛异常 + 发射器返结构体） | ❌ 否决（v2.12 · §2.3 ⑤） | ① ADR-062 §2.6 字面写 `CHFATAL_EXCEPTION`、发射器却返 `HierarchyEmitResult` ⇒ ⇒ 实施者被迫二选一 ⇒ ⇒ 要么违反字面、要么让异常落进 `print()` 的 `catch(...)` ⇒ ⇒ **静默失败 + 看起来成功**；② 统一为「对外 = 错误结果」后，各层的**可观测形态**由所在层决定（分析层抛 ⇒ 入口转换）⇒ 只有一条通道 |
| 8 | **统一为异常**（`toVerilogHierarchical` 抛 `HierError`） | ❌ 否决（v2.12） | ① 与 §2.3 ③ 的「校验 MUST NOT 落在 `catch(...)` 内」张力最大（既有 writer 三处 `catch(...)`）；② 静态析构期 `CHFATAL_EXCEPTION` **不抛** ⇒ 必须有返回码兜底 ⇒ ⇒ 等于又要两套；③ 调用方（含测试）无法用返回值表达「失败但不中断」 |

---

## 4. 后果 / 代价

### 正面
- 「输出层次化 Verilog」从**手工活**变成**框架能力** ⇒ 手工量与层次深度解耦；
- **顺带消除**扁平输出的端口膨胀（任意层 `ch_out` 都进顶层端口表）⇒ 顶层接口更干净；
- 等价性判据（§2.2）把「层次化没改语义」变成**机械可判定**的属性 ⇒ 不会静默分叉；
- **结构同构**不变式（§2.1）⇒ 「C++ 组件树 ⇄ RTL 模块树」同构第一次有了承重表述；
- 与既有扁平路径**完全解耦** ⇒ 回归风险仅来自 ADR-059 自身。

### 负面
- 需要**两套** RTL 产物语义（扁平 + 层次化）⇒ 认知负担；
- **映射表**是新产物 ⇒ 需要新的测试基础设施；
- 层次化路径**零**现有测试覆盖 ⇒ 必须新建等价性测试（成本集中在 P4）；
- `initialize_with_verilog` 是**给 backend 加口** ⇒ 虽「只增不改」，仍触及 backend。

### 风险

| # | 风险 | 对策 |
|---|---|---|
| 1 | **归属缺陷未修就开工** ⇒ 产出错 RTL 且难定位 | ADR-059 作为**硬前置**（§2.5）⇒ 实施路径上独立成 change |
| 2 | 等价性判据**空洞**（`O` 可声明为空 / 全是恒 0 信号） | §2.2 的三条判据（映射表 + 非空 + 至少一个变化信号）+ **判别力自证**（故意破坏 ⇒ 必须转红） |
| 3 | 层次化产物**不被 Verilator backend 消费** ⇒ 框架的「双后端 parity」能力对它**不可用** | §2.3 的**只增不改**例外；并在「已知残留」里登记「扁平仍是 backend 的唯一输入 ⇒ 两套真值长期存在」 |
| 4 | 顶层端口集合**比扁平少** ⇒ 外部工具看到的接口不同 | 等价性以**显式观测集合**为准；并产出**端口清单**供外部评审 |
| 5 | **`CH_MODULE_NAME` 忘写** ⇒ 模块类型名不可确定 ⇒ ⇒ 而 `typeid` 的拼写**是实现相关的**（Itanium vs MSVC）⇒ ⇒ 跨编译器/跨平台**不可复算** | ⚠️ **以 ADR-059 D4 v2.3 + §2.2.1 的四档来源**为准：① 类级 `CH_HIERARCHY_LAYER("name")` / ② `ch_layer<T>` 继承 `T` 的类级名 / ③ `ch_layer<Helper>` 在实例处给名 ⇒ ⇒ **①②③ 缺名编译期即不可表达**（⇒ ⇒ 原「运行期硬错」的兜底**不再需要**）⇒ ⇒ ④ `Granularity::ForceLayer` 强制的组件（用户**未**声明）⇒ ⇒ 允许回退 `sanitize(typeid)` **+ MUST 有 `CHWARN`**（⇒ ⇒ 名字仅「同 ABI 内」可复算） |
| 6 | **等价性判据只覆盖「顶层可见行为」** —— 层次化顶层输出集 = 扁平顶层输出集 − {`ch_out` 的最近成层祖先 ≠ 根} ⇒ ⇒ 被移出顶层的那批 `ch_out` 在层次化产物里**降级为子 module 端口**（或空连接 `.port()`）⇒ ⇒ 顶层**观测不到** ⇒ ⇒ 「子树内部算错但顶层输出恰好相同」这一类缺陷**结构性测不出** | ⚠️ **如实登记**（不是缺陷 ⇒ 是契约边界）⇒ ⇒ **输入方向无盲区**（顶层 input 集合两侧 MUST 相等 ⇒ 激励可完整施加）⇒ ⇒ MUST 在 **capability spec + 用法指南** 同步登记 ⇒ ⇒ **MUST NOT** 假装全覆盖 |
| 7 | **backend 只消费默认顶层名 `top`** 的产物（§2.3 E3 附近的顶层名约束）⇒ ⇒ 调用方若设 `top_module_name = "my_top"` ⇒ ⇒ 该产物**无法**被 backend 仿真 ⇒ ⇒ 「层次化产物不被 backend 消费」的已知残留**只对非默认名成立** | 保留选项（已落地公开契约）⇒ ⇒ 本期**不做**「backend 支持任意顶层名」（模板参数化 + 缓存 key 加顶层名 + 测试矩阵扩张 ⇒ ⇒ 属能力扩张 ⇒ ⇒ 须先回 ADR 重开）⇒ ⇒ 入口**显式校验 + 致命拒绝 + 点名**（fail-early）⇒ ⇒ 记为已知边界 |

---

## 5. Verification（**前瞻**验收判据，MUST 可勾选 —— 裁决落地后适用）

- [ ] **效果判据**：三层设计（Top → Mid → Leaf）经 `toVerilogHierarchical()` 产出
      **3 个 module + 2 条实例化语句** ⇒ 且**无任何** `assign` 直接跨越 module 边界
- [ ] **效果判据（最强 · ADR-061）**：**默认（全 inline）**的层次化产物 MUST 与扁平产物
      **逐字节一致**（同名、同序、同端口表）⇒ ⇒ **允许差异 = 空集**（`cmp` 退出码 0，见 §5 末）
      ⇒ ⇒ 这条是**零回归**判据（「inline ≡ 成层」的语义等价由下一条承担）
- [ ] **效果判据**：同一设计 `ForceInline` vs `ForceLayer` ⇒ ⇒ 显式观测集合
      **逐拍全等**（⇒ ⇒ inline 与成层**语义等价**）
- [ ] **效果判据（结构同构）**：RTL module 树 ≡ 「**被声明为成层的**」组件子树 ——
      **节点数、父子关系、实例名** MUST **一一对应** ⇒ ⇒ （计数对但树形错 ⇒ **不通过**）
      ⚠️ **「RTL module 树」MUST 从产物本身独立提取** ⇒ ⇒ MUST NOT 复用分析层的
      `ModuleDef` / `InstanceDef`（⇒ 否则判据**自证**，「计数对但树形错」**永远不可触发**）
      ⇒ ⇒ 提取工具 **MUST 用 `verilator --json-only`**（`.tree.json`）
      ⇒ ⇒ ⚠️ **`--xml-only` 已作废**：仓内钉的 verilator 是 **5.048**
      （`third_party/verilator/configure.ac:15`），其 `Changes:228` 明写「Remove deprecated
      `--xml-only`」⇒ ⇒ **该选项在本仓不可用**（oracle R1 实测）
      ⇒ ⇒ ✅ **提取机制已实测可行**（2026-11-02 · C0-0 spike）：`.tree.json` 的 `modulesp`
      是**模块列表**（每项带 `addr` 与 `name`），每个模块的 `stmtsp` 里 `type == 'CELL'`
      的项带 `name`（实例名）与 `modp`（指向被实例化模块的 `addr`）
      ⇒ ⇒ **一次查表**即得 `(parent_module, inst_name, module_name)`
      ⇒ ⇒ 提取器已固化为 `scripts/lib/verilog_tree_extract.py` + 护栏
      `scripts/verify_hierarchical_tree_extract.sh`（含负向对照 ⇒ 非恒绿）
- [ ] **效果判据**：**映射表** MUST 被产出，且 MUST 覆盖被观测的全部信号
      ⇒ ⇒ **载体** = `HierarchyEmitResult` 的新增公开字段（value-only · 带 `node_id` ·
      `hier_top_port_name` 允许为空以表达「不在层次化顶层」⇒ ⇒ 见 §2.2 的 v2.13 增补）
      ⇒ ⇒ **两次 emit MUST 共享同一 ctx**（`node_id` 有效性域）⇒ ⇒ harness MUST 断言之
      ⇒ ⇒ **变异**：映射表缺一项 ⇒ ⇒ MUST 转红
- [ ] **效果判据（backend 消费路径 · v2.13）**：`initialize_with_verilog` MUST 接收
      **显式端口集合**（⊇ {clock, reset}）⇒ ⇒ MUST 走该集合而非 ctx 全量
      ⇒ ⇒ 且该集合 MUST 由 **emitter 产物**（映射表 ∪ `port_provenance`）**派生**，
      **MUST NOT** 由 harness / backend 独立撰写 / 手工列举（§2.2 末段 + §2.3 E1）
      ⇒ ⇒ 失败 MUST **致命**（非 best-effort `CHWARN`）⇒ ⇒ harness MUST 断言 backend
      真初始化成功（**非 SKIP 即过**）⇒ ⇒ 既有 `initialize(ctx, …)` 行为逐字节不变
      ⇒ ⇒ **变异**：集合漏 clock/reset ⇒ ⇒ 寄存器永不更新 ⇒ ⇒ 逐拍全等**恒绿**
      ⇒ ⇒ **MUST 转红**（这是本判据的判别力自证）
- [ ] **效果判据（顶层名 · v2.13）**：层次化产物顶层 module 名 ≠ `top` ⇒ ⇒ 入口
      MUST **致命拒绝 + 点名**（fail-early）⇒ ⇒ **MUST NOT** 落到编译期才发现
- [ ] **效果判据（观测盲区登记 · v2.13 · 承重）**：等价性判据 MUST 显式声明其
      **只覆盖顶层可见行为**（被移出顶层的那批 `ch_out` 顶层不可观测 ⇒ ⇒
      「子树内部算错但顶层输出相同」**测不出**）⇒ ⇒ 且 MUST 在 capability spec +
      用法指南**同步登记**（只登记 ADR 一处 = 漂移）
      ⇒ ⇒ ⚠️ **输入方向无盲区**（顶层 input 集合两侧 MUST 相等）
- [ ] **效果判据**：对显式声明的观测集合 `O`（**非空**且**至少一个随激励变化**）⇒
      `flat[O] ≡ hier[O]` **逐拍全等**（≥ 5 个 DUT：两层 / 三层+兄弟连线 /
      bundle 成员跨层 / 含 `ch_mem` / 真实最深设计）
- [ ] **效果判据**：顶层端口集合 MUST 满足 §2.2 ③ 的**两条精确判据** ——
      ① **输入端口集合 MUST 相等**（名字 + 宽度逐项配对）；② **输出端口集合 MUST 等于
      「精确期望集」**（扁平输出集 − {`ch_out` 的**最近成层祖先 ≠ 根**} 的全部节点）
      ⇒ ⇒ ⚠️ **v2.7 订正**：本条原文写「`hier.top_ports ⊆ flat.top_ports`（集合包含关系，**独立**判据）」
      ⇒ ⇒ 该形态**已被 v2.5 撤销**（「丢端口正是**子集方向** ⇒ ⇒ `⊆` 单独**不是**有效判据」）
      ⇒ ⇒ ⇒ 且它与本 ADR 自己的 §2.2 ③ **互斥** ⇒ ⇒ ⇒ 现改为引用 §2.2 ③ 的两条精确判据
      ⇒ ⇒ ⇒ **本条 MUST NOT** 被读成「允许 `⊆`」
- [ ] **判别力**：故意破坏层次化产物（删一条连线 / 改一个隐式端口方向）⇒ ⇒
      等价性测试 MUST **转红** ⇒ ⇒ 还原后 MUST 复绿
- [ ] **结构判据（`owner == nullptr`）**：`default_clock` / `default_reset` 与用户在
      `main()` 里**不经组件**建的节点 MUST 出现在**顶层 module**（MUST NOT 被拒绝）⇒ ⇒
      用户在 `main()` 里建的节点 MUST 有 `CHWARN` 点名 ⇒ ⇒ **clock/reset MUST NOT 告警**
      （按设计不归属任何组件 ⇒ 告警会恒响）
- [ ] **结构判据（`owner ∉ subtree(top)`）**：一个**已 build 但未登记**进组件树的组件
      （`make_unique` + `build(ctx)` 而不 `add_child`）⇒ ⇒ emitter MUST **抛异常**
      （MUST NOT 静默并入顶层）⇒ ⇒ 且错误信息 MUST 给出 `add_child` 修法；
      ⇒ ⇒ **负向对照**：`add_child` 之后同一设计 MUST 正常产出
- [ ] **结构判据（clock/reset 发射规则 · oracle R3/R4）**：
      **根 module MUST 无条件**发射 `default_clock` / `default_reset`
      （扁平 `print_header` 就是无条件的，`src/codegen_verilog.cpp:333-343`；
      而 `context::init()` 恒定创建二者 ⇒ **纯组合设计的扁平产物也有这两个端口**）
      ⇒ ⇒ **只有非根 module** 才按 `has_seq` 决定
- [ ] **结构判据（clock / reset 必须分开判 · oracle R3 + critic R4）**：
      **clock** ⇒ 含 `ch_reg` **或** mem 写端口 **或** `sread`；
      **reset** ⇒ 含 `ch_reg` **或** `sread` —— ⚠️ **mem 写端口的 `always_ff` 没有 reset 分支**
      （`src/codegen_verilog.cpp:1310`）⇒ 纯 RAM 写模块 MUST **只发 clock**。
      ⚠️ 漏掉**写端口** ⇒ 该模块 `has_seq=false` ⇒ 引用未声明 clock ⇒ Verilator 报错；
      ⚠️ 用**一元** `has_seq` ⇒ 纯 RAM 写模块得到一根**永不使用**的 reset 端口
- [ ] **结构判据**：每个实例化的端口名 MUST ∈ 目标 module 的端口集合；宽度 MUST 一致
      ⇒ ⇒ **零**未声明引用
      ⇒ ⇒ ⚠️ **lint 门的唯一权威 = ADR-060 §2.5（v1.9 · 2026-11-05）**：本行原写的
      `--lint-only -Wall` **已被其取代** —— 实测 `-Wall` 会对 ADR-061 决策② 下**合法**的
      空连接（`.port()`）报 `PINCONNECTEMPTY`（warning-fatal ⇒ exit≠0）⇒ ⇒ 若按 `-Wall`
      判，任何含「未消费子输出」的常规层次化产物都**必然失败**。⇒ ⇒ 本行现读作：
      **按 ADR-060 §2.5 的命令与 warning 集执行**（不加 `-Wall` · 捕获退出码 ·
      stderr 不含 `%Error` · `lint_off` 白名单）
- [ ] **确定性判据**：同一设计连续生成两次 ⇒ **字节相同**
      ⇒ ⇒ ⚠️ 限定「**同一编译器 / ABI 内**」（`typeid` 拼写**实现相关** · ADR-059 D4 v2.2）
      ⇒ ⇒ 跨编译器 / 跨平台的 golden 比较**不在**本判据范围内
      （⚠️ 2026-11-03 用户裁决「CI 只做 Ubuntu」⇒ ⇒ 仓内**没有**任何跨平台对照证据 ⇒ ⇒ 「同 ABI」不是「有矩阵可对照」而是「无对照可查」）
- [ ] **零回归判据**：`toVerilog()` 的扁平产物**逐字节不变**
      （基准 = **ADR-059 落地后**的产物 ⇒ **不是** ADR-059 之前）
- [ ] **零回归判据**：`ctest -LE perf` / `run_all_samples.sh` /
      `run_all_ported_tests.sh` 全绿 · 计数锚点同步
- [ ] **护栏**：成层组件的模块类型名 MUST 按 **ADR-059 §2.2.1 的四档**确定：
      ① 类级 `CH_HIERARCHY_LAYER("name")` · ② `ch_layer<T>` **继承** `T` 的类级名 ·
      ③ `ch_layer<Helper>`（`T` 未声明成层）**在实例处给名** ⇒ ⇒ **①②③ 缺名 MUST 是编译期错误**
      （⇒ ⇒ MUST NOT 依赖发射时的运行期硬错兜底）；
      ④ `Granularity::ForceLayer` 强制成层的组件 ⇒ ⇒ 允许回退 `sanitize(typeid)`
      **+ MUST 有 `CHWARN`**（⇒ ⇒ 其名字仅「同 ABI 内」可复算）。
      ⇒ ⇒ **非成层**（inline）组件 MUST NOT 参与 module 命名 ⇒ ⇒ 无需模块类型名。

### 「逐字节一致」的**允许差异 = 空集**

> `toVerilogHierarchical(..., ForceInline)` 的**唯一**产物文件与 `toVerilog()` 的产物
> MUST **字节相等** ⇒ ⇒ 可执行判据 = `cmp` **退出码 0**。

⇒ ⇒ 全 inline 时只有 **1 个** module（就是 `module top`）⇒ 「module 包装」无操作含义
⇒ ⇒ 任何差异都说明实现引入了新东西。⇒ ⇒ 若实现**必须**引入差异（如 banner）
⇒ ⇒ MUST **先改本 ADR** 写成显式允许项，MUST NOT 在实现里默默引入。

⚠️ **端口顺序 MUST 也一致**：扁平的实际顺序是 **clock → reset → 无 driver 的 input →
output**（`src/codegen_verilog.cpp:333-362`）⇒ ⇒ 顶层 module 端口表 MUST 复刻该顺序。

---

## 6. 变更记录

| 日期 | 变更 |
|---|---|
| 2026-11-05 | **v2.14**（落实 critic 对本轮审稿的 **2 SHOULD_FIX + 2 NIT** ⇒ ⇒ 决策内容**零变更**，只补可判定性）：① **§2.2 映射表载体 + §2.3 E1 补同一条纪律** —— 显式端口集合 MUST 由 **emitter 产物**（映射表 ∪ `port_provenance`）**派生**，**MUST NOT** 由 harness / backend 独立撰写（否则与 emitter 真相脱钩 ⇒ 表错则两错相抵）；② **§2.3 的 `port_provenance` 块订正为「已落地」**（`include/hierarchy_verilog.h:48-69`；原文写「尚未落地」）并明示它**不含 `node_id`** ⇒ 不能顶替 §2.2 的四元组映射表（两者并存） |
| 2026-11-05 | **v2.13**（T3-④ 归档后 · architect 角色 · 据 **oracle** 对实施期缺口的对抗复核）：**补齐 T3-005 的四处「契约缺口」** —— ① **§2.2 加映射表载体**：MUST 由 emitter 产出（禁测试自建）· 载体 = `HierarchyEmitResult` 新增**公开字段**（value-only · 带 `node_id` · `hier_top_port_name` 允许为空以表达「不在层次化顶层」）· **不**另开 manifest 入口；并加 **`node_id` 有效性域**（ctx 私有 ⇒ 两次 emit MUST 共享同一 ctx ⇒ 跨 ctx 退化为「设计级信号名」维度）；② **§2.3 加该只增不改入口的三条硬约束 E1/E2/E3**（**显式端口集合** ⊇ {clock, reset} · **物理隔离**既有路径 + 双 work_dir · 失败**致命**非 best-effort ⇒ 否则 backend 静默不可用 ⇒ 套件 SKIP = 恒绿假绿）**+ 顶层 module 名约束**（实测：Verilator 顶层类名由 `--top-module` 或**输入文件名**决定 ⇒ 非 `top` ⇒ **编译期**失败 ⇒ 入口 MUST 显式校验并**致命拒绝**；**保留** `top_module_name` 选项（已落地公开契约）⇒ 「backend 只消费默认名」记为已知边界）；③ **§4 加风险 6（观测盲区：等价性只覆盖顶层可见行为 · 输入方向无盲区）+ 风险 7（backend 只消费默认顶层名）**；④ **§5 加三条对应判据**（映射表载体 + backend 消费路径 + 盲区登记），并把映射表判据补上「缺一项 ⇒ 转红」的判别力自证。⚠️ 决策内容**零变更** —— 本版是把**已裁决但未写死的实施约束**补成可机械判定的条款（oracle 实测缺口） |
| 2026-11-05 | **v2.12**（B3 开工前 · architect 角色 · 据实施期查明的**两处契约缺口**）：① **§2.3 的签名按已落地形态订正** —— 原文 `std::expected<HierManifest, HierError>` / `VerilogHierOptions` 与实现（`HierarchyEmitResult` / `HierarchyEmitOptions`）**漂移** ⇒ ⇒ 按实现订正（**语义契约不变**：失败可观测 + 诊断可承载 + 单文件产物）；② **§2.3 新增错误模型 ⑤「错误载体统一口径」** —— 对外唯一载体 = `HierarchyEmitResult`；**发射器内**校验失败（端口策略 / 跨层宽度 / 白名单 / 端口完整性 / 命名冲突）MUST 走 `ok=false` + `error`，**MUST NOT** 依赖异常（异常会落进 `print()` 的 `catch(...)` ⇒ 静默失败）；**分析层前置校验**允许 `CHFATAL_EXCEPTION`，入口 MUST 在第一次 I/O 前**转换**（不是吞掉）；并声明**各 ADR 中 `CHFATAL_EXCEPTION` 字样按本口径解释**（可观测形态由所在层决定）⇒ ⇒ 目的是消除 B3 的「白名单说抛异常、发射器返回结构体」两套错误通道 |
| 2026-11-02 | **创建**（裁决：新增独立 API · 语义边界 = 所有权边界 · 与扁平逐拍等价 · 隐式端口合成+告警 · 悬空输入冒泡 · module 复用 · ADR-059 为硬前置） |
| 2026-11-02 | **v2**（据对抗审稿）：① **范围收窄** ⇒ 端口策略与 module 复用**拆出到 ADR-060**（一决策一文档）；② **等价性判据重写**为「映射表 + 显式观测集合 `O`（非空且含变化信号）+ 顶层端口集合包含关系」⇒ 修掉「全部顶层输出全等」与「子输出不冒泡」的自相矛盾；③ **新增「结构同构」承重不变式**（module 树 ≡ Component 树）；④ **新增 backend 只增不改例外**（否则 P4 等价性**不可执行**）；⑤ 补「过渡手段的归位条件」；⑥ 删掉未复算的调用点数量表述 |
| 2026-11-02 | **v2.1**（用户拍板）：**backend 只增不改例外 ⇒ 允许**（§2.3 末尾已标注「已裁决」）⇒ ⇒ ⇒ P4 的「多文件 harness」退化为「backend 加一个只增入口」⇒ ⇒ ⇒ 预估按 ADR-058 §2.3 的例外路径执行 |
| 2026-11-02 | **v2.2**（据 ADR-061）：① 「一个组件 = 一个 module」限定为「**被声明为成层的**组件」⇒ ⇒ **默认 inline**；② 「结构同构」不变式改写为「**成层组件子树**」；③ **新增「默认全 inline ⇒ 与扁平逐字节一致」**这条最强判据；④ 命名权威统一为 **ctx 级全局表**（⇒ 顶层端口名与扁平逐字一致 ⇒ 下游按名解析不受影响） |
| 2026-11-02 | **v2.3**：① 裁决 **`owner(n) == nullptr` ⇒ 归顶层 module（+ `CHWARN`）**，**MUST NOT** 拒绝（理由：clock/reset 是 context 级基础设施 · 与扁平行为一致 · 语义上确是顶层胶合逻辑）⇒ 阻塞 P1 的未决项清零；② 订正 §2.2 的映射表理由（原先写「层次化侧靠 per-module 计数」，与 ADR-061 决策 ④ 的 ctx 级全局命名冲突）；③ `VerilogHierOptions` 补 `granularity` 字段 |
| 2026-11-02 | **v2.4**（据 critic 第 1 轮 2 BLOCKER）：① **新增 `owner(n) != nullptr` 但 `owner ∉ subtree(top)` 的裁决 = 响亮拒绝**（下游 `make_unique`+`build(ctx)` 形态的组件不在树里 ⇒ emitter 找不到 ⇒ **整块逻辑静默消失**，且三条判据**结构性失明**）；② **`CHWARN` 排除 clock/reset**（否则每个含寄存器的设计恒响）；③ **「允许差异」定为空集**（可执行：`cmp` 退出码 0）+ 端口顺序 MUST 复刻扁平；④ **结构同构的 RTL 树 MUST 从产物独立提取**（`verilator --xml-only` 或独立的文本解析）⇒ 否则判据自证；⑤ 订正映射表理由（权威两侧相同，要映射的是「一个信号出现在哪些位置」）；⑥ 「零回归」与「inline ≡ 成层」两条判据措辞拆开 |
| 2026-11-02 | **v2.5**（据 oracle 实测，6 条）：① 结构同构的提取工具**作废重定** —— `--xml-only` 在仓内 verilator **5.048** 已被删除（`Changes:228`）⇒ 改 `--json-only` / `--dump-tree`，并**新增 P0 前置 spike**（提不出 ⇒ 判据重设计）；② **`⊆` 单独不作判据**（丢端口正是子集方向）⇒ 改「输入集合**相等** + 输出集合**等于精确期望集**」；③ `has_seq` **补 `ch_mem` 写端口**（`codegen_verilog.cpp:1310` 发 `always_ff`）· **根 module 无条件发 clock/reset**（扁平就是无条件）；④ 映射表补 **`node_id` + `hier_top_port_name`**（否则 backend 用不了）；⑤ 返回类型改 `std::expected` + **错误模型**（校验先于 I/O · 临时目录 + rename · 不得落在 `catch(...)` 内） |
| 2026-11-02 | **v2.6**（C0-0 spike 完成）：「结构同构」的提取机制**实测可行** ⇒ `verilator --json-only` 的 `.tree.json` 里 `modulesp` 是**模块列表**、CELL 的 `modp` 可直接查表 ⇒ ⇒ 「P0 之前 MUST 先做 spike」这条**前置已消解** ⇒ 提取器 + fixture + 护栏（含负向对照）已固化并挂 CI |
| 2026-11-05 | **v2.11**（据 **critic BLOCKER** · architect 角色）：**消解跨 ADR 互斥** —— §5 的「零未声明引用（`verilator --lint-only -Wall` 零 error）」与 ADR-060 §2.5（v1.9）的「门**不加** `-Wall`」**互斥**，而 ADR-061 v1.6.4 把「未消费子 output 用空连接」定为**合法且强制** ⇒ ⇒ 按 `-Wall` 判会让常规层次化产物**必然失败**。⇒ ⇒ §5 该行加指针、**以 ADR-060 §2.5 为唯一权威**；`-Wall` 措辞**已被取代**（不是「二者并存」）。决策内容**零变更**（只是把验收口径缝合到单一权威） |
| 2026-11-04 | **v2.7**（据 2026-11-04 架构评审 · **消解跨 ADR 矛盾**，非新增决策）：① **§4 风险 5 + §5 护栏**的 `CH_MODULE_NAME` 口径**与 ADR-059 D4（v2.2 · 2026-11-03）对齐** —— 原措辞（「回退 ⇒ 退化可用、MUST NOT 编译失败」）**已被 ADR-059 明文撤销** ⇒ ⇒ **两条 Verification 判据原本互斥**（一条要求 MUST 回退、一条要求 MUST 硬错）⇒ ⇒ 现按**粒度**分述（成层 ⇒ 硬错 / 非成层 ⇒ 回退）⇒ ⇒ ⚠️ ADR-059 D4 明确写「撤销本 ADR 早期措辞」⇒ ⇒ 本次是**执行既有裁决**，不是新决策 ⇒ ⇒ ⚠️ **本行的措辞是中间态**：成层内部还分「用户显式给名」与「`ForceLayer` 回退」两格 ⇒ ⇒ **已被 v2.8 + ADR-059 §2.2.1 的四档来源取代**（本行保留作历史）；② **§5 确定性判据**补「**同一编译器 / ABI 内**」限定（ADR-059 D4 v2.2 已确立 `typeid` 拼写实现相关 ⇒ 该限定此前只存在于 ADR-059，本 ADR 未同步 ⇒ ⇒ 跨 ADR 漂移） |
| 2026-11-04 | **v2.9**（2026-11-04 · architect 角色 · 实施期缺口复核）：① **§2.3 的产物形态改判 = 单文件唯一模式** —— 撤销「目录模式 / 一 module 一文件」⇒ ⇒ 相应撤销 ADR-061 §2.5 的 `out_dir` / `one_file_per_module` 两个选项（Zero-Debt ⇒ 不留未实现字段）⇒ ⇒ 理由：N1 由「module 独立」满足（不要求文件独立）· T3-⑤ 的 backend 编译**单文件** ⇒ 多文件要**再翻倍**成本 · 错误模型简化；② **错误模型措辞改单文件**（临时**文件** → rename；失败路径 MUST NOT 触碰既有同名文件）；③ **「同名不同物」的消解手段明确** = 文件名由调用方显式指定 ⇒ 层次化产物路径 MUST NOT 与扁平产物同名（用法指南 + 护栏守）；④ 本次**未**改等价性判据（§2.2）与结构同构判据（§5）⇒ ⇒ T3-⑤ 的判据口径**零变化** |
| 2026-11-04 | **v2.8**（2026-11-04 架构评审 · 用户拍板）：① **§5 的「`⊆` 独立判据」撤销** —— 该形态**已被 v2.5 撤销**（「丢端口正是子集方向 ⇒ `⊆` 单独不是有效判据」）⇒ ⇒ 但 §5 的效果判据列表里**仍留着它** ⇒ ⇒ 与本 ADR 自己的 §2.2 ③ **互斥** ⇒ ⇒ 现改为引用 §2.2 ③ 的两条精确判据（输入集合**相等** + 输出集合**等于精确期望集**）；② **Status 转正**（Proposed → Accepted） |
| 2026-11-05 | **v2.10**（2026-11-05 · 用户拍板 · architect 角色）：**C1 闭合** —— 下游交付管线**接受「单文件多 module」**（⇒ 台账 §11.6 C1 ✅ 已答）⇒ ⇒ ⇒ §2.3 的「单文件唯一模式」由「**条件条款**」转为**无条件裁决** ⇒ ⇒ 「若交付管线要求文件级切分 ⇒ 按条件条款重开」这条**撤销** ⇒ ⇒ ⇒ 连带效果：T3-⑤ 的 backend **不再需要**文件列表 + 缓存 key 改造 ⇒ ⇒ ⇒ **成本不翻倍**；④ **给「不重开」补了**理由**（文件级切分 = 下游工具链动作 ⇒ ⇒ 上游零改动 ⇒ ⇒ 不构成重开条件）；③ ⚠️ 本版**未**改 §2.2 等价性判据 / §5 结构同构判据 / §2.3 的其余部分 ⇒ ⇒ **C2**（是否拼接多份产物）**仍未答** ⇒ ⇒ 但它**不**影响本裁决（顶层名已可配置；⚠️ 内部 module 名那一轴见台账 §11.6）；④ **给「不重开」补了**理由**（文件级切分 = 下游工具链动作 ⇒ 上游零改动）**并做了实测背书**（三层产物按 module 边界切成 3 个 `.sv` ⇒ `verilator --lint-only --top-module top` **exit 0**）|
