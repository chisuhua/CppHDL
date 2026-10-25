# 提案：RV32I SoC 双路 parity（interpreter + verilator）+ JIT 回落行为显式断言

> ## ⏸ 状态：**挂起**（2026-10-25 P2 实测后）
>
> **阻塞于** `.rddf/improvements/rv32i-pipeline-commit-path-2026-10.md`
> （RV32I 流水线 commit / valid 通路修复）。
>
> **原因**：P2 起步时把 `Rv32iPipelineTop` 从空壳改成真 DUT 并写了 41 条指令的双后端
> 逐周期比对测试 ⇒ **测试立刻证明 SoC 根本跑不动程序**：41 条固件 200 拍只退休 18 条，
> `pc` 每 11 拍归零，`perf_branch_count` 凭空 +18，`data_write_en` 在 53/200 拍
> 以 ALU 结果地址置位。逐层追出 **5 个前置缺陷**（1 个已修、4 个已定位到接线级）。
>
> ⇒ 本 change 的「≥20 指令 SoC 级双路 parity」在 SoC 不能跑程序时**写不出来**；
> 强行写只能断言「两端一致地错」—— 与仓库「绿 ≠ 对」原则冲突。
>
> **已完成的产物不丢**（`drafts/`）：
> - `drafts/rv32i_pipeline_top.h.draft` —— 端口契约改造（删 4 死端口 + 固件写端口 +
>   10 个观测输出 + TCM `ADDR_WIDTH=12`），**已编译通过、verilator 冷编译 13.7 s**
> - `drafts/test_rv32i_soc_dual_parity.cpp.draft` —— 编码器 + 手算常量交叉校验（12 断言绿）、
>   41 条固件、双实例逐周期 diff、`RV32I_SOC_DEBUG=N` trace dump
> - `drafts/cmake-registration.diff`
>
> **已落地**（不随本 change 挂起）：`if_stage` 的 `flush ⇒ PC=0` 占位实现已移除
> （见新 improvement 的 D1），7 个相关测试仍全绿。
>
> **恢复条件**：前置 change 的 §3.2 核心验收通过（SoC 能跑完 41 条固件）⇒ 把
> `drafts/` 下的三件套放回树里，从 T2 继续（见 `tasks.md`）。

## Why

`objective-rv32i-e2e` 的旗舰目标。当前 RV32I 的 verilator parity 只走到
**3 条无 hazard 的 ALU 指令**（`tests/test_verilator_complex_e2e.cpp::Rv32iShortInstructionTraceParity`），
观测点只有一个终值（`perf_instructions == 3`）。真实 SoC（pipeline + I-TCM + D-TCM）
**从未在双后端下逐周期比对过**，load/store/branch 路径也从未进入 verilator。

C1（`riscv-mini-test-triage-2026-10`）建立了绿色基线、C2
（`rv32i-canonical-convergence-2026-10`）把微架构收敛成 `include/cpu/` 的唯一实现
⇒ 现在有了**确定的 parity 对象**，本 change 是 e2e parity 的收口。

### 为什么现在才做：DUT 当前不是 DUT

`Rv32iPipelineTop`（ADR-046 D10 指定的 C3 DUT）实测是**空壳**：

| 实测 | 证据 |
|------|------|
| **6 个端口全是 `ch_in`，零 `ch_out`** | `examples/riscv-mini/src/rv32i_pipeline_top.h:13-18` ⇒ 顶层**不可观测** |
| **其中 4 个端口在 `describe()` 里从未被引用** | `ext_instr_data` / `ext_instr_ready` / `ext_data_read_data` / `ext_data_ready`（只接了 `ext_rst` / `ext_clk`）⇒ 死端口 |
| **无固件加载通路** | `InstrTCM` 有 `write_en` / `write_addr` / `write_data` 写端口（`rv32i_tcm.h:35-37`），但 top **没接** ⇒ imem 恒 0 ⇒ 只能跑 NOP |
| **6 个 perf 计数器被丢弃** | `Rv32iPipeline` 有 `perf_cycles` / `perf_instructions` / `perf_branch_count` / `perf_branch_mispredict` / `perf_load_use_stalls` / `perf_control_stalls`（`rv32i_pipeline.h:53-58`），top 一个都没引出 |

⇒ 所以「DUT = `Rv32iPipelineTop`、观测点走顶层 `ch_out`」这条验收标准
**在不动端口的前提下写不出来**。ADR-046 D10 的原文约束的是「**C2** 不得改端口」
（理由正是「C3 的观测点会被重做」—— 暗示观测点由 C3 定义），而 `AGENTS.md` 里
「MUST NOT 为观测加输出端口」那句是在**删除 `test_pipeline_smoke`** 的语境下写的
（该测试想靠加端口续命）。⇒ 本 change 把「C3 扩展端口」写成显式裁决（D10-rev）。

### 为什么是「双路」而不是「三路」

「三路 parity」不可能原生成立：`src/jit/jit_ir_gen_lnode_state.cpp:58-60` 对
`type_mem_read_port` 返回 `UNSUPPORTED_OP`，而 RV32I 必有 imem ⇒ JIT 必然整体回落
interpreter（2026-10-23 用户裁决）。⇒ 目标改为
**双路 parity（interpreter + verilator）+ JIT 回落行为显式断言**：把「第三路」
从**隐式**（悄悄回落）变成**显式契约**。

### 为什么 JIT 回落需要代码改动

`CHINFO("JIT skipped (small graph or unsupported op): %s", last_error_msg())`
（`src/simulator.cpp:500-502`）打印的 `last_error_msg_` 在 **mem read port 路径上
从未被设置**：`jit_ir_gen_lnode_state.cpp:58` 直接 `return UNSUPPORTED_OP`，
不写原因 ⇒ 日志会打印**空串或上一次的陈旧值**（例如 `graph too small`）。
⇒ 「JIT 因 mem read port 回落」与「JIT 因图太小跳过」**当前不可区分**，
而这两者的语义完全不同（前者是架构限制，后者是性能优化）。

## What Changes

### C3-a DUT：`Rv32iPipelineTop` 从空壳改成真 SoC 顶层

- **删除 4 个死端口**（`ext_instr_data` / `ext_instr_ready` / `ext_data_read_data` /
  `ext_data_ready`）—— ZERO-DEBT「无死代码」；TCM 在内部直连 pipeline，不存在
  「外部存储器接口」这个概念。
- **新增固件写端口**（3 个）：`prog_we` / `prog_addr` / `prog_data` →
  `itcm.io().write_en` / `write_addr` / `write_data`。
- **新增观测输出**（10 个）：`obs_instr_addr` / `obs_data_addr` /
  `obs_data_write_data` / `obs_data_write_en` + 6 个 `obs_perf_*`。
- 端口总数 15；最大位宽 48 < **64**（ADR-035 R3 verilator 端口上限）。
- TCM 规模取 `ADDR_WIDTH = 12`（4096 字 = 16 KB）：`1<<20` 字的 `ch_mem` 在
  verilator 下是 4 MB 展开数组，对 20 条指令的 parity 固件毫无收益。
- **ADR-046 D10-rev**：D10 原文的约束对象是 C2；C3 作为「归位事件」允许扩端口，
  且新端口集合由护栏 C6 快照钉住（改动可审计）。

### C3-b 双路 trace parity（新测试 target）

`tests/test_rv32i_soc_dual_parity.cpp`：

- 固件 ≥ 20 条指令，覆盖 **三类路径**：ALU-only + load/store + branch（含 taken /
  not-taken）。
- imem MUST 用 `sread`（保 `pc_lag` 对齐，`verilator-e2e-coverage` §R1 invariant 3）。
- **两实例手工双跑**（不是 `DUAL_BACKEND_TEST_PAIR`）：宏的语义是「同一 body 各跑一遍」，
  每个 run 只看得见自己的后端 ⇒ **无法**做跨后端逐周期 diff。逐周期比对必须
  像 `test_verilator_complex_e2e.cpp` 那样在同一 TEST_CASE 内建两个 `ch_device`
  + 两个 `Simulator`。
- 每 tick 比对 **10 个观测值**，另外断言**终值 golden value**（两端都错成一样时
  逐周期 diff 抓不到，golden value 兜住）。

### C3-c JIT 回落行为显式断言

1. `jit_compiler.cpp::generate_ir` 在 `r != SUCCESS` 时写
   `last_error_msg_ = "unsupported node type: <name> (node_id=N)"` ⇒ 日志可归因；
2. 测试断言 `set_jit_enabled(true)` 后 `is_jit_compiled() == false`（真的回落了）
   且日志含 `JIT skipped` + `mem_read_port`；
3. 回落后的观测向量与显式 `set_jit_enabled(false)` 的 interpreter 结果**逐周期一致**。

### C3-d 契约与护栏

- 新 capability delta：`verilator-rv32i-e2e` **ADDED**（SoC 级双路 parity +
  观测端口契约 + JIT 回落断言）。
- `scripts/verify_no_duplicate_rv32i_headers.sh`：
  - **C6 端口快照**更新为新端口集合；
  - **移除** `rv32i_pipeline_top.h` 的零消费者豁免条目，`EXPECTED_DEAD_ALLOWANCES`
    `1 → 0`（`rv32i-canonical-home` 的「stale 豁免 MUST 失败」要求它，本 change 正是
    该豁免的**失效条件**达成）。
- `include/cpu/AGENTS.md` / `examples/riscv-mini/src/*` 与
  `docs/developer_guide/patterns/COMPONENT-LIFECYCLE-GUIDE.md` 的端口示例同步。

## Acceptance

### A1 编译 + 测试

- [ ] `cmake --build build -j$(nproc)` 0 error 0 warning
- [ ] `ctest -R test_rv32i_soc_dual_parity` 绿（interpreter + verilator 两半都真跑）
- [ ] `ctest -LE perf` 零回归；`CTEST_TOTAL` `199 → 200`（+1 target）；
      `tests/test_*.cpp` `115 → 116`；`ctest -L verilator_e2e` 同步 +1

### A2 双路 parity

- [ ] 固件 ≥ 20 条指令，含 load/store + branch
- [ ] 每 tick 比对 10 个观测值，`≥ 200` 拍内全部相等
- [ ] 终值 golden value 断言（`perf_instructions` == 固件中全部退休指令数、
      `perf_branch_count` == 固件中分支数、`perf_branch_mispredict` 取值固定）
- [ ] DUT 观测点全走顶层 `ch_out`（verilator 可观测）；`imem` 用 `sread`

### A3 JIT 回落

- [ ] `is_jit_compiled() == false` 且 `is_jit_enabled() == true`（回落而非未启用）
- [ ] 日志含 `JIT skipped` **且**原因可归因到 `mem_read_port`（非空、非 `graph too small`）
- [ ] 回落 trace 与 `set_jit_enabled(false)` 逐周期一致

### A4 护栏 + 文档

- [ ] `scripts/verify_no_duplicate_rv32i_headers.sh` PASS（**豁免条目已移除**、
      C6 快照已更新）+ `--self-test` 全绿（fixture 的假顶层头同步新端口集合）
- [ ] `openspec validate rv32i-soc-dual-parity-2026-10 --strict` valid
- [ ] `include/cpu/AGENTS.md` + `COMPONENT-LIFECYCLE-GUIDE.md` 与磁盘一致
- [ ] root `AGENTS.md` 的 KNOWN LIMITATIONS 中「JIT 回落」条目更新为**已断言契约**

## Non-Goals

- **不实现 JIT 的 mem-read lowering**（独立 objective `jit-mem-read-port-support`）——
  本 change 只把「必须回落」写成契约，不改 JIT 能力。
- **不做 cache 的 SoC 级 parity**（`ICacheSync` / `DCacheSync` 集成是 follow-up）——
  本 change 的 DUT 是 pipeline + TCM。
- **不做 AXI 互连的 SoC 级 parity**（`axi4-interconnect-routing-2026-10` 的事）。
- **不引入 commit-PC 轨迹端口**：`pc` 只存在到 EX 级（`rv32i_pipeline.h:123-135`），
  未穿过 MEM/WB ⇒ 逐指令 PC 轨迹需要给流水线加寄存器（微架构改动，超出本 change）。
  现实的等价物 = `obs_instr_addr`（取指地址流）+ `perf_instructions`（退休数）。
- **不改 `rv32i-canonical-home` 的主 spec**：其「stale 豁免 MUST 失败」条款已覆盖
  本 change 对豁免表的处理，无需 delta。
- **不改 `dual-backend-test-coverage`**：本 change 消费 `add_dual_backend_test` /
  宏的既有语义，但逐周期 diff 走手工双实例（该宏覆盖不到，也不该覆盖）。

## Impact

### 风险

| 风险 | 等级 | 缓解 |
|------|------|------|
| DUT 扩端口触碰 ADR-046 D10 | 🟡 中 | 写 D10-rev（约束对象是 C2）+ 护栏 C6 快照同步 ⇒ 改动可审计、不可静默 |
| `InstrTCM`/`DataTCM` 在 verilator 下的大数组编译成本 | 🟡 中 | `ADDR_WIDTH=12`（4096 字）；实测冷编译时间写入 tasks 的 verify 块 |
| 20+ 条指令手写机器码易错 | 🟡 中 | 测试内建最小编码 helper（R/I/S/B 型）+ **对手算常量交叉校验**（不是自证） |
| 逐周期 diff 在 load/store 路径上暴露真实 codegen 缺陷 | 🟢 低（**这是目的**） | 若暴露 ⇒ 定位后按 ZERO-DEBT 修复或登记 follow-up；不掩盖 |
| 固件加载需要多拍写 imem（扰动计数器） | 🟢 低 | 加载期保持 `ext_rst=1`（计数在复位期被清 0）⇒ 释放复位后计数从第一条指令起算 |
