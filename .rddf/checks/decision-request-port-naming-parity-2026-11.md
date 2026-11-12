---
evidence: test:tests/test_port_name_parity.cpp
---

# 决策请求单：端口命名相关的**两份 spec 追加** + **残差台账两行**（2026-11-10）

> ⚠️ **本单的性质**：**①②③ 已由用户拍板**（oracle 建议 1/2/3 · 2026-11-10「按这个顺序推进」）
> ⇒ ⇒ **本单不是请 architect 重新决策** ⇒ ⇒ 而是请 architect **复核内容 + 执行两处机械落点**。
> ⇒ ⇒ ⇒ 全部内容**已可复现**（探针 + 分析器实跑）⇒ ⇒ ⇒ **裁决点只剩「是否同意」与「由谁落」**。
> ⇒ ⇒ ⇒ 落点涉及 `docs/architecture/**`（在 `.rddf/config/arch-docs.json` 的 `arch_paths` 内 ⇒ **architect 域**）
> ⇒ ⇒ ⇒ **planner 未越权改动** ⇒ ⇒ ⇒ 残差登记表**未被半改**（护栏仍 PASS / 20 行 ⇒ 仓库未留红）⇒ ⇒ ⇒ ⚠️ **时点标注（2026-11-11）**：此处的「20 行」是**本单起草时**的快照 ⇒ ⇒ ⇒ **已于 2026-11-10（commit `ba99793`）两侧同批落地 ⇒ ⇒ ⇒ 现状 = PASS / 22 行**（台账 §7 行 21/21b + 登记表 2 行）

---

## 0. 一句话背景

用户在 `port-naming-ratchet-2026-11.md` 提出的诉求「生成的 RTL **顶层端口**希望保持和 C++ 侧一样的命名」
⇒ ⇒ 三探针实测证明：**在当前扁平发射器下，「全体组件字面同名」不成立**；
⇒ ⇒ 「**根组件端口**字面同名」**有条件成立**，但要它成立需改公开 API 语义 + 全仓产物字节变更
⇒ ⇒ ⇒ **裁决为「当前不做 + 触发条件式结案」**，改以「**机器可解析**」的契约兑现诉求
⇒ ⇒ ⇒ 本单请 architect 落**两处**配套物（两份 spec 的追加 + 残差登记）。

---

## 1. 已被裁定的部分（**不在本单的裁决范围**）

| # | 裁决 | 出处 |
|---|---|---|
| **①** | 「顶层端口」口径 = **MUST 覆盖 T1**（顶层 module 端口表全体）· **字面同名 SHOULD 限定 T2**（`is_top_io == true`） | `port-name-literal-parity-2026-11.md` §裁决记录 ① |
| **②** | 与 `port-naming-ratchet` N4/AC:159 的冲突 = **方案 A + 三条加固**（「逐字相同」**删除** ⇒ 改「可机器解析」· 护栏 P1 增判「标识符 == 名字串」· 成本估算下调） | 同上 §裁决记录 ② |
| **③** | 「字面同名」**不作 MUST** ⇒ ⇒ 改为**带前提的 SHOULD** + 「**不可能条件** MUST NOT」 ⇒ ⇒ route (c) 记为「**已评估 · 当前不做 · 触发条件 = 用户在看到 manifest 产物后仍要求人眼可读**」 | 同上 §裁决记录 ②③ |

---

## 2. 请 architect 落点 **R1**：两份 spec 各追加一条 Requirement

> ⚠️ **落地面不是直接改 `openspec/specs/**`** —— 根 `AGENTS.md §OPENSPEC WORKFLOW` 明文：
> 「**Spec 变更必须走 change 流程**：直接修改 `openspec/specs/<capability>/spec.md` 是禁止的；
> 先创建 change 提案，archive 时由 CLI 自动合并。」
> ⇒ ⇒ ⇒ **delta 形态 = `## ADDED Requirements`**（**不是** `MODIFIED` ⇒ ⇒ `scripts/verify_openspec_delta.sh` R3 要求 MODIFIED 标题与主 spec **逐字一致**，否则**静默不合并**）
> ⇒ ⇒ ⇒ **architect 的动作 = 复核下列 delta 文本 + 批准其走 change 流程**

### R1-a → `openspec/specs/rtl-hierarchical-output/spec.md`（**产物端口命名**的规范所有者）

> ⚠️ **围栏内 = delta 正文（逐字复制进 change 的 `## ADDED Requirements`）**
> ⇒ ⇒ **围栏内 MUST NOT 含审稿日志 / 自指 TODO / 批注**（`ratchet` 要求「逐字复制」⇒ ⇒ 元信息会随 delta 进主 spec）
> ⇒ ⇒ ⇒ **本仓约定（critic 第 5 轮 SHOULD_FIX-3）**：审稿过程信息一律写在**围栏外**的 `>` 注里。

```markdown
### Requirement: 顶层端口名的 C++ 成员可解析性 MUST 按端口三类分别规定

顶层 module 端口表中的端口，MUST 按其**所属类别**分别满足该类别的 `cpp_member` 义务
⇒ ⇒ **B 类 MUST 提供 · A 类与 C 类 MUST NOT 被要求提供**
⇒ ⇒ 载体 = manifest 的 `cpp_member` 字段（schema 归 `rtl-emit-portability`）
⇒ ⇒ 下游工具 MUST NOT 依赖「Verilog 端口名 == C++ 成员名」这一**当前不成立**的假设。

#### Scenario: 端口的三类互斥划分与各自的 `cpp_member` 义务

- **WHEN** 一个设计发射出顶层 module 的端口表
- **THEN** 端口表中的每个端口 MUST 落入下列**恰好一类**
- **AND** 分类 MUST 按下列**判定顺序**执行（**先命中者为准** ⇒ ⇒ 顺序本身保证互斥）
- **AND** 分类 MUST 由 manifest 字段与命名棘轮登记表判定（MUST NOT 依赖人工判断）

| 序 | 类 | 判定（**按序判定，先命中者为准**） | `cpp_member` 义务 |
|---|---|---|---|
| 1 | **A · 基础设施** | `role` 为 `clock` 或 `reset` | **MUST NOT** 被要求提供 |
| 2 | **B · 显式命名** | **非** A 类，且该端口在**命名棘轮登记表**中有条目（⇒ ⇒ 判据源 = `port-naming-ratchet` N2 的登记表 + 护栏 P1「标识符 == 显式名字符串」） | **MUST** 提供，且其值 MUST 等于登记表中的显式名 |
| 3 | **C · 未显式命名** | **非** A 类，且**非** B 类 | **MUST NOT** 被要求提供（⇒ ⇒ 走兼容去重路径） |

- **AND** A 类 MUST NOT 被登记在命名棘轮的豁免表（⇒ ⇒ 它**不是** C 类 ⇒ ⇒ 无登记义务）
- **AND** C 类中**属于 product 侧**（`include/**` + `examples/**`）的端口 MUST 被登记在命名棘轮的豁免表
  （⇒ ⇒ `tests/**` 侧的端口不适用本条 ⇒ ⇒ 与棘轮扫描面一致）
- **AND** 下游工具 MUST NOT 把 A 类或 C 类「缺 `cpp_member`」当作**违规**
- **AND** 本 Requirement MUST NOT 适用于 `owner == nullptr` 且 `role` 非 `clock`/`reset` 的端口
  （⇒ ⇒ 此类由层次化分析层的 `CHWARN` 单独处置）
- **AND** 不存在既非 A/B/C 三类、也未被本 Requirement 显式排除的端口

#### Scenario: 未显式命名者（C 类）走兼容去重路径

- **WHEN** 一个端口落入 C 类
- **THEN** 它 MUST 走**兼容去重路径**（旧的去重规则保留为兼容模式）
- **AND** 它 MUST NOT 被要求提供 `cpp_member`

#### Scenario: 字面相同的成立条件与不可能条件

- **WHEN** 一个端口同时满足 ① 它是**根组件自有**的端口（manifest 的 `is_top_io == true`）
  ② 该组件实例名在其父下**唯一** ③ 该端口名与根组件自有内部节点名**不撞**
  ④ 其 C++ 成员名本身是**合法 Verilog 标识符**（`[A-Za-z_][A-Za-z0-9_]*`）
- **THEN** 该端口的 Verilog 端口名 **SHOULD** 与其 C++ 成员名逐字相同
- **AND** 同名兄弟实例的端口、子组件的端口、Verilog 关键字与数字开头的成员名
  **MUST NOT** 被承诺逐字相同
```

> ⚠️ **围栏外 = 审稿记录（不进 delta）** —— 以下说明**为何**这样写，供后续复核：
>
> ① **为什么按序判定（三类互斥的实现方式）**（critic 第 5 轮 BLOCKER-1 · 2026-11-11）：
> A 类（`role` ∈ {clock, reset}）与 C 类（「无显式 C++ 成员名」）**谓词本身有交集** —— clock/reset 是 ctx 级节点
> ⇒ ⇒ **无 C++ 成员** ⇒ ⇒ 同时满足 C 的谓词 ⇒ ⇒ 若两者**并列判定**则「恰好一类」对每个含 clock/reset 的设计**恒假**
> ⇒ ⇒ ⇒ 且 clock/reset 落 C 后会触发「MUST 登记在棘轮豁免表」⇒ ⇒ 而 ctx 级节点**不存在可登记条目**
> （登记条目身份 = 「`<文件>` + `<__io 块序号>` + `<声明文本>`」）⇒ ⇒ ⇒ 该 MUST **永不可满足**（常态违约复发）
> ⇒ ⇒ ⇒ 故 MUST **按序判定**（先 A ⇒ ⇒ 再 B ⇒ ⇒ 剩余为 C）⇒ ⇒ ⇒ **顺序保证互斥** ⇒ ⇒ ⇒ 「恰好一类」**可判定**。
>
> ② **为什么不用「每个端口 MUST 提供」**（critic 第 3/4 轮 BLOCKER-1 / BLOCKER-A · 2026-11-11）：
> **A 类无解** —— clock/reset 是 ctx 级基础设施（`include/core/context.h:245-246` 的
> `core::clockimpl *default_clock_` / `core::resetimpl *default_reset_`）⇒ ⇒ **无 C++ 成员**
> ⇒ ⇒ 而顶层端口表**必含**它们（`rtl-hierarchical-output/spec.md:312`）⇒ ⇒ 「每个端口 MUST 提供」= **常态违约**。
> **C 类无解** —— 存量未显式命名的端口（`port-naming-ratchet` 实测 **3103** 处）**本就没有** C++ 成员名
> ⇒ ⇒ 强求提供 = 常态违约 ⇒ ⇒ 它走**兼容去重路径**（旧去重规则保留为兼容模式）。
> **豁免判据 = `role`，不是 `owner == nullptr`** —— `src/hierarchy_analysis.cpp:744-753` 显示
> `owner == nullptr` 的节点**不止** clock/reset，另有一类「**归属空洞**」（非 clock/reset 且无归属）
> ⇒ ⇒ 分析层专门对它 `CHWARN` ⇒ ⇒ 按 `owner` 豁免会把**真缺陷**一并豁免。
> ⇒ ⇒ **`role` 字段已存在**于 manifest schema（C4 已定 `role`（`clock`/`reset`/`data`））⇒ ⇒ **不新增字段、不改 C4**。
>
> ③ **「归属空洞」类的显式排除**（critic 第 4/5 轮 SHOULD_FIX-5 + BLOCKER-1 · 2026-11-11）：
> `rtl-hierarchical-output/spec.md:98-111` 允许「用户不经组件直接创建的节点」归顶层 module
> ⇒ ⇒ 而 `src/hierarchy_analysis.cpp:386-389` / `:403-405` 显示这类节点**确实会进顶层端口表**
> ⇒ ⇒ 若不作排除，它落入 C 类 ⇒ ⇒ 触发「MUST 登记在棘轮豁免表」⇒ ⇒ 而它**无 `__io` 声明位置** ⇒ ⇒ ⇒ 无登记条目 ⇒ ⇒ ⇒ 永不可满足
> ⇒ ⇒ ⇒ 故 MUST 在**规范句**里显式排除（`owner == nullptr` 且 `role` 非 `clock`/`reset`）
> ⇒ ⇒ ⇒ 此类由分析层的 `CHWARN` 单独处置（**独立立项，不在本条范围**）
> ⇒ ⇒ ⇒ ⇒ **不设第四类**（第四类会与「三类互斥」自相矛盾 ⇒ ⇒ 排除语更精确）。
>
> ④ **B/C 判据的字段来源**（critic 第 5 轮 SHOULD_FIX-4 · 2026-11-11）：
> B/C 的判据「有/无显式 C++ 成员名」**在 C4 的字段表里没有直接对应字段** ⇒ ⇒ 若以「`cpp_member` 是否存在」为判据
> ⇒ ⇒ 则 B 类的「MUST 提供 `cpp_member`」成为**同义反复**（判据可被平凡满足）⇒ ⇒ ⇒
> 故判据源 MUST 是**命名棘轮登记表**（ratchet N2）⇒ ⇒ ⇒ 与 `role`（C4 已有字段）共同构成完整判据 ⇒ ⇒ ⇒ **不新增字段**。
>
> ⑤ **登记义务的 product-side 限定已写进规范句**（critic 第 5 轮 SHOULD_FIX-5 · 2026-11-11）：
> 限定 MUST 在**规范句**（AND 子句）里，**不能只靠邻接注** ⇒ ⇒ ⇒ 否则逐字复制后主 spec 的 Scenario 仍是无条件 MUST。

### R1-b → `openspec/specs/component-ownership-and-naming/spec.md`（**节点 / 实例命名**的规范所有者）

```markdown
### Requirement: 组件路径与节点名的前缀拼接 MUST 同口径

组件实例的**子**路径 MUST 由「父路径 + `.` + 局部名」构成
⇒ ⇒ MUST NOT 因父路径的**字面值**而单方面剥离路径段
⇒ ⇒ 节点名的前缀拼接 MUST 与组件路径拼接**同口径**
⇒ ⇒ 「无名」是**普通名字**，不是「无路径段」标记。

#### Scenario: 有父的组件实例路径由父路径与局部名构成

- **WHEN** 一个组件实例的**有父**、且其父组件的 `name()` 为 `<P>`（非空）、其局部名为 `<L>`
- **THEN** 该实例的 `name()` MUST 等于 `<P> + "." + <L>`
- **AND** MUST NOT 因 `<P>` 的字面值而剥离该路径段

#### Scenario: 无父的组件实例路径等于其局部名

- **WHEN** 一个组件实例**没有父组件**（`parent == nullptr`）
- **THEN** 该实例的 `name()` MUST 等于其 `local_name()`
- **AND** 该实例的**子**节点 MUST 以 `<local_name> + "."` 为前缀（与上一条同口径）
```

> ⚠️ **architect 复核订正（2026-11-10 · 见 `.rddf/reviews/port-naming-parity-spec-delta-review-2026-11-10.md`）**：
> 原 R1-b **漏了「无父」的情形**（根组件的 `name()` == `local_name()`）⇒ ⇒ 契约不完整
> ⇒ ⇒ 已补 **Scenario「无父的组件实例路径等于其局部名」** ⇒ ⇒ ⇒ 该 Scenario 同时钉住「根组件的子节点也走同一口径」

> ⚠️ **⚠️ `:62` 的既有 Requirement MUST NOT 动**（「其 Verilog 名 MUST 含该组件的路径段」）
> ⇒ ⇒ 方向 (i)（让 `lnodeimpl` 也剥哨兵 ⇒ ⇒ 根的成员节点变裸 ⇒ ⇒ **违反** `:62`）
> ⇒ ⇒ ⇒ **不在本次追加范围** ⇒ ⇒ ⇒ 登记为**未决残差**（见 R2-1）

---

## 3. 请 architect 落点 **R2**：残差登记（**两侧同批** · 已端到端验证）

> ⚠️ **双向 1:1 耦合**：`.rddf/checks/arch-residual-registry.md` 与
> `docs/architecture/2026-11-02-hierarchical-rtl-output.md` §7 由
> `scripts/verify_arch_ledger_residuals.sh` 做**双向 + 1:1** 核对 ⇒ ⇒ **单改一侧 ⇒ 护栏红**
> ⇒ ⇒ ⇒ **两侧 MUST 同批**。
> ✅ **planner 已在临时 root 上实跑 `scripts/lib/ledger_residual.py check` 验证**：两侧同批 ⇒ ⇒ **PASS（22 行）**
> ⇒ ⇒ ⇒ 下列文本**曾逐字可执行**（planner 在临时 root 实跑验证）⇒ ⇒ ⇒ ⚠️ **2026-11-11 · critic 第 3 轮 SHOULD_FIX-4**：**已执行并落地**（commit `ba99793`）⇒ ⇒ ⇒ architect 落地时把台账行的 `:62` 措辞改写 ⇒ ⇒ ⇒ **权威文本 = 磁盘已落地行**（台账 `:299-300` + 登记表末尾 2 行）⇒ ⇒ ⇒ 下列是**留档**，不再是待执行文本。

### R2-1 → 登记表（`.rddf/checks/arch-residual-registry.md` · 追加到**表格末尾**）

```
| docs/architecture/2026-11-02-hierarchical-rtl-output.md | 7 | 21 | 未决 | 2026-11-10 | route (c) 字面同名当前不做 · 触发条件 = 用户在看到 manifest 产物后仍要求人眼可读 | — | 用户在看到 manifest 产物后仍要求人眼可读 ⇒ ⇒ 重开 route (c) 裁定 | architect |
| docs/architecture/2026-11-02-hierarchical-rtl-output.md | 7 | 21b | 未决 | 2026-11-10 | 根端口裸名目标钉 · 随 route (c) 一并处置 | — | route (c) 重开时一并处置 | architect |
```

### R2-2 → 台账（`docs/architecture/2026-11-02-hierarchical-rtl-output.md` §7 的表内）

> ⚠️⚠️ **必须插进 §7 表内** —— 追加到文件末尾**不会被解析**（会报「登记表有台账无」）。这是 planner 首次试错时实测到的。

```
| 21 | **route (c)「字面同名」路线** —— 顶层端口名与 C++ 成员名逐字相同（根组件端口 + 实例名唯一 + 全设计 base name 不撞 + 成员名为合法 Verilog 标识符）⇒ ⇒ 探针实测：根实例名置空 ⇒ 得 `unnamed_prog_addr`（**不是**裸名）⇒ ⇒ N5(c) 的回退条件已被证伪；同名兄弟实例 ⇒ ⇒ 必然吃 `_N` ⇒ ⇒ 「全体组件字面同名」在当前扁平发射器下不成立；方向 (i)（让 `lnodeimpl` 也剥哨兵）⇒ ⇒ 撞 `openspec/specs/component-ownership-and-naming/spec.md:62`「Verilog 名 MUST 含该组件的路径段」⇒ ⇒ 需 architect 裁 |⚠️ **2026-11-11 · critic 第 3 轮 SHOULD_FIX-4 修复**：原文写「撞本表 `:62`」⇒ ⇒ **「本表」歧义**（本单不是 spec）⇒ ⇒ ⇒ 补全为**规范文件 + 行号** ⇒ ⇒ ⇒ 且 architect 落地时把该措辞改写为「撞本文件 `component-ownership-and-naming` 的「Verilog 名 MUST 含该组件的路径段」」⇒ ⇒ ⇒ **权威文本 = 磁盘已落地行**（台账 `:299`）⇒ ⇒ 本段是**已落地文本的留档**，不再是「逐字可执行」 ⚠️ 未决 ⇒ route (c) 字面同名当前不做 · 触发条件 = 用户在看到 manifest 产物后仍要求人眼可读 |
| 21b | **根端口裸名的目标钉**（「根组件端口的 Verilog 名 == C++ 成员名」）⇒ ⇒ 随行 21 一并处置 ⇒ ⇒ 判据与探针见 `.rddf/improvements/port-name-literal-parity-2026-11.md` | ⚠️ 未决 ⇒ 根端口裸名目标钉 · 随 route (c) 一并处置 |
```

### R2-3 → 格式约束（读 `scripts/lib/ledger_residual.py` 得出 · 供复核）

- `未决` 行 **MUST** 登记**复核条件**（不可为空）
- `闭口证据` 填 `—` ⇒ ⇒ `_split_evidence("—")` 返回空 ⇒ ⇒ 不触发「候选闭口证据已存在 ⇒ 红」
- `未决` **不在** `EVIDENCE_REQUIRED`（仅 `已收口` / `已裁决` 需证据）
- 后缀列与台账状态格的后半段 MUST **逐字相同**（规范句派生）

> ⚠️ **不重复登记**：`node-name-prefix-consistency-2026-11`（F-A）承载的是**方向 (ii)**
> ⇒ ⇒ 方向 (i) 由 **R2-1/R2-2 的同一条**覆盖 ⇒ ⇒ ⇒ **F-A MUST NOT 新增行**
> ⇒ ⇒ ⇒ 否则双向 1:1 护栏会被**双重登记**打红。

---

## 4. 事实与证据（**仓内可复现**）

| # | 事实 | 复现 |
|---|---|---|
| ① | 根实例名置空 ⇒ 节点名 `unnamed.prog_addr` ⇒ Verilog `unnamed_prog_addr` ⇒ **不是**裸 `prog_addr` | `/tmp/opencode/probe_bare_port_name.cpp`（**scratch · 未进仓** ⇒ ⇒ 结论已写进 `port-name-literal-parity` A.1 ① ②） |
| ② | 同名兄弟 ⇒ `top_dup_prog_addr` / **`top_dup_prog_addr_1`** ⇒ ⇒ 「逐字相同」当前不成立 | `/tmp/opencode/probe_dup_sibling.cpp` |
| ③ | 「去前缀后碰撞概率猛增」**被证伪**（根前缀是**公因子** ⇒ 碰撞条件剥前缀下**不变** ⇒ 今天 `top_prog_addr` 与 `top_prog_addr_1` **就已撞**） | `/tmp/opencode/probe_fb.cpp` |
| ④ | 层次化分析层**复用扁平全局表** ⇒ ⇒ **不**提供端口名消歧（`src/hierarchy_analysis.cpp:195-201`） | 读码 |
| ⑤ | 框架**无反射** ⇒ ⇒ `cpp_member` **只能是显式命名串** ⇒ ⇒ 「可解析到 C++ 成员名」**必须**由静态护栏强制「标识符 == 名字串」 | `include/core/io.h:319-329`（`__io` 生成匿名 struct） |
| ⑥ | R2 的补丁**端到端验证过** | 临时 root 上 `python3 scripts/lib/ledger_residual.py check --root <tmp>` ⇒ ⇒ **PASS（22 行）** |

---

## 5. 选项 / 代价

| 选项 | 内容 | 代价 | 评价 |
|---|---|---|---|
| **A（推荐）** | architect 复核 R1-a / R1-b 的 delta 文本 ⇒ ⇒ 走 openspec change 流程落地；**同批**执行 R2-1 / R2-2 | 两份 spec 各 1 条 Requirement + 台账 2 行 + 登记表 2 行 ⇒ ⇒ **机械可执行** ⇒ ⇒ 护栏立即可验 | ✅ |
| **B** | 只做 R2（残差登记），R1 的 spec 追加暂缓 | 残差有登记、契约无 ⇒ ⇒ 诉求在规范里**无落点** ⇒ ⇒ 后续必然再补一次 | ⚠️ |
| **C** | 全部暂缓 | 两份草案的 AC「MUST 登记」长期未闭 ⇒ ⇒ **本项不算完成** ⇒ ⇒ 须在残差表登记「未做」⇒ ⇒ 但那又是一条登记 | ❌ |

---

## 6. 建议

**采纳选项 A**。⇒ ⇒ 落点顺序建议：**R2（台账 + 登记表，同批）先做**（护栏立即可验）
⇒ ⇒ **R1（spec delta）走 openspec change 流程**（架构决策类 change ⇒ ⇒ architect 主导 ⇒ ⇒ planner/builder 配合）。
⇒ ⇒ 完成后即可申请 **critic 第 3 轮**（预算 2/2 已用满 · oracle 建议 6：**先修完再审** + 书面 scope 只含「上轮 BLOCKER 闭合 + 新 diff」）。

---

## 7. ⚠️ 与本单相关的其它待办（**不在本单范围**）

| # | 事项 | 归属 |
|---|---|---|
| ① | `node-name-prefix-consistency-2026-11` 的 P1（`include/component.h:274` 删一行）⇒ ⇒ **仓内零影响 + 下游可见行为变更** ⇒ ⇒ 走 **CHANGELOG + `public-api-surface.md`「行为变更（非符号移除）」节** | builder（B0 批）+ architect 复核其 R1-b 的 delta |
| ② | `rv32i-dead-component-consolidation` 的 **C8**（`mem_is_load` 装配点 tie-off ⇒ ⇒ 顶层 input 8→7）⇒ ⇒ **契约相容**（补 driver ≠ 改端口面） | builder（B0 批） |
| ③ | **critic 第 3 轮 + 第 4 轮** | 第 3 轮 ✅ 已跑完（REJECT · 5 条 BLOCKER）· 第 4 轮 ✅ 用户 2026-11-11 批准（scope 极窄）⇒ ⇒ 结论 = REJECT（1B-4 未闭合 + 新引入 2 条）⇒ ⇒ **修复中** ⇒ ⇒ ⇒ 第 3 轮记录 = `.rddf/reviews/port-naming-parity-critic-round3-2026-11.md` |
