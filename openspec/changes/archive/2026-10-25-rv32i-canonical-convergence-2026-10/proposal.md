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
| C4 | **硬死头**（两个独立证据源都判定无 includer）≤ 允许清单（带**期望条数**，沿用 `verify_example_test_wiring.sh` 的 allowlist 模式） |
| C5 | `include/cpu/AGENTS.md` 的 STRUCTURE / KEY FILES 表与磁盘一致（ADR-046 D9）+ 自述计数一致 |
| C6 | SoC 顶层 `Rv32iPipelineTop` 的端口集合（**名 + 方向 + 位宽**）与登记快照一致（ADR-046 D10） |

⚠️ C4 的允许清单**当前只有 1 条**（`rv32i_pipeline_top.h`，C3 的预定 DUT，零消费者已登记在
root `AGENTS.md` 的 KNOWN LIMITATIONS）⇒ C3 落地时**必须**移除该条
（清单项一旦不再成立 ⇒ 护栏 MUST 失败，不得静默残留）。

⚠️ **C4 的两条方向性约束**（第六至九轮对抗审稿的 BLOCKER 逼出来的，均已写进
capability delta 与 ADR-046 D11）：
① **构建输出目录 MUST 用纯内容判定**（实测判据：含 `CMakeCache.txt`，
**与目录名无关**）⇒ **不可**用 `build*` 这类前缀 glob / 名字锚定 ——
向前它会把 `buildtools/`、`builder/`、以及本仓真实存在的
`openspec/specs/build-cpp-standard/` 这类**非构建目录**也 prune 掉 ⇒
那里的消费者既不被源① 扫到、也不被「扫描面自检」探针报出 ⇒ 活头会被
误判成硬死头 ⇒ **安全属性失效**；向后它会把 `cmake -B out` 这类合理布局
里的**生成物**（`.c` 等）报成「扫描面之外」⇒ CI 被无关布局阻断。
② **豁免 stale 检查的方向 MUST 正确**：「已有消费者」只能由**真 include 证据**
（编译器 `.d`）**且必须是当前的**证明 —— 当前性 = 源文件**仍存在** +
**仍 `#include` 它**（比对对象是 `#include` 的**操作数**，否则注释里的提及也算证据）+
`.d` **不早于该源文件**（阈值 0）；任一不满足 ⇒ **只警告**。
⚠️ **已知误报向量**（如实声明，MUST NOT 记成「漏判 ⇒ 安全」）：
`#if 0` / `#ifdef` **非激活区**里的字面 `#include` **仍被计入证据**
（预处理器语义不在判据内；真修需嵌套 `#if`/`#else` ⇒ 否决；实际行为由
fixture ⑨h 钉住 ⇒ 行为变化可见；可达性受限 —— 需 `.d` 提及该头 +
`.d` 不早于 TU + TU 只在非激活区提及它三者同时成立 ⇒ 当前仓库不可达）。
源① 的
「原文提及」同样**只警告**、不得报违规（源① 的语义是「任何原文提及 ⇒
可能 include 过」—— 它在「判死」方向保守；拿它证明「已有真实消费者」
则方向相反 ⇒ 纯注释里的提及即假违规，且错误信息会指示「删豁免行」⇒
移除后护栏报「硬死头 MUST 删除」⇒ 诱导删掉 ADR-046 D10 **冻结**的 C3 DUT）。

### 6. 文档同步（ADR-046 D9）

`include/cpu/AGENTS.md`（STRUCTURE / NAMESPACE MAP / 自述计数）、root `AGENTS.md`、
`examples/riscv-mini/AGENTS.md`、ADR-046 §5 回填 `executed` + D3 #14 的落地形态修订。

## Acceptance

- [x] `cmake --build build -j$(nproc)` 0 error 0 warning
- [x] `ctest -LE perf` 全绿（2026-10-24 基线 **188/188**，C2 不得引入回归）
      ⇒ **2026-10-25 实测 188/188**（`base` 162 / `verilator_e2e` 34 / `spinalhdl-ported` 25）
- [x] `ctest -L perf` 全绿（基线 11/11）⇒ **2026-10-25 实测 11/11**（1923 s）
      ⚠️ 本次附带重生成 `perf_baseline.json`（**非** C2 性能变化 ⇒ 见 §「附带改动」的
      pre-C2-vs-HEAD 对照实验）
- [x] `ctest -L verilator_e2e` 全绿（基线 34）⇒ **2026-10-25 实测 34/34**
- [x] `bash scripts/verify_no_duplicate_rv32i_headers.sh` 绿（**6 条检查**）
- [x] `bash scripts/verify_no_duplicate_rv32i_headers.sh --self-test` 绿
      （**44 个 fixture** ⇒ 断言 6 条判据各自有判别力；护栏自身的回归测试）
- [x] `find examples/riscv-mini/src -name '*.h' | wc -l` = **1**（只剩 SoC 顶层）
- [x] `find examples/riscv-mini/src -name '*.h'` 与 `git ls-files` 一致（无未跟踪新增）
- [x] `include/cpu/AGENTS.md` 列出的每个 `.h` 都在磁盘上存在（无幽灵条目）
- [x] 5 个既有护栏脚本 + `run_all_ported_tests.sh` + `run_all_samples.sh` 全绿
      ⇒ **2026-10-25 实测**：5 护栏 exit 0 / `run_all_ported_tests.sh` **28/28** /
      `run_all_samples.sh` **35/35**
- [x] `bash scripts/verify_perf_baseline_anchors.sh HEAD` 绿（3 条守恒检查）
- [x] `bash scripts/verify_perf_baseline_anchors.sh --self-test` 绿（**15 项断言**）
      ⇒ 护栏自身的回归测试（它在开发中**实测暴露过 3 个真缺陷** ⇒ 必须有断言钉住）
- [x] `openspec validate` + `scripts/verify_openspec_delta.sh` 绿
- [x] `ctest -N` = **199**（测试数不变 —— 纯结构收敛）

## Capabilities

| Capability | 影响 |
|---|---|
| （新）`rv32i-canonical-home` | 单源化 + 归属表 + 护栏 + 前推 mux 的归属裁决 |

## Impact

### ⚠️ 附带改动：`tests/benchmark/perf_baseline.json` 重生成（**非** C2 性能变化）

旧 baseline 是 **2026-10-16** 在 `37ab63f` 采的，对当前机器**已过时** ⇒
`perf_regression` 报 9–11 处「回归」（比率 **1.3×–6.9×**）。

**这不是 C2 的性能变化**，由**对照实验**证明：在 pre-C2 的 worktree（`5191c47`）与
C2 HEAD 上**背靠背**跑 `perf_tests --tc=07 --tc=08` ——
① 两者**编译选项完全一致**（`-O3 -DNDEBUG -O2 -DNDEBUG -std=c++23`）；
② 两者**依赖闭包不含 `include/cpu/`**（perf 源文件不 include 任何 cpu/ 头，
`src/**` 也不 include ⇒ `cpphdl` 库未被 C2 触碰）；
⇒ **HEAD/pre-C2 比率 = 0.89–1.21（均值 ~1.03）⇒ C2 零性能影响**；
而 **pre-C2/baseline = 1.01–2.36×** ⇒ **pre-C2 构建同样"回归"**
⇒ 漂移是**环境性的**（共享主机上的并发负载；cgroup 无 CPU 配额、无 throttle 迹象）。
旁证：C2 完全没碰的 `TC-08|regs=1000|interpreter` 也从 6591.8 us 涨到 10140.5 us。

⇒ 故按仓库既有先例（`ed89fa4` / `3f1ab5c` / `92e7fe6` / `45f684b`）**重生成 baseline**，
并在 `note` 字段写入根因 + **旧值留档** + **排查指引**（再报红时先做 pre-C2-vs-HEAD 对照：
真正的代码回归是**稳定**比率，环境漂移是**波动**比率且波及 pre-C2 构建与 interpreter 行）。

#### ⚠️ TC-09 的 6 个锚点：**恢复**而非降级（否则未来真回归会永久不可见）

本次采集时 **TC-09 全部行 SKIPPED**（`skip_reason="no row for backend '…' params '…'
in subprocess output"` —— 与 2026-10-16 同一根因：`subprocess_runner.h` 的 **180 s**
超时不足以跑完 `ch_uint<32>` 算术链 depth=1000）。

而 `perf_regression.cpp` 的判据是 `if (base <= 0.0) continue;` ⇒ **baseline 行一旦
`median_us` 变成 0，就永久退出比较集合** ⇒ 若照抄本次采集结果，TC-09 的
**interpreter / jit × 3 depth = 6 个锚点**会被**静默丢弃** ⇒ 今后**任何真实的
TC-09 性能回归都不会报红**（第四轮审稿实测：合成 10× 的 TC-09 回归，新 baseline
判「No regressions」rc=0，旧 baseline 判「6 regression(s)」rc=1）。

⇒ **裁决**：这 6 个锚点**逐行原样恢复**自 2026-10-16 baseline（打上
`note_restored` 标记），**不**降级为 0 ⇒ 它们继续参与比较。
⚠️ 代价与边界：它们记录的值是**旧的**（2026-10-16 的机器状态），当机器再次
变慢时它们会**偏严**（比值偏高）⇒ 这属于**安全方向**（宁可误报不可漏报），
且与本次的环境漂移结论一致。

#### 新增护栏 `scripts/verify_perf_baseline_anchors.sh`（重生成 baseline 的守恒闸门）

上面两类风险（「行静默消失」「锚点降级为 0」「变松未披露」）在本次之前**没有任何
自动化检查** ⇒ 重生成 baseline 是**纯人工**动作 ⇒ 人工漏一处 ⇒ 未来真回归永久不可见。
⇒ 新增 3 条守恒检查（基准 = `git show <ref>:tests/benchmark/perf_baseline.json`，
PR 用 base branch、push 用 `HEAD`）：

| 检查 | 判据 | 守住什么 |
|---|---|---|
| **C1 行集守恒** | 新 baseline 的 (test, params, backend) 集合 MUST 等于基准的行集 | 行被删 ⇒ 永久退出 `perf_regression` 的比较集合 |
| **C2 锚点守恒** | 基准里 `status=PASS` 且 `median_us>0` 的行 MUST 仍存在**且仍 `>0`** | `perf_regression.cpp:179` 的 `if (base <= 0.0) continue;` ⇒ 降级为 0 = 永久失明 |
| **C3 变松披露** | 任何比基准**更慢**（超过该 backend 阈值）的行 MUST 在 baseline 的 `note` **披露表**里以 `<test> <params> <backend> old= new= ratio=` 的**行**被点名 | 重生成 baseline 会「用当前（较慢）机器水平」重新锚定 ⇒ 静默放宽检出力 |

⚠️ 阈值 MUST 从**权威源**（`tests/benchmark/perf_regression.cpp` 的
`kBackendThresholds`）读出，且权威源**不可读 / 不可解析**时 MUST **fail-closed**
（报违规）⇒ **不得**回落到脚本内置的默认值 —— 回落是 **fail-open**：权威源被
改写（格式变了 / 常量改名 / 新增 backend）时，护栏会静默用**旧阈值**判
⇒ 恰好复现本条想消除的漂移。⇒ 阈值表还 MUST 覆盖 baseline 里出现的
**所有** backend（否则那些行的「变松」永远不会被披露）。
⚠️ 披露判定 MUST 精确到**表行**（正则），**不能**用「三个子串各自出现」——
note 里还有别的句子会提到同名 token ⇒ 那样判会让该三元组**恒被判为已披露**
⇒ 披露表漏写它也不会被发现（**实测**：旧逻辑 rc=0 / 新逻辑 rc=1）。
⚠️ 披露行里的 `old=` / `new=` / `ratio=` **数值** MUST 与实际计算值一致
（数值写错的披露会让阅 reviewer 时被误导 ⇒ 也算违规）。
⚠️ **阈值 MUST 从权威源读出**（`perf_regression.cpp` 的 `kBackendThresholds`）
⇒ 脚本内不得留硬编码副本（副本漂移的方向可能是**不安全**的：有人放宽 C++ 侧
阈值而脚本仍按旧值判 ⇒ 放过未披露的变松）。
⚠️ **任何标记字段不得成为降级逃逸口**：`note_restored` 的唯一含义是
「**取值**逐字来自旧 baseline」⇒ 带该标记的行 MUST 断言其 `median_us` 与基准
**相等**（旧实现里，加一个 JSON 字段就能把 PASS 锚点永久降级为 0 而护栏报 PASS
⇒ 正是这道护栏要防的那件事）。
⚠️ 护栏自带 **`--self-test`（15 项断言）** —— 它在开发中**实测暴露过 3 个真缺陷**
（heredoc 后残留的 `RC=$?` 把退出码覆盖回 0 ⇒ 违规只打印、CI 却见 PASS；
上述披露判定缺陷；上述 `note_restored` 逃逸口）⇒ 三者都由 fixture 钉住
（变异验证：退回任一缺陷 ⇒ 对应 fixture 转红）。
⚠️ 阈值来源的 fixture MUST **自带** `tests/benchmark/perf_regression.cpp`
（否则阈值解析路径零覆盖 —— 护栏在 self-test 里始终用内置副本 ⇒
「阈值取自权威源」这条断言是空的）。
⚠️ 基准 ref 解析不到（如 CI 的 `git fetch` 失败）⇒ **跳过 + `::warning`**
（不阻断 PR），但 MUST 显式告警（静默跳过会让这道闸门长期形同虚设）；
⚠️ 因此 CI 侧 `git fetch` 失败 MUST **硬失败**（不得 `|| true` 吞掉），
否则闸门会在 PR 上静默失效而 CI 仍绿。
⚠️ push / schedule 事件下基准 = `HEAD` ⇒ **自比恒过** ⇒ 闸门只在 PR 事件生效
（脚本会打印 notice 明示这一点）。

### 风险

| 风险 | 等级 | 缓解 |
|---|---|---|
| `rv32i_decoder.h` 等 3 组「`using namespace` 位置差」重指后**名字可见性收窄** ⇒ 根测试编译失败 | 🟡 中 | 逐 target 编译验证；必要时补 `using namespace ch::core;` |
| **重生成 perf baseline ⇒ perf gate 阈值被「当前（较慢）机器水平」重新锚定** ⇒ 可能**放宽**对未来真回归的检出 | 🟡 中 | `note` 字段写入旧值留档 + 排查指引；真回归仍表现为**稳定**比率（对照实验可区分）；`ctest -L perf` 阈值本身（interpreter 2.5× / jit 1.6× / verilator 1.3×）**未改**；**新增护栏 `scripts/verify_perf_baseline_anchors.sh` 的 C3（变松披露）** ⇒ 每一条被放宽的行 MUST 在 `note` 的披露表里被点名（判据精确到表行正则**且数值与实际一致**）；阈值本身**从 `perf_regression.cpp` 读出**（不硬编码副本） |
| **重生成 baseline 时若某行从 PASS 变成 `median_us=0`（SKIPPED）⇒ 该行永久退出 `perf_regression` 的比较集合** ⇒ 未来的真回归**不可见** | 🟡 中 | 本次已**逐行恢复** TC-09 的 6 个锚点（`note_restored` 标记）；`note` 字段写明「重生成时 MUST 检查 `-status PASS` 计数是否归零」；`perf_regression` 的 `base<=0 ⇒ continue` 语义**未改**（改它属独立 change）；**新增护栏的 C1（行集守恒）+ C2（锚点守恒）** ⇒ 今后这类静默消失会被 CI 挡住；⚠️ **任何标记字段（含 `note_restored`）都不得成为降级逃逸口** ⇒ 带标记的行 MUST 断言取值与基准**逐字相等** |
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
- 不整治「根测试 target 直接编译 example 源文件」这一**第三形态**反向耦合
  （当前 1 处：`tests/CMakeLists.txt:63` 编译
  `examples/riscv-mini/tests/test_csr_unit.cpp`）⇒ 它是**已知债务**，属**测试归属**
  问题（非微架构单源化），归独立 change 处理；本 change 的护栏 C2/C3 只覆盖
  「反向 `-I`」与「反向 `#include`」两种形态，MUST NOT 被本护栏的 PASS 掩盖
