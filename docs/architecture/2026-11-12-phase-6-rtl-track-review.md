# Phase 6 / RTL 轨复盘 #3（T2 裁决后 · architect 域 · 2026-11-12 晚）

> **触发**：用户口令「切 architect 对完成和后续任务进行梳理，检查下我们要实现的目标，
> 为什么要实现这个目标, 已经完成了哪些，后续要怎么做, 为什么要这么做，目前的任务是什么，
> 然后切 planner 对后续还有哪些任务内容都梳理一遍，再给出后续任务内容和实施路径」。
> **本文件的定位**：**当前态快照 + 目标链 + 本轮实测发现 + 边界裁决**（架构主题文档 · **非决策文档**）
> ⇒ ⇒ **决策内容零变更**；本轮**不新建 ADR**、**不反转任何已签批决策**。
> **窗口唯一真相** = `.rddf/roadmap/phases/phase-6.md`（**本文件 §2–§6 是 2026-11-12 的实测快照，
> 会过期** ⇒ ⇒ **不作为状态真相使用**）。
> **上游** = `2026-11-12-phase-6-b2-midterm-review.md`（复盘 #2 · B2 中期）· `phase-6-rtl-productization.md`
> （批次编排 · 12 条依据可 grep）· `docs/adr/ADR-069-mem-data-alignment.md`（**Accepted** · T2 裁决）。
> **下游** = `.rddf/roadmap/phase-6-next-steps.md`（planner 域 · **任务内容 + 实施路径**的权威载体）。

---

## 1. 目标链（**做什么 → 为什么**）

### 1.1 顶层目标：为什么要有「RTL 生成能力产品化」这条线

`CppHDL` 的定位是 **C++23 的硬件描述库** ⇒ ⇒ 它的**交付物**不只是「能仿真的 C++ 代码」，还必须包括
**能被下游 FPGA/ASIC 工具链吃掉的产物**（Verilog + 端口清单 + 行为等价性）。

⇒ ⇒ 用户 2026-11-10 拍板的 7 项诉求，**每一条都指向同一个病根**：

| 用户诉求 | 草案 | 病根 |
|---|---|---|
| ① 生成器作为项目提供的能力 | `rtl-emit-product-tooling` | 「**能生成**」与「**是产品**」之间有一整段距离 |
| ② 固件转储器 + ELF 装载 | `firmware-load-chain` | 差一整条装载链路（现 `prog_*` **只**接 I-TCM ⇒ ⇒ 有 `.data` 的程序**装不进去**） |
| ③ examples 的 RTL 生成 + 独立仿真 + 单测 | `examples-rtl-sim-coverage` | 生成产物形态**零测试** ⇒ ⇒ 退化无人知 |
| ④ 层次化 RTL 示例 | （B1 已落地） | 「能生成」≠「能看结构」 |
| ⑤ 端口名与 C++ 成员名一致 | `port-naming-ratchet` · `port-name-literal-parity` | 现状端口名不可用（`top_unnamed_input_3`） |
| ⑥ 收口 RV32I 重复组件 | `rv32i-dead-component-consolidation` | 现状给的是**虚假信心**（两套实现，只有一套被流水线消费） |
| ⑦ RTL 卫生 | （B1 的 C6 已落地） | 「能生成」≠「产物能过 lint / 能综合」 |

⇒ ⇒ **共同点**：每一条都是「**声称成立但无人可验证**」⇒ ⇒ 所以这条线的统一解法**不是加功能**，
而是**给每个声称配一条可复算的机械判据**（护栏 / 产物级断言 / 变异自证）。

### 1.2 为什么分两批（B0–B4）

`phase-6-rtl-productization.md` 的批次编排**不是按工作量排，是按「交付风险」排**：

· **B0** = 零交付风险（除 F-A 外）⇒ 先把**零消费者**的组件修对 / 退役 ⇒ ⇒ 后面批次的红**不是**它们造成的
· **B1** = 地基（manifest / CLI / 产物护栏）⇒ 下游全部依赖它 ⇒ ⇒ 晚做 ⇒ ⇒ 后面每一批都要重新适配端口面
· **B2** = 消费侧（examples 覆盖）⇒ 只**消费** B1 的产物 ⇒ ⇒ 不改产品语义
· **B3** = 时序契约（D-TCM 寄存读）⇒ 动流水线时序 ⇒ ⇒ golden 会变 ⇒ ⇒ 必须**单独一批**（否则金标算两遍）
· **B4** = 核心重构（组件接入）⇒ 改 ID/EX 级 ⇒ ⇒ **必须在 B3 之后**（同依据）

⇒ ⇒ **硬约束（本轨最容易被违反的一条）**：**B3 → B4 不得乱序**。

### 1.3 B2 的真正价值（不是「覆盖率 +N」）

B2 的名义交付是「10 个 example 的独立 RTL 仿真覆盖」，**实际产出是三个被仿真抓出来的真实实现缺陷**
—— 它们在 C++ 侧的 demo 里**全部是绿的**：

· **D5** 手写 FIFO demo 的 `full` 门限错（31 在途 ⇒ 第 17 次写入起地址混叠）⇒ C++ 侧 demo 只写 2 个元素 ⇒ **永远够不到门限**
· **D6** `uart_rx` 的采样点**硬编码** `div == 4`（不随 `prescale` 缩放）⇒ demo 的 `prescale == 9` ⇒ 硬编码**恰好**等于 `prescale/2`
· **D7** `uart_rx` 解出的字节**位反转**（线上 `0x10` ⇒ 解出 `0x08`）⇒ demo 的测试值 **0x5A 是位回文** ⇒ **恰好掩盖**

⇒ ⇒ **D7 教训可外推**：**测试值必须避开对称值**（位回文 / 字节回文），否则位序缺陷被恰好掩盖
⇒ ⇒ 与仓内「期望值 MUST 手推」是同族纪律：**手推的期望值本身也要能判别**。

---

## 2. 已完成哪些（**实测快照 · 2026-11-12 晚**）

### 2.1 RTL 轨（7 份草案 · B0–B2）

| 批 | 状态 | 证据 |
|---|---|---|
| **B0** | ✅ **6/6** | `67926cf` + `bd21b45` · `.rddf/reviews/b0-batch-verify-2026-11.md`（ctest 257/257 · examples 28/28 · 判别力 4/4） |
| **B1** | ✅ **11/11** | C4/C1/C3/C5/C6/N1/N2/N3/N4/N5(a)/N6/P1 · `.rddf/reviews/b1-batch-verify-2026-11.md`（**复审 APPROVE**） |
| **B2** | ✅ **E1–E8 全部落地** | E3 = **10/10 verified** · E6 = `pr-verilator-fast` 的**一个 step**（不新建 job）· 护栏**严格模式 PASS** |

⇒ ⇒ **B2 的 10 个 TB = 实测 1038 条断言**（登记下界 782 · 语义场景 54 · spec-derived 676 ⇒ ⇒ **四个口径必须分清**）
⇒ ⇒ **每项都做了变异自证**（5 条/项，逐条转红 ⇒ ⇒ 判据有判别力 ⇒ ⇒ 不是恒绿护栏）。

### 2.2 phase-6 轨（P1–P15）

✅ 已闭：**P1 / P2 / P2b / P3 / P4 / P5 / P9 / P10 / P12 / P13 / P14 / P15** ·
⬜ 未闭：**P6**（RV32I ISA 缺口侦察+立项）· **P7**（②⑤ + ④ 闭口）· **P8**（①②④）·
🔒 **P11**（ADR-036 物理移除 · 日历锁最早 **2027-04-17**）。

### 2.3 本轮（复盘 #2 之后）新闭的 4 件

| # | 事项 | 状态 | 证据 |
|---|---|---|---|
| ① | 🔴 **R5 = B3 的 T2 裁决** | ✅ **已闭** | **ADR-069**（**Accepted · 2026-11-12**）· 决策 = **候选 ②（A0）** · oracle **×3**（第 1/2 轮 REJECT ⇒ 第 3 轮 PASS-WITH-NOTES）+ critic **×1**（APPROVE_WITH_CONDITIONS · **0 BLOCKER**）· `.rddf/reviews/ADR-069-mem-data-alignment.md` |
| ② | ✅ **R6 = B3 的公开面前置门** | ✅ **已闭** | `public-api-surface.md`「行为变更」节 + `CHANGELOG.md` 各 1 行（**前置登记 = 预告 + 迁移指引**，非「已发布」） |
| ③ | ✅ **R0 = 解 CI 红** + **R1 / R2 / R10** 状态订正 | ✅ **已闭** | `verify_ledger_evidence.sh` PASS（91 台账 · 6 项待补 = WARN）+ `--self-test` PASS |
| ④ | ✅ **architect 裁决「层次化同名兄弟静默覆盖」** | ✅ **已登记** | 机制**成立且更大**（七处索引表 last-wins ⇒ 实例连错 module + module 不可达 ⇒ **合法 Verilog ⇒ R1 lint 盲区** + 双驱动漏检 ⇒ 零覆盖）⇒ 登记表 **id=22** + 台账 §7 **行 22** · 护栏 PASS（23 行） |

⇒ ⇒ **同批落盘的 ADR 侧动作**：**ADR-068 §5.5 dated addendum**（T2 未裁决句**逐字保留**）·
**ADR-047 §5.5 dated addendum**（本轮新增 · 停顿 3→4 的**结果值**修订 · ⚠️ **语义零变更**
⇒ ⇒ **≠**「实现零改动」：D4 / D11 的**源**按该节 §5.5.2 ②③ **必须重指**（结构变更））。

### 2.4 残差台账（23 行）

**10 已收口 / 4 已裁决 / 4 不做 / 2 已知 / 3 未决** ⇒ ⇒ 护栏 `verify_arch_ledger_residuals.sh` **PASS**（双向 1:1 守恒）
⇒ ⇒ 2 条「已知」= 台账行 **15**（三档 PortPolicy 未分化 · 归属 builder）+ 行 **9**（Verilator 等价性排除项）
⇒ ⇒ 3 条「未决」= 行 **21 / 21b**（route (c) 字面同名 · 触发条件 = 用户在看到 manifest 产物后仍要求人眼可读）
+ 行 **22**（同名兄弟静默覆盖 · 归属 architect · **待独立立项**）。

### 2.5 9 份 RTL 轨草案的**逐份状态**（实测 frontmatter + AC 计数）

> ⚠️ **计数口径 = 「行首 + `<br>` 内联」双计**（仓内已确立 · `rtl-track-correction-list-2026-11.md:152`
> ⇒ 明写「只按行 grep 会漏」）⇒ ⇒ **任何 AC 计数 MUST 双计**。
> ⚠️ **本表是快照** ⇒ ⇒ **计数真相仍以各草案的 `## Acceptance` 段为准** ⇒ ⇒ **MUST NOT 被复制进别处**。

| 草案 | 状态 | AC | 未勾 |
|---|---|---:|---:|
| `rtl-emit-product-tooling` | `implemented` | 20/20 | 0 |
| `examples-rtl-sim-coverage` | `implemented` | 22/22 | 0 |
| `port-name-literal-parity` | `implemented` | 23/25 | **2** |
| `node-name-prefix-consistency` | `implemented` | 13/15 | **2** |
| `port-naming-ratchet` | `implemented` | 16/19 | **3** |
| `rv32i-dead-component-consolidation` | `in-progress` | 15/25 | **10** |
| `firmware-load-chain` | `approved` | 0/18 | **18** |
| `memory-timing-contract` | `draft` | 0/17 | **17** |
| `rv32i-isa-gap-closure` | `draft` | 0/**22** | **22** |

⇒ ⇒ **未勾总数 = 74**（B3 = 35 · B4 = **32** · 独立立项 = 7）。

---

## 3. ★ 本轮**实测发现**（**8 条** · 每条都有可复跑证据）

> 这 8 条是本轮 architect 复盘的**实质产出** —— 不是复述状态，是**实测发现的漂移与缺口**。
> ⚠️ **发现 8 = oracle 第 1 轮 P1-4 新增**（发现 1–7 = 本轮首扫）。
> ⚠️ 与复盘 #2 的 4 条发现的关系：**那 4 条全部已闭**（R0/R1/R2/R10 + R5/R6）⇒ ⇒ **本轮是新一轮的发现**。

### 3.1 发现 1 🔴：**`phase-6.md`（任务状态唯一真相）的 B3 行仍写「T2 尚未裁决」**

`.rddf/roadmap/phases/phase-6.md:75` 的 B3 行 🚪 入口条件写「① ADR-068 已 Accepted ✅ ② **T2 尚未裁决**」⇒ ⇒
而 **ADR-069 已 Accepted（2026-11-12）** ⇒ ⇒ **唯一真相本身错了** ⇒ ⇒ 任何按它决策的读者都会误判 B3 的门。

⇒ ⇒ **这是最严重的一类漂移**：不是「旁注陈旧」，是**真相源陈旧**。

### 3.2 发现 2 🔴：**`phase-6-next-steps.md` 同一文档内两个快照打架**

| 位置 | 说的是什么 |
|---|---|
| `phase-6-next-steps.md:39-40`（§1 总览） | **R5 ✅ 已闭** · **R6 ✅ 已闭** |
| `phase-6-next-steps.md:338-339`（§8.4 B3 开工门） | 门 1 **⬜ 未做** · 门 2 **⬜ 未做** ⇒ ⇒ 「登记未做 ⇒ B3 不得开工」 |

⇒ ⇒ ⚠️ **与复盘 #2 的发现 3（`phase-6.md` B2 行自相矛盾）同型复发** —— 只是这次发生在**同一文档内**
⇒ ⇒ 说明「**同一文档禁止粘两个时点的快照**」这条纪律**尚未被护栏化** ⇒ ⇒ 只能靠人工核对。

### 3.3 发现 3 🔴：**ADR-069 的强制项（ADR-047 dated addendum）此前未落地 ⇒ ⇒ 现状是「Accepted 但条件未闭」**

ADR-069 §6 ③ + critic 的 **SF-3** 把「ADR-047 追加 dated addendum」列为**强制项**（机制同 §6 ②：追加 ≠ 改写 · 原句逐字保留）。
实测（复盘 #2 之后、**本轮 architect 补齐之前**）：**ADR-047 零 addendum** ⇒ ⇒ 而 ADR-069 已在 §3.3 / §4 **声明**
「ADR-047 的停顿拍数契约被修订（3→4）」⇒ ⇒ **两份 Accepted ADR 对同一拍数说法不一致** = ADR-069 §4 自己判为「**持久矛盾**」的形态。

⇒ ⇒ **本轮已补**（§4.1）⇒ ⇒ 状态回到「Accepted 且强制条件已闭」。

### 3.4 发现 4 🔴：**`docs/CHANGELOG.md` 的 R6 登记里仍写「ADR-069（`Draft` · **未决**）」**

`docs/CHANGELOG.md:19` —— 那是**本轮 R6 登记时写下的**⇒ ⇒ **当天就漂** ⇒ ⇒ 公开面载体的**状态描述错误**
⇒ ⇒ 下游读者会以为 T2 还没裁决。
⇒ ⇒ ⚠️ **措辞精确化（oracle 第 1 轮 · P1-6）**：`CHANGELOG.md` 全文**只有 `:19` 一处**提 ADR-069
（`:20` 的「ADR-068（Accepted）」是**另一个** ADR）⇒ ⇒ 矛盾对象是「**该文件 vs 现实**」，
**不是**「同一文件内自相矛盾」⇒ ⇒ 本条严重度按「**载体与现实不符**」计。

### 3.5 发现 5 🟡：**`next-steps.md` §8.3 的「未完成 work item 逐条索引」漏列 2 份草案 ⇒ ⇒ 合计少 4**

| 草案 | 表里写 | 实测 | 差异 |
|---|---:|---:|---|
| `rv32i-isa-gap-closure` | **22** | **22** | ✅ **正确**（行首 18 + `<br>` 内联 4） |
| `port-name-literal-parity` | *（漏列）* | 2 | ⚠️ **漏 2** |
| `node-name-prefix-consistency` | *（漏列）* | 2 | ⚠️ **漏 2** |

⇒ ⇒ **合计 70 vs 真实 74** ⇒ ⇒ **单纯少 4**（**不是**补偿性错误）⇒ ⇒
⚠️ **本条曾被本复盘的第一稿误判为「补偿性错误 / 恰好相等」** ⇒ ⇒ 根因 = 第一稿**只按行首计数**
⇒ ⇒ **已订正** ⇒ ⇒ ⇒ **教训**：仓内 AC 计数口径是「**行 + `<br>` 内联**」双计
（`rtl-track-correction-list-2026-11.md:152`）⇒ ⇒ **任何 AC 计数 MUST 双计**。
⇒ ⇒ ⚠️ **若按第一稿去「修」§8.3 ⇒ ⇒ 会把正确的 22 改成 18 ⇒ ⇒ 且 4 条内联 work item 从索引里永久脱落**
⇒ ⇒ ⇒ **护栏化项（§6 ⑥）MUST 把这条口径写成显式规则**。

### 3.6 发现 6 🟡：**索引状态列与 frontmatter 不一致（2 份）** —— 且 R10 的判据只钉了 frontmatter 一侧

| 草案 | frontmatter | 索引表 | 一致？ |
|---|---|---|:--:|
| `mem-port-arg-cross-layer` | `implemented` | `draft` | ⚠️ |
| `public-api-remove-visibility` | `draft` | `approved` | ⚠️ |

⇒ ⇒ **R10 的判据只查 frontmatter**（`grep -m1 "^状态:"`）⇒ ⇒ **漏了索引列一侧** ⇒ ⇒ R10「已闭」是**半个闭口**。

### 3.7 发现 7 🟡：**两处「下一件事」指针仍指向已完成步骤**

| 位置 | 陈旧内容 | 现状 |
|---|---|---|
| `.rddf/checks/rtl-track-followup-list-2026-11.md` §7.5 | 「**下一件事** = ① 用户裁决 ADR-069 §2.3 … ② critic 轮 … ③ ADR-068 dated addendum」+「⚠️ **R5 仍未闭**」 | ✅ 四件**全部已完成**（裁决 + critic + 两份 addendum + R5 闭） |
| `.rddf/improvements/memory-timing-contract-2026-11.md:8` 进度行 | 「**T2（MEM 数据对齐 A/B）待 architect 裁定**」 | ✅ 已裁决（A0）· 且 ADR-069 已 Accepted |

⇒ ⇒ 与发现 1/4 同型（**指针漂**）⇒ ⇒ 同一批修。

### 3.8 发现 8 🟡：**另有 5 处指针 / 状态格仍指向已完成的步骤**（oracle 第 1 轮 · P1-4 新增）

| # | 位置 | 陈旧内容 | 现状 |
|---|---|---|---|
| ① | `improvement-suggestions.md:17` | `firmware-load-chain` 行的「🚩 入口门 **R5 + R6**」 | 两门**已闭**（2026-11-12） |
| ② | `improvement-suggestions.md:18` | `memory-timing-contract` 行的「**T2 裁决待 architect**」 | 已裁决（A0）· ADR-069 **Accepted** |
| ③ | `.rddf/improvements/firmware-load-chain-2026-11.md:8` | 进度行「⚠️ **前置门 = 公开面登记未做 ⇒ 不得开工**」 | **R6 已闭**（登记已做）⇒ ⇒ 与发现 7 的 `memory-timing-contract:8` **同型** |
| ④ | `.rddf/roadmap/phase-6-next-steps.md:39` | R5 行「⚠️ **B3 开工前仍欠**：① ADR-047 的 dated addendum ② `rv32i-data-forwarding` 的 MODIFIED」 | ① **本轮已补**（§4.1）· ② **被裁决 ② 重分类**为 change 内步骤（不是开工门）⇒ ⇒ **该从句须随 T-a 订正** |
| ⑤ | `.rddf/roadmap/phase-6-next-steps.md:34-44` | R0 / R1 / R2 / R10 四行**无完成标记** | 四项**已闭**（实测：`verify_ledger_evidence` PASS · `grep -c "E6 ⬜ 未做"` = 0 · `grep -c "① ✅ / ② ⬜ / ③ ✅ / ④ 🟡"` = 1 · frontmatter = `implemented`）⇒ ⇒ ⇒ **本复盘 §2.3-③ 与该 R 表自相矛盾** ⇒ ⇒ **发现 2 的同型复发**（同一文档两处状态不一致）⇒ ⇒ ⇒ **并入发现 2 的证据**（不是遗漏） |

⇒ ⇒ ①②③④⑤ **全部归入 T-a**（planner 域 · §4.4 裁决 ④）。

---

## 4. 边界裁决（**architect 域的判断** —— 决定「什么算欠项、什么算门」）

### 4.1 裁决 ①：**ADR-047 的 dated addendum = architect 的「现在欠项」，不是 B3 的门** ⇒ ⇒ **本轮已补**

**理由**：它是 **ADR-069 的 acceptance 条件**（SF-3），而 ADR-069 **已 Accepted** ⇒ ⇒
「已 Accepted 但强制条件未闭」本身就是**半截实现** ⇒ ⇒ 且该 addendum 承载的是**已裁决的事实**
（3→4 是由「EX→WB 级数 2→3」推导出来的**结果值**）⇒ ⇒ **不依赖 B3 的实施** ⇒ ⇒ 现在就能补。

⇒ ⇒ 已落：`docs/adr/ADR-047-rv32i-data-forwarding.md` **§5.5**（三处陈旧陈述**逐条对照** · 原文**逐字保留** ·
⚠️ **语义零变更**（≠ 实现零改动：D4 / D11 的**源**按 §5.5.2 ②③ **必须重指**）·
§5.5.2 登记 ADR-069 的**三个强制结构变更** ⇒ ⇒ 实施方漏任一 ⇒ ⇒ **静默错值** ·
§5.5.3 两份 capability 的落点分派 + **两条「不可误改」的「3 拍」**（附机制依据）·
§5.5.3.1 **活文档清扫 5 行**（根 `AGENTS.md` 是每会话注入面 · 流量最高））。

#### 4.1.1 ⚠️ **已登记的偏离**（两条令字面冲突 · 本轮的实际落地）

| # | 令 | 出处 | 本轮实际 |
|---|---|---|---|
| ① | 「ADR-047 追加 dated addendum ⇒ ⇒ **与 acceptance 同批**」 | ADR-069 §6 ③ | ❌ **未字面满足**（acceptance 在 `ca6e10b` · 本轮在其**之后**） |
| ② | 「ADR-047 的 dated addendum ⇒ ⇒ MUST 与 B3 的 spec MODIFIED **同批**」 | critic **SF-3**（`.rddf/reviews/ADR-069-mem-data-alignment.md`） | ❌ **未字面满足**（MODIFIED 属 B3 change 内 ⇒ 尚未存在） |

⇒ ⇒ ⚠️ **两条令本身冲突**（① 要求与 acceptance 同批 · ② 要求与 MODIFIED 同批 ⇒ ⇒ 二者互斥）
⇒ ⇒ **实际落地 = acceptance 之后单独补** ⇒ ⇒ **两条令字面均未满足** ⇒ ⇒
⇒ ⇒ 但**目的达成**：两份 Accepted ADR 对同一拍数的**持久矛盾窗口已消除**（ADR-069 §4 判定的核心风险）
⇒ ⇒ ⇒ **本偏离登记在此，不得被后续会话「当作未做」而重做一遍**。

### 4.2 裁决 ②：**`rv32i-data-forwarding` 的 MODIFIED = B3 change 内的一步（builder 域），不是开工门**

**理由**：它承载的是**实施后**的真实新基线（`load_use_stalls` **6 → 8**）⇒ ⇒ **只有 change 实施完才有值可写**
⇒ ⇒ 提前写 = 写一个未验证的值 ⇒ ⇒ 与 ADR-069 §5 **V11** 的口径一致（「禁只改实现不改 spec」）。

⇒ ⇒ **B3 的开工门因此只剩「立项」** —— 文档门（R5 / R6）已闭，ADR 侧欠项已补。

### 4.3 裁决 ③：**台账 id=22（同名兄弟静默覆盖）= 独立立项，不并入 B3 / B4**

**理由**：它的写点是 `src/hierarchy_analysis.cpp` + `src/hierarchy_verilog.cpp`（**发射器 / 分析层**）
⇒ ⇒ 与 B3（`include/cpu/**` 时序）/ B4（`include/cpu/**` 组件接入）**写点不重叠** ⇒ ⇒ 塞进去会让
「B3 → B4 不得乱序」这条硬约束的**边界变模糊** ⇒ ⇒ 且它有自己的**判别性探针需求**（构造同名实例的语料目前**零**）
⇒ ⇒ ⇒ 载体 = 新 improvement 草案（走 `add-improve` HARD-GATE）⇒ ⇒ **本轮不并入**。

### 4.4 裁决 ④：**发现 1/2/4/5/6/7 = planner 域**（真相源 / 状态格 / 公开面载体 / 索引 / 指针）⇒ ⇒ **planner 段一次性扫干净**

**理由**：它们全是**台账与索引的载体**，不涉及任何架构决策 ⇒ ⇒ architect **不代劳**（避免双写）。
⇒ ⇒ 但「**同一文档禁止粘两个时点的快照**」这条纪律**须被护栏化**（见 §6 的 R-新项）。

---

## 5. 目前的任务是什么（**按可执行性排序**）

> ⚠️ **本节是快照** ⇒ 状态真相仍以 `phase-6.md` 为准。

| # | 任务 | 归属 | 前置 | 机械判据 |
|---|---|---|---|---|
| **T-a** | 🔴 **12 条漂移一次性扫干净**（发现 1/2/4/5/6/7 + **发现 8 的 5 处**）⇒ ⇒ 真相源 / 状态格 / 公开面载体 / 索引列 / 指针 | planner | — | §3 各条判据逐条可 grep |
| **T-b** | 🔴 **B3 立项**（`memory-timing-contract` + `firmware-load-chain`）⇒ ⇒ 载体 = `openspec/changes/<name>/{proposal,design,tasks}.md` | planner → builder | T-a | 两个 change 目录存在 + 过 `verify_openspec_delta.sh` |
| **T-c** | 🔴 **B3 实施**（T1+T3 同批 · T6 · T2/T4/T5 · L1–L6）⇒ ⇒ 出口 = **ADR-068 的 V1–V10 + ADR-069 的 V1–V15 全绿** + 时序 golden **手推重算** | builder | T-b | 手推期望值 + 变异自证（禁抄仿真输出） |
| **T-d** | 🟡 **R3**（P7 的 ②⑤ + Reject 粒度 → change 立项/归档 → ④ 台账行 15 闭口） | builder → architect | — | 行 15 = `已收口` + 护栏绿 |
| **T-e** | 🟡 **R4**（P8 builder 步：删 `set_ab_verification` stub + golden 覆盖下限 + 两份文案订正） | builder | — | 代码面 `include/ src/` **0** 命中 + 新护栏绿 + 已挂 CI |
| **T-f** | 🟡 **R7 → R11**（P6-0 侦察 ⇒ ⇒ `FENCE` 语义裁决）⇒ ⇒ **planner 无权拍板 ISA 语义** | planner → architect | — | 复用面清单落进 `rv32i-isa-gap-closure` + FENCE 条款进 capability |
| **T-g** | 🟢 **R8**（D8 `ext_clk` 死端口）⇒ ⇒ 建议 **A2**（删端口或明确注释）· **A1 另独立立项** | architect → **用户** | — | 4 处写点同步 + 删前/删后逐拍 trace 等价 |
| **T-h** | 🔴 **B4 实施**（C1 差分 → C2 ALU 编码归一 → C3/C4 组件接入 → G3→G1+G4 同批 → 其余） | builder | **T-c 已闭** | 组件接入后 20 项 golden + 计数锚点重算 |
| **T-i** | 🔒 **R9**（P11 · ADR-036 物理移除） | builder | 日历锁 **2027-04-17** | 日历锁护栏守 |
| **T-j** | 🟡 **独立立项 8 条**（`phase-6-rtl-productization.md` §4）⇒ ⇒ 含 **id=22**（本轮裁决 ③） | planner → architect | — | 每条 MUST 有「对象 / 判据 / 触发条件 / 归属批」 |

⇒ ⇒ **T-a 是唯一「现在就能做且不需要任何前置」的项** ⇒ ⇒ **T-b 是 B3 的真起点**（文档门已闭 ⇒ ⇒ 只差立项）。

### 5.2 architect 的 **T 编号** ↔ planner 的 **R 编号**（一一对应 · 可交叉 grep）

| architect | planner（`.rddf/roadmap/phase-6-next-steps.md`） | 状态 |
|---|---|---|
| **T-a**（12 条漂移扫干净） | **R12** | ✅ **已执行**（2026-11-12 晚 · ⚠️ 完成态 = **待提交**） |
| **T-b**（B3 立项） | **R13** | ⬜ 未开工 |
| **T-c**（B3 实施） | **R14** | ⬜ 未开工 |
| **T-d**（R3 · P7 剩余面） | **R3** | ⬜ 未开工 |
| **T-e**（R4 · P8 删 stub） | **R4** | ⬜ 未开工 |
| **T-f**（R7 → R11 · P6-0 侦察 + FENCE 裁决） | **R7** → **R11** | ⬜ 未开工 |
| **T-g**（R8 · D8 `ext_clk`） | **R8** | ⬜ 待**用户**裁决 |
| **T-h**（B4 实施） | **R15** | ⬜ 未开工 |
| **T-i**（R9 · P11 日历锁） | **R9** | 🔒 日历锁 |
| **T-j**（独立立项 8 条） | **R16**（**10 条** = 批次表 §4 的 9 行 + 台账 id=22） | ⬜ 未开工 |
| —（**planner 追加** · metis P2-15） | **R17**（**用户 ⑤ 的端口名消化** ⇒ ⇒ **触发条件已成立** ⇒ ⇒ 需用户二选一） | ⬜ 待**用户**裁决 |

⇒ ⇒ ⚠️ **R17 是 architect 复盘 #3 之后由 metis 追加的** ⇒ ⇒ **用户 ⑤ 的原始诉求
在复盘 #3 的 T 清单里没有位置** ⇒ ⇒ 本轮补上 ⇒ ⇒ **它不是「等用户再问一次」**（触发条件**早已成立**）。

### 5.1 为什么这个顺序（**不可乱序的三条**）

| # | 约束 | 后果 |
|---|---|---|
| ① | **B3 → B4 不得乱序** | 时序金标只算一遍 ⇒ ⇒ 反序 ⇒ 「178 条退休 / 6 次 load-use 停顿」**两次作废** |
| ② | **T-a → T-b** | 真相源与状态格**先**对齐 ⇒ ⇒ 否则 change 立项时会把「门未过」的形态写进 proposal |
| ③ | **T-c → T-h** | 同 ①（时序金标）· 且 B4 改 ID/EX 级 ⇒ ⇒ 组件接入的差分基线必须是 **A0 之后**的 golden |

⇒ ⇒ ⚠️ **T-d / T-e / T-f / T-g 与 T-b / T-c 写点不重叠** ⇒ ⇒ **可并行**（T-d 动 `src/hierarchy_*` · T-e 动 `include/simulator.h` + `src/simulator.cpp` · T-c/T-h 动 `include/cpu/**`）⇒ ⇒ **但收口阶段不并行**（共享 5 个单点资源，见 `phase-6-next-steps.md` §1.1）。

---

## 6. 后续要怎么做（**为什么**）—— 落点

| # | 事项 | 归属 | 载体 | 为什么 |
|---|---|---|---|---|
| ① | **12 条漂移扫干净**（发现 1/2/4/5/6/7 + 8）⇒ ⇒ ⚠️ **AC 计数 MUST 用「行 + `<br>` 内联」双计** ⇒ ⇒ **禁**按行首单计 | planner | `phase-6.md` · `phase-6-next-steps.md` · `CHANGELOG.md` · `improvement-suggestions.md` · 3 份草案的指针 | 真相源 + 状态格 + 公开面载体必须与实际一致 ⇒ ⇒ 否则下一轮**再次**按陈旧状态决策 |
| ② | **B3 两个 change 立项** | planner → builder | `openspec/changes/{memory-timing-contract,firmware-load-chain}-2026-11/` | ADR 已裁 · 公开面已登记 ⇒ ⇒ 文档门齐了 ⇒ ⇒ 只差立项 |
| ③ | **B3 实施** | builder | 两个 change + ADR-068/069 的 V1–V10 / V1–V15 | 交付「存储器能上板」+「能装真程序」⇒ ⇒ 用户诉求 ② + ⑦ |
| ④ | **B4 实施** | builder | 两个 change（C1–C4 + G1–G5） | 交付「RV32I 重复实现收口」+「ISA 缺口显式化」⇒ ⇒ 用户诉求 ⑥ + ⑨ |
| ⑤ | **端口名消化**（用户 ⑤ 的原始诉求） | architect → **用户** | **R17**（`port-naming-ratchet` §4「N2 补遗」32 端口） | ⚠️ **用户原始诉求仍未在真实 CPU 上兑现**（`Rv32iPipelineTop` 的端口**全部未具名**）⇒ ⇒ 棘轮基线 = 1484/1484 ⇒ ⇒ **零组件被消费** ⇒ ⇒ ⚠️ **触发条件早已成立**（用户 2026-11-10 就提了 ⑤）⇒ ⇒ **不是**「等用户再问一次」⇒ ⇒ **需用户二选一**（「现在做」vs「推到 phase-7」） |
| ⑥ | **「同一文档禁止双快照」护栏化** | planner → builder | 新护栏（类比 `verify_roadmap_fragments.sh` 的 vendored 渲染） | 发现 1/2/4/5/6/7 **全部**是这一类 ⇒ ⇒ 目前只靠人工 ⇒ ⇒ 复发概率高 |
| ⑦ | **id=22 独立立项** | planner → architect | 新 improvement 草案（`add-improve` HARD-GATE） | 裁决 ③ |

---

## 7. 判据自检（**可复跑**）

```bash
cd /workspace/project/CppHDL
bash scripts/verify_arch_ledger_residuals.sh          # 双向 1:1（23 行）⇒ PASS
bash scripts/verify_ledger_evidence.sh                # 91 台账 ⇒ PASS
bash scripts/verify_public_api_visibility.sh          # 公开面 ⇒ PASS
bash scripts/verify_roadmap_fragments.sh              # AUTO 块守恒 ⇒ PASS
bash scripts/verify_test_counts.sh && bash scripts/update_test_count_anchors.sh --check
(cd build && ctest -N | tail -1)                     # Total Tests: 263
grep -m1 "^- \*\*Status\*\*" docs/adr/ADR-069-mem-data-alignment.md    # ⇒ Accepted
grep -c "本 ADR 不裁决 T2" docs/adr/ADR-068-dtcm-memory-timing-contract.md   # ⇒ 2（原文 + 引用）
grep -c "3 拍" docs/adr/ADR-047-rv32i-data-forwarding.md               # ⇒ 原文 3 处 + addendum 引用
```

---

## 8. 与既有文档的关系

| 文档 | 关系 |
|---|---|
| `2026-11-12-phase-6-b2-midterm-review.md`（复盘 #2） | **上游快照**（B2 中期时点）⇒ ⇒ 本文件**取代**它作为「当前态」⇒ ⇒ 但**不删**（保留为历史） |
| `2026-11-09-phase-6-midterm-review.md`（复盘 #1） | 历史快照 |
| `2026-11-02-hierarchical-rtl-output.md`（层次化主题文档） | **§7 残差台账**是本轨的登记面（23 行）⇒ ⇒ 本文件**不复制**台账状态格 |
| `phase-6-rtl-productization.md`（批次编排） | **不新增架构义务** ⇒ ⇒ 本文件只给边界与顺序 |
| `phase-6-next-steps.md`（planner 域） | **下游** = 任务内容 + 实施路径的权威载体（**R0–R17** · 本轮已刷新：§1 R 表 + §2 实施路径 + §6 判据自检 · **metis ×1 的逐条处置 = §7.0** · 未处置的 2 条 = **§7.0.1 backlog**） |
| `.rddf/reviews/phase-6-rtl-track-review-2026-11.md` | **本文件的审稿记录**（oracle ×1 + critic ×1 + metis ×1 的逐条处置） |
| `docs/adr/ADR-069-mem-data-alignment.md` | **T2 裁决**（Accepted）· 本文件是其上游复盘 |
| `docs/adr/ADR-047` §5.5 · `docs/adr/ADR-068` §5.5 | 两份 **dated addendum**（追加 ≠ 改写）⇒ ⇒ 消除跨 ADR 的**持久矛盾** |
