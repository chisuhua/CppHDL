---
name: cross-dut-bit-select-out-of-range-2026-11
priority: P2
source: 路线 B（I 批）双后端 parity 实施期实测挖出
phase: phase-5
category: framework
type: bugfix
created_at: 2026-11-03
---

# Improvement: 同一进程内**多个 DUT** ⇒ 出现 `bit_select` **越界**（跨 DUT 状态污染）

- **Date drafted**: 2026-11-03
- **Drafted by**: build-agent (builder role)
- **Status**: ✅ **已修复**（2026-11-03 · change
  `mux-slot-width-preservation-2026-11`）⇒ ⇒ **根因已定位**（见 §6）：
  **不是**跨 DUT 状态污染，而是 `include/ast/instr_mux.h` 的
  `eval()` 用 **拷贝赋值** `*dst_ = *分支` ⇒ ⇒ `bitvector::operator=`
  会 **RESIZE dst** ⇒ ⇒ 较窄的分支被选中时整个 `mux` 槽位被**压窄**
  ⇒ ⇒ 下游 `bit_select(mux, 2)` 的 `operand_width` 变成 1 ⇒ ⇒ **越界**。
  ⇒ ⇒ 修法 = `assign_truncate`（与 `instr_output` / `instr_proxy` /
  `instr_reg` 的**既有惯例**一致）⇒ ⇒ 越界日志 **6 → 0**，全量零回归。
- **来源**: 路线 B（I 批）`tests/chlib/test_stream_arbiter_dual.cpp` 实施期的 stderr 噪声。

## 1. Why (背景与动机)

新文件 `tests/chlib/test_stream_arbiter_dual.cpp`（2 个 DUT × 2 个后端 = 4 个 TEST_CASE）
在**完整运行**时，stderr 出现 **6 条**：

```
[instr_op_bit_sel] Error: Bit index out of range! index=2 operand_width=1 dst_width=1 src1_width=32
[instr_op_bit_sel] Error: Bit index out of range! index=1 operand_width=1 dst_width=1 src1_width=32
   （× 6）
```

⚠️ **插桩后的关键签名**（临时改 `include/ast/instr_op.h` 打印，后已还原）：

| 字段 | 值 | 含义 |
|---|---|---|
| `index` | 1 或 2 | 运行时索引 |
| `operand_width` | **1** | ⚠️ **操作数只有 1 位** ⇒ 索引 1/2 必然越界 |
| `dst_width` | 1 | 目标是 1 位（`ch_bool`） |
| `src1_width` | **32** | ⚠️ 索引是 **32 位** ⇒ **不是**编译期字面量 |

⇒ ⇒ 这**不是** `round_robin_selector` 里那个 `bit_select(request, pos)`
（那一个的操作数是 3 位、索引是 2 位）⇒ ⇒ 另有其人 ⇒ ⇒ 且 `operand_width=1`
⇒ ⇒ **操作数被污染成了 1 位**（本该是更宽的值）。

### 复现条件（实测矩阵）

| 运行范围 | 越界错误数 |
|---|---|
| `*round_robin*\[interpreter\]`（单独） | **0** |
| `*round_robin*\[verilator\]`（单独） | **0** |
| `*priority*\[interpreter\]` / `*priority*\[verilator\]`（单独） | **0** |
| `*\[interpreter\]`（两个 DUT 的 interpreter 半边） | **6** |
| `*\[verilator\]`（两个 DUT 的 verilator 半边） | **0** |
| 完整（4 个 TEST_CASE） | **6** |

⇒ ⇒ **是 interpreter → interpreter 的跨 DUT 污染**（先建 `priority` 仲裁器
⇒ ⇒ 再建 `round_robin` 仲裁器 ⇒ ⇒ 后者的图里出现 1 位操作数）
⇒ ⇒ **不是** verilator 后端的问题 ⇒ ⇒ 与
`tests/AGENTS.md` 记录的 **K1「ORC JIT 跨 DUT 状态污染」**属**同一族**
（该 K1 已知问题当时是用「perf 跑在**独立子进程**」绕过的）。

⚠️ **仓内其它 8 个 dual 文件均不复现**（实测 `test_cache_async_dual` /
`test_stream_pipeline_dual` / `test_selector_arbiter_dual` /
`test_hazard_complete_dual` / `test_bitwise_dual` / `test_onehot_dual` /
`test_arithmetic_advance_dual` / `test_sync_fifo_dual` 全部 **0** 条）
⇒ ⇒ 本次是**首个复现** ⇒ ⇒ 但**根因未定位**（⏱️ 超出该批次的预算）。

## 2. What Changes (变更范围)

⚠️ **本 draft 只登记现象与证据**，**根因未定位** ⇒ ⇒ 改法待定 ⇒ ⇒
但**必须**先做到：

- [ ] **定位**：哪个 `bit_select` 的操作数宽度被污染成 1 位
      ⇒ 建议手段：给 `BitSel::eval` 的错误分支加上**调用点**信息
      （`instr_base::name()` + `node_id` + 操作数宽度），或在
      `InterpreterBackend::eval_combinational` 的循环里打印
      `kv.first`（node id）+ 节点名（`ctx->get_node(id)->name()`）
      ⇒ ⇒ 有了 node id 就能在图里反查它的 `src(0)`。
- [ ] **判定污染源**：优先怀疑**跨 context 的全局/静态状态**
      （节点 id 分配器 · 字面量池 · 评估顺序缓存 · `data_map` 槽位复用）
      ⇒ ⇒ 与 K1 的排查路径相同。
- [ ] **修法**（待根因确定后补）：⚠️ **不得**用「加 try/catch 吞掉越界」来「修」
      ⇒ ⇒ 那是把症状藏起来 ⇒ ⇒ **必须**让两个后端在**同一语义**下求值。

## 3. ⚠️ 影响面（**如实登记**：目前**不影响**断言值）

- ✅ 本批的测试**全绿**（192 断言 / 4 用例）⇒ ⇒ **断言值未受影响**
  ⇒ ⇒ 越界的 `bit_select` 落在**未被断言输出消费**的路径上（死路径）。
- ⚠️ **但这不等于安全**：⚠️ 换一个激励，污染就可能落到**被断言**的路径上
  ⇒ ⇒ 而「静默的错误值」正是本仓反复出现的「假绿」形态
  ⇒ ⇒ ⇒ **必须修**，不能因为「当前断言全绿」就搁置。
- ⚠️ **CI 噪音**：6 条 stderr 错误会让「有错误输出 ⇒ 有问题」的直觉失效
  ⇒ ⇒ 在修好之前，**任何** dual 文件都可能出现这类噪音 ⇒ ⇒ 应在 CI 日志里标注。

## 4. Capabilities (契约归属)

- 建议新建 capability `core-simulator-cross-dut-isolation`（或并入既有的
  `perf-test-isolation`）⇒ **ADDED ×1**，含
  「**同一进程内构建多个 DUT 时，后构建的 DUT 的图 MUST 与单独构建时逐位等价**」
  这条 Requirement。
- ⚠️ **可执行判据**：本 draft 的复现矩阵本身就是判据的原型 ⇒ ⇒
  ⇒ 「建 DUT A 再建 DUT B」与「单独建 DUT B」的**图**逐节点比对
  ⇒ ⇒ （可用 `toDAG` 的 dot 输出做 diff ⇒ ⇒ 注意 `toDAG` 在本仓可产出 dot 文件）。

## 5. Impact (影响面)

- **受影响面**：任何**在同一 Catch2 进程内构建多个 DUT** 的测试
  ⇒ ⇒ 现有的 dual 文件（每个文件 1–3 个 DUT）是**高风险面**
  ⇒ ⇒ 实测 8 个既有 dual 文件均未复现 ⇒ ⇒ 但**不等于不存在**。
- ⚠️ **与 K1 的关系**：`tests/AGENTS.md` 记录的 K1（ORC JIT 跨 DUT 污染）
  ⇒ ⇒ 本条是**同一族**的**第二个实例**，且**不涉及 JIT**
  （dual fixture 已 `set_jit_enabled(false)`）⇒ ⇒ ⇒ **污染源不限于 JIT 状态**。

## 6. ✅ 根因定位与修复（2026-11-03）

### 6.1 定位过程（实测，非推断）

| 步 | 手段 | 结果 |
|---|---|---|
| 1 | 复现 | 完整跑 `test_stream_arbiter_dual` ⇒ **6** 条越界；单独跑任一 DUT ⇒ **0** 条 ⇒ 确认「多 DUT」是**触发条件** |
| 2 | 临时插桩（打印 `node_id` + `operand_width` + `src1_width`） | 越界点：`node 379/383`，`src0_width=1`，`src1_width=32`（⇒ **编译期索引**变体，不是运行期 `pos`） |
| 3 | 打印节点身份（`ctx->get_node_by_id()`） | `node 383 name='top.bit_select' size=1 src0=(367,'top.mux_select',**w3**) src1=(382,'top.bit_select_idx',w32)` ⇒ ⇒ **图里明明是正确的 3 位！** |
| 4 | 检测「槽位宽度被 eval 改变」 | `[DBG-WIDTH-CHANGE] node 367 name='top.mux_select' **3 -> 1** (node size=3)` ⇒ ⇒ **定位到 `mux` 的 eval** |
| 5 | 读实现 | `include/ast/instr_mux.h`：`*dst_ = *true_val_` / `*dst_ = *false_val_` |

### 6.2 根因

`bitvector::operator=` **会 RESIZE dst**（把宽度改成源的宽度），而 `assign_truncate`
保持 dst 的声明宽度（源被截断 / 零扩展）。

⇒ ⇒ `mux` 节点的宽度 = `max(真分支宽, 假分支宽)`（`build_mux`）⇒ ⇒
**较窄的分支被选中**时，`*dst_ = <窄分支>` 把整个 mux 槽位**压窄**
⇒ ⇒ 下游按**声明宽度**取位（`bit_select(m, 2)`）⇒ ⇒ `operand_width=1`
⇒ ⇒ **比特索引越界**，错误分支**写 0**。

### 6.3 为什么表现为「跨 DUT 才出现」/「只在 interpreter 侧」

| 观察 | 解释 |
|---|---|
| 只在 **interpreter** 侧 | Verilog 的 `assign d = c ? t : f;` 天然保持声明宽度 ⇒ ⇒ **这是两后端分歧** |
| 只在 **多 DUT** 时出现 | node id 是**进程级全局唯一**（`g_next_node_id`）⇒ ⇒ 同一 DUT 在不同运行下 id 不同 ⇒ ⇒ 拓扑序的 tie-break 变 ⇒ ⇒ `mux` 与消费它的 `bit_select` **谁先求值**变 ⇒ ⇒ **时好时坏** |
| 断言值**未受影响** | 越界分支写 0，而被选中的**窄分支**其高位本来就是 0 ⇒ ⇒ **值逐位等价** ⇒ ⇒ **良性但产生噪声** |

⚠️ **「良性」的边界（如实登记）**：目前只确认「值为 0 的高位被丢」是可观测的唯一后果
⇒ ⇒ 若将来出现「同一拍内读 mux 的**中间态**」的消费方 ⇒ ⇒ 就会读到**错误的宽度**
⇒ ⇒ 但那一刻的错误值仍是「该位为 0」⇒ ⇒ 除非消费方依赖**宽度本身**（如 `bits<>` 的
范围检查、`+` 的宽度推导）。

### 6.4 修复与判据

- **修法**：`include/ast/instr_mux.h` 的两处 `*dst_ = *分支` ⇒ ⇒
  `dst_->assign_truncate(*分支)`（与 `instr_output::eval` 的**既有惯例**一致，
  该处注释早已把同一个坑写明）。
- **判据**（`tests/test_mux_width_dual.cpp`，**1** pair / **42 == 2 × 21** 断言）：
  - 「宽分支 → 窄分支」后 `mux_out` 的**高位 MUST 归零**（抓「不清高位」，例如
    `assign_truncate` 的零扩展路径若不生效 ⇒ ⇒ 会残留 `0b110`）；
  - `bit2` / `bit1` 在窄分支被选中时**不越界**且为 0。
- ⚠️⚠️ **M1（把修法改回 `*dst_ = *分支`）在【值层面不可判别】** —— 已实测恒绿
  ⇒ ⇒ 压窄只丢「已经为 0」的高位 ⇒ ⇒ `REQUIRE` 抓不到
  ⇒ ⇒ ⇒ **唯一可观测量 = 越界错误日志** ⇒ ⇒ 用 ctest 的
  `FAIL_REGULAR_EXPRESSION "Bit index out of range"` 判红（**已验证**：
  注入 M1 ⇒ 该测试 Failed；还原 ⇒ 通过）。
  ⇒ ⇒ **删掉那条 `FAIL_REGULAR_EXPRESSION` 就等于删掉这一类回归的唯一护栏**。
