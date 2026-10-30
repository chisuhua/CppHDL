# ADR-046: RV32I 的权威单一归属地 —— `include/cpu/` 为唯一微架构权威，example 只留 SoC 顶层

- Status: Accepted（proposed 2026-10-23）
- Date: 2026-10-23
- Author: build-agent（architect role）
- Parent: ADR-038（forwarding namespace orphan）、ADR-042（bundle 字段一等节点）、ADR-036（逻辑算子 deprecation）、ADR-039 / ADR-041（cache 重写与 Sync/Async 拆分）
- 关联 OpenSpec change：`rv32i-canonical-convergence-2026-10`（C2，实施本 ADR）；强前置 `riscv-mini-test-triage-2026-10`（C1）
- 关联 objective：`objective-riscv-mini-convergence`（本 ADR 是其架构裁决）；下游 `objective-rv32i-e2e`（依赖单源化）
- Amends / executes：ADR-038 §5 的 `Future Work` 第 2 条（把 `examples/riscv-mini/src/{hazard,branch}_unit*.h` 移入共享 `include/cpu/` 命名空间）

## 1. Context

### 1.1 触发

2026-10-23 审计（证据见 `.rddf/roadmap/objectives/objective-riscv-mini-convergence.md` §1）确认：
`include/cpu/`（20 个头，3,642 LOC）与 `examples/riscv-mini/src/`（32 个头，6,571 LOC）**不是
同一实现的两份拷贝，而是两套不同架构的并行 RV32I 实现**，且互相遮蔽：

| | Cluster A（lib，`include/cpu/`） | Cluster B（example，`examples/riscv-mini/src/`） |
|---|---|---|
| 形态 | 集成式 `Rv32iPipeline`：内部 `ch_mem<ch_uint<32>,32>` 寄存器堆 + ALU 内联在 `ExStage` + 5 个 stage 组件 | 组件库式：`Rv32iAlu` / `Rv32iRegFile` / `riscv::ForwardingUnit` / `BranchDetector` / `BranchConditionUnit` / `BranchPredictor` / `ControlHazardUnit` / `HazardDetectionUnit` / `rv32i_pc` |
| 前推 | `hazard_unit.h` **前推被禁用**（注释「会导致 IR 图循环依赖」） | `hazard_unit.h` **启用前推** |
| 消费者 | `rv32i_pipeline_top.h` / `tests/test_verilator_complex_e2e.cpp` / example 内测试 | **根 `tests/`**（`test_rv32i_pipeline.cpp` 12 TEST_CASE + `test_rv32i_pipeline_behavior.cpp` 13 dual pairs）+ `rv32i_phase{1,2,3}_test.cpp` |

⇒ 「收敛」是**架构合并**（保留两侧各自的正确部分），不是机械删除。

### 1.2 消歧机制是 `-I` 顺序，且无护栏

10 组同 basename 头文件跨两个目录存在，全部已漂移：

| basename | example LOC | lib LOC | diff 行 | example 副本状态 |
|---|---|---|---|---|
| `rv32i_pipeline.h` | 207 | 247 | 318 | **死**（0 includer） |
| `stages/id_stage.h` | 238 | 237 | 75 | **死**（仅被死的 example `rv32i_pipeline.h` 引用） |
| `stages/ex_stage.h` | 235 | 232 | 63 | **死**（同上） |
| `hazard_unit.h` | 188 | 178 | 22 | **活** —— **语义分叉**（前推启用 vs 禁用） |
| `stages/mem_stage.h` | 217 | 227 | 14 | **死**（同上） |
| `stages/if_stage.h` | 171 | 171 | 6 | **死**（同上，`\|\|`→`\|` ADR-036 迁移的分叉） |
| `rv32i_tcm.h` | 134 | 133 | 5 | **活**（4 个 example 测试引用 example 副本） |
| `rv32i_decoder.h` | 161 | 161 | 4 | **活**（`using namespace` 位置差异） |
| `rv32i_pipeline_regs.h` | 490 | 490 | 4 | **活** |
| `stages/wb_stage.h` | 127 | 127 | 4 | **死**（同上） |

消歧仅靠 `examples/riscv-mini/CMakeLists.txt:19-23` 的**注释**维护
（`include/cpu/pipeline` 必须排在 `src` 之前，否则触发 struct 重定义），
**无任何自动化护栏** ⇒ 调序即**静默换实现**。

### 1.3 反向依赖：库测试依赖 example 源码

根 `tests/CMakeLists.txt` 共 **6** 处 `-I` 指向 example 源码
（`:54` / `:56` / `:66` / `:381` / `:554` / `:562`），且以下 7 个 example 头被**根 `tests/` 直接 include**：

```
tests/test_rv32i_pipeline.cpp           → rv32i_pipeline_regs.h / rv32i_forwarding.h /
                                          rv32i_branch_predict.h / rv32i_core.h
tests/test_rv32i_pipeline_behavior.cpp  → rv32i_branch_predict.h / rv32i_forwarding.h /
                                          rv32i_core.h / rv32i_pipeline_regs.h / hazard_unit.h
tests/test_blt_debug.cpp                → rv32i_branch_predict.h
tests/chlib/test_branch_predictor_v2.cpp→ branch_predictor_v2.h
tests/chlib/test_hazard_complete.cpp    → rv32i_hazard_complete.h
tests/chlib/test_phase4_integration.cpp → rv32i_hazard_complete.h（经 -I 解析）
```

⇒ **库（`tests/`）依赖 example（`examples/`）**，层级倒置。

#### 1.3.1 第三种形态：**库 target 直接编译 example 的源文件**（2026-10-30 补登记）

⚠️ 本节记录的是 §1.3 **未覆盖**的**第三种**反向依赖形态（前两种 = 反向 `-I`
与反向 `#include`）：

```cmake
# 根 tests/CMakeLists.txt（**收口前**）
add_catch_test(test_csr_unit ${PROJECT_SOURCE_DIR}/examples/riscv-mini/tests/test_csr_unit.cpp)
```

⇒ ⇒ **库**的测试 target 的编译闭包**包含 example 目录里的 `.cpp`** ⇒ ⇒
依赖方向倒置 ⇒ ⇒ 且它**不被** §1.3 的两条判据（前两种形态）覆盖 ⇒ ⇒
**零阻力复发**。

⇒ ✅ **已收口**（2026-10-30）：归属改到**源文件所在的一侧** ——
`examples/riscv-mini/CMakeLists.txt` 的 `riscv_mini_catch_test(test_csr_unit
tests/test_csr_unit.cpp)` ⇒ ⇒ **CTest 条目名不变**（`test_csr_unit`）
⇒ ⇒ **计数锚点不变**（`CTEST_TOTAL` 205 / non-perf 194 均未变）。

⇒ ⇒ **护栏**：新增 **C7**（`scripts/verify_no_duplicate_rv32i_headers.sh`）
—— 根 `tests/CMakeLists.txt` MUST NOT 编译 `examples/` 的源文件 ⇒ ⇒
判定覆盖**单行**与**跨行**（括号配平拼接）两种形态 ⇒ ⇒ 且**先剥注释再拼接**
⇒ ⇒ 说明性注释里的 `examples/` **不**误报 ⇒ ⇒ 3 个 self-test fixture 钉住。

⚠️ **判据的已知边界**（如实登记）：`#` 在**引号内**时是字面量 ⇒ 剥注释会误伤
⇒ ⇒ 本仓 CMake 无「引号内 `#`」⇒ ⇒ 零实际影响。

### 1.4 死头文件（源码级反查，非 build 目录反查）

2026-10-23 用「源码级 `#include` 反查」判定（见 §1.6 的方法学说明），**17 个头文件零消费者**：

- **零 incluer（12 个）**：`branch_predictor_bundle.h`(133) / `d_tcm.h`(329) /
  `firmware_loader.h`(279) / `hazard_unit_bundle.h`(194) / `i_tcm.h`(255) /
  `rv32i_axi_interface.h`(171) / `rv32i_control.h`(142) / `rv32i_core_simple.h`(324) /
  `rv32i_isa.h`(243) / `rv32i_pipeline.h`(207) / `rv32i_regs.h`(160) /
  `rv32i_system.h`(238)
  （⚠️ `rv32i_pipeline.h` 指 **example 副本** —— 它的 includer 全部指向 lib 副本
  `cpu/pipeline/rv32i_pipeline.h`，故 example 副本零消费者；它同时是下面
  5 个 stage 头的唯一 includer）
- **传递性死（5 个）**：其唯一 includer 是同样死掉的 example `rv32i_pipeline.h` ⇒
  `stages/{ex,id,if,mem,wb}_stage.h`(988)

12 + 5 = **17**（**零消费者**头）。另有 **4 个**「**同名双副本**」被删
（`hazard_unit.h` / `rv32i_decoder.h` / `rv32i_pipeline_regs.h` / `rv32i_tcm.h`
—— 它们**有**消费者，只是消费者用的是 lib 副本 ⇒ 不属「零消费者」）⇒ 本 change
合计删 **21** 个头（17 + 4）。其中 **8 个**是 `ba04fdb`(2026-06-15, `refactor(riscv-mini): move 8 unused CPU headers to examples/`)
从 `include/cpu/` **移出**的：`branch_predictor_bundle.h` / `d_cache_write_buffer.h` /
`firmware_loader.h` / `hazard_unit_bundle.h` / `i_cache_prefetch.h` / `rv32i_control.h` /
`rv32i_soc.h` / `rv32i_system.h` ⇒ 对它们「提升回 `include/cpu/`」是**反向操作**，须逐个裁决。

### 1.5 `include/cpu/AGENTS.md` 是**虚假权威**（6 个幽灵条目）

`include/cpu/AGENTS.md` STRUCTURE / KEY FILES 列出的以下文件**在磁盘上不存在**
（正是 `ba04fdb` 移走的那批，表格从未同步）：

`riscv/rv32i_control.h` / `riscv/rv32i_system.h` / `riscv/firmware_loader.h` /
`riscv/rv32i_soc.h` / `cache/i_cache_prefetch.h` / `cache/d_cache_write_buffer.h`

⚠️ 这不只是文档问题 —— builder 会照着它去 `#include` 一个不存在的路径。
同时该表格自称「22 files, ~5,600 lines」，实测 `include/cpu/` = 21 files（20 头 + AGENTS.md）、3,642 LOC。

### 1.6 方法学说明：为什么用源码级反查而不是 build 目录

`build/**/*.o.d`（编译器依赖表）**会撒谎**：它只反映**上一次实际构建过的 target**。
本次实测中 `rv32i_pipeline_top.h` 与 example `rv32i_tcm.h` 在 `.d` 中显示 0 引用，
而源码级反查显示它们各有 incluer（`test_pipeline_smoke.cpp` / 4 个 example 测试）
—— 因为 example 测试二进制未被增量构建。
⇒ 判定「死头文件」MUST 用**源码级 `#include` 反查**（全仓 `*.h` / `*.cpp` / `*.tpp` 的
`#include` 行），`.d` 反查只可作**辅助**证据。

### 1.7 下游动机

`objective-rv32i-e2e` 的旗舰目标（RV32I SoC 在 interpreter + verilator 双路 trace parity，
+ JIT 回落行为显式断言）需要一个**确定的 parity 对象**。当前存在两套实现 ⇒ parity 对象不确定
⇒ e2e 无法有意义地启动。故本 ADR 是 e2e 的**强前置**。

## 2. Decision

### D1. 权威归属地 = `include/cpu/`（唯一）

`include/cpu/` 是 RV32I 微架构的**唯一权威**。`examples/riscv-mini/src/` 收敛后
**只允许**存在两类文件：

1. **SoC 顶层装配** —— 把流水线 + TCM + cache 组装成 example 的 SoC 顶层
   （当前唯一实例：`rv32i_pipeline_top.h`，49 LOC）；
2. **example 专属件** —— 仅供该 example 的测试/演示使用、且不构成可复用微架构组件的文件。

任何**可复用的 RV32I 微架构组件**（ALU / 寄存器堆 / PC / decoder / 流水线寄存器 /
stage 组件 / hazard / forwarding / branch 单元 / TCM / cache 包装）
MUST 位于 `include/cpu/` 之下。

### D2. 目录细分规则

| 类别 | 归属 | 判据 |
|---|---|---|
| 流水线数据通路组件 | `include/cpu/pipeline/` | ALU、寄存器堆、PC、decoder、流水线寄存器、stage 组件、hazard / forwarding / branch 预测单元 |
| 体系结构 / 特权级组件 | `include/cpu/riscv/` | CSR、异常/陷入、地址解码、CLINT、system 指令 |
| cache 控制器与包装 | `include/cpu/cache/` | `i_cache_*` / `d_cache_*` 及在其之上的通用包装 |

### D3. 32 个 example 头的逐文件裁决

（本表为**权威归属表**，C2 实施时据此执行；任何偏离须回报 ADR-046 修订）

| # | 文件 | LOC | 裁决 | 去向 / 理由 |
|---|---|---|---|---|
| 1 | `rv32i_alu.h`（`Rv32iAlu`） | 172 | **提升** | `include/cpu/pipeline/` |
| 2 | `rv32i_regfile.h`（`Rv32iRegFile`） | 316 | **提升** | `include/cpu/pipeline/` |
| 3 | `rv32i_pc.h` | 47 | **提升** | `include/cpu/pipeline/` |
| 4 | `rv32i_forwarding.h`（`riscv::ForwardingUnit`） | 174 | **提升** | `include/cpu/pipeline/`（见 D6） |
| 5 | `rv32i_branch_predict.h`（`BranchDetector` / `BranchConditionUnit` / `BranchPredictor` / `ControlHazardUnit`） | 475 | **提升** | `include/cpu/pipeline/` |
| 6 | `rv32i_core.h`（`HazardDetectionUnit`） | 65 | **提升 + 改名** | `include/cpu/pipeline/hazard_detection_unit.h`（类名与文件名不符；见 D8） |
| 7 | `rv32i_hazard_complete.h`（`HazardUnitComplete`） | 195 | **提升** | `include/cpu/pipeline/` |
| 8 | `branch_predictor_v2.h`（`BranchPredictorV2`） | 151 | **提升** | `include/cpu/pipeline/` |
| 9 | `i_cache_prefetch.h`（`chlib::ICachePrefetch`） | 145 | **提升** | `include/cpu/cache/`（AGENTS.md 已如此声称；通用 cache 包装，非 SoC 装配） |
| 10 | `d_cache_write_buffer.h`（`chlib::DCacheWriteBuffer`） | 146 | **提升** | `include/cpu/cache/`（同上） |
| 11 | `rv32i_decoder.h` | 161 | **lib 为权威** | example 副本删；消费者重指 `include/cpu/pipeline/` |
| 12 | `rv32i_pipeline_regs.h` | 490 | **lib 为权威** | 同上 |
| 13 | `rv32i_tcm.h` | 134 | **lib 为权威** | 同上（4 个 example 测试的 include 路径需重指） |
| 14 | `hazard_unit.h` | 188 | **lib 为权威（仅结构）** | example 副本删；**前推语义不在本 ADR 裁决**（见 D5） |
| 15 | `rv32i_pipeline_top.h` | 49 | **保留 example** | SoC 顶层装配（D1 类别 1） |
| 16–32 | §1.4 的 17 个死头 | 3,663 | **删除** | 见 D4 |

> ⚠️ `rv32i_core_simple.h`(324) 与 `rv32i_regs.h`(160) 看似「另一份寄存器堆实现」，
> 但**零消费者** ⇒ 按 D4 删除，**不**提升、**不**合并（保留它们等于同时维护第三、第四份实现）。

### D4. 死头文件删除，**不**「回归 `include/cpu/`」

零消费者的头文件（§1.4 的 17 个）MUST `git rm`。
**禁止**把它们「提升回 `include/cpu/`」—— `ba04fdb` 正是因为它们**未被使用**才移出的；
移回去会立刻重建同一份技术债。

例外须同时满足：① 有一个**具名的** change 声明将消费它；② 该 change 已 approve。
否则一律删除（可随时从 git 历史取回）。

### D5. `hazard_unit.h` 的前推语义分叉**不在本 ADR 裁决**

lib 版前推被禁用（注释称「会导致 IR 图循环依赖」），example 版启用前推
⇒ 两版**行为不同**，是**行为变更**而非结构问题。本 ADR 只裁决**结构**：
`include/cpu/pipeline/hazard_unit.h` 是唯一文件。

前推语义（启用 / 禁用 / 重构为真正的转发通路）MUST 单独立项 —— 需独立 ADR +
回归证据（证明「启用前推不会产生 IR 图循环」或「确实会，需换实现」）。
**C2 MUST NOT 在收敛过程中顺手改前推语义。**

### D6. 与 ADR-038 的接口：两个 `ForwardingUnit` 并存，namespace 区分

| 组件 | 位置 | namespace | 处置 |
|---|---|---|---|
| `chlib::ForwardingUnit` / `ForwardingMux` | `include/chlib/forwarding.h` | `chlib` | 保持（ADR-038 §2.1 已移入 `include/chlib/`） |
| `riscv::ForwardingUnit` | 提升至 `include/cpu/pipeline/rv32i_forwarding.h` | `riscv` | 本 ADR D3 #4 |

两者**并存**，靠 namespace 区分（ADR-038 §2.3 已裁决「合并留给未来 change」）。
本 ADR **重申** ADR-038 §2.3：合并需要重新设计其中一方，本 ADR 不做。
⇒ 修正 ADR-038 §2.2 的表述：example 下的组件**不再**是「无 canonical home」，
而是**有 canonical home（`include/cpu/`）但尚未迁移**。

### D7. 消除 `-I` 顺序消歧

1. 根 `tests/CMakeLists.txt` MUST NOT 出现 `-I <...>/examples/riscv-mini/src`；
2. 根 `tests/**` 的任何 TU MUST NOT `#include` `examples/**` 路径；
3. example 内的 CMake **MUST NOT** 依赖 `-I` 顺序来在两个同名头之间消歧
   —— 审计发现 example 侧的 CMake 注释（`CMakeLists.txt:19-23`）正是这种依赖；
   收敛后同名头不再存在（D3 + D4），该注释随之作废。

### D8. 改名与 namespace：遵循 ADR-029 / ADR-038 的既有权衡

- 「类名与文件名不符」的 `rv32i_core.h`（只含 `HazardDetectionUnit`）MUST 改名为
  `hazard_detection_unit.h`（D3 #6）。
- 迁移的头文件**保持 `namespace riscv`**（不因搬家而改 namespace）。
- ⚠️ 已知不一致（**本 ADR 不修**）：`include/cpu/cache/*` 使用 `namespace chlib`，
  与 ADR-038 §2.1「`namespace chlib` 意味着文件在 `include/chlib/`」的规则冲突。
  D3 #9 / #10 的归属 `include/cpu/cache/` 与该目录既有约定一致 ⇒ 保持；namespace
  整体整治单独立项。

### D9. `include/cpu/AGENTS.md` 是**派生视图**，磁盘是权威

STRUCTURE / KEY FILES 表 MUST 与磁盘一致。§1.5 的 6 个幽灵条目 MUST 删除；
「22 files, ~5,600 lines」的自述 MUST 修正为实测值。
C2 MUST 把「AGENTS.md 与磁盘一致」纳入验收（防止再次漂移）。

### D10. 冻结 SoC 顶层接口（为 C3 服务）

`Rv32iPipelineTop` 的**端口列表**在 C2 冻结 —— C2 允许改其**内部装配**，但 MUST NOT
改端口。理由：C3（`rv32i-soc-dual-parity-2026-10`）的 DUT 就是它，C2 改端口会让
C3 的观测点全部重做。

### D10-rev. 端口契约的**观测面**修订（2026-10-25，change `rv32i-pipeline-commit-path-2026-10`）

D10 原文冻结的是「**C2** 不得改端口」（理由：「C3 的观测点会被重做」——
即观测点由 C3 定义）。本 change 是 C3 的**前置**，实测证明**原端口集合无法观测**
（6 个端口全是 `ch_in`，无固件加载、无输出）⇒ 必须扩端口。

**D10-rev 裁决**：

1. `Rv32iPipelineTop` 的端口集合**由 C2 的「冻结」改为「由 C3 重新定义并冻结」**：
   **32 个端口**（5 输入 + 27 输出 = 11 功能 `obs_*` + 16 诊断 `dbg_*`）。
   ⇒ 诊断面是必要的：`VerilatorBackend` 只对 `port_access_` 里的**顶层**端口
   建访问表 ⇒ 内部信号必须一路引到顶层，否则无法在 verilator 下观测。
2. 新端口集合由护栏 C6 快照钉住（名 + 方向 + 位宽）⇒ 之后的改动仍不可静默。
3. 本次同时删掉 4 个**死端口**（`ext_instr_data` / `ext_instr_ready` /
   `ext_data_read_data` / `ext_data_ready` —— 在 `describe()` 里从未被引用）。
   ⇒ ZERO-DEBT 的「无死代码」优先于「冻结」：被冻结的接口里若有死端口，
   冻结只是把死代码永久保留。
4. **零消费者豁免随之失效**：`rv32i_pipeline_top.h` 获得了消费者
   （`examples/riscv-mini/tests/test_rv32i_soc_dual_parity.cpp`）⇒
   单源化护栏的豁免条目 MUST 移除（`rv32i-canonical-home` 主 spec 的既有条款）。

### D11. 自动化护栏

新增 `scripts/verify_no_duplicate_rv32i_headers.sh`（挂进 CI），**六条**检查
（2026-10-24 C2 实施时扩充；原三条 = 现 C1/C2/C3）：

1. **C1** `include/cpu/**` 与 `examples/riscv-mini/src/**` **同 basename 头文件数 ≤ 1**；
2. **C2** 根 `tests/CMakeLists.txt` 无 `examples/riscv-mini/src` 的 `-I`
   （**跨行续行也 MUST 抓到** —— CMake 的多行命令靠括号配平，不靠反斜杠）；
3. **C3** 任何 TU 不得 `#include` example 路径；
4. **C4** **硬死头**（两个独立证据源都判定无 includer）MUST 删除；
   ⚠️ 「源码有 includer 但无 TU 编译它」**只警告不违规**（可能是传递性死，
   也可能是 target 名抽取盲区 ⇒ 误报会诱导删掉**活头**）；
   ⚠️ **源① = basename 原文提及**（**不做** include 形态识别）：某文件里出现
   `foo.h` 这个字符串 ⟺ 它**可能** `#include` 了该头（引号 / 尖括号 / 宏
   `#include HDR` / 续行 / 仓库根相对路径 / `-I` 差异一律覆盖）⇒ **不存在
   「解析失败」状态** ⇒ 只**漏报**（注释或文档里提到死头名字即判活，方向安全）。
   6 轮审稿中前 5 轮各在上一版 ~420 行手写解析器上找到新洞 ⇒ 整段删除而非继续
   打补丁（同类教训见 `verify_example_assertion_quality.sh`）。自身提及
   （`/** @file foo.h */`）MUST **按文件归属**排除 —— 不排除则每个头恒活；
   续行按 C++ 语义**零插入拼接**、状态按文件重置、文件末尾悬空续行仍参与判定。
   两条 fail-closed 探针：扫描面**之外**的 C/C++ 扩展名 ⇒ 违规；
   源① 实际处理 TU 数 ≠ 扫描面文件数（崩溃 / 少读）⇒ 违规；
   ⚠️ **扫描面的 prune 列表 MUST 只含**「第三方 / VCS 元数据」**与**
   「**构建输出目录**」；构建输出目录 MUST 用**纯内容**判定（实测判据：含
   `CMakeCache.txt`，**与目录名无关**）⇒ MUST NOT 只按**目录名前缀** prune
   （`build*` 这类 glob
   会连带 prune 掉 `buildtools/`、`builder/`、以及本仓真实存在的
   `openspec/specs/build-cpp-standard/` 这类**非构建目录** ⇒ 那里面的
   消费者既不被扫到、也不被探针报出 ⇒ 活头会被误判成死头）；
   ⚠️ **豁免 stale 检查的方向 MUST 正确**：「已有消费者」只能由**真 include
   证据**（编译器 `.d`）**且必须是当前的**证明 —— `.d` 是**累积**构建产物
   （TU 被删 / target 被移除后不会被清掉）⇒ 「`.d` 里出现过」**不等于**
   「现在有消费者」⇒ 当前性判据 = 源文件**仍存在** + **仍 `#include` 它**
   （比对对象是 include 的**操作数**：只查「文本提及」连注释也算证据；
   只查「行首是 `#include` 的整行」则行尾注释里的提及仍被当成证据
   ⇒ 方向倒置。⚠️ **已知误报向量**（如实声明）：`#if 0` / `#ifdef`
   **非激活区**里的字面 `#include` **仍被计入证据**（预处理语义不在
   判据内；真修需嵌套 `#if`/`#else` ⇒ 否决；实际行为由 fixture ⑨h
   钉住））+ `.d` **不早于**该源文件（阈值 0：预处理
   与源文件常**同秒** ⇒ 硬阈值会必然漏判）；任一不满足 ⇒ **只警告**（不报违规）⇒ 源① 的
   「原文提及」也**只警告**且措辞不得指示删除豁免行（否则注释里提到头名即
   假违规，且错误信息会指示删豁免行 ⇒ 移除后护栏报「硬死头 MUST 删除」⇒
   会诱导删掉 D10 **冻结**的 C3 DUT）；
5. **C5** `include/cpu/AGENTS.md` 的 STRUCTURE 表 MUST 与磁盘一致（**D9** 的机械化）；
6. **C6** SoC 顶层 `Rv32iPipelineTop` 的**端口集合** MUST 与登记快照一致（**D10** 的机械化）。

⚠️ 护栏自带 `--self-test`（构造合成树 ⇒ 断言 6 条判据**各自有判别力**）——
护栏自身的回归测试（否则同类缺陷会重演）。

⚠️ 该护栏 MUST 对 `include/cpu/cache/` 与 `examples/riscv-mini/src/` 两组同名文件同样生效
（D3 #9/#10 提升后即不再同名）。

### D12. 契约先行

C2 的代码改动 MUST 与 capability delta **同批落地**（新 capability `rv32i-canonical-home`）。
「先改代码、后补契约」在 ADR-045 已被判定为 REJECT 形态。

## 3. Consequences

### 正面

- **单一实现** ⇒ `objective-rv32i-e2e` 获得确定的 parity 对象（本 ADR 的核心目的）。
- 3,663 LOC 死代码删除 + 10 组同名头消除 ⇒ 调 `-I` 顺序不再能静默换实现。
- 根 `tests/` 不再依赖 `examples/` ⇒ 层级恢复（lib → example，不是反向）。
- `include/cpu/AGENTS.md` 不再指向 6 个不存在的文件。

### 负面 / 代价

- **公开 include 路径变更**：7 个 example 头被根 `tests/` 直接 include
  ⇒ 这些 TU 的 `#include` 行 MUST 改写（含 `tests/chlib/test_phase4_integration.cpp:29`
  经 `-I` 解析的隐式依赖）。
- `rv32i_core.h` 改名（D8）⇒ 引用点更新。
- example 的 `examples/riscv-mini/CMakeLists.txt` include 顺序注释作废 ⇒
  `riscv_mini_catch_test()` 的 `-I` 列表需重审。
- `rv32i_tcm.h` 的 example 副本被 4 个 example 测试使用 ⇒ 那些测试需重指 lib 路径
  （它们**不**在 C1 的 19 target 清单里，属 example 内测试，需单独核对）。
- **前推语义分叉被显式挂起**（D5）⇒ 收敛后 `include/cpu/pipeline/hazard_unit.h`
  仍禁用前推，Cluster B 的「启用前推」能力在收敛后**暂时丢失**
  ⇒ C2 MUST 在 capability 里写明这一**行为回退**，并指向将来的独立裁决。

### 不在本 ADR 范围

- `hazard_unit.h` 前推语义裁决（D5）—— 独立 ADR + change。
  ✅ **已裁决（2026-10-27）**：`docs/adr/ADR-047-rv32i-data-forwarding.md`
  —— 裁决**接**前推，形态 = `chlib::ForwardingMux`（零新增组件）+ 档位 select
  **打一拍** + 档位 1 源含 `link_value` + 档位 2 = `wb_stage.write_data`
  + 删 5 个预留端口（含**环饵** `ex_alu_result`）+ 收敛 `ex_stage.rs1_data` 的
  **双驱动**；验收 MUST 含**判别性固件**（当前固件刻意无冒险 ⇒ 前推接错全仓照绿）。
  ⇒ 本 ADR 的 D5 **原文决策不变**（本 ADR 只裁决结构），语义由 ADR-047 承接。
  ⚠️ **2026-10-27 补充（ADR-047 修订）**：ADR-047 在起草实施 change 的缺口分析中
  追加了 **D11** —— **距离 3** 的同沿竞态（ID/EX 锁存沿 == WB 写回沿）⇒
  裁决在**寄存器堆读点**加 **WB→ID 组合旁路** ⇒ 并把**距离矩阵**定为契约的
  **唯一权威**（`rv32i-data-forwarding` capability）⇒ ⚠️ 该缺陷**今天就存在**
  （现有固件用 3 条 NOP 恰好躲开）⇒ **不接前推也得修**。
- `chlib::ForwardingUnit` 与 `riscv::ForwardingUnit` 的合并（D6）—— ADR-038 §2.3 已留。
- JIT 对 `ch_mem` 读端口的 lowering —— 独立 objective `jit-mem-read-port-support`。
- e2e parity 本身 —— C3 / `objective-rv32i-e2e`。
- `include/cpu/cache/*` 的 `namespace chlib` 整治（D8）—— 独立立项。
- 19 个「编译但从不执行」的测试 target —— C1 `riscv-mini-test-triage-2026-10`
  （**本 ADR 的强前置**：C2 会大改 include 路径，需要 C1 的绿色基线做回归护栏）。

## 4. 备选方案（否决）

| 方案 | 结论 |
|---|---|
| **B. 反向：把 lib 收敛到 example** | ❌ 否决 —— `include/cpu/` 是**库包含图**的一部分（`include/` 是 `$<BUILD_INTERFACE>`），example 是**下游**。把库移进 example 会颠倒依赖方向。 |
| **C. 保持两套，只加护栏** | ❌ 否决 —— 护栏只能防「换序静默换实现」，防不了「两套并行漂移」；且 e2e parity 仍无对象。 |
| **D. 机械删除 example 侧全部同名头** | ❌ 否决 —— Cluster B 的组件是**被根 `tests/` 真实消费的活代码**（§1.3），删掉即丢覆盖。必须逐文件归属（D3）。 |
| **E. 把 17 个死头「提升回」`include/cpu/`** | ❌ 否决 —— `ba04fdb` 正因它们未使用才移出；移回即重建技术债（D4）。 |

## 5. 实施追踪

| 项 | 状态 |
|---|---|
| ADR 裁决（本文件）落盘 | ✅ 2026-10-23 |
| 强前置 C1 `riscv-mini-test-triage-2026-10` | ✅ 2026-10-24 归档 |
| C2 `rv32i-canonical-convergence-2026-10` 实施 D1–D12 | ✅ 2026-10-24（`examples/riscv-mini/src/` 32 → **1** 个头） |
| C2 archive 后回填本表（标 `executed`） | ⏳ |
| 下游 C3 `rv32i-soc-dual-parity-2026-10` | ⏳ 依赖 C2 |

### D3 #14 的落地形态修订（2026-10-24，C2 实施时回报）

D3 #14 原文：「`hazard_unit.h` —— **lib 为权威（仅结构）**；example 副本删；**前推语义不在本 ADR 裁决**（见 D5）」。

C2 实施时实测发现 **D3 #14 与 D5 互斥**，并据此修订落地形态：

- 两版**端口列表完全相同**，差异只在数据 mux。lib 版**必须**禁用 mux：
  把 mux 搬进 lib 版 ⇒ **5 个 target 直接红**（`FATAL: Cycle detected in IR graph
  during topological sort`，`top.ex_stage._lt_lt`）。根因：`hazard_unit` 一旦接入
  真实流水线装配，`hazard.rs1_data → ex_alu_result → ex_stage → id_stage →
  reg_file → hazard` 形成**组合环**（全链路无寄存器/存储器节点）。
  `git show bf78367`（2026-04-15，"Phase F - IR cycle resolution"）证明 lib 版
  的注释「已禁用：会导致 IR 图循环依赖」是**事故记录**，不是臆测。
- ⇒ 「删 example 副本 + 消费者重指 lib」对 3 个 example 消费者是**功能回退**
  （它们确实断言「前推后的数据」）⇒ 不能照字面执行。
- ⇒ **C2 的裁决**：数据 mux 拆到**既有的** `chlib::ForwardingMux`
  （`include/chlib/forwarding.h:183-246`，档位语义与原 mux **逐档等价**：
  0→REG / 1→EX / 2→MEM）⇒ `HazardUnit` 保持**唯一一份**且**不带** mux；
  3 个测试改用组合 DUT（`examples/riscv-mini/tests/forwarding_dut_fixture.h`）。
  **零新增组件、断言值不变、覆盖不丢。**
- ⇒ **D5 悬置的问题仍未裁决**：lib 流水线**仍然不接**数据前推
  （`rv32i_pipeline.h` 仍不消费 `hazard.io().rs1_data`）。
  ⚠️ 另发现（**不在 C2 范围**，须独立 ADR）：`rv32i_pipeline.h` 的
  `hazard.io().ex_rd_addr` / `mem_rd_addr` 接的是 **ID 级当前指令自己的 rd**
  ⇒ 即使破环，前推语义也是错的（正确前推源应为 EX/MEM 级**流水线寄存器**）。
  ✅ **后续状态（2026-10-27）**：D5 已由 **ADR-047** 裁决（「接」），且上述
  「前推源应是流水线寄存器」的发现已被 ADR-047 吸收（D3/D4）；另追加 **D11**
  （**距离 3** 的读点旁路）。本条保留为 **C2 实施时点**的历史记录。

### 附带改动：perf baseline 重生成 + 守恒护栏（2026-10-25，C2 实施时回报）

C2 本身**不碰**任何 perf 路径（perf 基准是寄存器链 / 异或链，不含 `include/cpu/` 头），
但 C2 实施期发现旧 `perf_baseline.json`（2026-10-16 在 `37ab63f` 采的）已对当前机器过时
⇒ `perf_regression` 报 9–11 处「回归」（比率 1.3×–6.9×）。**对照实验**证明这不是 C2 的
性能变化：pre-C2 worktree（`5191c47`）与 C2 HEAD **背靠背**跑 `perf_tests --tc=07 --tc=08`
⇒ **HEAD/pre-C2 = 0.89–1.21（均值 ~1.03）**，而 pre-C2/baseline 同样 1.01–2.36×
⇒ 漂移是**环境性的**。⇒ 按仓库既有先例**重生成 baseline**。

⚠️ 重生成暴露出一个**此前无任何自动化检查**的盲区：
`perf_regression.cpp:179` 的判据是 `if (base <= 0.0) continue;`
⇒ baseline 里 `median_us == 0` 的行**永久退出比较集合** ⇒ 该行的未来真回归
**永久不可见**（本次采集 TC-09 全 SKIPPED ⇒ 若照抄，6 个锚点会被静默丢弃）。
⇒ **新增护栏 `scripts/verify_perf_baseline_anchors.sh`**（3 条守恒检查：
行集守恒 / 锚点守恒 / 变松披露），并把它挂在 CI 两个 job 内
（PR 比 base branch、push 比 `HEAD`）；护栏自带 `--self-test`（15 项断言）。
⚠️ 阈值 MUST 从**权威源**（`tests/benchmark/perf_regression.cpp` 的
`kBackendThresholds`）读出，且权威源不可解析时 **fail-closed**
（MUST NOT 回落到脚本内置副本 —— 回落是 fail-open：权威源被改写时护栏会
静默用旧阈值判 ⇒ 恰好复现本条要消除的漂移）。
契约落在 capability `perf-test-isolation`（本 change 的 delta）。

### D5 的补充表述（2026-10-24）

D5 说「前推语义（启用 / 禁用 / 重构为真正的转发通路）MUST 单独立项」。
C2 做的是「把**已存在**的 mux 能力从 `HazardUnit` 挪到已有组件」——
**不新增、不删除任何前推能力**，也不裁决「流水线该不该有前推」
⇒ D5 的实质**保留**，其待裁决范围**扩大**为：流水线前推的**数据源**
（应为 EX/MEM 流水线寄存器）与**接线**（`ex_rd_addr` / `mem_rd_addr` 的自我匹配缺陷）。

## 6. 参考

- `.rddf/roadmap/objectives/objective-riscv-mini-convergence.md` §1（D1–D5 实测证据）
- `.rddf/improvements/rv32i-canonical-convergence-2026-10.md`（C2 5-segment 草稿）
- `.rddf/improvements/riscv-mini-test-triage-2026-10.md`（C1 5-segment 草稿）
- `docs/adr/ADR-038-forwarding-namespace-orphan.md` §2.3 / §5（本 ADR 承接其 Future Work）
- `include/cpu/AGENTS.md` STRUCTURE / NAMESPACE MAP（D9 修正对象）
- `examples/riscv-mini/CMakeLists.txt:19-23`（`-I` 顺序消歧注释，D7 作废对象）
- `tests/CMakeLists.txt:54,56,66,381,554,562`（反向 `-I`，D7 消除对象）
- root `AGENTS.md §COMPONENT LIFECYCLE PATTERNS`（`ch_module` 必须置于 `describe()` 内 —— C1 的 SIGSEGV 根因）
- `git show ba04fdb`（8 个 CPU 头移出 `include/cpu/` 的提交）
