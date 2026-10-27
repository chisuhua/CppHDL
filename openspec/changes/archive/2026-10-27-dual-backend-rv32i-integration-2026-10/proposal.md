# 提案：RV32I 子组件的 dual-backend 覆盖 MUST 补齐 Batch 2/3

- **5 段草案**：`.rddf/improvements/dual-backend-rv32i-integration-2026-10.md`（P1，2026-10-27 起草）
- **roadmap 归属**：`feat-dual-backend-coverage`（Phase 4 残余）+ `feat-rv32i-integration`（最后一条验收）+ `objective-rv32i-e2e`（Go 判据第 5 条）
- **规格影响**：**无 spec 变更** ⇒ 本 change 归档时 MUST 用 `openspec archive --skip-specs`
  （`AGENTS.md` 的规则：只有显式声明「no spec changes」的 change 才可用该标志）
  ⇒ 理由：`dual-backend-test-coverage` capability 治的是 **macro / CMake / 排除清单**，
  **不含**「按 Phase 记录进度」的 Requirement ⇒ 本 change 不新增也不修改任何 Requirement。
  ⇒ 受锁的 9 个 verilator 测试（`dual-backend-test-coverage` 的「现有 9 个 verilator 测试
  MUST 保留原状」）**不含**本 change 触碰的文件（`tests/test_rv32i_pipeline_behavior.cpp`）
  ⇒ **不触发** spec 锁。
- **前置**：无（C2 已归档 2026-10-25；Batch 1 已归档 2026-10-19）
- **规模**：**小**（3 个 dual pair + 簿记）⇒ 无需 ADR

## Why

### 缺口：一条 Go 判据没有可执行载体

`objective-rv32i-e2e` 的 Go 判据第 5 条写的是
「`dual-backend-rv32i-integration` change archived」，但这个 change **从未立项**：

- `openspec/changes/` 与 `openspec/changes/archive/` **均无**该名字；
- 范围定义只存在于两处**注释**里：`tests/test_rv32i_pipeline_behavior.cpp` 的文件头
  Batch 划分 + `.rddf/plans/2026-10-26-phase5-followup-execution-plan.md` 的 **T6**。

⇒ 一个「按名字判定」的 Go 判据，其被判定对象不存在 ⇒ 判据**永远无法达成**
⇒ objective 会因此**永久 active**（phase-5 收口时会出现「唯一未勾项是个不存在的 change」）。

### 事实：Phase 4 的 Batch 1 已归档，只剩 3 个 dual pair

`tests/test_rv32i_pipeline_behavior.cpp`（2026-10-19 归档于
`dual-backend-rv32i-pipeline-behavior-tests`）已落地 **Batch 1 = 13 个 dual pair /
5 个组件类**（BranchDetector ×3 / BranchConditionUnit ×4 / ForwardingUnit ×3 /
ControlHazardUnit ×1 / HazardDetectionUnit ×2）。剩余：

| Batch | DUT | 用例 | 原用例位置 |
|---|---|---|---|
| 2 | `IfIdPipelineReg` | 2（`transfers PC and instruction` / `clears instr_valid on flush`） | `tests/test_rv32i_pipeline.cpp` 的 **SECTION** 内 |
| 3 | `BranchPredictor`（BNT） | 1 | 同上（SECTION 内） |

**为什么它们没做**：不是硬件风险，而是原用例全在 **Catch2 SECTION** 里 ——
`dual-backend-test-coverage` 的 **R6 #5** 明确排除 SECTION 重入（macro 展开成 2 个
TEST_CASE，每个 half 独立遍历 SECTION ⇒ 破坏 fixture 语义）⇒ Batch 2/3 需要
**SECTION-free 重写**（放进已有的 `_behavior` 文件），而不是「迁移」。

### 更正两处被高估/算错的口径

**① 「clk-spike 风险」不成立**（roadmap / T6 都把它当成风险项）—— 读码后：

1. **`IfIdPipelineReg` 的 `clk` 是死端口**：`include/cpu/pipeline/rv32i_pipeline_regs.h`
   声明了 `ch_in<ch_bool> clk`，但其 `describe()`（`:203-227`）**从不读 `io().clk`**；
   寄存器推进靠 `ch_reg::next` + Simulator/Verilog 的**隐式时钟**。
2. **隐式时钟的 dual 能力已被证明**：`tests/test_dual_backend_clock.cpp`
   （Phase 1，spec R10「2-step clock toggle fixture」）已用 `CounterClock` 验证
   `always_ff @(posedge clk)` 在两后端一致。
3. **`BranchPredictor` 是纯组合**：`include/cpu/pipeline/rv32i_branch_predict.h:218-232`
   的 IO 里没有任何 clock/reset，也没有 `ch_reg` 状态。

⇒ 真实风险只有一条（本仓库已登记过的老问题）：**输入端口若不被显式驱动，读到的是
未定义值**（`AGENTS.md §ANTI-PATTERNS`：实测 `strbe` 读全 1、`rst_n` 读 0）⇒ 双后端
未驱动初值未必相同 ⇒ **同时红或同时绿都可能是假绿**。

**② `ctest -L verilator_e2e` 的计数是「文件数」，不是「用例数」** ——
`add_dual_backend_test()`（`tests/CMakeLists.txt:24-44`）= `add_catch_test(TEST_NAME …)`
+ `set_tests_properties(LABELS "base;verilator_e2e")` ⇒ **一个文件 = 一个 ctest 条目**
（实测 37 = 含 `DUAL_BACKEND_TEST_PAIR` 的 `.cpp` 文件数）
⇒ T6 的验收「+3 ⇒ 37 → 40」**是错的**；在**既有**文件里加 pair ⇒ **计数不变**。

## What Changes

### 1. 在既有文件加 3 个 `DUAL_BACKEND_TEST_PAIR`（不新建文件、不新建 target）

**文件**：`tests/test_rv32i_pipeline_behavior.cpp`

- **Batch 2-a** `IfIdPipelineReg` 传输：驱动 `if_pc` / `if_instruction` / `if_instr_valid` /
  `rst=0` / `stall=0` / `flush=0` / `clk=1` ⇒ `tick()` ⇒ `id_pc` / `id_instruction` 等于输入
- **Batch 2-b** `IfIdPipelineReg` flush：先打入有效值并 `tick()`，再 `flush=1` + `tick()`
  ⇒ `id_instr_valid == 0`（**只清 valid**：`describe()` 里 `pc_reg` / `instr_reg` 的 next
  由 `update` 门控、与 flush 无直接关系 ⇒ 断言 MUST 按**实际 RTL 语义**写，
  MUST NOT 按「flush 清一切」的直觉写）
- **Batch 3** `BranchPredictor` BNT：`instruction=0x00000063`（`BEQ x0,x0,+0`）+
  `rs1_data=rs2_data=0` ⇒ `is_branch=1` / `branch_taken=1` / `predict_error=1`

### 2. 端口驱动纪律（唯一的实质风险点）

- `IfIdPipelineReg` 的 **7 个输入端口逐个显式驱动**（含那个**死端口** `clk`）
- ⚠️ **MUST NOT 照抄**原 SECTION 用例的手动 clk toggle
  （`set clk=true → tick() → set clk=false → tick()`）：那是**死端口上的两次额外 tick**
  ⇒ 照抄会把「一拍」变成三拍；`sim.tick()` 本身已做 2-step toggle（Verilog 侧亦然）
- ⇒ 夹具 MUST 走「一次 tick = 一拍」的语义

### 3. `tests/test_rv32i_pipeline.cpp` MUST NOT 修改

（spec R7：保留 interpreter-only baseline；该文件是 12 个 TEST_CASE + 22 个 SECTION 的
既有基线，本 change 只在 `_behavior` 文件里**新建** SECTION-free 用例）

### 4. 簿记（planner 侧与 builder 侧同批）

- `feat-dual-backend-coverage`：Phase 4 由 🟡 转 ✅（Batch 2/3 完成）
- `feat-rv32i-integration`：最后一条验收「`dual-backend-rv32i-integration` change archived」勾选
- `objective-rv32i-e2e`：Go 判据第 5 条勾选（6/6 ⇒ objective **转 completed**）
  ⇒ ⚠️ 转 completed 前 MUST 复核 §9.2 的 Go 判据**逐条**满足
- `roadmap.md`：sprint 表 / Unmapped 计数

## Acceptance

### 功能性

- [ ] 3 个 dual pair **双后端全绿**（interpreter + verilator 各跑一遍）
- [ ] `IfIdPipelineReg` 传输：`id_pc` / `id_instruction` 等于驱动值
- [ ] `IfIdPipelineReg` flush：`id_instr_valid == 0`（且 `id_pc` / `id_instruction` **按实际
      RTL 语义**断言 —— 不是「清一切」）
- [ ] `BranchPredictor` BNT：`is_branch=1` / `branch_taken=1` / `predict_error=1`

### 可判别性（零信号断言不合格）

- [ ] 每个用例带**反向期望**（例：flush 后 `id_instr_valid` MUST NOT 仍为 1；
      BNT 用例 `predict_error` MUST NOT 为 0）
- [ ] golden **手工推导** + 代码注释留痕（MUST NOT 从仿真输出抄值）
- [ ] **变异验证**（每条先确认**变异后仍编译成功**，否则跑的是旧二进制 ⇒ 假绿）：
      - 把 flush 分支写成 `select(io().flush, ch_bool(true), valid_reg)`（反向）⇒ Batch 2-b 转红
      - 把 `predict_error` 改成恒 0 ⇒ Batch 3 转红
      - 漏驱动 `if_instr_valid` ⇒ 至少一个用例转红（否则说明该端口无信号 ⇒ 用例无效）

### 回归与工程

- [ ] `tests/test_rv32i_pipeline.cpp` **零改动**且仍全绿
- [ ] `CTEST_TOTAL` **202 不变** + `ctest -L verilator_e2e` **37 不变**
      （⚠️ 见 Why ②：一文件一条目 ⇒ 加 pair 不改计数）
- [ ] `EXAMPLES_TEST_SOURCES` **17 不变**
- [ ] `ctest -LE perf` 零回归（191/191）
- [ ] 7 个 `scripts/verify_*.sh` 全 exit 0
- [ ] `openspec validate --specs --strict` 不因本 change 变红
      （⚠️ 本仓 CLI 漂移：1.14.3 比仓库钉的 1.4.1 多两条 warning 规则 ⇒ 以**本 change
      前后无新增**为准，不以「全绿」为准）

## Capabilities

- **`dual-backend-test-coverage`** —— **无 spec 变更**（见头部声明）；本 change 是它
  Phase 4 的收尾。R6 排除清单**不变**（SECTION 重入仍排除 ⇒ 本 change 是**新建
  SECTION-free 用例**，不是解除排除）
- `verilator-rv32i-e2e` —— 无新契约（clk / 隐式时钟语义已由 `verilator-backend-e2e`
  与 `test_dual_backend_clock.cpp` 覆盖）
- `rv32i-commit-path` —— 无影响（不碰流水线装配，只加单元级 dual 用例）

## Impact

- **计数**：`CTEST_TOTAL` **202 不变** / `EXAMPLES_TEST_SOURCES` **17 不变** /
  `ctest -L verilator_e2e` **37 不变**
- **文件面**：`tests/test_rv32i_pipeline_behavior.cpp`（+~70 行）；
  `tests/test_rv32i_pipeline.cpp` **零改动**
- **风险**：**低**。无新 RTL、无新端口、无框架层改动。唯一实质风险 =
  「输入端口漏驱动 ⇒ 未定义值 ⇒ 假绿」⇒ 由端口驱动纪律 + 变异验证兜住
- **不做会怎样**：`objective-rv32i-e2e` 的 Go 判据第 5 条**不可判定**
  ⇒ objective 永久 active ⇒ phase-5 收口时唯一未勾项是个**不存在的 change**

## Non-Goals

- 不解锁 SECTION 重入（R6 #5 仍排除）
- 不把 `test_rv32i_pipeline.cpp` 整体 dual 化（属 Phase 5 全量覆盖面）
- 不动 `IfIdPipelineReg` 的**死端口 `clk`**（删端口 = API 变更 ⇒ 独立立项；
  本 change 只**显式驱动**它）
- 不追求 `ctest -L verilator_e2e` 数字增长（本 change 不改计数 ⇒ 见 Why ②）
