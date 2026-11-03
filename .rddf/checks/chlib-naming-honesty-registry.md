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
| `priority_encoder` | 优先级编码器：**编号最大**的置位优先（**高位优先**） | `tests/chlib/test_stream_arbiter_dual.cpp` · `[chlib][stream][arbiter][priority][grant][ready][dual]` | 2026-11-03 `header-self-containment-fix-2026-11` 订正了它的**头注释**（原文写「优先处理低位」⇒ 与实现**相反**；实测 `priority_encoder<4>(0b1010) == 3`）⇒ ⇒ 该方向由 `stream_arbiter_priority` 的 `[priority][grant]` 用例锁定 ⇒ 登记 |
| `priority_selector` | 优先级选择器；**最低**置位优先（低位优先级更高） | `tests/chlib/test_selector_arbiter_dual.cpp` · `[chlib][selector][priority][grant][dual]` | 2026-11-03 首次双后端验证（方向只有多位同时置位时才可观测）⇒ 登记 |
| `round_robin_selector` | 轮转选择器；扫描起点 = **上次授予位置 + 1**（环绕） | `tests/chlib/test_selector_arbiter_dual.cpp` · `[chlib][selector][round_robin][grant][dual]` | 2026-11-03 首次双后端验证（**有生产消费者**：`include/chlib/stream.h` 的 `stream_arbiter_round_robin`）⇒ 登记 |
| `stream_narrow_to_wide` | 累积 `RATIO` 个窄字为**一个**宽字（第 k 个落在 `[k*NB,(k+1)*NB)`），宽字**含最后一个窄字**且**组间隔离**；输出 `valid`/`payload` **寄存器化**（+1 拍） | `tests/chlib/test_stream_width_adapter_dual.cpp` · `[chlib][stream][width_adapter][narrow_to_wide][dual][adr064]` | 2026-11-03 ADR-064 重写：修复前**一仿真就 SIGSEGV**、且宽字**缺最后一个字** + 累积器不清零（跨组污染）|
| `stream_wide_to_narrow` | 把宽字拆成 `RATIO` 个窄字，**低字节优先**；输出满时上游 `ready=0` | `tests/chlib/test_stream_width_adapter_dual.cpp` · `[chlib][stream][width_adapter][wide_to_narrow][dual][adr064]` | 同上（修复前 `initialize()` SIGSEGV + `input.ready` 用 `operator=` ⇒ 上游收不到反压）|
| `stream_width_adapter` | 按位宽**自动择向**的泛型入口；同宽 ⇒ 零状态直通 | `tests/chlib/test_stream_width_adapter_dual.cpp` · `[chlib][stream][width_adapter][passthrough][dual][adr064]` | 同上（修复前同宽分支同样 SIGSEGV）|
| `stream_arbiter_lock` | **租借式**仲裁：一旦授予某通道，**保持选中直到事务完成**（`valid & ready`）；输出 `valid` = **选中通道**的 valid；输出 payload = **选中通道**的 payload | `tests/chlib/test_stream_arbiter_lock_dual.cpp` · `[chlib][stream][arbiter][lock][select][dual][adr065]` | 2026-11-03 ADR-065 修复：修复前选择由**建图期求值**的 C++ `if` 决定 ⇒ **恒选通道 0 且输出别的通道的 payload**（静默数据损坏）；且锁存**从未实现**（`ch_reg` 的 C++ 赋值）；`out.valid` 用「任一通道 valid」 |
| `round_robin_arbiter` | selector 层的**轮转**原语：自持 `ptr_reg`，`next_ptr` = 被授予位 + 1（环绕）⇒ **每拍推进**、无饿死 | `tests/chlib/test_stream_arbiter_lock_dual.cpp` · `[chlib][selector][round_robin_arbiter][rotation][dual][adr065]` | 2026-11-03 ADR-065 修复：修复前 `UpdatePriority` 的外层 `if (result.valid)` **建图期恒假** ⇒ `next_ptr` **恒 0** ⇒ 实为「固定最低位优先」⇒ 低位通道饿死高位通道 |

## 变更记录

| 日期 | 变更 |
|---|---|
| 2026-11-02 | **创建**（B8 落地：5 条登记 ⇒ `stream_demux` / `stream_fork` / `stream_arbiter_round_robin` / `sync_fifo` / `stream_fifo`）⇒ 护栏 R14 守「登记不漂移 + 判别性用例不消失」 |
| 2026-11-03 | **+6 条**（路线 B G/H/I 批触达的组件）⇒ `stream_m2s_pipe` / `stream_s2m_pipe` / `stream_half_pipe`（ADR-055 重写过的三个 pipe）· `priority_selector` / `round_robin_selector`（首次双后端验证）· `stream_arbiter_priority`（首次双后端验证）⇒ ⇒ 触发护栏 **R3**（最近的 chlib change MUST 声明 `touched-components`） |
| 2026-11-03 | **+3**：`stream_narrow_to_wide` · `stream_wide_to_narrow` · `stream_width_adapter`（ADR-064 重写）⇒ 登记 **11 → 14**；⚠️ 同时**删除** `stream_extend` / `stream_truncate`（零消费者 + 零测试 + 静默失效）⇒ **MUST NOT** 登记它们（R1 会检「登记 ⊆ 组件全集」）|
| 2026-11-03 | **+2**：`stream_arbiter_lock` · `round_robin_arbiter`（ADR-065 修复两处「建图期求值」缺陷）⇒ 登记 **14 → 16** |
| 2026-11-03 | **+1**：`priority_encoder`（`header-self-containment-fix-2026-11` 订正其头注释「优先处理低位」⇒ 实为**高位优先**）⇒ 登记 **16 → 17** |
