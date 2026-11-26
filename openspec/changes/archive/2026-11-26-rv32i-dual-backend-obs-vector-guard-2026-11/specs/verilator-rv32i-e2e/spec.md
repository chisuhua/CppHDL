## MODIFIED Requirements

### Requirement: RV32I SoC MUST 有 interpreter 与 verilator 的双路逐周期 parity

- **WHEN** 固件含 ≥ 20 条 RV32I 指令，覆盖 ALU-only / load-store / branch 三类路径
- **THEN** 同一 TEST_CASE 内 MUST 各建一份 `ch::ch_device<Rv32iCoreTop>`
  + 一份 `Simulator`；interpreter 侧 MUST 显式
  `set_backend(std::make_unique<InterpreterBackend>())`，verilator 侧 MUST
  `set_backend(std::make_unique<VerilatorBackend>(workdir))`
- **AND** 每 tick MUST 跨后端比对**核顶层全部 `ch_out` 观测端口**，持续 ≥ 200 拍
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

#### Scenario: 观测集合 MUST 对着端口面守恒（**新增**）

- **WHEN** 核顶层 `__io` **新增**一个 `ch_out` 端口（或既有端口改名 / 删除）
- **THEN** 该端口 MUST 出现在（跨后端比对向量 ∪ 缺口登记表）里
- **AND** 未纳入向量者 MUST 逐条登记在 `.rddf/checks/rv32i-obs-vector-registry.md`
  （带「为什么未纳入」+「归位条件」两列）
- **AND** 该集合 MUST **对着核顶层端口面**判定 ⇒ ⇒ **MUST NOT** 用「写死的条数」表达
  （写死条数 ⇒ 加端口时静默失同步 ⇒ 正是 S4a 的情形）
- **AND** 若它**只**出现在登记表 ⇒ MUST 有「为什么未纳入」与「归位条件」两列（空 ⇒ 红）
- **AND** 若它**补进了向量** ⇒ 对应的登记行 MUST 被删除（登记残留 ⇒ 红）
  ⇒ ⇒ 理由：`ch_out` 端口面是**被冻结的契约**（ADR-046 D10），而逐周期 parity 的
  **判别力 = 观测面 == 端口面**；once 观测面相对端口面缩水**而没有信号**，
  「parity 全绿」就退化成「比对了一个子集且无人知道」——`rv32i-soc-mmio-channel-2026-11`
  （S4a 加 8 in + 11 out）正是这种情况（向量停在 28，端口面 39，无护栏转红）

#### Scenario: 缺口登记 MUST 双向可判（**新增**）

- **WHEN** 登记表里出现一个**不存在**于核顶层 `ch_out` 的端口名
- **THEN** 判据 MUST 红（禁止编造豁免）
- **AND** 登记表的「为什么未纳入」/「归位条件」任一为空 MUST 红
  ⇒ ⇒ 理由：单向登记（只判「登记了就豁免」）会退化成「按名字的永久自动豁免」——
  仓内已在 `examples/removed_examples.txt` 上踩过这个形态

#### Scenario: verilator 缺席 MUST log-and-skip

- **WHEN** `command -v verilator` 失败
- **THEN** verilator 半边 MUST `WARN` + 提前返回，MUST NOT 用 Catch2 `SKIP`
  （全 SKIP ⇒ Catch2 exit code 4 ⇒ CTest 判红）
- **AND** 若 `VerilatorBackend::initialize()` 返回 false（如端口 > 64 位）同样
  `WARN` + 提前返回
