# Design：`stream_demux` / `stream_fork` 的 ready 语义

> 决策正本 = **ADR-056** / **ADR-057**；本文件只承载**手推金标 / 实现裁决**
> （ADR 契约要求推导不进正文）。

## 1. 实现形态

### 1.1 共享上游驱动：`detail::drive_master_ready`（`stream.h`）

三处响亮拒绝（`CHFATAL_EXCEPTION`）：**无节点**（缺 `as_master()`）·
**literal 目标**（调用方 `operator=` 喂了常量）· **已被真信号驱动**（last-wins）。

⚠️ **为什么不在 `stream.h` 里复用 `stream_pipeline.h` 的
`detail::drive_upstream_ready`**：后者的**位置**被护栏 **R11** 钉住
（判据要求「3 个 pipe 都调它」且函数体在 `stream_pipeline.h`）；而两个头文件的
include 关系是**反向**的（`stream_pipeline.h` 不 include `stream.h`，反之亦然）
⇒ 复用会打破 R11 的定位。⇒ ⇒ 两处实现**判据完全相同**、**语义 MUST 不漂移**
（改动一处 MUST 同步另一处；R11 / R13 各自钉住自己的那份）。

### 1.2 `stream_demux` 的形态

```
in_range      = OR_i (select == i)                       // select 是否在范围内
selected_ready= OR_i ((select == i) & output_i.ready)     // 被选中输出的 ready
input.ready   = in_range & selected_ready                 // ← D1 + D4
output_i.valid= input.valid & (select == i)               // ← D3（只依赖 valid）
output_i.ready<<= default(true)                           // ← D2（形态 E）
```

⚠️ **顺序承重**：MUST **先**给每个输出建默认驱动（使字段有节点），**再**读它们的
`ready` ⇒ 否则读到**空 proxy** ⇒ `&` / `|` 拿到 null 操作数 ⇒ node_builder 报
`Invalid operands`（这是本仓反复出现的坑）。

⚠️ **`result.input_stream = input_stream;` 是正确的别名写法**（**实测**，不是推理）：
`bundle_base::operator=` 的注释只说「只把目标**指向**源的节点」，容易让人以为
**字段成员不会被拷贝** ⇒ ⇒ 但探针实测（`ch_stream` 的成员 `payload` / `ready`
的 `impl()` 指针**逐一相同**）⇒ ⇒ **成员节点确实共享** ⇒ ⇒ 故整体拷贝即可得到
「只读别名」。⚠️ 反过来，**逐字段**写 `result.input_stream.ready = input_stream.ready;`
会被护栏 R8 误判为「上游驱动用了 `operator=`」（它只看 `xxx.ready =` 这个形态，
不看前缀）⇒ ⇒ **必须**用整体拷贝。

### 1.3 `stream_fork` 的形态

```
synchronous == false  ⇒ CHFATAL_EXCEPTION（建图期）
output_i.valid = input.valid ;  output_i.payload = input.payload   // ← D3
output_i.ready <<= default(true)                                   // ← D4
input.ready   = AND_i output_i.ready                               // ← D1
```

## 2. 手推金标

### 2.1 demux（3 路，`select` 宽 2 ⇒ **可表达越界 3**）

| 步骤 | 输入 | 手推 | 断言 |
|---|---|---|---|
| 初始 | `select=0`，`r0=r1=r2=1`，`in_valid=1`，`payload=0xAB` | 选中 0 ⇒ 只有 `o0` 有效；被选中输出就绪 ⇒ 上游就绪 | `o0_valid=1`、`o1_valid=o2_valid=0`、`o0_payload=0xAB`、`in_ready=1` |
| 拉低**被选中** | `r0=0` | 上游被挡住 | `in_ready=0` |
| 拉低**未选中** | `r0=1, r1=0` | 上游**不受影响** | `in_ready=1` |
| 切到 1 | `select=1`（r1 已低） | 由 r1 决定 | `o1_valid=1`、`in_ready=0` |
| **越界** | `select=3` | 无选中 ⇒ 全 valid 0 + 上游挡住 | `o*_valid=0`、`in_ready=0` |

⚠️ **判别性**：修复前 `input_stream.ready` 是**孤儿节点** ⇒ 读 **0**；而「被挡住」
时**也**是 0 ⇒ ⇒ 只看「为 0」**不可区分**（假绿）⇒ ⇒ MUST 断言**默认态 = 1**。

### 2.2 fork（2 路，同步）

| 步骤 | 输入 | 手推 | 断言 |
|---|---|---|---|
| 初始 | `r0=r1=1`，`in_valid=1`，`payload=0xA5` | AND(1,1)=1 | `in_ready=1`、`o0_valid=o1_valid=1` |
| 拉低第一面 | `r0=0` | AND=0 | `in_ready=0` |
| 拉低**第二面**（第一面恢复） | `r0=1, r1=0` | AND=0 | `in_ready=0`（排除「只看第一个」的实现） |
| 数据面 | 同上 | `valid`/`payload` 只依赖输入 | `o0_valid=1`、`o1_valid=1`、`o0_payload=0xA5` |
| 两面恢复 | `r1=1` | AND=1 | `in_ready=1` |

⚠️ **为什么「两面」是必需的**：只测「全高 ⇒ 1」无法区分 `AND` 与「恒 1」；
只测「一面为低」无法区分 `AND` 与「只看第一个」。

## 3. 调用点迁移（**都是「删掉自驱动 + 加 `as_master()`」**）

| 位置 | 原写法 | 迁移 |
|---|---|---|
| `tests/chlib/test_stream.cpp` demux | `input_stream.ready = true;` | 删除 + `as_master()` |
| 同 文件 fork（payload 段） | `in.ready = ch_bool(true)` + `stream_fork(in, false)` | 删除 + `as_master()` + 改 `true` |
| 同 文件 fork（异步 SECTION） | `stream_fork(in, false)` + 断 payload | 改 `REQUIRE_THROWS_AS` |
| `tests/chlib/test_stream_mux_demux.cpp` ×2 | `output_streams[i].ready = ch_bool(true)` | 删除（形态 E 默认已是恒就绪） |
| `examples/stream/stream_{demux,fork}_demo.cpp` | 无 `as_master()` / `ready = ch_bool(true)` | 补 `as_master()` + 删赋值 |

⚠️ **`REQUIRE_THROWS_AS` 的宏陷阱**：模板实参里的逗号会被宏当**参数分隔**
⇒ MUST 再包一层括号：`REQUIRE_THROWS_AS((stream_fork<ch_uint<8>, 2>(in, false)), std::runtime_error);`

## 4. 护栏

| 判据 | 位置 | 判什么 |
|---|---|---|
| **R8**（清空豁免） | `scripts/lib/stream_ready_ownership.py` | `stream_demux` / `stream_fork` 进扫描面（收口 15 → **17**）；认 `drive_master_ready(` 作为「驱动了上游」的证据 |
| **R13**（新） | 同上 | `drive_master_ready()` 的 `<<=` / `type_lit` / `num_srcs()` / ≥3 处 `CHFATAL_EXCEPTION` + **demux 与 fork 都必须调用它** |
| 投毒 | `verify_stream_ready_ownership.sh --self-test` | `demux-fork-master-ready-operator-assign`（helper 的 `<<=` ⇒ `=`）· `demux-fork-drop-drive`（删掉 demux 的调用）⇒ 各跑 `test_stream_demux_fork_dual` 判行为 |

⚠️ **R8 与 R13 不可互相替代**（与 R11 同型）：R8 只看「函数体里有没有驱动上游」
⇒ 若 helper 的 `<<=` 被改成 `operator=` ⇒ **demux 与 fork 同时静默失效**而 R8 仍绿
（它只看到「调用了 helper」）⇒ ⇒ 必须对 helper 本身有判据。

⚠️ **判别力二进制 MUST 匹配**：`demux-fork-*` 投毒落在 `stream.h` ⇒ 判别力用例是
`test_stream_demux_fork_dual`（只有它断言 `in_ready` 的默认态为 1）⇒ ⇒ 拿
`test_stream` 判会**恒绿**（它的 demux/fork 用例不断言 `ready`）。

## 5. 已知缺口（如实登记，不修）

现状有 **15 个**组合子（mux / arbiter×3 / join / filters / pipes…）**直接**
`<<= upstream.ready` 而**没有**多驱动守卫 ⇒ 「调用方自行驱动 upstream.ready」
在那些组合子上仍是 **last-wins 静默接管**。⇒ ⇒ 本 change 只给 demux / fork
补了守卫（因为它们的新契约明写 D5「调用方 MUST NOT 自行驱动」）⇒ ⇒ **其余组合子的
同类守卫 + 「组合子必须用共享 helper」是独立议题**（登记 backlog）。
