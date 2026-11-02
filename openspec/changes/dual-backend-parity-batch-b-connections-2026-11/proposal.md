# 提案：A5 批次 B（连接语义组）—— 4 个 `_behavior` 文件 / **16** 个 dual pair

- **依据**：`.rddf/plans/dual-backend-full-coverage-2026-11.md` §4 批次 B
  （B1 已于 2026-11-01 落地：`chlib::AssertChecker`）⇒ ⇒ 本 change 是**批次 B
  的剩余部分**（**连接语义**组）
- **规格影响**：**无 spec 变更**（`.openspec.yaml` 显式声明 `skip_specs: true`）
  ⇒ ⇒ 归档**可用** `--skip-specs`
- **规模**：4 个新测试文件 + 4 条 CMake 注册 + 计数锚点
- **关联**：`objective-test-coverage-100pct`（阈值 60）· capability
  `dual-backend-test-coverage`

## Why

`objective-test-coverage-100pct` 的完成判据是「**双后端 parity 源文件数** ≥ 60」
⇒ ⇒ 当前 **37**（本批 +4 后）⇒ ⇒ 仍差 23。

⚠️ **本批的立项理由是「这些语义值得双后端覆盖」，不是「凑数」**
（critic SHOULD_FIX #2 · 2026-11-01：**「补足到 60」的动机表述本身有诱导凑数
的嫌疑 ⇒ ⇒ MUST 改写**）⇒ ⇒ 下面逐条给出**为什么值得**。

| 转换对象 | 为什么值得双后端覆盖 |
|---|---|
| `test_bool_connection`（4 例） | `operator<<=` 的**连线语义**（bool / uint<1> / reg / literal 四种源）⇒ ⇒ codegen 与 interpreter 是**两条独立实现路径** ⇒ ⇒ 连线是否等价**只有**双后端能证明 |
| `test_operator_connection`（4 例） | 连线 + `select` + `bits<>` + 寄存器推进 ⇒ ⇒ **组合逻辑 + 时序**的联合语义 |
| `test_wires_connection`（5 例） | wire 链（4 级）+ 多级 wire 加法（**溢出截断**）+ wire→reg 捕获 + **宽度零扩展** |
| `chlib/test_multi_if_acceptance`（3 例） | `multi_if` 的**零扩展**金标（`0x0A5` / `0x00F`，**高位为 1 的形态** ⇒ 误用 `sext` 可判别）+ **直接赋 `ch_out` 与「先转 `ch_uint`」的逐位等价性**（ADR-052 的 E1/E3） |

⇒ ⇒ 这四条在**改动前**都只有 interpreter 一条路径 ⇒ ⇒ 「codegen 路径与
interpreter 路径在连线上是否一致」这条问题**从未被问过**。

## What Changes

| 文件 | 内容 |
|---|---|
| `tests/test_bool_connection_behavior.cpp`（**新**） | 4 个 dual pair（14 断言） |
| `tests/test_operator_connection_behavior.cpp`（**新**） | 4 个 dual pair（32 断言） |
| `tests/test_wires_connection_behavior.cpp`（**新**） | 5 个 dual pair（68 断言） |
| `tests/chlib/test_multi_if_acceptance_behavior.cpp`（**新**） | 3 个 dual pair（12 断言） |
| `tests/CMakeLists.txt` | 4 条 `add_dual_backend_test` |
| 计数锚点 | `CTEST_TOTAL` 210 → **214** · `VERILATOR_E2E_TOTAL` 42 → **46** · `DUAL_BACKEND_PARITY_TOTAL` 33 → **37** · `PAIRS` 194 → **210** · non-perf 199 → **203** · `tests/test_*.cpp` 122 → **125** · `tests/chlib/test_*.cpp` 32 → **33** |

## 被**刻意排除**的（以及为什么）—— **不为了计数而转换**

| 排除项 | 理由 |
|---|---|
| `test_axi_pwm_simple` 的 `Simulation` 用例 | ① 它只断 `REQUIRE(true)` ⇒ **零判别力**（转了也只是「跑起来了」）；② PWM **已被** `tests/test_axi_pwm_behavior.cpp` 覆盖 ⇒ ⇒ **重复** |
| `test_jit_concat` · `test_interpreter_jit_compat` | 身份是 **JIT-vs-interpreter** parity ⇒ ⇒ 与「interpreter-vs-verilator」**正交** ⇒ ⇒ dual 化**零增量价值**（plan §4 剔除项 ① 亦如此判定） |
| `chlib/test_multi_if_acceptance` 的 3 个 `REQUIRE_THROWS_AS` 用例 | **构图期**拒绝 ⇒ ⇒ 对两后端是**同一段 C++ 代码** ⇒ ⇒ **dual 化零增量价值**（plan §4 剔除项 ②） |
| `test_wires_connection` 的「Basic ch_uint connection」 | **裸 context** 的**节点身份**断言（`impl()` 指针相等）⇒ ⇒ 无 DUT ⇒ ⇒ 双后端宏**不适用**（macro R6 排除清单） |
| `test_wires_connection` 的「Empty constructed ch_uint」 | **空节点语义**（`impl() == nullptr`）⇒ ⇒ **解释器专属**（plan §4 剔除项 ⑤） |

## Verification（前瞻 · 可勾选）

- [ ] 每个新文件的 **N2′ 判据**：断言数(with verilator) == **2 ×** 断言数(interpreter-only)
      （14/7 · 32/16 · 68/34 · 12/6）
- [ ] 期望值**全部手推**（禁抄仿真输出）⇒ ⇒ 文件头逐条写明
- [ ] 原文件 **MUST NOT 修改**（保留 interpreter-only baseline ⇒ 零回归）
- [ ] `ctest -LE perf` 零回归
- [ ] 计数锚点同步且两个护栏脚本双绿
- [ ] **冷缓存墙钟实测**并回填 plan §5
