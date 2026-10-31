# Change: stream-ready-ownership-correction-2026-10

## Why

🔴 **归档次日，P0 探针挖出 4 条「已归档契约在说谎」的缺口** ⇒ ⇒ 主 spec
`chlib-stream-ready-ownership` 与实现**公开冲突**（比「缺覆盖」更坏）。

⚠️ **本 change 不是「新功能」**，而是**收口订正**：把上一轮归档时**说得比事实多**的
契约条款**改回与实现一致**，并把**尚未落地的义务显式移交**（不静默丢弃、不静默弱化）。

## What Changes

### 🔴 3 个 BLOCKER 的闭合（critic 第 1 轮 = REJECT）

| BLOCKER | 闭合方式 |
|---|---|
| **B1**：Q1 裁决 = **B**（我原建议的 **C** 是 BLOCKER）⇒ 主 spec 归档时误并入「转发类 MUST 收口」⇒ ⇒ 仓内**直接先例**（`chlib-stream-semantics/spec.md:90-95`：2026-10-22 对抗审稿 BLOCKER 的收口订正 = 「未实施的 MUST 以『已生效契约』存在于主 spec 属于契约与实现不一致 ⇒ 从 in-force 契约移除，待 follow-up 以 ADDED delta 重新提出」）⇒ ⇒ **本 change 重犯同一个错** | **R1 正文**收窄：保留「fifo 类已兑现 + 有护栏覆盖」⇒ ⇒ 转发类条款改为**「适用范围」**段 ⇒ ⇒ **显式移交** P1 台账 §3 ⇒ ⇒ 待 P1 落地时经 delta **加回** ⇒ ⇒ **NOT** 采用「新增一条『实施中』的 Requirement」（= 主 spec 会含一条**今天为假**的 MUST） |
| **B2**：P0' 范围漏掉 spec 侧 2 处「说谎点」③ ④ | **③ R3** 补「`<<=` 的**充分条件** = 目标节点 MUST 已存在」（+ 前置条件：调用方 MUST 先建立字段节点）· **④** 顶部「实施完成且已验证」加**范围限定**（**仅 fifo 类**）+ 归档后补写 Purpose（先例 `core-lnode-entry-set`） |
| **B3**：P0' 漏掉 **ADR-054 §4**（整节形态 B 残留，与 §3.2 形态 E 直接矛盾 ⇒ ⇒ P1 实施方会执行**反向动作** ⇒ ⇒ P1 返工） | **ADR §4 整节**重写：删除形态 B 残留（「结果在 `connect_ready` 之前不完整 ⇒ 全部调用点 MUST 迁移」·「未调 `connect_ready` ⇒ 响亮 Value not found」）+ 删除悬空残片 + 补形态 E 的真实后果 |

### 7 处订正一览

| # | 位置 | 动作 | 权限 |
|---|---|---|---|
| ① | 主 spec `Requirement 1` | 正文**收窄**（fifo 类保留 / 转发类移交 P1）+ 新增「本条的适用范围」段 | **architect**（用户已批准代改） |
| ② | 主 spec `Requirement 2` | 「field proxy」理由订正 + 补「**覆盖生效的三个必要条件**」（目标已存在且非 literal · 源非空 · 源持久）+ 三类失效形态不同 MUST NOT 单判据覆盖 | build |
| ③ | 主 spec `Requirement 3` | 补「`<<=` 的**充分条件**」+ 「调用方 MUST 先建立字段节点」前置条件 | build |
| ④ | 主 spec 顶部断言 | 「实施完成且已验证」加**范围限定**（**仅 fifo 类**） | build |
| ⑤ | ADR-054 §3.2 第 1 条 | 订正理由（「field proxy」→「目标 MUST 已存在且非 literal」）+ **收窄「追认」范围**（critic F2-Q2：「`<<=` 空目标建节点」MUST NOT 作契约依据 ⇒ ⇒ 正确路径是让该分支**不可达**） | **architect**（用户已批准代改） |
| ⑥ | **ADR-054 §4（整节）** | 删除形态 B 残留 + 悬空残片 + 补形态 E 真实后果 | build（措辞订正） |
| ⑦ | ADR-054 `:27` | 「关联 change: **待建**」⇒ ⇒ 已归档（路径订正） | build |

### 判别力用例 + 护栏

- **判别力用例**（`tests/chlib/test_stream.cpp`，进**既有** target ⇒ ⇒ **计数锚点不变**）：
  断言「`connect_ready` 之后，**调用方持有的** ready 节点 `num_srcs() > 0`
  **且其值跟随**消费端 ready」⇒ ⇒ 否则 mux 的 `<<=` 修复仍是**假绿**
  （P0 发现 ③：调用方未建节点时 `<<=` 另建节点 ⇒ ⇒ 只驱动副本）；
- **护栏**：扩展 `scripts/verify_stream_ready_ownership.sh` 的 R1 覆盖面说明
  （明确 R1 只覆盖 fifo 侧 ⇒ ⇒ 转发类**尚无**行为判据 ⇒ ⇒ 与 R1 的适用范围
  段一致）。

## 为什么不做什么（Non-Goals）

- **不改实现**：本 change **零**实现改动（`include/chlib/stream.h` 只改**注释**）
  ⇒ ⇒ 转发类的收口是 **P1**（`stream-forward-ready-closure-2026-10.md`）；
- **不删「转发类」义务**：义务**移交**而非**丢弃** ⇒ ⇒ 台账 + delta 加回路径
  都在仓内 ⇒ ⇒ **可追踪**；
- **不动 `fwft_fifo`**（ADR-045:148）· **不动「读未驱动字段诊断」**（框架层缺口）
  · **不动 RR 轮转**（P2，独立）；
- **不动 `.rddf/roadmap/**` 的 feature `status` / objective 判据**（planner 职责）。

## Impact

- **改动文件**：`openspec/specs/chlib-stream-ready-ownership/spec.md`（归档时）·
  `docs/adr/ADR-054-stream-ready-ownership-form.md` · `include/chlib/stream.h`（注释 3 处）·
  `tests/chlib/test_stream.cpp` · `scripts/verify_stream_ready_ownership.sh`（头注释）
- **爆炸半径**：**小** —— 契约/文档 + 1 个测试用例（进既有 target）⇒ ⇒ **零行为变更**
- **风险**：**低** —— 唯一的实现侧改动是**注释**；⇒ ⇒ 「转发类未兑现」这件事
  从「隐藏」变成「**明文**」⇒ ⇒ **诚实度提升**，但**不改变任何运行时行为**
- **计数锚点**：**全部不变**（`CTEST_TOTAL` 205 · `VERILATOR_E2E_TOTAL` 38 ·
  `DUAL_BACKEND_PARITY_TOTAL` 29 · `DUAL_BACKEND_PARITY_PAIRS` 184 · non-perf 194 ·
  `EXAMPLES_TEST_SOURCES` 17 · samples 39/39 · examples 28/28）

## 权限说明

⚠️ **①⑤ 两处属 architect 权限**（critic 给出的判定规则：「**实施方读完新文本后
被允许/被要求做的事是否变了** —— 变 ⇒ 决策内容变更 ⇒ architect 裁决」）。
⇒ ⇒ **①**（R1 收窄 ⇒ 转发类义务从「本能力承诺」变成「移交 P1」）与 **⑤**
（ADR §3.2 第 1 条的裁决理由）**满足该条件** ⇒ ⇒ 用户于 2026-10-31 **明确批准**
由 builder 代改 ⇒ ⇒ 本 change 落笔时**加注**「⚠️ 角色越权执行（用户已批准）」。
⇒ ⇒ **②③④⑥⑦** 是**措辞订正**（实施方义务不变）⇒ ⇒ build 权限内。

## 评审记录

- **第 1 轮 critic（REJECT）**：3 BLOCKER + 6 SHOULD_FIX + 3 NITS ⇒ ⇒ 全部处置
  （见 `design.md` §「critic 第 1 轮的处置」）。
- **第 2 轮 critic**：待执行（只提交「BLOCKER 闭合证据 + 本轮新增 diff」）。
