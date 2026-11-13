---
id: ADR-069-mem-data-alignment
日期: 2026-11-12
对象: docs/adr/ADR-069-mem-data-alignment.md
状态: ✅ **已闭**（oracle ×3：第 1/2 轮 REJECT ⇒ **第 3 轮 PASS-WITH-NOTES** · critic ×1：**APPROVE_WITH_CONDITIONS**（0 BLOCKER））⇒ ⇒ ADR 已转 **Accepted**
审稿链: oracle ×3（先）⇒ ⇒ critic ×1（后）⇒ ⇒ 合计 **4 轮**
---

# 审稿记录 · ADR-069（RV32I 访存数据对齐契约 · T2 裁决）

> ⚠️ **本 ADR 未闭** ⇒ ⇒ `Status` MUST 保持 `Draft` ⇒ ⇒ 「把未决问题写成已决」属
> `arch` 档 BLOCKER 第四类 ⇒ ⇒ 未审先 Accepted = 违规。
> ⚠️ **决策权在用户**：oracle 第 2 轮推翻了「第 2 版的 A0 严格支配 A1」⇒ ⇒
> ⇒ ⇒ **两个候选是真权衡** ⇒ ⇒ §2.3 已改为**两候选并列 + 裁决判据** ⇒ ⇒ 待用户裁决。

---

## 1. 审查链

| 轮 | 审查者 | 结论 | 处置 |
|---:|---|---|---|
| 1 | **oracle** | 🔴 **REJECT** · 8 条（**4 条必改** + 5 条其它）· 核心 = 「拍号模型错」+「选项表漏项」+「排除论证稻草人化」 | **8 条全部采纳** ⇒ ⇒ v2 |
| 2 | **oracle** | 🔴 **REJECT** · **2 BLOCKER** + 3 MEDIUM · 核心 = 「A0 的**三个强制结构变更**被系统性低估」+「**A0 严格支配 A1**的推理坍塌」 | **2 BLOCKER + 3 MEDIUM 全部采纳** ⇒ ⇒ v3（§2.3 改**待用户裁决**） |
| 3 | **oracle（闭合验证）** | ✅ **PASS-WITH-NOTES** ⇒ ⇒ **2 条 BLOCKER 实质闭合** · 3 条补字 | **3 条补字全部落地**（v3.1）：① **A0-① 字段清单补 `link_value`**（M2 MUST 同拍 transit ⇒ 否则 JAL/JALR 写回后继值 · 回捕 = V9 的 3 条 jump golden）② §2.1 的「流水线内部零 churn」改为「**停顿链**零修订 + **写接口**使能源 MUST 重指」③ §2.3 理由② **降级**（flush 抖动**功能无害** ⇒ 非 🔴 原则级）+ §2.2 补「写接口全体拍前移」 |
| 1 | **critic** | ✅ **APPROVE_WITH_CONDITIONS** · `mergeable: yes` · `scope_exceeded: false` ⇒ **0 BLOCKER** · **4 SHOULD_FIX** + 3 NITS | **4 SHOULD_FIX + 3 NITS 全部采纳**（v3.2）：SF-1 删 §3.3 的**重复表** · SF-2 AC 映射 `V1–V10` → **`V1–V15`** · SF-3 §6 ③ 的 ADR-047 addendum 由「视需要」改为**强制项**（与 acceptance 同批）· SF-4 §6 ② 的 ADR-068 addendum 改为 **acceptance 同期**（把「无桥窗口」压到零）· NIT-1 审稿轮次统一为 **oracle ×3 + critic ×1** · NIT-2 删 V10 的**幽灵数字**（−3~−8%）· NIT-3 V3 补两个限定词（**load** 的 / **同址**） |

> ⚠️ **critic 对 focus 的判定（0 BLOCKER 的理由）**：
> ① **不构成 BLOCKER 第一类**（与已签批 ADR 冲突未走流程）：ADR-069 的 header Parent 行 + §4 表 + §3.3 依据 + §6 ③ 载体**四处**显式声明「修订 ADR-047 的停顿拍数」⇒ ⇒ **已走修订流程**；且修订由**前提变更**（`aread`→`sread` 使 EX→WB 级数 2→3）驱动 ⇒ ⇒ 不是否定 ADR-047 的决策本身。
> ② **不构成 BLOCKER 第三类**（文档内自相矛盾）：「本 ADR 不裁决 T2」是**范围弃权句** ⇒ ⇒ ADR-069 填充的是它**显式预留**的格子 ⇒ ⇒ 是「弃权 → 后续裁决」的**时间序列**，不是同一决策的两个答案 ⇒ ⇒ **dated addendum 足以消解**（对「弃权」做 supersede 反而别扭）。
> ③ **判据不恒绿/恒红**：V13–V15 未写用例时 = 「**未验收**」（正确中间态）；V9 的 STALE 定义（**相对基线**的**同身份条目**数据不同 + 按退休序对齐）**不会**把正确实现判红（A0 下地址呈现拍不变 ⇒ 数据值逐字相同 ⇒ 全落 `SHIFT`）。

---

## 2. oracle 第 1 轮（REJECT · 8 条）的落点

| # | finding | 落点 |
|---:|---|---|
| ① | **拍号模型错**（「load 写回沿 T+2 → T+4」⇒ ⇒「停顿 3 拍不变」错） | v2 新增 **§2.1 拍级推导**，以「**EX→WB 级数**」为决定量（依据 `include/cpu/pipeline/hazard_unit.h:135-138` 的自陈「load 从 EX 到 WB 是 2 级 ⇒ 检测拍+3」） |
| ② | **选项表漏项**（漏了「地址照旧 + MEM/WB 延后」的形态） | v2 补 **A0**；并**删除 A2**（它把地址**延后** ⇒ ⇒ **不是**「保持旧时序」⇒ ⇒ 每轴不优于 A0） |
| ③ | **B 的排除理由是稻草人** | v2/v3 如实写：冻结气泡**不进入** `stall_hold_d1/d2` 链（该链只认 EX 级检测）⇒ ⇒ 消费者放行拍**不随**冻结延后 ⇒ ⇒ **提前放行 ⇒ 旧值** ⇒ ⇒ 必须握手化 ⇒ ⇒ 修订 ADR-047 |
| ④ | **`bf78367` 类比不成立** | v2 **删除**该类比；A1+旁路 降为「次选」；v3 进一步改为「**真权衡**」 |
| ⑤ | **ADR-068 / 069 的持久矛盾** | v2 §4 改为 **dated addendum** 机制（追加一段，原承重句**逐字保留**）⇒ ⇒ §4 表已登记 |
| ⑥ | **V3 机械化** | v2/v3 §5 V3 改为**拍级**判据 + 变异自证；V13–V15 补三条 distance 判别判据 |
| ⑦ | **实施陷阱**（地址级裂分必须搬移消费地址的组合逻辑） | v2/v3 §5 **V5 分项**：store 侧两项**不变** / load 侧车道选择**搬进 M2 时序**；§3.3 A0-① 钉死 M2 = **完整元数据寄存器** |
| ⑧ | **V8 归因纪律** | v2/v3 §5 **V9**：标签 `SHIFT` / `STALE` / `OTHER` + **对齐规则**（按退休序/rd 身份，**禁**按拍号 zip）+ STALE 的操作定义 |

---

## 3. oracle 第 2 轮（REJECT · 2 BLOCKER + 3 MEDIUM）的落点

| # | finding | 落点 |
|---:|---|---|
| **B-1** | 🔴 **A0 的真实代价被系统性低估**：「MEM/WB 整组延后 1 拍」**不只**影响 load-use 链 ⇒ ⇒ 它破坏了 ADR-047 的**前推 / 旁路观测窗**校准 ⇒ ⇒ 强制三个结构变更：① **M2 = 完整元数据寄存器**（地址/funct3/rd/is_*/valid）② **档位 2 的数据源由 `memwb_*` 改指 M2** ③ **D11 旁路源改指「下一提交者值」**（`wb_stage` 写回 mux 前移一级）⇒ ⇒ 且 v2 的「改动面窄」叙述**不成立** | v3 **§3.3 新增「候选 ②（A0）的强制结构变更」表**（A0-①/②/③，各带依据）⇒ ⇒ **§2.2 候选 ② 的「流水线内部 churn」行改写** ⇒ ⇒ §5 新增 **V13 / V14 / V15** 三条 distance 判别判据（各配变异自证）⇒ ⇒ §5 注明「现有固件覆盖**不明** ⇒ ⇒ **禁**靠现有 golden 兜底」 |
| **B-2** | 🔴 **「A0 严格支配 A1」的推理基础坍塌**：A1 的 MEM/WB 装载点可「**照旧**」（地址提前 ⇒ 数据 EX+1 拍可用）⇒ ⇒ **A1 的 EX→WB = 2 ⇒ ⇒ 停顿保持 3 拍** ⇒ ⇒ ADR-047 **零修订** ⇒ ⇒ ⇒ **A1+旁路 = 停顿 3 + 内部零 churn** ⇒ ⇒ ⇒ **真权衡**，非支配 | v3 **§2.1 表 A1 / A1+旁路两行的拍号修正**（EX→WB = 2 · 停顿 = 3）⇒ ⇒ **§2.1 关键观察 ②** 显式记「v2 的支配论证被推翻」⇒ ⇒ **§2.2 表** 候选 ①/② 并列 ⇒ ⇒ **§2.4-④** 把该条列为「**已排除项的理由**」里的**被推翻条目**（留痕）⇒ ⇒ **§2.3 改为「⛔ 待用户裁决」+ 4 条裁决判据** |
| **M-1** | **V9 缺对齐规则**（停顿 6→8 后序列**非均匀平移** ⇒ 朴素 zip 必假阳） | v3 §5 **V9** 补「按退休序 / rd 序列身份对齐，禁按拍号 zip」+ STALE 的操作定义 |
| **M-2** | **§3.2-① 的 −3~−8% 无源**（仓内无 load 密度统计） | v3 §3.2-① **删除该区间** ⇒ ⇒ 只保留「**公式**（load 密度 × 1 拍）」+ V10 的实测回填义务 |
| **M-3** | **M2 内容未钉死**（按「纯数据寄存器」理解 ⇒ ⇒ 车道选择的地址输入在数据消费拍是**后继地址** ⇒ ⇒ `SB`/`SH`/`LB` 静默错车道） | v3 **§3.3 A0-①** 钉死 M2 = 完整元数据寄存器（地址 + funct3 + rd + is_* + valid）· 数据**不驻留** M2 · 组合流入 MEM/WB′ ⇒ ⇒ §5 **V5** 分 store/load 两项 |

---

## 4. v3 之后的**状态**

| # | 事项 | 状态 |
|---|---|---|
| ① | oracle 两轮 findings | ✅ **全部采纳并并入**（v3） |
| ② | **决策本身** | ✅ **已裁决 = 候选 ②（A0）**（用户 2026-11-12「按你建议拍板」）⇒ ⇒ ADR 已转 **Accepted** |
| ③ | **critic 轮** | ✅ **APPROVE_WITH_CONDITIONS**（0 BLOCKER）⇒ ⇒ 本 record 的 `状态` 字段据此填 `已闭` |
| ④ | **ADR-068 的 dated addendum** | ✅ **已落盘（acceptance 同批 · SF-4）** ⇒ ⇒ `docs/adr/ADR-068-dtcm-memory-timing-contract.md` **§5.5**（dated addendum）⇒ ⇒ 原承重句**逐字保留**（验证：`grep -c` 命中 2 = 原文 + 引用） |
| ⑤' | **ADR-047 的 dated addendum** | ⬜ **未做**（SF-3 改为**强制项**）⇒ ⇒ 依据 = ADR-047 有 **3 处规范性「3 拍」陈述**（`:38` / `:191` / `:473`）⇒ ⇒ Accepted 后全部陈旧 ⇒ ⇒ MUST 与 B3 的 spec MODIFIED 同批 |
| ⑤ | `memory-timing-contract` 的**立项** | ⬜ 未立项 ⇒ ⇒ 本 ADR 的「关联 change」仍是 `backlog:` |

---

## 5. 复核命令

```bash
# 本 ADR 的 HARD-GATE 不适用（它不是 improvement 草案）⇒ ⇒ 复核对象 = 下列三项
bash scripts/verify_public_api_visibility.sh          # R6 登记的护栏
bash scripts/verify_arch_ledger_residuals.sh          # 台账 / 登记表双向 1:1
bash scripts/verify_roadmap_fragments.sh              # AUTO 块守恒
# ADR 自身的形状自检：§2.3 的决策必须显式标注「待裁决」⇒ Status 不得为 Accepted
grep -n "^- \*\*Status\*\*" docs/adr/ADR-069-mem-data-alignment.md
```

---

## 6. ⚠️ **追加节（2026-11-13）** —— §7 dated addendum 的审稿记录

> ⚠️ **本节是「追加」不是「改写」** ⇒ ⇒ 上文 §1–§5（`oracle ×3` + `critic ×1` · 原始 Accepted 审稿轨迹）**逐字保留**
> ⇒ ⇒ 本节记录的是 **§7 addendum** 的**独立审稿轨迹**。

### 6.1 审稿轨迹

| 轮 | 角色 | 结论 | 要点 |
|---|---|---|---|
| **第 4 轮** | oracle | ✅ **确认 + 纠正** | ✅ 独立重推拍级表 ⇒ 与 §2.1 一致（实施期推导无误）<br>✅ 确认「capability 距离矩阵右移一位」⇒ ⇒ `rv32i-data-forwarding` 的 MODIFIED delta 是**必须项**<br>🔴 **纠正**：「D11 源改指 M2」按字面 = **替换语义** ⇒ ⇒ 距离 4 生产者 + 4 拍停顿后的 load-use 消费者**静默读旧值**<br>⇒ ⇒ 正确形态 = **两级优先旁路**（M2 档优先 + WB 档保留）⇒ ⇒ `wb_stage` 写回 mux **MUST NOT 前移** |
| **第 1 轮** | critic（`arch` 档） | ❌ **REJECT** | **BLOCKER-1** = `ADR-047:629`（§5.5.2 ③「mux 前移一级」）⇄ 本 ADR §7.1.2「MUST NOT 前移」= **两份 Accepted ADR 的相反处方**，且 ADR-047 侧**无反向指针**<br>SHOULD_FIX ① = §7.1.3 的矩阵表构成「第二份权威矩阵」<br>SHOULD_FIX ② = §7.5 的 critic 记录指向写死 |
| **第 2 轮** | critic（复审 · `继承第 1 轮：arch`） | ✅ **APPROVE** | `mergeable: yes` · 0 BLOCKER · 0 新 SHOULD_FIX · 1 NIT（措辞精度 · 不阻断）⇒ ⇒ **三项闭合证据经逐一读文件核实** |

### 6.2 BLOCKER-1 的闭法（三处）

| # | 落点 | 内容 |
|---|---|---|
| ① | `ADR-047` **新增 §5.5.2.1** | dated note（**表 ③ 行逐字保留**）· 两级结构表（M2 档优先 / WB 档保留 / youngest-wins / `wb_stage` mux **MUST NOT 前移**）· 「WB 档是**承重件**，不是遗留件」的推导 · 指向本 ADR §7.1 |
| ② | 本 ADR **§6 ③** | 范围句扩大（登记 A0-③ 的实施形态订正）+「**已闭**：ADR-047 §5.5.2.1」⇒ ⇒ 义务有**唯一落点** |
| ③ | 本 ADR **§7.5** | 登记 critic 第 1 轮的 BLOCKER + 闭法 + 第 2 轮 APPROVE 结论 |

### 6.3 SHOULD_FIX 的处置

| # | 落点 | 改法 |
|---|---|---|
| ① | 本 ADR **§7.1.3** | 表前加「⚠️ **下表 = MODIFIED delta 的**目标内容**（**不是**第二份权威矩阵）⇒ ⇒ 权威仍是 capability `rv32i-data-forwarding` 本身 ⇒ ⇒ **delta 落地后以 capability 文本为准**」 |
| ② | 本 ADR **§7.5** | critic 记录指向改为「复审结论见本文件（本节追加）」⇒ ⇒ 闭环后回填实际结论 |

### 6.4 §7 addendum 生效后的**下游义务**

| # | 义务 | 归属 |
|---|---|---|
| ① | 🔴 **capability `rv32i-data-forwarding` 的 MODIFIED delta**（6 处：矩阵表 / 「一一对应」句 / 档位 2 的源 / 距离 2 的 NOTE / 距离 ≥ 4 的 Scenario / 寄存器堆读点旁路 Scenario）⇒ ⇒ 载体 = change `memory-timing-contract-2026-11` 的 `specs/` | planner（起草）· builder（实施） |
| ② | **§7.4 的 V14a–V16**（含「删 WB 档必红」变异自证）= B3 实施期的验收门 | builder · verifier |
| ③ | **§7.2 的 A0-① 措辞订正**（M2 = 元数据 + **原始字** · 9 个成员）⇒ ⇒ 车道选择留在 M2 下游 | builder |
| ④ | **§7.3 的单地址口零冲突**（T 拍地址 = `exmem_alu_result` ⇒ `sread` 在 T 拍发出 ⇒ T+1 拍 `rdata` = data(A_i)）⇒ ⇒ **不新增端口** ⇒ ⇒ store 侧车道移位 / `strbe` **留在 T 拍** | builder |

### 6.5 复跑命令（只读）

```bash
cd /workspace/project/CppHDL
grep -rn "前移一级" docs/adr/ | cut -c1-140   # ⇒ ADR-047:629（原文逐字）+ :631/:636/:638（订正）+ ADR-069:246/:302/:362
grep -n "5.5.2.1" docs/adr/ADR-047-rv32i-data-forwarding.md
grep -n "critic 第 2 轮" docs/adr/ADR-069-mem-data-alignment.md
```
