# Tasks：路线 B 第 4 批 —— 编码器 / 译码器 / BCD / 格雷码

## C1 —— 新增 dual 文件（**原文件 MUST NOT 修改**）

- [x] `tests/chlib/test_arithmetic_encoding_dual.cpp`（**4** pair / **172** 断言）
      ⇒ ① 编码器（多 bit **高编号优先**）② 译码器 + onehot ③ BCD ④ 格雷码
- [x] `tests/CMakeLists.txt`：1 条 `add_dual_backend_test`

## C2 —— 判据（N2′：断言数 == 2 ×）

- [x] `test_arithmetic_encoding_dual`：**172 == 2 × 86** ✔
- [x] 期望值**全部手推**（穷举表写进文件头；禁抄仿真输出）

## C3 —— 变异证明判别力

- [x] **M1′**：`priority_encoder` 的遍历反向（`for (i = N; i-- > 0;)`）
      ⇒ **2 failed**（两后端半边）
- [x] **M2**：`binary_to_bcd` 的 `/10 %10` 改成 `>>4 & 0xF` ⇒ **2 failed**
- [x] ⚠️ 每条变异均**先确认仍可编译**（`error:` 计数 = 0）⇒ 非假绿
- [x] ⚠️ 还原用 `copyfile`（刷新 mtime ⇒ 强制重建）⇒ 复验回到 172 全绿
- [x] 既有 `test_converter`(16) / `test_combinational`(56) / `test_arithmetic`(16)
      **不回归**

## C4 —— 计数与验收

- [x] 锚点：`CTEST_TOTAL` **219** · `VERILATOR_E2E_TOTAL` **51** ·
      `DUAL_BACKEND_PARITY_TOTAL` **42** · `PAIRS` **223** · non-perf **208** ·
      `tests/chlib/test_*.cpp` **37**
- [x] `ctest -LE perf` **208/208**（51 个 verilator_e2e · 零回归）
- [x] `verify_test_counts.sh` / `verify_dual_backend_parity.sh` 通过
- [x] 归档：**用** `--skip-specs`

## C5 —— 本批**如实登记**的发现（供 backlog）

- [x] ⚠️ **`priority_encoder` 头注释与实现相反**（注释「优先处理低位」vs 实现
      **高编号优先**）⇒ 既有断言与实现一致 ⇒ ⇒ **注释是错的** ⇒ 订正 = 独立议题
- [x] ⚠️ **我的首个变异 M1 是【无效变异】**（在原链后**追加一层同向链** ⇒
      两层都是「高编号优先」⇒ 结果不变 ⇒ **恒绿**）⇒ ⇒ 换成 M1′（反向遍历）
      才抓住 ⇒ ⇒ **教训**：「追加一层同向链」不改变语义 ⇒ 必须**反向遍历**
- [x] ⚠️ **手推被实测纠正两次**：`gray_to_binary` 表（标准折叠异或）与
      `binary_to_gray(0x80)`（`(0x40)^0x80 = 0xC0`，我漏了 `>>1` 那一半）
      ⇒ ⇒ 两次都是**两后端一致**给出标准值 ⇒ ⇒ **是推导错，不是 parity 分歧**
- [x] ⚠️ **`instr_op_bits_extract` 报 `Invalid bit range [7:0]`**（10 次/运行）
      ⇒ 值**全部正确**（172/172 绿）⇒ ⇒ **只影响日志** ⇒ ⇒ 触发条件 =
      `gray_to_binary` 的**运行时** `bit_select` 路径 ⇒ ⇒ 独立议题
