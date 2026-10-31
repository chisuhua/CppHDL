# Design: stream 组合子的 ready 所有权（ADR-054 形态 B）

> 承载**逐条推导** / **探针证据** / **变异清单** ⇒ ADR 正文只留决策。

## 1. 缺陷与违反面（实测）

```cpp
// include/chlib/stream.h:286（修复前）
ch_bool pop_ready = true;
...
auto fifo_result = sync_fifo<...>(..., pop_ready);   // 读使能 = 常量
result.pop_stream.ready = pop_ready;                  // 假设输出端总是就绪
```

⇒ ⇒ 消费端恒就绪 ⇒ ⇒ FIFO 永远在排空 ⇒ ⇒ **`full` 不可达**
⇒ ⇒ 「FIFO 满 ⇒ 上游暂停」**不可演示** ⇒ ⇒ 2 个 disabled sample 的全部价值依赖它。

⚠️ **违反已签批 ADR**：ADR-045:160「组合子 MUST NOT 硬编码常量 ready
（`ready = true` 之类）」。

⚠️ 硬编码常量 ready 共 **3 处**：`stream.h:251`（`out_ready = true`）·
`stream.h:286`（`pop_ready = true`）· `stream.h:488`（`out_ready = true`）。

## 2. 决策 0-B 为何**不足**（承重区分）

| 组合子类 | 对 ready 做什么 | 需要代理？ |
|---|---|---|
| mux / demux / arbiter | **驱动**结果字段 `input_streams[i].ready`（节点**已存在** ⇒ 直接 `<<=`） | ❌ 否 |
| **fifo** | **读取**消费端 ready 作**内部读使能**（构造期就要用 ⇒ 而消费端 ready 是**结果字段** ⇒ **时序悖论**） | ✅ 是 |

⇒ ⇒ 决策 0-B 的两阶段对 mux 类**足够**，对 fifo 类**不够** ⇒ ⇒ 需另立形态。

## 3. 两条实测约束（ADR-054 §1）

- **E1**：**未驱动**的 `ch_bool` 代理节点**无法驱动任何输出**
  —— `ch_out::operator<<=` 要求 driver 非 null（`core/io.h:459`）·
  `ch_out::operator=` 要求 `src_lnode` 非 null（`core/io.h:119`）⇒ **两条路都不通**；
- **E2**：代理 + 阶段 2 覆盖 ⇒ 阶段 1 → 2 之间存在**读临时值**的窗口
  ⇒ ⇒ 「未调用 `connect_ready` 就仿真」读到**临时值** ⇒ ⇒ **静默**。

⇒ ⇒ 形态 A（代理 + 覆盖）**否决**：接受 E2 窗口（违反「要么语义正确、要么
**响亮拒绝**」总原则）+ 依赖「`operator<<=` 走 `set_src(0,…)` ⇒ 覆盖已有驱动」
这一**实现细节** ⇒ ⇒ **不具可移植性**。

## 4. 形态 B（延迟构造）的端到端探针（✅ 已通过）

探针：`/tmp/opencode/t3a/p.cpp`

```
consumer_ready=0 ⇒ fifo_read_en=0 (期望 0)
consumer_ready=1 ⇒ fifo_read_en=1 (期望 1)
```

⇒ ⇒ 「阶段 1 空壳 → 调用方驱动消费端 ready → `connect_ready` 建内部 FIFO，
读使能取**同一节点**」**端到端可行** ✔

⇒ ⇒ 且两条 `<<=` 实现**都会**在节点为 null 时**建节点**：
`ch_uint<N>::operator<<=`（`core/uint.h:158-162`）·
`ch_bool::operator<<=`（`lnode/bool.tpp:59-63`）⇒ ⇒ 形态 B 的前提成立 ✔

## 5. 实施面（待实施，本 change）

| 文件 | 改什么 |
|---|---|
| `include/chlib/stream.h` | `stream_fifo` 改**延迟构造** + 新增 `stream_fifo_connect_ready(result)`；`StreamFifoResult` MUST 携带**输入 stream**（否则 `connect_ready` 无从建 FIFO）；`stream_queue` 别名同步；`stream_mux_connect_ready` 的连接手段 `operator=` ⇒ **`<<=`** |
| `include/chlib/stream_builder.h` | `StreamBuilder::queue<DEPTH>()` 的调用点同步 |
| `samples/spinalhdl_stream_example.cpp` | 调用点迁移（调 `connect_ready`） |
| `samples/stream_component_example.cpp` · `samples/stream_handshake_example.cpp` | **归位**（迁移到新 API）+ 修 **6 处** `ch_in` 直写 |
| `samples/disabled_samples.txt` | 移除这 2 行 + 更新判定基准（4 → 2） |
| `scripts/verify_stream_ready_ownership.sh`（**新建**） | ① 构造期纪律 grep-lint（禁硬编码常量 ready）② 调用点护栏（调 `stream_fifo` 后 MUST 有 `*_connect_ready`） |

## 6. 判据设计（每条 MUST 能**转红**）

| 判据 | 内容 | 防什么 |
|---|---|---|
| **R0** | 护栏自身可运行 + fixture 存在（**fail-closed**） | ⚠️ 本仓「护栏自身有盲区」病症（fixture 缺失被当成「通过」） |
| **R1** | grep-lint：`stream.h` MUST NOT 有 `ready = true` / `pop_ready = true` 等**常量** ready | 硬编码常量 ready 复发 |
| **R2** | **反向 fixture**：注入一处硬编码 ⇒ MUST 转红 | R1 恒真（无判别力） |
| **R3** | 调用点护栏：`stream_fifo(` / `stream_queue(` / `queue<` 的调用点后 MUST 有对应 `*_connect_ready` | 旧调用点「**编译仍过但行为错**」 |
| **R4** | **运行期**：未调 `connect_ready` 就读结果 ⇒ MUST 可诊断失败（NOT 读到 0/常量） | 「静默失效」 |
| **R5** | **运行期**：`full` **可达** ⇒ 反压场景可断言 | 硬编码 ready 未真正修掉 |

## 7. 变异清单（每条**先确认编译成功**）

| # | 注入 | 期望转红 |
|---|---|---|
| **M1** | 读使能退回常量 `true` | R1 + R5 |
| **M2** | `connect_ready` 里改用**代理**节点（形态 A） | R4（同一节点断言）+ R5 |
| **M3** | 删掉 `stream_fifo_connect_ready` 的调用（某调用点） | R3 + 运行期用例 |
| **M4** | `stream_mux_connect_ready` 退回 `operator=` | R3（连接手段检查） |

## 8. 非目标

- ADR-045 决策 0-A（入参改引用）的签名变更；
- `fwft_fifo` 重写 / 改名（ADR-045:148 单独立项）；
- 其余组合子的 ready 所有权（T3-b）；
- `examples/.../fifo/stream_fifo_example.cpp` 的指针门控（ADR-045:197-198）。

## 8. 实施期**新增**约束（前置分析产出；ADR-054 未记载 ⇒ MUST 携带）

⚠️ 这两条是**实施前置检查**（`StreamFifoResult` 使用点审计 + `ch_stream` 拷贝语义
核实）产出的 ⇒ ⇒ **ADR-054 未记载** ⇒ ⇒ **实施时 MUST 携带**。

### 8.1 `StreamFifoResult` MUST 用**拷贝构造**接收输入，MUST NOT 用 `operator=`

`StreamFifoResult` MUST 携带输入 stream（否则 `connect_ready` 无从建 FIFO）⇒ ⇒
**怎么存**是有陷阱的：

- ❌ **`operator=`（节点重绑定）** —— `bundle_base::operator=`（`bundle_base.h:214`）
  是**节点重绑定**语义 ⇒ ⇒ ADR-042:48 明文「MUST NOT 用 `operator=` 的
  **节点重绑定**语义」⇒ ⇒ 用它会把输入的字段节点**换掉** ⇒ ⇒ 拷贝来的输入
  变成**孤儿节点** ⇒ ⇒ **静默**；
- ✅ **拷贝构造** —— bundle 的普通拷贝**共享字段节点**
  （`detach_field_nodes()` 才是**显式**分离，ADR-042 D4）⇒ ⇒ 存进结果对象的
  输入，其 `payload` / `valid` 节点与调用方的**是同一个** ⇒ ⇒
  `connect_ready` 里读到的**仍是同一节点** ✔

⇒ ⇒ **结论**：形态 B 的「输入传递」**可行**，但 MUST 走**拷贝构造**
⇒ ⇒ `StreamFifoResult` 需要一个**带参构造**（同时保留默认构造）
⇒ ⇒ ⚠️ 它当前是 **aggregate** ⇒ ⇒ 加 ctor 会失去 aggregate 初始化能力
⇒ ⇒ MUST 核对**全部** aggregate 初始化点（实测：`StreamFifoResult` 的使用点
**只在 `include/chlib/stream.h` 内部 4 处** ⇒ ⇒ 无外部 aggregate 初始化 ⇒ ⇒ 安全）。

### 8.2 `pop_stream.ready` 必须是**结果对象自己的**字段节点

调用方 MUST 驱动 `result.pop_stream.ready`（`result` 是**结果对象**，不是入参）
⇒ ⇒ 该字段节点由 `create_field_proxy_nodes()` 在 bundle 构造期建立
（`bundle_base.h:487`）⇒ ⇒ **已存在** ⇒ ⇒ 可被 `<<=` 驱动 ✔
⇒ ⇒ 且 `connect_ready` 里读到的**就是同一个节点** ✔
⇒ ⇒ ⇒ **无需代理节点** ⇒ ⇒ E1 不适用（ADR-054 §3 的结论得到实现层确认）。

### 8.3 爆炸半径实测（**比预估小**）

`StreamFifoResult` / `stream_fifo` / `stream_queue` 的使用点**全部**在
`include/chlib/stream.h` 内部（4 处：声明 / 定义 / 别名 / 内部构造）⇒ ⇒
**无外部使用者** ⇒ ⇒ 「结果类型新增字段」的**公开面**变更**不波及**任何
外部代码 ⇒ ⇒ 真正的外部影响面 = **调用点**（3 个活跃 + 2 个禁用）⇒ ⇒
这与 ADR-054 §4「旧调用点**编译仍过**但**行为错**」的判断**一致**
⇒ ⇒ ⇒ grep-lint 护栏仍是**必需**的。

## 9. 🔴 实施期**证伪**记录（2026-10-31，实施尝试后**已回退**）

⚠️ 本轮按 §5/§6 实施了形态 B（`stream_fifo` 延迟构造 + `stream_fifo_connect_ready`）
⇒ ⇒ **既有测试红了** ⇒ ⇒ **已 `git checkout` 回退**（仓恢复 **194/194** 全绿）
⇒ ⇒ 但这三条**实测事实**必须沉淀（它们**直接决定形态**）。

### 9.1 🔴 ADR-054 裁决 3.2 第 3 条（「未连接 ⇒ 响亮失败」）**在框架层不可实现**

实测：既有测试**直接**用 `stream_fifo`（**不调** `connect_ready`）⇒ ⇒
结果字段**未驱动** ⇒ ⇒ 读到的是 **0**（`test_stream_fifo_dual.cpp:145`
`out_empty == 1` 实测得 **0**）⇒ ⇒ **不是** "Value not found"。

⇒ ⇒ **框架对「读未驱动字段」返回 0，不报错** ⇒ ⇒ ⇒ 「响亮拒绝」**做不出来**
⇒ ⇒ ⇒ **ADR-054 §3.2 第 3 条 MUST 被修订**（当前形态 B 会让**存量调用点静默退化**）。

⚠️ 且 `tests/chlib/test_stream.cpp:470` 在形态 B 下**直接 SIGABRT**（不只是读到 0）
⇒ ⇒ 后果更严重 ⇒ ⇒ 更不可接受。

### 9.2 🔴 E1 的适用范围比 ADR-054 记的**更窄** ⇒ ⇒ 存在**第 4 个形态**

E1 的原文是「**未驱动**的代理节点**无法驱动任何输出**」（`ch_out::operator<<=`
要求 driver 非 null · `ch_out::operator=` 要求 src_lnode 非 null）⇒ ⇒
E1 的**限定词是「驱动输出」**。

⇒ ⇒ 而「代理**只**喂内部 `sync_fifo` 的 `rden`（**不作输出驱动**）」⇒ ⇒
**E1 不适用** ⇒ ⇒ ⇒ 存在形态 **E**：

| 形态 | 描述 | E1 | E2 窗口 | 破坏性 |
|---|---|---|---|---|
| **B**（ADR-054 采纳） | 延迟构造（结果在 `connect_ready` 前**不完整**） | — | 无 | 🔴 **破坏存量调用点**（§9.1） |
| **E**（**本轮实测浮现**） | **默认值 + 可覆盖**：`stream_fifo` **立即**建 FIFO（行为**逐位不变**），读使能接一个**默认驱动为 `true`** 的字段；`connect_ready` 把消费端 ready **覆盖**到该字段 | ❌ **不适用**（代理不作输出驱动） | ❌ **无**（默认值本身即合法的「恒就绪」**旧语义**） | ✅ **零破坏**（旧调用点**不需要**迁移） |

⚠️ **形态 E 的代价**：「组合子 MUST NOT 硬编码常量 ready」这条 MUST
**被违反**（默认值是常量 `true`）⇒ ⇒ ⇒ 需要 architect 裁决：
① **接受**形态 E ⇒ ⇒ 该 MUST MUST 改写为「**当调用方需要背压时** MUST 调
   `connect_ready`；组合子 MUST NOT **无条件**忽略消费端 ready」；
② 或坚持形态 B ⇒ ⇒ 则 MUST 先**解决** §9.1（框架层「读未驱动字段返回 0」
   ⇒ ⇒ 需要框架提供「未驱动字段的诊断」机制 ⇒ ⇒ **框架层能力缺口**
   ⇒ ⇒ 需**另一篇 ADR** + `core/` 改动 ⇒ ⇒ 爆炸半径**显著扩大**）。

### 9.3 其它实测（供下轮直接用）

- ✅ `sync_fifo` 的写口**自保护**（`chlib/fifo.h:74`
  `will_write = wren & !is_full(count)`）⇒ ⇒ **写使能只需 `valid`**
  ⇒ ⇒ 「写使能 = `valid & ready`」是**先读后写**⇒ ⇒ **死结**（`ready` 尚未被驱动）
  ⇒ ⇒ **MUST NOT** 那样写；
- ✅ **端到端探针**（`/tmp/opencode/t3a/f.cpp`）验证形态 B 的**核心收益成立**：
  写 4 个后 **`full == 1`**（DEPTH=4）⇒ ⇒ **`full` 可达** ⇒ ⇒
  **反压场景确实可演示** ✔；第 5 个写 ⇒ `full` 仍 1 ⇒ ⇒ **自保护** ✔；
  `rready=1` ⇒ 依次弹出 `0x22` / `0x33` ⇒ ⇒ **FWFT show-ahead** ✔
  ⇒ ⇒ ⇒ **「读使能可外部化」这件事本身是可行的** ⇒ ⇒ 争的**只是形态**。
