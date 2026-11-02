# Tasks：路线 B 第 5 批 —— 进位/借位算术 + 门/寄存器/计数器

## C1 —— 新增 dual 文件（**原文件 MUST NOT 修改**）

- [x] `tests/chlib/test_arithmetic_carry_dual.cpp`（**3** pair / **112** 断言）
      ⇒ 带进位加法 · 带借位减法 · 比较器 + `abs`
- [x] `tests/chlib/test_sequential_counters_dual.cpp`（**4** pair / **370** 断言）
      ⇒ 逻辑门/mux/三态 · 寄存器（保持 + 复位优先）· 计数器族 · 边沿检测
- [x] `tests/CMakeLists.txt`：2 条 `add_dual_backend_test`

## C2 —— 判据（N2′：断言数 == 2 ×）

- [x] `test_arithmetic_carry_dual`：**112 == 2 × 56** ✔
- [x] `test_sequential_counters_dual`：**370 == 2 × 185** ✔
- [x] 期望值**全部手推**（逐条写进文件头）

## C3 —— 变异证明判别力

- [x] **M1**：`register_` 去掉 `en` 门控（`select(rst,0,select(en,d,reg))` ⇒
      `select(rst,0,d)`）⇒ **失去「保持」语义** ⇒ **2 failed**
- [x] **M2**：`add_with_carry` 的 `carry_out` 从 `bit_select(result, N)` 改成
      `bit_select(result, 0)` ⇒ **2 failed**
- [x] ⚠️ 每条变异均**先确认仍可编译**（`error:` 计数 = 0）⇒ 非假绿
- [x] ⚠️ 还原用 `copyfile`（刷新 mtime ⇒ 强制重建）⇒ 复验回到 112 / 370 全绿
- [x] 既有 `test_arithmetic`(16) / `test_sequential`(98) / `test_logic`(58)
      **不回归**

## C4 —— 计数与验收

- [x] 锚点：`CTEST_TOTAL` **221** · `VERILATOR_E2E_TOTAL` **53** ·
      `DUAL_BACKEND_PARITY_TOTAL` **44** · `PAIRS` **230** · non-perf **210** ·
      `tests/chlib/test_*.cpp` **39**
- [x] `ctest -LE perf` **210/210**（53 个 verilator_e2e · 零回归）
- [x] `verify_test_counts.sh` / `verify_dual_backend_parity.sh` 通过
- [x] 归档：**用** `--skip-specs`

## C5 —— 本批挖出的**三个真缺陷**（框架层 · 独立立项 · 本 change 不修）

- [x] **D1** `add_with_carry` / `sub_with_borrow`：端口 `cin`/`bin` **被完全忽略**
      ⇒ 根因：`make_literal(carry_in ? 1ULL : 0ULL, 1)` 的 **C++ 三元在建图期求值**
      ⇒ 既有测试用**建图期常量** ⇒ **从未暴露**
- [x] **D2** `edge_detector`：端口 `sig` 下 `pos`/`neg` **恒为 0**
      ⇒ 根因：`prev_signal->next = signal` 无条件跟随 ⇒ `prev` 恒等于 `signal`
      ⇒ 既有测试用**建图期常量** ⇒ **从未暴露**
- [x] **D3** `johnson_counter`：端口 `en` 下计数 **恒为 0**
      ⇒ 根因：`ch_bool feedback = !bit_select(counter, N-1);` 的 **`!` 是 C++ 布尔非**
      + 建图期求值 + 建图期三元 ⇒ 既有测试用**建图期常量** ⇒ **从未暴露**
- [x] ⇒ ⇒ **处置**：不修（独立立项）+ **钉住当前行为** ⇒ ⇒ 修好时本文件立即变红
      ⇒ ⇒ 两个新文件成为这三个缺陷的**回归探针**
