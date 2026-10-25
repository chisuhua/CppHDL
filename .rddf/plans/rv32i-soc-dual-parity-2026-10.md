# rv32i-soc-dual-parity-2026-10 Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use skill_use("execute") to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 让 RV32I **SoC 级**（pipeline + I-TCM + D-TCM）在 interpreter 与 verilator 两后端下**逐周期** trace 一致，并把「JIT 因 mem read port 必须回落」从隐式行为写成**显式可断言契约**。

**Architecture:** 改动集中在 4 个面 —— ①`examples/riscv-mini/src/rv32i_pipeline_top.h`（空壳 → 真 SoC 顶层：固件写端口 + 10 个观测输出 + 删 4 个死端口）；②新测试 `tests/test_rv32i_soc_dual_parity.cpp`（手工双实例逐周期 diff + JIT 回落三件套）；③`src/jit/jit_compiler.cpp`（失败原因可归因）；④护栏 `scripts/verify_no_duplicate_rv32i_headers.sh`（C6 快照 + 豁免条目失效）+ 计数/文档同步。

**Tech Stack:** C++23, Catch2 v3.7.0, CMake 3.20+, Verilator（submodule，`third_party/verilator`）, bash（护栏）。

执行模式：**lightweight**（就地，无 worktree）。

---

## 设计要点：实测发现的 5 处（与 improvement 草稿的预诊断不同）

> 全部**先读源码 / 解码指令确证**，再落到代码。

### R1. `Rv32iPipelineTop` 是空壳 ⇒ 「DUT 就是它」当前不成立

6 个端口**全是 `ch_in`**，其中 4 个（`ext_instr_data` / `ext_instr_ready` /
`ext_data_read_data` / `ext_data_ready`）在 `describe()` 里**从未被引用**；
`InstrTCM` 有写端口但 top 没接 ⇒ imem 恒 0；`Rv32iPipeline` 的 6 个 `perf_*`
`ch_out` 一个都没引出。⇒ 顶层既**不可观测**也**不可装固件**。

⇒ 用户裁决（2026-10-25 P0）：**扩展 `Rv32iPipelineTop`**（方案 A），
写 ADR-046 **D10-rev**（D10 原文约束对象是 C2）+ 护栏 C6 快照同步。

### R2. `DUAL_BACKEND_TEST_PAIR` 宏做不了逐周期 diff

宏的语义是「同一 body 各跑一遍」，每次 run 只看得见自己的后端 ⇒ 无法跨后端比对。
⇒ 逐周期 diff 必须用「同一 TEST_CASE 内 2×`ch_device` + 2×`Simulator`」
（先例 `tests/test_verilator_complex_e2e.cpp` 的 SpiFsm 用例，
helper `read_port_output` / `make_temp_dir` / `tool_available`）。

### R3. lib 版 `HazardUnit` **不接数据前推** ⇒ 固件必须无数据冒险

`hazard_unit.h:151-152`：`io().rs1_data = io().rs1_data_raw;`
（接上会在真实装配里形成组合环 —— ADR-046 D3 #14，`git show bf78367` 是事故记录）。
ADR-046 **D5 对此仍悬置**。

⇒ 固件 MUST 在「生产者 → 消费者」之间插 NOP（数量实测确定）。
**MUST NOT** 以「接上前推」为解决手段（那是 D5 的裁决对象）。

### R4. JIT 的「跳过原因」在 mem read port 路径上**从未被设置**

`src/jit/jit_ir_gen_lnode_state.cpp:58-60` 直接 `return UNSUPPORTED_OP`，
不写 `last_error_msg_` ⇒ `src/simulator.cpp:500-502` 的
`CHINFO("JIT skipped (small graph or unsupported op): %s", last_error_msg())`
会打印**空串或陈旧值**（例如之前算过的 `graph too small`）。
⇒ 「因 mem read port 回落」与「因图太小跳过」**不可区分**。

⇒ 在 `generate_ir` 的中心失败出口（`if (r != JitResult::SUCCESS) return r;`）统一
`last_error_msg_ = "unsupported node type: <type> (node_id=N)"` —— 不改 free function
签名、不改 `JitResult` 枚举。

⚠️ `CHINFO` 走 **`std::cout`**（`include/utils/logger.h:351`），**不是** `cerr`。
测试捕获要换 `std::cout` 的 `rdbuf`，且捕获窗口必须包住 `set_jit_enabled(true)`
（编译尝试发生在那里，`src/simulator.cpp:519-521`）。

### R5. `InstrTCM<20,32>` 的 `SIZE = 1<<20` 字 = 4 MB 展开数组

对 20 条指令的 parity 固件毫无收益，且让 verilator 背一个 1M 元素的 `VlUnpacked`。
⇒ DUT 取 `ADDR_WIDTH = 12`（4096 字 = 16 KB）。
代价（MUST 写进注释）：**pc / 数据地址的高位被截断** ⇒ 固件与数据必须 < 16 KB。
`pc_reg` 复位 `0x80000000`（`if_stage.h:66`）低 12 位为 0 ⇒ 从字 0 起取指。

---

## Task 1: 契约（已完成）

**Files:** `openspec/changes/rv32i-soc-dual-parity-2026-10/{proposal,design,tasks}.md` + `specs/verilator-rv32i-e2e/spec.md`

- [x] proposal / design / tasks / spec delta
- [x] `openspec validate … --strict` valid；`verify_openspec_delta.sh` 4/4

## Task 2: DUT 端口改造（R1/R5）

**Files:** `examples/riscv-mini/src/rv32i_pipeline_top.h`

- [ ] **Step 1 (Red)**: 先写 `tests/test_rv32i_soc_dual_parity.cpp` 的**端口存在性**
      静态断言（`decltype(io().obs_perf_instructions)` / `prog_we` 等）⇒ 编译失败
- [ ] **Step 2**: 跑 `cmake --build build --target test_rv32i_soc_dual_parity` ⇒ **红**
      （`Rv32iPipelineTop` 没有这些成员）
- [ ] **Step 3 (Green)**: 改 top —— 删 4 死端口 + 加 `prog_*`(3) + 加 `obs_*`(10) +
      TCM 改 `ADDR_WIDTH=12` + 文件头注释写明地址截断约束
- [ ] **Step 4**: 重新构建 ⇒ 绿
- [ ] **Step 5 (Refactor)**: 端口按「时钟/复位 → 固件 → 观测」分组排列，注释指向
      `design.md` D2 表

verify:
```bash
cd "$(git rev-parse --show-toplevel)" && \
  cmake --build build -j"$(nproc)" --target test_rv32i_soc_dual_parity
```

## Task 3: 早期风险验证 —— verilator 能编译含 TCM 的 SoC（R5）

> **先做这一步**。若 12 位 TCM 仍编译失败 / 冷编译过慢（> 300 s）⇒
> D1 需要重开 P0（退回 fixture wrapper 方案）。

- [ ] **Step 1**: 2 条指令 + 1 个 store 的最小 DUT 冒烟，双后端 30 拍，
      只断言 `obs_perf_instructions` 终值一致
- [ ] **Step 2**: 跑一次，记录**实测数据**：冷编译耗时 / verilog 行数 / 峰值内存
- [ ] **Step 3**: 把数据写进 `design.md` D3 下方（「实测」小节）

verify:
```bash
cd "$(git rev-parse --show-toplevel)" && \
  /usr/bin/time -v ctest --test-dir build -R test_rv32i_soc_dual_parity --output-on-failure 2>&1 | \
    grep -E "Elapsed|Maximum resident|passed|failed"
```

## Task 4: 固件编码 helper + 交叉校验（R3）

**Files:** `tests/test_rv32i_soc_dual_parity.cpp`

- [ ] **Step 1 (Red)**: 写 `enc_r/enc_i/enc_s/enc_b` 的**手算常量**断言
      （`enc_i(1,0,0x13,1) == 0x00100093`、`enc_s(3,1,0x23,16) == 0x0030A823`）⇒ 先让
      helper 返回 0 ⇒ 红
- [ ] **Step 2**: 实现编码 ⇒ 绿
- [ ] **Step 3**: 固件 ≥ 20 条，三类路径（ALU / `sw`→`lw` / taken + not-taken branch），
      生产者→消费者之间插 NOP
- [ ] **Step 4**: 断言固件长度 ≥ 20 且所有地址 < 0x1000

## Task 5: 双路逐周期 parity（R2）

**Files:** `tests/test_rv32i_soc_dual_parity.cpp`

- [ ] **Step 1 (Red)**: `Rv32iSoc - DualBackendTraceParity` 先只断言
      `obs_perf_instructions` 终值 ⇒ 跑一次记录**实际**值（用于确定 golden）
- [ ] **Step 2**: 加逐周期 diff（10 个观测值 / 每 tick / ≥ 200 拍）
- [ ] **Step 3 (Green)**: 出现分歧时按 ZERO-DEBT 定位（不掩盖、不调宽容差）
- [ ] **Step 4**: 加语义 golden（3 条：ALU 结果落 dmem / load 读回值再落 / branch 选出哪条 store）
- [ ] **Step 5 (Refactor)**: helper 抽 `obs_snapshot(dev)` 返回 std::array<uint64_t,10>

## Task 6: JIT 归因（R4）

**Files:** `src/jit/jit_compiler.cpp`

- [ ] **Step 1 (Red)**: 测试断言 `last_error_msg` 含 `mem_read_port` ⇒ 当前为空 ⇒ 红
- [ ] **Step 2 (Green)**: `generate_ir` 中心失败出口写 `last_error_msg_`
- [ ] **Step 3**: 跑 ⇒ 绿；确认没有把 `graph too small` 那条覆盖掉（`jit_compiler.cpp:261`）

## Task 7: JIT 回落断言（R4）

**Files:** `tests/test_rv32i_soc_dual_parity.cpp`

- [ ] **Step 1 (Red)**: 三件套断言（enabled && !compiled / 日志归因 / trace 一致）
- [ ] **Step 2 (Green)**: `std::cout` 捕获 + 双跑；`#if __has_include` 包裹
- [ ] **Step 3**: `CH_JIT_ENABLE=OFF` 构建仍编译

## Task 8: 护栏 + 计数（D9/D10）

**Files:** `scripts/verify_no_duplicate_rv32i_headers.sh`, `tests/CMakeLists.txt`, root `AGENTS.md`

- [ ] **Step 1 (Red)**: 直接跑护栏 ⇒ 报「豁免 stale」（`rv32i_pipeline_top.h` 已有消费者）
      和 C6 端口不一致 ⇒ 红
- [ ] **Step 2 (Green)**: 删豁免条目 + `EXPECTED_DEAD_ALLOWANCES 0` + C6 快照更新
      + `mk_fixture` 的假顶层头同步 15 端口
- [ ] **Step 3**: `--self-test` 全绿（⑭/⑮/⑯ 仍各有判别力 —— 各自单独变异自证）
- [ ] **Step 4**: CMake 注册 + `CTEST_TOTAL 199 → 200` + `tests/test_*.cpp 115 → 116`

## Task 9: 文档同步（D11）

- [ ] `include/cpu/AGENTS.md`（归位说明）
- [ ] `docs/developer_guide/patterns/COMPONENT-LIFECYCLE-GUIDE.md`（端口示例）
- [ ] root `AGENTS.md` KNOWN LIMITATIONS（JIT 回落 → 已断言契约；固件无数据冒险）

## Task 10: 门禁 + 对抗审稿 + 归档

- [ ] 全量 `ctest -LE perf` / examples 28 / samples 35 / 7 护栏
- [ ] critic 通过
- [ ] `openspec archive` + 手写 `## Purpose`
