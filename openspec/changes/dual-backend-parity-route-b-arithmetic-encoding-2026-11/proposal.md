# 提案：路线 B 第 4 批 —— 编码器 / 译码器 / BCD / 格雷码进双后端 parity

- **依据**：`.rddf/plans/dual-backend-full-coverage-2026-11.md` §4 **H 批**（路线 B）
  + 2026-11-02 的量化盘点结论（「仓内最核心的算术/编码语义，dual 覆盖率 = 0」）
- **规格影响**：**无 spec 变更**（`.openspec.yaml` 显式声明 `skip_specs: true`）
- **规模**：1 个新测试文件 + 1 条 CMake 注册 + 计数锚点
- **关联**：`objective-test-coverage-100pct` · capability `dual-backend-test-coverage`

## Why

`chlib` 的**编码器 / 译码器 / 优先级选择器**是仓内最基础的一批算术语义，而盘点
实测它们的 **dual 覆盖率是 0**：

| 组合子 | 既有测试 | 已被 dual 覆盖 |
|---|---|---|
| `priority_encoder` · `binary_encoder` · `binary_decoder` | `chlib/test_combinational.cpp`（13 TC / 56 运行期取值） | **0** |
| `binary_to_onehot` · `onehot_to_binary` · `bcd_to_binary` · `binary_to_bcd` · `gray_to_binary` | `chlib/test_converter.cpp`（6 TC / 25 运行期取值） | **0** |

⇒ ⇒ 它们的 **Verilog codegen 路径从未被验证过**。

⚠️ **为什么这类语义特别危险**：它们全由 **`select` 链**实现
（`result = select(cond_i, v_i, result)` 逐层套）⇒ ⇒ **链的方向**决定语义
⇒ ⇒ 而方向在 interpreter 侧**几乎不可观测**：

- `priority_encoder` / `binary_encoder`：既有测试**只覆盖单 bit**
  （`0x10`→4 · `0x20`→5）⇒ ⇒ **单 bit 无法区分方向**；
- `binary_decoder` / `binary_to_onehot`：输入**互斥**⇒ ⇒ 方向同样不可观测。

⇒ ⇒ **「多 bit 同时置位时选哪个」这条真正的语义，两条后端都没被钉过。**

## 为什么是【新增】而不是转换

既有 `chlib/test_combinational.cpp` / `chlib/test_converter.cpp` 是**裸 context**
形态（`ch_device=0` · `__io=0`）⇒ ⇒ `DUAL_BACKEND_TEST_PAIR` **不适用**
⇒ ⇒ 必须**新建带端口的 DUT** ⇒ ⇒ **期望值必须手推**（不能抄输出）。
⇒ 两个原文件 **MUST NOT 修改**。

## What Changes

| 文件 | 内容 |
|---|---|
| `tests/chlib/test_arithmetic_encoding_dual.cpp`（**新**） | **4** 个 pair / **172** 断言：<br>① 编码器（`priority_encoder` + `binary_encoder`，**多 bit 高编号优先**）<br>② 译码器（`binary_decoder` + `binary_to_onehot` + `onehot_to_binary`）<br>③ BCD（`bcd_to_binary` + `binary_to_bcd`）<br>④ 格雷码（`gray_to_binary` + `binary_to_gray`） |
| `tests/CMakeLists.txt` | 1 条 `add_dual_backend_test` |
| 计数锚点 | `CTEST_TOTAL` 218 → **219** · `VERILATOR_E2E_TOTAL` 50 → **51** · `DUAL_BACKEND_PARITY_TOTAL` 41 → **42** · `PAIRS` 219 → **223** · non-perf 207 → **208** · `tests/chlib/test_*.cpp` 36 → **37** |

## 期望值（**穷举手推**，禁抄仿真输出）

实现（`include/chlib/combinational.h` / `converter.h`）的链方向：
`for (i = 0 .. N-1) { result = select(cond_i, v_i, result); }`
⇒ 第 i 轮把 `result` 包在**里层** ⇒ ⇒ **后写的外层优先** ⇒ ⇒ **高编号 i 优先**。

⚠️ 与 `chlib::compose_guard_chain`（「先收集者在外层」）方向**相反** —— 两者都写
`select` 链但遍历方向不同 ⇒ ⇒ 这正是值得双后端验证的地方。

⚠️ **`priority_encoder` 的头注释说「优先处理低位」，与实现相反**（实现是高编号优先）。
既有测试的多 bit 断言（`0x32` ⇒ 期望 **5**）**与实现一致** ⇒ ⇒ 注释是错的、实现是既定的。
⇒ ⇒ 本文件按**实现**（= 既有断言）手推 ⇒ ⇒ 并**登记这处不一致**
（注释订正 / 实现改向 = **独立议题**，不在本 change 范围）。

手推表全部写进文件头（N = 8 ⇒ 输出宽度 3）。

## Verification（前瞻 · 可勾选）

- [ ] **N2′**：断言数(with verilator) == **2 ×** 断言数(interpreter-only) ⇒ `172/86`
- [ ] 期望值**全部手推**（穷举表；禁抄仿真输出）
- [ ] 两个原文件 **MUST NOT 修改**
- [ ] 变异证明：M1′ 反转 `priority_encoder` 的遍历方向 ⇒ 红 ·
      M2 `binary_to_bcd` 的 `/10 %10` 改成 `>>4 &0xF` ⇒ 红
- [ ] `ctest -LE perf` 零回归 · 既有 `test_converter` / `test_combinational` 不回归
- [ ] 计数锚点同步且两个护栏双绿
