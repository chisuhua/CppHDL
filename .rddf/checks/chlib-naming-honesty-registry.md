---
scope: chlib 的**对外暴露**组合子（stream / fifo / selector 族）
rule: capability `chlib-stream-arbitration` 的「仲裁器的命名与文档 MUST 与实现行为一致」
---

# chlib 命名诚实性**登记表**（B8）

> ⚠️ **为什么有这张表**：本仓反复出现「**名字/注释承诺了不存在的行为**」这一族缺陷
>   （`fwft_fifo` 承诺 FWFT 实则不是 · `stream_arbiter_sequence` 注释承诺
>   round-robin 实则 `priority_encoder` · `stream_arbiter_round_robin` 承诺轮转
>   实则恒传 0）⇒ ⇒ ⇒ **每次都靠「临时普查」发现 ⇒ ⇒ 下次还会漏**。
> ⇒ ⇒ 本表把「**哪些组件的名字曾经说谎 / 被改动过**」与「**它现在由哪条
>   行为用例锁定**」**绑定在一起** ⇒ ⇒ ⇒ 退化（用例被删）可被**自动检出**。

> ⚠️ **范围是「被改动过 / 名字说过谎」的组件**，**不是**全仓普查 ⇒ ⇒
>   「新增组件忘了登记」这一格**不由本表自动抓**（抓它需要全仓组件全集 ⇒ ⇒
>   那正是 B8 裁决时被**明确否决**的「全仓普查」义务）⇒ ⇒ 那一格由
>   capability 的 **per-change 义务** + 评审抓（见护栏 R3：最近的 chlib change
>   MUST 在 proposal 里列出它触及的组件 ⇒ ⇒ 那张列表 ⊆ 本表）。

## 登记表

| 组件 | 名字/注释**声称**的语义 | 判别性用例（**tag 或 macro**） | 登记原因 |
|---|---|---|---|
| `stream_demux` | 把一个上游流按 `select` **分流**到 N 路中的**一路**；上游 ready 由**被选中**输出的消费端驱动 | `tests/test_stream_demux_fork_dual.cpp` · `DUAL_BACKEND_TEST_PAIR` | 2026-11-02 方向修正（ADR-056）：修复前输出的 ready 由**输入**驱动（颠倒） |
| `stream_fork` | 把一个上游流**广播**到 N 路；同步模式下**全部**输出就绪才推进 | `tests/test_stream_demux_fork_dual.cpp` · `DUAL_BACKEND_TEST_PAIR` | 2026-11-02 同步语义落地（ADR-057）：修复前 `synchronous` 参数**不改变任何真实语义** |
| `stream_arbiter_round_robin` | **轮转**仲裁（持续有效的通道 MUST NOT 饿死） | `tests/chlib/test_stream_arbiter_dual.cpp` · `[chlib][stream][arbiter][round_robin][grant][ready][dual]` | 2026-10-31 真轮转修复（修复前恒传 `last_grant = 0_d` ⇒ 低位通道饿死）⇒ 2026-11-03 首次双后端验证（判别性用例从 `tests/chlib/test_stream.cpp` · `[stream][arbiter][round_robin][rotation]` 换成双后端版）|
| `stream_arbiter_priority` | 固定优先级仲裁（**高位优先**：`priority_encoder` 的语义） | `tests/chlib/test_stream_arbiter_dual.cpp` · `[chlib][stream][arbiter][priority][grant][ready][dual]` | 2026-11-03 首次双后端验证 ⇒ 登记（⚠️ 方向 = **高位优先**，与 `priority_selector` 的低位优先**相反** ⇒ 名字里的「优先级」在本仓含义不唯一）|
| `sync_fifo` | 同步 FIFO；`q_async` = **FWFT / show-ahead**（0 拍队首视图） | `tests/test_stream_fifo_dual.cpp` · `DUAL_BACKEND_TEST_PAIR` | 2026-10-22 FWFT 语义确立（0 拍 `aread`） |
| `stream_fifo` | stream 包装的 FIFO；`pop_stream.payload` = **FWFT（0 拍）** | `tests/test_stream_fifo_dual.cpp` · `DUAL_BACKEND_TEST_PAIR` | 2026-10-22 同上 |
| `stream_m2s_pipe` | master→slave 管道；`valid`/`payload` 各延迟 **1 拍**；下游停顿时 MUST **保持**数据 | `tests/chlib/test_stream_pipeline_dual.cpp` · `[chlib][stream][pipeline][m2s][ready][dual]` | 2026-11-01 ADR-055 重写（修掉「下游一停顿就丢数据」）⇒ 2026-11-03 首次双后端验证 ⇒ 登记 |
| `stream_s2m_pipe` | slave→master 管道；`payload`/`valid` **0 拍**直通，`ready` 延迟 **1 拍** | `tests/chlib/test_stream_pipeline_dual.cpp` · `[chlib][stream][pipeline][s2m][ready][dual]` | 2026-11-01 ADR-055 重写（修掉「寄存上游自己的 ready ⇒ 方向反了的自环」）⇒ 2026-11-03 首次双后端验证 ⇒ 登记 |
| `stream_half_pipe` | 半带宽管道；**全信号寄存**且**隔拍**才接收 | `tests/chlib/test_stream_pipeline_dual.cpp` · `[chlib][stream][pipeline][half][ready][dual]` | 2026-11-01 ADR-055 重写（修掉「toggle 由传输驱动 ⇒ 无数据时死锁」）⇒ 2026-11-03 首次双后端验证 ⇒ 登记 |
| `priority_selector` | 优先级选择器；**最低**置位优先（低位优先级更高） | `tests/chlib/test_selector_arbiter_dual.cpp` · `[chlib][selector][priority][grant][dual]` | 2026-11-03 首次双后端验证（方向只有多位同时置位时才可观测）⇒ 登记 |
| `round_robin_selector` | 轮转选择器；扫描起点 = **上次授予位置 + 1**（环绕） | `tests/chlib/test_selector_arbiter_dual.cpp` · `[chlib][selector][round_robin][grant][dual]` | 2026-11-03 首次双后端验证（**有生产消费者**：`include/chlib/stream.h` 的 `stream_arbiter_round_robin`）⇒ 登记 |

## 变更记录

| 日期 | 变更 |
|---|---|
| 2026-11-02 | **创建**（B8 落地：5 条登记 ⇒ `stream_demux` / `stream_fork` / `stream_arbiter_round_robin` / `sync_fifo` / `stream_fifo`）⇒ 护栏 R14 守「登记不漂移 + 判别性用例不消失」 |
| 2026-11-03 | **+6 条**（路线 B G/H/I 批触达的组件）⇒ `stream_m2s_pipe` / `stream_s2m_pipe` / `stream_half_pipe`（ADR-055 重写过的三个 pipe）· `priority_selector` / `round_robin_selector`（首次双后端验证）· `stream_arbiter_priority`（首次双后端验证）⇒ ⇒ 触发护栏 **R3**（最近的 chlib change MUST 声明 `touched-components`） |
