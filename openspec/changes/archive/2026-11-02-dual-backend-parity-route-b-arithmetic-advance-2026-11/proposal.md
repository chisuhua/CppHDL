# 提案：路线 B（新文件）—— `chlib/arithmetic_advance.h` 首次进双后端 parity

- **依据**：`.rddf/plans/dual-backend-full-coverage-2026-11.md` §4 **H 批**（路线 B · 新增有意义的 dual 文件）+ §4 的「可转性 4 问」
- **规格影响**：**无 spec 变更**（`.openspec.yaml` 声明 `skip_specs: true`）
- **规模**：1 个新测试文件（3 个 `DUAL_BACKEND_TEST_PAIR` / 104 断言/后端）+ 1 条 CMake 注册 + 计数锚点
- **关联**：`objective-test-coverage-100pct` · capability `dual-backend-test-coverage`

## Why

`chlib/arithmetic_advance.h` 是**实测零双后端覆盖**的 7 个 chlib 组件之一，且**不在任何
既有批次清单里**（`tests/chlib/test_arithmetic_advance.cpp` 既不在 D 批也不在 E/F 批）
⇒ ⇒ 计划漏项的第二个实例（第一个是 `test_bitwise.cpp`，已落地）。

**不可转换，只能新写**：既有测试用**裸 context**（`ctx_swap`，零 `ch_device`，无顶层端口）
⇒ 命中 capability `dual-backend-test-coverage` 的 R6 排除项 "bare-context tests"
⇒ ⇒ Verilator 后端只能观测**顶层端口**。

**为什么这 6 个函数值得钉**：每一个都是**进位 / 移位链**，而 interpreter（逐节点拓扑求值）
与 Verilog（连续赋值 + 位宽截断）在**进位传播**与**中间位宽截断**上最易分叉：

| 函数 | 承重点 |
|---|---|
| `carry_lookahead_adder` | 先扩到 `N+1` 位 ⇒ **扩位方式**决定 `carry_out`（少扩一位时 `sum` 仍对 ⇒ 只断 `sum` 抓不到） |
| `carry_save_adder` | `sum = a^b^c`、`carry = ((a&b)|(b&c)|(a&c)) << 1` ⇒ **先算后移** + 截断 |
| `wallace_tree_multiplier` / `booth_multiplier` | `2N` 位乘积 |
| `non_restoring_divider` | **两次** `select` 守卫（除零 ⇒ 商全 1、余 0） |
| `fixed_point_multiplier` | 乘到 `2N` → `>> Q` → **截断回 N** ⇒ 三处截断点 |
| `fixed_point_divider` | `a << Q` 扩到 `2N` → 除 → **截断回 N** |

## 🔴 读实现时挖出的三个既有缺陷 / 名不副实（**本 change 不修**，如实登记）

### ① 两个乘法器**函数体逐字相同**
`include/chlib/arithmetic_advance.h:56-73` 的 `wallace_tree_multiplier` 与
`booth_multiplier` 实现完全一致（都是 `ch_uint<2*N>(a) * ch_uint<2*N>(b)`）
⇒ ⇒ **既不是 Wallace 树、也不是 Booth 编码** ⇒ 与 `fwft_fifo`（承诺 FWFT 实则不是）、
`stream_arbiter_sequence`（注释承诺 round-robin 实则 priority）**同型**的「名字 over-promise」。
本 change 的处置：**断言两者逐位相同**（把当前事实钉住）；改名/重写会改变生成的 RTL
结构 ⇒ 需 **ADR** ⇒ **独立立项**。

### ② `carry_save_adder` 的 `carry` 静默截断
签名是 `ch_uint<N> carry`，而内部 `ch_uint<N+1> temp` 再 `temp << 1_d`（N+1 位）
⇒ 赋回 N 位时**最高位（= 真正的 carry-out）被丢掉**。
本 change 按**签名定义的当前行为**断言，并在向量表里**刻意**放了 `(0x80,0x80,0x00)`
这种「只有 carry-out 置位」的向量 ⇒ 该截断被**显式钉住**（将来若改成 `ch_uint<N+1>`，
本断言会转红 ⇒ 是一次**有意的**行为变更信号）。

### ③ `fixed_point_divider` 的除零保护是**死代码**
`include/chlib/arithmetic_advance.h:125` 是 `if (b == 0_d) { ... return all-ones; }`,
即 **C++ 控制流作用于硬件值**。机制（读代码可判，无需运行）：
`ch_bool::operator uint64_t()`（`src/lnode/bool.cpp:44-49`）**只对 const 节点返回真值，
非 const 恒返 0** ⇒ 端口形态下 `b == 0_d` 是非 const `eq` 节点 ⇒ `if` **建图期恒假**
⇒ 那条 `return` 从不发射 ⇒ 实际硬件是裸 `(a<<Q)/b`，**b == 0 无任何保护**。
本 change **不喂** `b == 0`（interpreter 侧是原生整数除法 ⇒ **SIGFPE**；为一条已知死代码
触发进程崩溃不值得）⇒ **独立立项**。修它 MUST 同时把守卫改成**真 `select`**
（参照 `non_restoring_divider` 的写法）。

⚠️ **对照（本文件的可执行判据）**：`non_restoring_divider` 用的是**真 select** 守卫
（`select(is_zero, ~0, dividend / select(is_zero, 1, divisor))`）⇒ 那是**真保护**
⇒ ⇒ 本文件**会**测它的 `b == 0`（商 `0xFF` / 余 `0`）⇒ ⇒ 「同仓两种除零守卫、
一种有效一种无效」这个对比本身就变成了判据。

## ⚠️ 判据边界（如实登记）

- `fixed_point_adder` / `fixed_point_subtractor` 的模板参数 `Q` **不参与运算**
  （对加减而言是正确的 —— Q 只影响解释），所以本文件对它们只断言**位模式**，
  不声称验证了「Q 的语义」（Q 真正生效的是乘/除的两个移位量）。
- 向量全部是**契约内输入**：除法只在 `b != 0` 时喂（理由见缺陷 ③）。

## 判别力（变异证据；每条先确认仍可编译；还原用 `cp` 后重建）

| 变异 | 内容 | 转红位置 |
|---|---|---|
| A1 | CLA 的 `carry_in` 被忽略 | `:254` `cla_sum` |
| A2 | CLA 的 `carry_out` 取 `total[N-1]` | `:256` `cla_carry` |
| A3 | CSA 丢掉第三个操作数（`a^b`） | `:258` `csa_sum` |
| A4 | CSA 的进位忘掉 `<< 1` | `:260` `csa_carry` |
| A5 | wallace 的 `*` 换成 `+` | `:277` `wallace` |
| A6 | booth 的 `*` 换成恒等 | `:281` `booth` |
| A7 | 除零守卫返回 `0` 而非全 1 | `:283` `quot` |
| A8 | 余数忘掉 `- quotient*divisor` | `:285` `rem` |
| A9 | `fmul` 的 `>> Q` 改成 `>> Q-1` | `:305` `fmul` |
| A10 | `fdiv` 的 `<< Q` 改成 `<< Q+1` | `:307` `fdiv` |
| A11 | `fadd` 的 `+` 改成 `-` | `:301` `fadd` |
| A12 | `fsub` 的 `-` 改成 `+` | `:303` `fsub` |

⇒ **12/12 全抓，且 12 个断言组各有一个专属变异**（转红行互不相同 ⇒ 无「一条变异
只打到第一个断言」的假覆盖）。

### ⚠️ 一处手算笔误（如实登记，作为方法论实例）

向量表首版把 **CSA 列**按 `(a,b,cin)` 猜着填了 —— 而 CSA 吃的是 `(a,b,c)` 这个
**不同的三元组** ⇒ 2 行错（`0xFF^0x01^0xFF` 是 `0x01` 不是 `0xFF`）⇒ ⇒ 被**双后端
一致地**证伪（两侧都给 `0x01`）⇒ 改为按 `a^b^c` 与 `((a&b)|(b&c)|(a&c))<<1` 逐行重算。
⇒ 教训：**同一张表里混用不同入参组合**是新型笔误（上一次是 `bit_field_insert` 的 OR
运算笔误）；「两后端一致地红 ⇒ 同一处推导错」这条反向判据再次生效。

## What Changes

| 文件 | 内容 |
|---|---|
| `tests/chlib/test_arithmetic_advance_dual.cpp`（**新**） | 3 pair：`AddersDut`（CLA 含 `carry_in` + CSA 三操作数，8 向量 × 4 输出）· `MulDivDut`（wallace / booth / 非恢复除法含 `b==0`，10 向量 × 4 输出）· `FixedPointDut`（N=16/Q=8 四则，8 向量 × 4 输出）⇒ **104 断言/后端** |
| `tests/CMakeLists.txt` | 1 条 `add_dual_backend_test`（含三个缺陷的登记注释） |
| `AGENTS.md` | 4 个计数锚点 + NOTES 计数 + 本批 NOTES 条目 |
| 计数锚点 | `CTEST_TOTAL` 230 → **231** · `VERILATOR_E2E_TOTAL` 62 → **63** · `DUAL_BACKEND_PARITY_TOTAL` 53 → **54** · `PAIRS` 251 → **254** · non-perf 219 → **220** · `tests/chlib/test_*.cpp` 44 → **45** |

## Verification（前瞻 · 可勾选）

- [x] **N2′**：**208 == 2 × 104**
- [x] 变异 **12/12 全抓**，且每个断言组有专属变异（转红行互不相同）
- [x] 全部输入端口**逐个显式驱动**
- [x] 期望值**手推**（公式逐条对照实现书写；一次笔误已被双后端一致证伪并改正）
- [x] **原文件 `tests/chlib/test_arithmetic_advance.cpp` MUST NOT 修改**（已遵守）
- [x] 只断言**契约内**输入（除法不喂 `b == 0` 的 `fixed_point_divider`，理由见缺陷 ③）
- [x] 三个既有缺陷**已登记**且**未在本 change 修复**（改名/修守卫需 ADR 或独立立项）
- [x] 全量重建后 `ctest -LE perf` 零回归
- [x] 计数锚点同步且两个计数护栏双绿
- [x] 归档：`--skip-specs`（`.openspec.yaml` 已显式声明）
