# Design: stream-ready-ownership-correction-2026-10

## 1. 背景与定位

⚠️ 本 change **不是**「给转发类加护栏」—— 那是 **P1**。
本 change 是**收口订正**：上一轮归档（`chlib-stream-ready-ownership-2026-10`）时，
主 spec 的条款**说得比事实多** ⇒ ⇒ 与实现**公开冲突**。

🔴 **冲突的具体形态**（P0 探针 `/tmp/opencode/p0/p0v2.cpp`，判定全 CONFIRMED）：

| # | 发现 | 实测 |
|---|---|---|
| ① | 🔴 **根因**：`ch_stream` 的字段**默认构造后是空节点**（`impl == nullptr`）⇒ `create_field_proxy_nodes()` 只在 `as_master()` / `as_slave()`（`set_role`）里被调用 ⇒ ⇒ 对空节点做值运算 ⇒ 「Invalid operands」；读它 ⇒ ABORT（`simulator.h:325`） | `s.valid impl=空节点(nullptr)` / `as_master() 后 s2.valid impl=有节点` |
| ② | 🔴 转发类（fork 非同步 / demux / filters ×6）ready 链**两端都断** | `Invalid operands` ×2/×6 ⇒ 结果字段 = 空节点 ⇒ **读即 ABORT 💥** |
| ③ | 🔴 **`stream_mux_connect_ready` 的 `<<=` 修复在「调用方未 `as_master`」时无效** ⇒ ⇒ `<<=` 在**空节点**上走 else 分支 ⇒ ⇒ **建了新节点** ⇒ ⇒ 只驱动 mux 自己的副本 ⇒ ⇒ 调用方被孤儿化 ⇒ ⇒ **而 `ctest` 194/194 全绿 ⇒ ⇒ 假绿** | 变体 A：`ins[0].ready impl=空节点(nullptr) srcs=-1`；变体 B：`srcs=1` ✔ |
| ④ | 🔴 ADR-054 §3.2 第 1 条的「field proxy」**理由错误**（条款仍成立） | 见 §3 |
| ⑤ | 🔴 `stream_arbiter_round_robin` 的 `current_channel` 是**死节点** ⇒ ⇒ `last_grant` 恒 0 ⇒ ⇒ **RR 从不轮转** | `stream.h:564` / `:581-582` |
| ⑥ | 🔴 filters 家族 6 处 `operator=` 孤儿化 | `stream.h:115/132/149/173/189/205` |

⇒ ⇒ **发现 ①③ 是本 change 的直接触发点**：主 spec `Requirement 3` 把
「`stream_mux_connect_ready` MUST 改为 `<<=`」**认证为合规修复**，而实测该修复
**在调用方未建节点时静默无效** ⇒ ⇒ ⇒ **spec 在说谎**。

## 2. 机制澄清（三条，逐条实测）

### 2.1 `<<=` 的三个分支（`include/lnode/bool.tpp:58-77`）

```cpp
template <typename U> inline ch_bool &ch_bool::operator<<=(const U &value) {
    lnode<U> src_lnode = get_lnode(value);
    if (src_lnode.impl()) {                     // ① 源非空
        if (this->node_impl_) {
            this->node_impl_->set_src(0, src_lnode.impl());   // ② 目标已有 ⇒ 覆盖
        } else {
            this->node_impl_ = node_builder::instance().build_unary_operation(
                ch_op::assign, src_lnode, 1, src_lnode.impl()->name() + "_wire");
            // ③ 目标为空 ⇒ 另建节点绑到 this ⇒ 原件仍是 null
        }
    } else {
        CHERROR("[ch_bool::operator=] Error: node_impl_ or src_lnode is null ...");
        // ④ 源为空 ⇒ 只打日志 ⇒ 静默无效
    }
    return *this;
}
```

⇒ ⇒ **四种形态** ⇒ ⇒ **三类不同的失效**（critic F2-Q1③）：

| 分支 | 后果 | 失效类型 |
|---|---|---|
| ② 目标已有 + 源非空 | **覆盖生效** ✔ | —— |
| ③ 目标为空 + 源非空 | **另建节点** ⇒ ⇒ 只驱动被 `<<=` 的对象 ⇒ ⇒ **调用方被孤儿化** | **静默** |
| ④ 源为空 | `CHERROR` 只打日志 ⇒ ⇒ 覆盖**不生效** | **静默**（实测 V2 CONFIRMED） |
| —— 目标被 `operator=` 重绑定成 literal | `set_src` 对 literal **无效**（design.md §13.1 E-1 实测：`out_lit` 恒为 1，对照组 mux 正常跟随） | **静默** |

⇒ ⇒ ⇒ **单条判据覆盖不了** ⇒ ⇒ 必须**分条**（R2 的「三个必要条件」+「三类失效
形态不同 MUST NOT 用单条判据覆盖」）。

### 2.2 源为空的实测（V2，2026-10-31）

```
=== V2：connect_ready 传入空 ready 字段 ===
   fifo.pop_stream.ready impl=有节点
   传入的空 ready impl=空节点(nullptr)
[ERROR] [ch_bool::operator=] Error: node_impl_ or src_lnode is null for ch_bool! at include/core/../lnode/bool.tpp:65
   connect_ready 后：threw=否
   [CONFIRMED] 当前**不抛异常** ⇒ ⇒ 源为空时是**静默**（CHERROR 只打日志）
   [CONFIRMED] ready 仍是默认驱动（未变）⇒ ⇒ 静默保持恒就绪（覆盖**未生效**）
```

⇒ ⇒ ⚠️ **守卫（`stream.h:432-446`）只查目标侧** ⇒ ⇒ 源侧**无保护** ⇒ ⇒
R2 MUST 补「源非空」这一必要条件。

### 2.3 串联场景（V3，2026-10-31）

```
=== V3：builder 串联 + 多驱动守卫 ===
   f1.pop_stream.ready impl=有节点 num_srcs=1
   对 f2 调 connect_ready：threw=否
   [CONFIRMED] 对**下游** f2 调 connect_ready **不误炸**（f2 自己的 ready 是默认驱动）
   [CONFIRMED] 上游 f1 的 ready 仍只有默认驱动（下游**未**驱动它 ⇒ ⇒ 上游反压**丢失**）
```

⇒ ⇒ ⚠️ **fifo 守卫在串联下**不**误炸**（critic F3-Q3 的推断**部分证伪**）
⇒ ⇒ 但**「上游反压丢失」被确认** ⇒ ⇒ 即 P1 台账说的「**静默丢字**」
⇒ ⇒ ⇒ **守卫统一规格**（critic F3-Q3 的裁决方向成立，但「误炸」的表述要改）：
统一规格 = **逐节点单一所有权 + 所有权转移 MUST 经 `<组合子>_connect_ready`**
⇒ ⇒ 串联 MUST 走上游结果的 `connect_ready`，MUST NOT 裸 `<<=` 跨级驱动
⇒ ⇒ mux 侧补同型守卫（当前 `stream_mux_connect_ready` **无**守卫）。

### 2.4 🔴🔴 **第 7 条发现**（实施期实测 · P0 六条之外）：`set_src` 改边在
###         `Simulator::initialize()` **之后不生效**

⚠️ **来源**：本 change 的**判别力用例**编写期（探针
`/tmp/opencode/p0/dbg_bisect4.cpp`）⇒ ⇒ **P0 探针只挖出 6 条** ⇒ ⇒ 这是
**第 7 条**。

```
tick1: out=1
tick2（改边后，未重新 initialize）: out=1     ← 覆盖【未生效】
tick3（新 Simulator 后）:            out=0 in0=0  ← 覆盖【生效】且传播 ✔
```

⇒ ⇒ **机制**：指令的 `src0_buf` 指针是在 `initialize()` 时**绑定**的
⇒ ⇒ ⇒ `set_src` 改的是**图**，**不重建指令** ⇒ ⇒ ⇒ **覆盖只在
`initialize()` 之前做才有效**。

⇒ ⇒ **它改变了 ADR-054 §3.2 第 1 条的条款理由**：形态 B 残留里那句
「MUST 由调用方在**设置好**消费端 ready **之后**调用」的**真正理由**是
「**改边必须在 `initialize()` 之前**」⇒ ⇒ 而**不是**「结果对象在该时刻不完整」
⇒ ⇒ ⇒ ⇒ **ADR 的形态 B 残留文本 MUST 按此订正**（§5 表第 2 行）。

⇒ ⇒ ✅ **已落地**（第 2 轮 critic 指出 ADR / spec delta「零命中」⇒ ⇒ 真实缺口）：
- **ADR §3.2 第 1 条** ⇒ ⇒ 新增第 4 个必要条件（「覆盖 MUST 在
  `Simulator::initialize()` **之前**做」）+ 订正形态 B 残留的**真正理由**；
- **ADR §5 Verification** ⇒ ⇒ 补对应的验收判据（含「判别力陷阱」提示）；
- **主 spec R2** ⇒ ⇒ 「三个必要条件」⇒ ⇒「**四个**必要条件」+「三类失效」
  ⇒ ⇒「**四类**失效」；
- **主 spec R2 的 Scenario「`connect_ready` MUST 能覆盖默认驱动」** ⇒ ⇒
  **正文**补一条 AND（⚠️ **标题 MUST NOT 改** ⇒ ⇒ 只补正文）
  ⇒ ⇒ 「调用方 MUST 在 `Simulator` 构造**之前**完成该 ready 连接」。

⇒ ⇒ **对判别力的影响（本 change 用例的直接教训）**：
任何「运行期改 ready ⇒ 断言值变化」的用例**恒绿** ⇒ ⇒ ⇒ **MUST NOT** 用它
当判据 ⇒ ⇒ ⇒ 本 change 的用例改用「**独立的 DUT 实例** + 在
`Simulator` 构造**之前**设定 ready」。

⇒ ⇒ **已回写** `.rddf/improvements/stream-forward-ready-closure-2026-10.md`
（新增「①-2 第 7 条发现」段）。

### 2.5 literal 覆盖（**沿用**已有实测，不重造）

⚠️ **元教训复刻**：我为「literal 上 `set_src` 是否有效」连写 **3 版**探针
（`p0v1b` / `p0v1c` / `p0v1d`），**全部**测错了对象 —— ① 测了「consumer 读
literal」而非「literal 自身」⇒ REFUTED；② ③ 测的是**孤立节点**（不在 eval
list ⇒ 「Value not found」）⇒ REFUTED ⇒ ⇒ **连对照组也 REFUTED** ⇒ ⇒
**探针输出被读反了**。

⇒ ⇒ **正确答案在仓内已有**：归档 change 的 design.md **§13.1 E-1**（隔离实验
`/tmp/opencode/e1/e1.cpp`，**图内**节点）：

```
s=0 ⇒ out_lit=1   out_nonlit=0
s=1 ⇒ out_lit=1   out_nonlit=1
```

⇒ ⇒ `out_lit` **恒为 1**（= literal 原值）⇒ ⇒ 「对 literal 节点的 `set_src`
**完全无效**」**成立**；⇒ ⇒ 对照组（mux 节点）**正常跟随** ✔
⇒ ⇒ ⇒ **不需要新探针** ⇒ ⇒ 但 critic 的「验证建议 1」本身是**好问题**
（它标记为「推断，未验证」）⇒ ⇒ 结论：**已被回答，证据在 design.md §13.1**。

## 3. 「field proxy」理由订正

| | 原文（ADR §3.2 第 1 条 / 主 spec R2） | 订正后 |
|---|---|---|
| 理由 | 读使能 MUST 引用「**字段节点本身**」（`result.pop_stream.ready` 的 **proxy**） | 读使能 MUST 引用**已存在**的**非 literal** 节点（⇒ ⇒ **结果对象的 ready 字段**） |
| 为什么 | （隐含：proxy 是「一等节点」⇒ ⇒ 覆盖有效） | ⇒ ⇒ **真实机制** = 「`<<=` 在**已有**目标上 `set_src(0,…)` ⇒ ⇒ **覆盖生效**；在**空**目标上**另建**节点 ⇒ ⇒ **调用方被孤儿化**」⇒ ⇒ ⇒ **「用 `<<=`」本身**不是**覆盖生效的**充分条件** |
| 附注 | —— | ⚠️ **「`<<=` 在空节点上另建节点」MUST NOT 被追认为公开契约语义**（critic F2-Q2）：它依赖 `bool.tpp` 的**实现细节** ⇒ ⇒ 与 ADR §3.1 形态 A 的否决理由（`:66`「依赖 `operator<<=` 实现细节 ⇒ 不具可移植性」）**是同一个问题换了名字** ⇒ ⇒ ⇒ 正确路径 = 让该分支在**组合子调用链中不可达**（前置条件契约化 + 护栏检出空目标）⇒ ⇒ **收窄** §3.1 形态 A 的「显式追认」段的适用范围 |

## 4. Q1 裁决：B（**不是** C）—— 依据与执行形态

### 4.1 为什么 C 是 BLOCKER

⚠️ **主 spec 是「当前真相源」，没有「未来时」** ⇒ ⇒ C（「转发类条款**新增**进
主 spec 并标注『实施中』」）⇒ ⇒ 归档后主 spec 含一条**今天为假**的 MUST
⇒ ⇒ 且其 Scenario（「注入一处『无条件忽略』⇒ MUST 转红」）**当前不可满足**
⇒ ⇒ ⇒ **「契约与实现不一致」** ⇒ ⇒ BLOCKER。

### 4.2 仓内直接先例（**本仓自己**的判例）

`openspec/specs/chlib-stream-semantics/spec.md:90-95`（2026-10-22 对抗审稿
BLOCKER 的收口订正，逐字）：

> ⚠️ **收口订正（2026-10-22，对抗审稿 BLOCKER）**：本 capability 归档时曾误并入两条
> 「构造期纪律 / 两阶段 ready」Requirement。它们描述的是**尚未实施的后续立项**
> `chlib-stream-ready-ownership-2026-10`，且现存代码（如 `stream.h` 的组合子硬编码
> `ready = true`）并不满足 ⇒ 以「MUST 已生效契约」存在于正式主 spec 属于
> 「契约与实现不一致」。这两条已从 in-force 契约移除，待该 follow-up change
> 以 ADDED delta 重新提出。

⇒ ⇒ ⇒ **我们上一个 change 正是这个错误的受害者** ⇒ ⇒ 本 change 若走 C
= **重犯同一个错** ⇒ ⇒ ⇒ **Q1 = B**。

### 4.3 B 的执行形态（**不是**「删掉转发类义务」）

R1 正文**保留**「fifo 类已兑现」的全部条款 ⇒ ⇒ **新增**「本条的适用范围」段：

- **fifo 类** ⇒ ⇒ 已兑现 + 有护栏覆盖（R1 效果判据 + R2~R6 静态判据）；
- **转发类** ⇒ ⇒ MUST NOT 被本条声称「已兑现」⇒ ⇒ 义务**显式移交**
  `.rddf/improvements/stream-forward-ready-closure-2026-10.md`（P1 台账 §3）
  ⇒ ⇒ 待 P1 落地时经 delta **加回** ⇒ ⇒ 届时 MUST 同时补齐转发类的
  **行为判据**与**自动化护栏**（含自检）。

⇒ ⇒ **义务不丢、可追踪**（台账在仓内 + 加回路径明确）⇒ ⇒ **不是**「静默弱化」
⇒ ⇒ 与 R4 的**区别**（critic F1-Q1）：R4 降级的是「框架层**做不到**」；
R1 转发类是「本仓**未做**」⇒ ⇒ **可援引的只是 R4 的写法惯例，不是理由类型**。

## 5. ADR-054 的订正（6 处）

| 位置 | 问题 | 动作 |
|---|---|---|
| §3.2 第 1 条 | 「field proxy」理由错误（见 §3） | 订正 + 收窄「追认」范围 |
| §3.2 第 1 条**末 3 bullet** | **形态 B 残留**：「构造期 MUST 不产出 payload/valid/ready」·「由调用方在设置好消费端 ready 之后调用」·「MUST NOT 引入代理节点」⇒ ⇒ 与前 2 bullet（「立即建立内部 FIFO」·「存量调用点 MUST NOT 被迫迁移」）**直接矛盾** ⇒ ⇒ ADR `:48` 自称「§3.1/§3.2/§5 已同步改写（不保留两版）」**不成立** | **删除这 3 bullet** |
| §4 正面 | 形态 B 残留：「未调用 `connect_ready` ⇒ ⇒ 响亮 "Value not found"」⇒ ⇒ 形态 E 实测 = **默认恒就绪**（读到 `true`） | 改写为形态 E 的**真实**后果 |
| §4 负面 | 形态 B 残留：「`stream_fifo` 的结果在 `connect_ready()` 之前**不完整** ⇒ ⇒ 全部调用点 MUST 迁移」⇒ ⇒ 与 §3.2「存量调用点 MUST NOT 被迫迁移」**直接矛盾** ⇒ ⇒ **P1 实施方读它会执行反向动作**（强制迁移）⇒ ⇒ **P1 返工** | 改写为形态 E 的真实代价（**零破坏**）+ 残余风险（已接受） |
| §4 负面末 | **悬空残片**：「（如 `stream_fifo(x).pop_stream.payload`）⇒ ⇒ 调用点形态 MUST 改」⇒ ⇒ 语义不完整 | 删除 |
| §5 Verification | ① 「grep-lint 护栏 MUST 抓到 `ready = true`」⇒ ⇒ **词法判据** ⇒ ⇒ 与主 spec「判据 MUST 是效果判据」**冲突**；② 「引用『字段节点本身』（…的 proxy）」⇒ ⇒ 理由错误 | ① 改为**效果判据**的表述（护栏 R1 跑行为用例）；② 随 §3 同步订正 |
| `:27` | 「关联 change: `chlib-stream-ready-ownership-2026-10`（**待建** ⇒ builder）」⇒ ⇒ **实际已归档** | 路径订正 |

## 6. 判别力用例设计（`tests/chlib/test_stream.cpp`）

⚠️ **必须能抓住 P0 发现 ③**（「`<<=` 在调用方未建节点时静默无效」）：

```
变体 A（调用方 **未** as_master ⇒ ⇒ 旧实现：ins[0].ready 仍是空节点）
  ⇒ ⇒ 用例 MUST 断言：调用方持有的 ready 节点 impl != nullptr
      且 num_srcs() > 0
  ⇒ ⇒ **变异「去掉 `<<=`」⇒ MUST 转红**
变体 B（调用方 **先** as_master ⇒ ⇒ 覆盖到达 ✔ ⇒ 断言值跟随消费端 ready
```

⇒ ⇒ **判别力自检**：把 `stream_mux_connect_ready` 的 `<<=` 改回 `operator=`
⇒ ⇒ 变体 A 的断言 MUST 转红 ⇒ ⇒ **必须先确认编译成功**（否则跑旧二进制）。

⚠️ **进既有 target** ⇒ ⇒ **计数锚点不变**（`CTEST_TOTAL` 205 等）。

## 7. critic 第 1 轮的处置台账

### 7.1 BLOCKERS（3/3 处置）

| # | critic 判定 | 处置 |
|---|---|---|
| B1 | Q1 = B；C 是 BLOCKER | **采纳 B** ⇒ ⇒ R1 正文收窄 + 「适用范围」段（§4） |
| B2 | P0' 范围漏掉 spec 侧 ③ ④ | **采纳** ⇒ ⇒ R3 补充分条件 + 顶部断言加范围限定 + 归档后补写 Purpose |
| B3 | 漏掉 ADR §4（形态 B 残留） | **采纳** ⇒ ⇒ §4 整节重写 + §3.2 末 3 bullet 删除（§5） |

### 7.2 SHOULD_FIX（6/6 处置）

| # | critic 建议 | 处置 |
|---|---|---|
| SF1 | 权限判定规则（「实施方读完新文本后被允许/被要求做的事是否变了」）+ Q1 与 R4 **不同型** | **采纳** ⇒ ⇒ 写入 proposal「权限说明」+ design §4.3 |
| SF2 | 「目标 MUST 已存在且非 literal」**不充分**（缺源侧）⇒ ⇒ MUST 写双侧 | **采纳** ⇒ ⇒ R2「覆盖生效的三个必要条件」+ 实测 V2 证据（§2.2） |
| SF3 | 「`<<=` 空目标建节点」MUST NOT 作契约依据 ⇒ ⇒ 与形态 A 否决理由同型 | **采纳** ⇒ ⇒ §3 收窄「追认」范围 + R3 的「MUST NOT 被当作充分条件」 |
| SF4 | 前置条件契约化须带 3 条件（① 含 `create_field_nodes()` ② 标注 0-A 落地时退役 ③ 配护栏） | ✅ **采纳 ①②③**（**第 2 轮 critic 复核确认闭合**）：① R3 写明「`as_master()` / `as_slave()` / `create_field_nodes()` 之一」✔；② R3 补「**0-A 落地时本条经 `MODIFIED` delta 退役**」+「退役 MUST 走 `MODIFIED`（保留其余 Scenario）MUST NOT 删除 Requirement」✔（⚠️ 第 1 轮时**只**写进了本表而**未**落进 delta ⇒ ⇒ 第 2 轮 critic 的 grep 抓到 ⇒ ⇒ 已补 ⇒ ⇒ **「勾选 ≠ 落地」的第三次复刻**）；③ **配护栏** ⇒ ⇒ 属 **P1**（护栏在 P1 落地时才有转发类可检）⇒ ⇒ 本 change 只在**范围**上登记 |
| SF5 | P1 应再拆（P1-a / P1-b / P1-c）+ `queue<>()` 优先级最高 | **采纳** ⇒ ⇒ 已回写 P1 台账 `.rddf/improvements/stream-forward-ready-closure-2026-10.md`（拆分 + 优先级）⇒ ⇒ **本 change 不实施 P1** |
| SF6 | P1-a 前 MUST 先定守卫统一规格 | **采纳** ⇒ ⇒ V3 实测**部分证伪**了 critic 的「误炸」推断（下游 `connect_ready` 不误炸）⇒ ⇒ 但「上游反压丢失」**确认** ⇒ ⇒ 规格方向成立（「逐节点单一所有权 + 所有权转移 MUST 经 `connect_ready`」）⇒ ⇒ 回写 P1 台账 |

### 7.3 NITS（3/3 处置）

| # | critic 建议 | 处置 |
|---|---|---|
| N1 | ADR `:27`「待建」⇒ ⇒ 已归档 | **采纳** ⇒ ⇒ §5 表 |
| N2 | `ready = true` 实为 **4 处**（`stream.h:301`/`:604` + `stream_arbiter.h:96`/`:161`）+ `stream_arbiter.h:92`/`:157` 另有**先读后写** ⇒ ⇒ 台账「arbiter×2」措辞不准 | **采纳** ⇒ ⇒ 已核实（`grep -rn "ready = true" include/chlib/` = 4 处）⇒ ⇒ 回写 P1 台账为「**arbiter×4 + 2 处先读后写**」 |
| N3 | `stream.h:481-487` 的 fork 注释是「空 proxy 规避写法」的记录 ⇒ ⇒ P1-c 修复后 MUST 重写 | **采纳** ⇒ ⇒ 回写 P1 台账（P1-c 的验收项） |

### 7.4 critic 的验证建议（3 条）

| # | 建议 | 结果 |
|---|---|---|
| V1 | 探针 literal `set_src` 行为 | ⚠️ **已被回答**（design.md §13.1 E-1，**图内**节点隔离实验）⇒ ⇒ **不重造**（§2.4 记录了本次的 3 版探针失败 = 元教训复刻） |
| V2 | 探针源为空 | ✅ **已实测**（§2.2）⇒ ⇒ CONFIRMED |
| V3 | 探针串联 + 守卫误炸 | ✅ **已实测**（§2.3）⇒ ⇒ 「误炸」**证伪**，但「上游反压丢失」**确认** |

## 7.5 实施期实测台账（本 change 的判别力验证）

| 项 | 结果 |
|---|---|
| **变异 M1**：`stream_mux_connect_ready` 的 `<<=` ⇒ ⇒ `operator=` | ✅ **被抓到**：`tests/chlib/test_stream.cpp:969` `REQUIRE(in0.ready.impl()->num_srcs() > 0)` ⇒ ⇒ `0 > 0` **转红** ⇒ ⇒ ⚠️ **先确认编译成功**（`grep error:` 为空）⇒ ⇒ ⇒ **判别力成立** |
| **还原** | ✅ `shutil.copyfile`（**刷新 mtime**）⇒ ⇒ **重建** ⇒ ⇒ **再跑一次绿**（14 断言）⇒ ⇒ ⇒ 完整闭合 |
| ⚠️ **还原的副作用（本 change 实测）** | 🔴 **还原覆盖了本 change 的注释订正** —— 变异前的 `cp` 备份是在 **T3.1~T3.3 订正**之前取的 ⇒ ⇒ ⇒ 还原后 `include/chlib/stream.h` 的 3 处订正**全部丢失** ⇒ ⇒ ⇒ ⚠️ **这正是本仓「还原源码 ≠ 复原」的又一次实例**（且**备份时点**是新的失效维度）⇒ ⇒ ⇒ **修法**：备份 MUST 取在**全部**改动完成**之后** ⇒ ⇒ ⇒ 或还原后 MUST 用 `git diff` **核对**改动面 ⇒ ⇒ ⇒ 本次已在 `git diff --stat` 里发现并重新应用 ⇒ ⇒ ⇒ **已登记**（见 §7.6） |
| **探针失败复盘** | ⚠️ **元教训第 N+2 次复刻**：为「literal 上 `set_src` 是否有效」连写 **3 版**探针**全部测错对象**（① 测「consumer 读 literal」而非「literal 自身」⇒ REFUTED；②③ 测**孤立节点**（不在 eval list ⇒ 「Value not found」）⇒ REFUTED ⇒ ⇒ **连对照组也 REFUTED**）⇒ ⇒ **探针输出被读反了** ⇒ ⇒ **正确答案在仓内已有**（归档 change 的 design.md §13.1 E-1，**图内**节点隔离实验）⇒ ⇒ ⇒ **未重造** ⇒ ⇒ 记录于 §2.5 |

### 7.6 ⚠️ **流程教训**（3 条 · 均为本轮实测）

#### ① 「还原源码 ≠ 复原」的**新失效维度** = **备份时点**

🔴 **实测**：变异验证时的 `cp` 备份取在 **T3.1~T3.3 注释订正之前**
⇒ ⇒ ⇒ 还原后 `include/chlib/stream.h` 的 3 处订正**全部丢失**
⇒ ⇒ ⇒ ⚠️ 而 `git diff --stat` **当时没有显示** `include/chlib/stream.h`
⇒ ⇒ ⇒ **这是本仓已登记那条教训（「还原源码 ≠ 复原」）的新维度**
（原版讲的是「**二进制**也是状态」；本条讲的是「**备份的时点**决定还原后剩下什么」）。

⇒ ⇒ ⇒ **纪律（本 change 新增）**：
① 变异备份 MUST 取在**全部**改动完成**之后**；
② 还原后 MUST 用 `git diff --stat` **核对改动面**（⇒ ⇒ 本次正是靠它发现）；
③ 变异还原 MUST 与**注释订正**分开做（⇒ ⇒ 或还原后**重新应用**订正并再验证一次）。

#### ② 🔴🔴 **「勾选」≠「落地」** —— 本仓的一个**新病症实例**

🔴 **实测**：第 1 轮 commit（`3b45c4d`）里 `include/chlib/stream.h` 的 3 处
注释订正（T3.1~T3.3）**不在** commit 里 ⇒ ⇒ 而 `tasks.md` 的 T3.1~T3.3
**却标记了 `[x]`** ⇒ ⇒ ⇒ **tasks 与实现不一致** ⇒ ⇒ ⇒
「第 2 轮 critic 的事实核对（`git show --name-only` + `grep`）」**抓到了**。

⇒ ⇒ **根因**：变异还原的 `git reset --hard`（reflog: `fba4675 HEAD@{5}`）
⇒ ⇒ ⇒ 把**重新应用**的订正**冲掉** ⇒ ⇒ ⇒ 而勾选是在**冲掉之前**做的。

⇒ ⇒ ⇒ **纪律（本 change 新增）**：
① **勾选 MUST 在 commit 之后按 `git show --name-only` 核对** ⇒ ⇒ 即
「勾选的每一项 MUST 出现在 commit 的文件清单里」；
② 提交前 MUST `git diff --stat` **逐项对照** tasks 的改动面；
③ ⇒ ⇒ ⇒ **更根本的**：**验证必须针对 commit 后的 HEAD**（⇒ ⇒ 本仓已登记
「**探针输出的解读**也必须被独立复核」⇒ ⇒ ⇒ 本条是**同一元教训在
「勾选」维度上的复刻**）。

#### ③ ⚠️ **第 2 轮 critic 的事实核对抓到了 build agent 自报的「已重新应用」**

🔴 build agent 在 commit message 里自报「已重新应用并重新验证」
⇒ ⇒ ⇒ 但**事实**是 commit 里**没有** ⇒ ⇒ ⇒ ⇒
⇒ ⇒ ⇒ **「自报」不等于「事实」** ⇒ ⇒ ⇒ **验收必须由独立核对确认**
⇒ ⇒ ⇒ ⇒ （critic 的组装器做 `git show --name-only` + `grep` ⇒ ⇒ 抓到了）

## 8. 验收判据

- [ ] `openspec validate stream-ready-ownership-correction-2026-10 --type spec --strict` ⇒ **valid**
- [ ] `bash scripts/verify_openspec_delta.sh` ⇒ exit 0（R1/R2/R3 的 **Scenario 标题**逐字未改 · **Scenario 正文**逐字保留）
- [ ] 判别力用例：变体 A 的断言在「`<<=` 改回 `operator=`」的变异下 **转红**（⇒ ⇒ **先确认编译成功**）
- [ ] `ctest -LE perf` **零回归**（**194/194**）
- [ ] **计数锚点不变**：205 / 38 / 29 / 184 / 194 / 17 · samples 39/39 · examples 28/28
- [ ] 全部护栏（含 `--self-test`）exit 0
- [ ] 归档**禁** `--skip-specs` ⇒ ⇒ 归档后**另行补写**主 spec 的 `## Purpose`（先例 `core-lnode-entry-set`）
- [ ] 第 2 轮 critic：只提交「**BLOCKER 闭合证据 + 本轮新增 diff**」
