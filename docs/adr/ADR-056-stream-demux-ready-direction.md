# ADR-056: `stream_demux` 的 **ready 方向** —— 消费端 ⇒ 上游（与 `stream_mux` 对称）

- Status: **Accepted**（2026-11-01 · 用户授权 architect 裁决 · 见
  `.rddf/checks/architect-decisions-pending-2026-11.md` 的 **B2**）
- Date: 2026-11-01
- Author: build-agent（architect role）
- Parent: **ADR-045**（stream 语义）· **ADR-054**（ready 所有权形态 E）·
  **ADR-055**（链内闭包不变式：只界定链内中间级，**不适用于**对外暴露的组合子）
- 关联 capability：`chlib-stream-ready-ownership`（主文本落此处）
- 关联台账：`.rddf/improvements/stream-forward-ready-closure-2026-10.md#demux`

---

## 1. 背景（**只陈述决策所需的事实**）

### 1.1 现状（`include/chlib/stream.h` 的 `stream_demux`）

```cpp
result.output_streams[i].ready = is_selected & input_stream.ready;   // ①
...
result.input_stream.ready = selected_ready;                          // ②
```

| # | 事实 | 后果 |
|---|---|---|
| ① | **输出的 `ready`** 由**输入的 `ready`** 驱动 | ⇒ **方向颠倒**：`ready` 是**从端**（slave）向主端（master）表达「我能接收」，其源 MUST 是**消费端**，不是生产端 |
| ② | `input_stream.ready` 用 **`operator=`** 赋值 | ⇒ `ch_stream` 的拷贝只重绑定**本地副本**的句柄 ⇒ **调用方持有的 `input_stream.ready` 仍是孤儿节点** ⇒ 上游永远读不到反压 |
| ③ | ② 的值 = `selected_ready` = 由 ① 推出的 `input_stream.ready` | ⇒ **自引用**（若改用 `<<=` 则构成**组合环**） |
| ④ | `select` **越界**时没有任何处理 | ⇒ 全部 `valid=0`，但 `input.ready` 仍按 ① 走 ⇒ 语义未定义（**静默**） |

### 1.2 既有覆盖的实际判别力

| 位置 | 断言 | 判别力 |
|---|---|---|
| `tests/chlib/test_stream.cpp` 的 `[stream][demux]` | 只断言选中/未选中输出的 `valid` + `payload` | ⇒ 对「谁驱动 `ready`」**零信号** |
| `tests/chlib/test_stream_mux_demux.cpp`（2 例） | 结构 + `valid`/`payload` | 同上 |
| 护栏 `R8` | **豁免** `stream_demux`（理由：方向修正是行为变更 ⇒ 需独立 ADR） | ⇒ **不在任何 ready 契约的覆盖范围内** |

⇒ ⇒ **该组件的 ready 契约既无判据也无护栏** ⇒ ⇒ 与 `stream_pipeline.h` 修复前同型。

### 1.3 对照物（`stream_mux` 的**正确形态**）

`stream_mux` 已经是「消费端 ⇒ 上游」：`output_stream.ready` 是**形态 E 字段**
（默认恒就绪、消费端 `<<=` 覆盖），`input_streams[i].ready <<= is_selected &
output_stream.ready`（**`<<=`**，且**消费端**是源）。

⇒ ⇒ **demux 修复后的形态 MUST 与 mux 双向对称**（mux：多上游 → 一消费端；
demux：一上游 → 多消费端）。

---

## 2. Decision

> **D1（方向）**：`stream_demux` 的 ready 方向 MUST 是**消费端 ⇒ 上游** ——
> `input_stream.ready` 由**被选中的那个输出的 ready** 驱动：
> `input_stream.ready = mux({output_streams[*].ready}, select)`。

> **D2（形态）**：每个 `output_streams[i].ready` MUST 是**形态 E 字段**
> （ADR-054）：**默认恒就绪**、由**消费端**用 `<<=` 覆盖。
> demux MUST NOT 用 `operator=` 给它们赋值（② 的同型缺陷）。

> **D3（数据面）**：`valid_i = input_stream.valid & (select == i)`；
> `payload_i = input_stream.payload`；`select` **当拍采样**（组合、0-cycle）——
> 与 `stream_mux` 的对称面一致。

> **D4（越界）**：`select` 越界 ⇒ **全部 `valid_i = 0` 且 `input_stream.ready = 0`**。
> MUST NOT 用「`ready = 1` 并丢弃」兜底（那是**静默丢数据**）。
> 契约 MUST 写明：调用方 MUST 保证 `select` 在 `[0, N_OUTPUTS)` 内。

> **D5（接线纪律）**：`input_stream` MUST 先 `as_master()`；
> **`input_stream.ready` 由 demux 驱动** ⇒ 调用方 MUST NOT 自行驱动它
> （否则触发多驱动守卫）。

⚠️ **D4 的代价必须明说**：越界时上游会被**永久挡住**（`ready=0`）。这是
**有意**的 —— 它把「调用方给了非法 `select`」从**静默丢数据**变成
**可观测的停顿**（真值 = 设计错误，不是数据丢失）。

---

## 3. 选项对比

### 3.1 ready 的方向

| # | 选项 | 结论 | 理由 |
|---|---|---|---|
| 1 | **消费端 ⇒ 上游**（`input.ready = mux(各输出 ready, select)`） | ✅ **采纳** | ① `ready` 的语义方向本就是**从端 → 主端**；② 与 `stream_mux` **双向对称**（消除框架内不一致）；③ 使「下游未就绪 ⇒ 上游停」**可演示**，即 ADR-045:160 的要求 |
| 2 | **保持现状**（输出 `ready` 由输入 `ready` 驱动） | ❌ 否决 | 方向颠倒 ⇒ 输出向**生产端**索取许可，语义不成立；且 ①② 叠加后调用方**永远收不到反压**（孤儿节点） |
| 3 | **输出 `ready` 恒 `true`**（不接受反压） | ❌ 否决 | 违反 ADR-045:160 明文（组合子 MUST NOT 无条件忽略消费端 ready）；且会让「下游慢 ⇒ 上游被拖」不可表达 |
| 4 | 「**两阶段** ready」（先构造、后连接） | ❌ 否决 | 形态 B 已被 ADR-054 **实施期证伪**（读未驱动字段返回 0 而不报错 ⇒ 存量调用点静默退化） |

### 3.2 `select` 越界

| # | 选项 | 结论 | 理由 |
|---|---|---|---|
| 1 | **`input.ready = 0` + 全 `valid = 0`** | ✅ **采纳** | **可观测**（上游停顿）⇒ 把设计错误暴露出来；且不丢数据 |
| 2 | `ready = 1` + 丢弃 | ❌ 否决 | **静默丢数据** —— 本仓最忌讳的失效形态 |
| 3 | 构建期拒绝 | ❌ 不可行 | `select` 是**运行期信号**，构建期不可判定 |

---

## 4. 后果 / 代价

### 正面
- demux 的 ready 链**首次成立**：下游未就绪 ⇒ `input.ready = 0` ⇒ 上游停；
- 与 `stream_mux` **双向对称** ⇒ 「mux/demux 对偶」这条心智模型成立；
- 该组件从「不在任何契约覆盖范围」进入 `chlib-stream-ready-ownership`。

### 负面
- **行为变更**：既有调用点若依赖「`input.ready` 恒为输入侧驱动」的旧形态，
  行为会变（⇒ 实测：**生产代码零消费者**，只有 3 处测试 + 1 个 example）；
- 越界时上游会被挡住（见 D4 的说明）。

### 风险

| # | 风险 | 对策 |
|---|---|---|
| 1 | 修复后既有的 3 处测试**变红**（它们只断 `valid`/`payload`，理论上不受影响，但 `input_stream.ready = true` 的旧写法会变成**自行驱动**） | 调用点 MUST 改：删掉对 `input_stream.ready` 的自赋值；判定以 `ctest` 为准 |
| 2 | 引入组合环（`input.ready` 依赖输出 `ready`，输出 `valid` 依赖输入 `valid`） | 拓扑排序 MUST 通过；**变异验证**：把 `input.ready` 改回 `<<=` 引用自身 ⇒ 必须能被检出 |
| 3 | 「默认恒就绪」掩盖未接线 | 判别性判据 MUST 是「**接了反压之后** `input.ready` 才落 0」，而不是「未接时非零」 |

---

## 5. Verification（**前瞻**验收判据，MUST 可勾选）

- [ ] **方向判据**：消费端把**被选中**输出的 `ready` 拉低 ⇒ `input_stream.ready == 0`
- [ ] **非选中隔离**：把**未选中**输出的 `ready` 拉低 ⇒ `input_stream.ready` **不变**
- [ ] **默认语义**：消费端不驱动任何输出 `ready` ⇒ `input_stream.ready == 1`（= 旧语义，零破坏）
- [ ] **越界**：`select >= N_OUTPUTS` ⇒ 全 `valid == 0` **且** `input_stream.ready == 0`
- [ ] **数据面**：选中输出拿到 `payload` 与 `valid`；未选中输出 `valid == 0`
- [ ] **无组合环**：含 demux 的设计 MUST 通过拓扑排序（`initialize()` 不抛 `CHFATAL`）
- [ ] **双后端 parity**：MUST 走 `DUAL_BACKEND_TEST_PAIR`（该组件此前**零 dual 覆盖**）
- [ ] **护栏**：`stream_demux` MUST 进 R8 扫描面（**移出豁免清单**）
- [ ] **变异**：把 `input.ready` 改回 `operator=` / 改回「输入侧驱动」⇒ 判据 MUST 转红（**先确认编译成功**）

---

## 6. 变更记录

| 日期 | 变更 |
|---|---|
| 2026-11-01 | **创建**（D1 消费端 ⇒ 上游 · D2 形态 E · D3 当拍采样 · D4 越界 `ready=0` · D5 接线纪律）。依据 = B2 裁决 + `stream_mux` 的既有正确形态 + 「三处测试对 ready 零判别力」的实测 |
