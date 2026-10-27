# Design: RV32I 子组件的 dual-backend 覆盖补齐 Batch 2/3

- **change**：`dual-backend-rv32i-integration-2026-10`（**无 spec 变更** ⇒ 归档用 `--skip-specs`）
- **架构依据**：无（本 change 不触及架构决策；ADR-046/047 无关）
- **5 段草案**：`.rddf/improvements/dual-backend-rv32i-integration-2026-10.md`
- **本文件定位**：**实施设计**（机制读码结论 + 端口驱动纪律 + 金标推导 + 变异计划）

## 1. 范围与执行顺序

| 序 | 用例 | DUT | 风险档 |
|---|---|---|---|
| 1 | `IfIdPipelineReg` — transfers PC and instruction | `riscv::IfIdPipelineReg` | 含 `ch_reg`（时序） |
| 2 | `IfIdPipelineReg` — clears instr_valid on flush | 同上 | 含 `ch_reg`（时序） |
| 3 | `BranchPredictor` — BNT strategy | `riscv::BranchPredictor` | 纯组合 |

全部加进**既有**文件 `tests/test_rv32i_pipeline_behavior.cpp` ⇒
`CTEST_TOTAL` 202 / `EXAMPLES_TEST_SOURCES` 17 / `ctest -L verilator_e2e` 37 **三者均不变**。

## 2. 机制读码结论（**风险画像与既有表述不同**）

roadmap / plan T6 把 Batch 2 描述为「`IfIdPipelineReg` **clk-spike** ⇒ 若 spike 失败就记原因」。
读码后**该风险画像不成立**：

1. **`IfIdPipelineReg` 的 `clk` 是死端口** —— `include/cpu/pipeline/rv32i_pipeline_regs.h:190`
   声明了 `ch_in<ch_bool> clk`，但其 `describe()`（`:203-227`）**从不读 `io().clk`**；
   寄存器推进靠 `ch_reg::next` + Simulator/Verilog 的**隐式时钟**；
2. **隐式时钟的 dual 能力已被证明** —— `tests/test_dual_backend_clock.cpp`
   （Phase 1，spec R10）用 `CounterClock` 验证了 `always_ff @(posedge clk)` 在两后端一致；
3. **`BranchPredictor` 是纯组合** —— `include/cpu/pipeline/rv32i_branch_predict.h:218-232`
   的 IO 里**没有** clock/reset，也没有 `ch_reg` 状态。

⇒ **唯一**实质风险 = **输入端口漏驱动读到未定义值**（`AGENTS.md §ANTI-PATTERNS`：
实测 `strbe` 读全 1、`rst_n` 读 0）⇒ 未驱动的端口在两后端未必同值 ⇒ **同时红或同时绿都可能是假绿**。

### 2.1 ⚠️ 不得照抄原用例的手动 clk toggle

原 interpreter-only 用例（`tests/test_rv32i_pipeline.cpp:104-107`）在死端口上手动 toggle：

```cpp
sim.set_input_value(reg.instance().io().clk, true);
sim.tick();
sim.set_input_value(reg.instance().io().clk, false);
sim.tick();
```

那是**死端口上的两次额外 tick** ⇒ 照抄会把「一拍」变成三拍 ⇒ 若将来该端口被接回内部逻辑，
用例会因**多打两拍**而静默改变语义。
⇒ 本实现**只显式驱动 `clk`（不照抄 toggle）**，节拍数由 `sim.tick()` 决定
（`sim.tick()` 本身已做 2-step toggle，Verilog 侧亦然）。

## 3. 端口驱动表（**逐个显式驱动**，含死端口）

### 3.1 `IfIdPipelineReg`（7 个输入端口）

| 端口 | 值 | 理由 |
|---|---|---|
| `if_pc` | `0x100` | 被测值（装载后应出现在 `id_pc`） |
| `if_instruction` | `0x00100093` | 被测值（`addi x1, x0, 1`；装载后应出现在 `id_instruction`） |
| `if_instr_valid` | `1` / `0` | 决定 `id_instr_valid` |
| `rst` | `0` | ⚠️ **该组件的 `rst` 是高有效**（`rv32i_pipeline_regs.h:210` 的 `update = select(!io().rst, …)` ⇒ `rst=0` 才是正常工作）⇒ 驱动 **0**（不是 1！） |
| `stall` | `0` | `stall=1` ⇒ `update=0` ⇒ 寄存器保持（本次不测） |
| `flush` | `0` / `1` | 用例 2 用 `1` |
| `clk` | `1` | **死端口**（`describe()` 从不读）⇒ 仍 MUST 显式驱动（否则读到未定义值）⇒ **只置 1，不做 toggle**（§2.1） |

### 3.2 `BranchPredictor`（4 个输入端口）

| 端口 | 值 | 理由 |
|---|---|---|
| `instruction` | `0x00000063` | `BEQ x0, x0, +0`（opcode `0x63` = 99，funct3 = 0 = BEQ） |
| `rs1_data` | `0` | BEQ 的相等比较 |
| `rs2_data` | `0` | 同上 |
| `pc` | `0x100` | 分支目标 = `pc + imm`（`rv32i_branch_predict.h:328`） |

## 4. 金标推导（**手工推导**，MUST NOT 从仿真输出抄值）

### 4.1 用例 1：`IfIdPipelineReg` 装载 PC 与指令

推导（`rv32i_pipeline_regs.h:210-227`）：

```
update = select(!rst, select(!stall, !flush, false), false)
       = select(1, select(1, 1, false), false) = select(1, true, false) = true
⇒ pc_reg->next   = select(update, if_pc,   pc_reg)   = 0x100
⇒ instr_reg->next = select(update, if_instruction, instr_reg) = 0x00100093
⇒ valid_reg->next = select(update, if_instr_valid, …) = 1
```

⇒ tick 后：`id_pc == 0x100`、`id_instruction == 0x00100093`、`id_instr_valid == 1`。

**反向期望**（判别力）：若 IF/ID 的装载被 `stall`/`flush` 误门控 ⇒ `id_pc == 0`（INIT）
⇒ `0x100 != 0` ⇒ 该断言有判别力。

### 4.2 用例 2：`IfIdPipelineReg` 在 flush 时清 valid

推导：

- 第 1 拍（`flush=0`）⇒ 按 §4.1 装入 ⇒ `id_pc=0x100`、`id_instruction=0x00100093`、
  `id_instr_valid=1`；
- 第 2 拍 `flush=1` ⇒ `update = select(!rst, select(!stall, !flush, false), false)
  = select(1, select(1, 0, false), false) = false`
  ⇒ `pc_reg->next = select(false, if_pc, pc_reg) = pc_reg` ⇒ **保持 0x100**
  ⇒ `instr_reg` 同理**保持 0x00100093**
  ⇒ `valid_reg->next = select(update=false, instr_ready, select(flush=true, false, valid_reg))
  = select(flush, false, valid_reg) = false` ⇒ **valid 清零**

⇒ tick 后：`id_instr_valid == 0`；且 `id_pc == 0x100`、`id_instruction == 0x00100093`（**未变**）。

⚠️ **关键判别点**：flush **只清 valid**，**不清** pc/instruction
（`pc_reg`/`instr_reg` 的 next 由 `update` 门控，与 flush 无直接关系）
⇒ 若实现把 flush 误写成「清一切」⇒ `id_pc` 会变成 0 ⇒ 断言能抓到。
⇒ **反向期望**：flush 后 `id_instr_valid` MUST NOT 仍为 1；`id_pc` MUST **仍是** 0x100。

### 4.3 用例 3：`BranchPredictor` 的 BNT 策略

推导（`rv32i_branch_predict.h:243-330`）：

```
opcode  = bits<6,0>(0x00000063) = 0x63 = 99 ⇒ is_branch_opcode = true ⇒ is_branch = 1
funct3  = bits<14,12>(0x00000063) = 0 ⇒ is_beq = true
equal   = (rs1_data == rs2_data) = (0 == 0) = true
branch_condition = (is_beq & equal) | … = true
branch_taken_val = is_branch & branch_condition = true ⇒ branch_taken = 1
predict_error = branch_taken_val = 1        （BNT：预测不 taken ⇒ 实际 taken ⇒ 预测错）
imm: B 型立即数 = 0 ⇒ imm_extended = 0 ⇒ branch_target = pc + 0 = 0x100
```

⇒ `is_branch == 1`、`branch_taken == 1`、`predict_error == 1`、`branch_target == 0x100`。

**反向期望**：`predict_error` MUST NOT 为 0（BNT 的定义）；`branch_taken` MUST NOT 为 0。

## 5. 变异验证（每条先确认**变异后仍编译成功**）

| # | 变异 | 预期转红 | 抓的是 |
|---|---|---|---|
| M1 | 把 `valid_reg->next` 的 flush 分支反向（`select(flush, valid_reg, false)`） | 用例 2 | 「flush 只清 valid」被写坏 |
| M2 | 把 `predict_error` 改成恒 0 | 用例 3 | BNT 语义 |
| M3 | 漏驱动 `if_instr_valid` | 用例 1（`id_instr_valid` 读到未定义值 ⇒ 两后端可能分叉） | 端口驱动纪律（**判别力可能为 0** ⇒ 若抓不到 ⇒ 说明该端口无信号 ⇒ MUST 记录） |

⚠️ 变异脚本 MUST **先构建成功**再跑测试（否则跑的是旧二进制 ⇒ 假绿）。
⚠️ 变异后 MUST **重建**并复跑全量，**不得**把变异二进制留在 build 目录。

### 5.1 变异结果（**实测**，2026-10-27）

> ⚠️ 下表的 `test_...(cpp):NNN` 行号是**第 1 轮实测时**的文件状态；
> 第 1 轮 BLOCKER 的修复（Batch 2-b 补 `flush=false` 驱动 + 5 行注释）使后续行**整体下移 6 行**
> ⇒ 按图索骥时请以**断言内容**为准，不要依赖行号。

| # | 变异 | 构建 | 结果 | 抓到的断言 |
|---|---|---|---|---|
| M1 | `valid_reg->next` 的 flush 分支反向（`select(flush, valid_reg, false)`） | ✅ 编译成功（error 数 0） | **2 failed**（两后端同时红） | `test_...(cpp):537 REQUIRE( id_valid == 0 )` |
| M2 | `io().predict_error = ch_bool(false)`（恒 0） | ✅ 编译成功 | **2 failed** | `test_...(cpp):580`（`predict_error == 1`） |
| M3 | 漏驱动 `if_instr_valid`（Batch 2-a） | ✅ 编译成功 | **2 failed** | `test_...(cpp):484 REQUIRE( id_valid == 1 )` ⇒ **本次实测**未驱动端口两后端都读到 **0**（⚠️ 不构成对其它端口 / 后端的保证，见 §5.2）⇒ 该断言在本例有判别力 |
| M3' | 删掉 Batch 2-b 第 1 拍前的 `flush=false` 驱动（第 1 轮审稿的 BLOCKER 变异） | ✅ 编译成功 | **仍全绿（62 断言）** ⇒ **零判别力** | 无 —— 该未驱动端口在两后端也读到 **0** ⇒ 与 `flush=0` **碰巧等价** |

⇒ **3/3 全被抓到**（M1 / M2 / M3）；且每条都是**两后端同时**转红（说明两后端对该变异的行为一致 ⇒ 不是单侧假象）。
⇒ 每条变异都**先确认编译成功**；还原后重建并复跑 ⇒ `All tests passed (62 assertions in 32 test cases)`。

### 5.2 ⚠️ M3' 的教训（**如实记录**：纪律 ≠ 可判别性）

M3' **零判别力**这一事实说明：

- 「未驱动端口」**不一定**能被测试抓到 —— 当**期望值恰好等于**未驱动时读到的值时（本例 `flush` 期望 0、未驱动读到 0），变异**不会**转红；
- ⇒ **端口驱动纪律的价值不在「能抓到」，而在「不依赖未定义行为」**
  （`AGENTS.md §ANTI-PATTERNS` 的实测：`strbe` 读**全 1**、`rst_n` 读 **0** ⇒ 未驱动值**因端口而异、因后端而异**）；
- ⇒ M3（漏驱动 `if_instr_valid`）能转红**只是因为**它的期望值是 **1**（≠ 未驱动的 0）——
  这是**运气**，不是纪律的可判别性证明。
- ⇒ 结论：**「未驱动端口」这一类缺陷没有通用的变异判别力** ⇒ 只能靠
  ① 端口驱动表逐条核对（design.md §3）；② 代码评审（第 1 轮 critic 正是这样抓到的）。
  ⚠️ 这也印证了 `AGENTS.md §ANTI-PATTERNS` 里「**当前无自动化护栏覆盖这一类**」的登记
  （`scripts/verify_rst_n_wiring.sh` 只覆盖 `rst_n`）⇒ 该护栏缺口仍是 backlog 项。

## 6. 规格与簿记

- **无 spec 变更** ⇒ `.openspec.yaml` 已声明 `skip_specs: true` ⇒ 归档用 `--skip-specs`
- 簿记（与实现同批）：
  - `feat-dual-backend-coverage`：Phase 4 由 🟡 转 ✅
  - `feat-rv32i-integration`：最后一条验收勾选
  - `objective-rv32i-e2e`：Go 判据第 5 条勾选 ⇒ **6/6** ⇒ objective 可转 `completed`
    （⚠️ 转 completed 前 MUST 逐条复核 §9.2 的 6 条）
  - `roadmap.md`：sprint 表 / Unmapped
  - 根 `AGENTS.md §NOTES`：本 change 的记录（计数**不变**的原因）

## 7. 风险与回退

| 风险 | 缓解 |
|---|---|
| 未驱动端口 ⇒ 假绿 | 端口驱动表逐个显式驱动（§3）+ M3 变异 |
| ⚠️ **未驱动端口无通用变异判别力**（§5.2：M3' 零判别力） | 靠**端口驱动表逐条核对 + 代码评审**兜住 ⇒ 该护栏缺口仍是 backlog 项（`rst_n` 之外无自动化护栏） |
| 照抄手动 clk toggle ⇒ 多打两拍 | §2.1 明令禁止 + 代码注释留痕 |
| flush 语义误写成「清一切」 | §4.2 的「`id_pc` MUST 仍是 0x100」断言 + M1 变异 |
| 计数漂移 | 用例进**既有** target ⇒ 三个计数**均不变**；`verify_test_counts.sh` 守 |
| 破坏既有 13 个 Batch 1 用例 | 改动只**追加**在文件末尾；既有代码零改动 |
