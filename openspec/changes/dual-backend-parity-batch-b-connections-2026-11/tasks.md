# Tasks：A5 批次 B（连接语义组）

## C1 —— 新增 `_behavior` 文件（**原文件 MUST NOT 修改**）

- [x] `tests/test_bool_connection_behavior.cpp`（**4** pair / **14** 断言）
      ⇒ bool ← bool / uint<1> / reg / literal 四种源
- [x] `tests/test_operator_connection_behavior.cpp`（**4** pair / **32** 断言）
      ⇒ 纯连线 / 寄存器 + `select` / 组合逻辑（`+` + `select` + `bits<>`）/ bool→uint<1>
- [x] `tests/test_wires_connection_behavior.cpp`（**5** pair / **68** 断言）
      ⇒ wire→wire / 多级 wire 加法（**4 位溢出截断**）/ wire→reg / 4 级链 / 4→8 位零扩展
- [x] `tests/chlib/test_multi_if_acceptance_behavior.cpp`（**3** pair / **12** 断言）
      ⇒ 零扩展金标（`0x0A5` / `0x00F`）+ **直接赋 `ch_out` ≡ 先转 `ch_uint`**
- [x] `tests/CMakeLists.txt`：4 条 `add_dual_backend_test`

## C2 —— 判据（N2′：**断言数 == 2 ×**）

- [x] `test_bool_connection_behavior`：14 == 2 × 7 ✔
- [x] `test_operator_connection_behavior`：32 == 2 × 16 ✔
- [x] `test_wires_connection_behavior`：68 == 2 × 34 ✔
- [x] `test_multi_if_acceptance_behavior`：12 == 2 × 6 ✔
- [x] 期望值**全部手推**（文件头逐条写明金标与推导）

## C3 —— 计数与墙钟

- [x] 锚点：`CTEST_TOTAL` **214** · `VERILATOR_E2E_TOTAL` **46** ·
      `DUAL_BACKEND_PARITY_TOTAL` **37** · `PAIRS` **210** · non-perf **203** ·
      `tests/test_*.cpp` **125** · `tests/chlib/test_*.cpp` **33**
- [x] **冷缓存墙钟实测**（假 HOME）：**33 s / 16 pair ≈ 2.1 s/pair**
      ⇒ ⇒ ⚠️ **比 plan §3 的成本模型（10.66 s/pair）低 5 倍** ⇒ ⇒
      ⇒ ⇒ **成本模型 MUST 按 DUT 复杂度分档**（本次全是**纯组合 / 轻连线** DUT；
      ⇒ ⇒ AXI peripheral（批次 C）才是**重** DUT）⇒ ⇒ 回填 plan §5

## 验收

- [ ] `ctest -LE perf` 零回归
- [ ] `verify_test_counts.sh` / `verify_dual_backend_parity.sh` 通过
- [ ] `verify_stream_ready_ownership.sh` 等护栏全绿
- [ ] 归档：**用** `--skip-specs`（`.openspec.yaml` 已显式声明 no spec changes）
