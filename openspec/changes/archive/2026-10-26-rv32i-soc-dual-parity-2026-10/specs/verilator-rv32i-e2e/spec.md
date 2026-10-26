# Capability: verilator-rv32i-e2e

## ADDED Requirements

### Requirement: RV32I SoC 顶层 MUST 暴露 verilator 可观测的观测输出

- **WHEN** `Rv32iPipelineTop`（`examples/riscv-mini/src/rv32i_pipeline_top.h`）被用作
  双后端 parity 的 DUT
- **THEN** 它 MUST 以 `ch_out` 暴露观测信号：`obs_instr_addr`、`obs_data_addr`、
  `obs_data_write_data`、`obs_data_write_en`、`obs_data_rdata` + 6 个 `obs_perf_*`
  （与 `Rv32iPipeline` 的 6 个 `perf_*` 端口一一对应）= **11 个功能观测**
- **AND** 它 MUST 额外暴露**诊断观测**（`dbg_*`）以定位「指令流 / 有效链 /
  写回 / 冒险脉冲」⇒ 合计 **27 个输出**（总端口 **32** = 5 输入 + 27 输出）
  ⚠️ 实际集合由护栏 C6 快照钉住 ⇒ 以快照为准
- **AND** 每个观测端口的位宽 MUST ≤ 64（ADR-035 R3 verilator 端口上限）

#### Scenario: 观测点必须走顶层端口

- **WHEN** parity 测试需要读某个内部信号
- **THEN** 该信号 MUST 通过顶层 `ch_out` 引出
- **AND** MUST NOT 依赖 verilator 读设计内部（`VerilatorBackend` 只对
  `port_access_` 里的顶层端口建访问表）

#### Scenario: 观测向量覆盖取指流与存储流

- **WHEN** 固件含 ALU / load-store / branch 三类路径
- **THEN** `obs_instr_addr` MUST 反映取指地址流（PC 未穿过 MEM/WB 级，
  `obs_instr_addr` 是 PC 流的等价物）
- **AND** `obs_data_addr` / `obs_data_write_en` / `obs_data_write_data` MUST
  反映 MEM 级存储流

### Requirement: RV32I SoC 顶层 MUST 暴露固件写端口且 MUST NOT 保留死端口

- **WHEN** `Rv32iPipelineTop` 装配 `InstrTCM` / `DataTCM`
- **THEN** 它 MUST 暴露 `prog_we` / `prog_addr` / `prog_data` 并接到
  `InstrTCM` 的 `write_en` / `write_addr` / `write_data`
- **AND** 其端口集合 MUST NOT 含**未被 `describe()` 引用**的端口
- **AND** 端口集合 MUST 由 `scripts/verify_no_duplicate_rv32i_headers.sh` 的 C6
  快照钉住（名 + 方向 + 位宽）

#### Scenario: 死端口 MUST 删除

- **WHEN** 某个 `ch_in` 端口在 `describe()` 里从未被引用（ADR-046 D10 冻结前的
  `ext_instr_data` / `ext_instr_ready` / `ext_data_read_data` / `ext_data_ready`）
- **THEN** 它 MUST 被删除（ZERO-DEBT 的「无死代码」）
- **AND** MUST NOT 以「保留给未来使用」为由留下

#### Scenario: 端口集合变化 MUST 被抓

- **WHEN** `Rv32iPipelineTop` 的端口被改名 / 增删 / 翻方向 / 改位宽，而 C6 快照未同步
- **THEN** 护栏 MUST 报违规
- **AND** 护栏 `--self-test` MUST 断言「改名 / 翻方向 / 改位宽」三种形态各自被抓

#### Scenario: 归位后零消费者豁免 MUST 失效

- **WHEN** `rv32i_pipeline_top.h` 因本 change 获得消费者（parity 测试 TU `#include` 它）
- **THEN** 护栏零消费者豁免表里的该条目 MUST 被移除，
  `EXPECTED_DEAD_ALLOWANCES` MUST 降为 0
- **AND** 条目残留 ⇒ 护栏 MUST 失败（`rv32i-canonical-home` 主 spec 的既有条款）

### Requirement: RV32I SoC MUST 有 interpreter 与 verilator 的双路逐周期 parity

- **WHEN** 固件含 ≥ 20 条 RV32I 指令，覆盖 ALU-only / load-store / branch 三类路径
- **THEN** 同一 TEST_CASE 内 MUST 各建一份 `ch::ch_device<Rv32iPipelineTop>`
  + 一份 `Simulator`；interpreter 侧 MUST 显式
  `set_backend(std::make_unique<InterpreterBackend>())`，verilator 侧 MUST
  `set_backend(std::make_unique<VerilatorBackend>(workdir))`
- **AND** 每 tick MUST 跨后端比对**全部 27 个输出观测值**（11 功能 `obs_*` +
  16 诊断 `dbg_*`），持续 ≥ 200 拍
- **AND** imem MUST 用 `sread`（`verilator-e2e-coverage` §R1 invariant 3）

#### Scenario: 不得用 `DUAL_BACKEND_TEST_PAIR` 宏做逐周期 diff

- **WHEN** 需要跨后端的**逐周期**比对
- **THEN** 测试 MUST NOT 用该宏（宏的语义是「同一 body 各跑一遍」，每个 run
  只看得见自己的后端 ⇒ 无法 diff）
- **AND** MUST 用「同一 TEST_CASE 内两个 device + 两个 Simulator」的形态
  （先例：`tests/test_verilator_complex_e2e.cpp` 的 SpiFsm 用例）

#### Scenario: 除逐周期 diff 外 MUST 断言终值 golden value

- **WHEN** 两端以**同一份错误**推进（逐周期 diff 全等）
- **THEN** 终值断言 MUST 抓到它：`obs_perf_instructions` 终值 == 固件中全部
  退休指令数、`obs_perf_branch_count` 终值 == 固件中分支指令数

#### Scenario: load/store 路径 MUST 真正影响结果

- **WHEN** 固件含 `sw` → `lw` 往返
- **THEN** 读回值 MUST 参与后续指令的计算（否则 load 路径未被覆盖 ——「绿 ≠ 对」）

#### Scenario: branch 路径 MUST 进入 parity

- **WHEN** 固件含一个 taken 分支与一个 not-taken 分支
- **THEN** `obs_instr_addr` 的逐周期序列 MUST 在两后端一致（取指流偏差即被抓）

#### Scenario: verilator 缺席 MUST log-and-skip

- **WHEN** `command -v verilator` 失败
- **THEN** verilator 半边 MUST `WARN` + 提前返回，MUST NOT 用 Catch2 `SKIP`
  （全 SKIP ⇒ Catch2 exit code 4 ⇒ CTest 判红）
- **AND** 若 `VerilatorBackend::initialize()` 返回 false（如端口 > 64 位）同样
  `WARN` + 提前返回

### Requirement: JIT 跳过原因 MUST 可归因

- **WHEN** JIT 编译在**任一**失败出口返回非 `SUCCESS`
  （⚠️ **实测有两个出口**，且**顺序**决定归因：`allocate_buffer` 的「节点宽度 > 64」
  在 `generate_ir` **之前** ⇒ 它是本 SoC 的**首个**命中点）
- **THEN** 该出口 MUST 设置 `last_error_msg_`，内容 MUST 标识**失败的节点**
  （类型 / node_id / 宽度）
- **AND** `Simulator` 的 `CHINFO("JIT skipped: %s")` MUST 打印该原因
  （实测：`unsupported node width: mem (node_id=…, size=131072 bits > 64)`）
  ⇒ MUST NOT 是**空**字符串，也 MUST NOT 是「small graph or unsupported op」
  这类**不可区分**的措辞
- **AND** MUST NOT 出现「日志说 `graph too small` 而真实原因是别的」这种归因错误

#### Scenario: JIT 回落原因 MUST 可归因（实测：**首个命中点是「节点宽度 > 64」**）

- **WHEN** JIT 编译失败并回落
- **THEN** 日志 MUST 打印**具体**原因（节点类型 / node_id / 宽度）
- **AND** MUST NOT 是空字符串或「small graph or unsupported op」这类**不可区分**的措辞
- **NOTE**: **实测**（2026-10-26）：RV32I SoC 的**首个**命中点是
  `allocate_buffer` 的「节点宽度 > 64」检查 ——
  `unsupported node width: mem (node_id=…, size=131072 bits > 64)`
  （D-TCM 的 `ch_mem<ch_uint<32>, 4096>` = 32 bit × 4096 字 = 131072 bit）。
  ⚠️ 因此**「mem read port」不是本 SoC 的首个命中点**（imem/dmem 确实含
  `type_mem_read_port`，但宽度检查**更早**）⇒ 断言 MUST 匹配**归因格式**，
  MUST NOT 匹配具体 node_id 或具体原因。

### Requirement: RV32I SoC 的 JIT 回落行为 MUST 被显式断言

- **WHEN** 对含 imem 的 RV32I SoC DUT 调用 `set_jit_enabled(true)`
- **THEN** 测试 MUST 断言：① `is_jit_enabled() == true` **且**
  `is_jit_compiled() == false`；② 日志含 `JIT skipped` 且原因**非空**且指明
  「节点类型 / node_id / 宽度」（⚠️ MUST 匹配**归因格式**，MUST NOT 匹配具体
  node_id 或具体原因 —— 实测 `find("mem_read_port")` 会命中**建图日志**的
  「Created node … (mem_read_port, 32 bits)」⇒ **假阳性**）；
  ③ 回落后的观测向量与**显式 `set_backend(InterpreterBackend)`** 的
  interpreter 结果逐周期一致
- **AND** `CH_JIT_ENABLE=OFF` 的构建里 MUST 编译通过并跳过该用例

#### Scenario: 回落不等于「未启用」

- **WHEN** 测试只断言 `is_jit_compiled() == false`
- **THEN** 该断言 MUST 视为**不充分**（`set_jit_enabled(true)` 从未被调用时它同样成立）
- **AND** 测试 MUST 同时断言 `is_jit_enabled() == true`

#### Scenario: 回落结果 MUST 与 interpreter 逐周期一致

- **WHEN** 同一 DUT 分别以 `set_jit_enabled(true)`（回落）与
  `set_jit_enabled(false)`（显式 interpreter）跑同一固件
- **THEN** 每 tick 的 10 个观测值 MUST 逐一相等
