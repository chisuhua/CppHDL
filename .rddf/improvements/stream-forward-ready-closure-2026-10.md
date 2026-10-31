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

### 2.3 ⚠️ **多驱动守卫**的框架内不一致

`stream_fifo_connect_ready` 有守卫（`CHFATAL_EXCEPTION`）⇒ ⇒
`stream_mux_connect_ready` **没有** ⇒ ⇒ ⇒ 补齐（同一契约面 ⇒ 同一 change）。

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
