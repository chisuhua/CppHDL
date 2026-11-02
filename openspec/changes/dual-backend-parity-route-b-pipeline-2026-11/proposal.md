# 提案：路线 B 第 6 批 —— `chlib::pipeline` 的**流水线寄存器控制语义**进双后端 parity

- **依据**：`.rddf/plans/dual-backend-full-coverage-2026-11.md` §4 **H 批**（路线 B）
  + objective `objective-test-coverage-100pct` §2 重述后的「诚实上限」（含
  **新增有意义的 dual 文件数**）
- **规格影响**：**无 spec 变更**（`.openspec.yaml` 显式声明 `skip_specs: true`）
- **规模**：1 个新测试文件 + 1 条 CMake 注册 + 计数锚点
- **关联**：`objective-test-coverage-100pct` · capability `dual-backend-test-coverage`

## Why

`chlib::pipeline` 是仓内**唯一**的流水线寄存器组件族（`PipelineStage` ·
`PipelineStageDual` · `PipelineChain` + 函数式 `pipeline_reg`），而盘点实测
它的 **dual 覆盖率 = 0** ⇒ ⇒ Verilog codegen 路径**从未被验证过**。

⚠️ **为什么这类语义特别危险**：三个控制信号（**rst** / **stall** / **flush**）
在 codegen 侧要写成

```systemverilog
always_ff @(posedge clk) q <= rst ? 0 : (stall|flush) ? q : d;
```

⇒ ⇒ **「stall 保持」「flush 清零」「rst 优先」三条规则的相对优先级**是最容易
写错的一类；而既有 `tests/chlib/test_pipeline.cpp` 的 3 个组件用例
**全部只做编译性检查**（`REQUIRE(true)`，裸 `nullptr` parent）⇒ ⇒ **从未真正
跑过**（连 interpreter 侧都没有「端口驱动」形态的覆盖）。

⪯ **本 change 实测印证了这个盲区**：把 `PipelineStage` 放进一个带端口的 DUT 后
**第一次运行就 SIGSEGV** —— 裸 `PipelineStage stage(this, "stage")` 构造的子组件
其 `describe()` **不会被框架调用**（`Component::build_internal` 只对**顶层**组件
调 `describe()`）⇒ ⇒ 子组件的 `ch_reg` 从未被创建。MUST 用 `ch::ch_module<T>`。
⇒ ⇒ 这条用法纪律在既有测试里**不可能被发现**（它压根没跑）。

## 量测到的语义差异（本 change 的判别点）

`PipelineStage` 与函数式 `pipeline_reg` **不是同一套语义**（实测 + 读实现）：

| 信号 | `PipelineStage` | 函数式 `pipeline_reg` |
|---|---|---|
| `rst` | **只**抑制更新（`update = !rst & !stall & !flush`）⇒ **寄存器保持** | **清零**（`reg->next = select(rst, 0, ...)`） |
| `stall` | 保持 | 保持 |
| `flush` | 保持寄存器，但**输出组合清零**（`data_out = select(flush, 0, data_reg)`） | 清零寄存器 + 输出组合清零 |

⇒ ⇒ 只钉一个组件会漏掉另一半 ⇒ ⇒ 本 change **两个都钉**（pair 1 / pair 2）。

## 为什么**不是**转换 `tests/chlib/test_pipeline.cpp`（如实说明）

它的 3 个组件用例是**编译性检查**（`PipelineStage<T> stage(nullptr, "x"); REQUIRE(true)`）
⇒ ⇒ 没有任何运行期断言 ⇒ 转换无对象；其函数式用例是**裸 context +
`sim.set_value`**（无 DUT 端口）⇒ ⇒ 按 macro 的 R6 排除清单**不适用**。
⇒ ⇒ 因此本 change 是**新增**（路线 B），**不是**转换：原文件 **MUST NOT 修改**。

## What Changes

| 文件 | 内容 |
|---|---|
| `tests/chlib/test_pipeline_dual.cpp`（**新**） | 3 个 `DUAL_BACKEND_TEST_PAIR`：<br>① `PipelineStage`（stall 保持 / flush 组合清零 / rst 只抑制更新）<br>② 函数式 `pipeline_reg`（rst 清零，与 ① 不同）<br>③ `PipelineChain<3>`（逐级延迟） |
| `tests/CMakeLists.txt` | 1 条 `add_dual_backend_test` |
| 计数锚点 | `CTEST_TOTAL` 221 → **222** · `VERILATOR_E2E_TOTAL` 53 → **54** · `DUAL_BACKEND_PARITY_TOTAL` 44 → **45** · `PAIRS` 230 → **233** · non-perf 210 → **211** · `tests/chlib/test_*.cpp` 39 → **40** |

## ⚠️ 同批被**推迟**的部分（`chlib::memory`）

同批原计划还有 `chlib::memory`（single-port / dual-port RAM）的 dual 覆盖，
**未落地** —— 写它的过程中挖出一次**真 parity 分歧**：

| 后端 | `sread` 禁读拍（`enable=0`） | 代码 |
|---|---|---|
| interpreter | 直接 `return` ⇒ **保持上一拍的值** | `src/ast/instr_mem.cpp:258-259` |
| Verilog | `else proxy <= 1'b0;` ⇒ **清零** | `src/codegen_verilog.cpp:1261` |

而 `chlib::memory` 的 RAM 正是 `mem.sread(addr, !we)`（**写拍禁读**）
⇒ ⇒ 两条后端不可能有同一组期望值 ⇒ ⇒ 无法写成满足 N2′ 的 dual 用例。

⇒ ⇒ **处置**（用户 2026-11-02 裁定 = 选项 A）：**独立 change** 修该分歧
（`openspec/changes/sread-disable-cycle-divergence-2026-11/`），
`memory` 的 dual 覆盖**等分歧修好后**再补。
⇒ ⇒ 已写的 `test_memory_dual.cpp`（192 行）留档在
`/tmp/opencode/sread-divergence/`，供后续 change 复用。

⚠️ **不在本 change 顺手修 codegen** —— plan §6 风险 3：dual 暴露真实 parity
分歧 ⇒ 停下开独立 change。

## 期望值的推导依据（**手推**，禁抄仿真输出）

语义前提（逐条来自 `include/chlib/pipeline.h` 的实现）：

1. `PipelineStage::update = !rst & !stall & !flush`；
   `data_reg->next = select(update, data_in, data_reg)`
2. `PipelineStage::data_out = select(flush, 0, data_reg)` —— **组合**
3. 函数式 `pipeline_reg`：`reg->next = select(rst, 0, select(update, d, reg))`；
   输出 `select(rst, 0, select(flush, 0, reg))`
4. `PipelineChain`：第 i 级的输入来自第 i−1 级的**寄存器**

⚠️ **时序语义（本批实测确认，不是假设）**：`Simulator::eval_sequential`
**先**执行时序指令（寄存器在 tick 内更新）⇒ ⇒ 之后求值的组合逻辑读到的是
**沿后**值 ⇒ ⇒ 组合输出（`data_out`）在 `tick()` 后呈现**沿后**的寄存器值。

⚠️ 这与「纯组合读」场景（如 `sync_fifo::q_async` 是 `aread` ⇒ 读内存当前内容、
与沿无关）**不同** ⇒ ⇒ 不能把两类混用同一套推导。

逐拍金标见文件头（`kStageSteps` 7 拍 / `kFuncSteps` 6 拍 / `kChainOut` 6 拍）。

## Verification（前瞻 · 可勾选）

- [ ] **N2′ 判据**：断言数(with verilator) == **2 ×** 断言数(interpreter-only)
- [ ] 期望值**全部手推**（文件头逐条写明推导）
- [ ] 原文件 `tests/chlib/test_pipeline.cpp` **MUST NOT 修改**
- [ ] 变异证明判别力：改坏 stall / flush / rst 的优先级之一 ⇒ **红**
- [ ] `ctest -LE perf` 零回归
- [ ] 计数锚点同步且 `verify_test_counts.sh` / `verify_dual_backend_parity.sh` 双绿
