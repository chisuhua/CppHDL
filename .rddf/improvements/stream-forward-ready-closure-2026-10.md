---
name: stream-forward-ready-closure-2026-10
priority: P1
source: P0 探针（`/tmp/opencode/p0/p0v2.cpp`，2026-10-31 实测）+ oracle / metis 架构评审
phase: phase-5
category: chlib
type: bugfix
created_at: 2026-10-31
---

# Improvement: 转发类 stream 组合子的 **ready 链收口**（+ 「调用方必须先建立角色」这条前置条件契约化）

- **Date drafted**: 2026-10-31
- **Drafted by**: build-agent (builder role)
- **Status**: 📋 5-segment draft，**决策权归 planner / architect**
  （⚠️ 本草案**只登记实测事实 + 范围界定**，**不**替 architect 裁决）
- **来源**：change `chlib-stream-ready-ownership-2026-10`（已归档）落地后，**P0 探针**
  挖出的 **6 条**实测事实（其中 4 条是**未登记**的缺口 ⇒ 此前只存在于会话记忆里）
- ⚠️ **关联**：capability `chlib-stream-ready-ownership`（**已在约**）· ADR-054

## 1. Why (背景与动机)

🔴 **capability `chlib-stream-ready-ownership` Requirement 1 已在约**（明文：转发类
mux / demux / arbiter **无条件**忽略消费端 ready ⇒ ⇒ **仍属违规** ⇒ ⇒ 且「MUST 有
自动化护栏检出」）⇒ ⇒ **但实现当前就在违反它，且没有任何护栏看守** ⇒ ⇒
**「契约与实现公开冲突」** —— 比「缺覆盖」更坏。

⚠️ P0 探针（`/tmp/opencode/p0/p0v2.cpp`，**全部 CONFIRMED**）实测到的事实：

### ① 🔴 根因：`ch_stream` 的字段**默认构造后是空节点**

```
s.valid impl=空节点(nullptr)      s.ready impl=空节点(nullptr)
as_master() 后 s2.valid impl=有节点
```

⇒ ⇒ `create_field_proxy_nodes()` **只在 `set_role`（`as_master`/`as_slave`）里**
被调用 ⇒ ⇒ **默认构造的 `ch_stream` 的 payload / valid / ready 全是 null impl**
⇒ ⇒ 对它们做值运算 ⇒ 「**Invalid operands**」⇒ ⇒ 读它们 ⇒ **ABORT**
（`simulator.h:325`「ch_bool implementation is null」）。

⇒ ⇒ ⇒ **一条从未被文档化的框架层隐含前提**：
**调用方的 stream MUST 先 `as_master()` / `as_slave()`，组合子才能驱动它的 `ready`**。
⇒ ⇒ 仓内**遵守情况不一致**：`spinalhdl_stream_example.cpp` **7** 处 ·
`test_stream.cpp` **5** 处 ✔ ↔ `stream_fifo_example.cpp` /
`stream_component_example.cpp` / `stream_handshake_example.cpp` **各 0** 处 ✘
⇒ ⇒ ⇒ **框架内不一致**（「修复不完整」病症的第 4 个实例）。

### ①-2 🔴🔴 **第 7 条发现（2026-10-31 实测 · P0 六条之外）**：`set_src` 改边在
###         `Simulator::initialize()` **之后不生效**
###         （⚠️ 这条是在 **P0' 的实施期**实测发现的 —— P0 探针**只**挖出 6 条）

⚠️ **实测**（探针 `/tmp/opencode/p0/dbg_bisect4.cpp`）：

```
tick1: out=1
tick2（改边后，未重新 initialize）: out=1     ← 覆盖【未生效】
tick3（新 Simulator 后）:            out=0 in0=0  ← 覆盖【生效】且传播 ✔
```

⇒ ⇒ **机制**：指令的 `src0_buf` 指针是在 `initialize()` 时**绑定**的
⇒ ⇒ ⇒ `set_src` 改的是**图**，**不重建指令** ⇒ ⇒ ⇒ **覆盖只在
`initialize()` 之前做才有效**。

⇒ ⇒ ⚠️ **这改变了 ADR-054 §3.2 第 1 条的条款理由**：形态 B 残留里那句
「MUST 由调用方在**设置好**消费端 ready **之后**调用」的**真正理由**是
「**改边必须在 `initialize()` 之前**」⇒ ⇒ 而**不是**「结果对象在该时刻不完整」
⇒ ⇒ ⇒ **ADR 的形态 B 残留文本必须按此订正**（已在
`stream-ready-ownership-correction-2026-10` 的 ADR 订正中处理）。

⇒ ⇒ **对本项（P1）的直接影响**：
- 「消费端不就绪」分支 MUST 用**独立的 DUT 实例**测 ⇒ ⇒ **MUST NOT**
  在运行期改 `output_stream.ready`（⇒ ⇒ 会得到**假绿**）；
- ⚠️ **判别力陷阱**：任何「运行期改 ready ⇒ 断言值变化」的用例**恒绿**
  ⇒ ⇒ ⇒ **MUST NOT** 用它当判据。

### ② 🔴 转发类的 ready 链**两端都断**（不是「方向颠倒」，是「根本没建起来」）

| 组合子 | 实测 |
|---|---|
| `stream_fork(in, /*sync=*/false)` | `Invalid operands` **×2** ⇒ `output.valid` = **空节点** ⇒ **读即 ABORT 💥** · `output.ready` = 空节点 |
| `stream_demux(in, sel)` | `Invalid operands` **×6** ⇒ `input.ready` = 空节点（上游推不动）· `output.ready` = 空节点（消费端无法反压） |
| `stream_take_when(in, cond)` | `Invalid operands` ×1 ⇒ 调用方 `in.ready` = 空节点（**孤儿化**）**且** 结果的 `filtered.ready` = 空节点 ⇒ ⇒ **链两端都断** |

⚠️ oracle 的静态推断（「方向颠倒」）是**次要**症状；**主**症状是**链根本没建起来**
⇒ ⇒ ⇒ P1 的实施面**比 oracle 估计的大**。

### ③ 🔴 `stream_mux_connect_ready` 的 `<<=` 修复在「调用方未 `as_master`」时**无效**

```
变体 A（调用方未 as_master）：
  connect_ready 后：ins[0].ready impl=空节点(nullptr) srcs=-1   ← 调用方仍无驱动
  connect_ready 后：mux.input_streams[0].ready impl=有节点 srcs=1 ← 只驱动了 mux 的副本
变体 B（调用方先 as_master）：
  connect_ready 后：ins[0].ready impl=有节点 srcs=1              ← 覆盖到达 ✔
```

⇒ ⇒ **根因**：`ch_bool::operator<<=` 在**空节点**上走 else 分支 ⇒ ⇒ **建了一个
新节点** ⇒ ⇒ 只驱动了被 `<<=` 的那个对象 ⇒ ⇒ **调用方持有的节点被孤儿化**
⇒ ⇒ 与 `operator=` **同样的后果**，只是发生在**另一个**节点上。

⇒ ⇒ ⚠️ ⇒ **而 `ctest` 194/194 全绿** ⇒ ⇒ **假绿**（无任何测试断言「调用方的
ready 真被驱动」）⇒ ⇒ ⇒ **已归档的 capability Requirement 3（「ready 链 MUST
用 `<<=` 连接」）在「调用方未 `as_master`」时**没有**被真正兑现** ⇒ ⇒ ⇒
**spec 在说谎** —— 正是 oracle 批评转发类的那条现象，**我们自己又犯了一次**。

### ④ 🔴 filters 家族 6 处 `operator=` 孤儿化

`include/chlib/stream.h:115`（`throw_when`）· `:132`（`take_when`）·
`:149`（`halt_when`）· `:173`（`translate_with`）· `:189`（`map`）·
`:205`（`transpose`）+ `:243-244`（`combine_with`）⇒ ⇒ 全是
`input_stream.ready = …` ⇒ ⇒ 入参**按值拷贝** ⇒ ⇒ `operator=` 只重绑定**局部
副本**的成员指针 ⇒ ⇒ **调用方的 ready 节点被孤儿化** ⇒ ⇒ **上游静默丢失反压**。

⇒ ⇒ 这正是 mux 已改掉、而同族 **6 处**未改的形态 ⇒ ⇒ ⇒ 「修复不完整」病症。

## 2. What Changes (变更范围)

### 2.1 🔴 **契约级的前置条件**（**必须先裁决** ⇒ ⇒ architect 决策）

- **调用方的 stream MUST 先 `as_master()` / `as_slave()`**（或自行建立一个
  **有节点**的 ready 字段）⇒ ⇒ 否则组合子**无法**驱动它的 ready
  ⇒ ⇒ ⇒ **这条 MUST 写进 capability**（⇒ ⇒ 它是「契约说谎」的解药）；
- ⚠️ **另一条候选裁决**：组合子能否**替调用方建节点**？⇒ ⇒ **不能** ——
  调用方持有的是**空节点**，组合子拿不到它的指针 ⇒ ⇒ ⇒ **前置条件无法回避**。

### 2.2 转发类组合子**套用形态 E**（**不是** oracle 建议的「单阶段」）

⚠️ **oracle 的推论 3 被 P0 探针证伪**：P0-d 的原假设（「构造期读结果侧 ready
字段 ⇒ 合法操作数」）**不成立** ⇒ ⇒ ⇒ 转发类**也**需要组合子**自己先建节点**
⇒ ⇒ ⇒ **形态 E（组合子建默认节点 + `connect_ready` 覆盖）恰恰是唯一能让
ready 链工作的形态** ⇒ ⇒ ⇒ **可推广** ✔

⇒ ⇒ 逐个组合子的**具体形态**（**待实测确认**）：
- `stream_mux` / `stream_arbiter_*`：**已有** `connect_ready` ⇒ ⇒ 只需修「调用方
  未 `as_master`」的**契约** + **判别力用例**（⇒ ⇒ 不必改实现）；
  ⚠️⚠️ **实测订正**：硬编码 `ready = true` 实为 **4 处**，**不是 2 处** ——
  `include/chlib/stream.h:301` / `:604` + **`include/chlib/stream_arbiter.h:96`
  / `:161`** ⇒ ⇒ 且 `stream_arbiter.h:92` / `:157` 另有**先读后写**
  （读 `result.output_stream.ready` 在其赋值**之前**）⇒ ⇒ ⇒
  ⇒ ⇒ 「arbiter×2」的措辞 MUST 改为「**arbiter×4 + 2 处先读后写**」；
- `stream_demux`：**方向颠倒**（`output.ready` 被**上游**的 ready 推导）
  ⇒ ⇒ **行为变更** ⇒ ⇒ **需独立 ADR** + 既有消费者盘点；
- `stream_fork`：**同步**语义（input ready = AND(各 output ready)）**从未工作过**
  ⇒ ⇒ 修完等于**新功能** ⇒ ⇒ 判据从零建；
- `stream_join`：`input_streams[i].ready = result.output_stream.ready`（重绑定）
  ⇒ ⇒ 改 `<<=`；
- filters ×6 + `combine_with`：`=` ⇒ `<<=`；
- `StreamBuilder::queue<>()`：**上游侧**反压（`push_stream.ready` 被丢弃）
  ⇒ ⇒ ⚠️ 链长 ≥ 2 时 fifo1 弹出的 word 会被 fifo2 的写口自保护**静默吞掉**
  ⇒ ⇒ **静默丢数据** ⇒ ⇒ 必须补上游回接。

### 2.1-1 🔴🔴 **P1-a 实施期的 4 个探针实测（2026-11-01 · 决定形态）**

⚠️ 探针位置 `/tmp/opencode/p1/p1a_probe{1,2,3,4}.cpp`（仓外 ⇒ 不可复算
⇒ ⇒ **输出已如实登记在此**）。

#### ① 转发类的 ready 链**全部断开**（探针 ①：5/5 CONFIRMED）

| 对象 | 实测 |
|---|---|
| `StreamBuilder::queue<4>().queue<4>()` | `in.ready` = **空节点** ⇒ ⇒ 上游永远收不到反压 |
| 单个 `queue<4>()` | 同上 |
| filters（`take_when` / `throw_when`） | `Invalid operands` ×2 ⇒ ⇒ 调用方与结果**两端都断** |
| `stream_join` | `a.ready` 空 + `output_stream.ready` **空** ⇒ ⇒ **两端都断** |
| `stream_arbiter_lock` | 构造期 `Invalid operands` ×2（**未**抛 ⇒ **静默**） |

⇒ ⇒ ⇒ **转发类（除 mux）的 ready 链全是断的** ⇒ ⇒ ⇒ 现状 = **死路**。

#### ② 🔴 **转发类现状 = 「上游恒不 ready = 死路」**（探针 ④）

```
=== 现状：不调 connect_ready ===
   in0.ready srcs=0   in1.ready srcs=0
   in0.ready 值 = 0
   ⇒ ⇒ 结论：上游**恒不 ready** ⇒ ⇒ 现状是**死路** ⇒ ⇒ 修复不是破坏
```

⇒ ⇒ ⇒ **「修复 = 行为变更」不成立** ⇒ ⇒ ⇒ **零破坏承诺成立**
⇒ ⇒ ⇒ ⇒ ⇒ **这是 P1-a 选单阶段的**关键前提**（若现状是「恒就绪」，
改成「可反压」才是行为变更 ⇒ ⇒ 需独立 ADR）。

#### ③ 🔴🔴 **拷贝后的 `ch_stream` 与原对象共享同一 ready 节点**（探针 ②：3/3 CONFIRMED）

```
a.ready impl=0x5639b4302040
b.ready impl=0x5639b4302040          ← 拷贝后**同一节点**
b.ready <<= sig ⇒ ⇒ a.ready srcs=1   ← 经副本驱动**到达原对象**
mux.input_streams[0].ready impl == in.ready.impl()  ← 结果对象与调用方**同一节点**
```

⇒ ⇒ ⇒ **组合子内部 `<<=` 能穿透副本驱动到调用方** ⇒ ⇒ ⇒
⇒ ⇒ **「调用方 MUST 先建立字段节点」是**唯一**的前置条件**
（节点存在 ⇒ ⇒ 可穿透；节点为空 ⇒ ⇒ `<<=` 另建节点 ⇒ ⇒ 副本专用 ⇒ ⇒ 失效）
⇒ ⇒ ⇒ ⇒ ⚠️ **这条是**框架层事实**，不是组合子能消除的。

#### ④ 🔴 **形态 E 在转发类上可行（单阶段）**（探针 ③：2/2 CONFIRMED）

```
A：消费端不就绪 ⇒ out.ready=0 in0.ready=0   ← 反压**穿透到调用方** ✔
B：不覆盖      ⇒ in0.ready=1                ← **恒就绪**（= 既有旧语义）✔
```

⇒ ⇒ ⇒ **单阶段成立**：组合子内部建「默认恒就绪」节点（源持久持有 ⇒ 非
literal ⇒ 可被覆盖）⇒ ⇒ 消费端**直接** `<<=` 覆盖 ⇒ ⇒ **不需要**
`connect_ready` ⇒ ⇒ ⇒ ⇒ 心智模型与 fifo 一致 + **无需调用方迁移**。

⇒ ⇒ ⇒ **裁决（2026-11-01 · 用户）**：**A｜单阶段（形态 E）**
⇒ ⇒ ⇒ ⇒ ⚠️ **代价**：与已签批 **ADR-054 §3.2 第 2 条**（「mux / demux /
arbiter 类 MUST 保持决策 0-B 的**两阶段**形态」）**冲突**
⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ **用户已批准由 builder 代改**（加注「角色越权执行」）
⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⚠️ **`stream_mux_connect_ready` 变为冗余**
⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ **其语义 MUST 被定义**（幂等覆盖？或标记废弃？）
⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ **P1-a 的实施 MUST 先定这一点**。

### 2.2-1 🔴 **实施面 MUST 拆成 3 个独立 change**（2026-10-31 critic 第 1 轮
####          SHOULD_FIX #5 的裁决；本台账自己曾以「爆炸半径」为由拆走 RR ⇒ ⇒
####          **一致性要求**同样适用于本项）

| 子集 | 内容 | 风险等级 | 独立 change |
|---|---|---|---|
| **P1-a** | ⓐ 仅改连接手段：`mux` / `arbiter×4` / `join` / `filters×6` / `combine_with` ＋ **`StreamBuilder::queue<>()` 上游回接** | **纯修复**（+ **静默丢字** ⇒ ⇒ 优先级**最高**） | ✅ |
| **P1-b** | `demux` 方向（输出 ready 被上游 ready 推导） | **行为变更** ⇒ ⇒ 台账**自认**「需独立 ADR」⇒ ⇒ 不该留在本 improvement 的实施面内 | ✅ |
| **P1-c** | `fork` 同步语义（input ready = AND(各 output ready)） | **从未工作过** ⇒ ⇒ 等于**新功能** ⇒ ⇒ 判据**从零建** | ✅ |

⇒ ⇒ **P1-a 的优先级判定**：`queue<>()` 的**静默丢字**（链长 ≥ 2 时 fifo1 弹出的
word 被 fifo2 的写口自保护**静默吞掉**）**优先于** `demux` 的行为变更
⇒ ⇒ ⇒ 理由：**静默丢数据** > **行为变更**（后者至少**可观测**）。

⚠️ **P1-c 的附带项**：`include/chlib/stream.h:481-487` 的 fork 注释是「空 proxy
规避写法」的**记录** ⇒ ⇒ P1-c 修复后该注释 MUST **重写** ⇒ ⇒ 否则成为下一处
「说谎注释」。

### 2.3 ⚠️ **多驱动守卫**的框架内不一致

`stream_fifo_connect_ready` 有守卫（`CHFATAL_EXCEPTION`）⇒ ⇒
`stream_mux_connect_ready` **没有** ⇒ ⇒ ⇒ 补齐（同一契约面 ⇒ 同一 change）。

⚠️⚠️ **统一规格 MUST 先定，再动实现**（2026-10-31 critic 第 1 轮 SHOULD_FIX #6
＋ **实测部分证伪**）：

⚠️ **实测**（探针 `/tmp/opencode/p0/p0critic.cpp` 的 V3）：

```
对下游 f2 调 connect_ready：threw=否
[CONFIRMED] 对**下游** f2 调 connect_ready **不误炸**（f2 自己的 ready 是默认驱动）
[CONFIRMED] 上游 f1 的 ready 仍只有默认驱动（下游**未**驱动它 ⇒ ⇒ 上游反压**丢失**）
```

⇒ ⇒ ⇒ critic 推断的「守卫会**误炸**」**被证伪**（下游 `connect_ready` 不误炸）
⇒ ⇒ ⇒ 但「**上游反压丢失**」**被确认** ⇒ ⇒ ⇒ 即「静默丢字」的机制确认。

⇒ ⇒ **统一规格（裁决方向）**：**逐节点单一所有权** ＋ **所有权转移 MUST 经
`<组合子>_connect_ready`**（⇒ ⇒ 串联 MUST 走上游结果的 `connect_ready`
⇒ ⇒ **MUST NOT** 裸 `<<=` 跨级驱动）⇒ ⇒ mux 侧补**同型**守卫
⇒ ⇒ ⇒ 该规格 MUST **先写进 capability**，再动实现。

⇒ ⇒ ⚠️ **另需补「源侧」检查**（实测 V2）：`connect_ready` 传入**空** ready 字段
⇒ ⇒ 当前**只打 `CHERROR`、不抛** ⇒ ⇒ **静默**保持默认驱动 ⇒ ⇒ ⇒
守卫 MUST 同时检查**源**（MUST 非空 ⇒ 否则 `CHFATAL_EXCEPTION`）。

## 3. Acceptance (验收判据 —— MUST 可勾选)

- [ ] **效果判据**（**MUST NOT** 词法）：调用方 `as_master()` 后 ⇒ ⇒
      `connect_ready` ⇒ ⇒ **调用方持有的 ready 节点 `num_srcs() > 0`** 且其
      **值**跟随消费端 ready ⇒ ⇒ **判别力**（变异：去掉 `<<=` ⇒ 转红）
- [ ] **效果判据**：fork 单个消费者不就绪 ⇒ ⇒ **上游 stall**（不推进）
- [ ] **效果判据**：demux 按 `select` 路由 ready ⇒ ⇒ 选中输出的消费端
      不就绪 ⇒ ⇒ **上游** stall
- [ ] **效果判据**：filters 透传反压 ⇒ ⇒ 调用方 ready 跟随下游
- [ ] **效果判据**：builder 链**上游侧** ⇒ ⇒ 两级 FIFO 填满 ⇒ ⇒
      **链最上游**的 ready 读到 0 ⇒ ⇒ 且**零丢字**
- [ ] ⚠️ **链长 ≥ 2 的用例**（`queue<4>().queue<4>()`）⇒ ⇒ 且**多驱动守卫在串联
      场景 MUST NOT 误炸**（⚠️ `stream_fifo_connect_ready` 的守卫断言「当前驱动
      仍是默认节点」⇒ ⇒ 串联时可能抛异常 ⇒ ⇒ **必须先实测**）
- [ ] **穷举全部入口**（⚠️ `multi_if::elif` 事件的第三次复刻风险）：
      `stream.h` 内 mux / demux / arbiter×2 / fork / join / filters×6 /
      `combine_with` + `stream_arbiter.h` 内 `lock` / `sequence`
- [ ] 变异：每条**先确认编译成功**
- [ ] 护栏：**扩展** `scripts/verify_stream_ready_ownership.sh`（同一契约 ⇒ 同一
      护栏）⇒ ⇒ 加**转发类**的静态 + 效果判据 ⇒ ⇒ **豁免非静默**
- [ ] `ctest -LE perf` 零回归 · **计数锚点不变**

## 4. Capabilities (契约影响)

- **MODIFIED** `chlib-stream-ready-ownership`：
  - Requirement「ready 链 MUST 用 `<<=` 连接」⇒ ⇒ **补「充分条件」**
    （目标节点 MUST **已存在**）⇒ ⇒ 并补「调用方 MUST 先 `as_master()`」；
  - **新增** Requirement「转发类组合子的 ready 链 MUST 两端都建立」；
- ⚠️ `demux` 的**行为变更** ⇒ ⇒ **需独立 ADR**（⇒ ⇒ 拆分或先裁决）；
- ⚠️ `fwft_fifo` 重写 / 改名（ADR-045:148）**不在**本草案范围。

## 5. Impact (影响面)

- **改动文件**：`include/chlib/stream.h`（转发类 ~12 处）·
  `include/chlib/stream_arbiter.h`（2 个入口）·
  `include/chlib/stream_builder.h`（上游回接）·
  `tests/chlib/test_stream.cpp` · `tests/chlib/test_stream_mux_demux.cpp` ·
  `samples/**`（若需补 `as_master()`）· 护栏脚本
- **爆炸半径**：**中-大** —— 转发类组合子的 ready 语义**逐个改** ⇒ ⇒ 既有调用点
  （含 `samples/` 与 `examples/`）MUST 复核
- **风险**：**中** —— ① 「调用方必须 `as_master()`」这条前置条件**可能与既有调用点
  冲突**（⇒ ⇒ 需盘点）；② `demux` / `fork` 是**行为变更**（⇒ ⇒ 「逐位不变」承诺
  不成立 ⇒ ⇒ MUST 记录）；③ 串联 `connect_ready` 的**多驱动守卫误炸**风险
- **回滚路径**：`revert` 实施 commit ⇒ ⇒ 无持久化面、无数据兼容负担
