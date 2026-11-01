---
evidence: test:tests/chlib/test_stream.cpp#[ready]
---

# stream 域收口清单（**契约 × 实现 × 判据 × 护栏** 对齐矩阵）

> ⚠️ **本清单是「收口坐标系」** —— 用途：让「哪些已兑现、哪些还欠着」**收敛到
> 一处**，不再散落在 capability Purpose 的「范围限定」段 + 多个 improvement
> 台账 + ADR 的「不在本 ADR 范围」段里（⇒ ⇒ **4 处各说一部分 ⇒ ⇒ 无法
> 宣告收官**）。
>
> ⚠️ **状态列不是手写的事实声明** —— 它由**指针集合推导**：
> - ✅ ⇒ **MUST 同时**有 `spec:`（契约）+ `test:`（可执行判据）+ `guard:`（自动化护栏）
> - ❌ ⇒ **MUST** 有 `debt:`（欠账落到真实的台账 / ADR）⇒ ⇒ 且 **MUST NOT**
>   同时具备 `test:` 与 `guard:`（否则 = **状态过时**）
>
> ⇒ ⇒ 由 `scripts/verify_stream_domain_closure.sh` **机械核验** ⇒ ⇒
> **本清单不可能自相矛盾**（⇒ ⇒ 「说谎」在结构上不可达）。

<!-- COUNT-ANCHOR
TOTAL_ROWS: 16
UNPAID_ROWS: 0
-->

## 指针语法

| 前缀 | 解析规则 |
|---|---|
| `spec:<capability>#<Requirement 标题片段>` | `openspec/specs/<capability>/spec.md` 的 `### Requirement:` 行含该片段 |
| `code:<path>#<符号>` | 该文件（**剥注释后**）含该符号 |
| `test:<path>#<tag 或宏>` | 该文件（**剥注释后**）含该 tag / 宏 |
| `guard:<script>#<判据 id>` | 该脚本含该判据 id（**不剥注释** —— 判据 id 常在注释里） |
| `debt:<path>#<片段>` | 该文件（**剥注释后**）含该片段 ⇒ ⇒ **欠账 MUST 落到真实文件** |

## 对齐矩阵

| # | 能力面 | 契约 | 实现 | 判据 | 护栏 | 欠账 | 状态 |
|---|---|---|---|---|---|---|---|
| 01 | fifo 类 ready 所有权（默认值 + 可覆盖） | `spec:chlib-stream-ready-ownership#fifo 类组合子的 ready 所有权 MUST 是` | `code:include/chlib/stream.h#stream_fifo_connect_ready` | `test:tests/chlib/test_stream.cpp#[stream][ready][backpressure][chlib]` | `guard:scripts/verify_stream_ready_ownership.sh#R1` | — | ✅ |
| 02 | fifo 读使能 MUST 引用**字段节点本身**（覆盖必然传播） | `spec:chlib-stream-ready-ownership#覆盖生效 MUST 同时满足四个必要条件` | `code:include/chlib/stream.h#result.pop_stream.ready` | `test:tests/chlib/test_stream.cpp#[stream][ready][backpressure][chlib]` | `guard:scripts/lib/stream_ready_ownership.py#def r2()` | — | ✅ |
| 03 | fifo 写使能 MUST 只用 `valid`（不依赖 `in.ready`） | `spec:chlib-stream-ready-ownership#fifo 类组合子的**写使能** MUST NOT 依赖调用方的` | `code:include/chlib/fifo.h#will_write` | `test:tests/chlib/test_stream.cpp#[stream][ready][backpressure][chlib]` | `guard:scripts/lib/stream_ready_ownership.py#def r4()` | — | ✅ |
| 04 | ready 链 MUST 用 `<<=`（`operator=` 会孤儿化调用方） | `spec:chlib-stream-ready-ownership#ready 链 MUST 用` | `code:include/chlib/stream.h#stream_mux` | `test:tests/chlib/test_stream.cpp#[stream][mux][ready][chlib]` | `guard:scripts/lib/stream_ready_ownership.py#def r8()` | — | ✅ |
| 05 | 调用点 MUST 接 ready（或**显式豁免**，豁免非静默） | `spec:chlib-stream-ready-ownership#调用方 MUST 先建立字段节点` | `code:include/core/bundle/bundle_base.h#as_master` | `test:tests/chlib/test_stream.cpp#[stream][mux][ready][chlib]` | `guard:scripts/lib/stream_ready_ownership.py#def r6()` | — | ✅ |
| 06 | 转发类 ready 所有权（形态 E · 单阶段） | `spec:chlib-stream-ready-ownership#转发类组合子的 ready 所有权 MUST 是` | `code:include/chlib/stream.h#stream_combine_with` | `test:tests/chlib/test_stream.cpp#[stream][mux][ready][chlib]` | `guard:scripts/lib/stream_ready_ownership.py#def r8()` | — | ✅ |
| 07 | 轮转仲裁 MUST 真轮转（MUST NOT 饿死持续有效的通道） | `spec:chlib-stream-arbitration#轮转仲裁器 MUST 真正轮转` | `code:include/chlib/stream.h#stream_arbiter_round_robin` | `test:tests/chlib/test_stream.cpp#[stream][arbiter][round_robin][rotation]` | `guard:scripts/lib/stream_ready_ownership.py#rr-feedback-const` | — | ✅ |
| 07b | `connect_ready` 多驱动守卫（响亮失败 · **串联不误炸**） | `spec:chlib-stream-ready-ownership#多驱动 MUST 被检出` | `code:include/chlib/stream.h#stream_fifo_connect_ready` | `test:tests/chlib/test_stream.cpp#[stream][ready][series]` | `guard:scripts/lib/stream_ready_ownership.py#def r9()` | — | ✅ |
| 08 | FIFO 输出时序 MUST 被定义（FWFT 0-cycle `aread`） | `spec:chlib-stream-semantics#FIFO 类组件的输出时序 MUST 被定义` | `code:include/chlib/fifo.h#q_async` | `test:tests/test_stream_fifo_dual.cpp#DUAL_BACKEND_TEST_PAIR` | `guard:scripts/verify_dual_backend_parity.sh#DUAL_BACKEND_PARITY_TOTAL` | — | ✅ |
| 09 | `stream_fifo` 行为 MUST 有双后端 parity 覆盖 | `spec:chlib-stream-semantics#的行为 MUST 有双后端 parity 覆盖` | `code:include/chlib/stream.h#stream_fifo` | `test:tests/test_stream_fifo_dual.cpp#DUAL_BACKEND_TEST_PAIR` | `guard:scripts/verify_dual_backend_parity.sh#DUAL_BACKEND_PARITY_TOTAL` | — | ✅ |
| 10 | FIFO 指针 MUST 受空/满门控（自保护） | `spec:chlib-stream-semantics#FIFO 数据通路的指针 MUST 受空/满门控` | `code:include/chlib/fifo.h#will_write` | `test:tests/chlib/test_fifo.cpp#[fifo][sync_fifo]` | `guard:scripts/lib/stream_domain_closure.py#def r10()` | — | ✅ |
| 11 | demux ready 方向（消费端 ⇒ 上游；**非选中输出隔离** · 越界挡住上游） | `spec:chlib-stream-ready-ownership#分流 / 分叉类组合子的 ready 语义 MUST 是` | `code:include/chlib/stream.h#stream_demux` | `test:tests/test_stream_demux_fork_dual.cpp#DUAL_BACKEND_TEST_PAIR` | `guard:scripts/lib/stream_ready_ownership.py#def r13()` | — | ✅ |
| 12 | fork 同步语义（`input ready` = AND(各 output ready)）· 数据面不随 ready 变 · `synchronous=false` 响亮拒绝 | `spec:chlib-stream-ready-ownership#分流 / 分叉类组合子的 ready 语义 MUST 是` | `code:include/chlib/stream.h#stream_fork` | `test:tests/test_stream_demux_fork_dual.cpp#DUAL_BACKEND_TEST_PAIR` | `guard:scripts/lib/stream_ready_ownership.py#def r13()` | — | ✅ |
| 13 | builder 链**上游反压**（链内闭包不变式：中间级读使能 ← 下一级容量信号） | `spec:chlib-stream-ready-ownership#链内中间级的读使能 MUST 由` | `code:include/chlib/stream_builder.h#queue` | `test:tests/test_stream_builder_backpressure_dual.cpp#DUAL_BACKEND_TEST_PAIR` | `guard:scripts/lib/stream_ready_ownership.py#def r10()` | — | ✅ |
| 13b | `stream_pipeline.h` 的 3 个 pipe（上游 ready 归属 + `output.ready` 可覆盖 + 数据通路保持） | `spec:chlib-stream-ready-ownership#链内中间级的读使能 MUST 由` | `code:include/chlib/stream_pipeline.h#stream_m2s_pipe` | `test:tests/test_stream_pipeline.cpp#[stream][pipeline][ready]` | `guard:scripts/lib/stream_ready_ownership.py#def r11()` | — | ✅ |
| 14 | 命名诚实性 —— `fwft_fifo` 实例**已消除**（物理删除 + 回归禁令 R11）· **通用条款已立法**（B8：被改动/名字说过谎的组件 MUST 登记 + 绑定判别性行为用例，护栏 R14） | `spec:chlib-stream-arbitration#仲裁器的命名与文档 MUST 与实现行为一致` | `code:include/chlib/fifo.h#q_async` | `test:tests/chlib/test_stream.cpp#[stream][arbiter][round_robin][rotation]` | `guard:scripts/lib/chlib_naming_honesty.py#def r2()` | — | ✅ |

## 能力面说明（**为什么这样切**）

- **01–06** = `capability chlib-stream-ready-ownership` 的 **fifo 类**（01–05）与
  **转发类**（06）—— 这两条是「ready 所有权」的**全部**已兑现面。
- **07** = `capability chlib-stream-arbitration` R1（**轮转**）—— 与 ready 所有权
  **正交**（它管的是「选谁」，不是「何时能推进」）。
- **08–10** = `capability chlib-stream-semantics` 的 FIFO 语义面。
- **11–12** = **分流 / 分叉类**（`demux` 方向 · `fork` 同步）—— 2026-11-02 由
  **ADR-056 / ADR-057** 兑现（同一条 Requirement + 同一个 dual 文件 + 同一个
  护栏判据 R13）。
- **13 / 13b** = **链式组合的链内中间级**（builder 链 · pipe 族）—— 2026-11-01 由
  ADR-055 的「链内闭包不变式」兑现（同一条 Requirement + 各自的判据 + 独立的
  护栏判据 R10 / R11）。
- **14** = 「名字/文档 over-promise」类缺陷 —— `fwft_fifo` 实例**已消除**（物理
  删除 + R11 回归禁令）· **通用条款**（B8）已立法 + 护栏 R14（登记表 ↔ 实现/测试
  的机械对齐）⇒ ⇒ 故本行**兑现**。

## 已知边界（如实声明）

- ⚠️ 本清单覆盖 **stream / fifo / builder / pipe** 四个面（`include/chlib/stream.h` ·
  `stream_arbiter.h` · `stream_builder.h` · `fifo.h` · **`stream_pipeline.h`**）
  ⇒ ⇒ **不含** `stream_operators.h` / `stream_width_adapter.h`
  ⇒ ⇒ ⇒ **若将来扩面，MUST 同时扩清单**（⇒ ⇒ 否则清单自身会成为「缩面」）。
  ⚠️ `stream_pipeline.h` 的纳入是 **2026-11-01（ADR-055）** 的事：此前它
  **不在** R8 扫描面内 ⇒ ⇒ 三个 pipe **不在任何 ready 契约的覆盖范围内**。
- ⚠️ 本清单**不断言**「stream 域已收官」⇒ ⇒ **收官的判据 = 本清单无 ❌ 行**
  ⇒ ⇒ ⇒ **当前有 0 行 ❌** ⇒ ⇒ ⇒ **⇒ stream 域已收官**（2026-11-02：B8 落地
  ⇒ ⇒ ⇒ ⇒ 命名诚实性的通用条款已立法 + 护栏 R14 守住「登记漂移 / 判别力消失」
  ⇒ ⇒ ⇒ ⇒ ⇒  ⇒ ⚠️ 收官 ≠ 「以后不会再有新的说谎组件」⇒ ⇒ ⇒ ⇒ ⇒ ⇒ **仍靠
  ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ 每条 capability 的契约 + 判据 + 护栏**逐项守住）。
- ⚠️ **归档文档 / 历史计划里的假陈述 MUST NOT 顺手改** ⇒ ⇒ 它们是**历史
  快照**（保真性 > 现行正确性）⇒ ⇒ 本清单第 14 行登记的 `fwft_fifo`
  over-promise，**活文档与 in-force ADR 已订正**（2026-11-01）⇒ ⇒
  ⇒ 但下列两处**保留原样**（**属历史记录**，非现行文档）：
  ① `docs/archive/old-usage-guides/CHLib_UsageGuide.md:58`
  （归档于 2026-04-08 的 usage guide）
  ② `PHASE1-REVISED-PLAN.md:66`（分析时间 2026-03-30 的历史计划）
  ⇒ ⇒ ⇒ **⇒ 「全仓 grep 无残留」不是本项的验收判据**（会误报）。
- ⚠️⚠️ **R1 只能验「指针可解析」（文件存在 + 标记命中）⇒ ⇒ 验不了
  「语义没漂移」** ⇒ ⇒ 例：第 13 行的欠账 marker 若仍指向源文件里**已被
  推翻**的旧表述，R1 **照样绿** ⇒ ⇒ ⇒ ⇒ ⇒ **⇒ 「可解析」≠「说的是同一件事」**
  ⇒ ⇒ ⇒ ⇒ ⇒ **⇒ 语义同步靠人工**（⇒ ⇒ 本仓已登记的病症：机械核验有边界）
  ⇒ ⇒ ⇒ ⇒ ⇒ **⇒ 已在本仓实际发生一次**（2026-11-01：第 13 行的 marker 从
  「静默丢字」改为「上游反压」⇒ ⇒ 靠人工发现）
- ⚠️ 「护栏」指针指向的是**判据本体或投毒名** ⇒ ⇒ 它证明「**有**自动化守护」
  ⇒ ⇒ **不**证明「该守护**覆盖了全部**契约条款」⇒ ⇒ 契约条款的**逐条**覆盖
  仍靠 capability 自身的 Scenario + 护栏自身的判据设计。

## 变更记录

| 日期 | 变更 | 依据 |
|---|---|---|
| 2026-11-01 | **创建**（15 行）⇒ ⇒ 首次把散在 4 处的未收口项收敛到一处 | P2 归档后的梳理；`chlib-stream-ready-ownership` Purpose 的「范围限定」段 + P1 台账 + P2 台账 + ADR-054 §6 |
| 2026-11-01 | **新增**第 15 行（待 architect 裁决的 8 项 ⇒ ⇒ 收敛到`.rddf/checks/architect-decisions-pending-2026-11.md` ⇒ ⇒ 此前散在 **6 处**） | A6 梳理轮结论：裁决项散落 6 处 ⇒ ⇒ 无人能一眼看出「还欠什么」 |
| 2026-11-01 | A4：第 14 行欠账指针改指**新台账**（命名诚实性审计）· **新增**第 13b 行（`stream_pipeline.h` 的 3 个 pipe：**实测零数据通过** + **不在任何 ready 契约覆盖范围** ⇒ ⇒ 契约缺口） | A4 实测（`/tmp/opencode/a4/probe_pipes*.cpp`）· 审计表见 `.rddf/improvements/chlib-naming-honesty-audit-2026-11.md` |
| 2026-11-01 | A3：第 13 行**根因订正**（「静默丢字」被实测推翻 ⇒ ⇒ 真缺陷 = **上游反压从未建立**）· **删**第 15 行（「编号一致性」是**护栏实现细节**，不是能力面 ⇒ ⇒ 已在护栏脚本内就地订正）· **新增**第 07b 行（`connect_ready` 多驱动守卫 + R9 + 投毒） | A3 实测（`/tmp/opencode/a3/probe_*.cpp`）· shell 标签「R2~R6」⇒ ⇒ 「R2~R6 + R8 + R9」· python 侧登记「R7 是空号」 |
| 2026-11-01 | 「已知边界」补登记**归档文档不改**（`fwft_fifo` over-promise 的两处历史快照保留） | A1 实施期发现：归档文档 + 历史计划属**历史快照**（保真性 > 现行正确性）⇒ ⇒ 不可用「全仓 grep 无残留」当验收判据 |
| 2026-11-01 | **第 13 / 13b 行 → ✅**（ADR-055 的「链内闭包不变式」兑现：新增 Requirement + 各自的 dual/行为判据 + 独立护栏判据 R10 / R11）· **删第 15 行**（8 项裁决**全部完成** ⇒ ⇒ 该行是过程项，不是能力面 ⇒ ⇒ 素材仍留在 `.rddf/checks/architect-decisions-pending-2026-11.md`）· 锚点 `17/6 → 16/3` · 「已知边界」把 `stream_pipeline.h` 纳入覆盖面 | 归档 change `stream-builder-and-pipe-ready-ownership-2026-11`（接 ADR-055）。⚠️ **本行的出现本身就是清单的一个已知边界**：状态列由**手写指针**推导 ⇒ ⇒ 实施完成后**不会自动记账** ⇒ ⇒ 靠人工补（与「R1 验不了语义漂移」同型） |
| 2026-11-02 | **第 14 行 → ✅**（命名诚实性：**通用条款**已立法（B8）· 登记表 ↔ 实现/测试 的机械对齐由新护栏 `scripts/lib/chlib_naming_honesty.py`（R1 登记漂移 · R2 判别力消失 · R3 per-change 义务）守住）⇒ 锚点 `16/1 → 16/0` ⇒ ⇒ **stream 域收官** | 归档 change `chlib-naming-honesty-clause-2026-11`。⚠️ **收官 ≠ 不会再有新的说谎组件** ⇒ ⇒ 仍靠每条 capability 的契约 + 判据 + 护栏逐项守住；且 R3 只覆盖「**最近的** chlib change」⇒ ⇒ 更早的 change 不再被回溯核对（如实登记） |
| 2026-11-02 | **第 11 / 12 行 → ✅**（`demux` 方向 · `fork` 同步语义：ADR-056 / ADR-057）⇒ 锚点 `16/3 → 16/1` ⇒ ⇒ **能力面全部兑现**，只剩第 14 行（B8 的「通用条款」） | 归档 change `stream-demux-fork-ready-semantics-2026-11`。⚠️ 第 11/12 行的 `guard` 指针指向 **R13**（`demux`/`fork` 共享上游驱动的结构判据）⇒ ⇒ 与 R8 **不可互相替代**（helper 的 `<<=` 被改成 `=` 时两个组件同时静默失效而 R8 仍绿） |
| 2026-11-01 | 第 14 行：`fwft_fifo` **实例已消除**（物理删除）⇒ `实现` 指针改指**真 FWFT**（`fifo.h#q_async`）· 补 `护栏` 指针（**R11 回归禁令**）· `欠账` 改指 **B8**（命名诚实性的**通用条款**仍未兑现）⇒ ⇒ **该行保持 ❌** | ⚠️ **不是「删了实例就改成 ✅」**：该行的 `test` 列**没有真实判据可填**（删除的正确性属于「不存在性」，只能由**结构护栏**守，无法由运行期断言证明）⇒ ⇒ 而**通用义务**（新增/改动组件 MUST 有行为用例锁定其声称语义）确实**未完成** ⇒ ⇒ ⇒ 保持 ❌ 才是诚实的口径 |
