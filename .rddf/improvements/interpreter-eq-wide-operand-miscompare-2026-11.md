---
name: interpreter-eq-wide-operand-miscompare-2026-11
priority: P1
source: ADR-065 实施期实测挖出（chlib 仲裁器族修复过程中）
phase: phase-5
category: framework
type: bugfix
created_at: 2026-11-03
evidence: test:tests/test_wide_shl_dual.cpp
---

# Improvement: 多 word 左移（`bv_shl_vector`）产生虚假高位 / 丢失跨 word 进位

> ⚠️ **标题已被订正**：立项时的标题是「interpreter 的 `eq` 在宽操作数下判错」。
> 2026-11-03 的根因定位证明 **`eq` 是对的**，缺陷在 `include/bv/bv_shift.h` 的
> `bv_shl_vector` ⇒ ⇒ 见 §6。原症状（`eq` 判错）保留在 §1 作为**现象记录**。

- **Date drafted**: 2026-11-03
- **Status**: ✅ **已修复**（2026-11-03 · change `wide-shl-truncate-word-carry-2026-11`）
  ⇒ ⇒ 根因 = `bv_shl_vector` 的两处笔误 + 一处漏判界（§6.2）
- **来源**：ADR-065（`chlib-arbiter-buildtime-eval-fix-2026-11`）实施期 —— 修
  `round_robin_arbiter` 时发现「N=64 时 `next_ptr` 恒 0」，逐层二分后定位到
  **框架层的 `eq`**（⇒ 当时**误判**，真因在 `shl`）。

## 1. Why（现象记录）

### 1.1 现象

`chlib::round_robin_arbiter<64>`（selector 层轮转原语）在 **N=64** 时**不轮转**：
唯一有效 = bit `k`（`k ≥ 2`）⇒ `next_ptr` **恒 0**（期望 `k+1`）。
而 `k = 0` / `k = 1` **正常** ⇒ ⇒ 与「移位量」强相关。

### 1.2 最小复现（**独立探针**，不入仓）

把「64 位值 `a`」与「`ch_uint<64>(1) << pos`」（`pos` 是 6 位，结果节点 **127 位**）
直接比较：

```
pos=0 a=0x1   eq=1 OK
pos=1 a=0x2   eq=1 OK
pos=2 a=0x4   eq=0 **FAIL**（应为 1）
pos=3 a=0x8   eq=0 **FAIL**
pos=4 a=0x10  eq=0 **FAIL**
pos=5 a=0x20  eq=0 **FAIL**
```

⚠️ **当时的推断是错的**：立项时写「`shl` 的值是对的（`0x4`），错的是 `eq`」——
那是因为探针只看了 `shl` 的**低 64 位**（经 `ch_uint<64>` 收窄）。真实的 127 位
节点里 `1 << 2` = `2^66 + 2^2` ⇒ ⇒ **高 64 位里有一位虚假的 1** ⇒ ⇒ `eq` 把
「2^2」和「2^66 + 2^2」比较 ⇒ 返回 0 是**正确行为**。

### 1.3 为什么是 127 位

`ch_uint<N>(1_d) << pos` 的结果宽度 = `N + (2^pos_bits - 1)` = 64 + 63 = **127 位**
（桶形移位的全量输出宽度）。生成 Verilog 里可直接看到：

```verilog
logic [126:0] shl_127;
assign shl_127 = uint_lit_129 << bits_254;   // uint_lit_129 = 64'h1
assign eq_eq_63 = mux_select_126 == _shl_127;  // 64 位 vs 127 位（Verilog 零扩展 ⇒ 正确）
```

### 1.4 影响面

- **已确认受害者**：`chlib::round_robin_arbiter<64>`（ADR-065 已**绕开**：
  改用 `bit_select(grant, pos)` ⇒ 不再产生 127 位中间节点）。
- **同型风险面**：任何「多 word（结果 > 64 位）左移」—— 包括
  `ch_uint<N> << <variable>` 且 `N + 2^k - 1 > 64`、`ch_uint<N> << k` 且
  `N + k > 64`、以及 `bitvector<N>::operator<<` / `operator<<=`（N > 64）。
  ⇒ ⇒ 不限于「移位结果参与比较」—— 任何消费该值的下游都拿到错值。
- ⚠️ **未做全仓普查**（B8 已否决「全仓普查」义务）⇒ 影响面未知，只登记本条。

## 2. What Changes（实际落地）

| 文件 | 改动 |
|---|---|
| `include/bv/bv_shift.h` | `bv_shl_vector`：`out[i] = (in[i-s] << d) \| (in[i-s-1] >> (64-d))`（**两边都判上下界**） |
| `include/bv/bitvector.h` | `bitvector::operator<<=`（同址调用）：先拷一份输入再调 `bv_shl` |
| `tests/test_wide_shl_dual.cpp`（**新**） | **1** 个 `DUAL_BACKEND_TEST_PAIR` / **154 == 2 × 77** 断言 |
| `tests/test_bitvector.cpp` | 新增 `TEST_CASE("bitvector: Multi-word left shift")`（**独立模型**逐位比对 + 同址 `<<=`） |
| `openspec/specs/dual-backend-test-coverage/spec.md` | **ADDED ×1**（「多 word 左移 MUST 与 Verilog 的 `<<` 一致」） |

⇒ ⇒ **不需要 architect 裁决**：原先列的三个候选（A 改比较语义 / B 建图期统一宽度 /
C 文档化）**全部作废** —— 比较语义本来就对，问题在一个**纯粹的错误实现**上
⇒ ⇒ 与 `mux` 的槽位压窄（`mux-slot-width-preservation-2026-11`）同一性质。

## 3. Acceptance（验收判据 —— 全部已达成）

- [x] 最小复现（§1.2）**修后** `eq == 1`（`pos = 0..5` 全部）
- [x] **两后端一致**：`tests/test_wide_shl_dual.cpp`（照 **1** 个 dual pair，
      `154 == 2 × 77`）⇒ 路径 A（源 1 word / 目标 3 word）+ 路径 B（源 2 word /
      目标 4 word）× 11 组激励 × 7 个 64 位切片
- [x] 既有 `ctest -LE perf` **232/232** 零回归
- [x] **生成 Verilog 零变化**（纯 interpreter 侧修复 ⇒ 无 RTL diff 门槛）

## 4. Impact

- **爆炸半径**：所有**多 word 左移**（interpreter 侧）⇒ 修后与 Verilog 逐位一致。
  ⇒ 已有测试零回归 ⇒ 说明既有用例**都没有覆盖多 word 左移**
  （`tests/test_bitvector.cpp` 里唯一的多 word 用例**整段被注释掉**）。
- **风险**：接近零 —— Verilog 侧行为不变；改动只让 interpreter **追上** Verilog。
- **不做**：`bv_shl<true>`（有符号）的 vector 路径 —— 全仓**零调用**
  （3 个调用点全是 `bv_shl<false>`）⇒ 登记为已知缺口，不顺手改。

## 5. 备注（为何 ADR-065 没有顺手修）

ADR-065 是**仲裁器族**的缺陷修复 change ⇒ 当时判断「修框架层的 `eq` 会把爆炸半径
从『两个 chlib 组件』扩大到『全仓所有比较运算』」⇒ ⇒ 独立立项（本文件）。
⇒ 根因定位后，爆炸半径实际是「**多 word 左移**」而不是「所有比较」
⇒ ⇒ 修复面反而更小、更安全。

## 6. ✅ 根因定位与修复（2026-11-03）

### 6.1 定位过程（实测，逐步否定前一个假设）

| 步 | 手段 | 结果 |
|---|---|---|
| 1 | 复现 `eq` 判错 | 确认 `pos ≥ 2` 错、`0/1` 对 |
| 2 | **只测比较原语**（`sdata_type(1<<p, 64) == sdata_type(1<<p, 127)`） | **全部 OK** ⇒ ⇒ `eq` / `bv_eq` / `bitvector::operator==` **都是对的** |
| 3 | 读实现 | `op::Eq::eval` → `*src0 == *src1` → `bv_eq_truncate` → `bv_eq` ⇒ `bv_eq_vector` 用 `BitAccessor` 做零扩展 ⇒ 语义正确 |
| 4 | **只测移位原语**（`bv_shl_truncate(dst=127bit, src=1@64bit, dist=p)`） | `p=0,1` OK；`p ≥ 2` **高位多出 `2^(64-p)`** ⇒ ⇒ **定位到 `bv_shl_vector`** |
| 5 | 读 `bv_shl_vector` | 跨 word 进位写成 `in[i-s-1] << (WORD_SIZE - shift_bits)`（**方向反了**），且第 ① 句少了 `<< shift_bits` |
| 6 | 独立参考模型（256 位容器逐位置位）对 192 组参数扫描 | 修前 **102/192 FAIL** ⇒ 修后 **0/192 FAIL** |

### 6.2 根因（`include/bv/bv_shift.h`）

```cpp
// 修前
out[i] = (i - shift_words < in_num_words) ? in[i - shift_words] : T(0);   // ① 少了 << shift_bits
if (i - shift_words - 1 >= 0) {
    out[i] |= in[i - shift_words - 1] << (WORD_SIZE - shift_bits);        // ② 方向反了（应为 >>）
}                                                                          // ③ 不判上界 ⇒ 越界读 in[]
```

三个后果：

1. **虚假高位**：`in[0] << (64-d)` 把**源**再左移一次塞进**高** word
   ⇒ ⇒ 实测 `1 << 2`（127 位槽位）= `2^66 + 2^2`（Verilog 给 `2^2`）。
2. **丢失跨 word 进位**：真正该进位的 `in[0] >> (64-d)` 一位都没进来
   ⇒ ⇒ 实测 `0x8000000000000000 << 1` = **0**（应为 `2^64`）。
3. **越界读**：只判 `i-s-1 >= 0`、不判 `< in_num_words`
   ⇒ ⇒ `out_num_words > in_num_words + shift_words` 时读到源数组之外的内存
   （`out=191bit / in=64bit` 即可触发）。

### 6.3 为什么长期零覆盖

- **窄宽度走不到**：`out_size <= 64 && in_size <= 64` 走 `bv_shl_scalar`（那一路正确）。
- **既有 bv 测试的多 word 用例整段被注释掉**（`tests/test_bitvector.cpp:145-264`
  的 `/* ... */`）⇒ ⇒ 「多 word 左移」这一格**零覆盖**。
- **双后端看不见**：没有任何 dual 用例构造多 word 左移 ⇒ 分歧无判据。
- **「值正确」的错觉**：探针若只看低 64 位切片，虚假高位**完全不可见**
  （它落在 bit ≥ 64）⇒ ⇒ 这正是 §1.2 误判的成因。

### 6.4 修复与判据

- **修法**：`out[i] = (in[i-s] << d) | (in[i-s-1] >> (WORD_SIZE - d))`，
  两个索引都判 `[0, in_num_words)`。
- **`bitvector::operator<<=` 的别名**：`bv_shl_vector` 第一件事是
  `std::fill_n(out, ...)` ⇒ ⇒ 同址调用（`operator<<=` 就是这么调的）会把**输入**
  清成 0（实测 128 位 `<<= 1` 得到 **0**）⇒ ⇒ 在 `operator<<=` 里先拷一份输入，
  并在 `bv_shl_vector` 的注释里写明「out / in MUST NOT 别名」。
- **判据（N2′）**：`tests/test_wide_shl_dual.cpp` **154 == 2 × 77**
  ⇒ 期望值由**独立模型**给出（256 位容器逐位左移 = 定义式，与「循环 + word 进位」
  的实现无共同代码路径）。
- **bv 层判据**：`tests/test_bitvector.cpp` 的 `Multi-word left shift`
  （11 组参数 × {`operator<<`, `operator<<=`} × 逐 word 比对）。

### 6.5 变异验证（**每条先确认仍可编译**）

| 变异 | 内容 | `test_wide_shl_dual` | `test_bitvector` |
|---|---|---|---|
| M1 | `bv_shl_vector` 还原为旧实现 | **1 failed / 1 passed**（⇒ **只有 interpreter 半边红**，verilator 半边绿 = 分歧证据）| 1 failed |
| M2 | `operator<<=` 还原为同址调用 | 全绿（**如实登记**：interpreter 走 `bv_shl_truncate`，dst ≠ src）| 1 failed |

⚠️ **M2 的边界**：`operator<<=` 的别名缺陷**只有** bv 层用例能抓（图层不可达）
⇒ ⇒ 两个判据**不可互相替代**。
