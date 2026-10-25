---
name: rv32i-pipeline-commit-path-2026-10
priority: P0
source: C3 实测发现（objective-rv32i-e2e 的**前置**）
phase: phase-5
category: bugfix
type: feature
created_at: 2026-10-25
---

# Improvement: RV32I 流水线 commit / valid 通路修复（C3 的前置）

- **Date drafted**: 2026-10-25
- **Drafted by**: build-agent（builder 角色，C3 P2 实测时发现）
- **Status**: 📋 Handoff document — 5-segment draft，等待 rdd-builder 启动
- **Objective**: `objective-rv32i-e2e` 的**前置**（原 C3 `rv32i-soc-dual-parity-2026-10` 已挂起）
- **阻塞对象**：`openspec/changes/rv32i-soc-dual-parity-2026-10`（C3，已挂起，草稿存于该目录 `drafts/`）

## 1. Why（背景与动机）

C3 的目标是「RV32I **SoC 级**双路 parity」。为此先把 `Rv32iPipelineTop` 从空壳
（6 个端口全是 `ch_in`、无固件加载、无观测输出）改造成真 DUT，再写 41 条指令的
双后端逐周期比对测试。

**结果：测试立刻证明 SoC 根本跑不动程序。**

| 实测（改造后的 DUT，41 条固件，200 拍） | 现象 |
|---|---|
| `perf_instructions` | **18**（应为 41）——且这 18 次全是「重启后头几条」 |
| `perf_branch_count` / `perf_branch_mispredict` | **18 / 18**（固件里只有 2 条分支） |
| `obs_instr_addr` | 每 11 拍从 `0x80000028` 跳回 **0** |
| `obs_data_write_en` | 53/200 拍置位，地址是 ALU 结果（10 / 13 / 0），**不是** store 地址 |

注入探针（在固件开头插一个 NOP，整体右移一字）⇒ 重启点从 cyc 10 移到 cyc 12
⇒ **触发点跟着程序走**，确认是「某条指令触发 flush ⇒ PC 归零」而非固定周期噪声。

### 为什么这件事必须先做

1. 没有可运行的程序，C3 的「≥20 指令双路 parity」写不出来；强行写只能断言「两端
   一致地错」——与仓库的「绿 ≠ 对」原则直接冲突（C1 的立论）。
2. 这些缺陷**不是 C3 引入的**：`if_stage` 的 flush 分支带着作者注释
   「简化处理，实际应该是分支目标」；`mem_valid_out` 的语义错误在
   `rv32i_pipeline.h:220` 的 `commit_count` 上已经存在。此前它们**不可观测**，
   因为 SoC 顶层没有任何输出端口（C2 的 AGENTS.md 也因此把它登记为零消费者豁免头）。

## 2. What Changes（变更范围）

### D1 `if_stage` 的 flush ⇒ PC 归零（**已落地**，本次）

```cpp
// 旧（include/cpu/pipeline/stages/if_stage.h，注释自认占位实现）
// flush 时 PC 重置为 0 (简化处理，实际应该是分支目标)
auto pc_flush_val = select(io().flush, ch_uint<32>(0_d), next_pc_target);
```

**推理**：`hazard.io().flush = is_branch & branch_taken`（`hazard_unit.h`），
而 `if_stage.io().branch_valid` 接的**正是** `ex_stage.io().branch_taken`
（`rv32i_pipeline.h:117`）⇒ flush 断言时 `next_pc_target` 已经是分支目标
⇒ **不需要**任何 flush 覆盖。

**落地结果**：PC 不再归零（实测 pc 一路 `0x8000002c → … → 0x800000c8` 前进），
且 7 个相关测试（`test_verilator_complex_e2e` / `test_rv32i_pipeline` /
`test_rv32i_pipeline_behavior` / `test_forwarding` / `test_hazard` /
`test_phase4_chlib` / `test_axi_*` 无关项未跑）全绿。

### D2 `mem_stage.mem_valid_out` 的语义错误（**待修**，阻塞 commit 计数）

```cpp
// include/cpu/pipeline/stages/mem_stage.h:206-217
auto mem_data_valid = io().is_load & io().mem_valid;
io().mem_valid_out = mem_data_valid;   // ← 端口名是「MEM 级有效」，实现是「load 数据有效」
```

`commit_count`（`perf_instructions` 的唯一来源）与 `memwb_valid` 都接它：

```cpp
// include/cpu/pipeline/rv32i_pipeline.h:220-221
auto commit_valid = mem_stage.io().mem_valid_out;
commit_count->next = select(reset_active, 0, commit_count + commit_valid);
```

⇒ **`perf_instructions` 统计的是「load 指令次数」**，不是退休指令数。
实测固件只有 1 条 `lw` ⇒ `perf_instructions` 卡在 1。

⚠️ 这同时意味着**既有测试是「绿而错」**：`tests/test_verilator_complex_e2e.cpp`
的 RV32I 用例断言 `perf_instructions == 3`（3 条 ALU 指令，无 load），
在该语义下**不可能是「3 条指令退休」** ⇒ 它被别的机制（spurious `is_load` /
早期 PC 重启）满足。

### D3 「EX/MEM 级寄存器装 ID 级信号」一整族（**待修**）

| 位置 | 现状 | 问题 |
|---|---|---|
| `rv32i_pipeline.h:143` | `exmem_is_load <<= id_stage.io().is_load` | EX/MEM 寄存器装**ID 级**标记 ⇒ 与同拍的 EX 指令错位 |
| `rv32i_pipeline.h:164-165` | `memwb_is_alu/is_jump->next = select(mem_stage.io().mem_valid, id_stage.io().is_alu/is_jump, …)` | MEM/WB 寄存器装 ID 级信号 |
| `rv32i_pipeline.h:192-193` | `hazard.io().ex_rd_addr/mem_rd_addr <<= id_stage.io().rd_addr` | EX / MEM 级的 rd 用 ID 级 ⇒ 冒险判定与前推选择错位 |
| `rv32i_pipeline.h:199` | `hazard.io().ex_branch <<= id_stage.io().is_branch` | 「EX 是分支」用 ID 级 ⇒ 与 `ex_branch_taken`（EX 级）**拍不对齐** ⇒ 实测 cyc 10 出现**无分支指令时的 spurious flush** |

（`hazard_detection_unit.h` 那处独立的 load-use 检测已在 C2 讨论中记录同样问题。）

### D4 `data_write_en` 在非 store 拍置位（**待定位**，现象已实测）

`ex_stage.io().is_store_alu = io().is_store`（`ex_stage.h:228`）与
`exmem_is_store <<= ex_stage.io().is_store_alu`（`rv32i_pipeline.h:138`）**看起来是接对的**，
所以 53/200 拍的 spurious `write_en` 需要逐拍追（候选：`ch_reg::operator<<=`
在 `ex_run` 路径上的语义、或 `is_store` 从 ID 到 EX 的传播）。
**MUST NOT** 当作「don't-care」掩盖 —— 它说明 dmem 被写了垃圾。

### D5 EX/MEM 寄存器裸值两后端分歧（**待定位**）

C3 的逐周期 diff 显示：`obs_instr_addr` + 6 个 `perf_*` **200 拍 0 处不同**，
而 `obs_data_addr` 17 处、`obs_data_write_data` 35 处不同（interp 13 / verilator 3；
interp 3 / verilator 0）。候选根因：父组件读**子组件输出**（`ex_stage.io().alu_result`
→ `exmem_alu_result`）时的求值序，或 `ch_reg::operator<<=` 的 codegen 形态。

⚠️ 这条**独立于 D1–D4**：即使 D1–D4 修完，它仍会让 C3 的逐周期 diff 失败。

## 3. Acceptance（验收标准）

### 3.1 D1（已落地部分）

- [x] `if_stage` 在 flush 时 PC 走向 `branch_target`（不再归零）
- [x] `ctest -R "test_verilator_complex_e2e|test_rv32i_pipeline|test_rv32i_pipeline_behavior|test_forwarding|test_hazard|test_phase4_chlib"` 全绿

### 3.2 核心验收：SoC 能跑完一段真实程序

- [ ] 41 条固件的 SoC 在 interpreter 下 `perf_instructions` 到达**预期退休数**
      （由程序结构推导，含被 taken 分支冲刷的那一条）
- [ ] `obs_instr_addr` 序列与程序的取指顺序一致（含 taken 分支的跳转）
- [ ] `obs_data_write_en` **只在** store 拍置位（3 条 `sw` ⇒ 恰好 3 个 store 拍）
- [ ] 三个 store 的 `(addr, data)` 等于 ISA 正确值（`0x100←10` / `0x104←10` / `0x108←2`）
- [ ] `perf_branch_count` == 固件分支数；`perf_branch_mispredict` 有确定值

### 3.3 后端一致性

- [ ] 10 个观测值在 200 拍内**逐周期完全一致**（不再需要用 don't-care 掩码绕开
      `data_addr`/`data_write_data`）

### 3.4 测试诚实性

- [ ] `test_verilator_complex_e2e` 的 RV32I 断言改为**语义断言**
      （3 条 ALU 指令必须让 x3 == 3；`perf_instructions` 断言改为「≥ 3 且 `x3` 正确」
      或直接改为依赖 store 结果），**不得**继续用「计数器 == 3」这种可被 spurious 事件满足的断言
- [ ] 若某断言在修复前后都绿 ⇒ 必须查明它到底钉了什么（「绿 ≠ 对」）

### 3.5 门禁

- [ ] `ctest -LE perf` 零回归；`CTEST_TOTAL` 同步
- [ ] 护栏脚本全绿（若 C3 的 DUT 端口契约随之落地 ⇒ C6 快照 + 零消费者豁免同步）

## 4. Capabilities（涉及的能力维度）

| Capability | 影响 | 备注 |
|------------|------|------|
| `rv32i-canonical-home` | 可能 MODIFIED | 若落地 C3 的 DUT 端口契约 ⇒ 零消费者豁免条目失效（其既有条款要求移除） |
| 新 `rv32i-commit-path` | ADDED（建议） | 「退休 = 流水线有效 & 未被冲刷」；「EX/MEM 级寄存器 MUST NOT 装 ID 级信号」；「观测端口契约」 |
| `verilator-rv32i-e2e` | MODIFIED | 既有 Requirement 只钉 3 条 ALU 指令的 `perf_instructions == 3`，需改为语义断言 |

## 5. Impact（影响面）

### 5.1 风险

| 风险 | 等级 | 缓解 |
|---|---|---|
| D3「ID 级信号装进 EX/MEM 寄存器」是一整族，逐个修会连环改行为 | 🔴 高 | 先修 D2（commit 通路，收益最大且可独立验证），再逐个修 D3 的 4 处并每处跑 C3 的逐周期 diff |
| 修 D2 会改变 `perf_instructions` 的**所有**既有断言 | 🟡 中 | 先 grep 全部 `perf_instructions` 断言（`tests/`、`openspec/specs/verilator-rv32i-e2e/`），逐个改为语义断言 |
| D5 疑似 codegen 求值序 ⇒ 可能牵进 `src/codegen_verilog.cpp` | 🟡 中 | 先用最小 DUT 二分（单寄存器跨层次读）定位，再决定是否进 codegen |
| 修完 D1–D4 后 load/store 才真正执行 ⇒ 可能暴露 dmem 时序问题 | 🟡 中 | 固件已按「无前推」构造（ADR-046 D5 悬置），load-use 间隔 ≥3 NOP |

### 5.2 明确不做（Non-Goals）

- **不接数据前推**（ADR-046 **D5** 仍悬置未裁决）—— 固件按无前推构造
- **不实现 JIT 的 mem-read lowering**（独立 objective `jit-mem-read-port-support`）
- **不做 cache 的 SoC 级集成**
- **不重写流水线**：目标是让现有装配正确，不是换架构

### 5.3 复现材料（C3 已备好，直接可用）

```bash
# 1) 把 C3 的草稿 DUT + 测试放回（C3 挂起时它们不在树里）
cp openspec/changes/rv32i-soc-dual-parity-2026-10/drafts/rv32i_pipeline_top.h.draft \
   examples/riscv-mini/src/rv32i_pipeline_top.h
cp openspec/changes/rv32i-soc-dual-parity-2026-10/drafts/test_rv32i_soc_dual_parity.cpp.draft \
   tests/test_rv32i_soc_dual_parity.cpp
git apply openspec/changes/rv32i-soc-dual-parity-2026-10/drafts/cmake-registration.diff

# 2) 构建 + 跑
cmake -S . -B build && cmake --build build -j"$(nproc)" --target test_rv32i_soc_dual_parity
RV32I_SOC_DEBUG=200 ./build/tests/test_rv32i_soc_dual_parity "Rv32iSoc - DualBackendTraceParity"
#   ⇒ 每拍打印 10 个观测值的 interp/verilator 对（`-` 分隔），
#     以及逐周期 diff 的首个失败点。
```

固件与 golden 值、编码器（含手算常量交叉校验）都在该草稿测试里，可直接复用。
