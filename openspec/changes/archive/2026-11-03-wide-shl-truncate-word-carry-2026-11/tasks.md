# Tasks：多 word 左移的跨 word 进位

## T1 —— 根因定位（**实测**，逐步否定前一个假设）

- [x] 复现 `eq` 判错（`pos ≥ 2` 错、`0/1` 对）
- [x] **只测比较原语** ⇒ 全部正确 ⇒ `eq` / `bv_eq` / `bitvector::operator==` 无罪
- [x] 读实现链（`op::Eq::eval` → `bv_eq_truncate` → `bv_eq`）⇒ 零扩展比较，语义正确
- [x] **只测移位原语** ⇒ `p ≥ 2` 多出 `2^(64-p)` ⇒ 定位 `bv_shl_vector`
- [x] 读 `bv_shl_vector` ⇒ 进位方向反了 + 缺 `<< shift_bits` + 不判上界
- [x] 独立参考模型（256 位容器逐位置位）扫 192 组参数 ⇒ 修前 102 FAIL

## T2 —— 修复

- [x] `include/bv/bv_shift.h`：`out[i] = (in[i-s] << d) | (in[i-s-1] >> (64-d))`，
      两个索引都判 `[0, in_num_words)`；注释写明别名契约与缺陷来历
- [x] `include/bv/bitvector.h`：`operator<<=` 先拷一份输入（同址调用被 `fill_n` 清空）

## T3 —— 判据

- [x] `tests/test_wide_shl_dual.cpp`（**新**，1 pair / **154 == 2 × 77**）
      - 路径 A：源 1 word（64 位字面量 `1`）⇒ 结果 191 位（3 word）
      - 路径 B：源 2 word（`concat(hi, lo)`）⇒ 结果 255 位（4 word）
      - 11 组激励 × 7 个 64 位切片，期望值由**独立模型**给出
- [x] `tests/test_bitvector.cpp`：`Multi-word left shift`（11 组 × {`<<`, `<<=`}）
- [x] `tests/CMakeLists.txt`：注册（`TIMEOUT 300`）

## T4 —— 变异 / 判别力（**如实登记边界**）

- [x] M1（`bv_shl_vector` 还原为旧实现）⇒ `test_wide_shl_dual` 1 failed / **1 passed**
      （⇒ **只有 interpreter 半边红** = 分歧的直接证据）+ `test_bitvector` 1 failed
- [x] M2（`operator<<=` 还原为同址调用）⇒ `test_bitvector` 1 failed，
      `test_wide_shl_dual` **全绿**（**如实登记**：图层走 `bv_shl_truncate`，dst ≠ src
      ⇒ ⇒ 该缺陷只有 bv 层用例能抓，两个判据**不可互相替代**）
- [x] 还原后两者复绿；全部 192 组参数扫描 0 FAIL

## T5 —— 契约与验收

- [x] capability `dual-backend-test-coverage` **ADDED ×1**（1 Requirement + 4 Scenario）
      ⇒ 归档**禁** `--skip-specs`
- [x] improvement 状态改为**已修复** + 标题订正 + §6（定位过程 / 根因 / 为何零覆盖 / 判据 / 变异）
- [x] 全量重建 0 error · `ctest -LE perf` **232/232**
- [x] 计数锚点同步（`CTEST_TOTAL` 242 → **243** · `VERILATOR_E2E_TOTAL` 74 → **75** ·
      parity 65 → **66** · PAIRS 288 → **289** · non-perf 231 → **232** ·
      `tests/test_*.cpp` 134 → **135**）
