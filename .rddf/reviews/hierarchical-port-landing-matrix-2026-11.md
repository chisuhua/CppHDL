# 审稿记录：父侧落点形态矩阵的轴缺口补齐（T8 · improvement 草稿）

- **审稿对象**：`.rddf/improvements/hierarchical-port-landing-matrix-2026-11.md`（planner 起草的 improvement 5 段草稿）
- **关联残留**：台账 §7 行 16 · 裁定输入文档 `docs/architecture/2026-11-08-hierarchical-parent-side-landing.md`
- **审查者**：**metis**（planner 的固定审查者）· 预算 **1 轮** ⇒ metis 无 BLOCKER 概念 ⇒ 逐条处置后放行
- **HARD-GATE**：`pre_create_brainstorm_check.sh` ⇒ **exit 0** ✅（v1 与 v2 均通过）

---

## 1. metis 第 1 轮 —— 核心判断

⚠️ **v1 草稿的事实前提被证伪**：v1 写「四族判别力**全部**落在仓外探针」「**没有任何测试**断言族 ④ 产物形态」
⇒ ⇒ 实测仓内**已有**大量覆盖：

| 族 | v1 的假设 | 实测 | 出处 |
|---|---|---|---|
| ④ | 仓外 | ✅ **逐条**断言父侧网 + `.port(net)` | `tests/test_hierarchy_verilog.cpp:1379-1406` |
| ③ | 仓外 | ✅ 真实 DUT `ImmConsistencyDut` + `hier_eq::emit(ForceLayer)` | `tests/test_rv32i_stage_dual.cpp:430-441`（+ 登记 `tests/hier_corpus_registry.md:31`） |
| ② | 仓外 | ✅ 既有 | `tests/test_hierarchy_verilog.cpp:1915` / `:1947` |
| ① | 仓外 | ❌ **确为空白** | —— |

⇒ ⇒ **v1 的 scope 大部分是重复建设** ⇒ ⇒ v2 收窄为「**扩展既有 36 格 probe matrix 的一个轴缺口**」。

⇒ ⇒ **缺口的精确定位**（metis 与 architect 独立得出同一结论）：
既有矩阵（`tests/test_hierarchy_verilog.cpp:2073`）轴 A = 跨层**拓扑**、轴 B = **子侧端点**类型
⇒ ⇒ **两个轴都不表达「父侧消费点带运算」**
⇒ ⇒ `{"row2", "ch_out", Ok}` = **S1 直通**（父读子 output · 父侧**无**运算）⇒ ⇒ 族 ①/③ 无处可落 ⇒ ⇒ **矩阵缺一轴**。

---

## 2. 逐条处置（9 MUST + 4 SHOULD + 4 NIT）

| # | 级别 | finding | 处置 | v2 落点 |
|---|---|---|---|---|
| 1 | MUST | Why §1 的事实前提为假 ⇒ scope 大部分是重复建设 | **采纳** ⇒ 重写 Why + In Scope；明确写出族 ②/④/③ 已有覆盖的出处 | `架构依据` 表新增「既有仓内覆盖」列 · `Why` |
| 2 | MUST | **粒度机制缺位** ⇒ 用例语义不可判定（默认 `RespectDeclarations` ⇒ 全 inline ⇒ 四族根本不出现） | **采纳** ⇒ 写明成层由**类级 `CH_HIERARCHY_LAYER`** 激活 + **新增默认粒度负向对照**（断言产出单 module） | `范围` In Scope ② · `What Changes` ① 末条 · `Acceptance` |
| 3 | MUST | **诊断子串断言既脆弱又与 Q3 冲突**（Q3 是无条件义务 ⇒ 会改这段诊断 ⇒ 与 Q1 裁法无关地必然变红） | **采纳** ⇒ **删除全部诊断措辞断言** ⇒ 只断言 `ok==false` + 文件不存在 + 错误非空且**点名子 module key** ⇒ ⇒ 「可行动性」整体留给 Q3 / 行 17 | `What Changes` ① · `Acceptance` |
| 4 | MUST | `tests/hier_corpus_registry.md` 的更新会**踩护栏或空转**（方向①要求登记 DUT 是双后端语料 ⇒ 新 fixture 必红；只写散文则解析器不核验） | **采纳** ⇒ **删除该步骤** + 写入 `Out of Scope` 并说明两条理由 | `范围` Out of Scope |
| 5 | MUST | AC 缺 fail-closed / 作用域锚定 / 手推期望值的**可执行算法** | **采纳** ⇒ ① `ok==false && 文件不存在`；② 用既有 `module_text(all, "<parent>")` 提取父 body 断言 + **显式反断言**同一网名不在子 body（`all.find` 不算作用域锚定）；③ 期望 trace **字面量表**写在源码 + 与 `flat_trace` 分开断言 | `What Changes` ①·② · `Acceptance` |
| 6 | MUST | 锚点/标签更新不完整（`LABELS "base;verilator_e2e"` / `RESOURCE_LOCK` / `TIMEOUT`）+ `verify_test_counts.sh` 在 build 目录缺件时**假绿** | **采纳（因 scope 收窄而消解）** ⇒ v2 **不新增 CTest 条目** ⇒ ⇒ 零锚点变化 ⇒ ⇒ 并保留「先 `cmake -B build` + 构建再跑该脚本」的前置 | `What Changes` ③ · `Acceptance` 末项 |
| 7 | MUST | 变异无判别力目标、无**重建**纪律 | **采纳** ⇒ 给出**变异表 M1–M3**（含 **2 条改产品代码**：删父侧网声明 / 改 `.port(net)` 为 `.port(parent_out)`）+ **1 条互斥性**（族 ① fixture 换成族 ③ 写法 ⇒ 必红）+ 「还原后 MUST **重建**」 | `What Changes` ④ · `Acceptance` |
| 8 | MUST | 与既有 36 格矩阵的关系未定 ⇒ helper 会重复 ⇒ 「矩阵」一词还会与既有 probe matrix 冲突 | **采纳** ⇒ **扩展**既有矩阵（理由：复用 `module_text` / `count_occurrences` + evidence tag + `CTEST_TOTAL` 不变 + 无第二份真相）⇒ ⇒ **不新建文件** | `范围` In/Out · `What Changes` ① |
| 9 | MUST | S1 / Q2 与族 ① 的关系未写 | **采纳** ⇒ In Scope 增「S1 直通：`ok=1` 且**不造网**」的显式断言（Q2 收窄后唯一需守的成功形态）+ Q2 的**文本修改归 architect** | `范围` ③ · `Capabilities` |
| 10 | SHOULD | 只有 8-bit 形态 | **采纳** ⇒ 至少一个 `ch_bool` / 1-bit fixture（对齐真实站点 `hazard.io().ex_is_load`） | `What Changes` ① |
| 11 | SHOULD | 真实站点与合成 fixture 脱节 | **采纳** ⇒ 复刻真实站点写法 `select(cond, child_out, …)`（`rv32i_pipeline.h:398`）+ 在 fixture 注释说明为何用合成 fixture（真实顶层先被跨模块 `ch_mem` 遮蔽 ⇒ 行 16 记载） | `What Changes` ① |
| 12 | SHOULD | `ch::hier_eq` 的观测面 / 命名耦合 | **采纳（降级）** ⇒ 「网**存在**」的断言独立于 `signal_map`（不能从同一张表自证）；命名耦合在实现期以 `signal_map` 的 `hier` 位置定位 | `Acceptance` 逐拍等价项 |
| 13 | SHOULD | 「扇出 / 多消费点」与「每端口单一驱动」混为一谈 | **采纳** ⇒ 区分：前者是 Q4（B 档义务，本轮**不测**）；后者是**构造纪律** ⇒ ⇒ 多消费点扇出在 A 档下各自被拒 | `范围` / `What Changes` ① |
| 14 | NIT | 文件名与既有 36 格 "probe matrix" 混淆 | **消解** ⇒ v2 **不新建文件** | —— |
| 15 | NIT | `Out of Scope` 里「不新建 `improvement-suggestions.md`」是防御性噪声 | **保留（附理由）** ⇒ 本仓**确无**该索引文件且无消费方 ⇒ ⇒ 新建即孤儿文件（`ZERO-DEBT POLICY`）⇒ ⇒ 保留并写明理由 | `范围` Out of Scope |
| 16 | NIT | `Capabilities` 的「不修改规范内容」不可核查 | **采纳** ⇒ 改为「不改 `openspec/specs/**` 与 `docs/adr/**`」 | `Capabilities` |
| 17 | NIT | Impact 风险②「引到顶层可观测」不必自行论证 | **采纳** ⇒ 改为**引用**既有机械护栏（`ch::hier_eq` 的 `observation-not-visible` 前缀，`include/hierarchy_equivalence.h:43-57`） | `Impact` 风险② |

---

## 3. architect 域独立复核（与 metis 结论一致）

architect 侧**独立**核对了 metis 引用的关键行号（不盲信 subagent 引用）：

| 引用 | 核对结果 |
|---|---|
| `tests/test_hierarchy_verilog.cpp:1379-1406` | ✅ 确认 = 「sibling edges go through a parent-module wire」用例 ⇒ 逐条断言父 module 的 `logic [7:0] top_a_unnamed_output;` + `.top_a_unnamed_output(top_a_unnamed_output)` + `.top_b_unnamed_input(top_a_unnamed_output)` ⇒ **族 ④ 已覆盖** |
| `tests/test_hierarchy_verilog.cpp:145-153` | ✅ 确认 = `T3` fixture（`io().dout <<= mid.io().dout;`）⇒ **S1 直通已有 fixture** |
| `tests/test_hierarchy_verilog.cpp:2073` | ✅ 确认 = 「probe matrix is complete and every cell is decided」⇒ 轴 A = 6 拓扑 · 轴 B = 6 子侧端点类型 · 三态 + `cells.size()==36` + evidence tag 校验（`:2258-2278`） |
| `tests/test_hierarchy_verilog.cpp:1915` | ✅ 确认 = 「row-1 implicit input port is synthesized」⇒ **行 1**（父→子 · 合成 `__if_0`）⇒ ⚠️ **不等于族 ②**（族 ② = 父表达式 → 子 `ch_in` 端口）⇒ ⇒ v2 保留族 ② 的一格为**待实测**（不假称已覆盖） |
| `include/component.h:36` | ✅ 确认 `CH_HIERARCHY_LAYER` 宏（类级成层声明）⇒ 成层激活方式确定 |
| `src/component.cpp:160` | ✅ 确认 `thread_local Granularity g_granularity = Granularity::RespectDeclarations;` ⇒ 默认全 inline ⇒ **负向对照成立** |
| `include/hierarchy_equivalence.h:132` | ✅ 确认 `emit(...)` 默认 `RespectDeclarations` |

⇒ ⇒ **结论一致**：T8 = 扩展既有矩阵 + 补一轴，**零产品代码改动、零 CTest 条目新增**。

---

## 4. 验证结果

| 项 | 结果 |
|---|---|
| HARD-GATE `pre_create_brainstorm_check.sh` | **exit 0** ✅ |
| 仓内产品代码改动 | `src/` `include/` = **0** ✅（`git diff --stat -- src include` 为空） |
| CTest 条目变化 | **0**（`CTEST_TOTAL` 253 / `VERILATOR_E2E_TOTAL` 77 **均不变**）✅ |
| 台账护栏 `scripts/verify_arch_ledger_residuals.sh` | **PASS**（18 行 · exit 0）✅ |
| 语料护栏 `scripts/verify_hier_corpus_coverage.sh` | **PASS**（exit 0）✅（★ 本 change **不动**语料登记表 ⇒ 必须仍绿） |
| 产物级护栏 `scripts/verify_hierarchical_rtl.sh` | **PASS**（R1–R8 全绿 · exit 0）✅ |
| `scripts/verify_test_counts.sh` | **exit 0** ✅ |
| `tests/test_hierarchy_verilog` | **946 assertions / 34 test cases 全绿** ✅ |
| 全量 `ctest -LE perf` | **242/242 通过**（含 77 个 `verilator_e2e` · exit 0）✅ —— ★ **最终版**（第四态 `Broken` 恢复后 + M3 还原后）复跑确认 |
| 判别力自证 **M3**（互斥性） | 变异 → **红**（`:2452`）⇒ ⇒ 还原 + **重建** → **绿**（946 assertions）✅ |
| 未提交 | 是（按路径显式 `git add`，等用户确认） |

---

## 5. ★ 实施期新发现：**「父侧运算消费子成员非端口端点」⇒ 放行但产物不可编译**（**新缺陷**）

### 5.1 事实（实测 · 2026-11-08 · 仓外探针 `probe_row2expr` + 仓内用例）

| 形态 | `ok` | 产物 | Verilator lint |
|---|---|---|---|
| 父读子 **ch_out** 端口 + 父侧运算 | ❌ 0 | ——（无产物） | —— |
| 父读子 **ch_reg** / **bundle 字段** / **op 节点** + 父侧运算 | ✅ 1 | 父 module 内 `assign top_or_or = top_leaf__reg \| 8'h1;` ⇒ ⇒ 引用**子 module 内部信号名** | ❌ **`Can't find definition of variable`** |
| 父读子 **ch_reg**（**无**父侧运算 · 既有 `row2` 行） | ✅ 1 | 父侧读**合成端口名** ⇒ ⇒ 产物**干净** | ✅ 干净 |

⇒ ⇒ **触发条件唯一** = 「父侧带运算」⇒ ⇒ **C++ 侧合法**（无 CHREQUIRE 失败）⇒ ⇒ 是**发射器缺陷**。

### 5.2 契约关系

⚠️ **违反 ADR-060 R2**（`:86-112`「**所有** `src(0)` 解析 / 节点穿透路径 MUST 先查重定向表」）
⇒ ⇒ 不是「未定义行为」，是**契约违反**。

⚠️ 与台账行 16 是**同族的两条路径**：行 16 = 「子侧端点是 `ch_out` 端口 ⇒ 被拒」；
本条 = 「子侧端点是**非端口端点** ⇒ 放行但产物坏」⇒ ⇒ ⇒ **候选 B 的实现 MUST 同时覆盖两条**
⇒ ⇒ **候选 B 的范围比裁定文档 §6 估的更大**（⚠️ 需 architect 在写 ADR 时纳入）。

### 5.3 严重度与处置

- **中**：① 仓内**零实例**（`ch_out` 路径被拒 ⇒ 不产生坏产物）；② 形态**极常见**；
  ③ **非静默数据损坏**（lint 直接失败 ⇒ 用户看得见）⇒ ⇒ 但发射器**本应 fail-closed**。
- 本 change **不修**（修它 = 行 16 的实施域 + 需先裁定）⇒ ⇒ 只做三件事：
  ① 矩阵**第四态 `Broken`** 显式记录（不静默记成 `Ok`）；
  ② **现状锁定断言**（缺陷修复后**预期转红** ⇒ ⇒ 届时 MUST 改写）；
  ③ **登记为新残留行**（建议 id **19** · 归属 architect · 复核条件 =
  「父侧运算消费子非端口端点的产物 MUST 可编译（或响亮拒绝）」）⇒ ⇒ **待 architect 登记**（本 change 未改登记表）。

### 5.4 一处过程纠正（如实记录）

⚠️ 实施中期我曾把「父层对子成员 `ch_reg` 做运算」判为「C++ 侧非法（CHREQUIRE 失败）」⇒ ⇒ 据此把
三格改判为 `Na` ⇒ ⇒ **该判断被实测证伪**（探针输出无任何 `[ERROR]`；那 2 条 `[ERROR]` 来自**既有用例**，
`git stash` 对照确认改动前后**同为 2 条** ⇒ ⇒ 本次改动**净增 0 条**）⇒ ⇒ 已**回滚**为 `Broken`
⇒ ⇒ 记录在此以免后人重复该误判。

---

## 6. 遗留（**未完成项** ⇒ 明确交用户裁决）

| 项 | 状态 | 原因 |
|---|---|---|
| **T3** 族 ④ 的逐拍等价（既有 `test_hierarchy_equivalence` 目标内补 1 例） | ✅ **已完成**（第二轮续做） | `EqSiblingTop`（A 的 `ch_out` → B 的 `ch_in`，父侧网中转）+ S1 直通引到顶层可观测 ⇒ ⇒ **手推期望 {17,90,165,255} 逐拍吻合** · 15 assertions · 该 target 由 414/9 → **429/10** |
| **T4-M1**（删发射器的父侧网登记 ⇒ 族 ④ 红） | ✅ **已完成** | 变异 → **红（`:1497`）** ⇒ 还原 + **重建** → 绿 |
| **T4-M2**（被驱动端实参由父侧网改为目标节点自体名 ⇒ 族 ④ 红） | ✅ **已完成** | 变异 → **红（`:1503`）** ⇒ 还原 + **重建** → 绿 |
| **T4-M3**（互斥性变异） | ✅ 已完成（第一轮） | 变异 → 红（`:2452`）⇒ ⇒ 还原 + 重建 → 绿 |
| ★ **产品代码零改动复核** | ✅ | `git diff --stat -- src include` = **空**（两变异均已还原 + 重建） |
| **T6** 归档（`openspec validate` + `--skip-specs` + commit） | ⏸ 待做 | T3/T4 已全部完成 ⇒ ⇒ 仅剩归档与提交（commit 需用户确认） |
| **行 19 登记**（§5.3 ③） | ❌ 未做 | **architect 域**（残留登记表归 architect）⇒ ⇒ 需切 architect 或由用户决定 |
| **improvement 草稿同步（v3）** | ❌ 未做 | **planner 域** ⇒ ⇒ v2 写的是「6 格 + 实测填入」，**未预见第四态 `Broken`** ⇒ ⇒ 需把「第四态 + §6.1 缺陷 + 行 19 登记」回填 v2 |
| **裁定输入文档的补充**（§5.2 ⇒ 候选 B 的范围变化） | ❌ 未做 | **architect 域** ⇒ ⇒ 同上 |

⇒ ⇒ **本 change 目前未达归档条件**（T3 / T4 未完成）⇒ ⇒ 下一轮续做即可（探针与 fixture 已在仓内）。
