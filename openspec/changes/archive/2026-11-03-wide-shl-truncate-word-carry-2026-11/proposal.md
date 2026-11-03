# 提案：多 word 左移的跨 word 进位（修 `bv_shl_vector` + 补双后端判据）

- **关联 capability**：`dual-backend-test-coverage`（**ADDED ×1**）⇒ 归档**禁** `--skip-specs`
- **行为变更**：**有**（interpreter 的多 word 左移结果 ⇒ 向 Verilog 对齐）
- **立项来源**：`.rddf/improvements/interpreter-eq-wide-operand-miscompare-2026-11.md`
  （⚠️ 立项时的标题「`eq` 判错」**已被证伪**，见下）

## Why

### 现象

`chlib::round_robin_arbiter<64>` 不轮转（唯一有效 = bit `k ≥ 2` ⇒ `next_ptr` 恒 0）。
最小复现：`ch_uint<64> a == (ch_uint<64>(1) << pos)`（`pos` 6 位 ⇒ 结果节点 **127 位**）
⇒ `pos = 0,1` 正确、**`pos = 2..5` 返回 0（应为 1）**。

### 根因（不是 `eq`）

| 步 | 手段 | 结论 |
|---|---|---|
| 1 | 只测比较原语（`sdata_type(1<<p, 64) == sdata_type(1<<p, 127)`） | **全部正确** ⇒ `eq` / `bv_eq` / `bitvector::operator==` 无问题 |
| 2 | 只测移位原语（`bv_shl_truncate(dst=127bit, src=1@64bit, dist=p)`） | `p ≥ 2` 时**多出 `2^(64-p)`** ⇒ 定位到 `bv_shl_vector` |

`include/bv/bv_shift.h` 的 `bv_shl_vector`：

```cpp
out[i] = (i - shift_words < in_num_words) ? in[i - shift_words] : T(0);  // ← 少了 << shift_bits
if (i - shift_words - 1 >= 0) {
    out[i] |= in[i - shift_words - 1] << (WORD_SIZE - shift_bits);       // ← 方向反了（应 >>）
}                                                                        // ← 且不判上界 ⇒ 越界读
```

⇒ 三个后果：① **虚假高位**（`1 << 2` 在 127 位里 = `2^66 + 2^2`）· ② **丢失跨 word 进位**
（`0x8000000000000000 << 1` = 0，应 `2^64`）· ③ **越界读** `in[]`。

⇒ Verilog 的 `<<` 天然正确 ⇒ ⇒ **interpreter ↔ Verilog 分歧**。

### 为什么长期零覆盖

- `out_size <= 64 && in_size <= 64` 走 `bv_shl_scalar`（那一路正确）⇒ 窄宽度到不了；
- `tests/test_bitvector.cpp` 的多 word 用例**整段被注释掉**（`:145-264`）⇒ 零覆盖；
- 没有 dual 用例构造多 word 左移 ⇒ 分歧无判据；
- 立项时的探针只看 `shl` 的**低 64 位**（虚假高位落在 bit ≥ 64）⇒ ⇒ **误判为 `eq` 的语义问题**。

## What Changes

| 文件 | 改动 |
|---|---|
| `include/bv/bv_shift.h` | `out[i] = (in[i-s] << d) \| (in[i-s-1] >> (64-d))`（两个索引都判上下界）+ 注释写明「out / in MUST NOT 别名」 |
| `include/bv/bitvector.h` | `bitvector::operator<<=`：先拷一份输入再调 `bv_shl`（同址调用会被 `fill_n` 清空） |
| `tests/test_wide_shl_dual.cpp`（**新**） | **1** 个 `DUAL_BACKEND_TEST_PAIR` / **154 == 2 × 77** 断言 |
| `tests/test_bitvector.cpp` | 新增 `TEST_CASE("bitvector: Multi-word left shift")`（独立模型逐位比对 + 同址 `<<=`） |
| `openspec/specs/dual-backend-test-coverage/spec.md` | ADDED ×1（1 Requirement + 4 Scenario） |

## 为什么不是「改比较语义」

立项时的三个候选（A 改 `bv_eq` 的宽度语义 / B 建图期统一宽度 / C 文档化）
**全部作废** —— 比较原语本来就正确（逐值验证过）。
⇒ ⇒ 这和一个纯粹的**错误实现**有关，与 `mux` 的槽位压窄
（`mux-slot-width-preservation-2026-11`）同性质：`node->size()` 是权威，
`eval` 必须按它算。

## Verification

- [x] 最小复现修后 `pos = 0..5` 全部 `eq == 1`
- [x] `test_wide_shl_dual` **154 == 2 × 77**（N2′）
- [x] `test_bitvector` `Multi-word left shift`（11 组参数 × {`<<`, `<<=`} × 逐 word）
- [x] 独立参考模型对 192 组参数扫描：修前 **102/192 FAIL** ⇒ 修后 **0/192**
- [x] 全量重建 0 error · `ctest -LE perf` **232/232** 零回归
- [x] 变异 M1（还原 `bv_shl_vector`）⇒ dual 半边红（**只有 interpreter**）+ bv 层红
- [x] 变异 M2（还原 `operator<<=` 同址）⇒ **只有** bv 层红（如实登记：图层不可达）
- [x] 生成 Verilog **零变化**（纯 interpreter 侧修复）
