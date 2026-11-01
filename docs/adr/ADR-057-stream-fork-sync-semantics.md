# ADR-057: `stream_fork` 的**同步语义** —— `input.ready = AND(各输出 ready)`；`synchronous=false` 响亮拒绝

- Status: **Accepted**（2026-11-01 · 用户授权 architect 裁决 · 见
  `.rddf/checks/architect-decisions-pending-2026-11.md` 的 **B3**）
- Date: 2026-11-01
- Author: build-agent（architect role）
- Parent: **ADR-045**（stream 语义）· **ADR-054**（ready 所有权形态 E）·
  **ADR-052**（接受面：要么语义正确、要么响亮拒绝）· **ADR-056**（同批的 demux）
- 关联 capability：`chlib-stream-ready-ownership` + `chlib-stream-arbitration`（命名诚实性）
- 关联台账：`.rddf/improvements/stream-forward-ready-closure-2026-10.md#fork`

---

## 1. 背景（**只陈述决策所需的事实**）

### 1.1 现状（`include/chlib/stream.h` 的 `stream_fork`）

```cpp
if (synchronous) {
    result.input_stream.ready = input_stream.ready;              // ①
    for (i) result.output_streams[i].ready = result.input_stream.ready;  // ②
} else {
    result.input_stream.ready = input_stream.ready;              // ①
    for (i) result.output_streams[i].valid = input_stream.valid & input_stream.ready;  // ③
}
```

| # | 事实 | 后果 |
|---|---|---|
| ① | `input_stream.ready` 用 **`operator=`** 赋值 | ⇒ 只重绑定**本地副本** ⇒ **调用方持有的节点仍是孤儿** ⇒ 上游收不到反压 |
| ② | 同步模式下**输出的 `ready`** 由 `input_stream.ready` 赋值 | ⇒ **方向颠倒**（`ready` 必须来自消费端）；⇒ 而且 **AND 各输出 ready 从未被计算** ⇒ **「同步」语义从未实现** |
| ③ | 异步模式下**数据面的 `valid`** 依赖**反压面的 `input_stream.ready`** | ⇒ 把 `valid` 与 `ready` 混为一谈 ⇒ `valid` 会在「上游不就绪」时消失（数据面被反压面污染） |
| ④ | 两个分支的 `input_stream.ready` 都是 ① 的自引用 | ⇒ 若改用 `<<=` 立即构成**组合环** |

⇒ ⇒ **`synchronous` 这个参数目前不改变任何真实语义** —— 两条分支在
「谁驱动 ready」上都错，且差异落在 `valid` 上（③ 是纯粹的错误）。

### 1.2 既有覆盖的实际判别力

| 位置 | 断言 | 判别力 |
|---|---|---|
| `tests/chlib/test_stream.cpp` 的 `[stream][fork]` 同步 SECTION | 只断 `payload`（×2）+ `valid`（×2） | ⇒ 对 `ready` **零信号** |
| 同文件异步 SECTION | 构造后 `output_streams[i].ready = true`，只断 `payload` | ⇒ 对「`valid` 依赖 `input.ready`」这条错误**零信号** |
| `tests/chlib/test_stream.cpp:781` 段 | 断 payload 复制 + valid 透传 | 同上 |
| `examples/stream/stream_fork_demo.cpp` | 只打印 | ⇒ 非判据 |
| 护栏 `R8` | **豁免** `stream_fork`（理由：同步语义从未工作过 ⇒ 等于新功能 ⇒ 判据从零建） | ⇒ **不在任何 ready 契约的覆盖范围内** |

⇒ ⇒ **两个分支的行为都从未被断言过** ⇒ ⇒ 「fork 可用」这条印象**无判据支撑**。

### 1.3 参照语义（SpinalHDL `StreamFork`）

- `synchronous = true`：所有输出**同时**有效；**输入就绪 = 所有输出就绪的 AND**；
- `synchronous = false`：每个输出**独立**被消费（各自记住「本轮已消费」）⇒
  输入在所有输出都消费过之后才就绪 ⇒ **需要每输出的持久状态**。

---

## 2. Decision

> **D1（`synchronous = true` 的真语义）**：
> `input_stream.ready = AND({output_streams[*].ready})`；
> `output_streams[i].valid = input_stream.valid`（**只依赖 `valid`**）；
> `output_streams[i].payload = input_stream.payload`。

> **D2（`synchronous = false`）**：**保留参数，但当前实现 MUST 在建图期
> 响亮拒绝**（`CHFATAL_EXCEPTION`），并指向「异步 fork 独立立项」。
> MUST NOT 返回一个「看起来像异步 fork」的半成品。
>
> ⚠️ **D2 不是新原则**：它是 ADR-052「接受面里每一格 MUST 要么语义正确、
> 要么**响亮拒绝**」对本函数的**应用** ⇒ ⇒ 因此它属于「本函数的契约」这一条
> 决策，**不构成**第二个独立决策（异步 fork 的**真语义**是另一篇 ADR 的事，
> 本 ADR 只把它登记为**已知残留**）。

> **D3（数据面与反压面分离）**：`valid` / `payload` MUST NOT 依赖任何 `ready`
> 信号（含 `input_stream.ready`）。⇒ 现状 ③ 被判为**缺陷**，不是设计选择。

> **D4（形态）**：每个 `output_streams[i].ready` MUST 是**形态 E 字段**
> （默认恒就绪 + 消费端 `<<=` 覆盖）；fork MUST NOT 用 `operator=` 给它们赋值。

> **D5（接线纪律）**：`input_stream` MUST 先 `as_master()`；
> `input_stream.ready` 由 fork 驱动 ⇒ 调用方 MUST NOT 自行驱动。

⚠️ **D2 的代价必须明说**：`synchronous=false` 的**既有 2 处测试调用点与 1 个
example** 会从「静默跑错」变成「建图期抛异常」⇒ 调用点 MUST 改为 `true`
（或删除该参数用法）。这是**有意的降级** —— 与「半成品继续伪装成可用」相比，
**响亮失败**是本仓一贯的选择（ADR-052）。

---

## 3. 选项对比

### 3.1 `synchronous = false` 的处置（**本 ADR 的核心取舍**）

| # | 选项 | 结论 | 理由 |
|---|---|---|---|
| 1 | **保留参数 + `false` 建图期响亮拒绝** | ✅ **采纳** | ① 与 ADR-052「要么语义正确、要么响亮拒绝」**同型**；② **零生产消费者**（只有 2 处测试 + 1 example）⇒ 拒绝的爆炸半径可枚举；③ 保留参数 = 保留「这里迟早要有异步 fork」的**位置**，且拒绝消息即**归位条件**；④ 不把「独立立项」的工作**夹带**进本 ADR |
| 2 | **实现真异步语义**（每输出独立消费状态） | ❌ 否决（**本 ADR 内**） | ① 它需要**每输出的持久状态 + 轮次计数器** ⇒ 是**新功能**（判据从零、金标需手推）；② 与本 ADR 的「同步语义」是**两个独立决策** ⇒ 塞进一篇会破坏「一文档一决策」；③ 仓内**零需求**（无生产消费者）⇒ 先实现再找用途 = 制造需要维护的语义 |
| 3 | **删掉 `synchronous` 参数**（只保留同步） | ❌ 否决 | ① 会把「异步 fork」这条未来方向**从 API 上抹掉**（改名/加回都是破坏性变更）；② 且它**不能**表达「这里需要但还没实现」⇒ 而选项 1 的拒绝消息**正是**那段信息 |
| 4 | 保持现状（`false` 静默走错误路径） | ❌ 否决 | ③ 是**数据面被反压面污染**的确定性缺陷 ⇒ 保持 = 保留缺陷 |

### 3.2 同步语义的形状

| # | 选项 | 结论 | 理由 |
|---|---|---|---|
| 1 | `input.ready = AND(各输出 ready)` | ✅ **采纳** | SpinalHDL 标准语义；且它是**唯一**能让「任一消费者慢 ⇒ 上游停」成立的形式 |
| 2 | `input.ready = OR(各输出 ready)` | ❌ 否决 | 会让**未被消费**的输出丢数据（上游推进而后进者未取）⇒ 与「fork 复制广播」的语义冲突 |
| 3 | `input.ready` 恒就绪（不接反压） | ❌ 否决 | 违反 ADR-045:160；且「慢消费者」不可表达 |

---

## 4. 后果 / 代价

### 正面
- `stream_fork` 的 `synchronous=true` **首次具备真实语义**（此前名称承诺的行为从未存在）；
- 数据面/反压面**分离**（③ 消除）⇒ `valid` 不再随上游 `ready` 消失；
- `synchronous=false` 从「静默错」变为「响亮拒绝 + 归位条件」；
- 该组件进入 `chlib-stream-ready-ownership` 的覆盖范围，并有判别性判据。

### 负面
- **行为变更**：同步模式下 `input.ready` 的源从「输入侧」变成「各输出 ready 的 AND」；
- `synchronous=false` 的 3 处调用点必须迁移（`tests/chlib/test_stream.cpp` ×2 +
  `examples/stream/stream_fork_demo.cpp` 用 `true` ⇒ 无需改）；
- 异步 fork 变成**显式欠账**（独立立项），不再有「看起来能用」的假象。

### 风险

| # | 风险 | 对策 |
|---|---|---|
| 1 | 既有 fork 测试因 `input_stream.ready = true` 的旧写法变成「自行驱动」而红 | 调用点 MUST 删掉该自赋值；判定以 `ctest` 为准 |
| 2 | 误把「默认恒就绪」当成「同步语义生效」 | 判别性判据 MUST 是：「只有**一个**输出 `ready=0` ⇒ `input.ready=0`」（两面命中才证明 AND） |
| 3 | D2 的拒绝被绕过（有人加个 `if` 分支返回半成品） | 护栏 MUST 对 `synchronous=false` 路径设结构判据（该分支内 MUST 只有 `CHFATAL_EXCEPTION`，MUST NOT 有 `<<=` 驱动） |

---

## 5. Verification（**前瞻**验收判据，MUST 可勾选）

- [ ] **AND 判据（两面）**：两输出 `ready` 均高 ⇒ `input_stream.ready == 1`；
      **任一**为低 ⇒ `== 0`（只测一面 ⇒ 与「OR」「取第一个」不可区分）
- [ ] **数据面独立**：`input_stream.ready` 为低时，各输出 `valid` / `payload`
      **MUST 不变**（= ③ 的回归锁）
- [ ] **默认语义**：消费端不驱动任何输出 `ready` ⇒ `input_stream.ready == 1`（零破坏）
- [ ] **响亮拒绝**：`stream_fork(in, false)` ⇒ 建图期抛 `std::runtime_error`
      （⚠️ MUST 用 `CHFATAL_EXCEPTION`；`CHERROR` / `CHFATAL` **只打日志不抛**）
- [ ] **无组合环**：含 fork 的设计 MUST 通过拓扑排序
- [ ] **双后端 parity**：MUST 走 `DUAL_BACKEND_TEST_PAIR`（该组件此前**零 dual 覆盖**）
- [ ] **护栏**：`stream_fork` MUST 进 R8 扫描面（**移出豁免清单**）
- [ ] **变异**：把 AND 改成 OR / 改回「输入侧驱动」/ 删掉拒绝 ⇒ 判据 MUST 转红
      （**先确认编译成功**）
- [ ] **命名诚实性**：`synchronous` 参数的存在 MUST 在头注释里如实说明
      「`false` 目前拒绝，归位条件 = 异步 fork 独立立项」

---

## 6. 变更记录

| 日期 | 变更 |
|---|---|
| 2026-11-01 | **创建**（D1 AND 同步语义 · D2 `false` 响亮拒绝 · D3 数据面/反压面分离 · D4 形态 E · D5 接线纪律）。依据 = B3 裁决 + 「两条分支的 ready 都错、③ 是数据面污染」的实测 + ARD-052 的接受面原则 |
