# 审稿记录：ADR-066 父侧落点语义（Q1 裁 B）

- **审稿对象**：`docs/adr/ADR-066-hierarchical-parent-side-landing-semantics.md`（**新增** · ADR）
- **关联**：台账 §7 行 **16**（本 ADR 裁定其语义）· 行 **19**（本批新登记）· 裁定输入文档 `docs/architecture/2026-11-08-hierarchical-parent-side-landing.md`
- **审稿顺序**：oracle → critic（不可颠倒）
- **档位**：`review_profile: arch`
- **预算**：oracle **1 轮**（已用 1）· critic **2 轮**（已用 **2 = 上限**）

---

## 1. oracle 第 1 轮

- **判定**：`治理面判定: PASS-WITH-NOTES` · **0 BLOCKER** · 6 SHOULD_FIX + 4 NIT
- **核心判断**：决策本身对，坏在三处**文档级精度缺口**。

### 1.1 逐条处置（全部采纳）

| # | 级别 | finding | 处置 |
|---|---|---|---|
| ① | SHOULD | **落点网命名未钉死** + 现状代码有**自指陷阱**：row-2 合成连接实参用 `names.at(e.to_ptr)`（被驱动节点自体名，`src/hierarchy_verilog.cpp:697-698`）⇒ 消费点是表达式时产出 `assign top_or_or = top_or_or \| 1`（自指 + 双驱动）；兄弟边用 `names.at(e.from_ptr)`（驱动端点全局名，`:618`）才是正确默认 | **采纳** ⇒ 决策 ① 增补「落点网名 MUST = 驱动端点 ctx 全局名 · MUST NOT = 被驱动节点自体名」+ 自指/双驱动陷阱说明 |
| ② | SHOULD | **V2/V3 子串判据对正确实现假红**：网名 = 驱动端点全局名 ⇒ 子 module **构造性**含该串（`codegen_verilog.cpp:627-634`、`:1086-1090`） | **采纳** ⇒ V2 改为「父 body 内每个标识符 MUST 在父 module 作用域内可解析」+ 显式声明 **MUST NOT** 写「网名不出现于子 body」；V3 改为「父 body 引用的标识符 MUST NOT 是仅在子 module 内声明的名字」+ 机械判据 = R1 lint 门零 `%Error` |
| ③ | SHOULD | **V8 路径枚举方向搞反**：ADR-060 的四条 `src(0)` 路径是 **`input` 方向**的守卫面；Broken 的真实失效点在**表达式操作数命名**（`print_unary_op:868-897` / `print_mux:1144-1154` 直接用 `node_names_[src]`，**从不查重定向表**） | **采纳** ⇒ V8 拆成**两个面**（面 ① 操作数名解析 · 面 ② R2 四路径回归）⇒ 并给 **grep 锚** `node_names_[`（变异点数 MUST == grep 命中数） |
| ④ | SHOULD | **决策 ② 缺选项分析**（support vs fail-close）+ 措辞「读合成端口名」不确 | **采纳** ⇒ §3 增 **B′**（路径 B 改 fail-close）行并显式驳回；措辞改为「读**父侧落点网名**」 |
| ⑤ | SHOULD | **决策 ① 过度包含父 `input` 端口** | **采纳** ⇒ 增「消费者排除项：父 `input` 端口 ⇒ MUST 拒绝」+ 新 V14 负向用例 |
| ⑥ | SHOULD | **①×④ 重叠无 V 项** + 连接去重语义未定 | **采纳** ⇒ V7 增该重叠形态 + 「同 `(实例, pin)` 相同网名幂等去重 · 不同网名合并为一根网扇出」 |
| ⑦ | NIT | 表述校准（「用户零改写」限定 / 「零告警」改述 / 决策 ⑤「天然满足」/ C 的驳回补半句） | **采纳**（全部） |

---

## 2. critic 第 1 轮

- **结论**：**`REJECT`** · `mergeable: no` · `scope_exceeded: false` · **1 BLOCKER** + 2 SHOULD_FIX + 3 NIT

### 2.1 BLOCKER（**确认属实 · 是我按 oracle 建议 ⑤ 改动时引入的**）

**决策 ① 的规范性引用块**把「**或父 module 的 `output` 端口**」列入 MUST-被支持形态，且「MUST 合成一根 `logic` 网」在块内是**无条件**陈述 ⇒ ⇒ 与同文档三处矛盾：① 本节标题（范围 = 消费点**不是**父 `output` 端口）· ② 背景表 S1 行（直通 ⇒ 不造网）· ③ **决策 ③**（消费点就是父 `output` 端口时 **MUST NOT** 强制造网）⇒ ⇒ 属 arch BLOCKER 第 3 类（文档内自相矛盾）。

**处置**：引用块重写为**按消费点性质分两子情形** —— (a) 消费点 = 父 `output` 端口（直通 · 无运算）⇒ **无需造网** ⇒ **由决策 ③ 管辖 · 不是决策 ① 的授权对象**；(b) 消费点 = 父体内节点（带运算 · 族 ①/③）⇒ MUST 造网 + 操作数读网。⇒ 自查：全文 grep「或父 module 的 `output` 端口」= **0 命中** ✅

### 2.2 SHOULD_FIX / NIT 处置

| # | 级别 | finding | 处置 |
|---|---|---|---|
| ① | SHOULD | **V12 缺「ADR-060 §2.1 矩阵补格」的验收项**：V1 翻转的是 `tests/` 的 42 格 probe matrix，**不是** ADR-060 契约矩阵本身 ⇒ 实施 change 可全绿而契约矩阵**永不补格** | **采纳** ⇒ 新增 **V15**（矩阵 MUST 补格 · 与第 2 行**并列存在** · 与 V1 分开验收） |
| ② | SHOULD | **取代流程只走一半**：被修订处（ADR-062 §5 该条 · spec 的 MODIFIED delta）MUST 加反向指针（本仓惯例：ADR-058 v2.11 / ADR-060 v1.9） | **采纳** ⇒ V12 补反向指针要求 + 声明「本轮**不**预先改 ADR-062 正文（ADR-066 尚 `Draft` ⇒ 预先改 Accepted ADR 违反惯例）⇒ 指针随 change 落地」 |
| ③ | NIT | 决策 ② 静默依赖「端点名 ⇒ 隐式端口名 ⇒ 网名」链条 | **采纳** ⇒ 增 ⓘ 链条说明（⚠️ 见 §3 —— 该说明本身引入了新 BLOCKER） |
| ④ | NIT | V8 面 ① 的「等」不可枚举 | **采纳** ⇒ 给 grep 锚 `node_names_[` + 「变异点数 == grep 命中数」 |
| ⑤ | NIT | 一文档多决策（①–⑥） | **采纳其结论** ⇒ 不拆分（同一主题下的条款 · 互相交叉引用）；仅记录 |

---

## 3. critic 第 2 轮（复审 · 只验 BLOCKER 闭合）

- **BLOCKER 闭合判定**：**CLOSED** ✅（决策 ① 的 (a)/(b) 拆分是真修复，非措辞挪位；三方一致性已核：S1 行 · 事实 ⑦ · 决策 ③）
- **新 BLOCKER**：**1 项** —— 我在处置 NIT ③ 时加的「链条」说明写「**三者同名**」（端点名 = 隐式端口名 = 网名）
  ⇒ ⇒ **与 ADR-060 §2.2 v1.8 的命名订正冲突**（`docs/adr/ADR-060-hierarchical-port-policy-and-module-reuse.md:222-225`：
  隐式端口名 = `__if_<n>` / `__x_<n>`，`n` = 该 module 内**出现序**）⇒ 且与本文档 V2 自己的示例
  （`assign __x_0 = <端点全局名>`）**自相矛盾** ⇒ ⇒ 属 BLOCKER 第 3 类 + 冲突已签批 ADR。

### 3.1 该新 BLOCKER 的处置（**已修 · 但未经第 3 轮复审**）

- **核实**：architect 独立核对 `ADR-060:222-225` 原文 ⇒ **critic 正确**（隐式端口名 = `__x_<n>`，不是端点全局名）⇒ ⇒ 我的「三者同名」是**误判**。
- **修复**：链条说明改写为 —— 端口名 = `__x_<n>`（ADR-060 §2.2 v1.8 · 出现序）；端点 ctx 全局名出现在**两处**：
  ① 子 module 内 `assign __x_<n> = <端点全局名>` 的 **RHS**；② **父侧落点网名**（决策 ①）⇒ ⇒ ⚠️ **端口名 ≠ 网名**，
  MUST NOT 把合成端口命名为端点全局名（那会违反 ADR-060 的确定性命名契约）。
- **自查**：全文 grep「三者同名」= **0 命中** ✅ · 文档 247 行（≤250）✅、`mergeable: no`（存在上述 1 项新 BLOCKER；上一轮 BLOCKER 本身已闭合）
  · `scope_exceeded: false`

---

## 4. 预算状态（**硬约束**）

| 项 | 状态 |
|---|---|
| oracle 轮次 | **1 / 1**（已用尽；无 BLOCKER ⇒ 按预算规则不重发） |
| critic 轮次 | **3 / 2**（**用户已明确批准超出上限**）⇒ 第 3 轮 = 只验 BLOCKER2 闭合 |

### 4.1 critic 第 3 轮（用户批准 · 只验闭合）

| 项 | 结论 |
|---|---|
| **第 2 轮 BLOCKER 闭合判定** | **CLOSED** ✅ —— 新段落与 ADR-060 §2.2 v1.8（`:222-225`）逐字对齐；与 V2 示例（`assign __x_0 = <该名>`）**互相支撑**（不再矛盾）；与决策 ① 的落点网命名规则一致；决策 ② 的 MUST 未被削弱；且把命名权威**归还** ADR-060（不是新授权）⇒ ⇒ 原「错误归因」问题消除 |
| **新 BLOCKER** | **NONE** ✅（该段替换未触碰 ADR-060 §2.1/C6/R1/R2、ADR-062 §2.6/§5） |
| `mergeable` | **yes** · `scope_exceeded: false` |

⇒ ⇒ **本 ADR 满足「ADR 完成的标志」第 2 条**（BLOCKER 全部闭合并经复审确认）。

### 4.2 Status 转换

`Draft` ⇒ **`Accepted`**（2026-11-08）⇒ ⇒ 元数据已写明：用户拍板 Q1=B · oracle 1 轮 + critic **3 轮** · 两轮 BLOCKER 全部闭合 · 审稿记录指针。

---

## 5. 本批其它产出与验证

| 项 | 结果 |
|---|---|
| **行 19 登记**（`.rddf/checks/arch-residual-registry.md` + 台账 §7 派生） | ✅ 已落 · `bash scripts/verify_arch_ledger_residuals.sh` ⇒ **PASS（19 行 · exit 0）** |
| 护栏判别力 `--self-test` | ✅ **11 条投毒 · 失败 0**（exit 0） |
| 产品代码 | `src/` `include/` = **0** 改动 ✅ |
| ADR 行数 | **247 行**（≤250 ✅） |
| ADR 四要素 | 元数据（含取代声明 + change 待立项占位）· 选项对比 4 行 · 独立后果/代价章 · Verification V1–V15 前瞻判据 ✅ |
| 过程日志 | **未**入 ADR 正文（本文件承载）✅ |

---

## 6. 遗留

| 项 | 状态 |
|---|---|
| **第 3 轮 critic 复审** | ⏸ 待用户批准（预算上限） |
| ADR-066 转 `Proposed` | ⏸ 依赖上一条 |
| ADR-062 §5 该条 + spec `:241-246` 的反向指针 | ⏸ 随 change 落地（V12）· 本轮**不**预改 Accepted ADR（正确） |
| `backlog:` → 实际 change 指针 + AC 映射 | ⏸ planner 立项后回填 |
| 实施 change（`hierarchical-parent-side-landing-implementation`） | ⏸ planner 域 |
| T3 / T4-M1,M2（T8 的遗留） | ⏸ builder 域 |
