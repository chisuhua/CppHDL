# ADR-045: `chlib` stream 组合子的语义契约（ready 所有权 + FIFO 时序）

- Status: Accepted
- Date: 2026-10-22
- Author: build-agent
- Parent: ADR-035（Verilator 后端）、ADR-042（bundle 字段一等节点）、ADR-044（`fifo_bundle` 读数据通路）
- 关联 OpenSpec change：`chlib-stream-semantics-2026-10`（新 capability `chlib-stream-semantics`）
- 前置审查：对提交 `2991eed`（`fix(samples): restore 9 disabled samples + fix 3 chlib null-operand defects`）的对抗审稿判定 **REJECT**

## Context

### 触发

`2991eed` 在 `include/chlib/stream.h` 修了 3 处「先读后写 ⇒ 空操作数」缺陷
（`stream_fifo` / `stream_fork` / 两个 arbiter），并新增了一个自称「行为级」的
Catch2 用例。该提交的对抗审稿判定 **REJECT**，实测发现：

| 现象 | 实测 |
|---|---|
| `chlib::stream_fifo` 的数据通路**仍然是坏的** | 推入 0x5A 后 `pop_stream.payload` 在 cycle 0..3 全为 `0x0`，cycle 4 才是 `0x5A` |
| 新增的「行为级」用例对上述缺陷**零信号** | 该用例只断言 `occupancy` / `empty`，**从不断言 `payload`** |
| commit 的「变异验证 ⇒ 证明是真护栏」**自证失效** | 该变异只区分「能跑 vs 崩溃」，不区分「对 vs 错」 |
| 同型「先读后写」缺陷**未修全** | `include/chlib/stream_arbiter.h:92` / `:157`、`include/chlib/stream.h:193-194` / `:395` 仍在；`stream_arbiter_lock` 额外**静默返回错值** |

⇒ 该提交做的是「**在从未被定义的地方打补丁**」，而非「定义语义并实现它」。

### 根因：ready 的所有权从未被定义

| 环节 | 证据 |
|---|---|
| `bundle_base` 拷贝构造**共享字段节点** | `include/core/bundle/bundle_base.h:66-70` |
| `operator=` 是**节点重绑定**（只换 `node_impl_` 指针） | `include/lnode/logic_buffer.tpp:21-26` |
| ⇒ 组合子按值收 `ch_stream` 后，函数体内写 `input_stream.ready = X` **只改本地拷贝的字段指针** | ⇒ **组合子永远无法驱动调用方的 `in.ready`** |

于是每个组合子都只能二选一地绕开：

1. **在构造期读一个还没被赋值的字段** ⇒ null proxy ⇒ `node_builder` 报
   "Invalid operands for binary operation"（`include/lnode/node_builder.tpp:31-34`）；
2. **硬编码一个常量 ready** 来绕过（`stream.h:274` `pop_ready = true`、
   `:251` / `:460` `out_ready = true`、`:344` / `:353` `result.input_stream.ready = input_stream.ready`）。

**这不是四个独立 bug，而是同一个根因的四个切面。**

### 背压从未存在过（不是 `2991eed` 引入的回归）

`git show 2991eed` 证实：fork 原实现读的是构造期空 proxy ⇒ **构造即失败**。
⇒ `stream.h:344` 的 `result.input_stream.ready = input_stream.ready` 不是
「改坏了语义」，而是**当前唯一能编译通过的写法**。实测（`/tmp/opencode/repro_fork.cpp`）：
`fork.input_stream.ready` / `fork.output_streams[0].ready` / `fork.output_streams[1].ready`
与调用方的 `in.ready` 是**同一个 impl 指针**。

### FIFO 数据通路的第二个根因：指针与计数脱钩

`include/chlib/fifo.h` 的 `sync_fifo`：`count` 用的是**带门**的
`will_write` / `will_read`（`:68-69`），而**写口与双指针**用的是**裸**信号
（`:76` / `:79-81` / `:84-86`）⇒ 计数与指针脱钩 ⇒ **静默数据损坏**：

- 满 FIFO 再写 ⇒ 写指针回绕 ⇒ **覆写队首**（实测排空得到 `[255,2,3,4]` 而非 `[1,2,3,4]`）；
- 空 FIFO 上 `rden=1` ⇒ 读指针自由运行 ⇒ 之后写入的元素**永远读不出来**
  （实测 `q=0x0`，而 `count` 照样归零）。

### 本仓已有的正确参照

- `include/chlib/fifo.h:112-177` 的 `fwft_fifo`：**门控部分**正确
  （`:134` `write_enable = wren & !is_full(count)`、`:145` `read_enable = rden & !is_empty(count)`、
  `:138-139` / `:148-150` 双指针受门）。
- `tests/test_verilator_memory_e2e.cpp:106-125` 的 `SyncFifoFixture`：**已实现**同一套门控，
  且**已跑过 interpreter + verilator 双后端**。

⚠️ 反例：`examples/spinalhdl-ported/fifo/stream_fifo_example.cpp:54-55` 的
`rd_ptr->next` / `wr_ptr->next` **没有**空/满门 ⇒ cycle 5 起进入**假满**并永久卡死
⇒ 它**不是**正确实现的参照，只是「0-cycle 数据通路」的参照。

## Decision

### 1. 输入 ready 的所有权：**两阶段**（决策 0-B）

> ⏸ **本决策本次不生效**：下面的 MUST（`*_connect_ready` 形态、`<<=` 连接手段、
> 读取规则）由 follow-up change `chlib-stream-ready-ownership-2026-10` 落地，
> **未收录**为 capability `chlib-stream-semantics` 的 in-force MUST
> （现存组合子代码不满足它们；见「不在本次范围」）。

组合子**不拥有**调用方的入参 ready 字段。它 MUST：

1. 在**构造期**只产出 payload / valid；
2. 提供一个 `<组合子>_connect_ready(result)` 形式的函数，由调用方在**设置好**
   `result.output_streams[i].ready` **之后**调用，用来把背压接到
   `result.input_stream.ready` / `result.input_streams[i].ready`。

**连接手段**：ready 链 MUST 用 `<<=`（数据流边）。MUST NOT 用 `operator=`
—— `include/chlib/stream.h:537-544` 的 `stream_mux_connect_ready` 正是这么写的，
它会**孤儿化调用方的 `in.ready` 节点** ⇒ 调用方继续读自己的字段会拿到一个
**无驱动的孤儿节点**（恒 0 / "Value not found"）⇒ **静默**。
⇒ 沿用该先例的**形态**，拒绝它的**连接手段**。

**读取规则（契约化）**：调用方 MUST 读 `result.input_stream.ready` /
`result.input_streams[i].ready`，MUST NOT 读自己的入参字段。
⚠️ 改成 `<<=` 后，调用方自己的 `in.ready` 字段将**不再被驱动** ⇒ 旧调用代码
若读自己的字段会**静默读到无驱动节点** ⇒ 这是**行为必需**的改动，不是文档活。

> **长期方向（本次不做）**：把入参改成 `ch_stream<T>&` 并用 `<<=` 写 ready，
> 是唯一能让背压彻底闭环的路径。但它**破坏公开 API 签名**、波及 6 个组合子的全部
> 调用点 ⇒ 单独立项。在契约写出前改签名 = 把未定义的东西固化成公开 API。

### 2. FIFO 时序契约

#### 2.1 `sync_fifo` MUST 自保护

写口、写指针、读指针 MUST 全部使用与 `count` **同源**的 `will_write` / `will_read`。
⇒ 契约从「**调用方保证** `rden` 只在非空时拉高」变为「**自保护**」。

连带语义（MUST 写明，否则又是未定义行为）：
- **满时同拍读写 ⇒ 本拍写被丢弃**（`will_write = wren & !is_full(count)` = 0，
  `will_read` = 1）。与 `fwft_fifo:134` 同形。

#### 2.2 `chlib::stream_fifo` 的输出 MUST 是 FWFT（0-cycle）

`pop_stream.payload` MUST 与 `pop_stream.valid` **同拍**有效，且呈现**当前队首**
（show-ahead）。

- 实现形态：给 `sync_fifo` 增加一个 `aread` 读端口并把结果暴露为独立字段
  （`q_async`）；`sync_fifo::q` 的 **1-cycle `sread` 语义保持不变** ⇒
  既有断言零改动。禁止在 `stream_fifo` 内重抄指针/count 引擎；禁止动
  `fifo.h:64` 的 `sread`（否则 `sync_fifo::q` 的公开契约被静默改掉）。
- **首拍 write-through**：`tick()` 的求值序是 **comb → posedge → comb**
  （`src/simulator.cpp:308` / `:328`），写端口在 `sequential_instr_list_`、
  `aread` 在 `combinational_instr_list_` ⇒ 沿后第二次 comb 读到的是**刚写入的值**。
  ⇒ 契约：**push 沿之后同拍 `valid == 1` 且 `payload == 刚写入的数据`**。
  这与 `sim-mem-read-timing-align-2026-10` 已反转的 race 期望（写后值）一致。
- **pop 沿之后 `payload` 指向下一个队首**（`aread` 组合读 ⇒ 沿后立即反映新
  `read_ptr`）—— 与 ADR-044 裁决 5 **同型**。
- ⚠️ **同名不同义**：`sync_fifo::q` 是「**沿弹出**」语义（1-cycle 寄存视图），
  `stream_fifo::pop_stream.payload` 是「**队首 show-ahead**」语义（0-cycle）。
  两者 MUST NOT 被当作可互换。

#### 2.3 `DEPTH` MUST 是 2 的幂

`compute_idx_width(DEPTH)` 使非 2 的幂的 `DEPTH` 映射到**下一个** 2 的幂
（`DEPTH=5` ⇒ 深度 8）⇒ 「FIFO 深度」本身未定义。
⚠️ 外溢面：`include/chlib/stream_builder.h:56` 直接实例化 `stream_fifo<T, DEPTH>`
⇒ 该约束同时成为 `StreamBuilder` 的公开约束。

#### 2.4 `fwft_fifo` **不是** FWFT

实测：写入 0xA1 / 0xB2 后 `q` 恒为 `0x0`，直到发起读才出现 0xA1
⇒ 其 `output_reg` 更新条件（`fifo.h:152-160`）有缺陷。
⇒ capability MUST 写明「`fwft_fifo` 不是 FWFT，勿用」；
其重写 / 改名**单独立项**（本次不改名、不删除）。

### 3. 组合子的构造期纪律

> ⏸ **本决策本次不生效**：下面的 MUST（构造期不读未赋值字段 / 不硬编码常量
> ready / grep-lint 自动化检查）由 follow-up change
> `chlib-stream-ready-ownership-2026-10` 落地，**未收录**为 capability
> `chlib-stream-semantics` 的 in-force MUST（现存组合子——含本 ADR 同时落地的
> `stream_fifo`——不满足其中的常量 ready 禁令；见「不在本次范围」）。

- 组合子 MUST NOT 在构造期读**尚未赋值**的字段（⇒ null operand ⇒ 构造即炸；
  这条是**自执行**的，配一条「每个组合子都能被实例化且不报 CHERROR」的用例即成护栏）。
- 组合子 MUST NOT 硬编码常量 ready（`ready = true` 之类）。
  ⚠️ 类型层面不可查 ⇒ 配 grep-lint 脚本（仿 `scripts/verify_rst_n_wiring.sh`）。
- FIFO 类组件 `empty` 时的数据输出 MUST 是 **don't-care**：
  MUST NOT 被强制为 0（那会用**假的**「数据是 0」掩盖真空），测试 MUST NOT 在
  `empty` 时断言 payload。⚠️ 两后端实测都返回 0（零初始化）⇒ 极易「顺手钉 0」。

### 4. 契约先行

代码改动 MUST 与 capability delta **同批落地**。
⚠️ 「先改代码、后补契约」正是 `2991eed` 被 REJECT 的同一种形状。

## Consequences

### 正面

- `chlib::stream_fifo` 的数据通路**第一次可用**（此前推入的数据读不出来）。
- `sync_fifo` 的契约从「调用方保证」变为「自保护」⇒ 计数与指针不再脱钩。
- 组合子现有 ready 行为的**边界被写明**（何为 undefined、何为移交 follow-up）；
  真正的 ready 所有权契约将由 `chlib-stream-ready-ownership-2026-10` 定义。

### 负面 / 代价

- `sync_fifo` 的**公开契约变更**（自保护）⇒ 既有依赖「调用方保证」的代码行为改变。
- `sync_fifo::q_async` 是**公开 API 新增字段**。
- `stream_fifo` 的 `pop_stream.payload` 时序从 1-cycle 变 0-cycle ⇒
  消费方 MUST 在 pop 沿**之前**采样（ADR-044 裁决 5 同型）。
- `DEPTH` 的 2 的幂约束是**新的公开约束**（影响 `StreamBuilder`）。

### 不在本次范围

- 决策 0-A（入参改引用）与裁决 1/3 全部 MUST 的落地（两阶段 ready、
  `<组合子>_connect_ready`、构造期纪律 + grep-lint）—— 单独立项
  `chlib-stream-ready-ownership-2026-10`；capability 未收录这些为 in-force MUST。
- 真轮转 `sequence`、`fwft_fifo` 重写、`stream_fork` 真 async 语义 —— 单独立项。
- 打包式 bundle API 的编译期拒绝（`fifo_bundle` 超宽）—— 与本 ADR 无关，
  且爆炸半径最大（至少 2 处编译失败 + 1 个运行时护栏重写 + 1 个 spec MODIFIED）
  ⇒ 独立立项。
- `examples/.../fifo/stream_fifo_example.cpp` 的指针门控修复 —— 该 example 是
  **反例**（假满），修复单独立项；本 ADR 只把它降级为「数据通路参照」。

## 参考

- `openspec/specs/mem-read-timing/spec.md:43-44`（同拍写 + 同址 `aread` ⇒ 写后值）
- `openspec/specs/dual-backend-test-coverage/spec.md`（`DUAL_BACKEND_TEST_PAIR`）
- `docs/simulation/ARCHITECTURE.md`「Memory read port timing」
- `include/core/bundle/bundle_base.h:184-196`（`<<=` vs `=` 的正规定义）
