# 提案：路线 A（D 批）—— `ch_mem` 读端口的**连接形态**进双后端 parity

- **依据**：`.rddf/plans/dual-backend-full-coverage-2026-11.md` §4 **D 批**（路线 A）
- **规格影响**：**无 spec 变更**（`.openspec.yaml` 声明 `skip_specs: true`）
- **规模**：1 个新测试文件 + 1 条 CMake 注册 + 计数锚点
- **关联**：`objective-test-coverage-100pct` · capability `dual-backend-test-coverage`

## Why

`tests/test_mem_connection.cpp` 覆盖 **4 种 `ch_mem` 读端口的连接形态**：

| # | 形态 | 原用例 |
|---|---|---|
| 1 | `sread` → `ch_out`（enable 恒 1） | `ch_mem sread port connection to ch_uint` |
| 2 | `aread` → `ch_out`（0-cycle） | `ch_mem aread port connection to ch_uint` |
| 3 | `sread` → `ch_uint` 中转 → `ch_out`（16 位数据） | `ch_mem read port connection with ch_uint variable` |
| 4 | `sread` → `ch_reg` → `ch_out`（多一拍） | `ch_mem sread port to ch_reg connection` |

⇒ ⇒ **全部是裸 `ch_device` 单后端** ⇒ ⇒ 这 4 条连接路径在 verilator codegen 上
**从未被验证过**。

## ⚠️ 为什么「连接形态」值得单独钉（这是 codegen 的高危面）

`ch_mem` 的读端口是**虚构节点**（它自己没有 Verilog 信号），数据靠 `data_proxy`
承载（`read_port → data_proxy` 的边）⇒ ⇒ codegen 必须同时做对三件事：

1. **跳过**「`src(0)` 是 mem read port」的 proxy 赋值（否则双驱动）；
2. 在 `print_output` 的代理链遍历里**不越过** mem read port
   （否则 `assign out = <read_port>` 引用**未声明**信号）；
3. **不把** `type_mem_read_port` 放进 wire 声明列表（否则悬空 wire）。

⇒ ⇒ 这三处任一错了，**单后端测试完全看不见** —— interpreter 走 `data_map`，
根本不经过这些 codegen 分支 ⇒ ⇒ **只有双后端能抓**。

## 与既有 dual 文件的分工（不重复）

| 文件 | 钉什么 |
|---|---|
| `test_mem_sread_disable_dual.cpp` | **禁读拍**（`enable == 0`）的语义 |
| `test_mem_read_timing_dual.cpp` | `aread` 0-cycle / `sread` 1-cycle 的**时序** |
| **本文件** | **连接形态**（`sread`/`aread` × `ch_out`/`ch_uint`/`ch_reg`），enable 恒 1 |

⇒ ⇒ 三者正交。

## 为什么**不是**修改原文件

原文件是「单后端」的**既有回归**，且 `dual-backend-test-coverage` 有「既有测试
MUST 保留原状」的锁 ⇒ ⇒ 本 change 是**新增** `_dual` 文件 ⇒ 原文件
`tests/test_mem_connection.cpp` **MUST NOT 修改**。

## What Changes

| 文件 | 内容 |
|---|---|
| `tests/test_mem_connection_dual.cpp`（**新**） | 4 个 `DUAL_BACKEND_TEST_PAIR`（每 pair 5 个地址 ⇒ **20 断言/后端**） |
| `tests/CMakeLists.txt` | 1 条 `add_dual_backend_test` |
| 计数锚点 | `CTEST_TOTAL` 224 → **225** · `VERILATOR_E2E_TOTAL` 56 → **57** · `DUAL_BACKEND_PARITY_TOTAL` 47 → **48** · `PAIRS` 237 → **241** · non-perf 213 → **214** · `tests/test_*.cpp` 127 → **128** |

## ⚠️ 与原 DUT 的两处**有意偏离**（如实登记）

1. **省略 `clk_in` / `rst_in` 端口**：原 DUT 声明了它们，但**从未驱动、也从未
   使用**（死端口）⇒ ⇒ 双后端下「未驱动端口读到的是**未定义值**」（仓内
   ANTI-PATTERNS 已登记，实测读到全 1）⇒ 保留只会引入无谓风险 ⇒ ⇒ 故不复制。
   ⚠️ 这是**结构简化**，不改任何可观测行为（原文件在单后端下同样不依赖它们）。
2. **不复制原用例里的 `toDAG(...)` 调试调用**（那是 dot 文件导出，不是断言）。

## 期望值的推导依据（**手推**，禁抄仿真输出）

1. `sread` 是 1-cycle（`always_ff`）⇒ `tick()` 一次即完成沿上采样；
2. `aread` 是 0-cycle（`assign`）⇒ 同拍可见；
3. `ch_reg` 中转**多一拍**（寄存器在下一个沿采到 proxy 的沿前值）
   ⇒ ⇒ 该 pair 用 **2 次 tick**（沿用原文件的节奏）；
4. 初始化内容逐 DUT 见其 `describe()`。

⚠️ 各 pair 的 tick 次数**沿用原文件**（pair1 2 次 / pair2 1 次 / pair3 1 次 /
pair4 2 次）⇒ ⇒ 这样期望值与原单后端用例**同源可互证**。

## Verification（前瞻 · 可勾选）

- [ ] **N2′**：断言数(with verilator) == **2 ×** 断言数(interpreter-only)
- [ ] 4 个 pair 全部**一次通过**（手推正确性的证据）
- [ ] 原文件 `tests/test_mem_connection.cpp` **MUST NOT 修改**
- [ ] 变异证明判别力：让 sync / aread 读端口的**地址解码**失效 ⇒ **必红**
- [ ] `ctest -LE perf` 零回归（**全量重建后**跑 —— 见 AGENTS.md 的 COMMANDS 警告）
- [ ] 计数锚点同步且两个计数护栏双绿
