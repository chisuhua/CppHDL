# ADR-076 审稿记录 —— `.verif` 原语集扩展（历史/序列判据）

- **ADR**: `docs/adr/ADR-076-verif-history-primitives.md`
- **关联 change**: `openspec/changes/verif-history-primitives-2026-11/`
- **审查顺序**: oracle → critic（**不可颠倒**）
- **结论**: **Accepted** · 双审闭合 · **0 BLOCKER**

---

## 1. oracle 第 1 轮（审查域: 其他审查）

- **判定**: `APPROVE_WITH_CONDITIONS`
- **输入**: `design.md`（4 条原语 · 扫描期静态良构性 · 两端对照）· `proposal.md` · ADR-074 全文 · 既有实现 4 个文件 · wait-level 同型先例

### 1.1 oracle 指出的根因（6 项）

| # | 根因 | 处置 | 闭合证据 |
|---|---|---|---|
| 1 | **静态计数器的作用域未钉死** ⇒ ⇒ 「运行期不可能超限」的前提缺失 ⇒ ⇒ per-scenario 局部写法会越界 / 误拒 | ✅ 采纳 | `design.md` §2.1（承诺成立的**充要前提**四条）+ §4（**静态计数器的生命周期**段 + **实施期反模式**两条）+ `proposal.md` AC16 |
| 2 | **`EXPECT_SEQ_STEP` 的「同位」在 LV ≠ LE 时不是「同拍」** ⇒ ⇒ 作者会成「第三个解释器」 | ✅ 采纳 | `design.md` §2.3（「同位」的**精确定义** + **使用前提** + 交错记录范式）+ `proposal.md` AC17 |
| 3 | **>64 位端口的 `RECORD` 截断**未登记（两端「大概都截断」） | ✅ 采纳（**改为拒绝**而非登记） | `design.md` §4「端口位宽」行 + §5.2 + `proposal.md` AC15 |
| 4 | **场景首样本继承复位尾缝**（渲染侧多一个 `@(posedge clk)`） | ✅ 采纳（**继承登记** + 夹具前缀范式义务） | `design.md` §5.3「复位尾缝」行 + `proposal.md` AC21 |
| 5 | **⑥''/D5 锚点义务未对新原语裁决** | ✅ 采纳 | `design.md` §6.1（D5/⑥'' 裁决）+ `proposal.md` AC20 |
| 6 | 次要规格留白（32 位上溢 · STEP 双端口措辞 · sanitize 单射 · 多标识符两历史） | ✅ 采纳 | `design.md` §4 表 + §5.2/§5.3 + `proposal.md` AC2 / AC18 / AC19 |

### 1.2 oracle 的「被否方案复核」

- ④ 个被否方案（降级绝对判据 / 万能原语 / `EXPECT_SEQ_EQ` / 变量与条件）**否决理由站得住**
  ⇒ ⇒ 唯一加固项 = `EXPECT_SEQ_EQ` 的否决属**负载断言** ⇒ ⇒ 已加**举证义务**
  （迁移 change MUST 附逐条映射表；出现 port-vs-port 历史恒等型消费者 ⇒ STOP）。
- **无更小切法**：`OBSERVE` + 外部比对会重开**第二语义实现**（违反 D1 核心不变量）。

---

## 2. critic 第 1 轮（review_profile: arch, build）

- **判定**: `APPROVE_WITH_CONDITIONS` · `mergeable: yes` · `scope_exceeded: false`
- **BLOCKER**: **0 条**

### 2.1 critic 的 focus 核对结论

| focus | 结论 |
|---|---|
| ① ADR-074 冲突面 | ① §7 自禁追加 ⇒ **独立新 ADR 是唯一合规载体**（本 ADR 走的就是这条路）② 「生成期求值 ⇒ IR 无表达式」**逐字保持** ③ D4 条数等式口径自洽（本 change 不迁 example ⇒ `min_checks=113` 不受影响）④ D7 逃生口与「4 条都有消费者」**不矛盾** ⑤ D8 举证被 AC4/AC5/AC6 覆盖且**严于**原文 |
| ② 仓内契约/护栏面 | ① ⑥/⑥'/⑥''/⑥⁗/⑨ 的判据对象全在 `examples/**` + tsv ⇒ 新夹具落 `tests/**` ⇒ 无自洽性问题 ② ②'' 是**软告警** ⇒ 无冲突 ③ **公开面登记义务未被覆盖** ⇒ SHOULD_FIX #3 |
| ③ 文档载体 | 独立 ADR 合规 · `design.md` 自我定位为「推导与实现细则」**无越权** ⇒ 仅 NIT（决策回流义务） |

### 2.2 critic 的 SHOULD_FIX（3 条 · **全部采纳**）

| # | finding | 处置 | 闭合证据 |
|---|---|---|---|
| #1 | **原 M3 变异无效**（`EXPECT_SEQ_STEP` 的 `n` 40→39 **恒绿** ⇒ 因该原语只产 1 条 check 且 `n` 减 1 只是窗口**严格子集** ⇒ ⇒「条数不等」的预期**不成立**）⇒ ⇒ 照表实施会「预期红实际绿」 | ✅ 采纳（**撤回** M3-n ⇒ ⇒ 换成「两端口对调」；⇒ ⇒ 并按仓内「变异纪律」第 ④ 条**如实登记**为无效变异且**不计入**判别力条数） | `design.md` §6 变异表（M3 换为端口对调 + **已撤回的变异**段）· `proposal.md` AC6 |
| #2 | **AC15–AC21 无任务映射**；A3/A5 判据列**漏**「width>64 fail-closed」与「sanitize 单射」⇒ ⇒ 这两点是「宽端口截断这一类分叉在扫描期消灭」的**承重件** ⇒ ⇒ 漏任务即可能漏实现 | ✅ 采纳 | `tasks.md` 批次 A（A3/A5/A6 判据列已补）+ **AC → 任务映射**已补 AC15–AC21 |
| #3 | **C4 文档同步清单漏** `.rddf/checks/public-api-surface.md` 登记义务（公开头加 4 kind + 5 字段 ⇒ ADR-074 §6 的登记义务被触发）⇒ ⇒ 若裁决「不触发」则 MUST 写明裁决与理由，**不得让义务悬空** | ✅ 采纳（C4 已加该义务 + 「若不触发则写明裁决」的兜底） | `tasks.md` C4 |

### 2.3 critic 的 NITS（4 条 · **全部采纳或已登记**）

| # | NIT | 处置 |
|---|---|---|
| N1 | 批次 0 的 ADR 双审结论若与 `design.md` 冲突 ⇒ MUST 回流修订 | ✅ `tasks.md` A0 已加该条款 |
| N2 | sanitize 撞名检查落在**渲染期** ⇒ ⇒ 解释器会「先跑成功再在渲染侧失败」⇒ ⇒ 两端不同步 | ✅ **改为扫描期**判定 + sanitize 函数由扫描器与渲染器**共用**（落 `src/rtl_emit/verif_internal.h` 内部头 ⇒ 不扩公开面）⇒ ⇒ `design.md` §5.2 + `tasks.md` A3 |
| N3 | 比对形态升级（逐对 vs D8 原文「集合相等」）应让 ADR 明记 | ✅ ADR-076 **D7** 明记 |
| N4 | 静态充足性对「`RECORD` 与 `EXPECT_SEQ_*` 同处一个 `REPEAT` body」是**过拒** | ✅ **登记**为已知限制 + 写法约束（记录与比较分到不同拍次）⇒ ⇒ `design.md` §4 + `tasks.md` A6 |

---

## 3. 实施期发现（2026-11-21 · 由判别力夹具抓出 · **已闭合**）

| 项 | 内容 |
|---|---|
| **缺陷** | `EXPECT_SEQ_STEP` 的公式**差一错位**：V 侧写成 `hV[LV-n+i+1]`（⇒ **越界**读 `hV[LV]` ⇒ UB）· E 侧写成 `hE[LE-n+i]`（⇒ **晚一拍**的 E 值 ⇒ ⇒ 「造成这次跃迁的 E」应是**较早端**的那个） |
| **为何两端没抓到** | ⇒ ⇒ **两端一致地错** ⇒ ⇒ 正是根 `AGENTS.md`「**两后端一致不能当作正确性证据**」的活标本 ⇒ ⇒ 只有**手推期望**能抓 ⇒ ⇒ 夹具的手推期望（`Δcnt == en`）当场转红 |
| **修正** | `(hV[LV-n+i] - hV[LV-n-1+i]) mod 2^64 == hE[LE-n-1+i]`；**扫描期充足性**改为两端口**各需 ≥ n+1** |
| **同步面** | 解释器 · 渲染器 · 扫描期充足性（3 处同批）· `design.md` §2.3 · **ADR-076 D5** |
| **教训（已写进 ADR）** | ADR-076 D5 的公式里加了「**不是** `hV[LV-n+i+1]`（那会越界）」「**不是** `hE[LE-n+i]`（那是晚一拍）」两条**反向锚点** ⇒ ⇒ 防同一形态复发 |

---

## 4. 闭合后的状态

| 项 | 状态 |
|---|---|
| 四要素（元数据 / 决策 / 选项对比表 ≥3 行 / 后果代价章 / Verification 章） | ✅ 齐（ADR-076 §1–§5） |
| oracle → critic 双审 | ✅ 闭合 · **0 BLOCKER** |
| `Status` | ✅ `Accepted`（2026-11-21） |
| 关联 change | ✅ 已立项（`openspec/changes/verif-history-primitives-2026-11/`）· AC1–AC21 已映射 |
| ADR-074 正文 | ✅ **逐字未改**（change 的 tasks A1 判据） |
| 审稿报告 | ✅ 本文件 |

---

## 4. 遗留（SHOULD_FIX / NITS 的落点）

| 项 | 落点 |
|---|---|
| `EXPECT_SEQ_EQ` 否决的举证义务 | 迁移 change（`spinalhdl-ported-verif-migration-2026-11`）的逐条映射表 |
| 公开面登记（4 kind + 5 字段） | 迁移 change / 本 change 的 C4（`.rddf/checks/public-api-surface.md`） |
| 「记录与比较分到不同拍次」的写法约束 | 迁移 change 的写法指南 |
