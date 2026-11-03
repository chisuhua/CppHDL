---
name: chlib-naming-honesty-audit-2026-11
priority: P1
source: A4（梳理轮次 · 2026-11-01）——「命名诚实性」从 1 个组件扩到 1 类
phase: phase-5
category: chlib
type: audit
created_at: 2026-11-01
evidence: unmeasured
---

# Improvement: `chlib` 的**命名诚实性**审计（+ 顺带挖出 `stream_pipeline.h` 的**未收口面**）

- **Date drafted**: 2026-11-01
- **Drafted by**: build-agent (builder role)
- **Status**: ✅ **已实施**（2026-11-02 · B8 落地）⇒ ⇒ 交付 = 命名诚实性登记表 `.rddf/checks/chlib-naming-honesty-registry.md` + 护栏 `scripts/verify_chlib_naming_honesty.sh`（R1 登记 ⊆ 组件全集 / R2 判别性用例仍在 / R3 per-change 义务 + `--self-test` 3/3）⇒ ⇒ ⚠️ **登记范围只覆盖「被改动过 / 名字说过谎」的组件**，不是全仓普查（漏报方向）。
- **来源**：P2（`stream-arbiter-rr-rotation`）发现「`stream_arbiter_sequence` 的
  注释谎称 round-robin」⇒ ⇒ **该类缺陷（名字/注释 over-promise）此前只修了
  1 个组件** ⇒ ⇒ ⇒ **⇒ 本项做一次普查**
- ⚠️ **关联**：capability `chlib-stream-arbitration` R2（命名与文档 MUST 与
  实现行为一致）· 收口清单 `.rddf/checks/stream-domain-closure.md`
  第 14 行

## 1. Why (背景与动机)

🔴 **本仓最隐蔽的一类缺陷**：**组件名 / 注释 / 文档声称的语义**与
**实现的实际语义**不一致 ⇒ ⇒ ⇒ 读者（人或 AI）会照着声称写代码
⇒ ⇒ ⇒ **依赖不存在的行为** ⇒ ⇒ ⇒ ⇒ 且**编译通过 / 测试全绿**
⇒ ⇒ ⇒ ⇒ ⇒ **⇒ 静默失效**（本仓「假绿」家族）。

⚠️ **为什么此前只修了 1 个**：P2 修了 `stream_arbiter_sequence`（注释谎称
round-robin）⇒ ⇒ 但**没有普查** ⇒ ⇒ ⇒ **同类缺陷可静默复发**
⇒ ⇒ ⇒ ⇒ ⇒ **⇒ 这正是 capability `chlib-stream-arbitration` R2 要防的**。

⚠️ **本项的独特价值**：本仓登记过多次「**只查已知符号 ⇒ 漏掉同族入口**」
的护栏盲区 ⇒ ⇒ ⇒ 而**命名诚实性**的同类症状是「**只修了已知的那个**」
⇒ ⇒ ⇒ ⇒ ⇒ **⇒ 普查是唯一的解**（护栏无法凭空发现未知的 over-promise）。

## 2. What Changes (范围界定 —— **只登记，不顺手改**)

### 2.1 ✅ 已核对**一致**的组件（无需动）

| 组件 | 声称 | 核对方式 | 结论 |
|---|---|---|---|
| `chlib::lifo_stack` | 「LIFO Stack - 后进先出栈」 | 读实现：栈指针 push 递增 / pop 递减 + `sread(pop_addr)` | ✔ **真 LIFO** |
| `chlib::async_fifo` | 「异步 FIFO — 未实现（因 CppHDL 不支持 CDC）」 | 读注释 + 头注释 | ✔ **如实**（未实现也如实登记） |
| `chlib::DCacheWriteBuffer` | 「只有尺寸常量 —— 缓冲功能本身**尚未实现**（直通式）」 | 读注释（2026-10-28 已订正） | ✔ **如实** |
| `chlib::stream_s2m_pipe` | 「0-cycle payload delay」 | **实测**（A4 探针） | ✔ **兑现**（payload 组合直通） |
| `chlib::stream_arbiter_round_robin` | 「轮询方式」 | P2 已修实现（真轮转） | ✔ **一致** |
| `chlib::stream_arbiter_priority` | 「优先级仲裁器」 | 读实现（`priority_encoder`） | ✔ **一致** |
| `chlib::stream_arbiter_lock` | 「锁定仲裁器」 | 读实现（锁存首个通道） | ✔ **一致** |
| `chlib::stream_extend` / `stream_truncate` | 「zero-extend」/「taking LSBs」 | 读实现 | ✔ **一致** |
| `chlib::stream_narrow_to_wide` / `wide_to_narrow` | 「Accumulates multiple narrow transfers」/「Splits one wide transfer」 | 读实现 | ✔ **一致** |

### 2.2 ✅ 已核对**有偏差**的组件（**已修 / 已登记**）

| 组件 | 声称 | 实际 | 处置 |
|---|---|---|---|
| `chlib::fwft_fifo` | 「First-Word Fall-Through (FWFT) FIFO - 先字后进FIFO」/「输出端口始终显示下一个可读取的数据」 | **不是 FWFT**（实测：`q` 只在**读之后**才更新） | ✅ **已修**（A1 · 2026-11-01：标题块 + struct 成员 + 2 处「临时替代方案」+ in-force `ADR-008`）⇒ ⇒ **组件本身**的重写/改名**仍欠**（ADR-045 §2.4 推迟至今） |
| `chlib::stream_arbiter_sequence` | 「using round-robin selection」/「wraps around」 | 固定优先级扫描 | ✅ **已修**（P2 · 2026-11-01 · 零行为变更） |

### 2.3 🔴🔴 本次普查挖出的**新发现**：`stream_pipeline.h` 的 3 个 pipe —— **只在常量接线下活着** + **不在任何契约覆盖范围**

⚠️ **实测**（探针 `/tmp/opencode/a4/probe_pipes.cpp` + `probe_pipes2.cpp`）：

⚠️⚠️ **判定的精确措辞**（**第 5 次元教训**的载体 ⇒ ⇒ 见下）：**不是「功能死」**
⇒ ⇒ 而是「**只在『调用方把 ready 重绑定成常量』时活着**」⇒ ⇒ **真实用法
（上游遵守 ready 协议）下零通过**。

| pipe | 注释声称 | 实测（**正确接线**） | 判定 |
|---|---|---|---|
| `stream_m2s_pipe` | 「**1-cycle delay**」 | 🔴 **零通过**（`out.valid` 恒 0） | ❌ **只在常量接线下活着** |
| `stream_half_pipe` | 「全信号寄存 + **带宽减半**」 | 🔴 **零通过**（`out.valid` 恒 0） | ❌ 同上 |
| `stream_s2m_pipe` | 「**0-cycle payload delay**」 | ✅ 正常（payload 组合直通） | ✔ **兑现** |

⚠️⚠️⚠️ **两种接线的判决性对比**（探针 `/tmp/opencode/a4/probe_pipes3.cpp`）：

```
变体 A（**既有测试** `tests/test_stream_pipeline.cpp:20-22` 的口径）：
  `input_stream.ready = true;`（⚠️ **operator= 重绑定到 literal**）
  ⇒ ⇒ `input_fire = valid & ready` 恒 1
  ⇒ ⇒ 实测：out.valid=1 out.payload=0x42   ✔（与既有断言一致）

变体 B（**正确接线**：`as_master()` + `<<=`，**不驱动** `in.ready`）：
  ⇒ ⇒ `in.ready` num_srcs=0（未驱动 ⇒ 值 0）
  ⇒ ⇒ 实测：out.valid=0 out.payload=0x0     ✘（零通过）
```

⇒ ⇒⇒ **精确判决**：`stream_m2s_pipe` / `stream_half_pipe`
**不驱动上游的 `input_stream.ready`** ⇒ ⇒ ⇒ **上游遵守 ready 协议时
（ready 由上游驱动）⇒ ⇒ 组合子的 `input_fire` 恒 0 ⇒ ⇒ ⇒ 零通过**
⇒ ⇒ ⇒ ⇒ ⇒ **⇒ 根因 = pipe 的 ready 链【未收口】**
⇒ ⇒ ⇒ ⇒ ⇒ ⇒ **⇒ 与 P1-a 的 R8 缺口【同型】**
⇒ ⇒ ⇒ ⇒ ⇒ ⇒ **⇒ 且 `stream_pipeline.h` 【不在 R8 的扫描范围】**
⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ **⇒ ⇒ 双缺口**。

🔴🔴 **既有测试是「假绿」的一个【新形态】** —— 不是「缺覆盖」，而是
**接线把覆盖变成了假覆盖**：
`tests/test_stream_pipeline.cpp:20-22` 用 `operator=` 把 `ready` **重绑定成
常量 `true`** ⇒ ⇒ ⇒ **恰好绕过了「pipe 不驱动上游 ready」这个真实依赖**
⇒ ⇒ ⇒ ⇒ ⇒ **⇒ 断言全绿，但绿灯来自一个真实用法里不成立的假设**
⇒ ⇒ ⇒ ⇒ ⇒ ⇒ **⇒ 这是本仓「假绿」家族的新成员**
⇒ ⇒ ⇒ ⇒ ⇒ ⇒ **⇒ 教训：接线方式本身【就是】判据的一部分**
⇒ ⇒ ⇒ ⇒ ⇒ ⇒ **⇒ 「测试存在」不等于「测试验证了真实用法」**。

**机制（实测定位）**：
- `stream_m2s_pipe` 的 `p.output.ready.impl() == nullptr` ⇒ ⇒ **组合子自己的
  `ch_out` 字段节点从未建立**（`ch_stream` 未 `as_master()` / `as_slave()`）
  ⇒ ⇒ ⇒ 调用方 `p.output.ready <<= out_ready` 落在**空目标**上 ⇒ ⇒
  **另建节点** ⇒ ⇒ ⇒ **驱动进不去组合子内部读的那个节点** ⇒ ⇒ ⇒
  `input_fire` 恒 0 ⇒ ⇒ ⇒ **`valid_reg` 恒 0 ⇒ ⇒ 零数据通过**；
- ⚠️ `stream_half_pipe` 的 `p.input.ready` 与 `p.output.ready` 是**同一节点**
  （`operator=` 重绑定生效）且 num_srcs=1 ⇒ ⇒ 但 `out.valid` 仍恒 0
  ⇒ ⇒ ⇒ **机制未完全定位** ⇒ ⇒ **如实登记为「未定位」**（不猜）。

**⚠️⚠️ 第二重缺口（比功能死更结构性问题）**：
- 3 个 pipe 的**调用方** `in.ready` 的 `num_srcs() == 0`（实测 3/3）
  ⇒ ⇒ ⇒ **上游永远收不到反压**；
- ⚠️ **且 `stream_pipeline.h` / `stream_width_adapter.h` 不在护栏 R8 的扫描
  范围**（R8 只扫 `stream.h` + `stream_arbiter.h`）⇒ ⇒ ⇒
  ⇒ ⇒ **它们不在 capability `chlib-stream-ready-ownership` 的任何条款
  覆盖范围内** ⇒ ⇒ ⇒ ⇒ ⇒ **⇒ 这是一个【契约缺口】（不是漂移）**
  ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ **⇒ 收口清单必须新增行**（`.rddf/checks/stream-domain-closure.md`）。

### 2.4 ❓ **待实测**的组件（**如实登记为「未核」** —— 不假装已核）

| 组件 | 待核内容 | 备注 |
|---|---|---|
| `stream_transpose` | 「位宽转换/重组」与实现是否一致 | ⚠️ 需探针 |
| `stream_halt_when` / `stream_take_when` / `stream_throw_when` / `stream_continue_when` | 「暂停 / 只在条件为真时传递 / 丢弃 / 只在条件为真时继续」与实现的**条件语义**是否一致 | ⚠️ ⚠️ 与 ADR-051 的 `_if` **不是**同一套语义（`_if` 历史上双重失效）⇒ ⇒ 值得核 |
| `stream_join` | 「等待所有输入流都有效后传输」的 AND 语义 | ⚠️ 需探针 |
| `stream_combine_with` | 「组合两个流」的合并语义 | ⚠️ 需探针 |
| `stream_half_pipe` | 「**带宽减半**」的占空比（当前**零数据通过** ⇒ ⇒ 该声称**无法核**） | ⇒ ⇒ 依赖 2.3 的修复 |
| `stream_mux` / `stream_demux` | 路由语义（demux 的 ready **方向**问题已另立 P1-b） | ⚠️ 需探针 |

## 3. Acceptance (验收判据 —— MUST 可勾选)

- [ ] **效果判据**：每个「名字含强语义词」的组件 ⇒ ⇒ MUST 有一条**可执行**
      断言（运行期）锁定其声称的语义 ⇒ ⇒ **零行为覆盖的组件 MUST 登记**
      （⇒ ⇒ **零判别力的组件清单本身就是交付物**）
- [ ] **效果判据**：`stream_m2s_pipe` / `stream_half_pipe` ⇒ ⇒ MUST 有一条
      用例断言「**在正确接线（`as_master()` + `<<=`）下**数据能通过」
      （⇒ ⇒ 当前实测**零通过** ⇒ ⇒ 修复前该用例必红）
      ⇒ ⇒ ⚠️ **MUST NOT** 用「`ready = true` 常量接线」当判据
      （⇒ ⇒ 那是**既有测试的口径** ⇒ ⇒ 会把假绿固化）
- [ ] **效果判据**：`stream_pipeline.h` / `stream_width_adapter.h` 的组合子
      ⇒ ⇒ MUST 被纳入**某个** ready 所有权契约（或**显式豁免** + `::notice`
      点名）⇒ ⇒ ⇒ **禁止**「不在扫描范围 ⇒ ⇒ 静默不管」
- [ ] **效果判据**：每个「待实测」项 ⇒ ⇒ MUST 有实测结论（⇒ ⇒ **不**允许
      长期停留在「待实测」）
- [ ] **变异**：把任一组件的语义改坏 ⇒ ⇒ 对应用例 MUST **转红**
      （⇒ ⇒ ⚠️ **先确认编译成功**）
- [ ] `ctest -LE perf` 零回归 · **计数锚点不变**

## 4. Capabilities (契约影响)

- **MODIFIED** `chlib-stream-arbitration` R2 ⇒ ⇒ MUST 补「**审计制**」
  条款（「同类缺陷 MUST 有普查机制」⇒ ⇒ 否则「只修已知那个」会复发）
  ⚠️ ⇒ ⇒ **是否新增 Requirement 由 architect / planner 裁决**
  （本项**不擅自**改主 spec）
- ⚠️ **可能** MODIFIED `chlib-stream-ready-ownership` ⇒ ⇒ 若 pipe 族被纳入
  ⇒ ⇒ 需加条款（当前**不在**任何条款覆盖范围内 ⇒ ⇒ 缺口）

## 5. Impact (影响面)

- **改动文件**（本项**只登记**，不改实现）：
  `.rddf/checks/stream-domain-closure.md`（新增行）· 本台账
- **后续实施**（若立项）：
  `include/chlib/stream_pipeline.h`（3 个 pipe 的 ready 链 + 节点建立）·
  `tests/chlib/`（新增 pipe 行为用例）· 护栏 R8 的扫描范围（**或**显式豁免）
- **爆炸半径**：**中** —— pipe 族修好 = **行为从「零通过」变成「能通过」**
  ⇒ ⇒ ⇒ **既有消费者可能依赖「零通过」？**（⇒ ⇒ 实测仓内消费者需盘点）
- **风险**：**中** —— ① 「零通过」被改成「能通过」是**行为变更**
  ② 契约覆盖范围扩张 ⇒ ⇒ 需 architect 裁决
- **回滚路径**：`revert` 实施 commit
