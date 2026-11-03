# ADR-064: chlib stream 宽度适配器 —— 裁决「重写 vs 删除」· ready 所有权 · 寄存输出

- Status: **Accepted**（2026-11-03，用户授权「按你的建议继续 A-2」）
- Date: 2026-11-03
- Author: build-agent（architect role）
- Parent: **ADR-054**（ready 形态 E：默认值 + 可覆盖）· **ADR-055**（链内中间级 ready 所有权）
  · **ADR-042**（bundle 字段一等节点）· **ADR-045**（FIFO 自保护）
- 关联 improvement: `.rddf/improvements/stream-width-adapters-crash-on-simulate-2026-11.md`
- 关联 capability: `chlib-stream-ready-ownership`（**MODIFIED ×1 + ADDED ×1**）
  ⇒ 归档**禁** `--skip-specs`
- 前置证据：三个独立探针（`/tmp/opencode/probe_wa1.cpp` 复现崩溃 ·
  `probe_wa2.cpp` 三模式对照），**不入仓**

---

## 1. Context

### 1.1 触发

路线 B 的 4 问① 只读查重逐个读 `include/chlib/stream_width_adapter.h` 时发现，
并用独立探针实测确认。improvement 已登记，但把「数据通路」标为**未实测**
⇒ 本 ADR 补了实测（§1.4）。

### 1.2 事实一：三个对外 API **一仿真就 SIGSEGV**（实测）

| API | 行 | 探针结果 |
|---|---|---|
| `stream_narrow_to_wide` | `:54` | 💥 **exit=139** |
| `stream_wide_to_narrow` | `:107` | 💥 **exit=139** |
| `stream_width_adapter`（同宽分支） | `:163` | 💥 **exit=139** |
| `stream_extend` | `:191` | ✔ 可仿真（但**零消费者 + 零测试**） |
| `stream_truncate` | `:213` | ✔ 可仿真（同上） |

崩溃点在 `Simulator` 构造（⇒ `initialize()` 实例化图）。根因是**空操作数**：

```cpp
// :91 —— result.ready 是刚建出来的 ch_stream 字段，**没有任何节点**
input.ready = select(is_full, result.ready, ch_bool(true));   // 🔴 mux 的操作数为空
```

同型第二处：`:133` 的 `is_done & result.ready`（`stream_wide_to_narrow`）。

⚠️ 对照仓内**已修好**的形态（`stream.h` 的 `stream_arbiter_priority`）：
先 `result.output_stream.ready <<= default_ready;` 建默认驱动，**再**读它。
⇒ 适配器**缺了这一步** ⇒ 与 ADR-054 形态 E 的既有纪律不一致。

### 1.3 事实二：既有 8 个用例**零判别力**

`tests/test_stream_width_adapter.cpp` 有 **8 个 `TEST_CASE`**，但 `Simulator`
出现 **0 次** ⇒ 全部裸 `context` + 只断 `payload.width` / `valid.width` / `ready.width`
⇒ **图从不被实例化** ⇒ 空操作数永不兑现 ⇒ 缺陷完全不可见。

⇒ 这是「**裸 context 的测试连「图能否被实例化」都验证不了**」的直接证据
⇒ 与本仓「`R6` 排除裸 context」是同一条线索的两面。

### 1.4 事实三：数据通路也错（实测，improvement 只标了「疑似」）

8→32 累积，输入 `a0..a3`（`0xA0..0xA3`）然后 `b0..b3`，下游恒 ready。
三种实现形态的 **tick 后**观测：

| 形态 | 第 4 拍（完整字应出现） | 第二组 | 说明 |
|---|---|---|---|
| 只补默认 ready 驱动 | `00a2a1a0` | `a3a2a1b0` | **缺最后一个窄字** + **跨组污染**（`accumulator` 从不清零） |
| 补「payload 含当前字」+ 清零（**组合输出**） | `a2a2a1a0` | — | ⚠️ 见下方「读点错位」 |
| **寄存输出**（valid/payload 都是 `ch_reg`） | **`a3a2a1a0`** ✔ | **`b3b2b1b0`** ✔ | 两者都对 |

#### ⚠️ 「组合输出」的关键问题：**可观测性**，不是正确性

组合输出（`valid`/`payload` 都是当拍组合值）在真实硬件里**语义是对的**（输入在
时钟沿上变化 ⇒ 沿前的组合值正是消费者采样的值）。但本仓的测试读模型是
`set_input → tick() → get_port_value()` ⇒ 读到的是**沿后**组合值，而 `tick()` 的
最后一步用**边沿后的寄存器 + 当拍输入**重新求值 ⇒ **valid 比"消费者采样时刻"早一拍**
⇒ 探针与测试**无法观测到那个正确的组合值**（读到的是 `a2a2a1a0` 这种中间态）。

⇒ ⇒ **若选组合输出，双后端 parity 判据写不出来**（只能验 payload，而 payload 也是
组合的，同样错位）⇒ ⇒ 而这正是本 objective 的下一步（`stream_width_adapter`
是路线 B 仅剩的候选）。

### 1.5 事实四：消费者 = **0**

全仓只有 `include/chlib.h:49` 的导出与该测试文件 ⇒ **零生产消费者** ⇒ 爆炸半径为零。

---

## 2. Decision

### D1. **重写**（不物理删除）

删除的先例是 `fwft_fifo` / `stream_arbiter_sequence` —— 但它们的**能力已被别处覆盖**
（`sync_fifo.q_async` / `stream_arbiter_priority`）。宽度适配**没有替代**
（`stream_extend` / `stream_truncate` 只做单拍零扩展/截断，不做多拍累积/拆分）
⇒ 删除 = **造能力缺口** ⇒ 重写。

### D2. ready 所有权：形态 E + 调用方 MUST `as_master()`

- 结果对象的 `ready` MUST **先建默认驱动**（`ch_bool default_ready(true); result.ready <<= default_ready;`）
  **再**被读；
- 上游 `input.ready` MUST 用 **`<<=`**（数据流边）驱动，**MUST NOT** `operator=`
  （重绑定 ⇒ 调用方收不到反压 ⇒ 静默失效）；
- 调用方 MUST 先 `as_master()`（否则 `ready` 无节点 ⇒ 组合子驱动无效）
  ⇒ 该接线纪律 MUST 写进文件头注释与使用示例。

### D3. 输出形态：**寄存**（`valid` / `payload` 都是 `ch_reg`）

这是**架构约束**，不是实现偏好：**组合输出的 valid 在本仓测试读模型下不可观测**
（§1.4）⇒ 选它等于放弃为这个组件写 parity 判据。

- 代价：**+1 拍延迟**；
- 收益：所有输出 `tick()` 后即可观测 ⇒ 测试与 dual parity **可写**；
- 契约：`valid` MUST 保持到 `ready`（标准握手）；`payload` 在 `valid` 期间 MUST 稳定。

### D4. `narrow_to_wide` 的数据通路契约

| 项 | 契约 |
|---|---|
| 累积顺序 | 第 k 个窄字落在 `[k*NB, (k+1)*NB)`（**低字节先到**） |
| 完整字 | 宽字 MUST **包含最后一个窄字**（现状缺它） |
| 组间隔离 | 一组完成 MUST **清零**累积器（现状 OR 累积 ⇒ 跨组污染） |
| 背压 | 满时（等最后一个字）`input.ready` MUST 反映「输出寄存器能否被腾空」；**MUST NOT** 无条件 1 |
| 气泡 | 输入 `valid=0` 时 MUST NOT 推进计数器、MUST NOT 改变累积器 |

### D5. `wide_to_narrow`：数据通路**已核对正确**，只改 ready

手推 + 探针核对：拆分顺序（`data_reg >> k*NB`）、`counter` 回绕、`valid_flag`
保持到 `ready` 均正确 ⇒ 只改 §D2 的两处 ready 缺陷。
⚠️ **实现顺序约束**：`input.ready` MUST **先**驱动、**再**参与 `load_new = input.valid & input.ready`
（否则读的是空节点 ⇒ 仍崩）—— 这条与 `stream_arbiter_priority` 的注释同源。

### D6. 同宽直通分支（`stream_width_adapter` 的 `else`）：形态 E

零状态直通 ⇒ 不做累积，但 `result.ready` 同样 MUST 先建默认驱动、
`input.ready` MUST 用 `<<=`。

### D7. `stream_extend` / `stream_truncate`：**物理删除**

零消费者 + 零测试 + 静默失效（`input.ready = result.ready` 是重绑定）⇒
按 Zero-Debt（「No dead code … Never leave for later」）删除，**不**给它们补测试。

### D8. capability 归属：`chlib-stream-ready-ownership`（MODIFIED ×1 + ADDED ×1）

- **MODIFIED**：把「转发类组合子的 ready 所有权 MUST 是默认值 + 可覆盖」的
  **适用面**扩展到**宽度适配类**（含直通分支）；
- **ADDED**：「组合子 MUST 构建出**可实例化**的图」+ 适配器的输出形态/数据通路契约。

⚠️ **不新建 capability**：该 capability 的 Purpose 已是「stream 组合子的 ready 所有权」
⇒ 适配器就是 stream 组合子 ⇒ 新建会产生两个管辖同一面的 capability。

### D9. 修复后 MUST 补两类判据（同 change 内）

1. **行为判据**（interpreter）：§D4 的 5 条契约逐条可判 + 「构造 `Simulator` 不崩溃」；
2. **dual parity**：三个适配器进 `DUAL_BACKEND_TEST_PAIR`（本组件是路线 B 仅剩的候选）。

⚠️ 既有 8 个结构性用例 **MUST NOT** 被删除（它们锁宽度/类型），但 **MUST** 补一条
「真跑仿真」的用例 —— 否则「零判别力」的病根没治。

---

## 3. 选项对比

| # | 选项 | 结论 | 理由 |
|---|---|---|---|
| A | **最小修复**：只补默认 ready 驱动 | ❌ 否决 | 实测仍**缺最后一个字** + **跨组污染**（§1.4）⇒ 从「崩」变成「静默错数据」，**更坏** |
| B | 最小修复 + 修数据通路，**保持组合输出** | ❌ 否决 | 语义虽对，但在 `set_input → tick → read` 下 **valid 不可观测** ⇒ **写不出 parity 判据** ⇒ 与 D9 冲突 |
| C | **重写 + 寄存输出**（D2–D6） | ✅ **选中** | 语义正确且**可观测**；代价仅 +1 拍；重写面 ≈ 100 行、零生产消费者 ⇒ 爆炸半径为零 |
| D | 物理删除整个文件（含 `wide_to_narrow`） | ❌ 否决 | 宽度适配**无替代**（`extend`/`truncate` 是单拍组合，不是多拍累积/拆分）⇒ 删除 = 造能力缺口；与 `fwft_fifo` 的先例不同（那有替代） |
| E | 冻结现状 + 文档化「不可用」 | ❌ 否决 | 组件**一仿真就崩**；`chlib.h` 已对外导出 ⇒ 用户按文档用就崩 ⇒ 文档化不能替代修复 |

---

## 4. 后果与代价

### 正面

- 三个对外 API 从「**一仿真就崩**」变成**可用且有契约**；
- 顺带消掉三类同源缺陷：空操作数崩溃 · `operator=` 重定向（静默无反压）· 数据通路错；
- `stream_extend` / `stream_truncate` 的**静默失效**被消除（删除而非留坑）；
- 为路线 B 补上最后一个候选（`stream_width_adapter`）的 dual parity。

### 负面 / 代价

- **+1 拍延迟**（D3）⇒ 调用方若做吞吐敏感的 0 延迟适配，需自行绕开（本仓零消费者，
  当前无影响）；
- 「组合输出不可观测」这条**架构约束**会永久限制未来同类组件的形态选择：
  想 0 延迟就**写不出 parity 判据** ⇒ 已登记在 §1.4，后来者须知道这个取舍；
- 重写后**既有 8 个结构性用例**仍只验宽度 ⇒ 它们对行为仍零判别力 ⇒
  行为判据必须**新增**（D9），不能靠"用例数变多"装作覆盖提升。

### 风险

| 风险 | 缓解 |
|---|---|
| 重写引入新的时序错误（累积器/计数器边界） | §5 要求**手推期望表**（探针已给出正确序列 `a3a2a1a0` / `b3b2b1b0`）且**禁抄仿真输出** |
| 背压路径未真正打通（只测恒 ready） | §5 强制一个「下游 ready=0 时上游必须停」的用例（存量缺陷正是"从不测背压"） |
| `as_master()` 接线纪律被遗忘 ⇒ 又静默失效 | 契约条款 + 文件头注释 + 用例固定写法；⚠️ 当前**无通用护栏**（见 `input-port-driving` 系列已登记的护栏缺口） |
| 删除 `extend`/`truncate` 打断未知下游 | 全仓 grep 已确认**零消费者**（含 `chlib.h` 的导出面也不含它们） |

---

## 5. Verification（前瞻验收判据 —— **可勾选**；实施后逐条回填）

### 5.1 不再崩溃（D2）

- [ ] `stream_narrow_to_wide` / `stream_wide_to_narrow` / `stream_width_adapter`
      构造 `Simulator` **不崩溃**（同宽、窄→宽、宽→窄 三种）
- [ ] 上游 `ready` 由**调用方持有的 handle** 观测到（不是组合子内部副本）

### 5.2 数据通路（D4 / D5）

- [ ] `narrow_to_wide` 8→32：`a0..a3` ⇒ 宽字 `0xA3A2A1A0`（**含最后一个字**）
- [ ] 第二组 `b0..b3` ⇒ `0xB3B2B1B0`（**无跨组污染**）
- [ ] 输入 `valid=0` 的拍 ⇒ 计数器不推进、累积器不变
- [ ] `wide_to_narrow` 32→8：`0xA3A2A1A0` ⇒ 依次 `0xA0` / `0xA1` / `0xA2` / `0xA3`
- [ ] **背压**：下游 `ready=0` ⇒ 上游 `ready=0` 且输出保持不变

### 5.3 输出形态（D3）

- [ ] `valid` / `payload` 都是寄存器 ⇒ `tick()` 后读即得（不依赖组合重求值时序）
- [ ] `valid` 保持到 `ready`；`valid` 期间 `payload` 稳定

### 5.4 删除（D7）

- [ ] `stream_extend` / `stream_truncate` **不存在**（编译期断言或 grep 判据；二者从未被单独导出）

### 5.5 变异验证（每条**先确认编译成功** ⇒ 否则跑的是旧二进制 ⇒ 假绿）

| 变异 | 预期 |
|---|---|
| 去掉 `result.ready <<= default_ready` | 🔴 崩溃复现（`initialize()`） |
| `input.ready <<=` 改回 `operator=` | 🔴 上游 ready 恒 0/恒 1 ⇒ 背压用例红 |
| `is_full & fire` 时不清零累积器 | 🔴 跨组污染用例红 |
| 输出改成组合（去掉寄存） | 🔴 §5.3 的「tick 后可得」用例红（**这正是 D3 的判据**） |
| 宽字用 `accumulator`（不含当前字） | 🔴 `0xA3A2A1A0` 用例红 |

### 5.6 契约与计数

- [ ] `chlib-stream-ready-ownership` 的 MODIFIED + ADDED 已合并（禁 `--skip-specs`）
- [ ] 三个适配器进 `DUAL_BACKEND_TEST_PAIR`（⇒ parity 源文件数 +1）
- [ ] `ctest -LE perf` 零回归

---

## 6. 参考与审查记录

- ADR-054（形态 E）· ADR-055（链内 ready）· ADR-042（字段一等节点）· ADR-045（FIFO 自保护）
- 同族两个已登记缺陷：`stream_arbiter_lock` · `round_robin_arbiter` ⇒ 各自独立立项，不在范围
- ⚠️ **同文件族的文档缺陷**（`priority_encoder` 注释与实现相反）= 独立议题
  （`priority-encoder-doc-contradicts-implementation-2026-11.md`），**不夹带**
- 审查记录：**build-agent 自查一轮**；critic 因 Token Plan 配额不可用（外部限制），
  **未过外部评审** ⇒ 实施前如有异议请指出
