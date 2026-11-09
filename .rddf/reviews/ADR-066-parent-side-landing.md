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

---

## 7. v1.1 修订轮（2026-11-08 · 实施期裁定 · oracle 1 轮 + critic 1 轮）

### 7.1 触发（四类**实施期新事实** ⇒ 需修订已签批 ADR）

| # | 事实 | 证据 | ⇒ ⇒ 触发 |
|---|---|---|---|
| ① | 分析层把跨 module 边分**两桶**（`pending_implicit_edges` vs `analysis.edges`）⇒ ⇒ 「决策 ① 的实现面覆盖两条路径」**不成立** | `hierarchy_analysis.cpp:528-660` · `hierarchy_verilog.cpp:405` | **Q2**（拆批口径） |
| ② | 命名真源**唯一**（`node_names_ = view_.names`）⇒ ⇒ codegen 侧**无需改动** ⇒ ⇒ V8 面 ① 的「变异点数 == grep 命中数」会制造 **62 条无效变异**且对重构脆弱 | `codegen_verilog.cpp:53`（62 **行** / 64 **次**） | **Q3**（判据重述） |
| ③ | 「静默不发射」被**运行期实测证伪** ⇒ 真实 = 分析层**响亮拒绝 + 归因错误** | 见 §7.3 的探针输出 | **Q4 / Q5**（mem 范围 + 归类边界） |
| ④ | 「MUST NOT 引用子 module 的**内部信号名**」与「落点网名 = 驱动端点的 ctx 全局名」**自相矛盾**（同一串） | ADR-066 §2.2 vs §2.1 决策 ① | **Q6**（措辞订正） |

⇒ ⇒ 裁定输入 = `openspec/changes/hierarchical-parent-side-landing-implementation/decision-requests.md`（Q2–Q6）
⇒ ⇒ **oracle + metis 双审**（裁定输入层）⇒ ⇒ 结论与 disposition 逐条记录在该文件 §9。

### 7.2 审稿结论（**契约层**）

| 轮 | 审查者 | 结论 | 处置 |
|---|---|---|---|
| 1 | **oracle**（arch） | 四类修订的**决策本身全部成立**；但指出 **2 处执行面缺陷** + **2 处 NIT** | ✅ 全处置：① ADR-062 **§5:325-327** 的无条件判据原句未收窄（⇒ 与 §2.6 形成**同一 ADR 内的活矛盾**）⇒ 已收窄 + 指针；② ADR-060 §6 缺 2026-11-08 变更记录 + 行标签 **v1.14 撞号** ⇒ 已改 **v1.16** + 补条目（ADR-062 同 ⇒ **v1.6**）；③ 3 处 stale 「ADR-060 `:69` 矩阵第四行」⇒ `:70` 第五行；④ ADR-060 新格 / spec delta 的谓词宽狭与 ADR-066 决策 ① 不一致 ⇒ 补「父 `input` 排除项」半句 + 交叉引用 |
| 1 | **critic**（arch） | **APPROVE_WITH_CONDITIONS** · `mergeable: yes` · `scope_exceeded: false` · **0 BLOCKER** | 3 条 SHOULD_FIX 全处置：① 「第四态 `Broken`」的时间线在 ADR 内**互相矛盾**（元数据/§4 当作现状 vs V1 当作前瞻 vs V8① 说已替代）⇒ 对齐为「**已在本 change 内移除** + 永久替代 = R9」；② Parent 元数据漏列 ADR-062 **§2.6**（被修订点清单不完整）⇒ 已补；③ V12 内嵌过程日志（指针已落地）⇒ 移出，只留判据 |
| — | **verifier**（builder 域） | 本 change 的验收（`tasks.md` T0–T6）由 builder 自查 + 全量回归承担 | 见 §7.4 |

⇒ ⇒ **审查预算**：本 change 的 critic 共 **1 轮**（限额 2）⇒ oracle 1 轮 ⇒ **无 BLOCKER ⇒ 无需复审**。

### 7.3 P0 运行期复现（**Q4 的判据** · oracle 要求转贴）

```
DUT：父组件 CH_HIERARCHY_LAYER + ch_mem 成员 + 成层子组件的 ch_out 驱动 mem.aread 地址
命令：g++ -std=c++23 -I include … probe.cpp build/libcpphdl.a && ./probe
输出：
[FATAL] hierarchical analysis: cross-module ch_mem access
         ('top.mem_aread' in 'top' <- 'top.leaf.unnamed_output' in 'top.leaf').
         ADR-060 requires the memory and its user to be in the same module. …
ok=0
error=FATAL: hierarchical analysis: cross-module ch_mem access …
```

⇒ ⇒ **F4 成立**：「静默不发射」**不可达** ⇒ ⇒ 现状 = **响亮拒绝 + 归因错误**
⇒ ⇒ **决策 ⑦**（本形态不属父侧落点消费点）+ **台账行 20 改记「已裁决（不纳入）」并归下游**。

### 7.4 实施与闭口（builder 域 · 2026-11-09）

| 项 | 结果 |
|---|---|
| 实施 | `src/hierarchy_verilog.cpp`（父侧落点网合成 + 诊断跳过 + 诊断文案重写）· `tests/*` · `scripts/verify_hierarchical_rtl.sh`（**R9** + 投毒 10）· `tests/hier_rtl_products.cpp`（4 个父侧落点 fixture） |
| 全量回归 | `ctest -LE perf` = **242/242 通过**（含 77 个 `verilator_e2e`） |
| 护栏 | 5 个 **全绿**（`verify_test_counts` · `verify_hierarchical_rtl` · `verify_hierarchical_port_naming` · `verify_hier_corpus_coverage` · `verify_arch_ledger_residuals`）+ 4 个 `--self-test` **全绿** |
| 契约 | ADR-066 **v1.1** · ADR-060 **v1.16**（§2.1 补格）· ADR-062 **v1.6**（§2.6 + §5 收窄 + 双向指针）· spec MODIFIED 已合并 |
| 归档 | `openspec/changes/archive/2026-11-09-hierarchical-parent-side-landing-implementation/`（**无** `--skip-specs`） |
| 台账 | 行 **16** 已裁决 · 行 **17** 已收口 · 行 **19** 已收口 · 行 **20** 已裁决（不纳入 · 归下游）⇒ 登记表 20 行 + 台账派生 ⇒ 护栏 **PASS** |

### 7.5 遗留（**如实登记**）

| 项 | 状态 |
|---|---|
| **A8 面 ① 的 62 处** | **零变异**（命名真源唯一 ⇒ 无需改）⇒ 「无需改动」的机械证据（grep 清单 + 负向注入）**待补**（`tasks.md` T4.4）⇒ ★ **登记为待办**，非已闭口 |
| **mem 端口实参跨层**（台账行 20 的下游立项） | ⏠ **未立项** ⇒ 分析层 `:543` 的粒度拆分 + 归因订正仍待做 |
| ADR-066 正文 306 行 | ⚠️ > 250 自检阈值；决策+后果+选项占比 ≈ 53% ⇒ **未触发**「决策被稀释」判据 ⇒ 备案 |
| `A15`（mem 读值非 0） | ⏸ **随行 20 的下游立项**（本 change 不含） |

---

## 8. **修订 v1.2 轮**（phase-6 **P4** 范围裁定 · 2026-11-09 · architect 域）

> **触发**：phase-6 `P4` = 「mem 端口实参跨层」的**范围裁定** ⇒ ⇒ 目标 = 把 ADR-066 §2.7
> 压成一句的边界**写死**（逐字可 grep）并落进 ADR-060 §2.1 矩阵。
> **上游依据**：`.rddf/improvements/mem-port-arg-cross-layer-2026-11.md`（`draft` · A3 / A8 / A9）
> · 用户 2026-11-09 裁决（「粒度拆分 + 归因订正」无条件必做 / 「正确支持」条件化于边界句）。
> **产物**：ADR-066 **v1.2**（§2.6 / §2.7 / §3 选项 D·E·F / §4 / §5 **V18·V19·V20** / 元数据）
> + ADR-060 **v1.17**（§2.1 矩阵补格 + §2.4.1 收窄 + §5 效果判据 + §6 + 元数据）。

### 8.1 裁定内容（**边界句 · 逐字 · 唯一权威**）

```
「mem 持有 module == 使用 module、仅`实参视线`跨层」
```

⇒ ⇒ (i) **mem 本体跨 module** ⇒ MUST 响亮拒绝 · (ii) mem 本体同 module + 仅 `addr`/`en`/`wdata`
**实参视线**跨层（且实参来源**相邻**）⇒ 不属该闸门 · (iii) **非相邻**实参（孙 / 兄弟）⇒ 仍按
`:563` 相邻性规则拒绝 ⇒ ⇒ 三者**互斥且穷举**（判别程序见 §2.7，含**节点级**口径）。

### 8.2 oracle 第 1 轮（9 条 · 逐条应答）

| # | finding | 处置 |
|---|---|---|
| 1 | **高影响**：§2.7 (i) 把拒绝机制钉在 `:543` ⇒ 该边闸门对字面 (i) **不可达**（mem 本体节点零 `add_src` ⇒ 端口与其 src 的 owner 同 module ⇒ `:539` continue）⇒ 「`:543` 不变」是**错误**的机制描述 | ✅ **采纳** ⇒ (i) 改为「**MUST 响亮拒绝**（**契约要求**）」+ ⚠️ **机制订正**段（写明不可达 + P5 MUST 新增**归属比对**判定）+ 现状处置**标为预期**并挂 §5 **V20** 运行期探针 |
| 2 | 「mem 持有 / 使用 module」的**归属判定**不可猜（owner = 构造时调用侧作用域）⇒ 需节点级判别程序 | ✅ **采纳** ⇒ §2.7 新增**节点级判别程序**（① `port->parent()` 本体的 inline 折叠后归属 = 持有；② 端口节点自身的归属 = 使用；③ (i) ⇔ ①≠②；(ii) ⇔ ①==② + 三槽 + **相邻**；④ 折叠口径指针） |
| 3 | (i)/(ii) **不互斥不穷举**（孙组件实参字面满足 (ii)）⇒ 缺**第三桶** | ✅ **采纳** ⇒ 新增 **(iii) 非相邻实参**（仍按 `:563` 拒）+ 判别程序 ③ 加**相邻性**约束（★ 该条经 critic 第 1 轮 BLOCKER 进一步收紧：见 §8.3） |
| 4 | 「正确支持」支的**产物形态**无人裁定（improvement 草案越权预定「走父侧落点网」） | ✅ **采纳** ⇒ §2.7 新增「**产物形态归属**」：本 ADR **不裁定** (ii) 支持态的产物形态；若裁「支持」⇒ 产物形态 MUST 走**独立修订**；并澄清「落点网规则不适用」仅指**端口不是一根信号** ⇒ **不禁止** `addr` 实参复用决策 ① 的网机制；草案句降级为**候选形态** ⇒ ⚠️ 草案属 **planner 域** ⇒ 本轮**未改** ⇒ 列为 planner 待办 |
| 5 | ADR-060 §2.4.1「合成不适用」表里的「跨模块 `ch_mem`」格**未收窄**（漏网漂移点） | ✅ **采纳** ⇒ 该格加限定词「= mem **本体**跨 module · ADR-066 §2.7 (i)」 |
| 6 | ADR-066 决策 ⑥ §2.6 的「跨模块 `ch_mem`」行 + §4 正面「仍被 ADR-060 `:70` 先拒」在 (ii) 豁免后**失真** | ✅ **采纳** ⇒ 两处均加**限定词**（= mem 本体跨 module）+ §4 正面补「本 ADR 的正面条目**不含** (ii)」 |
| 7 | ADR-066 正文超 250 行 ⇒ 需**超长自检**登记 | ✅ **采纳** ⇒ 元数据加「**超长自检（v1.2）**」：判定 = 非设计规格（决策 + 后果 + 前瞻判据）⇒ **不拆分**（被引面横跨 5 处 ⇒ 拆分 = 反向指针修订风暴）⇒ 对齐 ADR-067 形态 |
| 8 | 台账行 20 / 登记表的「响亮拒绝」归因描述随探针结果订正 | ⏳ **不采纳（时序）** ⇒ 探针在 **P5**（builder）⇒ ⇒ 改为在 §5 **V20** 登记**义务**（「探针结果 MUST 登记进台账行 20 **与** `.rddf/checks/arch-residual-registry.md` 的归因描述」）⇒ ⇒ 本轮**不改**两处活账（避免先写未实测的结论） |
| 9 | 替代方案 **G**（边界句只写 improvement / spec，不进 ADR） | ✅ **评估后驳回**（与 oracle 同判）⇒ 理由：闸门的权威链 = 代码注释 → ADR-060 §2.1 → ADR-066；边界句若只落在 `draft` 态 improvement ⇒ ADR 层的旧单句仍是唯一可引文本 ⇒ 「只追加不删旧句」的失效模式重新打开 |

### 8.3 critic 第 1 轮（**1 BLOCKER** + 3 SHOULD_FIX + 3 NITS · 逐条应答）

| # | finding | 处置 |
|---|---|---|
| **B1** | 判别程序 ③ 与 (iii) 分句对**形态 E**（mem 本体同 module + **非相邻**实参）给出**相反**裁决 ⇒ 「互斥且穷举」被证伪 | ✅ **采纳并修** ⇒ ③ 加**相邻性约束**（「非相邻 ⇒ (iii)」+ `:563` 的相邻性检查**不变**、位置在归属比对**之后**）+ 破口形态 E **逐字登记** + §5 **V18** 负向用例补第二条（非相邻实参 MUST 仍红）⇒ ⇒ **复审判定 `CLOSED`** |
| S1 | ④ 的口径引用「ADR-060 §2.2 G3」**错误**（G3 讲冒泡的注册层位与端口顺序，不定义折叠归属） | ✅ **采纳** ⇒ 改为「ADR-060 的『跨模块』判定口径（§2.1 `:169` · v1.2 行）/ ADR-062 v1.2 同词口径」+ 显式排除该章 ⇒ ⇒ 文件内 `§2.2 G3` 字面 = **0** |
| S2 | ADR-060 §2.1 矩阵行仍以完成时断言「✅ 已裁决：响亮拒绝」，未指向**契约-实现缺口** ⇒ 两个读者面不一致 | ✅ **采纳** ⇒ 行尾追加「⚠️ **现状实现缺口（v1.17）** … ★ 本行是**契约要求**（不是现状断言）⇒ ⇒ 登记义务 = ADR-066 §2.7 (i) + §5 **V20** · phase-6 **P5**」 |
| S3 | §2.7「理由」段的实测结论未**锚定分句** ⇒ 与 (i) 的「现状处置」并存时易被读成自相矛盾 | ✅ **采纳** ⇒ 理由段首追加「⚠️ **本段实测 = ②(ii) 形态** …；⚠️ **字面 (i)** 的现状处置**另见**机制订正 + §5 **V20** ⇒ ⇒ 两段『现状』结论**各自正确**、**不矛盾**」 |
| N1 | V20 排在 V19 前 | ✅ **采纳** ⇒ 已交换（现 **V18 / V19 / V20**） |
| N2 | 行号口径不一（`:543` vs `:542-553`） | ✅ **采纳** ⇒ 统一为「闸门**段** = `:542-553`，闸门**点** = `:543`」 |
| N3 | improvement 草案的「候选形态」降级 | ✅ **注销**（critic 确认 ADR 内登记已足 ⇒ draft 态 improvement 无需 architect 域改动）⇒ ⇒ 转为 **planner 待办**（见 §8.5） |

### 8.4 critic 第 2 轮（**复审**）

⇒ ⇒ **APPROVE**（`mergeable: yes` · `scope_exceeded: false`）· B1 **CLOSED**
⇒ ⇒ 复核要点：判别程序三分支**全部谓词化**（无自由裁量口）· 形态 E 破口逐字登记 ·
V18 负向用例两条 ⇒ 互斥性**可测** · 三处 SHOULD_FIX / 三处 NIT **逐字核到**。

### 8.5 判据自检（**当前实测**）

```bash
awk '/^### 2.7/,/^---/' docs/adr/ADR-066-hierarchical-parent-side-landing-semantics.md | grep -c '维持.*响亮拒绝'   # 0
awk '/^### 2.7/,/^---/' docs/adr/ADR-066-hierarchical-parent-side-landing-semantics.md | grep -c '实参视线'            # ≥1
awk '/^### 2.7/,/^---/' docs/adr/ADR-066-hierarchical-parent-side-landing-semantics.md | grep -c '非相邻 ⇒ (iii)'    # 1
grep -c '§2.2 G3' docs/adr/ADR-066-hierarchical-parent-side-landing-semantics.md                                  # 0
grep -c '现状实现缺口（v1.17）' docs/adr/ADR-060-hierarchical-port-policy-and-module-reuse.md                    # 1
grep -c 'mem 持有 module == 使用 module、仅`实参视线`跨层' docs/adr/ADR-060-hierarchical-port-policy-and-module-reuse.md  # ≥1
```

⚠️ **判别力的方向性**：`维持.*响亮拒绝` = 0 是**修订后**的期望值；该模式在**修订前**的 ADR 上 = **1**
（实测）⇒ ⇒ 判据**不恒真** ⇒ ⇒ 能检出「只追加新句、不删旧句」的部分修订。

### 8.6 遗留（**如实登记**）

| 项 | 归属 | 状态 |
|---|---|---|
| **字面 (i) 的现状处置**（分析放行 + 发射期静默丢端口？） | **P5（builder）** | ⏳ 待运行期探针（§5 **V20**）⇒ ⇒ 探针结果 MUST 登记进台账行 20 + `.rddf/checks/arch-residual-registry.md` 的归因描述 |
| **归属比对判定**（P5 MUST 新增）+ **(ii) 支持态的产物形态** | **P5（builder）** | ⏳ 产物形态 MUST 走**独立修订**（本 ADR 不裁定） |
| improvement 草案 `:66` / `:125` 的「走父侧落点网」降级为**候选形态** | **planner** | ⏳ 待办（architect 域**未**改） |
| phase-6 `P4` 行回填 ✅ + 证据路径 | **planner** | ⏳ 待办（`.rddf/roadmap/**` 属 planner 域） |
| **分析层 `:542` 的代码注释**（权威链的**首环**）MUST 随 P5 的判定拆分同步（分 (i)/(ii)） | **P5（builder）** | ⏳ 待办（现状写「跨 module 的 ch_mem ⇒ 响亮拒绝（ADR-060 §2.1：mem MUST 与使用者同层）」⇒ ⇒ 未区分两分句 ⇒ ⇒ 属**代码**改动 ⇒ architect **未**改） |

---

## 9. **修订 v1.3 轮**（第二次裁定 = `(ii)` 的产物形态 · 2026-11-09 · architect 域）

> **触发**：用户口令「同时开第二次 architect 裁定（产物形态 ⇒ 才能开『正确支持』）」。
> **上游**：ADR-066 **v1.2** 决策 ⑦ 明写「产物形态 MUST 走**独立修订**」⇒ ⇒ 本轮 = 该修订。
> **产物**：ADR-066 **v1.3**（§2.8 决策 ⑧ · §2.9 选项 A–G · §2.10 后果 / 代价 · §5 **V21/V22/V23** ·
> 元数据）· ADR-060 **v1.20**（§2.1 行 96 改指决策 ⑧ · **R2 表 +1 行 = 登记义务** ·
> **`en` 槽 fail-closed** · **R1 防御性注记** · §6 · 元数据）。

### 9.1 裁定内容

**`(ii)` 的产物形态 = 复用决策 ① 的父侧落点网（由**登记**触发）** ⇒ ⇒ 六条规定：
① **网合成（承重）**：mem 实参槽的跨层边 MUST **登记为父侧消费边** ⇒ 合成一根父侧 `logic` 网
（复用 `register_parent_wire` / `parent_landing_wire` / `add_conn`）；
② **命名恒等**：网名 = 驱动端点的 ctx 全局名，而 `node_names_` = **同一份**全局名表
⇒ ⇒ **codegen MUST NOT 新增「查重定向表」**（恒等映射 = 死代码）；**不登记**的后果 =
父 module **无声明** ⇒ Verilog **implicit wire（1-bit）⇒ 宽度截断 ⇒ 值错**（lint 可能仍绿）；
③ **R2 表 +1 行** = 「mem 实参槽的跨层边 MUST 登记落点网」（**登记义务**，不是 codegen 查表）；
④ MUST NOT 合成 mem 专用端口 / 第二套命名；
⑤ 落点网**只**承载实参值 ⇒ **`rdata` / `data_proxy` 跨层 ⇒ 不属 (ii)** ⇒ 仍按「mem 本体跨 module」拒；
⑥ **`en` 槽**：`nullptr` ⇒ `1'b1`（合法缺省）· 非 `nullptr` 且**无名** ⇒ **fail-closed**（MUST NOT 回退）。

### 9.2 oracle 第 1 轮（10 条 · 逐条应答）

| # | finding | 处置 |
|---|---|---|
| 1 | **重定向表的写入位置坑**：既有两处写入都是写进**子** module 的 view，而 mem 端口在**父** view 打印 ⇒ 照抄会查表落空 | ✅ 采纳（该条的**前提**后被 critic 证明不成立 —— 见 §9.3 B1）；「写入父 view」的结论**在 α 形态下不再需要**（改为**无 foreign 键**） |
| 2 | ⚠️ **事实订正**：`node_names_ = view_.names` 是**全局名表整份拷贝** ⇒ `!node_names_.count(addr)` **不会** early-return；真实失败 = 打印 `mem[<子侧全局名>]` ⇒ 父作用域不可解析 ⇒ **lint 响**（非静默）；**真正静默的是 `en` 槽**（缺名 ⇒ 回退 `1'b1`） | ✅ **采纳** ⇒ ⑧② 的失败模式**重写** + ⑧⑥ 新增 `en` fail-closed（★ 这条直接引出 critic 的 B1） |
| 3 | `print_mem_write_port` **存在且同型** ⇒ ② 必须点名 | ✅ 采纳（α 形态下改为「两者都**无需**查表」+ `en` 义务**对两者都适用**） |
| 4 | R2 粒度**按函数** ⇒ mem 侧不能写「第 5 条」单数 | ✅ 采纳（后经 B1 修正为 **+1 行 = 登记义务**，不再是按函数的查表行） |
| 5 | **R1 与 foreign 键字面冲突** ⇒ 需 carve-out | ✅ 采纳 ⇒ 最终形态 = **无 foreign 键** ⇒ carve-out 降级为**防御性注记**（将来若要写 MUST 先开显式 carve-out） |
| 6 | §2.9 补 **E（IR 别名）/ F（只支持读侧）/ G（穿透）** | ✅ 采纳（E 被 ADR-060 的「MUST NOT 改写 IR」+ 三条承重危害禁止） |
| 7 | ⑤ 补 `rdata` 边界句 | ✅ 采纳 |
| 8 | V22 措辞（变异对象）+ V21「两处」 | ✅ 采纳（后经 B1 改为「变异对象 = **网合成 / 登记侧**」） |
| 9 | 元数据登记配套修订 | ✅ 采纳（ADR-060 v1.20 + capability Requirement 走 change 流程） |
| 10 | ① 指名复用函数 | ✅ 采纳 |

### 9.3 critic 第 1 轮 ⇒ **REJECT**（1 BLOCKER）· 第 2 轮 ⇒ **APPROVE**

| # | finding | 处置 |
|---|---|---|
| **B1** | **§2.8 自相矛盾**：⑧① 的网名规则（= ctx 全局名）与 ⑧② 的失败模式（「不查表 ⇒ 父作用域不可解析 ⇒ lint 红」）**不可能同真** —— `node_names_` 是同一份全局名表 ⇒ 查表 = **恒等映射**（死代码）⇒ **V22 的变异会恒绿** | ✅ **采纳（选项 α）** ⇒ ⑧① 改「**登记 / 网合成**」为承重；⑧② 改「**命名恒等** ⇒ codegen **不查表**」+ 不登记的后果 = **implicit wire（1-bit）⇒ 宽度截断 ⇒ 值错**；⑧③ R2 **+2 → +1 行**（登记义务）；新增 ⑧⑥ `en` fail-closed；R1 carve-out 降级为防御性注记；**V22 变异对象改指登记侧** |
| S1 | improvement 草案的「待独立裁定」表述已被决策 ⑧ 取代 ⇒ planner MUST 更新（否则 P5 重开裁决 / 照字面执行已消失的分支） | ✅ **采纳** ⇒ ADR-066 元数据登记 **planner 待办** |
| S2 | ⑧② 未写「查不到怎么办」（`en`） | ✅ **采纳** ⇒ ⑧⑥ 写死（`nullptr` 合法缺省 / 有名则用 / **无名 ⇒ fail-closed**） |
| N1 / N2 | 矩阵行的 forward-facing 措辞 / R1 carve-out 加「当前无 foreign 键」 | ✅ 采纳（N2 已在 v1.20 写为**防御性注记**） |

**复审结论**：✅ **APPROVE**（`mergeable: yes` · `scope_exceeded: false`）· **B1 `CLOSED`** · 无新 BLOCKER
⇒ ⇒ 复核：① 矛盾本体已消除（新失败模式与 ⑧① 逻辑自洽：名字语法可解析为 implicit wire ⇒ 失败在**宽度**而非不可解析）；② ⑧① 的复用符号（`register_parent_wire` / `parent_landing_wire` / `add_conn`）**已 grep 核实**；③ R2 第 5 行两侧一致；④ `en` 静默缺省的**事实前提已核实**（`en_str` 初始化）；⑤ R1 降级为防御性注记；⑥ V22 变异对象已改（原恒绿变异废弃）；⑦ 配套三处互指无冲突。
⇒ ⇒ 另 2 条 NIT 已处置：① 旧措辞「纳入重定向表」在三处加**括注**（≠ 写重定向项）；② ADR-066 的 Status 行加注「其评审轮次 = **v1.0 轮**的记录」（防误读为本轮）。

### 9.4 判据自检（当前实测）

```bash
grep -c '命名恒等' docs/adr/ADR-066-hierarchical-parent-side-landing-semantics.md            # 3
grep -c 'implicit wire' docs/adr/ADR-066-hierarchical-parent-side-landing-semantics.md        # ≥1
grep -c '命名恒等' docs/adr/ADR-060-hierarchical-port-policy-and-module-reuse.md             # ≥1
grep -c 'v1.19' docs/adr/ADR-060-hierarchical-port-policy-and-module-reuse.md                # 0（全升 v1.20）
```

### 9.5 遗留（**如实登记**）

| 项 | 归属 | 状态 |
|---|---|---|
| **improvement 草案的条件化表述更新**（Status / In Scope 3 / A4 的「待独立裁定」） | **planner** | ⏳ 待办（本轮 architect **未**改 planner 域文件） |
| **ADR-060 R2 表第 5 行的实现**（登记义务）+ **`en` 槽 fail-closed** | **P5（builder）** | ⏳ 待做 |
| **V22 的变异**（不登记落点网 ⇒ 值判据转红）· **V21 值判据**（读 `addr` / 写 `wdata`·`en` 两侧） | **P5（builder）** | ⏳ 待做 |
| capability `rtl-hierarchical-output` 新增 Requirement | **P5（builder · change 流程）** | ⏳ 待做 |
