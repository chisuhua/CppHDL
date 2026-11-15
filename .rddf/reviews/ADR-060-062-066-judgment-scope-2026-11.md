# ADR-060 / ADR-062 / ADR-066 —— **判据正文收窄与适用面界定**（审稿记录）

> **日期**：2026-11-15 · **角色**：architect（build agent）
> **对象**：`docs/adr/ADR-060-hierarchical-port-policy-and-module-reuse.md`（**v1.19 + v1.20**）·
> `docs/adr/ADR-062-hierarchical-unit-interface-typing.md`（**v1.7**）·
> `docs/adr/ADR-066-hierarchical-parent-side-landing-semantics.md`（**v1.6**）
> + capability `rtl-hierarchical-output` 的 **MODIFIED delta**（已归档
> `openspec/changes/archive/2026-11-15-hier-top-port-table-modification-2026-11`）
> + 测试的根段切法（`examples/riscv-mini/tests/test_rv32i_hier_rtl.cpp`）
> **来源**：change `2026-11-15-rv32i-hierarchical-example-2026-11`（**已归档**）交回的实测订正项
> ⇒ ⇒ **本轮只做「追认 / 收窄 / 界定」** ⇒ ⇒ **零反转**（不推翻任何既有裁决）。

---

## 1. 为什么需要这一轮（触发链）

| 步骤 | 事实 |
|---|---|
| ① | change `2026-11-15-rv32i-hierarchical-example-2026-11` 实施期实测出**三处契约口径**与实测不符（顶层端口表「逐字逐序」只对**根段**成立 · 「子 → 父」行被误当双驱动 · 分析层旧行未登记致 `Duplicate pin connection`）⇒ ⇒ change **只交回事实、不改 ADR 正文** |
| ② | architect 起草三处**收窄 / 界定**（ADR-062 顶层端口表判据 · ADR-060 §2.1.1 反向不变式 · ADR-066 决策⑤ 适用面） |
| ③ | **oracle 第 1 轮**（对抗式）⇒ **PASS-WITH-NOTES** + **6 条 finding**（F1–F6）⇒ ⇒ 全部采纳并修完（F1 = spec 侧**活的矛盾**，最重） |
| ④ | **critic 第 1 轮**（冲突 / 取舍）⇒ **REJECT**（`mergeable: no`）⇒ **1 BLOCKER**（ADR-060 **两处**未收窄的旧判据）+ **3 SHOULD_FIX** |
| ⑤ | 修完 ⇒ **critic 第 2 轮（闭合复审）**⇒ **APPROVE**（`mergeable: yes` · `scope_exceeded: false`） |

---

## 2. oracle 第 1 轮 findings 与处置（**6/6 采纳**）

| # | finding | 处置 | 证据 |
|---|---|---|---|
| **F1**（高） | **spec 侧旧判据未同步** ⇒ 仓内两条**互斥**判据并存（旧「逐字逐序相同」对 RV32I 交付物被违反，新的被满足）⇒ 下游契约**二义** ⇒ `rdd-verifier` 按 spec 验收必红 | ✅ **另开 change** 做 **MODIFIED delta**（只改一个 Scenario 的**正文**，**标题保留** ⇒ openspec CLI 的 MODIFIED 语义要求 Scenario **同名**）⇒ 已归档 | `openspec/changes/archive/2026-11-15-hier-top-port-table-modification-2026-11/`；主 spec 旧句**零命中** |
| **F2**（高） | ADR-062 的「根 `io()` 端口段」被「（如上）」回指到**旧宽口径**（含 inline 后代）⇒ ① 变成缩小版的原病 | ✅ 窄口径钉死：根组件**自身** `ch_in`/`ch_out` ∪ 时序基础设施 ∪ 冒泡 input ⇒ **不含** inline 后代；并订正旧括注 | `ADR-062` §5 v1.7 |
| **F3**（中） | 「根段」在 ADR 中**无机械切法** ⇒ 测试当时用**名称前缀黑名单** ⇒ 实例改名即静默错分类 | ✅ ADR 写明「段归属 MUST 以 `PortProvenanceOrigin` 为准，MUST NOT 以名称前缀推断」；**测试同步改用** `r1.port_provenance` | `ADR-062` §5 v1.7；`test_rv32i_hier_rtl.cpp`（断言 169 → **232**） |
| **F4**（中） | `⊆` **单向** ⇒ 开了「少暴露」缝（漏抬某个 inline 后代 `ch_out` ⇒ ①② 都不抓） | ✅ 增**排他句**（承重）：**扁平 \ 层次化顶层** 的端口 MUST ⊆ {成层子组件的 `ch_out`} ⇒ 等价 = 层次化顶层 MUST 含**全部** inline 后代 `ch_out` ⇒ **两半都写成可执行断言** | ADR-062 v1.7 + 主 spec Scenario + 测试 ④ |
| **F5**（低） | ADR-062 **两个 v1.5**（v1.5 = 2026-11-05 · v1.6 = 2026-11-08 已占）⇒ 撞号 ⇒ 版本锚失效 | ✅ 本次收窄改号 **v1.7**；三处销注指针同步订正 | `ADR-062` 变更记录；§7 批 3 行；improvement |
| **F6**（低） | 「决策内容零变更」**措辞高估**（ADR-066 v1.6 实增机制义务；ADR-060 的「`parent_side` 必然不等」是修辞性 over-claim） | ✅ 改为「**零反转**（但新增机制义务）」；「必然不等」降格为**说明性**注记（承重判据只留「方向」） | ADR-060 §2.1.1；ADR-066 元数据 + §2.5 |

---

## 3. critic 第 1 轮 findings 与处置（**全部闭合**）

| # | finding | 处置 | 闭合证据（critic 第 2 轮已核验） |
|---|---|---|---|
| **B1**（BLOCKER） | `ADR-060:304`（§2.2 **G3-1 理由列**）与 `:590`（§2.2 **选项表 row 9 理由①**）**两处**仍逐字保留未收窄的旧判据 ⇒ 与 ADR-062 v1.7 ② + 主 spec 互斥 | ✅ 两处收窄为「**根端口段**逐字逐序」+ 新增 **v1.20** 修订行 | critic 第 2 轮：「所有现存『逐字逐序』出现处**均带「根端口段 / 根 `io()` 端口段」限定**；未限定的旧句仅存于**修订历史行**（不构成生效判据）⇒ 三侧口径一致，跨文档二义已消除」 |
| **S1** | ADR-060 §2.1.1 表 row 1 缺「同一 pin 出现**两行**」限定 ⇒ 字面读起来像「所有父驱动子 input 都 fail-closed」 | ✅ 加限定 + 明写「**单条**父驱动子 input 是**合法**形态 ⇒ MUST NOT 被误判」 | critic 第 2 轮：「已加限定…且与 :266『混判』告诫一致」 |
| **S2** | v1.7 未就地引 **G3-5**（根 module `Synthesized` MUST 恒空）⇒ 两类划分闭合性缺就地依据 | ✅ ADR 侧补「两类划分是闭合的…」+ 引 G3-5 + 端口命名契约 N1；**spec 侧不补**（critic 判**非阻断**：Scenario 正文自足可执行 ⇒ 决策依据落 ADR 即满足可追溯性 ⇒ 不产生验收洞） | critic 第 2 轮：「维持上轮判断 —— **非阻断**」 |
| **S3** | 三处销注版本指针写成「ADR-062 v1.5」（实为 **v1.7**）；归档 change 的 5.4 销注**漏了「ADR-060 侧当时也漏改」**这一事实 | ✅ 三处指针订正；归档 5.4 销注改为「**两侧都在**」+ 显式记「**ADR-060 侧当时漏改** ⇒ 由 critic 第 2 轮 BLOCKER 指出并补齐」 | critic 第 2 轮：「三处销注版本指针已统一…归档 tasks 5.4 已补『ADR-060 侧当时漏改』的事实订正」 |
| N1/N2 | 测试排他句半句 (b) 近冗余；`root_segment` 放弃「根段是前缀」假设 | ⬜ **记录即可**（无需动作） | — |

---

## 4. 本轮**零反转**的核对（critic 第 1 轮问题 2 的回答）

- **ADR-062 v1.7**：只**收窄**「顶层端口表 == 扁平端口表」这一条**判据**的适用面 ⇒ ⇒ 能力面**零收缩**（非根 module 的 `⊇` + 五类来源 + 自驱动拒绝**全部保留**）⇒ ⇒ 属**判据面澄清**。
- **ADR-060 v1.19 §2.1.1**：只规定**双驱动判定的判据维度**（方向）⇒ ⇒ 未新增能力面义务 ⇒ ⇒ 属**适用面界定**。
- **ADR-066 v1.6**：**新增了机制义务**（分析层旧行 MUST 登记 + 发射期 MUST 丢弃 + 改接清单 MUST 结构化）⇒ ⇒ **不是**纯适用面 ⇒ ⇒ 已在元数据与 §2.5 **如实自登记**（「零反转 + 新增机制义务」）⇒ ⇒ critic 判**不越权**（备选 `CHWARN` 被明确列出并给出拒绝理由 ⇒ 属对已实现行为的追认）。

---

## 5. 复核命令（可复跑）

```bash
# ① 三侧口径一致（残留「逐字逐序」出现处 MUST 带「根端口段」限定）
grep -n "逐字逐序" docs/adr/ADR-060*.md docs/adr/ADR-062*.md openspec/specs/rtl-hierarchical-output/spec.md
# ② 主 spec 的旧句已消失
grep -c "该端口表 MUST 与扁平产物\*\*逐字逐序相同\*\*" openspec/specs/rtl-hierarchical-output/spec.md   # ⇒ 0
# ③ 归档门
bash scripts/verify_openspec_delta.sh          # ⇒ 4 条全绿
openspec validate --specs                      # ⇒ 47 passed / 0 failed
# ④ 台账 / 残差一致性
bash scripts/verify_arch_ledger_residuals.sh   # ⇒ PASS（26 行 · 含 id=25）
# ⑤ 交付物判据（机械切法 + 排他句）
cmake --build build -j$(nproc) --target test_rv32i_hier_rtl && ./build/examples/riscv-mini/test_rv32i_hier_rtl
```

---

## 6. 遗留（**不在**本轮）

- **spec 侧未引 G3-5**（critic 判非阻断 ⇒ 记录即可）。
- 测试排他句半句 (b) 与「根段是前缀」假设的放弃（**记录**）。
- **`funct3` / `funct7` 非法组合**的架构台账残差登记 ⇒ 需**扩展分析器作用域** ⇒ 独立小批（R26）。
- `include/cpu/AGENTS.md` STRUCTURE 树里其余 **6** 个 per-file 行数陈旧（**预存在漂移** ⇒ R25）。
