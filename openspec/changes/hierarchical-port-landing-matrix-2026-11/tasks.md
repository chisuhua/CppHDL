# Tasks: 父侧落点形态矩阵的轴缺口补齐

> TDD 5 步纪律：**先写会失败的判据 → 再写实现 → 再跑**。
> 每项完成后把 `[ ]` 改成 `[x]`（★ 由执行方在**同一会话**内勾选）。

## T0 — 前置核对（不改代码）

- [x] T0.1 确认无活跃 change 冲突：`ls openspec/changes/` ⇒ 只有本 change
- [x] T0.2 `cmake -B build` ⇒ 配置成功（⚠️ 后续 `verify_test_counts.sh` 依赖 build 目录有 `CTestTestfile.cmake`，
      否则它只 warning + `exit 0` ⇒ **假绿**）
- [x] T0.3 记录基线：`cd build && ctest -N | tail -1` ⇒ 确认总数与 `AGENTS.md` 的 `CTEST_TOTAL` 相等
- [x] T0.4 记录基线：`cd build && ctest -N -L verilator_e2e | tail -1` ⇒ 与 `VERILATOR_E2E_TOTAL` 相等

## T1 — 判据先行：6 个 fixture 的现状实测

- [x] T1.1 写 4 个 `MatRow2Expr*` DUT（`ch_out` / `ch_reg` / `ch_proxy` / `op`；每个带类级
      `CH_HIERARCHY_LAYER`；**每端口单一驱动**；`ch_out` 格复刻 `select(cond, child_out, …)` 真实写法）
      ⇒ ⚠️ 实测后收敛：`ch_in` 格 = **Na**（方向非法）· `mem` 格复用既有 `[reject][naming]` tag
      ⇒ ⇒ **二者无 fixture**；⚠️ `ch_bool`/1-bit 变体**未加**（同属 `ch_out` 族 ⇒ 边际价值低）
- [x] T1.2 写一个**临时**探针（或临时用例）逐个跑 6 个 fixture ⇒ **记录**每格的
      `ok` / 产物形态 / 错误文本 ⇒ ⇒ **实测填入** `design.md` §2.2 的表格（**不得预设**）
- [x] T1.3 写粒度负向对照 fixture `MatInlineControl`（**不带** `CH_HIERARCHY_LAYER`）
      ⇒ ⇒ 断言默认粒度下产出**单 module**

## T2 — 扩展既有矩阵

- [x] T2.1 在 `tests/test_hierarchy_verilog.cpp` 的 `cells` 中新增 site `row2_expr` 的 6 格
      （取值 = T1.2 实测结果；`Na` 格 MUST 写理由且**不是** tag）
- [x] T2.2 同步上界与三态计数（`cells.size()` 36 → 42；`n_ok` / `n_reject` / `n_na`）
- [x] T2.3 每格补**行为断言**：
      `Ok` ⇒ `ok==true` + 产物形态断言（用 `module_text` 做作用域锚定 + 显式反断言同名网不在子 body）；
      `Reject` ⇒ `ok==false` + 产物文件不存在 + 错误非空且点名子 module key（**MUST NOT** 断言整串）；
      `Na` ⇒ 理由非空
- [x] T2.4 在矩阵注释里写明「本矩阵是**活的现状快照** ⇒ ⇒ 行 16 若实施（B）⇒ `row2_expr × ch_out` 格
      **预期转红** ⇒ ⇒ 届时 MUST 更新该格与计数」
- [x] T2.5 补 S1 直通的「**不造网**」显式断言（挂在既有 `row2 × ch_out` 用例上）：
      `ok==true` + 产物**无**父侧 `logic` 网 + `.top_mid_unnamed_output(top_unnamed_output)`

## T3 — 族 ④ 的逐拍等价（✅ 已做 · 手推期望 {17,90,165,255} 逐拍吻合）

- [x] T3.1 在**既有** `tests/test_hierarchy_equivalence.cpp` 目标内加一例（⇒ ⇒ 不新增 CTest 条目）
- [x] T3.2 `ch::hier_eq::emit(..., ForceLayer)` ⇒ `compare(...)` ⇒ ★ 期望 trace 以**字面量表**写在源码
      + **推导注释**完整 ⇒ ⇒ 与 `flat_trace` **分开断言**
- [x] T3.3 被观测值 MUST **引到顶层可观测**（否则 `observation-not-visible` 机械拒绝）

## T4 — 判别力自证（变异表）

- [x] T4.1 **M1（已做）**：临时删发射器的**父侧网登记**（`SiblingPort` 登记一行）⇒ ⇒ 族 ④ 用例**红**（`:1497`）⇒ 还原 + **重建** ⇒ 绿：临时删发射器父侧网声明 ⇒ ⇒ 族 ④ 用例**红** ⇒ ⇒ **编译确认 + 重建**
- [x] T4.2 **M2（已做）**：临时把被驱动端的实例实参从**父侧网**改为**目标节点自体名** ⇒ ⇒ 族 ④ 用例**红**（`:1503`）⇒ 还原 + **重建** ⇒ 绿：临时把族 ④ 的 `.port(net)` 改成 `.port(parent_out)` ⇒ ⇒ 族 ④ 用例**红**
- [x] T4.3 M3：把 `row2_expr × ch_out` 的 fixture 换成 `row2_expr × ch_reg` 写法 ⇒ ⇒ 该格**红**
- [x] T4.4 全部还原 ⇒ ⇒ **重建** ⇒ ⇒ 复跑 ⇒ ⇒ 全绿 ⇒ ⇒ `git diff --stat -- src include` **为空**

## T5 — 验证与收口

- [x] T5.1 `cmake --build build -j$(nproc)` ⇒ 0 error / 0 warning（新增/改动文件）
- [x] T5.2 `ctest --output-on-failure` ⇒ 全绿（⚠️ MUST **全量重建**后再跑 ⇒ 否则跑的是旧二进制 ⇒ 假绿）
- [x] T5.3 `ctest -N | tail -1` ⇒ 总数**不变**（与 `AGENTS.md` 的 `CTEST_TOTAL` 仍相等）
- [x] T5.4 `ctest -N -L verilator_e2e | tail -1` ⇒ **不变**
- [x] T5.5 `bash scripts/verify_test_counts.sh` ⇒ 转绿
- [x] T5.6 `bash scripts/verify_hierarchical_rtl.sh` ⇒ 转绿（产物级 R1–R8）
- [x] T5.7 `bash scripts/verify_hier_corpus_coverage.sh` ⇒ 转绿（★ 本 change **不动**语料登记表 ⇒ ⇒ 必须仍绿）
- [x] T5.8 `bash scripts/verify_arch_ledger_residuals.sh` ⇒ 转绿（台账未被本 change 触碰）
- [x] T5.9 文档同步：矩阵的新 site 说明 + `AGENTS.md`（若锚点或护栏清单有变 ⇒ 本 change **预期不变**）

## T6 — 归档前置

- [ ] T6.1 `openspec validate hierarchical-port-landing-matrix-2026-11 --strict` ⇒ 通过
      （⚠️ 本仓 CLI 为 1.14.x ⇒ ⇒ 既有 spec 可能因新 strict 规则报 warning ⇒ ⇒ **与本 change 无关** ⇒
      ⇒ 只看**本 change** 的判定）
- [ ] T6.2 **显式声明「no spec changes」**（本 change 无 `specs/` 目录）⇒ ⇒ 归档时**允许** `--skip-specs`
- [ ] T6.3 `rddf feedback add` **不需要**（本 change 无 `plan_drift` / `design_drift` ⇒ ⇒ 未改 plan / AC / 决策）
- [ ] T6.4 commit：**按路径显式** `git add <path>`（★ 禁 `git add -A` —— 本仓多人并行）⇒ ⇒ 且**等用户确认**
