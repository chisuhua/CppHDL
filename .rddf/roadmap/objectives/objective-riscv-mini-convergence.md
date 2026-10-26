---
id: objective-riscv-mini-convergence
status: completed
created: 2026-10-23
last_revised: 2026-10-26
review_by: 2027-01-23
owner: rdd-planner
priority: P0
manual_deps: []
supersedes: null
theme: 消除 riscv-mini 技术债 + 把 include/cpu 与 examples/riscv-mini 收敛成单一 RV32I 实现
---

# Objective: objective-riscv-mini-convergence

## 1. 驱动诊断（Why now）

2026-10-23 审计发现 riscv-mini 子系统有四类结构性技术债，**任何一类都足以让「RV32I e2e parity」这个
旗舰目标失去意义**（因为不存在"单一实现"可供比对）：

### D1. 两套不同架构并行（不是"两份拷贝"）
- **Cluster A（lib，`include/cpu/pipeline/`）**：集成式 `Rv32iPipeline` —— 内部 `ch_mem<ch_uint<32>,32> reg_file`、
  ALU 内联 `ExStage`、5 stage 组件、`hazard_unit` 前推**被禁用**。
- **Cluster B（example，`examples/riscv-mini/src/`）**：组件库式 —— `Rv32iAlu` / `Rv32iRegFile` /
  `riscv::ForwardingUnit` / `BranchDetector` / `BranchConditionUnit` / `BranchPredictor` /
  `ControlHazardUnit` / `HazardDetectionUnit`（在 `rv32i_core.h`）/ `rv32i_pc`。这些组件**有真实消费者**
  （根 `tests/test_rv32i_pipeline.cpp` 12 TEST_CASE + `tests/test_rv32i_pipeline_behavior.cpp` 13 dual pairs +
  `rv32i_phase{1,2,3}_test.cpp`）。
- 结论：**"收敛"是架构合并**（保留两侧各自的正确部分），不是机械删除。

### D2. 同名头靠 `-I` 顺序消歧（无护栏）
10 组同名头文件，全部已漂移：

| basename | example LOC | lib LOC | 备注 |
|---|---|---|---|
| `rv32i_pipeline.h` | 207（**死**） | 247 | 318 行 diff；example 版零消费者 |
| `stages/id_stage.h` | 238（**死**） | 237 | lib 版加 `pc`/`reg_rs1_data`/`reg_rs2_data` + `valid_out`；**opcode 4→7 bit 修正** |
| `stages/ex_stage.h` | 235（**死**） | 232 | 63 行 diff |
| `hazard_unit.h` | 188（**活**） | 178（活） | **语义分叉**：example 启用前推 / lib 禁用 |
| `stages/mem_stage.h` | 217（**死**） | 227 | 14 行 diff |
| `stages/if_stage.h` | 171（**死**） | 171 | 6 行 diff（`\|\|`→`\|` ADR-036 迁移） |
| `rv32i_tcm.h` | 134（**活**） | 133（活） | 5 行 diff |
| `rv32i_decoder.h` | 161（**活**） | 161（活） | 4 行 diff（`using namespace` 位置） |
| `rv32i_pipeline_regs.h` | 490（**活**） | 490（活） | 4 行 diff |
| `stages/wb_stage.h` | 127（**死**） | 127 | 4 行 diff |

消歧仅靠 `examples/riscv-mini/CMakeLists.txt:19-23` 的注释（`include/cpu/pipeline` 必须在 `src` 之前），
**无自动化护栏** ⇒ 调序即静默换实现。

### D3. 17 个从未被任何 TU 编译的头（3,663 LOC）
用 `build/**/*.o.d`（编译器生成的真实依赖表）反查：

```
branch_predictor_bundle.h(133) / d_tcm.h(329) / firmware_loader.h(279) / hazard_unit_bundle.h(194) /
i_tcm.h(255) / rv32i_axi_interface.h(171) / rv32i_control.h(142) / rv32i_core_simple.h(324) /
rv32i_isa.h(243) / rv32i_pipeline.h(207) / rv32i_regs.h(160) / rv32i_system.h(238) /
stages/{ex,id,if,mem,wb}_stage.h(988)
```

其中 8 个是 `ba04fdb`(2026-06-15)「move 8 unused CPU headers to examples/」从 `include/cpu/` **移出**的
⇒ 对它们「提升回 include/cpu/」是**反向操作**，需逐个裁决「删」还是「回归」。

### D4. 覆盖假象：19 个 target 编译但从不执行
`grep -rn "add_test" examples/` = 0 ⇒ `examples/riscv-mini/CMakeLists.txt` 的 19 个 Catch2 target
**每次 CI 都编译、却从未被执行**。2026-10-23 手工运行实测：

```
exit=139 (SIGSEGV): test_pipeline_minimal / test_pipeline_debug / test_pipeline_debug4 /
                    test_pipeline_debug5 / test_rv32i_pipeline_device / test_address_decoder
exit=44 (44/49 断言失败): test_riscv_tests
exit=0: 其余 12 个
```

`test_pipeline_minimal.cpp:9-13` 的 `ch::ch_module<Rv32iPipeline>` 直接放在 TEST_CASE 顶层
⇒ SIGSEGV，正是根 `AGENTS.md §COMPONENT LIFECYCLE PATTERNS` 警告的反模式。
另有 7 个 orphan 测试源文件未注册到任何 target。

### D5. 层级倒置
根 `tests/CMakeLists.txt:54,56,66,381,554,562` 共 6 处 `target_include_directories(... examples/riscv-mini/src)`；
`tests/test_rv32i_pipeline.cpp:25-28` + `tests/test_rv32i_pipeline_behavior.cpp:55-59` 直接
`#include "../examples/riscv-mini/src/..."` ⇒ **库测试依赖 example 源码**。

适用场景：收敛 riscv-mini 的 RV32I 实现、消除技术债、为 e2e parity 建立单一实现基线；
不适用场景：JIT 支持 mem read port（见 `jit-mem-read-port-support`，独立 objective）。

## 2. 目标愿景 + 完成判据

- `include/cpu/` 是 RV32I 微架构的**唯一权威**；`examples/riscv-mini/` 只剩 SoC 顶层装配 + example 专属件
- 无任何同 basename 头文件跨两个目录；无任何根 tests/ → example 的反向 include
- 无从未编译的头文件；无「编译但从不执行」的测试 target
- 完成判据（done-when）：
  - [x] C1 `riscv-mini-test-triage-2026-10` archived（19 target 清零 / 7 orphan 定性 / CTEST_TOTAL 同步）— 2026-10-24
  - [x] ADR-046 `rv32i-canonical-home` 落盘（2026-10-23）
  - [x] C2 `rv32i-canonical-convergence-2026-10` archived（17 死头删 / 10 组同名裁决 / 反向 include 消除 / grep-lint 护栏）— 2026-10-25
  - [x] `bash scripts/verify_no_duplicate_rv32i_headers.sh`（新护栏）绿（含 `--self-test`）
  - [x] `include/cpu/AGENTS.md` STRUCTURE 表与磁盘一致

## 3. 架构依据

- **ADR-046** `rv32i-canonical-home` — RV32I canonical home 裁决（✅ 2026-10-23 起草落盘：`docs/adr/ADR-046-rv32i-canonical-home.md`）
- **ADR-038** — forwarding namespace orphan（`chlib::ForwardingUnit` vs `riscv::ForwardingUnit` 重复的既有裁决；C2 需衔接）
- **ADR-042** — bundle field first-class nodes（`rv32i_pipeline_regs.h` 的 struct 字段语义相关）
- **ADR-036** — 逻辑算子 deprecation（`stages/if_stage.h` 的 `||`→`|` 漂移即此迁移的分叉）
- `docs/adr/ADR-DISCUSSION-PLAN.md` — 既有讨论
- 根 `AGENTS.md §COMPONENT LIFECYCLE PATTERNS` — `ch_module` 必须置于 `describe()` 内（D4 SIGSEGV 根因）

## 5. 反例

- **不要**把「两套实现」当成「两份拷贝」机械删除 —— Cluster B 的组件是被根 tests/ 真实消费的**活代码**，
  删错即丢覆盖（见 D1）。收敛必须先有一份**逐文件归属表**。
- **不要**把 C1（测试清零）与 C2（收敛）压进同一个 change —— 违反本仓库「一个 change 一个关注点」；
  且收敛会大改 include 路径，混在一起无法归因「我改坏了」还是「本来就红」。
- **不要**在收敛前就做 e2e parity —— 没有单一实现，parity 对象不确定。
- **不要**在 C2 里顺手改「hazard 前推语义」而不立项 —— 这是**行为变更**，需独立裁决 + 回归证据。
- **不要**把 `include/cpu/AGENTS.md` 的漂移当作纯文档问题 —— 它是 6 个幽灵文件的**虚假权威**，
  会让 builder 去 include 不存在的路径。

## 9. 目标依赖与 Decision Gate

### 9.1 前置 objective 依赖
- 无强前置。C1（测试清零）是本 objective 的第一个 change，独立可做。
- **与 `objective-rv32i-e2e` 的关系**：本 objective 是它的**前置**（e2e 依赖单一实现）。

### 9.2 Go / No-Go Decision Gate
- **Go 判据**（满足全部 4 条 → 转 completed）：
  - [x] C1 + C2 两个 change 均 archived（2026-10-24 / 2026-10-25）
  - [x] `scripts/verify_no_duplicate_rv32i_headers.sh` + `scripts/verify_example_test_wiring.sh` 双护栏绿
  - [x] `ctest -LE perf` **189/189** 零回归 + `CTEST_TOTAL` 同步（**200**）
  - [x] `include/cpu/AGENTS.md` STRUCTURE 与磁盘一致
- **No-Go 判据**（任一成立 → defer）：
  - `hazard_unit.h` 前推语义冲突无法在不改行为的前提下裁决（需升级为独立 ADR + change）
  - Cluster B 组件的提升需要重写 `Rv32iPipeline` 装配（超出 scope）
  - 收敛导致 verilator e2e（`test_verilator_complex_e2e`）无法保持绿

### 9.3 跨 objective 影响
- 受此 objective 影响的下游：`objective-rv32i-e2e`（依赖单一实现）
- 受上游 objective 影响：无

## 10. next_sprint_candidates

- **sprint-2026-10**：
  1. `riscv-mini-test-triage-2026-10`（C1，P0，无依赖，可立即启动）
  2. ~~ADR-046 起草（rdd-arch；C2 前置）~~ ✅ 2026-10-23 完成
- **sprint-2026-11+**：
  3. `rv32i-canonical-convergence-2026-10`（C2，依赖 C1 + ADR-046）
  4. `rv32i-soc-dual-parity-2026-10`（C3，归 `objective-rv32i-e2e`，依赖 C2）

## 11. 跟踪台账（append-only）
| Sprint | kind | 内容 | Decision/调整 | 原因 |
|--------|------|------|---------------|------|
| sprint-2026-10 | sprint-review | objective 创建（roadmap 重梳理） | 激活为 Phase 5 旗舰目标之一 | 2026-10-23 审计发现 riscv-mini 四类结构性技术债，收敛是 e2e parity 的前置 |
