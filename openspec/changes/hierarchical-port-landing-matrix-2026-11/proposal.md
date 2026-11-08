# Proposal: 父侧落点形态矩阵的轴缺口补齐（族 ①/③ 的现状基线）

> **无 spec 变更声明**：本 change **不修改**任何 capability 的规范内容
> ⇒ ⇒ `openspec/changes/hierarchical-port-landing-matrix-2026-11/specs/` **不存在**
> ⇒ ⇒ 归档时**允许** `--skip-specs`（依据：`AGENTS.md` §OPENSPEC WORKFLOW 的例外条款
> 「除非该 change 显式声明 "no spec changes"」）。
>
> **权威来源**：`.rddf/improvements/hierarchical-port-landing-matrix-2026-11.md`（improvement 草稿 v2 ·
> 已过 metis 第 1 轮 gap 分析 + `pre_create_brainstorm_check.sh` HARD-GATE）·
> 审稿记录 = `.rddf/reviews/hierarchical-port-landing-matrix-2026-11.md`。

## Why

### 1. 缺口：**父侧「带运算」这一维度在仓内不可表达**

层次化 RTL 输出的「父层消费子 `ch_out`」有**四族形态**（分类与实测见
`docs/architecture/2026-11-08-hierarchical-parent-side-landing.md` §2.2）：

| 族 | 方向 | 消费点 / 父表达式 | 现状 | 既有仓内覆盖 |
|---|---|---|---|---|
| ① | 子 → 父 | 父内 reg / 父内 op 节点（**不含**父 output 直通） | ❌ 被拒 | ❌ **空白** |
| ② | 父 → 子 | 表达式**只含父内信号** | ✅ `ok=1` | ⚠️ 部分（见 design §1.2） |
| ③ | 父 → 子 | 表达式含子 `ch_out` **且含运算** | ❌ 被拒 | ✅ `tests/test_rv32i_stage_dual.cpp:430-441` |
| ④ | 父 → 子（兄弟边） | 表达式 = **单个**子 `ch_out` | ✅ `ok=1` | ✅ `tests/test_hierarchy_verilog.cpp:1379-1406` |

仓内已有**有上界的 36 格 probe matrix**（`tests/test_hierarchy_verilog.cpp:2073`）：

- 轴 A = 跨层**拓扑**（`row1` / `row2` / `third_axis` / `upward` / `sibling` / `bubble`）
- 轴 B = **子侧端点**类型（`ch_in` / `ch_out` / `ch_reg` / `ch_proxy` / `op` / `mem`）
- 三态（`Ok` / `Reject` / `Na`）+ **evidence tag 必须在源文件中真实存在**（`:2258-2278`）

⇒ ⇒ **两个轴都不表达「父侧消费点带运算」** ⇒ ⇒ `{"row2", "ch_out", Ok}` = **S1 直通**
（父读子 output · 父侧**无**运算）⇒ ⇒ **族 ①/③ 在矩阵里无处可落** ⇒ ⇒ **矩阵缺一轴**。

### 2. 为什么现在必须补

1. **族 ①/③ 是台账行 16 的裁定对象**（`未决` · 归属 architect）⇒ ⇒ 裁定需要「今天的行为是什么」的
   **可核查基线**；目前它只在仓外探针里 ⇒ ⇒ 谁都无法在 CI 上确认。
2. **行 15 与行 16 的实施都依赖「父侧带运算」这一维** ⇒ ⇒ 没有这一维，改动**没有落点**。
3. **变更可见**：把族 ①/③「当前被拒」固化为矩阵格 ⇒ ⇒ 行 16 若实施（B），该格从 `Reject` 变 `Ok`
   ⇒ ⇒ 这是**预期的可见变更**（矩阵是活的现状快照），而非静默语义漂移。

### 3. 边界：本 change **不裁定**行 16

本 change **只固化现状基线** ⇒ ⇒ **不**实现族 ①/③ 的支持（那是行 16 的实施，阻塞于用户对 Q1 的拍板，
属 architect 域）⇒ ⇒ **不**改 spec / ADR / 台账状态 ⇒ ⇒ **零产品代码改动**。

## What Changes

1. **扩展既有 36 格 probe matrix**（`tests/test_hierarchy_verilog.cpp`）：
   - 新增轴 A 取值 `row2_expr`（父读子端点 **且父侧带运算**）× 轴 B 的 6 个类型 ⇒ **6 格**；
   - 同步 `cells.size()` 上界与三态计数（`n_ok` / `n_reject` / `n_na`）；
   - 每格 = 独立 DUT fixture + 独立 evidence tag（复用既有 `[hierarchy][emit][matrix]` tag
     ⇒ ⇒ **不新增 CTest 条目** ⇒ ⇒ `CTEST_TOTAL` / `VERILATOR_E2E_TOTAL` **不变**）。
2. **粒度激活 + 负向对照**：成层由**类级 `CH_HIERARCHY_LAYER("...")`** 宏激活 ⇒ ⇒ 新增一个
   **不带该宏**的对照 fixture ⇒ ⇒ 断言默认粒度（`RespectDeclarations`）下产出**单 module**
   ⇒ ⇒ 把「这些用例到底在测什么」钉死。
3. **S1 直通的「不造网」显式断言**：Q2（收窄 spec `:241-246` 的造网要求）落地后，
   「不造网」是**合法**形态 ⇒ ⇒ MUST 被钉住（挂在既有 `row2 × ch_out` 用例上）。
4. **族 ④ 的逐拍等价**：在**既有** `tests/test_hierarchy_equivalence.cpp` 目标内补一例
   （`ch::hier_eq::compare`）⇒ ⇒ 期望值 MUST **手推**（禁抄 `flat_trace`）。
5. **判别力自证**：变异表 M1–M3（含 **2 条改产品代码** + **1 条互斥性**）⇒ ⇒ 每条
   「编译确认 → **重建** → 红 → 还原 → **重建** → 绿」。

### Out of Scope

- ❌ 不实现族 ①/③ 的支持（行 16 · 阻塞于 Q1 拍板 · architect 域）；
- ❌ **不新建**测试文件 ⇒ ⇒ **扩展**既有矩阵（避免第二份真相 + 孤儿文件）；
- ❌ 不改 `src/` `include/`（唯一例外 = 变异自证的**临时**改动，实施后 MUST 还原 + 重建）；
- ❌ 不动 `PortPolicy` 三档（行 15 · 独立可并行）；
- ❌ 不动 `tests/hier_corpus_registry.md`（新 fixture 不是双后端语料 ⇒ 登记会使
  `scripts/verify_hier_corpus_coverage.sh` 方向①转红；且族 ③ 已被 `:31` 登记 ⇒ 新增即重复）；
- ❌ 不动 `tests/test_rv32i_stage_dual.cpp:430-441`（其过时归因属行 17 · 阻塞于 Q1）；
- ❌ 不改 `openspec/specs/**` / `docs/adr/**` / 台账状态。

## Impact

- **产品代码**：`src/` `include/` **零改动** ⇒ ⇒ 无回归风险面；
- **测试**：`tests/test_hierarchy_verilog.cpp`（矩阵扩轴 + fixture + 断言）·
  `tests/test_hierarchy_equivalence.cpp`（补 1 例）；
- **CI**：**零新增 CTest 条目** ⇒ ⇒ 锚点 `CTEST_TOTAL` / `VERILATOR_E2E_TOTAL` **不变**；
- **下游**：行 15 与行 16 可直接复用这批格子与基线；
- **风险**：① 矩阵固化「今天的行为」⇒ ⇒ 行 16 若实施（B）⇒ 该格转红是**预期的**（MUST 在矩阵注释写明）；
  ② 逐拍等价 fixture MUST 把被消费值**引到顶层可观测**（否则被 `ch::hier_eq` 的
  `observation-not-visible` 机械拒绝）；③ 「两后端 parity 对同源缺陷结构性失明」⇒ ⇒ 期望值 MUST **手推**。

## 验收标准

- [ ] 既有 36 格矩阵**扩展**为含新 site `row2_expr` 的矩阵 ⇒ `cells.size()` 上界与三态计数**同步更新**
      ⇒ 且**每格**的 evidence tag 在源文件中**真实存在**；
- [ ] `row2_expr × ch_out` 格 = **族 ①/③ 的现状**（以实施期实测为准）⇒ 且该格**独立可辨**
      （M3 变异 MUST 转红 ⇒ 证明它没被误写成别的族）；
- [ ] 每个 `Reject` 格断言：**`ok == false`** + **产物文件不存在** + 错误非空且**点名子 module key**
      ⇒ **MUST NOT** 断言诊断整串；
- [ ] 每个 `Ok` 格断言**产物形态**（父侧网声明 / `.port(net)` / 父侧表达式读该网）⇒ 且用 `module_text`
      提取父 body 做**作用域锚定** + **显式反断言**同一网名不出现在子 module body；
- [ ] 新增**默认粒度负向对照**：同一形态在默认粒度下产出**单 module**；
- [ ] S1 直通（`row2 × ch_out`）有「**不造网**」显式断言；
- [ ] 族 ④ 的逐拍等价在**既有** `test_hierarchy_equivalence` 目标内 ⇒ 期望 trace 为**源码内字面量表**
      + 推导注释 ⇒ 且与 `flat_trace` 分开断言；
- [ ] 变异表 M1–M3 逐条：**编译确认 → 重建 → 红 → 还原 → 重建 → 绿**；
- [ ] **零产品代码改动**：`git diff --stat -- src include` 为空（唯一例外 = M1/M2 的临时变异，已还原）；
- [ ] **零锚点变化**：`ctest -N` 总数与 `AGENTS.md` 的 `CTEST_TOTAL` 仍相等，`VERILATOR_E2E_TOTAL` 不变
      ⇒ ⇒ `bash scripts/verify_test_counts.sh` 转绿（⚠️ 该脚本在 build 目录缺 `CTestTestfile.cmake`
      时只 warning + `exit 0` ⇒ ⇒ MUST 先 `cmake -B build` + 构建再跑）。
