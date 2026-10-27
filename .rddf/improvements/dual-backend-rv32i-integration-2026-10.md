---
name: dual-backend-rv32i-integration-2026-10
priority: P1
source: 2026-10-27 roadmap 审查（objective-rv32i-e2e 的 Go 判据第 5 条缺可执行载体）
phase: phase-5
category: testing
type: feature
created_at: 2026-10-27
---

# Improvement: RV32I 子组件的 dual-backend 覆盖 MUST 补齐 Batch 2/3

- **Date drafted**: 2026-10-27
- **Drafted by**: build-agent (planner role)
- **Status**: 📋 5-segment draft；change 目录**待建**（本草案是为 `objective-rv32i-e2e` 的
  Go 判据第 5 条补上「可执行载体」）
- **依据**：
  - `tests/test_rv32i_pipeline_behavior.cpp` **文件头**的 Batch 划分（唯一的原始定义）
  - `.rddf/plans/2026-10-26-phase5-followup-execution-plan.md` **T6**（范围 / 步骤 / 验收）
  - `openspec/specs/dual-backend-test-coverage/spec.md`（R6 排除清单 / R7 固化）
- **roadmap 归属**：`feat-dual-backend-coverage` Phase 4 残余 + `feat-rv32i-integration` 的
  最后一条未勾验收 + `objective-rv32i-e2e` Go 判据第 5 条

## 1. Why (背景与动机)

### 缺口：Go 判据第 5 条**没有载体**

`objective-rv32i-e2e` 的 Go 判据第 5 条写的是
「`dual-backend-rv32i-integration` change archived」，但这个 change **从未立项**：

- `openspec/changes/` 与 `openspec/changes/archive/` **均无**该名字；
- `.rddf/improvements/` 也**无**对应草案；
- 该名字只出现在 roadmap / plan 的**判据文字**里，范围定义仅存在于 plan T6 与
  `test_rv32i_pipeline_behavior.cpp` 的文件头注释。

⇒ 一个「按名字判定」的 Go 判据，其被判定对象不存在 ⇒ 判据**永远无法达成**，
objective 会因此**永久 active**（这与会无限期占用完成条件的 Windows CI 项是同一类问题）。

### 事实：Phase 4 的 Batch 1 已归档，剩 3 个 dual pair

`test_rv32i_pipeline_behavior.cpp`（2026-10-19 归档于 `dual-backend-rv32i-pipeline-behavior-tests`）
已落地 **Batch 1 = 13 个 dual pair / 5 个组件类**（BranchDetector ×3 / BranchConditionUnit ×4 /
ForwardingUnit ×3 / ControlHazardUnit ×1 / HazardDetectionUnit ×2）。剩余：

| Batch | DUT | 用例数 | 原用例位置 |
|---|---|---|---|
| 2 | `IfIdPipelineReg` | 2（`transfers PC and instruction` / `clears instr_valid on flush`） | `tests/test_rv32i_pipeline.cpp` 的 **SECTION** 内 |
| 3 | `BranchPredictor`（BNT strategy） | 1 | 同上（SECTION 内） |

**为什么它们没做**：不是因为硬件风险，而是因为原用例全在 **Catch2 SECTION** 里 ——
`dual-backend-test-coverage` **R6 #5** 明确排除 SECTION 重入（macro 会展开成 2 个
TEST_CASE，每个 half 独立遍历 SECTION ⇒ 破坏 fixture 语义）⇒ Batch 2/3 需要
**SECTION-free 重写**（放进已有的 `_behavior` 文件），而不是「迁移」。

### 更正一处被高估的风险（planner 读代码后的判断）

roadmap / `feat-dual-backend-coverage` 把 Batch 2 描述为 **`IfIdPipelineReg` clk-spike**，
plan T6 也要求「先做 clk-spike，失败就记原因」。**实测读代码后：这个风险画像不成立**：

1. **`IfIdPipelineReg` 的 `clk` 是死端口** —— `include/cpu/pipeline/rv32i_pipeline_regs.h`
   声明了 `ch_in<ch_bool> clk`，但它的 `describe()`（`:203-227`）**从不读 `io().clk`**；
   寄存器推进靠 `ch_reg::next` + Simulator/Verilog 的**隐式时钟**。
2. **隐式时钟的 dual 能力已被证明** —— `tests/test_dual_backend_clock.cpp`
   （Phase 1，spec R10「2-step clock toggle fixture」）已用 `CounterClock` 验证
   `always_ff @(posedge clk)` 在两后端一致。
3. **`BranchPredictor`（Batch 3）是纯组合** —— `include/cpu/pipeline/rv32i_branch_predict.h:218-232`
   的 IO 里没有任何 clock/reset，也没有 `ch_reg` 状态。

⇒ 真实风险只有 **一条**，且是本仓库已登记过的老问题：
**新增/既有输入端口若不被显式驱动，读到的是未定义值**（`AGENTS.md §ANTI-PATTERNS`：
实测 `strbe` 读全 1、`rst_n` 读 0）。`IfIdPipelineReg` 有 7 个输入端口
（`if_pc`/`if_instruction`/`if_instr_valid`/`clk`/`rst`/`stall`/`flush`）⇒
dual 夹具 MUST **逐个显式驱动**（含那个死端口 `clk`），否则 interpreter 与 verilator
的未驱动初值未必相同 ⇒ **两后端同时红或同时绿都可能是假绿**。

⚠️ 另有一条**从原文抄不来的坑**：原 SECTION 用例手动 toggle 了 `io().clk`
（`set clk=true → tick() → set clk=false → tick()`）来「模拟一个时钟周期」——
那是**死端口上的两次额外 tick**。dual 夹具 MUST NOT 照抄：`sim.tick()` 本身已做
2-step toggle（Verilog 侧亦然），照抄会让「一拍」变成三拍 ⇒ 若将来该端口被接回内部
逻辑，用例会因为**多打两拍**而静默改变语义。

## 2. What Changes (变更范围)

1. **在既有文件 `tests/test_rv32i_pipeline_behavior.cpp` 里新增 3 个 `DUAL_BACKEND_TEST_PAIR`**
   （Batch 2 ×2 + Batch 3 ×1）—— **不新建测试文件、不新建 CMake target**
   - Batch 2-a：`IfIdPipelineReg` 传输 —— 驱动 `if_pc`/`if_instruction`/`if_instr_valid`/
     `rst=0`/`stall=0`/`flush=0` ⇒ `tick()` ⇒ `id_pc`/`id_instruction` 等于输入
   - Batch 2-b：`IfIdPipelineReg` flush —— 先打入有效值并 `tick()`，再 `flush=1` + `tick()`
     ⇒ `id_instr_valid == 0`（**只清 valid，不清 pc/instruction**：`describe()` 里
     `pc_reg`/`instr_reg` 的 next 由 `update` 门控、与 flush 无直接关系 ——
     断言须按**实际 RTL 语义**写，MUST NOT 按「flush 清一切」的直觉写）
   - Batch 3：`BranchPredictor` BNT —— `instruction=0x00000063`（`BEQ x0,x0,+0`）+
     `rs1_data=rs2_data=0` ⇒ `is_branch=1` / `branch_taken=1` / `predict_error=1`
2. **`tests/test_rv32i_pipeline.cpp` MUST NOT 修改**（spec R7：保留 interpreter-only baseline）
3. **spec**：`dual-backend-test-coverage` 用 MODIFIED 记录「Phase 4 Batch 2/3 完成」
   （若该 capability 的正文不含 Phase 划分，则在 `feat-dual-backend-coverage` 侧记账即可）
4. **簿记**：`objective-rv32i-e2e` 判据第 5 条勾选 + `feat-rv32i-integration` 最后一条验收勾选
   + `feat-dual-backend-coverage` Phase 4 由 🟡 转 ✅ + `roadmap.md` sprint 表

## 3. Acceptance (验收标准)

- [ ] 3 个 dual pair **双后端全绿**（interpreter + verilator 各跑一遍）
- [ ] **`CTEST_TOTAL` 202 不变** + **`ctest -L verilator_e2e` 37 不变**
      ⚠️ **这里必须纠正 plan T6 的验收数字**：T6 写「+3 ⇒ 37 → 40」是**错的** ——
      `add_dual_backend_test()` 是 **`add_catch_test(TEST_NAME …)` + 打 label**
      （`tests/CMakeLists.txt:24-44`）⇒ **一个文件 = 一个 ctest 条目**，`ctest -N -L verilator_e2e`
      数的是**文件/二进制**而不是 Catch2 用例数（实测 37 = 含 `DUAL_BACKEND_TEST_PAIR` 的
      `.cpp` 文件数）。在**既有**文件里加 pair ⇒ **计数不变**。要涨计数只能新增**文件**。
- [ ] `test_rv32i_pipeline.cpp`（interpreter-only baseline）**零改动**且仍全绿
- [ ] 所有输入端口**逐个显式驱动**（含死端口 `clk`），且**不**照抄原用例的手动 clk toggle
- [ ] golden **手工推导**并在代码注释留痕（MUST NOT 从仿真输出抄值）
- [ ] 每个用例带**反向期望**（例：flush 后 `id_instr_valid` MUST NOT 仍为 1；
      BNT 用例 `predict_error` MUST NOT 为 0）
- [ ] **变异验证**（每条先确认编译成功）：
      - 把 flush 分支写成 `select(io().flush, ch_bool(true), valid_reg)`（反向）⇒ Batch 2-b 转红
      - 把 `predict_error` 改成恒 0 ⇒ Batch 3 转红
      - 漏驱动 `if_instr_valid` ⇒ 至少一个用例转红（否则说明该端口无信号 ⇒ 用例无效）
- [ ] `ctest -LE perf` 零回归（191/191）

## 4. Capabilities (涉及的能力维度)

- **`dual-backend-test-coverage`** —— 本 change 是它 Phase 4 的收尾（Batch 2/3）；
  R6 排除清单**不变**（SECTION 重入仍排除；本 change 是**新建 SECTION-free 用例**，不是解除排除）
- `verilator-rv32i-e2e` —— 无新契约（clk/隐式时钟语义已由 `verilator-backend-e2e` 与
  `test_dual_backend_clock.cpp` 覆盖）
- `rv32i-commit-path` —— 无影响（不碰流水线装配，只加单元级 dual 用例）

## 5. Impact (影响面分析)

- **计数**：`CTEST_TOTAL` **202 不变** / `EXAMPLES_TEST_SOURCES` **17 不变** /
  `ctest -L verilator_e2e` **37 不变**（⚠️ 见 §3 对 plan T6 的纠正）
- **文件面**：`tests/test_rv32i_pipeline_behavior.cpp`（+~70 行）；
  `tests/test_rv32i_pipeline.cpp` **零改动**（spec R7 硬约束）
- **风险**：低。没有新的 RTL、没有新的端口、没有框架层改动。
  唯一实质风险 = 「输入端口漏驱动 ⇒ 未定义值 ⇒ 假绿」⇒ 由 §3 的显式驱动 + 变异验证兜住。
- **不做会怎样**：`objective-rv32i-e2e` 的 Go 判据第 5 条**不可判定**（载体不存在）
  ⇒ objective 永久 active ⇒ `phase-5` 收口时会出现「唯一未勾项是个不存在的 change」。
- **Non-Goals**（明确不在本 change 内）：
  - 不解锁 SECTION 重入（R6 #5 仍排除）
  - 不把 `test_rv32i_pipeline.cpp` 整体 dual 化（那是 Phase 5 全量覆盖面的事）
  - 不动 `IfIdPipelineReg` 的**死端口 `clk`**（删端口是 API 变更 ⇒ 独立立项；
    本 change 只**显式驱动**它）
  - 不追求 `ctest -L verilator_e2e` 数字增长（本 change 不改计数 —— 见 §3）

## 6. 与既有文档的差异（需 builder 在 tasks.md / 执行时同步）

| 文档 | 现写法 | 实际 |
|---|---|---|
| plan T6 验收 | 「`ctest -L verilator_e2e` 计数 +3（37 → 40）」 | **不变**（37）—— 一文件一条目 |
| plan T6 步骤 1 | 「先做 clk-spike ⇒ 失败就记原因」 | clk 风险已被 `test_dual_backend_clock.cpp` 证明不存在；`IfIdPipelineReg` 的 `clk` 是**死端口** ⇒ 该 spike 可省，改为「驱动纪律 + 变异验证」 |
| `feat-dual-backend-coverage` Phase 4 行 | 「Batch 2 IfIdPipelineReg clk-spike + Batch 3 BranchPredictor follow-up」 | 同上：spike 不是风险项；真实工作 = SECTION-free 重写 3 个用例 |
| `objective-rv32i-e2e` 判据 5 | 「`dual-backend-rv32i-integration` change archived」 | 本草案即为其载体 ⇒ 立项后判据可判定 |
