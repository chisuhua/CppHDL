# 提案：路线 B（新文件）—— `chlib/bitwise.h` · `chlib/onehot.h` · `chlib::ForwardingMux` 首次进双后端 parity

- **依据**：`.rddf/plans/dual-backend-full-coverage-2026-11.md` §4 **H 批**（路线 B · 新增有意义的 dual 文件）+ §4 的「可转性三问」前置条件
- **规格影响**：**无 spec 变更**（`.openspec.yaml` 声明 `skip_specs: true`）
- **规模**：3 个新测试文件（6 个 `DUAL_BACKEND_TEST_PAIR`）+ 3 条 CMake 注册 + 计数锚点
- **关联**：`objective-test-coverage-100pct` · capability `dual-backend-test-coverage`

## Why

本批**不是**"把既有单后端文件转成 dual"，而是**新增 3 个 dual 文件**，覆盖 3 个
**双后端零覆盖**的组件。依据来自实测的覆盖矩阵（"含 `DUAL_BACKEND_TEST_PAIR(` 调用"
的源文件全集 × `include/chlib/*.h`）：

| 组件 | 既有单后端测试 | 为什么**不能转换** | 覆盖价值 |
|---|---|---|---|
| `chlib/bitwise.h` | `tests/chlib/test_bitwise.cpp`（9 用例） | **全部是裸 context**（无 `ch_device` / 无顶层端口）⇒ 命中 R6 排除项 "bare-context tests" | 6 个检测器 + 3 个位操作 = **9 条 select 链 / 移位 / 掩码**语义 |
| `chlib/onehot.h` | `tests/chlib/test_onehot.cpp`（7 用例 / 21 SECTION） | **21 个 Catch2 SECTION** ⇒ 命中 R6 排除项 "SECTION re-entry" | one-hot 译码/编码 = **N 级 select 链**（方向敏感） |
| `chlib::ForwardingMux` | `tests/chlib/test_forwarding.cpp`（8 用例） | 8 个里 **6 个**用**函数式**接口（裸 context）⇒ 同上 | RV32I 关键路径上的**三档位选择 + `is_jump` 取返回地址**（ADR-047 D4） |

⚠️ **而原计划批次表（E/F 批）里根本没有 `test_bitwise.cpp` 与 `test_arithmetic_advance.cpp`**
（`chlib/arithmetic_advance.h` = CLA / CSA / Wallace / Booth / 非恢复除法 / 定点，
同样**零 dual 覆盖**且同样是裸 context）⇒ ⇒ **计划漏了两个高价值目标**
⇒ ⇒ 本批先落地其中最容易的 3 个，`arithmetic_advance` 留作下一批（工作量更大：
每个函数都是**多级进位链**，期望值需逐位手推）。

## ⚠️ 三问判据（本批新增的前置条件，已回填 plan §4）

每个候选文件先答三问，任一为「否」就不转：

| # | 判据 | 怎么查 | 本批结论 |
|---|---|---|---|
| ① | 语义**是否已被别的 dual 文件覆盖**？ | 按**语义 + 判别形态**在既有 dual 文件里搜（不是按文件名） | `chlib/onehot.h` 与 `chlib/converter.h`（既有 `test_arithmetic_encoding_dual.cpp`）**不同组件** ⇒ 不重复 |
| ② | 断言形态**对目标维度有判别力**吗？ | 读断言实现 + 变异实测 | 见下「判别力」段 |
| ③ | **与后端无关**吗？（纯编译期 / codegen 文本断言 / 构图期拒绝） | 读实现 | 本批 3 个文件全部**跑仿真**且读**顶层端口** ⇒ 通过 |

## 三问之外的第 4 问（本批踩到 ⇒ 补进判据）

**「形态上不可转换」≠「不值得覆盖」**：本批 3 个目标的既有单后端测试都**不可转换**
（裸 context / SECTION）⇒ ⇒ 若只做「转换」路线，它们永远进不了矩阵
⇒ ⇒ **必须允许「新写」路线**（自带 DUT + 手推期望），且**必须如实登记它是新写**。

## 判别力（变异证据；每条先确认仍可编译；还原用 `cp` 后重建）

| 文件 | 变异 | 内容 | 结果 |
|---|---|---|---|
| `test_bitwise_dual` | M1 | `leading_zero_detector` 链方向反转 | 转红（`lead_zero`） |
| | M2 | `population_count` 的逐位贡献取反 | 转红（`popcount`） |
| | M3 | `bit_reversal` 移位量差 1 | 转红（`reversed`） |
| | M4 | `first_set_bit_detector` 链方向反转 | 转红（`first_set`） |
| | M5 | `bit_field_insert` 忘记清目标域 | 转红（`inserted3`） |
| | M6 | `trailing_zero_detector` 的 `found_one` 反馈取反 | 转红（`trail_zero`） |
| | M7 | `leading_one_detector` 链方向反转 | 转红（`lead_one`） |
| | M8 | `trailing_one_detector` 的 `found_zero` 反馈取反 | 转红（`trail_one`） |
| | M9 | `bit_swap` 的两个源位未互换 | 转红（`swapped`） |
| `test_onehot_dual` | O1 | `onehot_dec` 的 `bit_select` 索引翻转 | 转红 |
| | O2 | `onehot_enc` 的移位量翻转 | 转红 |
| | O3 | `onehot_enc` 的比较取反 | 转红 |
| `test_forwarding_dual` | F1 | `is_jump` 判定取反（ADR-047 D4 的核心） | 转红 |
| | F2 | 档位 1 的比较取反 | 转红 |
| | F3 | 档位 1 与档位 2 的源互换 | 转红 |

⇒ **15/15 全抓**。

### ⚠️ 如实登记的两条**判别力边界**

1. **`onehot_dec` / `onehot_enc` 的「链方向」在合法输入下不可判别**：两者都是
   `select(bit_i, i, result)` 形态，方向反转只改变**多位置位**输入的结果，而文档
   明写多位置位/全 0 ⇒ 输出**未定义** ⇒ ⇒ 本文件**只断言合法单置位输入**
   ⇒ ⇒ 「谁胜出」这类方向变异**抓不到**（方向敏感性由 `bitwise` 文件的
   `first_set_bit_detector`（最低位胜出）与 `onehot_dec`（最高位胜出）**相反**这一点
   记录在文件头 ⇒ 供后人参考，但**不**作为断言）。
2. **`bit_field_insert` 的 `inserted2` 与 `inserted3` 由同一函数产出** ⇒ M5 先在
   `inserted3` 转红 ⇒ `inserted2` 没有**专属**变异（共享同一断言组）。

### ⚠️ 一条**期望值手算笔误**（如实登记，作为方法论实例）

`bitwise` 文件的 `inserted2` 期望值手算时把 `0xE2 | 0x14` 写成 `0xE6`（实为 **0xF6**）
⇒ ⇒ **被双后端一致地证伪**（两侧都给 0xF6）⇒ ⇒ 修正为「按实现公式逐行重算」
（公式取自 `include/chlib/bitwise.h`，**不是抄仿真输出**）。
⇒ **教训**：手推期望必须**逐位复核 OR/AND 的结果**；「双后端一致地红」恰恰说明
**两侧同构**（不是 parity 分歧）⇒ 这是「两后端一致 ⇒ 同一处推导错」这条反向判据的
又一次实例。

## What Changes

| 文件 | 内容 |
|---|---|
| `tests/chlib/test_bitwise_dual.cpp`（**新**） | 2 pair：`BitwiseDetectorsDut`（6 个检测器 × 10 向量 = 60 断言）· `BitwiseOpsDut`（位反转/位交换/2 种位域插入 × 10 向量 = 40 断言）⇒ **100 断言/后端** |
| `tests/chlib/test_onehot_dual.cpp`（**新**） | 3 pair：`onehot_dec_module<4>` · `onehot_enc_module<4>`（**零 DUT 复制**，直接用库组件）· 函数式 `onehot_dec`/`onehot_enc`（运行期输入）⇒ **16 断言/后端** |
| `tests/chlib/test_forwarding_dual.cpp`（**新**） | 1 pair：`chlib::ForwardingMux<32>`（**零 DUT 复制**）× 5 个档位组合 ⇒ **10 断言/后端** |
| `tests/CMakeLists.txt` | 3 条 `add_dual_backend_test` |
| 计数锚点 | `CTEST_TOTAL` 226 → **229** · `VERILATOR_E2E_TOTAL` 58 → **61** · `DUAL_BACKEND_PARITY_TOTAL` 49 → **52** · `PAIRS` 243 → **249** · non-perf 215 → **218** · `tests/chlib/test_*.cpp` 41 → **44** |

## Verification（前瞻 · 可勾选）

- [x] **N2′**：`test_bitwise_dual` **200 == 2 × 100** · `test_onehot_dual` **32 == 2 × 16** ·
      `test_forwarding_dual` **20 == 2 × 10**
- [x] 变异 **15/15 全抓**（每条先确认仍可编译；还原用 `cp` 后重建）
- [x] 全部 3 个文件**逐个显式驱动全部输入端口**
- [x] **只断言契约内的行为**（不把「多位置位 / 全 0 / 档位 3」等**未定义**行为冻结成断言）
- [x] 全量重建后 `ctest -LE perf` 零回归
- [x] 计数锚点同步且两个计数护栏双绿
- [x] 归档：`--skip-specs`（`.openspec.yaml` 已显式声明）
