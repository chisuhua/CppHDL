# Design：`chlib` 链式组合的 ready 归属

> 决策正本 = **ADR-055**；本文件只承载**推导 / 手推金标 / 实现裁决**（ADR 契约要求
> 推导不进正文）。

## 1. B1 —— `StreamBuilder` 的链内闭包不变式

### 1.1 语义推导（为什么「一级一个 pending 节点」就够）

链上第 k 级的**输出 ready** 是第 k+1 级要驱动的节点；第 k 级的**入参 ready** 由

- **FIFO 级**（`stream_fifo`）：**不驱动**入参 ready（写使能只用 `valid`，内部
  `sync_fifo` 自保护）⇒ ⇒ **builder 必须补**：`current_stream_.ready <<=
  result.push_stream.ready`；
- **过滤级**（`haltWhen` / `continueWhen` / `throwWhen` / `takeWhen` / `map`）：
  **自己**驱动（`input_stream.ready <<= result.ready | condition`）⇒ ⇒
  builder **MUST NOT** 再动它 ⇒ ⇒ 否则会**抹掉 `| condition` 那一项**
  ⇒ ⇒ 「halt 期间数据被丢」。

⇒ ⇒ 两者合起来就是**一条**规则：

```
stage 创建后：  ① 若 stage 不自己驱动入参 ready ⇒ 用本级的容量信号驱动它
                ② pending_ready_ = stage.output.ready    ← 留给下一级
```

⇒ ⇒ 因此**只需要记住「下一级要驱动的那一个节点」**，不需要保存链上各级的
result 对象。

⚠️ **与 ADR-055 §2.3 的差异（如实登记）**：ADR 写「用私有成员保存链上各级的
result 对象」。实现取了**等价但更省**的形态（一级一个 `pending_ready_`）：
- 语义等价：第 k 级的 output ready 在建立第 k+1 级时**立刻**被驱动掉，
  之后再也不需要 ⇒ ⇒ 保存历史没有用处；
- 且**不随链长线性增长**（ADR 的「负面：内存线性增长」因此不成立）。

⇒ ⇒ **决策未变**（链内闭包不变式 + 公开 API 零变更），仅**实现形态**更省。

### 1.2 手推金标

`queue<4>().queue<4>()`（总容量 8），消费端恒不就绪：

| 阶段 | 手推 | 断言 |
|---|---|---|
| 链空 | `full₁ == 0` ⇒ `in_ready == 1` | **判别性**（修复前该节点无驱动 ⇒ 读 0 ⇒ 必红） |
| 按 ready 协议推 8 词 | 两级各 4 ⇒ 都满 | `in_ready == 0` |
| 链头 | FIFO₂ 从未弹出 ⇒ 最早写入的词 | `out_payload == 0x10` |
| 消费端恒就绪 | 每拍弹 1，FIFO₁ 同拍补 1 ⇒ FIFO₂ 计数恒 4 | `out_valid` 连续 8 拍为 1 |
| 序列 | 逐项 | `0x10 0x11 … 0x17` |
| 弹空 | 两级皆空 | `in_ready == 1` |

⚠️ **判别性判据 MUST 是「空链可接收」**：修复前 `in_ready` **无驱动** ⇒ 读 0；
而「满」时它**也**是 0 ⇒ ⇒ 只断言「满时为 0」**不可区分**（假绿）。

### 1.3 三种「驱动不了」⇒ 响亮拒绝

| 形态 | 后果（若静默） | 判据 |
|---|---|---|
| `source.ready` **无节点**（缺 `as_master()`） | `<<=` 另建节点、只绑本地副本 ⇒ 调用方收不到反压 | `impl() == nullptr` ⇒ `CHFATAL_EXCEPTION` |
| `source.ready` 是 **literal**（`operator=` 喂常量） | literal 的值不随 src 变 ⇒ 驱动无效 | `type() == type_lit` ⇒ `CHFATAL_EXCEPTION` |
| 链首**已被驱动** | `set_src(0, …)` = last-wins ⇒ **静默接管**调用方的信号 | `pending_is_source_ && num_srcs() > 0` ⇒ `CHFATAL_EXCEPTION` |

⚠️ 第三条**只对链首**生效：非链首的 pending 节点是**上一级组合子的默认驱动**
⇒ 覆盖它是**设计意图**（ADR-054 形态 E）。

### 1.4 为什么 `queue()` 里那句 `stream_fifo<` 仍需 R6 豁免标记

R6 的判据是「调用点窗口里 MUST 出现 `connect_ready`」。builder **不用**
`connect_ready`（它自己实现链内闭包）⇒ ⇒ 豁免标记的**理由已改写为
「本文件自己实现不变式」**（不是「懒得接」）。

## 2. B6 —— `stream_pipeline.h` 的 pipe 族

### 2.1 修复前的两个语义缺陷（不止 ready）

| # | 缺陷 | 后果 |
|---|---|---|
| ① | `input_stream.ready = result.input.ready`（**`operator=`**） | 节点重绑定 ⇒ **调用方被孤儿化** ⇒ 上游收不到反压 |
| ② | `result.output.ready` **从未被驱动**（`m2s` / `half` 是空节点；`s2m` / `half` 是 `ready_reg`） | 消费端**无处**施加反压 |
| ③ | `s2m` / `half` 的 `ready_reg->next = input_stream.ready` | 寄存的是**上游自己的** ready ⇒ ⇒ **方向反了**的自环 |
| ④ | `m2s`：`valid_reg->next = input_fire` | 下游一停顿就**丢数据**（`valid_reg` 被清零） |
| ⑤ | `half`：`valid_reg->next = accept_data` | 同上：未被消费的数据下一拍**消失** |

⇒ ⇒ ①②③ 属 **ready 归属**（本 change 的契约）；④⑤ 属**数据通路**，但**必须
一起修** —— 因为「真 ready」一接上，④⑤ 的丢数据就**第一次可观测**
（修复前它只在常量接线下「看起来对」）。

### 2.2 三个 pipe 的最终形态

| pipe | `output.ready` | `input.ready` | 数据通路 |
|---|---|---|---|
| `m2s` | 可覆盖字段（默认恒就绪） | `!valid_reg \| output.ready`（**组合**） | payload/valid 寄存；`valid_reg` **保持**未被消费的数据 |
| `s2m` | 同上 | `RegNext(output.ready)` | payload/valid **组合**通过（0 拍）⇒ ⚠️ ready 迟 1 拍 ⇒ 下游 MUST 在 1 拍内响应 |
| `half` | 同上 | `toggle & (!valid_reg \| output.ready)` | `toggle` **自由运行** ⇒ 隔拍接收；`valid_reg` **保持**未被消费的数据 |

⚠️ **`half` 的 toggle MUST 自由运行**（MUST NOT 由 `input_fire` 驱动）：若 toggle
只在「传输」时翻转，则 `input.ready = toggle & …` 在 toggle==0 时永远是 0
⇒ ⇒ toggle 永远等不到传输 ⇒ ⇒ **死锁**。

⚠️ **「半带宽」= 输入侧隔拍就绪**（上游自己保持数据）⇒ **MUST NOT** 是
「隔拍丢弃」（那是丢数据，不是限流）。

### 2.3 共享上游驱动实现（`detail::drive_upstream_ready`）

- 一处实现、三处使用 ⇒ ⇒ 语义**不会漂移**；
- 三种「驱动不了」⇒ `CHFATAL_EXCEPTION`（同 §1.3）；
- ⚠️ **多驱动判据必须允许覆盖「默认常量」**（`src(0)->type() == type_lit`）
  ⇒ ⇒ 否则**链式 pipe**（`s2m(...).output` 再进 `m2s`）会被**误判为多驱动**
  ⇒ ⇒ `tests/test_stream_operators.cpp` 的 4 个链式用例会**全部崩**
  （⇒ 实测：该文件 11 个用例 45 断言全绿）。

### 2.4 手推金标

**`m2s`**（1 拍寄存器，`consumer_ready` 由测试驱动）：

| 拍 | 手推 | 断言 |
|---|---|---|
| 结算拍 | 寄存器空 ⇒ `in_ready == 1` | 判别性（修复前恒 0） |
| 收 `0xA0`、下游不就绪 | 寄存器满 ⇒ `in_ready == 0` | 反压到达 |
| 再等一拍 | `valid_reg` **保持** | `out_valid == 1 && out_payload == 0xA0` |
| 下游就绪、停止推入 | 弹走 ⇒ `out_valid == 0`、`in_ready == 1` | — |
| 每拍换 payload（`0x10+k`） | 输出滞后一拍 | `out_payload == 0x10+k` |

**`half`**（隔拍）：

| 拍 | 手推 | 断言 |
|---|---|---|
| 第 1 拍（toggle=0） | 不收 | `out_valid == 0`、`in_ready == 1` |
| 第 2 拍（toggle=1） | 收 `0x20` | `out_valid == 1 && out_payload == 0x20` |
| 第 3 拍 | 隔拍不收 | `in_ready == 0` |
| 下游不就绪再等 2 拍 | **保持** | `out_valid == 1 && out_payload == 0x20` |
| 下游就绪 | 取走 | `out_valid == 0` |

## 3. 护栏

| 判据 | 位置 | 判什么 |
|---|---|---|
| **R10** | `scripts/lib/stream_ready_ownership.py` | `queue()` MUST 调 `drive_pending_ready(` · `drive_pending_ready()` MUST 用 `<<=` + `type_lit` + `num_srcs()` + ≥3 处 `CHFATAL_EXCEPTION` |
| **R11** | 同上 | `drive_upstream_ready()` 同上三条 + **3 个 pipe 都必须调它** |
| **R8**（扩面） | 同上 | 扫描面加 `stream_pipeline.h` ⇒ 3 个 pipe 进 R8（已收口 13 → **16**） |
| 投毒 | `verify_stream_ready_ownership.sh --self-test` | `builder-drop-chain-closure` · `builder-upstream-operator-assign` · `pipe-upstream-operator-assign` ⇒ 行为 + 静态**双红** |

⚠️ **R8 与 R11 不可互相替代**（实测推理）：R8 只看「组合子函数体里有没有驱动上游」
⇒ ⇒ pipe 把驱动**收在 helper 里** ⇒ ⇒ 若 helper 的 `<<=` 被改成 `operator=`
⇒ ⇒ **三个 pipe 同时静默失效**而 **R8 仍绿**（它只看到「调用了 helper」）
⇒ ⇒ ⇒ **必须对 helper 本身有判据**（= R11）。

⚠️ **投毒的判别力二进制 MUST 与被投毒文件匹配**：`builder-*` 落在
`stream_builder.h`（只有 `test_stream_builder` include 它）· `pipe-*` 落在
`stream_pipeline.h`（`test_stream_pipeline`）⇒ ⇒ 拿 `test_stream` 判
会**恒绿** ⇒ ⇒ 「判别力为零」被读成「投毒未被抓住」。
