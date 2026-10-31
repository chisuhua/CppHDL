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
| **R1** | **效果判据**（**MUST NOT** 词法）：消费端 ready == 0 ⇒ MUST **阻止推进**（运行期断言） | 「无条件忽略消费端 ready」复发 |
| **R2** | **反向 fixture / 原地投毒**：注入一处「无条件忽略」⇒ MUST 转红 | R1 恒真（无判别力） |
| **R3** | 调用点护栏：调 `stream_fifo` 后 MUST 出现 `connect_ready` **或** 对 `pop_stream.ready` 的 `<<=` ⇒ ⇒ **或**显式豁免标记（豁免**非静默**） | 「忘了接」被误当成「故意恒就绪」 |
| **R4** | **运行期**：调用方**已自行驱动** ready ⇒ `connect_ready` MUST **抛异常** | 多驱动 last-wins 静默 |
| **R5** | **运行期**：`full` **可达**（DEPTH=4 ⇒ 写 4 个后 `full == 1`）⇒ ⇒ 反压可断言 | 硬编码 ready 未真正修掉 |
| **R6** | **结构**：读使能 MUST 引用**字段节点本身**（MUST NOT 局部节点）⇒ ⇒ 覆盖 MUST **传播** | **覆盖不传播 ⇒ 静默失效**（测试全绿而功能死） |

⚠️ ⚠️ **R1 MUST NOT 是词法判据**（critic BLOCKER 2 ②）：形态 E 的**合法**默认驱动
（`ch_bool pop_ready = true`）必然触发 grep-lint ⇒ ⇒ 变异 M1 与**正确实现同构**
⇒ ⇒ **判别力归零** ⇒ ⇒ 故 R1 改为**效果判据**。

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
| **E**（**本轮实测浮现**） | **默认值 + 可覆盖**：`stream_fifo` **立即**建 FIFO（行为**逐位不变**），读使能接一个**默认驱动为 `true`** 的字段；`connect_ready` 把消费端 ready **覆盖**到该字段 | ❌ **不适用**（代理不作输出驱动） | ❌ **无**（默认值本身即合法的「恒就绪」**旧语义**） | ✅ **零破坏**（旧调用点**不需要**迁移）⚠️ **代价 = 契约强度削弱**（默认值仍是常量 ⇒ 「禁硬编码常量 ready」这条 MUST MUST 被**改写**，见 §10.6 的**限定条件**） |

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

## 10. 形态 E 的**机制规范** + **待 architect 修订清单**（2026-10-31，oracle + critic 评审后）

⚠️ **critic 结论 = `REJECT`**（`mergeable: no`）⇒ ⇒ **2 个 BLOCKER**
⇒ ⇒ **方向成立**（形态 B 已被 §9 证伪 · 形态 E 成立）⇒ ⇒ **但全套工件仍写着形态 B**
⇒ ⇒ ⇒ **修订是前置动作** ⇒ ⇒ 且 **ADR-054 的修订属 architect 权限**
⇒ ⇒ ⇒ **builder 不得自行改已签批 ADR 的决策内容** ⇒ ⇒ 本节只登记**待修订清单**
+ **实施必需但与最终措辞无关的机制规范**。

### 10.1 🔴 BLOCKER 1（arch）—— ADR-054 须**原地修订**（不另立 ADR-055）

⚠️ ADR-054 **同日签批 · 从未实施 · 无 in-force spec 依赖** ⇒ ⇒ **原地修订**即可
⇒ ⇒ **不得**两篇 Accepted 并存（否则实施方无法判断哪版有效）。

| # | ADR-054 位置 | 修订要求 |
|---|---|---|
| ① | §3.1 候选表 | **增补形态 E 行**（采纳）；形态 B 改判「**实施期证伪否决**（依据 design.md §9.1 / §9.2）」 |
| ② | §3.1 否决 A 的理由 ② | **限定**为「否决的是**接受 E2 静默窗口**，**非**覆盖机制本身」；并**显式追认**：「`<<=` 对**已有驱动**的覆盖（`include/lnode/bool.tpp:59-63` 的 `set_src(0,…)`）为**公开契约语义**」（与 `ch_reg::next` last-wins 同源）⇒ ⇒ **不追认**则「不具可移植性」变**空洞** |
| ③ | §3.2 第 1 条 | 改写为**形态 E 条款**（默认驱动 + 可覆盖） |
| ④ | §3.2 第 3 条 | **降级为长期方向**（依赖「读未驱动字段诊断」这一**框架能力** ⇒ ⇒ 另案 ⇒ ⇒ 不得在本 change 内承诺） |
| ⑤ | §5 Verification | 形态 B 的判据**同步改写**（勿留两版） |

### 10.2 🔴 BLOCKER 2（plan）—— change 侧工件须与修订后的 ADR **同批**落地

| # | 位置 | 修订要求 |
|---|---|---|
| ① | delta Requirement 1 | 「MUST NOT 硬编码常量 ready」是**词法**判据 ⇒ ⇒ 形态 E 的合法默认驱动 `ch_bool pop_ready = true` 必然触发 grep-lint **误报** ⇒ ⇒ 变异 M1 与**正确实现同构** ⇒ ⇒ **判别力归零** ⇒ ⇒ MUST 改写为**效果判据**：「组合子 MUST NOT **无条件**忽略消费端 ready ⇒ ⇒ 存在路径使 `ready == 0` 阻止推进」 |
| ② | delta Requirement 2 / 4 + 对应 Scenario + proposal AC + design R4 | 全部改写为**形态 E 语义**（未连接 = **恒就绪旧语义**，**合法**；「读到常量」不再是缺陷）⇒ ⇒ 否则**归档即冻错主 spec** |
| ③ | design §6 判据表 | 词法 R1 替换为**结构 / 行为**判据（见 §10.3） |

### 10.3 形态 E 的**机制规范**（实施必需；与最终措辞无关 ⇒ 先钉死）

⚠️ oracle 指出：形态 E 的**核心机制未钉死** ⇒ ⇒ 实现可合法选出一个**覆盖不传播**的变体
⇒ ⇒ MUST 写清：

1. **默认驱动的建立方式**：`stream_fifo` 构造期 MUST 为读使能字段建立一个
   **默认驱动**（`true` = 旧语义「恒就绪」）⇒ ⇒ 该默认 MUST 是**可被覆盖**的
   （`<<=` 走 `set_src(0,…)` ⇒ 覆盖已有驱动）；
2. 🔴 **读使能 MUST 引用「字段节点本身」**（`result.pop_stream.ready` 的 proxy），
   **MUST NOT** 引用**局部** `pop_ready` 节点
   ⇒ ⇒ 否则 `connect_ready` 覆盖的是**字段**、而内部 `sync_fifo` 表达式仍读
   **局部节点** ⇒ ⇒ **覆盖不传播 ⇒ 静默失效**（测试可能全绿而功能死）
   ⇒ ⇒ 与现缺陷**同型** ⇒ ⇒ **MUST** 落成一条可判别的判据；
3. **`connect_ready` 的职责**：MUST 把消费端 ready **覆盖**到该字段节点 ⇒ ⇒
   **MUST NOT** 代理（代理 ⇒ E1）；⇒ ⇒ **签名/时序义务** MUST 写进契约
   （调用方先设好消费端 ready，再调 `connect_ready`）；
4. **多驱动守卫**：`StreamFifoResult` MUST 记录默认驱动节点指针 ⇒ ⇒
   `connect_ready` MUST 断言「当前 `src(0)` == 默认节点」⇒ ⇒ 否则
   **`CHFATAL_EXCEPTION`**（⚠️ MUST NOT 用 `CHERROR` —— 实测**只打日志不抛**，
   ADR-050 已踩过）；⇒ ⇒ 同型守卫：调用方若**已自行驱动** `in.ready`
   （字段节点**共享**）⇒ ⇒ 组合子侧驱动前 MUST 检查既有驱动。

### 10.4 「忘了调 `connect_ready`」vs「故意恒就绪」**不可区分**（残余风险）

⚠️ 形态 E 下两者**观测等价** ⇒ ⇒ **接受**该残余风险，但 MUST：
① 在 ADR 的**后果 / 代价**章**登记**为**已接受**；
② 用**调用点护栏**在**意图层**区分 ⇒ ⇒ 沿用本仓 `rst_n_guard: intentionally-unwired`
先例：调 `stream_fifo` 后 MUST 出现 `connect_ready` **或** 对 `pop_stream.ready`
的 `<<=` ⇒ ⇒ **或**显式豁免标记 ⇒ ⇒ 豁免**非静默**（护栏 `::notice` 点名）。

### 10.5 变异清单的**修正**（critic F3）

| # | 注入 | 判别力 | 修正 |
|---|---|---|---|
| **M1** | 读使能改引用**局部常量节点** | ✅ 钉住 oracle 承重依赖 ②（「覆盖不传播」） | 保留（**这是最关键的一条**） |
| ~~M2~~ | ~~`connect_ready` 覆盖改 no-op~~ | 🔴 **钉不住** —— 若调用方**直接** `<<=` 字段，则「正确的 `connect_ready`」本就**可能是 no-op** ⇒ ⇒ 平凡通过 | ⚠️ **替换**为 **M2''**：「字段连接 `<<=` 换 `operator=`（重绑定）」⇒ ⇒ 字段**孤儿化** ⇒ ⇒ R5 转红 ⇒ ⇒ 钉住「连接手段 MUST `<<=`」 |
| **M3** | 删掉双驱动守卫 | ⚠️ **需配套一个「真实双驱动」的用例**，否则**恒绿** | MUST 补该用例（否则删了守卫也看不出来） |

### 10.6 ⚠️ 契约「削弱」的**先例风险**（critic F2 ③）

「默认 `true` 豁免」**MUST 限定**于「读使能是**内部逻辑的输入**且默认值 = **既有旧语义**」
的 **fifo 类** ⇒ ⇒ **转发类**（mux / demux / arbiter）**硬编码常量 ready** =
**无条件忽略** ⇒ ⇒ **仍禁止** ⇒ ⇒ 否则形态 E 会变成「其它组合子硬编码 ready 也合法化」
的**先例**。

### 10.7 commit 划分（oracle + critic 一致 ⇒ 可用）

| # | 内容 |
|---|---|
| **C1** | 形态 E + 多驱动守卫 + **运行期**行为判据（`full` 可达 · 背压暂停 · FWFT 序列 · 双驱动抛异常） |
| **C2** | 2 个 disabled sample 归位 + **6 处** `ch_in` 修复 + `disabled_samples.txt` 更新 |
| **C3** | mux `operator=` ⇒ `<<=` 迁移 + grep-lint 护栏 + capability delta（归档**禁** `--skip-specs`） |

⚠️ ⚠️ **顺序约束**：**C3 的 delta MUST 与修订后的 ADR-054 同批落地**
⇒ ⇒ ⇒ **ADR-054 的修订（architect）必须先于 C1**，且 **C1~C3 之间不可插 ADR 修订**
⇒ ⇒ ⇒ 避免「实现依据的 ADR 版本」与「归档依据的 ADR 版本」不一致。

## 11. 🔴 形态 E 的**实施尝试 2**（已回退）—— 「零回归」承诺**未达成**

⚠️ 2026-10-31：按 ADR-054 修订（形态 E）实施 ⇒ ⇒ 编译通过 ⇒ ⇒ 但
**既有测试 SIGSEGV** ⇒ ⇒ **已 `git checkout` 回退**（仓恢复 **194/194** 全绿）。

⇒ ⇒ 🔴 **形态 E 的「零回归」承诺目前【未】被验证通过** ⇒ ⇒
**不得**宣称形态 E 已成立 ⇒ ⇒ ⚠️ ADR-054 的「形态 E 采纳」**需要再验证**
⇒ ⇒ 下一步**必须**先做**最小隔离复现**，定位 SIGSEGV 的根因。

### 11.1 实施了什么（最小形态）

- `stream_fifo` 改为：**立即**建内部 FIFO（**既有行为不变**）；
- 读使能改为引用 **`result.pop_stream.ready`**（结果对象**自己的字段节点**）
  ⇒ ⇒ 调用方覆盖它 ⇒ ⇒ 覆盖**传播**（oracle 的承重依赖 ②）；
- 新增 `stream_fifo_connect_ready(result, consumer_ready)`：
  `result.pop_stream.ready <<= consumer_ready;`（覆盖默认驱动）；
- 默认驱动 = `true`（既有旧语义「恒就绪」）。

### 11.2 🔴 失败现象

```
tests/chlib/test_stream_fifo_dual.cpp:133  →  FAILED: SIGSEGV
tests/chlib/test_stream.cpp:470            →  FAILED: SIGSEGV
```

⚠️ **无栈**（Catch2 捕获了 SIGSEGV，未给 backtrace）⇒ ⇒ 根因**未定位**。

### 11.3 ⚠️ 中途排除的一个假设（**有价值的新事实**）

初版实现用 **`<<=`** 驱动 `result.pop_stream.ready`（bundle field 的 proxy）
⇒ ⇒ **同样 SIGSEGV** ⇒ ⇒ 改回现状的 **`operator=`（节点重绑定，行为逐字不变）**
⇒ ⇒ **仍然 SIGSEGV** ⇒ ⇒ ⇒ 「`<<=` 不能驱动 bundle field proxy」这条假设
**【未】成立**（两者都崩）⇒ ⇒ ⚠️ 故**不能**把根因归给「`<<=` vs `operator=`」。

⇒ ⇒ ⇒ **下一步**：用**最小隔离复现**（单文件 + 一个 `stream_fifo` 调用）
定位 SIGSEGV ⇒ ⇒ 再决定形态 E 的实现细节。

### 11.4 其它实测

- ✅ `logic_buffer` 的**拷贝构造 = 共享节点**
  （`include/lnode/logic_buffer.tpp:16-17` `node_impl_(other.node_impl_)`）
  ⇒ ⇒ 「默认驱动」可随结果对象传出去 ⇒ ⇒ 形态 E 的**前提之一成立**；
- ✅ `ch_bool(bool)` **会建 literal 节点**（`include/lnode/bool.tpp:12-14`
  `build_literal`）⇒ ⇒ 「用常量 `true` 作默认驱动」的**源**是**有效**的
  ⇒ ⇒ SIGSEGV **不是**「源无效」导致；
- ⚠️ ⇒ ⇒ 因此 SIGSEGV 的根因**大概率**在「读使能引用**结果对象的字段节点**」
这条新路径上（`sync_fifo` 的 `rden` 参数按值拷贝 ⇒ ⇒ 共享该节点）
⇒ ⇒ **待最小复现确认**。

### 11.5 对 ADR-054 的影响

⚠️ ADR-054 已把形态 E 定为「采纳」，但**形态 E 的实现尚未通过「零回归」验证**
⇒ ⇒ ⇒ **ADR-054 的裁决状态 MUST 标注为「待验证」** ⇒ ⇒
⚠️ **builder 不得**据此继续实施 ⇒ ⇒ 需 architect 在**验证通过**后追认，
或在验证失败后**再次**改裁 ⇒ ⇒ ⇒ **不得**让「未验证的形态」进入归档。

## 12. 🔴 「覆盖不传播」的**根因**（2026-10-31，判别性探针命中 oracle 的承重依赖 ②）

⚠️ 本节记录**形态 E 的第 3 次实施尝试**（已回退）的**定位结果** ——
它是 oracle 指出的**承重依赖 ②**（「`rden` MUST 引用字段节点本身」）的**实测命中**。

### 12.1 现象（判别性探针，`/tmp/opencode/repro/e.cpp`）

```
写 0x11 后: full=0 empty=0 rvalid=1 rdata=0x11
写 0x22 后: full=0 empty=0 rvalid=1 rdata=0x22     ← 每写一个就弹一个
第 5 个写后: full=0 (期望 1)                        ← ❌ `full` 不可达 ⇒ 背压**未生效**
```

⇒ ⇒ **覆盖完全没生效**（FIFO 恒就绪 ⇒ 永远排空）⇒ ⇒ 与**形态 B 时期**的
「读到 0」**不同** ⇒ ⇒ 本次是**更隐蔽**的失效（看起来一切正常，只是 `full` 永不置位）。

### 12.2 ⚠️⚠️ **订正（2026-10-31，第二次）**：§12.2 的根因推断**【已被实测排除】** ❌

⚠️ 本节初版断言「根因 = `operator=` 的重绑定指向**局部**变量的节点 ⇒ ⇒ 该节点随
局部变量**失效**」⇒ ⇒ **该推断【错误】**，依据两条**实测**：

| 实测 | 结果 | ⇒ ⇒ 排除的假设 |
|---|---|---|
| `grep` 析构 / 引用计数 / `release_node`（`include/core/logic_buffer.h` · `include/lnode/logic_buffer.tpp` · `include/lnode/bool.tpp` · `include/core/lnodeimpl.h`） | **零命中** ⇒ ⇒ **无析构、无引用计数** | ❌ 「节点会随 C++ 对象析构而失效」 |
| **节点 id 实测**（`/tmp/opencode/repro/e.cpp` + 临时诊断）：`stream_fifo` 内部 `result.pop_stream.ready` = **18** · 调用方 `r.pop_stream.ready` = **18** · `connect_ready` 之后 = **18** | **同一节点** ⇒ ⇒ 覆盖**落在同一节点上** | ❌ 「调用方与内部不是同一节点」 |
| `select` 的实现（`include/core/operators.h:147-153`） | 走 `ternary_operation<mux_op>` ⇒ ⇒ **硬件 mux** ⇒ ⇒ **运行期**求值 | ❌ 「`select` 的条件在建图期求值」（本仓的经典坑，但**此处不适用**） |

⇒ ⇒ ⇒ **三条假设全部排除** ⇒ ⇒ **真根因仍未定位** ⇒ ⇒ ⇒ ⚠️ **不得**把任何
未定位的推断当「结论」⇒ ⇒ ⇒ 下一步**必须**继续实测（见 §12.6）。

⚠️ ⚠️ **元教训**（本仓第 N 次）：**「推理出的根因」必须被「实测」确认**才可写入结论
⇒ ⇒ 本节保留被推翻的推断（**不删**）⇒ ⇒ 作为「已排除清单」的一部分 ⇒ ⇒
避免下轮重走。

⚠️ 现状（改动前）的写法：

```cpp
ch_bool pop_ready = true;                        // ← **局部**变量
result.pop_stream.ready = pop_ready;             // ← 重绑定：字段节点 = 那个 literal 节点
```

⇒ ⇒ `result.pop_stream.ready` 的节点**就是** `pop_ready` 的 literal 节点
⇒ ⇒ 而 `pop_ready` 是 `stream_fifo` 的**局部** ⇒ ⇒ **函数返回时它析构**
⇒ ⇒ ⇒ **该节点随之失效** ⇒ ⇒ 而内部 FIFO 的 `rden` 与调用方拿到的
`result.pop_stream.ready` **都指向它** ⇒ ⇒ ⇒ 调用方
`result.pop_stream.ready <<= consumer_ready;` 是**往一个已失效的节点上覆盖**
⇒ ⇒ ⇒ **覆盖不传播** ⇒ ⇒ **恒就绪**。

⇒ ⇒ ⇒ ⚠️ **这解释了「重绑定」为什么危险**：它让字段的**生命周期**绑在
**另一个对象**上 ⇒ ⇒ 而那个对象（局部变量）的生命周期**短于**结果对象。

### 12.3 ⚠️ 排除的假设（避免下轮重走）

| 假设 | 结论 |
|---|---|
| 「`<<=` 不能驱动 bundle field 的 proxy」 | ❌ **不成立** —— 用 `operator=` 与用 `<<=` **同样**崩（那次崩在 `<<=` + **临时字面量源** ⇒ 见 §12.4） |
| 「`ch_bool(bool)` 字面量无效」 | ❌ **不成立** —— `include/lnode/bool.tpp:12-14` 确实 `build_literal` ⇒ **有效** |
| 「`logic_buffer` 拷贝会**新建**节点」 | ❌ **不成立** —— `include/lnode/logic_buffer.tpp:16-17` 是 `node_impl_(other.node_impl_)` ⇒ ⇒ **共享节点**；`bundle_base` 的拷贝构造（`include/core/bundle/bundle_base.h:66-70`）只拷贝 base 的 impl ⇒ ⇒ **成员字段由 `ch_bool` 的拷贝处理 ⇒ ⇒ 仍共享** |

### 12.4 ⚠️ 另一条**独立**的坑（同时命中）

用 `<<=` 建立**默认**驱动时（`result.pop_stream.ready <<= ch_bool(true);`）⇒ ⇒
**源**是**临时 `ch_bool` 字面量** ⇒ ⇒ `get_lnode(ch_bool(true))` **无效**
⇒ ⇒ 驱动**未建立** ⇒ ⇒ `sync_fifo` 的 `rden` 拿到**无效操作数** ⇒ ⇒ 💥 **SIGSEGV**。

⇒ ⇒ ⇒ **两条坑的共同教训**：
① **`operator=`（重绑定）⇒ 节点的**生命周期**绑在**源对象**上 ⇒ ⇒ 源是**局部** ⇒ ⇒ 失效；
② **`<<=` ⇒ 源 MUST 是**持久的有效节点** ⇒ ⇒ **临时字面量**无效 ⇒ ⇒ 崩。

⇒ ⇒⇒ ⚠️ **【本推断亦已被实测排除】** —— 既然节点**不会**随局部变量失效 ⇒ ⇒
「把 literal 节点存进结果对象」**并不能**解决问题 ⇒ ⇒ ⇒ **下述「正确形态」作废**。

⚠️⚠️ **仍未定位的真根因**（下一步实测方向）：
① **`set_src(0, …)` 对「literal 节点」是否有效**？⇒ ⇒ 默认驱动若是 literal 节点
   ⇒ ⇒ 覆盖**打在常量节点上** ⇒ ⇒ 若框架**禁止**给 literal 节点加数据流边
   ⇒ ⇒ ⇒ **覆盖无效** ⇒ ⇒ **这与「id 相同但行为不变」完全吻合** ✔（待验证）；
② `get_lnode(ch_bool(true))` 为何**无效**（⇒ ⇒ 那个 SIGSEGV）⇒ ⇒ 需查
   `get_lnode` 对**临时对象**的处理。

⇒ ⇒ ⇒ **默认驱动的正确形态**（**已于 §14 实测确认成立**）：
**默认驱动的源 MUST 活得比结果对象久** ⇒ ⇒ 最直接的形态 =
**把那个 literal 节点存进结果对象**（新增成员持有它）⇒ ⇒ 默认驱动
`result.pop_stream.ready <<= result.default_ready_source;` ⇒ ⇒ 两者**同生命周期**
⇒ ⇒ 覆盖**必然传播** ✔

⚠️⚠️ **订正（2026-10-31，第三次）**：本节初版写的是
`result.pop_stream.ready = result.default_ready_source;`（**`operator=` 重绑定**）
⇒ ⇒ **该写法【实测无效】** ⇒ ⇒ 正确写法 MUST 是 **`<<=`**
⇒ ⇒ 因为 `operator=` 会把**字段节点本身**重绑定到 literal 节点 ⇒ ⇒
字段节点**变成**了 literal ⇒ ⇒ 而 **literal 节点不可被 `set_src`**
（§14 的 E-1）⇒ ⇒ 覆盖**无效** ⇒ ⇒ ⇒ **这正是「id 相同但行为不变」的机制** ✔
⇒ ⇒ ⇒ **两个约束缺一不可**：
① **目标** MUST 是**非 literal** 的节点 ⇒ ⇒ MUST 用 **`<<=`**（MUST NOT `operator=`）；
② **源** MUST 是**持久有效**的节点 ⇒ ⇒ MUST 由**结果对象持有**。

⚠️ **不可行的替代**：`static` 局部变量（节点永存）⇒ ⇒ **多实例共享**同一个
ready 节点 ⇒ ⇒ 一个实例的覆盖**影响另一个** ⇒ ⇒ **静默** ⇒ ⇒ ❌

### 12.6 ⚠️ 下一步的**实测**清单（按优先级）

| # | 实测 | 目的 |
|---|---|---|
| **E-1** | 对一个**纯 literal 节点**调 `set_src(0, src)` ⇒ ⇒ 观察它是否变成数据流边 | ⇒ 若**无效** ⇒ ⇒ **真根因锁定**（默认驱动落在 literal 节点 ⇒ 覆盖无效）⇒ ⇒ 修法 = 默认驱动 MUST 建在**非 literal** 的节点上（如一个 `mux` / `assign` 节点） |
| **E-2** | `get_lnode(ch_bool(true))` 的返回值 / `impl()` 是否为 null ⇒ ⇒ 追 `get_lnode` 对**临时对象**的路径 | ⇒ ⇒ 解释那个 **SIGSEGV** ⇒ ⇒ 决定默认驱动能否用 `<<=` 建 |
| **E-3** | 在 `sync_fifo` **内部**打印 `rden` 的节点 id ⇒ ⇒ 与 18 对比 | ⇒ ⇒ 确认内部读的**就是**节点 18（而非某个派生节点） |

⇒ ⇒ ⚠️ **E-1 是最可能一击命中的**（它直接解释「id 相同但行为不变」）。

### 12.5 ⇒ ⇒ 对 ADR-054 的影响（**结论不变**）

⚠️ 形态 E **仍然成立**（方向对：默认恒就绪 + 可覆盖）⇒ ⇒ 但**实现**必须
采用 §12.4 的「默认源随结果对象持有」形态 ⇒ ⇒ ⇒ **ADR-054 §3.2 第 1 条
需要补一句**：默认驱动的**源 MUST 活得比结果对象久**（MUST NOT 指向**局部**变量
的节点）⇒ ⇒ ⇒ ⚠️ 这是**第 4 次**实施尝试的**唯一**待办 ⇒ ⇒ 其余部分
（读使能引用字段节点本身 / 写使能只读 `valid` / 覆盖用 `<<=`）**已实测正确**
（既有测试 62/62 全绿 + 探针除 `full` 外全部符合手推）。

## 13. E-1 / E-2 的**实测结论**（2026-10-31）+ 第 4 次实施尝试

### 13.1 ✅ E-1：**literal 节点不可被 `set_src(0, …)` 变成数据流边**（**实测确认**）

隔离实验（`/tmp/opencode/e1/e1.cpp`）：

```
[DBG] lit node id=8   nonlit node id=11
s=0 ⇒ out_lit=1   out_nonlit=0
s=1 ⇒ out_lit=1   out_nonlit=1
```

⇒ ⇒ `out_lit` **恒为 1**（= literal 的原值）⇒ ⇒ **对 literal 节点的
`set_src` 完全无效**；⇒ ⇒ 而对照组（**mux 节点**）**正常跟随** `s` ✔

⇒ ⇒ ⇒ **「默认驱动重绑定到 literal 节点 ⇒ ⇒ 覆盖无效」这条机制成立** ✔

### 13.2 ✅ E-2：**proxy 节点可被 `set_src`**（**实测确认**）

同实验加第三组（`ch_stream` 的 `ready` 字段 = **bundle field proxy**）：

```
[DBG] proxy node id = 14
s=0 ⇒ out_lit=1   out_nonlit=0   out_proxy=0
s=1 ⇒ out_lit=1   out_nonlit=1   out_proxy=1
```

⇒ ⇒ **`out_proxy` 正确跟随 `s`** ✔ ⇒ ⇒ **proxy 可被 set_src** ⇒ ⇒
「目标 MUST 是非 literal 节点」这条**可行**。

### 13.3 🔴🔴 第 4 次实施尝试：**修法实测【成功】**（本节初版结论**错误** ⇒ 已订正）

⚠️⚠️⚠️ **订正（2026-10-31，第四次）**：本节初版断言「修法**实测仍不传播** ⇒ ⇒ 必有一个
尚未识别的机制」⇒ ⇒ **该断言【错误】** ⇒ ⇒ **错因 = 把探针输出【读反了】**。

按 13.1 / 13.2 的结论实施（**两个约束同时满足**）：

```cpp
result.default_ready_source = ch_bool(true);               // 源：**持久**的 literal 节点
result.pop_stream.ready <<= result.default_ready_source;   // 目标：**field proxy**（非 literal）
auto fifo_result = sync_fifo<...>(input_stream.valid, input_stream.payload,
                                  result.pop_stream.ready); // 读使能 = **字段节点本身**
```

⇒ ⇒ **编译通过** · **既有测试 62/62 全绿** ⇒ ⇒ **判别性探针的实测输出**：

```
写 0x11 后: full=0 empty=0 rvalid=1 rdata=0x11
写 0x22 后: full=0 empty=0 rvalid=1 rdata=0x11      ← 队首不动 ⇒ 未弹出 ⇒ **反压生效** ✔
写 0x44 后: full=1 empty=0 rvalid=1 rdata=0x11      ← **full 可达** ✔
第 5 个写后: full=1 (期望 1)                          ← **自保护** ✔
rready=1 后: full=0 rvalid=1 rdata=0x22 (期望 0x22)   ← **恢复弹出** ✔
再一拍:      rdata=0x33 (期望 0x33)                    ← **队列推进正确** ✔
```

⇒ ⇒ ⇒ **逐项对照手推金标：全部相符** ⇒ ⇒ ⇒ **覆盖【传播】了** ✔

⚠️⚠️ **错因解剖（**元教训**，本仓第 N+1 次**）：
初版把这段输出与「**未修复**时」的输出**并排**看 ⇒ ⇒ 误以为「队首停在 0x11」
是「**没弹出 = 没生效**」⇒ ⇒ **实际**语义恰好相反：

| | 未修复（恒就绪） | 修复后（反压生效） |
|---|---|---|
| 写 0x22 后队首 | **0x22**（弹出 ⇒ 排空） | **0x11**（**不弹出** ⇒ 反压） |
| 写 4 个后 `full` | **0**（永远排空） | **1**（**满** ✔） |
| 第 5 个写 | `full` 仍 0 | `full` 仍 1（自保护） |

⇒ ⇒ ⇒ ⚠️ **「队首不动」恰恰是【反压生效】的证据**，而不是失效的证据
⇒ ⇒ ⇒ ⚠️ **教训**：判别性探针 MUST 输出**显式 PASS/FAIL 判定**
（而非把期望值塞进 printf 的括号里让人肉眼比对）⇒ ⇒ ⇒
**自检已改成显式判定版**（§14.1）⇒ ⇒ **这才是本轮能一击定案的原因**。

⇒ ⇒ ⇒ ⚠️ **代价**：「形态 E 第 4 次尝试**失败**」这个结论已被**写入仓库**
（commit `fba4675`）⇒ ⇒ 本节是它的**订正** ⇒ ⇒ ⇒
**元教训强化**：**推理出的结论**（含「实测输出」的解读）**同样**必须被
**独立复核**才可写入结论 ⇒ ⇒ 不只是「根因推断」需要实测。

### 13.4 ⚠️ 已排除的假设（**5 条**，累计）

| # | 假设 | 排除依据 |
|---|---|---|
| 1 | 节点随 C++ 对象析构而失效 | `grep` 析构 / 引用计数 / `release_node` = **零命中** ⇒ **无析构、无引用计数** |
| 2 | 调用方与内部**不是**同一节点 | **节点 id 实测相同**（内部 = 调用方 = 覆盖后，**同一 id**） |
| 3 | `select` 的条件在**建图期**求值 | `select` → `ternary_operation<mux_op>` ⇒ **硬件 mux** ⇒ **运行期**求值 |
| 4 | **literal** 节点不可被覆盖 | ✅ **实测成立**（E-1）⇒ ⇒ ⚠️ **订正**：它**就是**根因（不是「与本次失败无关」）⇒ ⇒ 「默认驱动 MUST 用 `<<=` 打在**非 literal** 节点上」这条**正是修法** |
| 5 | **proxy** 节点不可被覆盖 | ✅ **实测不成立**（E-2 ⇒ proxy **可以**被覆盖） |
| 6 | 写使能 `valid & ready`（旧写法）是「不传播」的**原因** | ❌ **不成立** —— 它是**前置条件**而非失败原因：恢复旧写法 ⇒ ⇒ 直接 **`Invalid operands for binary operation`** ⇒ ⇒ 组合子**完全不可用** ⇒ ⇒ ⇒ **它是形态 E 契约的【必需】部分**（§14.3） |

### 13.5 ✅ E-3（已解决）：节点 id **相同** ⇒ ⇒ 「按值返回时字段节点未共享」**不成立**

**E-3 的实测**（临时诊断打印，`stream_fifo` 内部 vs 调用方）：

```
[DBG] stream_fifo: result.pop_stream.ready node id = 18
[DBG] caller: r.pop_stream.ready node id = 18
[DBG] caller: after connect_ready, node id = 18
```

⇒ ⇒ **三处 id 全同** ⇒ ⇒ 「按值返回时字段节点**未共享**」**不成立** ✔
⇒ ⇒ ⇒ 与代码阅读（`ch_stream() = default` ⇒ 隐式拷贝 ⇒ `logic_buffer` 拷贝共享节点
⇒ ⇒ `bundle_base` 拷贝构造**不**调 `create_field_proxy_nodes()`）**一致** ✔

⇒ ⇒ ⇒ **E-3 的两条分支都不成立** ⇒ ⇒ ⇒ 「覆盖不传播」的真因**不在**节点身份上
⇒ ⇒ ⇒ 真因 = **覆盖的【落点】不对**（打在 literal 节点上 ⇒ ⇒ `set_src` 无效）
⇒ ⇒ ⇒ 即 §14 的 **E-1** ✔

⇒ ⇒ ⚠️ **被推翻的推断保留在此**（不删）⇒ ⇒ 「若 id 不同 ⇒ ⇒ 查 `bundle_base` 的
派生链拷贝路径」这条分支**已排除** ⇒ ⇒ 下轮不必重走。

### 13.6 ⇒ ⇒ 对 ADR-054 的影响（**已更新**）

⚠️ 形态 E 的**方向**与**实现细节**均**已打通**（§14）⇒ ⇒ ⇒
⇒ ⇒ **ADR-054 的「**【待验证】**」标注【可以撤销】** ⇒ ⇒ （architect 权限 ⇒ ⇒
由 architect 追认，见 tasks.md 的第 5 组）

⇒ ⇒ ⇒ **尚未完成** ⇒ ⇒ **不得**让本 change 归档：
① `stream_fifo` 的**行为用例**（它至今**零行为覆盖** ⇒ ⇒ 本次修复 MUST 有可执行证据）；
② **多驱动守卫**（ADR-054 §3.2 第 1 条）+ 配套用例；
③ `stream_queue` / `StreamBuilder::queue<>` 的同步；
④ `stream_mux_connect_ready` 的 `operator=` ⇒ **`<<=`**；
⑤ 2 个 disabled sample 归位 + **6 处** `ch_in` 直写；
⑥ 护栏（**效果判据** + **原地投毒**自检）+ 变异 + 归档。

## 14. ✅ 形态 E **打通**（2026-10-31）—— 判别性探针（**显式 PASS/FAIL 判定版**）

### 14.1 探针（自检版）：`/tmp/opencode/repro/e2.cpp`

⚠️ **与 §13.3 用的是同一个探针** ⇒ ⇒ **唯一**差别 = **输出显式 PASS/FAIL**
（`CHECK(...)` 宏 ⇒ ⇒ 失败计数 ⇒ ⇒ 退出码）⇒ ⇒ **消除「肉眼看错」的空间**
⇒ ⇒ ⇒ **这正是本轮能一击定案的原因**（§13.3 的错因解剖）。

实测输出：

```
场景 1：消费端 rready=0 ⇒ 反压生效
  写 0x11 ⇒ full=0 rdata=0x11
  [PASS] 队首停在 0x11（未被弹出）
  写 0x22 ⇒ full=0 rdata=0x11
  [PASS] 队首停在 0x11（未被弹出）
  写 0x33 ⇒ full=0 rdata=0x11
  [PASS] 队首停在 0x11（未被弹出）
  写 0x44 ⇒ full=1 rdata=0x11
  [PASS] 队首停在 0x11（未被弹出）
  [PASS] 写满 4 个后 full == 1
  [PASS] 第 5 个写后 full 仍 == 1（自保护）
场景 2：消费端 rready=1 ⇒ 恢复弹出
  [PASS] 弹出 0x11 后队首 == 0x22（得 0x22）
  [PASS] 弹出后 full == 0
  [PASS] 再一拍队首 == 0x33（得 0x33）

==== ✅ 全部通过（FAIL 数 = 0）====
```

⇒ ⇒ **12/12 PASS** ⇒ ⇒ ⇒ **形态 E 的机制【成立】** ✔

### 14.2 ✅ 对照组：**不调** `connect_ready` ⇒ MUST 保持**逐位旧语义**

同一探针去掉 `stream_fifo_connect_ready` 调用 ⇒ ⇒ 实测：

```
写 0x11 ⇒ rdata=0x11
写 0x22 ⇒ rdata=0x22      ← 队首逐拍推进 ⇒ 恒就绪
写 0x33 ⇒ rdata=0x33
写 0x44 ⇒ rdata=0x44
写满 4 个后 full == 0      ← 永远排空 ⇒ full 恒 0（= 旧语义）
```

⇒ ⇒ ⇒ **「不调 = 恒就绪 · 行为逐位不变」** 这条契约**实测成立** ✔
⇒ ⇒ ⇒ 这是形态 E 的**核心承诺**（ADR-054 §3.2 第 1 条第 1 项）⇒ ⇒ **已验证**

### 14.3 🔴 写使能变更是**承重**的（形态 E 契约的**前置**，不是无关变更）

实测：形态 E 的其余部分保持不变，仅把写使能**恢复**为旧写法
`input_stream.valid & input_stream.ready` ⇒ ⇒

```
[ERROR] [node_builder] Invalid operands for binary operation at include/core/../lnode/node_builder.tpp:32
```

⇒ ⇒ `input_stream.ready` 在此**尚未被驱动**（是**空 proxy**）⇒ ⇒ `&` 拿到
**null 操作数** ⇒ ⇒ `sync_fifo` 拿到**无效操作数** ⇒ ⇒ **组合子完全不可用**
⇒ ⇒ ⇒ 写使能 MUST 只用 `input_stream.valid`（内部 `will_write` 已**自保护**）
⇒ ⇒ ⇒ ⚠️ 这是**行为变更**（`wren` 由 `valid & ready` ⇒ `valid`）⇒ ⇒
MUST **单独 commit + 单独测试 + 契约化**（⇒ ⇒ 不与形态 E 混在一个 commit 里
⇒ ⇒ 但**实施顺序**上必须先于它 ⇒ ⇒ 因为它是形态 E 的**前提**）。

### 14.4 ⇒ ⇒ 本 change 的**最终实施面**（`include/chlib/stream.h`）

```cpp
// ① 结果对象持有默认源（持久有效）
ch_bool default_ready_source;
// ② 默认驱动打在字段 proxy 上（非 literal）⇒ 用 `<<=`
result.default_ready_source = ch_bool(true);
result.pop_stream.ready <<= result.default_ready_source;
// ③ 读使能引用字段节点本身 ⇒ 覆盖必然传播
auto fifo_result = sync_fifo<...>(input_stream.valid, input_stream.payload,
                                  result.pop_stream.ready);
// ④ 调用方覆盖
template <typename T, unsigned DEPTH, typename C>
void stream_fifo_connect_ready(StreamFifoResult<T, DEPTH> &result,
                               const C &consumer_ready) {
    ch_bool cr(to_operand(consumer_ready).impl());
    result.pop_stream.ready <<= cr;
}
```

⚠️ **两个约束缺一不可**（§14.3 / §12.4）：
① **目标** MUST 是**非 literal** 的节点 ⇒ MUST 用 **`<<=`**（MUST NOT `operator=`）；
② **源** MUST 是**持久有效**的节点 ⇒ MUST 由**结果对象持有**。

⇒ ⇒ ⇒ ⚠️ **「两个约束」的回归锁 MUST 落成护栏**（⇒ ⇒ 变异 **M1**：
读使能改引用**局部常量节点** ⇒ ⇒ MUST 转红）⇒ ⇒ ⇒
**结构判据**：MUST NOT 用 `operator=` 驱动 `pop_stream.ready`。

## 15. 实施记录（2026-10-31，T3-a 完成）

### 15.1 实施面（`include/chlib/stream.h`）

| # | 改动 | 依据 |
|---|---|---|
| 1 | `StreamFifoResult` 新增 `ch_bool default_ready_source`（**持有**默认源） | §14.4 ① |
| 2 | `result.default_ready_source = ch_bool(true);` + `result.pop_stream.ready <<= result.default_ready_source;` | §14.4 ②（**MUST `<<=`**） |
| 3 | `sync_fifo` 的**读使能** ⇒ `result.pop_stream.ready`（**字段节点本身**） | ADR-054 §3.2 第 1 条 |
| 4 | `sync_fifo` 的**写使能** ⇒ `input_stream.valid`（**不**含 `in.ready`） | §14.3（**承重**） |
| 5 | 新增 `stream_fifo_connect_ready(result, consumer_ready)`：形参**模板化** + `to_operand` 归一化 | ADR-053 / E3 同型教训 |
| 6 | `connect_ready` 的**多驱动守卫** ⇒ `CHFATAL_EXCEPTION` | ADR-054 §3.2 第 1 条（⚠️ `CHERROR` 只打日志） |
| 7 | `stream_queue_connect_ready`（薄转发）⇒ ⇒ 别名路径也能接背压 | API 自洽 |
| 8 | `stream_mux_connect_ready` 的 `operator=` ⇒ **`<<=`** | ADR-045 决策 0-B 第 2 条 |

### 15.2 行为证据（`stream_fifo` 此前**零行为覆盖** ⇒ ⇒ 本次 MUST 有）

| 文件 | 用例 | 断言数 |
|---|---|---|
| `tests/chlib/test_stream.cpp` | 反压阻止推进（`full` 可达 · 队首不动 · 自保护 · 恢复弹出） | 28 |
| `tests/chlib/test_stream.cpp` | 未接背压 ⇒ **逐位旧语义**（恒就绪 · 队首逐拍推进 · `full` 恒 0） | 9 |
| `tests/chlib/test_stream.cpp` | 多驱动 ⇒ `connect_ready` **抛异常** | 1 |
| `tests/test_stream_builder.cpp` | `StreamBuilder::queue<>()` 的反压**传过 builder 链** | 6 |

⇒ ⇒ `test_stream` **94 断言 / 12 用例** ⇒ **132 断言 / 15 用例**（+38）
⇒ ⇒ `test_stream_builder` **4 用例** ⇒ **5 用例 / 10 断言**

### 15.3 变异（每条**先确认编译成功**）

| # | 注入 | 结果 |
|---|---|---|
| **M1** | 读使能改引用**局部常量节点** | ✅ 转红（`occ() == 2` 得 **1** ⇒ 无反压 ⇒ 排空） |
| **M2''** | 字段连接 `<<=` ⇒ `operator=`（重绑定） | ✅ 转红（`default-driver-operator-assign` 投毒） |
| **M3** | 删掉**多驱动守卫** | ✅ 转红（多驱动用例） |
| **M4** | 写使能改回 `valid & ready` | ✅ 转红（**SIGSEGV** —— "Invalid operands" ⇒ 组合子不可用） |
| **M5** | `stream_handshake_example` 的读使能**漏掉忙门控** | ✅ 转红（`popped payload MUST be 0x10 (got 0x12)` ⇒ **丢数据**） |

### 15.4 护栏（`scripts/verify_stream_ready_ownership.sh` + `scripts/lib/stream_ready_ownership.py`）

| 判据 | 类型 | 内容 |
|---|---|---|
| **R0** | fail-closed | 必需文件 / 二进制缺失 ⇒ **exit 1**（MUST NOT 当「通过」） |
| **R1** | **效果** | 跑行为用例 ⇒ MUST 绿（**MUST NOT** 词法「源码含常量 `true`」） |
| **R2** | 静态 | 读使能 MUST 引用 `result.pop_stream.ready`（**字段节点本身**） |
| **R3** | 静态 | 默认驱动 MUST 用 `<<=`（MUST NOT `operator=` 重绑定） |
| **R4** | 静态 | 写使能 MUST 只用 `input_stream.valid` |
| **R5** | 静态 | `connect_ready` MUST 存在且形参**模板化** |
| **R6** | 调用点 | 调 `stream_fifo` / `stream_queue` 后 MUST 接 ready ⇒ ⇒ **或**显式豁免标记（**豁免非静默** ⇒ `::notice` 点名 8 处） |

⚠️ **静态判据 MUST 走 Python + 剥注释**（复用 `scripts/lib/fsm_wiring.py` 的
`strip_comments_and_strings` ⇒ ⇒ 与 FSM 护栏口径**不漂移**）⇒ ⇒ 注释里的
字面量不会误判（实测：shell + grep 的版本把**注释**里的 `valid & ready` 当成
违规 ⇒ ⇒ 假红）。

⚠️⚠️ **护栏自身踩的两个坑**（本仓「护栏自身有盲区」病症的新实例）：

① **shell 双引号里的反引号** —— `ok "…用 <<= …"` 里的反引号被 shell 当
   **命令替换** ⇒ ⇒ 护栏消息被**静默吞掉** ⇒ ⇒ **所有**消息 MUST NOT 含
   反引号（改用「`<<=`」的纯文本描述）。

② 🔴 **还原源码 ≠ 复原**（**二进制也是状态**）—— 原地投毒的还原用
   `shutil.copy2` ⇒ ⇒ **保留 mtime** ⇒ ⇒ 还原后的文件 mtime **比备份还旧**
   ⇒ ⇒ `make` 判定「已是最新」⇒ ⇒ **不重新编译** ⇒ ⇒ **跑的仍是被投毒的
   二进制** ⇒ ⇒ 报「还原写坏了文件」的**假失败** ⇒ ⇒ 而且**下一轮**的
   R1 也会**假红**（二进制还是脏的）⇒ ⇒ 修法：**还原 MUST 用 `copyfile`**
   （刷新 mtime ⇒ 强制重建）**且** 护栏的 R1 MUST **先重建再跑**
   ⇒ ⇒ 彻底消灭「stale binary」这一整类假结果。

⇒ ⇒ **自检**：4 种投毒（读使能局部常量 · 写使能 `valid & ready` · 默认驱动
`operator=` · 删多驱动守卫）⇒ ⇒ 每种 MUST 转红 ⇒ ⇒ 还原后 MUST 再绿一次 ✔

### 15.5 2 个 disabled sample 的归位（**重写**）

⚠️ 旧版同时踩了 **5** 个坑：旧 `stream_fifo` 签名 · `Stream<T>` 类型名 ·
`get_default_clk/rst` 改名 · `ch_reg` 的 `->clk/->rst` 成员已移除 ·
⚠️ **`bundle-as-IO` 违反仓库约定**（根 AGENTS.md「Bundle as IO」⇒ ⇒ bundle
成员**不会**被注册为组件 IO ⇒ ⇒ **不会**成为生成 Verilog 的**顶层端口**）。

⇒ ⇒ 重写后：独立端口 + **自检断言**（samples 护栏**只查退出码** ⇒ ⇒ 行为
退化而 `return 0` 会被判 PASS ⇒ ⇒ 这正是本仓长期「假绿」的机制）
⇒ ⇒ `stream_component_example` 演示「**FIFO 满 ⇒ 上游暂停**」
⇒ ⇒ `stream_handshake_example` 演示「**有效消费端 ready**」这条纪律
（FIFO 的读使能 MUST 用 `out_ready & !busy` ⇒ ⇒ 否则**丢数据且无报错**）
⇒ ⇒ samples 护栏 **39/39** 全绿（37 → 39 / 禁用 3 → 1）

### 15.6 ⚠️ 登记为**已知缺口**（不在本 change 范围 ⇒ ⇒ 独立立项）

- **builder 链的上游侧反压**：`StreamBuilder::queue<>()` 把 FIFO 的
  `push_stream.ready`（= `!full`）**丢弃**了 ⇒ ⇒ 链上游**收不到**「FIFO 满了」
  ⇒ ⇒ ⚠️ 本 change 只打通**消费端 → 读使能**这条方向（形态 E 的标的）
  ⇒ ⇒ 上游方向**独立立项**。
- **demux / arbiter 的 ready 所有权**（T3-b，ADR-054 §6 已列）。
- **`fwft_fifo` 重写 / 改名**（ADR-045:148 单独立项）。
