# Tasks：路线 B 第 2 批 —— 条件语义

## C1 —— 新增 dual 文件（**原文件 MUST NOT 修改**）

- [x] `tests/chlib/test_if_stmt_dual.cpp`（**4** pair / **26** 断言）
      ⇒ `then/else_` 两路 · 先匹配者胜 + 无 `else_` **保持** · 嵌套内层优先 ·
      **输出端口**的显式默认尾部
- [x] `tests/chlib/test_guard_contribution_dual.cpp`（**1** pair / **8** 断言）
      ⇒ **非互斥**守卫 ⇒ 链序运行期可观测
- [x] `tests/CMakeLists.txt`：2 条 `add_dual_backend_test`

## C2 —— 判据（N2′：断言数 == 2 ×）

- [x] `test_if_stmt_dual`：**26 == 2 × 13** ✔
- [x] `test_guard_contribution_dual`：**8 == 2 × 4** ✔
- [x] 期望值**全部手推**：文件头逐拍列出

## C3 —— 变异证明判别力

- [x] **M1**（`if_stmt`）：把分支体里的 `conditional_assign(r).next() = v`
      换成裸 `r->next = v`（= 绕过捕获 ⇒ 条件被忽略）
      ⇒ **2 failed**（两后端半边）
- [x] **M2**（`guard_contribution`）：把 `rbegin/rend` 换成 `begin/end`
      （= 链序反转 ⇒ 后匹配者胜）⇒ **2 failed**（两后端半边）
- [x] ⚠️ 每条变异均**先确认仍可编译**（`error:` 计数 = 0）⇒ 非假绿
- [x] ⚠️ 还原均用 `copyfile`（刷新 mtime ⇒ 强制重建）⇒ 复验回到 26 / 8 全绿

## C4 —— 计数与验收

- [x] 锚点：`CTEST_TOTAL` **217** · `VERILATOR_E2E_TOTAL` **49** ·
      `DUAL_BACKEND_PARITY_TOTAL` **40** · `PAIRS` **217** · non-perf **206** ·
      `tests/chlib/test_*.cpp` **35**
- [x] `ctest -LE perf` **206/206**（49 个 verilator_e2e · 零回归）
- [x] `verify_test_counts.sh` / `verify_dual_backend_parity.sh` 通过
- [x] 归档：**用** `--skip-specs`（`.openspec.yaml` 已显式声明 no spec changes）
