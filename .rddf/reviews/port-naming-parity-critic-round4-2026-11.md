# critic 第 4 轮审查结论（2026-11-11 · 端口命名 parity 批）

> ⚠️ **本文件是 critic 第 4 轮的输出誊写**（审查者只读 ⇒ ⇒ 由 planner 代为落盘）
> ⇒ ⇒ **用途 = 第 5 轮复审的仓内基准**（「上轮 BLOCKER 闭合」的逐条依据行）
> ⇒ ⇒ **不是** planner 的自检，**不是** architect 的自检。
> ⚠️ **scope**：用户明确批准（critic 预算闸门第 3 次打开）⇒ ⇒ 书面 scope 限定为
> 「① 上轮 7 条 BLOCKER 是否闭合 ② 本轮新 diff 是否引入新 BLOCKER」。

**审查者**：critic · **轮次**：4 · **profile**：`plan, arch`
**结论**：`REJECT` · `mergeable: no` · `scope_exceeded: true`

---

## 1. 上轮 7 条的闭合判定

| # | 上轮项 | 判定 |
|---|---|---|
| 1B-1 | R1-a 常态违约 MUST（clock/reset 无 `cpp_member`） | **闭合（就 clock/reset 而言），但修复引入新矛盾** |
| 1B-2 | 「空名哨兵语义 MUST 明写」两草案冲突 | **闭合**（`grep` 剩余命中全是事实性描述或姊妹禁令 ⇒ 无条款级残留） |
| 1B-3 | delta 形态三方不一致 | **闭合**（三份 Capabilities 均 `ADDED`，主 spec 无同名 Requirement） |
| 1B-4 | 同一义务三处落点且文本不等价 | **未闭合**（⇒ ⇒ 见 §2 BLOCKER-B） |
| 1B-5 | B0 ⑧ 修 SLTU ⇒ B4 的 C1 门槛不可满足 | **闭合（主判据）**，两处残留副本口径未同步（⇒ ⇒ SHOULD_FIX-1） |

---

## 2. 🔴 BLOCKER（2 条）

### BLOCKER-A —— R1-a 内部自相矛盾：同一端口类同时「MUST 提供」与「MUST NOT 被要求提供」`cpp_member`

- `decision-request:66`：「**其余端口（`role` 不为 `clock` / `reset`）MUST 全部提供 `cpp_member`**」
- `decision-request:82`：「（未显式命名者）**MUST NOT 被要求提供 `cpp_member`**」

⇒ ⇒ 一个**未显式命名 + `role = data`** 的端口（= 存量 3103 处端口的主体）**同时满足两条的 WHEN**
⇒ ⇒ 一条要求提供、一条禁止被要求提供 ⇒ ⇒ **同一决策自相矛盾**。

**focus #2 问的「缝隙」确实存在**：clock/reset 端口同时落入 Scenario 2（`:61`）与 Scenario 3（`:79`）⇒ ⇒ `:81` 要求它们「MUST 被登记在**命名棘轮的豁免表**」⇒ ⇒ 而棘轮表按 `port-naming-ratchet:66`（N2③）只登记 `__io` 声明（扫描面 = `include/**` + `examples/**`）⇒ ⇒ **ctx 级 clock/reset 不可登记** ⇒ ⇒ 该 MUST 对这一类**无解**。

**最小修法**：在表内补判定**顺序/优先级**（先 A；A 不成立再判 B；B 不成立为 C），或把 C 的判定写成「`role` 非 clock/reset 且无显式 C++ 成员名」；并把 clock/reset 从登记义务里显式排除。

### BLOCKER-B —— 条款正文副本仍在 3 处，且每处都自带「已移出」的**失实声明**

- `port-name-literal-parity:86`：**同一单元格内**先写「本项的**条款正文已移出** ⇒ ⇒ 改为**指针**」，**紧接着**仍是逐字条款（`• (MUST · 集合 = T1) …` + `• (SHOULD · 集合 = T2) …` + `• (MUST NOT) …`）⇒ ⇒ 同一格内自相矛盾
- `port-name-literal-parity:216`：以「追加「端口名 MUST 与 C++ 成员名**机器可解析对应**…」开头，**同一格**声明「本行**条款正文已移出**」⇒ ⇒ 失实；且注释被**插进引文中间**，引文已不成句
- `port-naming-ratchet:194`：同一形态

**实质危害（不止文字）**：这三份副本与 R1-a **不等价** —— 均**缺** clock/reset 豁免与「未显式命名者」豁免 ⇒ ⇒ 实施方若从 Capabilities 表取材（Capabilities 表正是 delta 的取材处），1B-1 的常态违约 MUST 会原样进 delta ⇒ ⇒ 「**唯一落地文本 = R1-a**」在仓内**不成立**。

---

## 3. SHOULD_FIX（7 条）

| # | 项 |
|---|---|
| 1 | `rv32i-dead-component-consolidation:67` + `phase-6-rtl-productization:114` —— 两处 C1 描述仍写「差异清单 MUST 至少含 **SLTU 借位**」，缺新口径 |
| 2 | `phase-6:44-45` —— 修复说明写「⑥ ratchet `:224`→`:225`」，但表内实际值是 `:227`，且 `:225` 是表头行 ⇒ ⇒ 「已**逐条实测**后改正」的声明与自身不符 |
| 3 | `decision-request:81` vs `port-naming-ratchet:66`（N2①）—— Scenario 3 的「MUST 被登记在棘轮豁免表」与棘轮扫描面（`tests/**` 排除）未对齐 |
| 4 | `phase-6:86` —— R1-a 的 Scenario 3 依赖棘轮豁免表先存在（N2 在 B1 `:84`）⇒ ⇒ B1 内 **N2 → N4** 的顺序未写 |
| 5 | `decision-request:74-75` vs `:48`/`:55` —— 「归属空洞」类在 delta 正文里没有对应限定 |
| 6 | `ratchet:194` + `sister:216` + `sister:88` + `phase-6:86` —— **两个 change 都声称承载同一条 Requirement** ⇒ ⇒ 归档后主 spec 会出现两条同名 Requirement |
| 7 | `ratchet:69`（N5 第 2 步）仍留**已被证伪**的回退条件 ⇒ ⇒ 与本轮改写的 `phase-6:148` 内容不一致 |

---

## 4. NITS（不计入 BLOCKERS）

- `ratchet:68` 仍引「RTL 端口名 MUST 可机器解析…」原文，但它位于标注为「✅ 2026-11-10 · oracle 建议 1 裁决」的**历史决策记录**内 ⇒ ⇒ **边界规则**：有该类标注 = 留档（不算副本），无标注 = 副本
- `ratchet:162`（AC）复述判据 ⇒ ⇒ 按「AC 只保留**可判定的期望值**且集合 ⊂ R1-a 义务集」的边界，**不算另一份副本**
- `sister:146` 的「R1-a 已含 ①②③④」= **目录式索引**，非条款正文 ⇒ ⇒ 不算副本
- `decision-request:202` §7 ③ 与 `phase-6:180` ⑧ 的状态格仍停留在第 3 轮时点（critic 第 3 轮 / 待用户批准）⇒ ⇒ 陈旧

---

## 5. 修复轮次登记

| 轮次 | 时间 | 结论 | 处置 |
|---|---|---|---|
| 1 | 2026-11-09 | REJECT（5 条 BLOCKER） | 全部修复 |
| 2 | 2026-11-10 | REJECT（2 条新增 BLOCKER） | 全部修复 + 新增 2 份草案 + 批次表 + decision-request |
| 3 | 2026-11-10 | REJECT（上轮 7/7 闭合 · 新引入 5 条 BLOCKER） | 全部闭合 ⇒ 记录 = `port-naming-parity-critic-round3-2026-11.md` |
| **4** | **2026-11-11** | **REJECT（1B-4 未闭合 + 新引入 2 条 BLOCKER）** | **全部闭合 ⇒ 记录 = 本文件** |
| **5** | **2026-11-11** | **REJECT（2 条 BLOCKER + 7 条 SHOULD_FIX）** | **全部闭合 ⇒ 记录 = `port-naming-parity-critic-round5-2026-11.md`** |

⇒ ⇒ **第 5 轮的书面 scope** = 「① 上轮 2 条 BLOCKER + 7 条 SHOULD_FIX 是否闭合 ② 第 5 轮新 diff 是否引入新 BLOCKER」。
