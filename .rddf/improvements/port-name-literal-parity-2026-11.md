---
name: port-name-literal-parity-2026-11
priority: P2
phase: phase-6
category: framework
type: feature
状态: draft
主题: Verilog codegen 完整化
依赖: port-naming-ratchet-2026-11（命名规则 + N5 第 1 步 manifest 双字段）· rtl-emit-product-tooling-2026-11（manifest schema · is_top_io）· component-ownership-and-naming（节点名构造的规范所有者）· public-api-remove-visibility-2026-11（公开面登记机制）
来源: 用户 2026-11-10 拍板「⑤ 生成的 RTL 顶层端口，希望保持和 C++ 侧一样的命名」+ planner 三探针实测（2026-11-10）+ metis 第 1 轮 gap 分析
生成时间: 2026-11-10
evidence: probe:三探针（根名前缀 / 扁平命名空间消歧 / 同名兄弟实例）—— 源码 MUST 逐字搬进 tests/test_port_name_parity.cpp（见 P1）⇒ ⇒ 仓内可复现
---

# Improvement: 端口名与 C++ 侧**字面同名**的**可行边界**（探针固化 + 路线裁定）

- **Date drafted**: 2026-11-10
- **Drafted by**: planner 角色（build-agent）
- **Status**: 📝 **draft**（待用户拍板 ⇒ 拍板后转 builder 流程）
- **来源**：`port-naming-ratchet-2026-11.md` 的 **N5 第 2 步（路线 (c)）** ⇒ ⇒ 该步被明文要求「**独立立项**」⇒ ⇒ 本文件即该立项
- ⚠️ **与 N5 第 2 步的关系**：本项**不是**「把 N5 第 2 步做完」，而是「**先测出可行边界，再决定做不做**」⇒ ⇒ 实测 + metis 复核**共同收窄**了 N5 第 2 步的收益（见 §A.3）
- **归属**：**端口选择语义**归 `rtl-emit-product-tooling-2026-11` · **产物端口命名规则**归 `rtl-hierarchical-output` · **节点 / 实例命名的构造**归 `component-ownership-and-naming` ⇒ ⇒ 本项只管「**字面同名的成立条件**」这一条契约

---

**主题**: Verilog codegen 完整化

## 架构依据

### A.0 ⚠️ 两个「顶层端口」集合**不是同一集合**（metis 订正 · 本项的第一前提）

| 集合 | 定义 | 证据 |
|---|---|---|
| **T1 = 顶层 module 的端口表** | `clock → reset → 无 driver 的 input → **全部** ch_out（⇒ **含 inline 后代**）⇒ ⇒ **⊋ T2** | `openspec/specs/rtl-hierarchical-output/spec.md:309-316`（已签批契约） |
| **T2 = 根组件自有的端口** | `is_top_io == true` 的那部分 | `rtl-emit-product-tooling-2026-11.md:64`（manifest 的 `is_top_io` 字段） |

⇒ ⇒ **用户原话「顶层端口」指 T1 还是 T2，仓内无证据** ⇒ ⇒ 本项**不预设** ⇒ ⇒ **🔴 oracle 建议 2 已裁定**：<br>⇒ ⇒ **判别字段** = `is_top_io`（**不新造判据**）✅ planner 此点成立<br>⇒ ⇒ **但 MUST 的适用集合取 T1**（顶层 module 端口表**全体**）⇒ ⇒ **SHOULD（字面同名）的适用集合取 T2**（`is_top_io == true`）<br>⇒ ⇒ ⇒ **理由**：MUST 的职责是「**每个顶层端口都能被机器解析回 C++ 成员名**」⇒ ⇒ T1 里那 80 个 inline 后代输出**同样在 .v 里**（契约要求）⇒ ⇒ 下游仍需知道它们叫什么 ⇒ ⇒ 若 MUST 只覆盖 T2 ⇒ ⇒ **覆盖面漏洞** ⇒ ⇒ 而 SHOULD 只对「用户声明的接口」（T2）谈「字面同名」才有语义 ⇒ ⇒ 对契约强制上顶的内部输出谈「C++ 侧同名」**语义不成立**<br>⇒ ⇒ ⇒ **T1/T2 的口径因此不再阻塞 P2**（MUST 集合恒为 T1）⇒ ⇒ **§Impact 风险表里「裁决前 P2 不得定稿」已删除**<br>⇒ ⇒ ⚠️ **本项全程用「根组件端口」指 T2、用「顶层 module 端口」指 T1**，二者不混用。

### A.1 实测事实（2026-11-10 三探针 · 非推断）

> 三个探针的源码在 `/tmp/opencode/{probe_bare_port_name,probe_bare_collision,probe_dup_sibling}.cpp`（**scratch**）
> ⇒ ⇒ ⚠️ **本项的第一个交付物就是把它们逐字搬进仓内**（P1）⇒ ⇒ 否则结论**随会话蒸发**（本仓 ANTI-PATTERNS：「未运行的验证」不可判）。

| # | 事实 | 证据 |
|---|---|---|
| ① | ✅ **`__io` 内具名构造**可用**（实测生效）**：`__io(ch_in<ch_uint<32>> prog_addr{"prog_addr"};)` ⇒ ⇒ 图节点名 = `top.prog_addr` ⇒ ⇒ Verilog 端口名 = **`top_prog_addr`**（**不是** `top_unnamed_input_2`）⇒ ⇒ 框架的 NSDMI 经 `new (io_storage_) io_type` **确实生效** | 探针 ①：`IN raw_name=top.prog_addr parent=top` → `verilog=top_prog_addr`<br>⇒ ⇒ **`port-naming-ratchet` N1②「本轮静态推断，未运行验证」** ⇒ ⇒ **该开放问题已闭合** |
| ② | 🔴 **路线 (c) 当前**走不通**：根实例名置空（`Component(parent, "")`）⇒ ⇒ 图节点名 = **`unnamed.prog_addr`** ⇒ ⇒ Verilog 端口名 = **`unnamed_prog_addr`** ⇒ ⇒ **既不是**裸 `prog_addr`，**也不是** N5(c) 预判的 `.prog_addr` → `_prog_addr` ⇒ ⇒ **是第三种形态** ⇒ ⇒ **N5(c) 的字面回退条件已被证伪** | 探针 ①（根名 = `""`）：`IN raw_name=unnamed.prog_addr parent=unnamed` → `verilog=unnamed_prog_addr`<br>⇒ ⇒ 按 N5(c) 预案 ⇒ ⇒ **退回 ADR 讨论** |
| ③ | ⚠️ **根因 = 三处拼接**（metis 订正：原稿写「两处」自相矛盾）：<br>① `src/component.cpp:66` `local_name_(name.empty() ? "unnamed" : name)` ⇒ ⇒ **空名被替换成字符串哨兵 `"unnamed"`**（永不为空）<br>② `include/component.h:274` 的 `build_hierarchical_name` **有** `"unnamed"` 特判（`parent_path.empty() \|\| parent_path == "unnamed"` ⇒ ⇒ 返回**裸** `local_name`）⇒ ⇒ **组件名**路径已能去前缀<br>③ **但** `src/core/lnodeimpl.cpp:30-35` 的**节点名**拼接**没有**对应特判（`name_ = parent_->name() + "." + name`）⇒ ⇒ ⇒ **恒产出 `unnamed.<name>`** | `src/component.cpp:66` · `include/component.h:266-279` · `src/core/lnodeimpl.cpp:28-36`（**唯一**节点名拼接点）⇒ ⇒ 修 ③（对齐 ② 的特判）是**候选**最小改动点（**但见 §边界情况 ⇒ 「一行即可」是低估**） |
| ④ | ⚠️ **扁平命名空间靠前缀消歧**：根的 `prog_addr` 与叶子的 `prog_addr` ⇒ ⇒ `top_prog_addr` vs **`top_leaf_prog_addr`** ⇒ ⇒ 前缀是**当前**消歧手段 | 探针 ②：`raw=top.prog_addr → top_prog_addr` · `raw=top.leaf.prog_addr → top_leaf_prog_addr` |
| ⑤ | 🔴 **同名兄弟实例 ⇒ 追加去重后缀**：两个 `ch_module<Leaf>` **都叫 `dup`**、各带 `prog_addr` ⇒ ⇒ `top_dup_prog_addr` 与 **`top_dup_prog_addr_1`** | 探针 ③：`raw=top.dup.prog_addr → top_dup_prog_addr` · `raw=top.dup.prog_addr → top_dup_prog_addr_1`<br>⇒ ⇒ 去重计数器 `include/codegen_naming.h:72-88`（`counts[base]++`，**按 eval-list 拓扑序**） |
| ⑥ | ⚠️ **层次化产物**不提供**端口名歧义消除**（**metis 订正 · 原稿此处是自发明**）：层次化分析层**复用扁平全局表**（`build_verilog_name_table(ctx->get_eval_list())`）⇒ ⇒ **顶层端口名与扁平产物逐字相同**（`spec.md:10` 的硬契约）⇒ ⇒ 层次化买到的是 **module 作用域 + 实例路径**，**不是**端口名消歧<br>⇒ ⇒ ⚠️ 且同名兄弟在层次化侧走 `src/hierarchy_verilog.cpp:247` 的 `inst_of[m.key + "\x1f" + inst.instance_name]` ⇒ ⇒ **同名即静默覆盖** ⇒ ⇒ **层次化路径的同名兄弟后果 = 待查项** | `src/hierarchy_analysis.cpp:195-201`（注释自陈「顶层端口名与扁平产物**逐字相同**」）· `src/hierarchy_verilog.cpp:244-248` |
| ⑦ | ⚠️ **「字面同名」依赖三条前提**（由 ①–⑥ 合成，metis 订正后**不再是**单一条件）：<br>① 组件实例名在其父下**唯一**（排除 ⑤ 的同名兄弟）<br>② 允许**根组件**端口**去前缀**（需要 ③ 的对齐改动）<br>③ ⛔ **全设计 base name 不撞**（metis 边界情况 ⇒ ⇒ **这条是硬前提**）<br>⇒ ⇒ ⇒ **「全体组件字面同名」在当前扁平发射器下不成立**；「根组件端口字面同名」**有条件成立**（条件 ①②③） | 由 ①–⑥ 推得；③ 的两个反例见 §边界情况 |

### A.2 为什么必须先把边界测出来（而不是直接做）

- 诉求是「顶层端口同名」⇒ ⇒ **但「顶层」有 T1 / T2 两个集合**（§A.0）⇒ ⇒ 在用户语义裁决前，**「可兑现的那一半」是未定义的** ⇒ ⇒ ⇒ **不测就做 ⇒ ⇒ 极可能做出一个「只覆盖某个集合、却宣称通用」的半成品** ⇒ ⇒ 正是仓内「虚假信心」纪律要治的形态（见 `rv32i-dead-component-consolidation` W.1）。
- ② 已经**证伪了** N5(c) 的**字面回退条件** ⇒ ⇒ 若不订正，后续实施者会按**错误的失败形态**设计降级方案 ⇒ ⇒ ⇒ **契约必须先订正**（P2）。
- ⚠️ **与上游的正面冲突**（metis 发现 · 本轮最重的一条）：`port-naming-ratchet-2026-11.md:139` 已写「顶层端口名 MUST 与 C++ 侧**显式命名**逐字相同」+ `:159` 的 AC 写「`prog_we` ⇒ `prog_we`（**非** `top_unnamed_input_2`）」⇒ ⇒ 而本项若立「**禁**立 MUST 字面相同」⇒ ⇒ **两份同期草案对同一 spec 提交互相矛盾的 Requirement** ⇒ ⇒ ⇒ **必须二选一**（§待裁决 ② · 采纳 metis 的**方案 A**）。

### A.3 三条路线的裁决

| 路线 | 覆盖范围 | 改动面 | 产物影响 | 结论 |
|---|---|---|---|---|
| **① manifest 双字段 + `is_top_io`**（N5 **第 1 步** · 已在 B1 批内） | **全部端口**（不分层级） | **零产物变更** · 零 ADR 变更 · 零快照漂移 | 无 | ✅ **主力** ⇒ ⇒ 它不要求「RTL 名 == C++ 名」，只要求 manifest **记录对应关系** ⇒ ⇒ **对所有组件都成立** ⇒ ⇒ 且已能表达 T1 / T2 的区分 |
| **② route (c) 去根前缀**（= N5 第 2 步） | **仅根组件端口（T2）** | ① `src/core/lnodeimpl.cpp:30-35` 加 `"unnamed"` 特判（对齐 `include/component.h:274`）② 可能需放开 `component.cpp:66` 的 `"unnamed"` 哨兵 ③ **`Component::name()` 语义变更 ⇒ 公开 API 变更** ④ **唯一性论证（必需，见 §边界情况）** | **全仓产物字节变更**（每个设计的根端口名都变）⇒ ⇒ 既有 golden 全部要更新 | 🟡 **有条件做** ⇒ ⇒ 收益 = **根组件端口字面同名**；代价 = 公开面变更 + 全仓产物 churn + ADR-061:32 需一并修订 ⇒ ⇒ ⚠️ **metis 订正：原稿的「一行特判即可」是低估**（唯一性论证缺失，且上游 N5 已显式警告「先计数再剥**不保证唯一**」）⇒ ⇒ **MUST 由 architect 裁「值不值」**（P3 裁定包） |
| **③ 层次化发射器** | —— | 已有契约 | 另一条产物路径 | ⚠️ **不是「全层级无歧义」的答案**（A.1⑥ 已证伪：它**复用**扁平全局表）⇒ ⇒ **撤回原表述** |

### A.4 边界情况（metis 补充 · 5 条 · 全部进入 P3 的碰撞检测策略）

| # | 边界 | 事实 | 后果 |
|---|---|---|---|
| **E1** | 🔴 **route (c) 后，根端口与根组件**直接拥有**的内部节点同名** | `src/core/lnodeimpl.cpp:30-35` 对**所有**节点统一拼名；`include/codegen_naming.h:76-86` 对**全部** eval 列表节点去重 ⇒ ⇒ 若根组件同时有 `ch_out<...> prog_addr`（端口）与 `ch_reg<...> prog_addr`（内部寄存器），去前缀后两者 base 相同 ⇒ ⇒ **谁排后面谁吃 `_1`** | ⇒ 「字面同名」**静默失效**（端口名可能变成 `prog_addr_1`）⇒ ⇒ 且打破 `ADR-061:34`「端口名 MUST NOT 与内部节点同名」 |
| **E2** | 🔴 **🔴 planner 复核订正（2026-11-10 · oracle 根因 F-B + planner 实测）**：原稿写「sanitize 非单射 ⇒ ⇒ **去前缀后碰撞概率猛增**」⇒ ⇒ **该表述是错的** ⇒ ⇒ 正确事实：**根前缀是碰撞条件的两边公因子** ⇒ ⇒ 碰撞条件**剥前缀下不变** ⇒ ⇒ ⇒ 「根端口 `prog_addr` vs 子组件 `prog` 的端口 `addr`」**今天就已撞名** ⇒ ⇒ ⇒ **这是**既有不变式（隐含在 `ADR-061:34`）⇒ ⇒ **不是** route (c) 引入的新负担 ⇒ ⇒ ⇒ **route (c) 的成本估算 MUST 下调** | **实测**（`/tmp/opencode/probe_fb.cpp`）：<br>`root="top"` ⇒ 根口 `top_prog_addr` + 子口 `top_prog_addr_1` ← **今天就撞**<br>`root=""` ⇒ 根口 `unnamed_prog_addr` + 子口 `prog_addr`<br>⇒ ⇒ 碰撞条件 `M == I+"_"+m` **剥前缀前后同构** ⇒ ⇒ ⇒ **A.1⑦ 的第 ③ 条前提（「全设计 base name 不撞」）仍然成立**（它是**必要**条件）⇒ ⇒ 但它**不是**「去前缀引入的新风险」⇒ ⇒ ⇒ P3 的「唯一性论证」实为**对既有不变式的文档化**（不是新增论证） |
| **E3** | 🟡 **顶层组件不是 `ch_device`** | `ch_module` 作顶层时 `Component::node_owner()` 为空 ⇒ ⇒ `CHFATAL_EXCEPTION`（`include/module.h:50-61`）⇒ ⇒ route (c) **不能**靠「无条件去前缀」处理这一类 | ⇒ P3 的最小改动集 MUST 覆盖该形态（上游 N5 的「对 `ch_module` 作顶层静默失效」正是这条） |
| **E4** | 🟡 **Verilog 关键字 / 数字开头** | `sanitize_verilog_name`（`codegen_naming.h:48-62`）会给关键字加 `_r`（`reg` ⇒ `reg_r`）、给数字开头加 `_`（`1st` ⇒ `_1st`） | ⇒ 「字面同名」不可能 ⇒ ⇒ 已并入 P2 的「C++ 成员名 MUST 本身是合法 Verilog 标识符」判据 |
| **E5** | 🟡 **同名兄弟在层次化侧的后果未验证** | `src/hierarchy_analysis.cpp:422-435` 用相对 `local_name()` 拼实例名（未见唯一化）；`src/hierarchy_verilog.cpp:247` 的 map 以「父 + 实例名」为键 ⇒ ⇒ **同名即静默覆盖** | ⇒ ⇒ **待查项**（独立立项）⇒ ⇒ ⚠️ 与 A.1⑥ 一起 ⇒ ⇒ **不再声称「层次化天然无歧义」** |

---

## 范围
### ✅ 在内

| 编号 | 内容 |
|---|---|
| **P1** | **探针固化进仓**：`tests/test_port_name_parity.cpp`（Catch2）⇒ ⇒ **把三探针源码逐字搬进**（保留其「N5(c) 原始问句」注释）⇒ ⇒ **🔴 oracle 建议 3 定案：①②③④⑥ 一次搬入；⑤ 不作 SKIP 用例 ⇒ 转残差登记**：<br>① `Ratchet*`（**现状钉**）：`__io` 内具名构造生效 ⇒ 节点名 `top.prog_addr` / Verilog 名 `top_prog_addr`<br>② `Ratchet*`（**现状钉**）：根实例名置空 ⇒ 节点名 `unnamed.prog_addr`（**不是** `prog_addr`，**也不是** `.prog_addr`）<br>③ `Ratchet*`（**现状钉**）：同名兄弟 ⇒ 端口名含 `top_dup_prog_addr` 与 `top_dup_prog_addr_1`<br>④ `Device*`（**交付路径对照**）：用 `ch::ch_device<T>`（走 `include/device.h:28` 的**真实** `"top"` 路径）⇒ ⇒ 与 ① 的直接构造**等价** ⇒ ⇒ ⚠️ **不得**只固化仿真路径而不覆盖交付路径<br>⑥ 退化用例：根无名 + 无任何 `ch_in` ⇒ 命名表为空 ⇒ ⇒ 不崩且表为空<br>⑤ **目标钉（「根端口裸名」）** ⇒ ⇒ **不进测试文件** ⇒ ⇒ 改登记进 `.rddf/checks/arch-residual-registry.md`（含解除条件 + 触发条件）⇒ ⇒ 理由：「未运行的验证不可判」⇒ ⇒ SKIP **零判别力** · 残差登记有 fail-closed 护栏 ⇒ ⇒ **会主动催办** · Zero-Debt 对永久 SKIP 不友好 · 目标语义已写在 §A.1⑦ + P2 + P3 里 ⇒ ⇒ 再抄一份进测试文件**不增信息** |
| **P2** | **契约化（🔴 oracle 建议 2 订正了 planner 原案 · 集合口径改成「MUST 覆盖 T1 / 字面同名 SHOULD 限定 T2」）**：<br>① `rtl-hierarchical-output`（**产物端口命名的规范所有者**）追加 Requirement —— ⚠️ **条款正文见唯一落地文本** ⇒ ⇒ **唯一落地文本 = `.rddf/checks/decision-request-port-naming-parity-2026-11.md` §2 R1-a** ⇒ ⇒ ⇒ 该 R1-a 含 ① 「顶层 module 端口表全体 MUST 可解析到 C++ 成员名（manifest `cpp_member`）」（**按 A/B/C 三类互斥划分**：A 基础设施 · B 显式命名 · C 未显式命名 ⇒ ⇒ **B 类 MUST 提供 · A/C 类 MUST NOT 被要求提供**）+ ② 「C 类走兼容去重路径 + 登记棘轮豁免表」+ ③ 「字面同名的成立条件 / 不可能条件」⇒ ⇒ ⇒ **本项 MUST NOT 再复述条款正文**（否则又造一份副本 ⇒ ⇒ 漂移）<br>② `component-ownership-and-naming`（**节点 / 实例命名的规范所有者**）追加 Requirement —— ⚠️ **条款正文见唯一落地文本** ⇒ ⇒ **唯一落地文本 = `.rddf/checks/decision-request-port-naming-parity-2026-11.md` §2 R1-b** ⇒ ⇒ ⇒ 该 R1-b 含「节点名的**前缀拼接** MUST 口径**一致**」（有父 / 无父 两个 Scenario）<br>⇒ ⇒ ⚠️ **2026-11-11 · critic 第 3 轮 BLOCKER-2 修复**：原 ② 内的「空名哨兵（`"unnamed"`）的语义 MUST 明写」**已删除** ⇒ ⇒ 理由：metis finding 15 判定该 MUST **不可违反**（属 AI slop）⇒ ⇒ 姊妹草案 `node-name-prefix-consistency:141` 已明令**禁**加 ⇒ ⇒ ⇒ ⇒ 本项与姊妹草案原先一留一删 ⇒ ⇒ ⇒ 现**一致**（只作事实登记，不作 MUST）<br>⇒ ⇒ ⚠️ **「无名是普通名字，不是「无路径段」标记」这一**陈述句**保留在姊妹草案**（不是 MUST ⇒ ⇒ 不制造不可违反条款）⇒ ⇒ 且其规范所有者是 `component-ownership-and-naming` ⇒ ⇒ **本项 MUST NOT 复述**<br>⇒ ⇒ ⚠️ **禁**立「MUST 字面相同」这种**当前不可满足**的契约（metis：不可满足条款 = 永久技术债）⇒ ⇒ **禁**新立第 5 个命名所有者（metis 订正）<br>⇒ ⇒ ⚠️ **字段语义归 `rtl-emit-portability`**（`rtl-emit-product-tooling-2026-11.md:208` 是 manifest schema 的规范所有者）⇒ ⇒ 本项**只引用** `cpp_member` / `is_top_io` / `owner` / `driven` / `effective` 的定义 ⇒ ⇒ **MUST NOT** 重定义（否则又造一份 schema 副本）<br>⇒ ⇒ ⚠️ **2026-11-11 · critic 第 4 轮 BLOCKER-B 修复**：本格原先在「条款正文已移出」标注之后**仍逐字留着条款正文** ⇒ ⇒ ⇒ ⇒ **已真正删除**（不是只加标注）⇒ ⇒ ⇒ 现在本格**只有指针** ⇒ ⇒ ⇒ ⚠️ **判别规则**（供后续维护）：**有「条款正文已移出 / 条款正文见唯一落地文本」标注但正文仍在 ⇒ ⇒ 算副本 ⇒ ⇒ 违规** |
| **P3** | **route (c) 可行性裁定包**（**只出结论，不实施**）：产出 `.rddf/checks/decision-request-port-name-literal-parity-2026-11.md`（沿用仓内既有 3 份 `decision-request-*.md` 的形态）⇒ ⇒ MUST 含 6 项：① 最小改动集（**精确到 file:line**）② **唯一性论证 + 碰撞检测策略**（去前缀后与内部节点 / 子组件前缀串撞名时：**报错** / **保留 `_N`** / **禁止该写法** —— 三选一）③ 影响面**量化**（口径 + 数字）④ 公开面影响清单 ⑤ **「不做」选项 + 代价** ⑥ 裁决者 + 触发条件<br>⚠️ **planner 复核订正（2026-11-10 · oracle 建议 5 + 实测）**：<br>⇒ ⇒ ② 的「唯一性论证」实为「**对既有不变式的文档化**」⇒ ⇒ **禁**用「去前缀后碰撞概率猛增」当理由（E2 已证伪）⇒ ⇒ 真实的新增风险只有 **E1**（根端口 vs 根组件自有内部节点撞名）<br>⇒ ⇒ **选项清单 MUST 含「路线 ④ F-D」**（根实例名 = 产品路径参数）⇒ ⇒ ⚠️ **F-D 不是零成本**（实测：保留哨兵 ⇒ 得 `unnamed_prog_addr`（非字面同名）；拆哨兵 ⇒ 违反 `component-ownership-and-naming:62`「Verilog 名 MUST 含该组件的路径段」）⇒ ⇒ F-D = **零产物 churn · 但需 MODIFIED 一份已签批 spec**<br>⇒ ⇒ ⑤ 的「不做」选项 MUST 写成「**已评估 · 当前不做 · 触发条件**」⇒ ⇒ **禁**写「永久放弃」 |
| **P4** | **公开面登记**：若 P3 裁定「做」⇒ ⇒ `Component::name()` / 节点名前缀语义变更 MUST 走 `.rddf/checks/public-api-surface.md` + `docs/CHANGELOG.md` + **ADR-061 修订** ⇒ ⇒ 本项**只登记，不实施**<br>⚠️ **2026-11-10 更新**：P3 的结论已由 oracle 建议 1 定为「**当前不做 + 触发条件**」⇒ ⇒ ⇒ 故 P4 **不触发** ⇒ ⇒ ⇒ 本项的公开面登记义务**仅剩 P2 的 spec 追加**（走 openspec change 流程）⇒ ⇒ ⇒ 详见 `.rddf/checks/decision-request-port-naming-parity-2026-11.md` |
| **P5** | **与 N5 第 1 步的关系澄清**：第 1 步（manifest 双字段）**不依赖**本项 ⇒ ⇒ B1 批**不阻塞** ⇒ ⇒ 并把 §A.2 的**上游冲突**处置结果回填 `port-naming-ratchet` 的 N4 / AC（见 §待裁决 ② 的方案 A）<br>✅ **2026-11-10 已回填**（方案 A + 三条加固 ⇒ ⇒ 详见 `port-naming-ratchet` 的 N4 / N2 / AC / What Changes / Capabilities 五处） |

### ❌ 不在本项

- **命名规则本身**（合法标识符、关键字表、显式名优先级）⇒ ⇒ 归 `port-naming-ratchet-2026-11`
- **端口选择语义**（哪些端口算顶层）⇒ ⇒ 归 `rtl-emit-product-tooling-2026-11`
- **层次化成层策略 / owner 命名** ⇒ ⇒ 归既有 `rtl-hierarchical-output` + `ADR-060` / `ADR-066`
- **同名兄弟实例的消歧** ⇒ ⇒ ⚠️ **当前政策下**必然追加 `_N`；若 ratchet N3 切到 `kRejectDuplicates` ⇒ ⇒ 结局变为**创建点抛异常** ⇒ ⇒ ⇒ **这是政策依赖的结论，不是物理定律** ⇒ ⇒ 两条政策**显式二选一**（不试图在本项解决）
- **子组件端口的字面同名** ⇒ ⇒ 当前扁平发射器下**不成立**（A.1④/⑦）⇒ ⇒ 只**登记为已知限制**
- **层次化侧同名兄弟的静默覆盖**（A.1⑥）⇒ ⇒ 登记为**待查项**（独立立项），不在本项解决

---

## Why

### W.1 诉求可兑现，但「可兑现的那一半」是**未定义的**（metis 订正）

A.1⑦ + §A.0 ⇒ ⇒ 「顶层端口」有 T1 / T2 两个集合 ⇒ ⇒ 而用户说的是哪一个**仓内无证据** ⇒ ⇒ ⇒ 原稿 W.1 那句「正好落在可兑现的那一半」是**未经验证的推断** ⇒ ⇒ ⇒ 必须先裁决（§待裁决 ①）⇒ ⇒ ⇒ 否则 P2 的措辞无从下笔。

### W.2 N5(c) 的回退条件已被实测证伪（且是第三种形态）

② ⇒ ⇒ N5(c) 写的是「若产出的是 `.prog_addr` → `_prog_addr` ⇒ ⇒ (c) 也不成立」⇒ ⇒ 实际产出 **`unnamed_prog_addr`** ⇒ ⇒ ⇒ 该条件**永不命中** ⇒ ⇒ 若不订正，实施者会按**错误的失败形态**设计降级方案 ⇒ ⇒ ⇒ **契约必须先订正**（P2）。

### W.3 「一行特判」是**低估**（metis 订正 · planner 复核再订正）

③ ⇒ ⇒ 组件名路径（`component.h:274`）**已经**有 `"unnamed"` 特判 ⇒ ⇒ 节点名路径（`lnodeimpl.cpp:30-35`）**缺** ⇒ ⇒ 看似「补一处不一致」⇒ ⇒ **但**：<br>① 去前缀后**根端口与根组件自有内部节点**会撞名（E1 · **新增**风险）⇒ ⇒<br>② 上游 N5 **已显式警告**「先计数再剥**不保证唯一**」⇒ ⇒<br>③ ⚠️ **planner 复核订正**：原稿此处写「去前缀会引入**两类新碰撞**（其中一类 = 碰撞概率猛增）」⇒ ⇒ **该表述已被实测证伪**（E2 · 根前缀是公因子 ⇒ ⇒ 碰撞条件剥前缀下**不变** ⇒ ⇒ 那是**既有不变式**不是新负担）⇒ ⇒ ⇒ **真实的「低估」只剩 ① 与 ②**（E1 的内部节点撞名 + 唯一性论证）⇒ ⇒ ⇒ **route (c) 的成本估算应据此下调**（唯一性论证实为**对既有不变式的文档化**）⇒ ⇒ ⇒ P3 的成本估算 MUST 按此口径，**禁**用「碰撞概率猛增」当理由。

### W.4 不测就做的失败形态（仓内同型事故）

`rv32i-dead-component-consolidation` W.1：「组件测试全绿 ⇒ 但**交付路径根本没跑过它** ⇒ ⇒ **虚假信心**」。
本项的同型风险 = 「三探针只在 `/tmp/opencode/` ⇒ ⇒ 下个会话没人知道它们存在 ⇒ ⇒ 于是又按 N5(c) 的**错误**回退条件去做」⇒ ⇒ ⇒ **P1（探针固化进仓）不是可选项，是本项的最低交付** ⇒ ⇒ 且 P1 MUST 覆盖 `ch_device` **交付路径**（④），否则固化的是仿真路径。

---

## What Changes

### P1 — 探针固化进仓

- 新建 `tests/test_port_name_parity.cpp` ⇒ ⇒ **逐字搬入**三探针源码 + 两条补充用例（`ch_device` 对照 ④ · 退化用例 ⑥）⇒ ⇒ **🔴 oracle 建议 3 定案：⑤ 不搬入测试文件**（⇒ ⇒ 改落残差登记 ⇒ ⇒ 见 §范围 P1 的理由）。
- ⚠️ **现状钉的命名与归属**（metis 订正 + oracle 建议 3）：
  - ①②③ 改名 `Ratchet*` ⇒ ⇒ 文件头注释写明「本用例钉的是**当前行为**；若 route (c) 落地 ⇒ ⇒ 本组 MUST 被**改写**为目标语义」
  - ⑤（目标钉）⇒ ⇒ **不进测试文件** ⇒ ⇒ 落 `.rddf/checks/arch-residual-registry.md`（含**解除条件 + 触发条件**）⇒ ⇒ ⚠️ 残差登记与台账 §7 是**双向 1:1** ⇒ ⇒ **两侧 MUST 同批** ⇒ ⇒ ledger 侧属 architect 域 ⇒ ⇒ **单改登记表会打破护栏** ⇒ ⇒ ⇒ 落点见 §裁决记录 ④
- ⚠️ **变异自证 MUST 是「正向变异」**（metis 订正）：
  - ② 的变异 = 令 `component.cpp:66` 的 fallback 保空（或给 `lnodeimpl` 加特判）⇒ ⇒ **必须转红**（**禁**「删掉断言语句」式的假变异）
  - ③ 的变异 = 强制 `build_verilog_name_table` 的 counter 恒 0 ⇒ ⇒ **必须转红**
  - ① 的变异 = 把 `__io` 的具名成员改回匿名 ⇒ ⇒ **必须转红**
- ⚠️ **🔴 oracle 建议 3 补的硬要求 ①：断言层级 MUST 上到「发射出的 module 文本」**
  - ①②③④ 目前只钉到「命名表条目」⇒ ⇒ 而**有名字 ≠ 是顶层端口**（`src/codegen_verilog.cpp:100-102` 会把**有 driver** 的 input 滤成内部 wire）⇒ ⇒ ⇒ **至少 ①④ MUST 断言发射出的 module 文本**
  - 公开 API：`ch::verilogwriter`（`include/codegen_verilog.h:212-222`）+ `print(ostream)` ⇒ ⇒ 仓内既有写法见 `tests/test_verilog_arith.cpp:29-34` / `tests/test_verilog_gen.cpp:66-72` ⇒ ⇒ ⇒ **两个层级各有自己的变异**（命名层 / 产物层）
- ⚠️ **🔴 oracle 建议 3 补的硬要求 ②：变异自证 MUST 含「重建」步骤**
  - 根 `AGENTS.md` COMMANDS 段明写「**还原源码 ≠ 复原** —— 二进制也是状态」⇒ ⇒ 「必须转红 ⇒ 还原复绿」**必须**加「还原后**重建该 target** 再跑」⇒ ⇒ 否则跑的是**旧二进制** ⇒ ⇒ **假绿**（仓内已实测过同型事故）
- ⚠️ **exact-string 断言对全局表顺序敏感**（metis 订正）：`codegen_naming.h:69-70` 明写「换顺序就换名字」⇒ ⇒ fixture MUST **只含本用例所需节点**，并在用例内注释指明该依赖；或断言改为「以 `top_prog_addr` 为前缀且无 `_N`」。
- ⚠️ **退化用例**：根无名 + 无任何 `ch_in` ⇒ 命名表为空 ⇒ ⇒ 用例 MUST 断言「不崩且表为空」。
- ⚠️ **零新增端口 / 零公开面变更** ⇒ ⇒ 用**局部** fixture 组件 ⇒ ⇒ 现有产物**逐字节不变** ⇒ ⇒ 既有 golden 零影响。

### P2 — 契约化（两份 capability · 各司其职）

- `openspec/specs/rtl-hierarchical-output/spec.md`（**产物端口命名**的规范所有者）⇒ ⇒ 追加一条 Requirement ⇒ ⇒ ⚠️ **条款正文见唯一落地文本** ⇒ ⇒ **唯一落地文本 = `.rddf/checks/decision-request-port-naming-parity-2026-11.md` §2 R1-a** ⇒ ⇒ 该 R1-a 已含 ① 「顶层 module 端口表**全体** MUST 可解析到 C++ 成员名（manifest `cpp_member`）」+ ② 「clock/reset 基础设施端口 MUST NOT 被要求 `cpp_member`」+ ③ 「未显式命名者 MUST 走兼容去重路径 + 登记棘轮豁免表」+ ④ 「字面同名 SHOULD 的成立条件 / 不可能条件」⇒ ⇒ ⇒ **本项 MUST NOT 再复述条款正文**（否则又造一份副本 ⇒ ⇒ 漂移）
- `openspec/specs/component-ownership-and-naming/spec.md`（**节点 / 实例命名**的规范所有者）⇒ ⇒ 追加：「节点名的**前缀拼接**（`src/core/lnodeimpl.cpp:30-35` 与 `include/component.h:266-279`）MUST 口径**一致**」（⚠️ **2026-11-11 · critic 第 3 轮 BLOCKER-2 修复**：原此处的「空名哨兵 `"unnamed"` 的语义 MUST 明写」**已删除** ⇒ ⇒ 理由 = metis finding 15（不可违反的 MUST = AI slop）+ 姊妹草案 `:141` 明令禁加 ⇒ ⇒ ⇒ 两草案现**一致**）
- ⇒ ⇒ ⚠️ **禁**新立 capability（metis：仓内已有 4 个相关所有者 ⇒ 再立第 5 个 = 制造第 5 份规范副本 ⇒ ⇒ 且与 `hierarchical-port-naming-guard` Purpose 第 8 行「MUST NOT 复述」的纪律正面冲突）。
- ⇒ ⇒ ⚠️ **禁**立「MUST 字面相同」这种**当前不可满足**的条款（自检：每条 Requirement MUST 有**可构造的违反样例**）。

### P3 — route (c) 可行性裁定包（只出结论）

- 产出 `.rddf/checks/decision-request-port-name-literal-parity-2026-11.md`（沿用仓内既有 3 份 `decision-request-*.md` 的形态）⇒ ⇒ 6 项内容见 §范围 P3。
- ⚠️ **「不实施」的机械判据**：该 change 的 `git diff --stat` **不含 `src/**` 与 `include/**`** ⇒ ⇒ 防「裁定包顺手把实现做了」。
- ⚠️ 本项**不实施** P3 的结论 ⇒ ⇒ 结论落地是**独立 change**（architect → builder）。
- ⚠️ **选项清单 MUST 含「路线 ④ F-D」**（oracle 建议 5 · metis 与原稿均漏列）⇒ ⇒ **「把根实例名变成产品路径的参数」**：新建 CLI `cpphdl-emit-rtl` 用**空名**构造根组件（`include/device.h:28` 的 `"top"` **不动**）⇒ ⇒ 不改 `build_verilog_name_table`（`codegen_naming.h:71-89` 签名不变）⇒ ⇒ 「扁平/层次化顶层端口名**逐字相同**」**自动成立**（两发射器共用同一张表：`codegen_verilog.cpp:69` + `hierarchy_analysis.cpp:197`）⇒ ⇒ 不改扁平 module 名（`codegen_verilog.h:41` 的 `kFlatModuleName = "top"` 与根实例名是两个独立常量）⇒ ⇒ Verilator 的 `Vtop` 约定**不受影响**。
  - ⚠️ **planner 复核订正（2026-11-10 · 实测）**：oracle 称 F-D 是「**零 churn** 兑现字面同名」⇒ ⇒ **该说法不完整** ⇒ ⇒ 实测：<br>（a）**保留** `"unnamed"` 哨兵 ⇒ ⇒ 根端口名 = `unnamed_prog_addr` ⇒ ⇒ **不是**字面同名 ⇒ ⇒ F-D **白干**（与今天的探针 ① ② 同款）；<br>（b）**拆掉**哨兵 ⇒ ⇒ 根端口名 = 裸 `prog_addr`（字面同名 ✓）⇒ ⇒ **但**根组件**自有成员节点**的 Verilog 名同步变裸（实测 `unnamed.counter` → `top.counter` 对照 ⇒ 拆哨兵后为裸 `counter`）⇒ ⇒ ⇒ **违反既有已签批 Requirement** `openspec/specs/component-ownership-and-naming/spec.md:62`「其 Verilog 名 MUST 含该组件的路径段」⇒ ⇒ ⇒ **F-D 必须连该 Requirement 一起 MODIFIED** ⇒ ⇒ ⇒ **F-D 不是零 churn**。<br>⇒ ⇒ P3 的选项清单 MUST 把 F-D 写成「**零产物 churn · 但需 MODIFIED 一份已签批 spec**」⇒ ⇒ **禁**写成「零成本路径」。
- ⚠️ **P3 的「不做」选项 MUST 写成「已评估 · 当前不做 · 触发条件」**（形态同残差登记表）⇒ ⇒ **禁**写「永久放弃」（用户诉求仍在案 ⇒ ⇒ 写「永久放弃」= 替用户关掉诉求）⇒ ⇒ 触发条件 = **用户在看到 manifest 产物后仍要求人眼可读**。

### P4 / P5 — 登记与澄清

- P4：若 P3 裁定「做」⇒ ⇒ 登记走 `public-api-remove-visibility-2026-11`（`.rddf/checks/public-api-surface.md` + `docs/CHANGELOG.md` + ADR-061 修订）。
- P5：在 `port-naming-ratchet-2026-11.md` 的 **N4** 与 **AC** 处**回填**对齐结论（见 §待裁决 ② 方案 A），并注明「第 1 步**不依赖**本项 ⇒ ⇒ B1 批**不阻塞**」。

---

## Acceptance

### 探针固化（P1）

- [ ] `tests/test_port_name_parity.cpp` 存在且注册进 CTest；`bash scripts/verify_test_counts.sh` 绿
- [ ] `Ratchet*` ①：`__io` 内具名构造 ⇒ 节点名 `top.prog_addr` 且 Verilog 名 `top_prog_addr`
- [ ] `Ratchet*` ②：根实例名置空 ⇒ 节点名 `unnamed.prog_addr`（**不是** `prog_addr`）
- [ ] `Ratchet*` ③：同名兄弟 ⇒ 端口名含 `top_dup_prog_addr` 与 `top_dup_prog_addr_1`
- [ ] `Device*` ④：`ch::ch_device<T>` 路径 ⇒ 与 ① 的直接构造**等价**（覆盖 `include/device.h:28` 的**真实交付路径**）
- [ ] 退化用例 ⑥：根无名 + 无端口 ⇒ 不崩且命名表为空
- [ ] 🔴 **oracle 建议 3 硬要求 ①**：①④ MUST **断言发射出的 module 文本**（`ch::verilogwriter` + `print(ostream)`）⇒ ⇒ 样板见 `tests/test_verilog_arith.cpp:29-34` / `tests/test_verilog_gen.cpp:66-72` ⇒ ⇒ **命名层与产物层各有自己的变异**
- [ ] 🔴 **oracle 建议 3 硬要求 ②**：三条 `Ratchet*` **各有正向变异**（① 改匿名 / ② 改 fallback / ③ 强制 counter 恒 0）⇒ ⇒ **必须转红** ⇒ ⇒ 还原后**重建该 target** 再跑 ⇒ ⇒ **必须复绿**（根 `AGENTS.md` COMMANDS 段「还原源码 ≠ 复原」⇒ ⇒ 禁「删断言」式假变异）
- [ ] fixture **只含本用例所需节点**（exact-string 断言的顺序敏感性已在用例注释中登记）
- [ ] **零产物变更**：既有 `.v` golden 与 `scripts/verify_hierarchical_rtl.sh` 全绿且产物**逐字节不变**
- [ ] ⚠️ **⑤（目标钉）不进测试文件** ⇒ ⇒ 改登记进 `.rddf/checks/arch-residual-registry.md`（含解除条件 + 触发条件）⇒ ⇒ **登记表与台账 §7 MUST 同批**（双向 1:1）⇒ ⇒ `bash scripts/verify_arch_ledger_residuals.sh` 绿

### 契约化（P2）

- [ ] `rtl-hierarchical-output` 与 `component-ownership-and-naming` **各**追加 Requirement（**两份 · 各司其职** ⇒ ⇒ **不新立 capability**）
- [ ] 🔴 **oracle 建议 2 的集合口径**：① `rtl-hierarchical-output` 的 **MUST 覆盖 T1**（顶层 module 端口表**全体**）· **字面同名写成 SHOULD 且限定 T2**（`is_top_io == true`）· **不可能条件写成 MUST NOT** ⇒ ⇒ **禁**写「MUST 字面相同」
- [ ] 契约里**无**「MUST 字面相同」这类**当前不可满足**条款（自检：每条 MUST 有**可构造的违反样例**）
- [ ] ⚠️ **只引用 `rtl-emit-portability` 的字段语义**（`cpp_member` / `is_top_io` / `owner` / `driven` / `effective`）⇒ ⇒ **MUST NOT** 在两份 spec 里重定义（否则又造一份 schema 副本）
- [ ] `bash scripts/verify_openspec_delta.sh` 绿；`openspec validate --specs`（**非 strict**）**不新增**红项
- [ ] `hierarchical-port-naming-guard` **MUST NOT** 复述命名规则（沿用既有裁定）

### 裁定包（P3）

- [ ] `.rddf/checks/decision-request-port-name-literal-parity-2026-11.md` 存在，且含 6 项：① 最小改动集（**精确到 file:line**）② **唯一性论证 + 碰撞检测策略**（三选一）③ 影响面**量化**（**口径 + 数字**）④ 公开面影响清单 ⑤ **「不做」选项 + 代价** ⑥ 裁决者 + 触发条件
- [ ] 影响面量化的**枚举源**已写明（枚举哪些设计 / 哪些 golden 文件 · 清点命令写进 AC）
- [ ] ⚠️ **「不实施」的机械判据**：该 change 的 `git diff --stat` **不含 `src/**` 与 `include/**`**
- [ ] 同名兄弟在**层次化**侧的后果（A.1⑥ · `hierarchy_verilog.cpp:247` 疑似静默覆盖）已**登记为待查项**

### 残差登记

- [ ] 「同名兄弟 ⇒ 当前政策下必然 `_N`（切 `kRejectDuplicates` 后变为抛异常）」+「层次化侧同名兄弟疑静默覆盖」登记进 `.rddf/checks/arch-residual-registry.md`；`bash scripts/verify_arch_ledger_residuals.sh` 绿

### 不回归

- [ ] 既有 ctest 全绿；`CTEST_TOTAL` 锚点同步（**+1** ⇒ **禁**写死「257」）
- [ ] `scripts/verify_hierarchical_rtl.sh` 全绿且产物**逐字节不变**
  - ⚠️ **口径登记**：该脚本的实际判据集合是 **R1–R9**（含 R1a）⇒ ⇒ 而 `openspec/specs/rtl-hierarchical-output/spec.md:20` 写 **R1–R7**、根 `AGENTS.md:304` 写 **R1–R8** ⇒ ⇒ **三处口径漂移** ⇒ ⇒ **本项不修**（超出范围）⇒ ⇒ 登记为残差
- [ ] 既有 7 份 improvement 草案**零改动**（除 P5 的一行回填）

---

## Capabilities

| capability | 动作 | 说明 |
|---|---|---|
| `rtl-hierarchical-output` | **ADDED** | 追加**一条** Requirement —— ⚠️ **条款正文见唯一落地文本**（**本行不引条款原文**）⇒ ⇒ **唯一落地文本 = `.rddf/checks/decision-request-port-naming-parity-2026-11.md` §2 R1-a** ⇒ ⇒ ⇒ ⚠️ **2026-11-11 · critic 第 4 轮 BLOCKER-B**：Capabilities 表**正是 delta 的取材处** ⇒ ⇒ ⇒ 若本行留引文 ⇒ ⇒ 实施方取材时会拿到**缺 A/C 类豁免的旧版** ⇒ ⇒ ⇒ 常态违约 MUST 会原样进 delta ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ 故本行**连引文都不留**（只留指针）⇒ ⇒ ⇒ ⚠️ **判别规则**（供后续维护）：**凡 Capabilities 表的「内容」列 ⇒ ⇒ MUST NOT 引条款原文 ⇒ ⇒ 只许写指针**<br>⚠️ **本 capability 只管「产物端口命名」**（`spec.md:30` 的边界自陈）⇒ ⇒ **节点 / 实例命名的构造归下条** |
| `component-ownership-and-naming` | **ADDED** | 追加「节点名的**前缀拼接**（`src/core/lnodeimpl.cpp:30-35` 与 `include/component.h:266-279`）MUST 口径**一致**」<br>⇒ ⇒ ⚠️ **2026-11-11 · critic 第 3 轮 BLOCKER-2**：原此处的「+「空名哨兵 `"unnamed"` 的语义 MUST 明写」」**已删除**⇒ ⇒ 理由 = metis finding 15（不可违反的 MUST = AI slop）+ 姊妹草案 `:141` 明令禁加 ⇒ ⇒ ⇒ 两草案现**一致**（只作事实登记）<br>⇒ ⇒ **该 capability 的 Purpose 已声明「只管名字与归属」** ⇒ ⇒ 落点正确 |
| `hierarchical-port-naming-guard` | **不动** | 沿用既有裁定：guard capability MUST NOT 复述命名规则 |
| `core-ctx-node-name-uniqueness` | **不动**（引用） | 「同名 ⇒ 报错」策略的归属不变 ⇒ ⇒ 本项只是把它作为**政策二选一**的一支记录 |
| ~~`port-name-parity-contract`（新）~~ | ❌ **撤回** | ⚠️ **metis 订正**：仓内已有 4 个相关所有者 ⇒ ⇒ 再立第 5 个 = 制造第 5 份规范副本 ⇒ ⇒ 且与自身的 MODIFIED 行**逐条同义** ⇒ ⇒ **撤回**，改为对既有两份 capability 的 MODIFIED |

---

## Impact

### 爆炸半径

| 面 | 影响 |
|---|---|
| `tests/test_port_name_parity.cpp`（新） | **零生产面改动** ⇒ ⇒ 产物逐字节不变 ⇒ ⇒ golden 零影响 |
| `openspec/specs/rtl-hierarchical-output/spec.md` + `openspec/specs/component-ownership-and-naming/spec.md` | 各追加 Requirement ⇒ ⇒ **两份** ⇒ ⇒ 需 architect 复核（各自是规范所有者） |
| `include/component.h` / `src/component.cpp` / `src/core/lnodeimpl.cpp` | ⚠️ **本项不改** ⇒ ⇒ 改动只在 P3 的**裁定结论**里描述 ⇒ ⇒ 真正实施是**独立 change** |
| 全仓 `.v` 产物 | ⚠️ **若**将来实施 route (c) ⇒ ⇒ **每个设计的根端口名都变** ⇒ ⇒ golden 全量更新 ⇒ ⇒ （**本项不触发**） |
| 计数锚点 | `CTEST_TOTAL` **+1**（P1 新增 1 个 ctest）⇒ ⇒ 同步 |
| `ADR-061` | ⚠️ 若实施 route (c) ⇒ ⇒ 「下游按 `node->name()` 精确匹配」需一并修订 ⇒ ⇒ 本项**只登记**（P4） |

### 已知风险

| 风险 | 缓解 |
|---|---|
| **P1 的现状钉 ⇒ 未来「修好了」反而转红** | ⚠️ ①②③ 是**现状钉**，**不是**目标 ⇒ ⇒ MUST 改名为 `Ratchet*` + 文件头注明「route (c) 落地时 MUST 被改写为 `Goal*`」⇒ ⇒ 目标语义**另立** `Goal*`（当前 `SKIP()` + 解除条件）⇒ ⇒ 改写 MUST 是**独立 commit** 且 MUST 登记 |
| **P1 的变异做成「删断言」式的假变异** | ⇒ ⇒ MUST 是**正向变异**（改实现 ⇒ 必红）⇒ ⇒ 三条各自的正向变异已在 AC 中写死 |
| **P2 的 Requirement 被写成不可满足条款** | 自检：每条 MUST 有**可构造的违反样例** ⇒ ⇒ 写不出样例的 MUST = 不可满足 ⇒ ⇒ 禁写 |
| **P3 变成「又一个方案文档」而不裁决** | ⇒ ⇒ P3 落到 `decision-request-*.md`（仓内既有形态）⇒ ⇒ MUST 含「**不做**选项 + 代价」⇒ ⇒ 并由 architect 择一 ⇒ ⇒ 且 `git diff --stat` 判「不实施」 |
| **P3 的成本估算失真**（metis：原稿「一行特判」低估） | ⇒ ⇒ P3 MUST 含**唯一性论证 + 碰撞检测策略** ⇒ ⇒ 缺任一项 ⇒ ⇒ P3 **不算完成** |
| **探针留在 scratch 不进仓 ⇒ 结论随会话蒸发** | ⇒ ⇒ P1 是**最低交付** ⇒ ⇒ 未做 P1 ⇒ ⇒ 本项**不算完成**（如实登记） |
| ~~**在用户语义（T1 / T2）裁决前就写 P2 的措辞**~~ | ✅ **已消解**（oracle 建议 2）⇒ ⇒ **MUST 集合恒为 T1**（两种解读下都成立）⇒ ⇒ **SHOULD 的适用范围由 `is_top_io` 字段界定** ⇒ ⇒ ⇒ **P2 的措辞不再依赖用户语义** ⇒ ⇒ ⇒ 本行从风险表**删除**（不再阻塞） |

### 与其他 improvement 的关系

| 项 | 关系 |
|---|---|
| `port-naming-ratchet-2026-11` | **上游**（命名规则 + N5 第 1 步）⇒ ⇒ 本项**不阻塞**它 ⇒ ⇒ **但**两者的 Requirement 措辞 MUST 二选一（§待裁决 ② · 方案 A）⇒ ⇒ P5 负责回填对齐 |
| `rtl-emit-product-tooling-2026-11` | **上游**（manifest schema：`cpp_member` + `is_top_io`）⇒ ⇒ 本项 P2 的「机器可解析对应 + T1/T2 区分」依赖它 ⇒ ⇒ **同一批（B1）**落地 ⇒ ⇒ ⚠️ **本行是「依赖关系」描述，不是条款副本** ⇒ ⇒ 条款正文仍**只在** R1-a 一处 |
| `examples-rtl-sim-coverage-2026-11` | 无耦合（TB 生成器用 manifest 的 `cpp_member` ⇒ ⇒ 本项 P2 落地后 TB 端口注释可读） |
| `rv32i-dead-component-consolidation-2026-11` | 无耦合 |
| `memory-timing-contract-2026-11` | 无耦合 |

---

## ✅ 裁决记录（2026-11-10 · oracle 建议 1/2/3 · **用户已点头**）

> ✅ **三项全部已裁**（用户 2026-11-10「按这个顺序推进」）⇒ ⇒ **本项的待裁决项清零** ⇒ ⇒ P2 可定稿。

### 裁决 ① —— 「顶层端口」口径：**MUST 覆盖 T1 / 字面同名 SHOULD 限定 T2**（oracle 建议 2）

- 判别字段 = **`is_top_io`**（**不新造判据**）✅
- **MUST（机器可解析对应）集合 = T1**（顶层 module 端口表**全体**）⇒ ⇒ 理由：T1 里那 80 个 inline 后代输出**同样在 .v 里**（契约要求）⇒ ⇒ 若 MUST 只覆盖 T2 ⇒ ⇒ **覆盖面漏洞**（恰是用户嫌吵的那批口）
- **SHOULD（字面同名）集合 = T2**（`is_top_io == true`）⇒ ⇒ 只对「用户声明的接口」谈「C++ 侧同名」才有语义
- ⇒ ⇒ **T1/T2 不再阻塞 P2** ⇒ ⇒ §Impact 风险表里「裁决前 P2 不得定稿」**已删除**

### 裁决 ② —— 与 `port-naming-ratchet` N4 / AC:159 的冲突：**方案 A + 三条加固**（oracle 建议 1）

- **加固 ①（补 F-C 的洞 · oracle 根因 F-C）**：框架**无反射**（`__io` 生成匿名 struct · `include/core/io.h:319-329`）⇒ ⇒ 运行期唯一存活的是**显式命名串** ⇒ ⇒ **`cpp_member` 只能是「显式命名串」**，不是 C++ 成员标识符本身 ⇒ ⇒ ⇒ 「可解析到 C++ 成员名」**必须**由**静态护栏**强制「标识符 == 名字串」⇒ ⇒ ⇒ 否则方案 A 只是把「不可满足的 MUST」换成「**不可验证的 MUST**」⇒ ⇒ ⇒ **护栏 P1 MUST 新增该判据**（`scripts/verify_port_naming.sh` 的扫描面已拿到声明文本 ⇒ 加一条 `ident == "ident"` 比对 ⇒ ⇒ **零新增成本**）
- **加固 ②（订正代价）**：P3 的「唯一性论证」= **对既有不变式的文档化**（E2 已证伪「碰撞概率猛增」）⇒ ⇒ route (c) 成本估算**下调**
- **加固 ③（字面同名的正确归宿）**：「字面同名」**不作 MUST** ⇒ ⇒ 改为 `rtl-hierarchical-output` 内一条带前提的 **SHOULD** + 一条「**不可能条件**」⇒ ⇒ **用户诉求不丢**（写进规范）⇒ ⇒ **不制造一条常态违约的 MUST**
- ⇒ ⇒ **route (c) 的归宿**：「**已评估 · 当前不做 · 触发条件 = 用户在看到 manifest 产物后仍要求人眼可读**」⇒ ⇒ **禁**写「永久放弃」
- ⇒ ⇒ **不选方案 B**（会把 B1 整批阻塞在一个纯命名问题上）

### 裁决 ③ —— P1 用例范围：**①②③④⑥ 一次搬入；⑤ 不作 SKIP ⇒ 转残差登记**（oracle 建议 3）

- ①②③④⑥ 一次搬入（与 roadmap B1 表一致）
- **⑤ 转 `.rddf/checks/arch-residual-registry.md`**（残差登记有 fail-closed 护栏 ⇒ ⇒ **会主动催办**；SKIP 不会 ⇒ ⇒ 违反「未运行的验证不可判」）⇒ ⇒ ⚠️ **连带约束**：残差登记与台账 §7 是**双向 1:1** ⇒ ⇒ **ledger 那侧属 architect 域**（`docs/architecture` 在 `arch_paths` 内）⇒ ⇒ ⇒ **本项 MUST NOT 单改登记表**（会打破护栏）⇒ ⇒ ⇒ **A7 的耦合落点见 §待裁决 ④**
- **两条 AC 硬要求**：①④ MUST 断言**发射出的 module 文本**（不只命名表 —— 「有名字 ≠ 是顶层端口」）· 变异自证 MUST 含**重建**步骤（「还原源码 ≠ 复原」）

### ⚠️ 待用户裁决（**0 项** · ✅ 2026-11-11 全部闭合）

4. ✅ **已落地（2026-11-10 · architect 角色 · commit `ba99793`）—— A7 的落点（残差登记）**：ledger 侧（台账 `docs/architecture/2026-11-02-hierarchical-rtl-output.md` §7 **行 21 / 21b**）与 planner 侧（登记表 `.rddf/checks/arch-residual-registry.md` 末尾 2 行）**两侧同批**落地 ⇒ ⇒ 护栏 `scripts/verify_arch_ledger_residuals.sh` **PASS（22 行）** ⇒ ⇒ ⇒ 原「🔴 Blocked · 待切 architect」已解除
   - 现状：残差登记表 `.rddf/checks/arch-residual-registry.md` 与台账 `docs/architecture/2026-11-02-hierarchical-rtl-output.md` §7 是**双向 1:1** 耦合（护栏 `scripts/verify_arch_ledger_residuals.sh`，**已落两侧后现状 = PASS / 22 行**）⇒ ⇒ **单改登记表 ⇒ 护栏红** ⇒ ⇒ **必须两侧同批**
   - `docs/architecture` 在 `.rddf/config/arch-docs.json` 的 `arch_paths` 内 ⇒ ⇒ **ledger 侧属 architect 域** ⇒ ⇒ planner **不越权**改它
   - ⇒ ⇒ **选项**：(a) 切 **architect** 角色做 ledger 侧 + planner 补登记表侧（**一次同批**）· (b) 本项**暂缓** A7 ⇒ ⇒ 两条残差先只留在本草案文本里（AC 里已写「MUST 登记」⇒ ⇒ 未登记 ⇒ **本项不算完成** ⇒ ⇒ 如实登记）
   - ⇒ ⇒ **⚠️ 登记内容（两条合并为一条 · 避免双向 1:1 护栏被双重登记打红）**：
     - **残差 1** = 「route (c)（含**方向 (i)**：让 `lnodeimpl` 也剥哨兵 ⇒ ⇒ 撞 `component-ownership-and-naming:62` ⇒ ⇒ 需 architect）⇒ ⇒ **已评估 · 当前不做 · 触发条件 = 用户在看到 manifest 产物后仍要求人眼可读**」
     - **残差 2** = 「⑤ 目标钉（根端口裸名）⇒ ⇒ 随残差 1 一并处置」
     - ⇒ ⇒ ⚠️ **`node-name-prefix-consistency-2026-11`（F-A）MUST NOT 新增行** —— 它承载的是**方向 (ii)**（已在本项定为实施方向）⇒ ⇒ 方向 (i) 由本条覆盖 ⇒ ⇒ ⇒ **两草案共用一条残差**
    - ⇒ ⇒ ✅ **已采纳 (a) 并已执行**（2026-11-10 · architect 角色 · commit `ba99793`）⇒ ⇒ 改动是机械的：两侧各加 2 行 ⇒ ⇒ 护栏双向 1:1 ⇒ ⇒ 立即可验 ⇒ ⇒ **已 PASS（22 行）**
    - ✅ **补丁已端到端验证（2026-11-10 · planner 在临时 root 上实跑 `scripts/lib/ledger_residual.py check`）**：两侧同批 ⇒ ⇒ **PASS（22 行）** ⇒ ⇒ ⇒ ⚠️ **2026-11-11 · critic 第 3 轮 SHOULD_FIX-4 修复**：原写「行文本与落点如下（**逐字可执行**）」⇒ ⇒ **已执行** ⇒ ⇒ 但 architect 落地时把台账行的「撞本表 `:62`」微调为「撞本文件 `component-ownership-and-naming` 的「Verilog 名 MUST 含该组件的路径段」」⇒ ⇒ ⇒ 故下方文本**不再是逐字可执行**，而是**已落地文本的留档** ⇒ ⇒ ⇒ **权威文本 = 磁盘上已落地的两行**（台账 `:299-300` + 登记表末尾 2 行）
     - **登记表**（`.rddf/checks/arch-residual-registry.md` · 追加到**表格末尾**）：
       ```
       | docs/architecture/2026-11-02-hierarchical-rtl-output.md | 7 | 21 | 未决 | 2026-11-10 | route (c) 字面同名当前不做 · 触发条件 = 用户在看到 manifest 产物后仍要求人眼可读 | — | 用户在看到 manifest 产物后仍要求人眼可读 ⇒ ⇒ 重开 route (c) 裁定 | architect |
       | docs/architecture/2026-11-02-hierarchical-rtl-output.md | 7 | 21b | 未决 | 2026-11-10 | 根端口裸名目标钉 · 随 route (c) 一并处置 | — | route (c) 重开时一并处置 | architect |
       ```
     - **台账**（`docs/architecture/2026-11-02-hierarchical-rtl-output.md` §7 的表内 · ⚠️ **必须插进 §7 表内**，追加到文件末尾**不会被解析** ⇒ ⇒ 会报「登记表有台账无」）：
       ```
       | 21 | **route (c)「字面同名」路线** —— 顶层端口名与 C++ 成员名逐字相同（根组件端口 + 实例名唯一 + 全设计 base name 不撞 + 成员名为合法 Verilog 标识符）⇒ ⇒ 探针实测：根实例名置空 ⇒ 得 `unnamed_prog_addr`（**不是**裸名）⇒ ⇒ N5(c) 的回退条件已被证伪；同名兄弟实例 ⇒ ⇒ 必然吃 `_N` ⇒ ⇒ 「全体组件字面同名」在当前扁平发射器下不成立；方向 (i)（让 `lnodeimpl` 也剥哨兵）⇒ ⇒ 撞本表 `:62`「Verilog 名 MUST 含该组件的路径段」⇒ ⇒ 需 architect 裁 | ⚠️ 未决 ⇒ route (c) 字面同名当前不做 · 触发条件 = 用户在看到 manifest 产物后仍要求人眼可读 |
       | 21b | **根端口裸名的目标钉**（「根组件端口的 Verilog 名 == C++ 成员名」）⇒ ⇒ 随行 21 一并处置 ⇒ ⇒ 判据与探针见 `.rddf/improvements/port-name-literal-parity-2026-11.md` | ⚠️ 未决 ⇒ 根端口裸名目标钉 · 随 route (c) 一并处置 |
       ```
     - ⚠️ **格式约束**（读 `scripts/lib/ledger_residual.py` 得出）：`未决` 行 **MUST** 登记**复核条件**（不能为空）· `闭口证据` 填 `—`（⇒ `_split_evidence("—")` 返回空 ⇒ 不触发「候选证据已存在」红）· 后缀列与台账状态格的后半段 MUST **逐字相同**（规范句派生）⇒ ⇒ ⇒ `未决` **不在** `EVIDENCE_REQUIRED`（仅 `已收口`/`已裁决` 需证据）

---

## metis 第 1 轮 gap 分析与处置（2026-11-10）

> **轮次**：1/1（该 improvement 总计 1 轮）· **审查者**：metis（预规划 gap 分析）
> ⚠️ **本节是过程日志，不是决策** —— 决策在正文各节的「⚠️ metis 订正」标注处。
> ⚠️ **原稿曾有一节「metis 结论」由 planner 预写** ⇒ ⇒ metis 判定为**自我盖章** ⇒ ⇒ **整段删除**，本节为 metis 的**真实输出**与处置。

### ✅ 已订正（按 metis 的 6 个检查面）

| # | 检查面 | metis finding | 处置 |
|---|---|---|---|
| 1 | 隐藏意图 | 🔴 **与上游正面冲突**：`port-naming-ratchet:139`/`:159` 已写「顶层端口名 MUST 与 C++ 侧显式命名**逐字相同**」（AC 写 `prog_we` ⇒ `prog_we`）⇒ ⇒ 本项原 P2「**禁**立 MUST 字面相同」⇒ ⇒ **两份同期草案对同一 spec 提交互相矛盾的 Requirement** | ✅ 升为 §待裁决 ②（方案 A / B）⇒ ⇒ **采纳方案 A**（metis 推荐）⇒ ⇒ P5 负责回填 ratchet 的 N4 + AC ⇒ ⇒ 原稿 P5「不重叠、不阻塞」的**虚假声明已删除** |
| 2 | 隐藏意图 | 🔴 「用户说的『顶层端口』是哪个集合**没有证据**」⇒ ⇒ 原稿 W.1「正好落在可兑现的那一半」是**未经验证的推断** | ✅ 新增 §A.0（**T1 / T2 两集合**）⇒ ⇒ W.1 已改写 ⇒ ⇒ 「用户语义」列为**待裁决 ①** ⇒ ⇒ P2 的措辞**依赖**该裁决（已写入 Impact 风险表） |
| 3 | 隐藏意图 | 🟡 真实意图可能是「让下游工具不必猜名字」（`ADR-061:32` 用 `node->name()` **精确匹配**）⇒ ⇒ manifest 路线 ① 已闭合 ⇒ ⇒ route (c) 的收益只剩「人眼友好」 | ✅ §A.3 路线 ① 的结论已写明「它不要求 RTL 名 == C++ 名，只要求 manifest **记录对应关系** ⇒ ⇒ 对所有组件都成立」⇒ ⇒ route (c) 的收益已收窄并**明示**为「人眼友好」 |
| 4 | 歧义 | 🔴 **「字面同名」的对照物是哪一级名字**？`port::name()`（`"unnamed_input"`）/ IR 节点名（`top.prog_addr`）/ Verilog 端口名（`top_prog_addr`）三者不同 ⇒ ⇒ 原 P2 未指定 ⇒ ⇒ 无法写出可判读的 Requirement | ✅ P2 已逐层写明：产物端口名（Verilog）↔ IR 节点名 ↔ C++ 成员名（`cpp_member`）三层，且**明示** `port::name()` 恒为 `"unnamed_input"`（不是可比对象） |
| 5 | 歧义 | 🟡 「两处拼接不对称」实为**三处** | ✅ A.1③ 已改为**三处**（`component.cpp:66` · `component.h:274` · `lnodeimpl.cpp:30-35`），并逐条标号 |
| 6 | 歧义 | 🟡 护栏口径 **R1–R7 vs R1–R8 vs 脚本实际的 R1–R9** | ✅ 已核：脚本实含 **R1–R9 + R1a**；`spec.md:20` 写 R1–R7；`AGENTS.md:304` 写 R1–R8 ⇒ ⇒ **三处漂移** ⇒ ⇒ Acceptance 以**脚本为准**并**登记为残差**（本项不修） |
| 7 | 歧义 | 🟡 `"unnamed"` 是**哨兵**还是合法名字？（用户把组件真命名为 `"unnamed"` 会意外命中特判） | ✅ **哨兵语义只作事实登记**（§A.1 + §已知风险）⇒ ⇒ ⚠️ **2026-11-11 · critic 第 3 轮 BLOCKER-2**：原处置写「已写入 P2 的 `component-ownership-and-naming` Requirement（「空名哨兵的语义 MUST 明写」）」⇒ ⇒ **该 MUST 已删除**（不可违反 ⇒ AI slop）⇒ ⇒ ⇒ 现处置 = 与姊妹草案 `:251` **一致**（只作事实登记，不进 MUST） |
| 8 | AI slop | 🔴 **自我盖章**：原稿预写了一整节「metis 结论」并自称「已逐条落地」，且其中一条声称「§A.1⑥ 已改成『根组件的端口』」—— 而 A.1⑥ 原文**并无**该措辞 | ✅ **整段删除** ⇒ ⇒ 本节为 metis 的**真实输出** ⇒ ⇒ 逐条处置已落到正文（可用 `grep "根组件"` 复核） |
| 9 | AI slop | 🔴 新 capability `port-name-parity-contract` 是**第 5 个命名所有者**，且与自身的 MODIFIED 行**逐条同义** ⇒ ⇒ 与 `hierarchical-port-naming-guard` 的「MUST NOT 复述」纪律正面冲突 | ✅ **撤回该 capability** ⇒ ⇒ 改为对 `rtl-hierarchical-output` + `component-ownership-and-naming` **各**追加（**一份 Requirement · 一处落点**） |
| 10 | AI slop | 🟡 「结构性不可能」是**政策依赖**的结论，被写成**物理事实** | ✅ A.1⑦ / §不在本项 已改为「**当前政策下**必然 `_N`；若切 `kRejectDuplicates` ⇒ 结局变为**创建点抛异常**」⇒ ⇒ 两条政策**显式二选一** |
| 11 | AI slop | 🔴 「层次化路径用 `owner.port` ⇒ 天然无歧义」是**自发明事实** —— 层次化**复用扁平全局表** | ✅ **已删** ⇒ ⇒ A.1⑥ 改为实测事实（`hierarchy_analysis.cpp:195-201` 自陈「顶层端口名与扁平产物逐字相同」）⇒ ⇒ 并新增「层次化侧同名兄弟 ⇒ `hierarchy_verilog.cpp:247` 疑静默覆盖」为**待查项** |
| 12 | AI slop | 🟡 P3 的「最小改动集 = 一行特判」是**低估** —— 上游 N5 已显式警告「先计数再剥**不保证唯一**」 | ✅ W.3 已改写 ⇒ ⇒ P3 **MUST** 含**唯一性论证 + 碰撞检测策略**（三选一）⇒ ⇒ 缺任一项 ⇒ P3 不算完成 |
| 13 | 缺失验收 | 🔴 无「与 ratchet N4/AC 已对齐」的判据 | ✅ 已加（§待裁决 ② + P5 回填） |
| 14 | 缺失验收 | 🔴 P2 无 openspec 侧**机械判据** | ✅ 已加：`bash scripts/verify_openspec_delta.sh` 绿 + `openspec validate --specs`（非 strict）**不新增**红项 |
| 15 | 缺失验收 | 🟡 P1 断言 ②③「恒绿 / 无判别力」⇒ ⇒ 只在**对未来改动敏感**时才有意义 ⇒ ⇒ 且变异自证的**必要条件**没写（必须含**正向变异**） | ✅ 已加：改名 `Ratchet*`（现状钉）+ 新增 `Goal*`（目标钉 · `SKIP()` + 解除条件）⇒ ⇒ 三条**正向变异**在 AC 中逐条写死（禁「删断言」式假变异） |
| 16 | 缺失验收 | 🟡 P1 的 exact-string 断言**对全局表顺序敏感** | ✅ 已加：fixture **只含本用例所需节点** + 用例内注释登记该依赖 |
| 17 | 缺失验收 | 🟡 P3「只出裁定不实施」**无落地载体** | ✅ 已加：产出 `.rddf/checks/decision-request-port-name-literal-parity-2026-11.md`（沿用仓内既有 3 份形态）⇒ ⇒ AC 六项内容 |
| 18 | 缺失验收 | 🟡 P3 的「影响面量化」**无口径** | ✅ 已加：AC 要求**枚举源 + 清点命令**写进 AC |
| 19 | 缺失验收 | 🟡 残差登记**无格式 / 校验判据** | ✅ 已加：`bash scripts/verify_arch_ledger_residuals.sh` 绿 |
| 20 | 缺失验收 | 🟢 计数只提 `CTEST_TOTAL +1` | ✅ 已加：`bash scripts/verify_test_counts.sh` 绿 |
| 21 | 缺失验收 | 🟡 P1 **无 `ch_device` 真实根路径覆盖**（固化的是仿真路径，不是交付路径） | ✅ 已加 `Device*` ④（`ch::ch_device<T>` 对照）+ 「等价的论证」写入 AC |
| 22 | 边界情况 | 🔴 **route (c) 后，根端口与根组件**直接拥有**的内部节点同名 ⇒ 静默 `_N`** ⇒ ⇒ 且打破 `ADR-061:34`「端口名 MUST NOT 与内部节点同名」 | ✅ 已写入 §边界情况（下列表）与 P3 的碰撞检测策略三选一 |
| 23 | 边界情况 | 🔴 **sanitize 非单射**（`codegen_naming.h:42-46` 把 `.` 也换成 `_`）⇒ ⇒ 去前缀后碰撞概率**猛增** ⇒ ⇒ A.1⑦ 的成立条件**不充分** | ✅ A.1⑦ 已补第 ③ 条前提「**全设计 base name 不撞**」⇒ ⇒ 并进入 P3 的碰撞检测策略<br>⚠️ **planner 复核订正（2026-11-10 · oracle F-B + 实测）**：metis 这条 finding 的**机理描述有误** —— 根前缀是碰撞条件的两边**公因子** ⇒ ⇒ 碰撞条件**剥前缀下不变** ⇒ ⇒ 「碰撞概率猛增」**不成立**（实测：今天 `top_prog_addr` 与 `top_prog_addr_1` **就已撞**）⇒ ⇒ **该风险是既有不变式、不是 route (c) 引入的** ⇒ ⇒ A.1⑦ 第 ③ 条前提**仍然成立**（它是必要条件）⇒ ⇒ 但 **route (c) 的成本估算 MUST 下调** ⇒ ⇒ E2 / W.3 已同步改写 |
| 24 | 边界情况 | 🟡 Verilog **关键字**与**数字开头**（`output` / `reg` / `1st` ⇒ `_r` / `_`）⇒ ⇒ 字面同名不可能 | ✅ 已并入 P2 的「C++ 成员名 MUST 本身是合法 Verilog 标识符」判据（沿用 ratchet N5）+ 可构造反例 |
| 25 | 边界情况 | 🟡 **顶层组件不是 `ch_device`**（`ch_module` 作顶层 ⇒ `Component::node_owner()` 为空 ⇒ `CHFATAL_EXCEPTION`）⇒ ⇒ route (c) 不能靠「无条件去前缀」处理 | ✅ 已写入 §边界情况（下列表）+ P3 的最小改动集须覆盖该形态 |
| 26 | 边界情况 | 🟡 同名兄弟在**层次化**产物里的真实后果**未被验证** ⇒ ⇒ 与「层次化天然无歧义」直接冲突 | ✅ 已改为**待查项**（A.1⑥ + P3 的 AC 项）⇒ ⇒ 不再声称「天然无歧义」 |
| 27 | 边界情况 | 🟢 **退化用例**（根无名 + 无任何 `ch_in` ⇒ 命名表为空） | ✅ 已加进 P1 的用例设计说明（退化用例不崩且表为空） |
| 28 | 边界情况 | 🟢 三探针**不可由 metis 复跑**（只在会话临时目录；metis 的 shell 白名单不允许 `g++`）⇒ ⇒ metis 用**源码复核**替代，结论逐条一致 | ✅ 已采纳：结论可信，但「已实测」在仓内**不可判** ⇒ ⇒ **P1 把探针逐字搬进仓内**是**最低交付**（已写入 Impact 风险表） |

### ⚠️ metis 未能独立验证的项（如实登记）

- metis 的 shell 白名单不允许 `g++` / 执行二进制 ⇒ ⇒ **三探针由 metis 用源码复核**（`component.cpp:66` · `component.h:266-279` · `lnodeimpl.cpp:31-35` · `codegen_naming.h:72-88`）⇒ ⇒ **结论与源码逐条一致** ✓ ⇒ ⇒ 但「已实测」这一措辞在**仓内**仍不可判 ⇒ ⇒ P1 未做 ⇒ ⇒ 本项不算完成。
