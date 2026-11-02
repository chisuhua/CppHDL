# Tasks：路线 B 第 3 批 —— `sync_fifo` 本体自保护

## C1 —— 新增 dual 文件（**原文件 MUST NOT 修改**）

- [x] `tests/chlib/test_sync_fifo_dual.cpp`（**2** pair / **88** 断言）
      ⇒ ① 写 4 再按序读 ② 满写 MUST NOT 覆盖 + 空读 MUST NOT 移动指针
- [x] `tests/CMakeLists.txt`：1 条 `add_dual_backend_test`

## C2 —— 判据（N2′：断言数 == 2 ×）

- [x] `test_sync_fifo_dual`：**88 == 2 × 44** ✔
- [x] 期望值**全部手推**：文件头逐条写明语义前提（`will_write` / `will_read` /
      0-cycle `q_async` / 沿上写入）

## C3 —— 变异证明判别力

- [x] **M1**：`will_write` 去掉 `!is_full(count)` 门控（满写会覆写队首）
      ⇒ **2 failed**（两后端半边）
- [x] **M2**：读指针从 `will_read` 换成裸 `rden`（空读让指针自由运行）
      ⇒ **2 failed**（两后端半边）
- [x] ⚠️ 每条变异均**先确认仍可编译**（`error:` 计数 = 0）⇒ 非假绿
- [x] ⚠️ 还原用 `copyfile`（刷新 mtime ⇒ 强制重建）⇒ 复验回到 88 断言全绿
- [x] 既有 `test_fifo`（52 断言）/ `test_stream_fifo_dual`（62 断言）**不回归**

## C4 —— 计数与验收

- [x] 锚点：`CTEST_TOTAL` **218** · `VERILATOR_E2E_TOTAL` **50** ·
      `DUAL_BACKEND_PARITY_TOTAL` **41** · `PAIRS` **219** · non-perf **207** ·
      `tests/chlib/test_*.cpp` **36**
- [x] `ctest -LE perf` **207/207**（50 个 verilator_e2e · 零回归）
- [x] `verify_test_counts.sh` / `verify_dual_backend_parity.sh` 通过
- [x] 归档：**用** `--skip-specs`
