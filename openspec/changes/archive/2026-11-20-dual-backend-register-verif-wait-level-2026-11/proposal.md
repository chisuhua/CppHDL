# Proposal: 登记 `test_verif_wait_level` 为单后端条目（`dual-backend-register-verif-wait-level`）

- **状态**: **立项（2026-11-20 · builder 发现 → 用户批准执行）** · 待归档
- **性质**: **登记类**（**零实现改动**）—— 只补 capability spec 的一条登记段
- **依据**: `scripts/verify_dual_backend_parity.sh` 的既有违规
  `VIOLATION unclassified-labeled-entry: test_verif_wait_level`

---

## Why

`2026-11-20-verif-wait-level-2026-11`（已归档）按它自己的 tasks B1 把
`tests/test_verif_wait_level.cpp` 经 `add_dual_backend_test()` 注册（⇒ 自动带
`verilator_e2e` 标签），但**没有**在 `dual-backend-test-coverage` capability 的
「现有 9 个 verilator 测试 MUST 保留原状」Requirement 里登记为**单后端条目**。

⇒ ⇒ 护栏 `scripts/verify_dual_backend_parity.sh` 的交叉核验把「带标签但没有双后端 parity、
且不在契约锁定清单里」判为违规 ⇒ ⇒ **自那次归档起在 CI 上恒红**（4 个执行点：
`build-and-test` / `pr-verilator-fast`，各含 `--self-test`）。

判定依据（三处都在 HEAD 已提交内容里，与本 change 的实现无关）：

- `tests/CMakeLists.txt` 用 `add_dual_backend_test(test_verif_wait_level ...)` 注册；
- `tests/test_verif_wait_level.cpp` 内**无** `DUAL_BACKEND_TEST_PAIR` 调用；
- 主 spec 的契约锁定清单里**没有**该文件。

## What Changes

`openspec/specs/dual-backend-test-coverage/spec.md` 的
**「现有 9 个 verilator 测试 MUST 保留原状」** Requirement 追加一段**新增登记**：

- 把它归为**单后端条目**（它比较的是 `.verif` 的**两条执行路径** —— IR 解释器 vs 渲染出的
  TB 经 verilator 实跑 —— 不是「同一 DUT 跑两个 eval backend」）；
- 明确 MUST 保留原状（MUST NOT 迁移到 macro）、MUST 带 `verilator_e2e` 标签
  （否则在 `BUILD_VERILATOR=OFF` 的 job 里恒跳过）、**不**增加 parity 的两个锚点；
- Requirement 标题里的「9 个」（macro 的 reference implementation 数量）**不变**
  —— 与本 spec 既有三条同类登记的写法一致。

**不改任何代码 / 测试 / 锚点**：`CTEST_TOTAL` / `VERILATOR_E2E_TOTAL` /
`DUAL_BACKEND_PARITY_TOTAL` / `_PAIRS` 全部保持现值（本 change 不增删测试）。

## Out of Scope

- 不讨论「`test_verif_wait_level` 是否应该改成双后端形态」—— 它测的是 `.verif` 原语语义，
  与「同一 DUT 两个 eval backend」不是同一个判据面（改形态属另一项工作）。
- 不动护栏脚本的解析逻辑（它按既有规则正确识别了这条漏登记）。

---

## 用户可见变更

> ⚠️ **本段是 2026-11-21 补写的**（归档时遗漏 ⇒ `scripts/verify_public_api_visibility.sh`
> 的 **R1**（归档 change MUST 带「## 用户可见变更」段 · 机制生效日 2026-11-09）报红
> ⇒ ⇒ 补的是**归档流程的产物要求**，不改动本 change 的任何决策与实施内容）。

| 项 | 前 | 后 |
|---|---|---|
| capability `dual-backend-test-coverage` 的「现有 9 个 verilator 测试 MUST 保留原状」Requirement | **未登记** `tests/test_verif_wait_level.cpp` ⇒ ⇒ 该文件带 `verilator_e2e` 标签但既无 `DUAL_BACKEND_TEST_PAIR`、也不在契约锁定清单里 ⇒ ⇒ `scripts/verify_dual_backend_parity.sh` 判 `unclassified-labeled-entry` ⇒ ⇒ **CI 的 4 个执行点自 2026-11-20 归档起恒红** | **新增一段单后端登记**（分类依据 = 它比的是 `.verif` 的两条执行路径而非两个 eval backend · MUST 保留原状 · MUST 带 `verilator_e2e` 标签 · **不**增加 parity 两个锚点）⇒ ⇒ 护栏**零 VIOLATION** |
| 库代码 / 测试 / 锚点（`CTEST_TOTAL` · `VERILATOR_E2E_TOTAL` · `DUAL_BACKEND_PARITY_TOTAL` · `_PAIRS`） | — | **零改动**（本 change 只改 spec 文本 ⇒ 不增删任何测试） |
