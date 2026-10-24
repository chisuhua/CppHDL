# 提案：RV32I 单源化收敛（`include/cpu/` 为唯一权威）

## Why

`include/cpu/`（20 头）与 `examples/riscv-mini/src/`（32 头）**不是同一实现的两份拷贝，
而是两套并行架构的 RV32I**，同名头靠 `-I` 顺序消歧，**无任何自动化护栏**。
后果有三：① 调 `-I` 顺序即**静默换实现**；② 根 `tests/` 反向依赖 `examples/` 源码（层级倒置）；
③ 下游 e2e 双后端 parity 没有确定的 DUT 对象。

本 change 执行 ADR-046 D1–D12：把 RV32I 微架构收敛到 `include/cpu/` 唯一权威，
example 侧只留 SoC 顶层，并落地单源化护栏。

> 强前置：C1 `riscv-mini-test-triage-2026-10`（已归档）—— 提供绿色回归基线。

## 实测证据（2026-10-24，编译器 `.d` 依赖文件为真相源，非推测）

### E1 同名双副本 10 组，其中 **4 组两个副本都活着**

| basename | lib 版 target 数 | example 版 target 数 | 实际消歧结果 |
|---|---|---|---|
| `rv32i_decoder.h` | 6 | 8 | **两个都活** |
| `rv32i_pipeline_regs.h` | 6 | 4 | **两个都活** |
| `rv32i_tcm.h` | 9 | 3 | **两个都活** |
| `hazard_unit.h` | 7 | 3 | **两个都活**（且**语义不同**，见 E3） |
| `stages/{ex,id,if,mem,wb}_stage.h` | 各 6–8 | 0 | example 副本死 |
| `rv32i_pipeline.h` | 6 | 0 | example 副本死 |

⇒ 前 3 组的差异**仅**是 `using namespace ch::core;` 与 `namespace riscv {` 的**先后位置**
（语义等价，但 lib 版**收窄**了名字可见性 ⇒ 重指需编译验证）。

### E2 `examples/riscv-mini/src/` 32 头 = **14 活 / 18 死**

18 个死头 = ADR-046 §1.4 列的 17 个 + `rv32i_pipeline_top.h`
（其唯一消费者 `test_pipeline_smoke` 在 C1 被删除 ⇒ 变成孤儿头；
它是 **C3 的预定 DUT**，本 change **保留**，见 D3 #15）。

### E3 `hazard_unit.h` 的语义分叉是**结构性**的，不是实现质量问题 —— ADR-046 D3 #14 与 D5 实测互斥

两版**端口列表完全相同**，差异只在数据 mux：

```cpp
// lib 版：直通（注释称「已禁用：会导致 IR 图循环依赖」）
io().rs1_data = io().rs1_data_raw;

// example 版：mux
rs1_from_ex  = select(forward_a == 1, ex_alu_result, rs1_data_raw);
rs1_from_mem = select(forward_a == 2, mem_alu_result, rs1_from_ex);
io().rs1_data = rs1_from_mem;
```

**实验 A（把 mux 搬进 lib 版）⇒ 5 个 target 直接红**，同一根因：

```
FATAL: Cycle detected in IR graph during topological sort
       (node type: proxy, name: top.ex_stage._lt_lt)
```

⇒ 「禁用前推」**不是过度保守，而是必需的**：`hazard_unit` 一旦接入真实流水线装配，
`hazard.rs1_data → ex_alu_result → ex_stage → id_stage → reg_file` 形成组合环。

⇒ **方案 A（前推提升到 lib）被实测否决**。
⇒ 而 D3 #14 要求「example 副本删、消费者重指 lib」⇒ 对 3 个 example 消费者
（`test_pipeline_exec` / `test_rv32i_pipeline_example` / `test_rv32i_pipeline_behavior`）
就是**功能回退** —— 且它们的测试**确实断言了前推后的数据**
（`test_pipeline_exec.cpp:375` `REQUIRE(rs1_data == 42)`、
`examples/riscv-mini/tests/test_rv32i_pipeline.cpp:166` `REQUIRE(rs1_data == 42); // 前推后的数据`）。

⇒ **本 change 的裁决（D14）**：把数据 mux 从 `HazardUnit` 拆到**已有的**
`chlib::ForwardingMux`（`include/chlib/forwarding.h:184`）——
它的 mux 语义与 example 版**逐档等价**（0→REG / 1→EX / 2→MEM）：

| `forward_a/b` | `chlib::ForwardingMux` | example 版 `hazard_unit` mux |
|---|---|---|
| 0 | `rs1_data_reg` | `rs1_data_raw` |
| 1 | `ex_mem_alu_result` | `ex_alu_result` |
| 2 | `mem_wb_result` | `mem_alu_result` |

⇒ `HazardUnit` 收敛为**唯一一份**（lib 语义，流水线装配无环、7 个 lib 消费者**零改动**）；
3 个 example 消费者改装配为「`HazardUnit`（检测）+ `chlib::ForwardingMux`（数据 mux）」两级
⇒ **断言语义不变、覆盖不丢、零新增组件**。

⚠️ 这**不裁决** D5 悬置的问题（「lib 流水线该不该接上数据前推」）——
流水线侧**仍然不接**（`rv32i_pipeline.h` 仍不消费 `hazard.io().rs1_data`），
C2 只把**已存在**的 mux 能力从 `HazardUnit` 挪到已有组件。

### E4 反向依赖：6 处 `-I` + 7 个根测试 TU 直接 include example 源码

`tests/CMakeLists.txt` 的 `:54` `:56` `:66` `:381` `:554` `:562` 六处
`target_include_directories(... examples/riscv-mini/src)`；另有
`tests/{test_rv32i_pipeline,test_rv32i_pipeline_behavior,test_blt_debug}.cpp`、
`tests/chlib/{test_branch_predictor_v2,test_hazard_complete,test_phase4_integration}.cpp`
直接 include example 路径 ⇒ **库依赖 example**，层级倒置。

### E5 `include/cpu/AGENTS.md` 是虚假权威

STRUCTURE 表列出**磁盘上不存在**的文件（`riscv/rv32i_control.h` / `riscv/rv32i_system.h` /
`riscv/firmware_loader.h` / `riscv/rv32i_soc.h` / `cache/i_cache_prefetch.h` /
`cache/d_cache_write_buffer.h`），且自述「22 files, ~5,600 lines」而实测 20 头。

## What Changes

### 1. 删死头（17 个，全部在 `examples/riscv-mini/src/`）

`branch_predictor_bundle.h` / `d_tcm.h` / `firmware_loader.h` / `hazard_unit_bundle.h` /
`i_tcm.h` / `rv32i_axi_interface.h` / `rv32i_control.h` / `rv32i_core_simple.h` /
`rv32i_isa.h` / `rv32i_pipeline.h` / `rv32i_regs.h` / `rv32i_system.h` /
`stages/{ex,id,if,mem,wb}_stage.h`

⚠️ **不**「提升回 `include/cpu/`」—— `ba04fdb` 正因它们未被使用才移出（ADR-046 D4）。

### 2. 提升到 `include/cpu/`（10 个，ADR-046 D3 #1–#10）

| 源（`examples/riscv-mini/src/`） | 去向 | 类别 |
|---|---|---|
| `rv32i_alu.h` | `include/cpu/pipeline/rv32i_alu.h` | 数据通路 |
| `rv32i_regfile.h` | `include/cpu/pipeline/rv32i_regfile.h` | 数据通路 |
| `rv32i_pc.h` | `include/cpu/pipeline/rv32i_pc.h` | 数据通路 |
| `rv32i_forwarding.h` | `include/cpu/pipeline/rv32i_forwarding.h` | 数据通路 |
| `rv32i_branch_predict.h` | `include/cpu/pipeline/rv32i_branch_predict.h` | 数据通路 |
| `rv32i_core.h` | `include/cpu/pipeline/hazard_detection_unit.h`（**改名**，D8） | 数据通路 |
| `rv32i_hazard_complete.h` | `include/cpu/pipeline/rv32i_hazard_complete.h` | 数据通路 |
| `branch_predictor_v2.h` | `include/cpu/pipeline/branch_predictor_v2.h` | 数据通路 |
| `i_cache_prefetch.h` | `include/cpu/cache/i_cache_prefetch.h` | cache |
| `d_cache_write_buffer.h` | `include/cpu/cache/d_cache_write_buffer.h` | cache |

⇒ 迁移的头**保持 `namespace riscv`**（ADR-046 D8，不因搬家改 namespace）。

### 3. 双副本收敛为 lib 权威（4 个，ADR-046 D3 #11–#14）

`rv32i_decoder.h` / `rv32i_pipeline_regs.h` / `rv32i_tcm.h` ⇒ example 副本删，消费者重指 lib。
`hazard_unit.h` ⇒ 按 E3 的裁决（拆 mux 到 `chlib::ForwardingMux`）。

### 4. 消除反向依赖（ADR-046 D7）

- 删根 `tests/CMakeLists.txt` 的 6 处 `target_include_directories(... examples/riscv-mini/src)`
- 改写 6 个根测试 TU 的 `#include`（见 E4）
- example 侧 `CMakeLists.txt:19-23` 的「include 顺序敏感」注释**作废**（同名头不再存在）

### 5. 新护栏 `scripts/verify_no_duplicate_rv32i_headers.sh`（ADR-046 D11）

| 检查 | 内容 |
|---|---|
| C1 | `include/cpu/**` 与 `examples/riscv-mini/src/**` **同 basename 头文件数 ≤ 1** |
| C2 | 根 `tests/CMakeLists.txt` 无 `examples/riscv-mini/src` 的 `-I` |
| C3 | 根 `tests/**` 的 TU 不 `#include` `examples/**` 路径 |
| C4 | **零消费者头** ≤ 允许清单（带**期望条数**，沿用 `verify_example_test_wiring.sh` 的 allowlist 模式） |

⚠️ C4 的允许清单**当前只有 1 条**（`rv32i_pipeline_top.h`，C3 的预定 DUT，零消费者已登记在
root `AGENTS.md` 的 KNOWN LIMITATIONS）⇒ C3 落地时**必须**移除该条
（清单项一旦不再成立 ⇒ 护栏 MUST 失败，不得静默残留）。

### 6. 文档同步（ADR-046 D9）

`include/cpu/AGENTS.md`（STRUCTURE / NAMESPACE MAP / 自述计数）、root `AGENTS.md`、
`examples/riscv-mini/AGENTS.md`、ADR-046 §5 回填 `executed` + D3 #14 的落地形态修订。

## Acceptance

- [ ] `cmake --build build -j$(nproc)` 0 error 0 warning
- [ ] `ctest -LE perf` 全绿（2026-10-24 基线 **188/188**，C2 不得引入回归）
- [ ] `ctest -L verilator_e2e` 全绿（基线 34）
- [ ] `bash scripts/verify_no_duplicate_rv32i_headers.sh` 绿（4 条检查）
- [ ] `find examples/riscv-mini/src -name '*.h' | wc -l` = **1**（只剩 SoC 顶层）
- [ ] `find examples/riscv-mini/src -name '*.h'` 与 `git ls-files` 一致（无未跟踪新增）
- [ ] `include/cpu/AGENTS.md` 列出的每个 `.h` 都在磁盘上存在（无幽灵条目）
- [ ] 5 个既有护栏脚本 + `run_all_ported_tests.sh` + `run_all_samples.sh` 全绿
- [ ] `openspec validate` + `scripts/verify_openspec_delta.sh` 绿

## Capabilities

| Capability | 影响 |
|---|---|
| （新）`rv32i-canonical-home` | 单源化 + 归属表 + 护栏 + 前推 mux 的归属裁决 |
| `example-runtime-coverage` | MODIFIED：example 侧测试的 include 路径变更 |

## Impact

### 风险

| 风险 | 等级 | 缓解 |
|---|---|---|
| `rv32i_decoder.h` 等 3 组「`using namespace` 位置差」重指后**名字可见性收窄** ⇒ 根测试编译失败 | 🟡 中 | 逐 target 编译验证；必要时补 `using namespace ch::core;` |
| `hazard_unit.h` 收敛时 mux 拆解改坏 3 个 example 消费者的断言 | 🟡 中 | 用 `chlib::ForwardingMux` 的**等价** mux；断言语义不变（值不变、端口名变） |
| 提升的头改了 include 路径 ⇒ 漏改 | 🟢 低 | 编译器 `.d` 为真相源，逐 TU 复核 |
| 6 处 `-I` 移除后某些 target 编译失败 | 🟢 低 | 逐 target 编译验证 |

### Non-Goals（明确不做）

- **不裁决** D5 悬置的「lib 流水线该不该接上数据前推」⇒ 流水线侧仍不接
- 不改 `Rv32iPipeline` 内部装配（除 include 路径）
- 不做 e2e parity（那是 C3 `rv32i-soc-dual-parity-2026-10`）
- 不删 `rv32i_pipeline_top.h`（C3 的 DUT）
- 不改 `Rv32iPipelineTop` 的**端口列表**（ADR-046 D10 冻结）
- 不合并 `chlib::ForwardingUnit` 与 `riscv::ForwardingUnit`（ADR-038 §2.3 已留）
- 不整治 `include/cpu/cache/*` 的 `namespace chlib`（ADR-046 D8 已知不一致）
