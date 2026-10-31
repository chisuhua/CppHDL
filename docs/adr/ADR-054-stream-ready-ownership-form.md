# ADR-054: stream 组合子的 ready 所有权 —— **实施形态**裁决（修订 ADR-045 决策 0-B）

- **Status**: Accepted
- **Date**: 2026-10-31
- **Author**: build-agent（architect 角色，2026-10-31 用户口令切换后裁决）
- **Parent**: `ADR-045-chlib-stream-semantics.md`（§1 决策 0-B「输入 ready 的所有权：两阶段」）
- **关系**: **修订** ADR-045 决策 0-B 的**第 1 条**（构造期语义）与**第 2 条**（连接手段的适用范围）；
  其余条款（组合子不拥有入参 ready · `<组合子>_connect_ready` 形态 · `<<=` 连接手段 · 读取规则）**保持有效**
- **关联 capability**: **新建** `chlib-stream-ready-ownership`（**ADDED ×N** ⇒ 归档**禁** `--skip-specs`）
- **关联 change**: `chlib-stream-ready-ownership-2026-10`（**待建** ⇒ builder）

## 1. 背景（**只**陈述决策所需的事实）

ADR-045 决策 0-B 裁决了 ready 所有权**归属**（组合子不拥有入参 ready · 两阶段 · `<<=` 连接手段），
并明写「⏸ **本决策本次不生效** ⇒ 由 follow-up change 落地」。

实施期探针实测出**两条该裁决未记载的约束**（证据与逐条推导见
`.rddf/improvements/chlib-stream-ready-ownership-2026-10.md` §2.2，**不在本 ADR 重复**）：

- **E1**：**未驱动**的 `ch_bool` 代理节点**无法驱动任何输出**
  ——`ch_out::operator<<=` 要求 driver 非 null（`core/io.h:459`），
  `ch_out::operator=` 要求 `src_lnode` 非 null（`core/io.h:119`）⇒ ⇒ **两条路都不通**；
- **E2**：若采用「代理 + 阶段 2 覆盖」，则阶段 1 → 阶段 2 之间存在**读临时值**的窗口
  ⇒ ⇒ 「未调用 `connect_ready` 就仿真」会读到**临时值** ⇒ ⇒ **静默**。

⇒ ⇒ **E1/E2 迫使本裁决回答**：两阶段的**代理**该怎么建？以及**「响亮拒绝」能不能做出来？**

## 2. 承重区分（**为什么**不是所有组合子同一个形态）

| 组合子类别 | ready 的方向 | 需要对 ready 做什么 | 是否受 E1 影响 |
|---|---|---|---|
| **mux / demux / arbiter 类** | 消费端 ready → 各输入 ready | **驱动**结果字段 `input_streams[i].ready` | ❌ **否** —— 结果字段的节点在 bundle 构造时**已存在** ⇒ ⇒ 直接 `<<=` 驱动即可 ⇒ ⇒ **无需代理** |
| **fifo 类**（`stream_fifo`） | 消费端 ready → **内部读使能** | **读取**消费端 ready 作**内部逻辑的输入** | ✅ **是** —— 读使能在**构造期**就要用 ⇒ ⇒ 而消费端 ready 是**结果字段** ⇒ ⇒ **时序悖论** |

⇒ ⇒ **决策 0-B 的两阶段形态对 mux 类是足够的；对 fifo 类不够** ⇒ ⇒
**必须**为 fifo 类另立形态 ⇒ ⇒ 这就是本 ADR 的标的。

## 3. Decision（裁决）

### 3.1 三种候选形态

| # | 形态 | 结论 | 理由 |
|---|---|---|---|
| **A** | **代理 + 阶段 2 覆盖**：构造期建一个**带临时驱动**的代理 `ch_bool` 当读使能；`connect_ready` 里用 `<<=` **覆盖**其驱动 | ❌ **否决**（仅作对照保留） | ① **接受 E2 窗口** ⇒ ⇒ 「未调用 `connect_ready` 就仿真」读到**临时值** ⇒ ⇒ **静默** ⇒ ⇒ 违反本仓总原则「接受面里每一格 MUST **要么语义正确、要么响亮拒绝**」（ADR-052 D 总原则）⇒ ⇒ 该缺口**不可被编译期抓住** ⇒ ⇒ 只能靠「调用方纪律」⇒ ⇒ **不可接受**；② 依赖「`ch_bool::operator<<=` 在节点已存在时走 `set_src(0, src)` ⇒ **覆盖**已有驱动」这一**实现细节** ⇒ ⇒ 形态**不具可移植性**（换实现 ⇒ 静默失效） |
| **B** | **延迟构造**：组合子返回一个**不完整**的结果对象；调用方设置好 `result.pop_stream.ready` 后调用 `connect_ready(result)`，**此时**才建内部 FIFO ⇒ ⇒ 读使能**直接**取该 ready 的节点（**同一节点**）⇒ ⇒ **无代理、无窗口** | ✅ **采纳**（fifo 类） | ① **无代理** ⇒ ⇒ E1 **不适用** ⇒ ⇒ 不依赖任何实现细节 ⇒ ⇒ **可移植**；② **E2 消失** ⇒ ⇒ 「未调用 `connect_ready`」的后果 = 结果字段**无节点** ⇒ ⇒ 读取时报 **"Value not found"** ⇒ ⇒ **响亮**（不静默）⇒ ⇒ 满足 ADR-052 总原则；③ 与 mux 类的两阶段**形态统一**（都有 `connect_ready`，只是 mux 类在**构造期**就能算）⇒ ⇒ 调用方**一个**心智模型；④ **代价**（见 §4）：构造期**不产出** payload/valid ⇒ ⇒ **偏离**决策 0-B 第 1 条 ⇒ ⇒ 本 ADR **显式修订**之 |
| **C** | **显式读使能入参**：`stream_fifo(input, pop_ready_in)` ⇒ ⇒ 调用方在**调用前**驱动 `pop_ready_in` ⇒ ⇒ 单阶段 | ❌ **否决** | ① 调用方要**两处**喂消费端 ready（一次给 `pop_stream.ready`、一次给 `pop_ready_in`）⇒ ⇒ **同一信息两处表达** ⇒ ⇒ 易失配 ⇒ ⇒ **静默**风险；② 与 mux 类的两阶段形态**不一致** ⇒ ⇒ 调用方要记**两套**心智模型；③ 它其实是「把消费端的 ready **再喂一次**给 FIFO」⇒ ⇒ 语义冗余 |

### 3.2 裁决条款

1. **fifo 类组合子 MUST 采用形态 B（延迟构造）**：
   - 构造期 MUST **不**产出 `payload` / `valid` / `ready`（结果对象在该时刻**不完整**）；
   - MUST 提供 `<组合子>_connect_ready(result)`，由调用方在**设置好**消费端 ready **之后**调用；
   - `connect_ready` MUST 在**内部逻辑建立时**直接使用消费端 ready 的**同一节点**
     ⇒ ⇒ MUST NOT 引入代理节点 ⇒ ⇒ E1 MUST NOT 再出现。
2. **mux / demux / arbiter 类 MUST 保持决策 0-B 的两阶段形态**，且连接手段 MUST 为 `<<=`
   ⇒ ⇒ **修订**决策 0-B 第 2 条的**适用范围**：其「构造期只产出 payload / valid」
   对该类**保持有效**（它们的结果字段节点已存在 ⇒ ⇒ 无 E1）。
3. **「未调用 `connect_ready` 就读取结果」MUST 是**可诊断**的失败**（响亮拒绝）
   ⇒ ⇒ MUST NOT 退化为「读到常量 / 读到 0 / 读到临时值」。
4. **调用方 MUST 读 `result.input_stream.ready` / `result.input_streams[i].ready`**，
   MUST NOT 读自己的入参字段（决策 0-B 的读取规则**保持有效**）。
5. **形态 A（代理 + 覆盖）MUST NOT 被采用** —— 它依赖实现细节且接受静默窗口。
6. **决策 0-A（入参改引用）仍是**长期方向**，但**不再是唯一路径**
   ⇒ ⇒ 形态 B 落地后，若仍需进一步简化签名 ⇒ ⇒ 另议。

## 4. 后果 / 代价

### 正面

- **E1 / E2 均被消除**（形态 B）⇒ ⇒ 无「依赖实现细节的覆盖技巧」⇒ ⇒ **可移植**；
- 「未调用 `connect_ready`」⇒ ⇒ **响亮**（"Value not found"）⇒ ⇒ 满足
  ADR-052 总原则「要么语义正确、要么响亮拒绝」；
- 与 mux 类的**统一**心智模型（都有 `connect_ready`）⇒ ⇒ 调用方学习成本不增；
- 解锁 **2 个 disabled sample**（`stream_component_example` / `stream_handshake_example`）
  + 修复 **6 处 `ch_in` 静默失效**（`io().X = value` 落在 `ch_in` 上）。

### 负面

- 🔴 **公开 API 变更**：`stream_fifo` 的结果在 `connect_ready()` **之前不完整**
  ⇒ ⇒ 全部调用点 MUST 迁移（实测：3 个活跃 + 2 个禁用）⇒ ⇒ 旧代码**编译仍过**
  （`stream_fifo` 签名不变）但**行为错**（读到空节点）⇒ ⇒ ⇒ **必须**有护栏钉住
  「调用点 MUST 调 `connect_ready`」；
- ⚠️ **偏离**决策 0-B 第 1 条（「构造期只产出 payload / valid」）⇒ ⇒ 本 ADR
  **显式修订**之（⇒ ⇒ 决策 0-B 第 1 条对 fifo 类**不适用**）；
- ⚠️ 形态 B 下 `stream_fifo` **不能**在 `connect_ready` 之前被**内联**使用
  （如 `stream_fifo(x).pop_stream.payload`）⇒ ⇒ 调用点形态 MUST 改。

### 风险

| 风险 | 缓解 |
|---|---|
| 旧调用点**编译仍过**但**行为错**（读到空节点） | ① grep-lint 护栏：调用 `stream_fifo` / `stream_queue` / `StreamBuilder::queue` 之后 MUST 有对应的 `*_connect_ready`；② 契约 MUST 明写「结果在 `connect_ready` 前不完整」；③ ⚠️ 该缺口**不可**编译期抓住 ⇒ ⇒ MUST 有**运行期**判据 |
| `ch_uint<N>` 的 `<<=` 在**节点为 null** 时是否建节点（`ch_bool` 已确认会建，`uint.h:147` 待核） | ⚠️ **实施期 MUST 先探针验证** ⇒ ⇒ 形态 B 的可行性依赖它 |
| 「连接手段 MUST `<<=`」与既有 `stream_mux_connect_ready` 的 `operator=` **不一致** | 决策 0-B 第 2 条**保持有效** ⇒ ⇒ mux 类的 `connect_ready` MUST 一并改为 `<<=`（⇒ T3-b） |

## 5. Verification（**前瞻**验收判据，MUST 可勾选）

> ⚠️ 本章只列**验收判据**；实施期的实测记录 MUST 落在 change 的 `design.md`，
> **不得**写进本 ADR（过程日志不进架构文档）。

- [ ] `stream_fifo` 的读使能 MUST **NOT** 硬编码常量（ADR-045:160）
      ⇒ ⇒ grep-lint 护栏 MUST 抓到 `ready = true` / `pop_ready = true`；
- [ ] **反压场景可演示**：`full` **可达** ⇒ ⇒ 「FIFO 满 ⇒ 上游暂停」可被**断言**；
- [ ] 「未调用 `connect_ready` 就读取结果」MUST 产生**可诊断**失败
      ⇒ ⇒ MUST NOT 退化为「读到 0 / 读到常量」；
- [ ] `connect_ready` **之后**的 ready 链 MUST 是**同一条节点**
      ⇒ ⇒ MUST NOT 出现「代理节点被覆盖」⇒ ⇒ **E1 的回归锁**；
- [ ] **6 处 `ch_in` 静默失效** MUST 全部修复（逐处核实端口方向）；
- [ ] **2 个 disabled sample MUST 归位** ⇒ ⇒ `samples/disabled_samples.txt`
      MUST 移除对应行 + 补进 `CH_SAMPLE_TARGETS` ⇒ ⇒ samples 护栏数量守恒；
- [ ] **变异**：删掉 `connect_ready` 的调用 ⇒ ⇒ MUST 转红（**判别力**证明）；
- [ ] `ctest -LE perf` 零回归；**计数锚点不变**；
- [ ] 契约载体：capability `chlib-stream-ready-ownership`（**ADDED ×N**）
      ⇒ ⇒ 归档**禁** `--skip-specs`。

## 6. 不在本 ADR 范围

- **其余 5 个组合子**的 ready 所有权（T3-b）—— 本 ADR 只定**形态**，不逐个实施；
- **`fwft_fifo` 重写 / 改名**（ADR-045:148「单独立项」）；
- **决策 0-A**（入参改引用）的签名变更；
- **`examples/.../fifo/stream_fifo_example.cpp`** 的指针门控修复（ADR-045:197-198
  「反例（假满）」单独立项）。

## 参考

- `docs/adr/ADR-045-chlib-stream-semantics.md`（§1 决策 0-B · §「不在本次范围」）
- `docs/adr/ADR-052-multi-if-acceptance-surface.md`（总原则「要么语义正确、要么响亮拒绝」）
- `.rddf/improvements/chlib-stream-ready-ownership-2026-10.md`（实施期实测约束 E1/E2/E3 + 范围界定）
