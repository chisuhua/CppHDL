---
name: hierarchical-port-landing-matrix-2026-11
priority: P1
phase: phase-5
category: test
type: chore
状态: draft
主题: 真层次化 RTL 输出（成层粒度由用户显式声明 · 默认 inline · 与扁平逐字节一致）
依赖: ADR-060, ADR-062
来源: 台账 §7 残留 16 + 裁定输入文档 docs/architecture/2026-11-08-hierarchical-parent-side-landing.md
生成时间: 2026-11-08
evidence: probe:docs/architecture/2026-11-08-hierarchical-parent-side-landing.md#2
---

# Improvement: 父侧落点形态矩阵的轴缺口补齐（族 ①/③ 的现状基线）

- **Date drafted**: 2026-11-08（**v2** —— 据 metis 第 1 轮 gap 分析**大幅收窄** scope：既有覆盖远多于初稿假设 ⇒ ⇒ 改为**扩展既有 36 格 probe matrix 的一个轴缺口**，而非新建平行矩阵）
- **Drafted by**: planner 角色（build-agent）
- **Status**: 📝 草稿 v2 ⇒ 待 `rdd-builder` P0 批准门
- **权威登记**：台账 §7 残留 **16**（`未决` · 归属 architect）· 本 improvement **不裁定**行 16，只固化其**现状基线**
- **本文件不是 ADR**：MUST NOT 修改任何已签批决策的结论（契约裁定归 architect）

**主题**: 真层次化 RTL 输出（成层粒度由用户显式声明 · 默认 inline · 与扁平逐字节一致）

## 架构依据

层次化 RTL 输出的「父层消费子 `ch_out`」有**四族形态**（分类与实测见
`docs/architecture/2026-11-08-hierarchical-parent-side-landing.md` §2.2）：

| 族 | 方向 | 消费点 / 父表达式 | 现状 | 既有仓内覆盖 |
|---|---|---|---|---|
| ① | 子 → 父 | 父内 reg / 父内 op 节点（**不含**父 output 直通） | ❌ 被拒 | ❌ **空白** |
| ② | 父 → 子 | 表达式**只含父内信号** | ✅ `ok=1` | ✅ 既有矩阵 `row1`/`row2` + `:1915` |
| ③ | 父 → 子 | 表达式含子 `ch_out` **且含运算** | ❌ 被拒 | ✅ `tests/test_rv32i_stage_dual.cpp:430-441`（真实 DUT） |
| ④ | 父 → 子（兄弟边） | 表达式 = **单个**子 `ch_out` | ✅ `ok=1` | ✅ `tests/test_hierarchy_verilog.cpp:1379-1406`（**逐条**断言父侧网 + `.port(net)`） |

⚠️ **仓内已有「有上界的 36 格 probe matrix」**（`tests/test_hierarchy_verilog.cpp:2073`）：
轴 A = 跨层**拓扑**（`row1` 父→子直接 / `row2` 子→父直接 / `third_axis` / `upward` / `sibling` / `bubble`），
轴 B = **子侧端点**类型（`ch_in` / `ch_out` / `ch_reg` / `ch_proxy` / `op` / `mem`），
三态（`Ok` / `Reject` / `Na`）+ **evidence tag 必须在源文件中真实存在**（`:2258-2278`）⇒ ⇒ 矩阵**不会静默缩水**。

⇒ ⇒ **缺口精确定位**：该矩阵的**两个轴都不表达「父侧消费点带运算」** ⇒ ⇒
`{"row2", "ch_out", Ok}` = **S1 直通**（父读子 output · 父侧无运算）⇒ ⇒ **族 ①/③ 无处可落** ⇒ ⇒ **矩阵缺一轴**。

## 范围

- **In Scope**：
  1. **扩展既有 36 格矩阵**：新增轴 A 取值 `row2_expr`（父读子端点 **且父侧带运算**）× 轴 B 的 6 个类型
     ⇒ **6 格**，每格 = 独立 DUT fixture + 独立 evidence tag ⇒ ⇒ **现状快照**（今天发射器怎么判就怎么记）。
     ⇒ ⇒ **格数上界同步**（36 → 42）与三态计数同步（`n_ok` / `n_reject` / `n_na`）。
     ⇒ ⇒ 其中 `row2_expr × ch_out` = **族 ①/③ 的真实空白格**（★ 本 improvement 的核心产出）。
  2. **粒度激活方式写进 fixture**：成层由**类级 `CH_HIERARCHY_LAYER("...")`** 宏激活
     （与既有矩阵 fixture 一致）⇒ ⇒ **新增负向对照**：同一形态在**默认粒度**
     （`RespectDeclarations`，`src/component.cpp:160`）下 ⇒ 产出**单 module**（全 inline）⇒ ⇒ 跨层形态**不出现**
     ⇒ ⇒ 这条把「这些用例到底在测什么」钉死。
  3. **S1 直通的「不造网」显式断言**（Q2 收窄后「不造网」是**合法**形态 ⇒ ⇒ MUST 被钉住）：
     `ok=1` 且产物**不**含父侧 `logic` 网 + 子 output 直接接父 output 端口 ⇒ ⇒ 挂在既有 `row2 × ch_out` 用例上。
  4. **族 ④ 的逐拍等价**：在**既有** `tests/test_hierarchy_equivalence.cpp` 目标内补一例
     （`ch::hier_eq::compare`）⇒ ⇒ 该目标已带 `verilator_e2e` 标签 ⇒ ⇒ **不新增 CTest 条目**。
     ★ 期望值 MUST **手推**（禁抄 `flat_trace`）。
  5. **判别力自证**：变异表（见 `Acceptance`）⇒ ⇒ 至少 1 条**改产品代码**的变异（临时删发射器的父侧网声明 / `.port(net)` 连接）
     ⇒ ⇒ 用例 MUST 转红 ⇒ ⇒ 还原 + **重建** ⇒ ⇒ 复绿。
- **Out of Scope**（★ 全部属**别的角色 / 别的 change**）：
  - ❌ **不实现**族 ①/③ 的支持 ⇒ ⇒ 那是行 16 的实施，**阻塞于用户对 Q1 的拍板**（architect 域）；
  - ❌ **不新建**测试文件 ⇒ ⇒ **扩展**既有矩阵（理由：复用 `module_text` / `count_occurrences` helper + evidence tag 机制
    ⇒ ⇒ **CTEST_TOTAL 不变** ⇒ ⇒ 无孤儿文件、无第二份真相）；
  - ❌ **不改** `src/` `include/`（产品代码）⇒ ⇒ 唯一例外是判别力自证的**临时**变异（实施后 MUST 还原 + 重建）；
  - ❌ **不动** `PortPolicy` 三档 ⇒ ⇒ 那是行 15（独立可并行）；
  - ❌ **不动** `tests/hier_corpus_registry.md` ⇒ ⇒ ① 新 fixture **不是**双后端语料 ⇒ ⇒ 登记会使
    `scripts/verify_hier_corpus_coverage.sh` 方向①转红；② 族 ③ 已被 `:31` 的 `ImmConsistencyDut` 登记 ⇒ ⇒ 新增即重复；
  - ❌ **不动** `tests/test_rv32i_stage_dual.cpp:430-441` ⇒ ⇒ 它断言的过时归因属**行 17**（阻塞于 Q1）；
  - ❌ **不改** spec / ADR / 台账状态 / `openspec/specs/**` ⇒ ⇒ 契约裁定与残留状态变更归 architect；
  - ❌ **不新建** `improvement-suggestions.md`（本仓无此索引文件且无消费方 ⇒ ⇒ 新建即孤儿文件）。

## Why

1. **族 ①/③ 是行 16 裁定的对象，而它们的现状无任何仓内判据** ⇒ ⇒ 裁定的「现状基线」目前只在仓外探针
   ⇒ ⇒ 谁都无法在 CI 上确认「今天的行为是什么」⇒ ⇒ 裁定与后续实施都缺锚。
2. **矩阵缺一轴 ⇒ 「父侧带运算」这一维度在整个仓内不可表达** ⇒ ⇒ 任何依赖「父侧运算」语义的改动
   （行 15 / 行 16）都**没有落点**⇒ ⇒ 必须先把这一轴补上。
3. **变更可见**：把族 ①/③「当前被拒」固化为矩阵格 ⇒ ⇒ 行 16 若实施（B），该格从 `Reject` 变 `Ok`
   ⇒ ⇒ 这是**预期的可见变更**（矩阵是活的现状快照）⇒ ⇒ 而非静默语义漂移。
4. **不是「加测试」而是「给裁定一个可信基线」**：这批格子的首要读者是**架构裁定者**，其次才是回归护栏。

## What Changes

1. `tests/test_hierarchy_verilog.cpp`：
   - 新增 6 个 DUT fixture（`MatRow2Expr*`）⇒ 每个带 `CH_HIERARCHY_LAYER` ⇒ 每个**端口单一驱动**；
     ⇒ 其中至少一个用 `ch_bool` / 1-bit（对齐真实站点 `hazard.io().ex_is_load <<= id_stage.io().is_load;`），
       且**复刻**真实站点写法 `select(cond, child_out, …)`（见 `rv32i_pipeline.h:398`）。
   - 矩阵新增 site `row2_expr` 的 6 格 + 同步 `cells.size()` 上界与三态计数 + 每格 evidence tag
     （★ 复用既有 `[hierarchy][emit][matrix]` tag ⇒ ⇒ **不新增 CTest 条目 ⇒ `CTEST_TOTAL` 不变**）。
   - 每格的**行为断言**：`Ok` ⇒ `ok==true` + 产物形态断言；`Reject` ⇒ **`ok==false` + 文件不存在**
     + 错误非空且**点名子 module key**；`Na` ⇒ 有理由且不是 tag。
     ⚠️ **MUST NOT** 断言诊断**整串**（行 17 的过时刻固化即此类 ⇒ 且 Q3 会改这段诊断 ⇒ 断言整串必然假红）。
   - 「作用域锚定」算法：用既有 `module_text(all, "<parent>")` 提取**父 module body** ⇒ 在该 body 内断言
     网声明 / `.port(net)` / 消费引用 ⇒ ⇒ 并**显式反断言**同一网名**不**出现在子 module body 内
     （防「子串误配」判绿；⚠️ `all.find(...)` 不算作用域锚定）。
   - 新增**默认粒度负向对照** fixture（不带 `CH_HIERARCHY_LAYER`）⇒ 断言产出**单 module**。
   - 在既有 `row2 × ch_out` 用例上补「**不造网**」显式断言（S1）。
2. `tests/test_hierarchy_equivalence.cpp`（既有目标，已带 `verilator_e2e`）⇒ 补族 ④ 的逐拍等价一例
   ⇒ ★ 期望 trace 以**字面量表**写在源码 + 推导注释完整 ⇒ ⇒ 与 `flat_trace` **分开断言**。
3. **零锚点变化**：`CTEST_TOTAL` / `VERILATOR_E2E_TOTAL` / `DUAL_BACKEND_PARITY_*` **均不变**
   ⇒ ⇒ 不新增 CTest 条目 ⇒ ⇒ `scripts/verify_test_counts.sh` 的锚点核对不受影响。
4. 判别力**变异表**（每行：改动位置 → 编译确认 → **重建** → 预期红 → 还原 + **重建** → 预期绿）：

   | # | 变异 | 预期 |
   |---|---|---|
   | M1 | 临时删发射器里**父侧网声明**（`print_decl` 一处） | 族 ④ 用例**红**（网缺失） |
   | M2 | 临时把族 ④ 的 `.port(net)` 改成 `.port(parent_out)` | 族 ④ 用例**红**（形态不符） |
   | M3 | 把 `row2_expr × ch_out` 的 fixture 换成 `row2_expr × ch_reg` 写法 | 该格**红**（形态分类错位 ⇒ 互斥性判据） |

   ⚠️ 每条 MUST **先确认仍能编译**（编译失败 ⇒ 跑的是旧二进制 ⇒ 假绿）⇒ ⇒ 且还原后 MUST **重建**
   （★ 「二进制也是状态」）。

## Acceptance

- [ ] 既有 36 格矩阵**扩展**为含新 site `row2_expr` 的矩阵 ⇒ `cells.size()` 上界与三态计数**同步更新**
      ⇒ ⇒ 且**每格**的 evidence tag 在源文件中**真实存在**（既有 tag 校验机制仍生效）；
- [ ] `row2_expr × ch_out` 格 = **族 ①/③ 的现状**（`Reject` 或 `Ok`，以实测为准）⇒ ⇒ 且该格**独立可辨**
      （M3 变异 MUST 转红 ⇒ ⇒ 证明它没被误写成别的族）；
- [ ] 每个 `Reject` 格断言：**`ok == false`** + **产物文件不存在** + 错误非空且**点名子 module key**
      ⇒ ⇒ **MUST NOT** 断言诊断整串；
- [ ] 每个 `Ok` 格断言**产物形态**（父侧网声明 / `.port(net)` / 父侧表达式读该网）⇒ ⇒ 且用
      `module_text` 提取父 body 做**作用域锚定** + **显式反断言**同一网名不出现在子 module body；
- [ ] 新增**默认粒度负向对照**：同一形态在默认粒度下产出**单 module** ⇒ ⇒ 证明跨层形态只在成层声明下出现；
- [ ] S1 直通（`row2 × ch_out`）有「**不造网**」显式断言（`ok==1` 且产物无父侧网）；
- [ ] 族 ④ 的逐拍等价在**既有** `test_hierarchy_equivalence` 目标内 ⇒ ★ 期望 trace 为**源码内字面量表**
      + 推导注释 ⇒ ⇒ 与 `flat_trace` 分开断言；
- [ ] 变异表 M1–M3 逐条：**编译确认 → 重建 → 红 → 还原 → 重建 → 绿** ⇒ ⇒ 全程有据；
- [ ] **零产品代码改动**：`git diff --stat -- src include` 为空（唯一例外 = M1/M2 的**临时**变异，实施后 MUST 还原）；
- [ ] **零锚点变化**：`ctest -N` 总数与 `AGENTS.md` 的 `CTEST_TOTAL` **仍相等**，
      `VERILATOR_E2E_TOTAL` 不变 ⇒ ⇒ `bash scripts/verify_test_counts.sh` 转绿
      （⚠️ 该脚本在 build 目录缺 `CTestTestfile.cmake` 时只 warning + `exit 0` ⇒ ⇒ MUST 先 `cmake -B build` + 构建再跑）。

## Capabilities

- **不新增** capability；**不修改** `openspec/specs/**` 与 `docs/adr/**`；
- 本 improvement 只把**已存在的**契约（spec `:241-246` 造网 Scenario + ADR-062 §2.6 落点表）变成
  **可机械判定的产物级判据** ⇒ ⇒ MUST NOT 复述契约成第二份真相；
- ⚠️ **Q2（收窄 spec `:241-246`）的文本修改归 architect** ⇒ ⇒ 本轮只钉住「不造网是合法形态」这一**事实**，
  **不**改规范文本；
- **判别力归属** = 既有 capability `hierarchical-corpus-regression` 的登记机制 ⇒ ⇒ 但本轮**不新增登记**
  （理由见 `Out of Scope`）。

## Impact

- **产品代码**：`src/` `include/` **零改动** ⇒ ⇒ 无回归风险面（唯一例外 = 判别力自证的临时变异，已还原）；
- **测试**：`tests/test_hierarchy_verilog.cpp`（矩阵扩轴 + fixture + 断言）· `tests/test_hierarchy_equivalence.cpp`（补 1 例）；
- **CI**：**零新增 CTest 条目** ⇒ ⇒ `CTEST_TOTAL` / `VERILATOR_E2E_TOTAL` **不变** ⇒ ⇒ 无锚点漂移风险；
- **下游**：行 15 与行 16 可**直接复用**这批格子与基线 ⇒ ⇒ 避免重复建设；
- **风险**：
  ① 矩阵把「今天的行为」固化 ⇒ ⇒ 行 16 若实施（B）⇒ 该格**转红是预期的**（矩阵是活的现状快照）⇒ ⇒ MUST 在矩阵注释写明；
  ② 逐拍等价 fixture 若不把被消费值**引到顶层可观测** ⇒ ⇒ 会被机械拒绝（`ch::hier_eq` 的
     `observation-not-visible` 前缀，见 `include/hierarchy_equivalence.h:43-57`）⇒ ⇒ MUST 引到顶层；
  ③ 本仓纪律：「两后端 parity 对同源缺陷结构性失明」⇒ ⇒ 期望值 MUST **手推**（禁抄仿真输出）。
