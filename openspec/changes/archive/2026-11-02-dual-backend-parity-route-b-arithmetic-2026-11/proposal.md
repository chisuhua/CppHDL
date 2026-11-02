# 提案：路线 B 第 5 批 —— 带进位/带借位算术 + 门/寄存器/计数器进双后端 parity

- **依据**：`.rddf/plans/dual-backend-full-coverage-2026-11.md` §4 **H 批**（路线 B）
  + 2026-11-02 的量化盘点（`arithmetic` / `logic` / `sequential` 的 dual 覆盖率 = 0）
- **规格影响**：**无 spec 变更**（`.openspec.yaml` 显式声明 `skip_specs: true`）
- **规模**：2 个新测试文件 + 2 条 CMake 注册 + 计数锚点
- **关联**：`objective-test-coverage-100pct` · capability `dual-backend-test-coverage`

## Why

`chlib` 的**带进位/带借位**算术与**门/寄存器/计数器/边沿检测**是仓内最基础的一批
组件，盘点实测它们在 42 个 dual 文件里**零覆盖** ⇒ ⇒ 它们的 Verilog codegen 路径
**从未被验证过**。

⚠️ **为什么这类语义特别危险**：

| 面 | 危险点 |
|---|---|
| `add_with_carry` / `sub_with_borrow` | 实现是「**扩位 + 截断**」三步（`ch_uint<N+1>` → 算 → `bits<N-1,0>` 截断 + `bit_select(result,N)` 取进位）⇒ ⇒ **扩位方式**与**截断**是两后端最容易分叉的地方 |
| `register_` / `dff_` / 各类计数器 | 实现是「**寄存器 + 条件组合**」（`reg->next = select(en, d, reg)`）⇒ ⇒ **保持语义**（`en=0` 时**保持上一拍的值**而不是 0）是极易写错的一类 ⇒ ⇒ 只有「先命中一个值、再关掉使能、断言读回**旧值**」这种序列才能抓到 |

## 为什么是【新增】而不是转换

既有 `chlib/test_arithmetic.cpp` / `chlib/test_sequential.cpp` / `chlib/test_logic.cpp`
是**裸 context** 形态（`ch_device=0` · `__io=0`）⇒ ⇒ `DUAL_BACKEND_TEST_PAIR`
**不适用** ⇒ ⇒ 必须**新建带端口的 DUT** ⇒ ⇒ **期望值必须手推**。
⇒ 三个原文件 **MUST NOT 修改**。

## 🔴🔴 本批挖出的**三个真缺陷**（框架层 · 独立立项 · 本 change 不修）

三个缺陷**同型**：**「寄存器 / 条件量在【建图期】被求值」**，而既有测试**恰好用建图期
常量** ⇒ ⇒ 全部**从未被暴露**。本批的 DUT 把它们改成**端口** ⇒ ⇒ 缺陷立刻显形。

| # | 组件 | 现象（实测） | 根因 |
|---|---|---|---|
| **D1** | `add_with_carry` / `sub_with_borrow` | 端口 `cin` / `bin` **被完全忽略** | 内部写 `make_literal(carry_in ? 1ULL : 0ULL, 1)` ⇒ **C++ 三元在建图期求值** ⇒ 建图期 `carry_in` 未驱动 ⇒ 恒取 else |
| **D2** | `edge_detector` | 端口 `sig` 下 `pos`/`neg` **恒为 0** | `prev_signal->next = signal` ⇒ `prev_signal` **无条件跟随** ⇒ 恒等于 `signal` ⇒ `signal & !signal = 0` |
| **D3** | `johnson_counter` | 端口 `en` 下计数 **恒为 0** | `ch_bool feedback = !bit_select(counter, N-1);` ⇒ **`!` 是 C++ 布尔非**（非按位非）+ **建图期**求值 ⇒ 再叠加建图期三元 |

⚠️ **既有测试为何没抓到**：`chlib/test_arithmetic.cpp` 的 `carry_in` 是
`ch_bool(true)`（建图期常量）· `chlib/test_sequential.cpp` 的 `signal` 是
`ch_bool signal = false;`（建图期常量）⇒ ⇒ 全部**恰好绕开**。

⇒ ⇒ **处置**：本 change **不修**（属框架层，独立立项）⇒ ⇒ 但**如实钉住当前行为**
⇒ ⇒ 「`cin`/`pos`/`johnson` 在端口下恒为固定值」⇒ ⇒ **将来修好时本文件立即变红**
⇒ ⇒ 本文件因此成为这三个缺陷的**回归探针**。

## What Changes

| 文件 | 内容 |
|---|---|
| `tests/chlib/test_arithmetic_carry_dual.cpp`（**新**） | **3** 个 pair / **112** 断言：带进位加法 · 带借位减法 · 比较器 + `abs` |
| `tests/chlib/test_sequential_counters_dual.cpp`（**新**） | **4** 个 pair / **370** 断言：逻辑门/mux/三态 · 寄存器（保持 + 复位优先）· 计数器族（binary/BCD/gray/johnson/ring）· 边沿检测 |
| `tests/CMakeLists.txt` | 2 条 `add_dual_backend_test` |
| 计数锚点 | `CTEST_TOTAL` 219 → **221** · `VERILATOR_E2E_TOTAL` 51 → **53** · `DUAL_BACKEND_PARITY_TOTAL` 42 → **44** · `PAIRS` 223 → **230** · non-perf 208 → **210** · `tests/chlib/test_*.cpp` 37 → **39** |

## 期望值（**穷举手推**，禁抄仿真输出）

全部写进两个文件头。几个关键推导：

- **`borrow_out`**：实现是 `a - b - bin` 的 **N+1 位**结果取 bit N ⇒ ⇒ 是
  「**绝对值**意义上的借位」（`0x00-0x01-0 = 0x1FF` ⇒ bit8 = 1）
  ⇒ ⇒ 与 `a < b` 在 `bin=0` 时等价、`bin=1` 时**不同**；
- **`abs`**：符号位语义 ⇒ `abs(0x81) = 0x7F`（= `0 - 0x81`）；
- **计数器**：**tick N 读到的是【第 N 次推进后】的值** ⇒ ⇒ 第 1 拍就是 1；
  周期：binary = 16 · BCD = **10** · gray = **16** · johnson = 8 · ring = **4**；
- **`gray_counter` 输出是组合的**（`bin ^ (bin>>1)`）⇒ ⇒ 与当前 `bin` **同拍**；
- **`bcd_counter` 的 `carry` 是组合的**（`(counter == 9) & en`）⇒ ⇒ 与同拍 count 对应。

## Verification（前瞻 · 可勾选）

- [ ] **N2′**：`112 == 2 × 56` · `370 == 2 × 185`
- [ ] 期望值**全部手推**
- [ ] 三个原文件 **MUST NOT 修改**
- [ ] 变异证明：M1 `register_` 去掉 `en` 门控（失去「保持」）⇒ 红 ·
      M2 `add_with_carry` 的 `carry_out` 取 bit0 ⇒ 红
- [ ] `ctest -LE perf` 零回归 · 既有 `test_arithmetic` / `test_sequential` /
      `test_logic` 不回归
- [ ] 三个真缺陷**如实登记**且**钉住当前行为**
- [ ] 计数锚点同步且两个护栏双绿
