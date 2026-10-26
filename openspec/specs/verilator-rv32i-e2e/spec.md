# verilator-rv32i-e2e Specification

## Purpose
规定 **RV32I** 相关组件在 **VerilatorBackend**（生成的 Verilog 仿真）下 MUST 满足的
契约，覆盖三个层面：

1. **流水线 trace parity** —— 同一份 RTL 在 interpreter 与 verilator 两个后端下
   MUST 行为一致；且断言 MUST 是**语义断言**（可从外部观测的 ISA 事实）⇒
   纯计数器常量**钉不住任何东西**（"绿 ≠ 对"）。
2. **SoC 级的双路逐周期 parity** —— RV32I SoC 顶层 MUST 暴露可观测端口（观测
   输出 + 固件写端口，且 MUST NOT 留死端口），比对 MUST 覆盖**全部**输出观测值
   （**无掩码**）⇒ 逐周期 diff MUST 在「同一 TEST_CASE 内两个 device + 两个
   Simulator」的形态下做（MUST NOT 用 `DUAL_BACKEND_TEST_PAIR` —— 那个宏的
   语义是「同一 body 各跑一遍」，无法跨后端 diff）。
3. **JIT 回落行为 MUST 被显式断言** —— RV32I 的 imem/dmem 用 `ch_mem` ⇒ 图的
   节点宽度 > 64 bit ⇒ JIT **必然回落**。这不是缺陷，而是**已知能力边界**
   ⇒ MUST 被**显式断言**：①「回落」≠「未启用」（`is_jit_enabled()` 真但
   `is_jit_compiled()` 假）；② 回落**原因可归因**（日志 MUST 指明节点类型 /
   node_id / 宽度，MUST NOT 是空字符串或「small graph or unsupported op」这类
   不可区分的措辞）；③ 回落后的观测向量与**显式 interpreter** 的结果逐周期一致
   （为将来「JIT 支持了 mem read port」的前置护栏）。

⚠️ 已知边界（**如实登记**，见 `AGENTS.md` 的 KNOWN LIMITATIONS）：当前实现会丢弃
「消费者之后的第一条指令」（imem 的 1 拍 `sread` + IF/ID 冻结 ⇒ 已提交未锁存的
那条无处可存）⇒ 固件 MUST 用 NOP 填充该位置（已有 compile-time 静态护栏）；
正解需**取指 skid buffer + PC 回退** ⇒ 独立立项。

## Requirements

### Requirement: RV32I 5-stage 流水线 MUST 在 VerilatorBackend 下与 InterpreterBackend trace parity

`tests/test_verilator_complex_e2e.cpp::ComplexE2E - Rv32iShortInstructionTraceParity` MUST 在同一 fixture 上分别用 `InterpreterBackend` 与 `VerilatorBackend` 跑 ≥ 20 cycle。

⚠️ **断言 MUST 是语义断言**，MUST NOT 是可被「伪事件」满足的计数器
断言（`perf_instructions` 的语义见 `rv32i-commit-path` capability）
⇒ 该用例 MUST 断言**可从外部观测的 ISA 语义事实**。

imem MUST 用 `ch_mem` sread（posedge 对齐，与 aread 0-cycle comb 区分），
dmem 必须是简化 stub（write=0, read_data=0，因为该固件不触发 load/store）。

#### Scenario: RV32I 3 条无 hazard 指令 trace parity

- **WHEN** fixture 装以下 **3 条有效指令** + **3** 条 NOP（其余 **10** 个字为 NOP 填充；
  共 16 字）：
  （3 条有效指令 = 2 条 `addi` + 1 条 `add`；NOP 是为「无数据前推」留的间隔）
    - `addi x1, x0, 1` = `0x00100093`   （word 0）
    - `addi x2, x0, 2` = `0x00200113`   （word 1）
    - `nop` = `0x00000013`（×3，word 2..4 —— 无前推 ⇒ 生产者与消费者之间
      MUST 留间隔）
    - `add x3, x1, x2` = `0x002081B3`    （word 5）
- **AND** 分别用 interpreter 与 verilator 跑 20 cycle
- **THEN** 两端的**退休指令数** MUST 相等
- **AND** 两端 MUST 都观测到「**x3 的写回值为 3**」
  （`wb_write_en == 1` 且 `rd == 3` 且 `write_data == 3`）
  —— 这才是 ISA 语义断言（`x3 = 1 + 2 = 3`）
- **AND** 该断言 MUST 在「退休 = 有效链」的语义之上
  （若退休被错接成「load 数据有效」，本固件**不含 load** ⇒ 退休数恒 0 ⇒ 断言失败）
- **NOTE**: 观测点 `perf_instructions` / `wb_write_en` / `wb_write_data` /
  `wb_rd_addr` 是顶层 `ch_out` 端口（verilator 可观测）

#### Scenario: 退休指令数的期望值 MUST 由**逐拍实测**推导并记录

- **WHEN** 断言退休指令数的具体数值
- **THEN** 该数值 MUST 等于「拍数 − 填充拍数」，且**推导过程 MUST 写在代码注释里**
- **AND** MUST NOT 写成「指令条数」
  （该 fixture 的固件在 20 拍内会**回绕**（`bits<5,2>` ⇒ 16 字可达）⇒
  「已执行的指令条数」与「拍数」**不是**同一个量 ⇒ 写成「指令条数」会
  与实现对不上）
- **NOTE**: 当前值 = **17**（20 拍 − 3 拍填充：IF/ID → ID/EX → EX/MEM → MEM/WB
  依次就绪，退休发生在 MEM 级）

#### Scenario: RV32I imem 用 ch_mem sread（非 aread）

- **WHEN** fixture 用 `ch_mem<ch_uint<32>, 16>` 初始化 3 条有效指令 + 3 条 NOP
      + 10 个 NOP 填充
- **AND** imem 通过 sread 端口返回 `instr_data`（posedge 对齐）
- **AND** 字索引 MUST 用 `bits<5,2>`（**4 位** ⇒ 16 字，与 `ch_mem` 容量一致；
  ⚠️ `bits<MSB,LSB>` 是**闭区间** ⇒ 宽度 = MSB−LSB+1 ⇒ `bits<4,2>` 只有
  **3 位** ⇒ 只可达 8 字 ⇒ 与 16 字的 `ch_mem` **不匹配**）
- **THEN** trace parity 成立（vs aread 0-cycle comb 会破坏 parity，见 verilator-e2e-coverage spec §Requirement 1 invariant 3）

#### Scenario: RV32I dmem stub 不影响 perf_instructions 验证

- **WHEN** 该固件全部是 ALU 操作（addi / add），不触发 load/store
- **THEN** dmem stub（write=0, read_data=0）的存在不改变退休指令数
- **AND** trace parity 仍然成立

#### Scenario: RV32I pipeline 5 阶段不依赖 M0.5 issue #25

- **WHEN** 退休指令数通过 ch_out<ch_uint<48>> 暴露
- **AND** commit_count 是 always_ff reg 但其值通过顶层 ch_out 端口被 verilator 的 Vtop 字段访问器读出
- **THEN** 即使内部流水线寄存器 (IF/ID, ID/EX, EX/MEM, MEM/WB) 在 M0.5 dispatch 下传播不可靠 (issue #25)，只要退休计数的累加路径正确，trace parity 仍然成立
- **NOTE**: 仍依赖 issue #25 影响 eval_sequential 的 register update 路径

#### Scenario: RV32I 不触发 CSR / exception / interrupt 路径

- **WHEN** 该固件全部是 ALU 操作，无 ECALL / EBREAK / CSR 读写
- **THEN** RV32I 内部 CSR bank / exception handler / CLINT 路径不触发，fixture 不依赖这些组件的正确性

### Requirement: RV32I e2e test 必须有 explicit InterpreterBackend pinning

`tests/test_verilator_complex_e2e.cpp::ComplexE2E - Rv32iShortInstructionTraceParity` interpreter 一侧 MUST 显式调用 `sim_a.set_backend(std::make_unique<InterpreterBackend>())` —— 默认 `Simulator` 在 `CH_JIT_ENABLE=ON`（默认）下实际走 JIT（`include/simulator.h:569`），parity 测试的 interpreter 侧 MUST 显式 `set_backend()`，否则失败时无法归因 JIT vs verilator。

#### Scenario: InterpreterBackend 显式 pin

- **WHEN** fixture 构造 `Simulator` 实例
- **THEN** interpreter 一侧的 `Simulator` 调用 `set_backend(std::make_unique<InterpreterBackend>())` 显式锁定
- **AND** verifier 一侧的 `Simulator` 调用 `set_backend(std::make_unique<VerilatorBackend>(workdir))`

#### Scenario: CH_JIT_ENABLE=OFF 也能跑

- **WHEN** 环境变量 `CH_JIT_ENABLE=OFF` 或 cmake `-DCH_JIT_ENABLE=OFF`
- **THEN** RV32I e2e test 不受影响（fixture 不依赖 JIT）
- **AND** `ctest -R test_verilator_complex_e2e` 在该环境下仍通过

### Requirement: RV32I e2e test TIMEOUT 必须显式 600s

`tests/CMakeLists.txt` 注册 `test_verilator_complex_e2e` 时 MUST `set_tests_properties(test_verilator_complex_e2e PROPERTIES TIMEOUT 600)`（`add_catch_test` 不支持 TIMEOUT 参数；RV32E 测试 vis RV32I 命令数 > 200，cold verilator compile 约 1-3 min，warm cache ~5-10s，TIMEOUT 600s 给 2× warm 余量）。

#### Scenario: cold cache 编译在 TIMEOUT 内

- **WHEN** `$HOME/.cache/cpphdl/verilator/` 不含 RV32I 设计的 cache key
- **AND** `ctest -R test_verilator_complex_e2e`
- **THEN** 整个测试在 600s 内完成（cold compile + 2 TEST_CASE 运行）

#### Scenario: warm cache 立即恢复

- **WHEN** `$HOME/.cache/cpphdl/verilator/` 已含 RV32I 设计的 cache key（previous test run）
- **AND** `ctest -R test_verilator_complex_e2e`
- **THEN** 整个测试在 60s 内完成（warm cache + 2 TEST_CASE 运行）

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
