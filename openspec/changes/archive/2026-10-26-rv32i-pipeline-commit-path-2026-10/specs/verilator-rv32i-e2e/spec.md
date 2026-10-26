# Capability: verilator-rv32i-e2e

## MODIFIED Requirements

### Requirement: RV32I 5-stage 流水线 MUST 在 VerilatorBackend 下与 InterpreterBackend trace parity

`tests/test_verilator_complex_e2e.cpp::ComplexE2E - Rv32iShortInstructionTraceParity` MUST 在同一 fixture 上分别用 `InterpreterBackend` 与 `VerilatorBackend` 跑 ≥ 20 cycle。

⚠️ **断言 MUST 是语义断言，不得是可被「伪事件」满足的计数器断言**：
`perf_instructions` 的语义是「已退休指令数」，而退休由**有效链**定义
（见 `rv32i-commit-path` capability）。若实现把「退休」错接成「load 数据有效」，
则一条**不含 load** 的固件也会「凑」出计数 ⇒ 纯计数器断言
**钉不住任何东西**（"绿 ≠ 对"）。

⇒ 该用例 MUST 断言**可从外部观测的 ISA 语义事实**，MUST NOT 仅断言计数器常量。
当前实现采用的语义断言是「**写回值**」：固件的 ALU 结果 MUST 经寄存器堆写回
端口被观测到，且等于 ISA 正确值。

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
