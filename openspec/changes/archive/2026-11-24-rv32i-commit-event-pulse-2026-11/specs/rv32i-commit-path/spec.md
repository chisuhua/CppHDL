## MODIFIED Requirements

### Requirement: RV32I SoC 顶层 MUST 有可观测的端口契约

- **WHEN** `Rv32iPipelineTop` 被用作双后端比对的 DUT
- **THEN** 它 MUST 暴露固件写端口（`prog_we` / `prog_addr` / `prog_data`）
  并接到 I-TCM 写接口
- **AND** 它 MUST 暴露**观测输出**：取指地址流、存储流（地址 / 写数据 / 写使能）、
  以及 6 个性能计数器
- **AND** 所有端口位宽 MUST ≤ 64（ADR-035 R3 verilator 端口上限）
- **AND** 端口集合 MUST NOT 含未被 `describe()` 引用的端口
- **AND** 端口集合 MUST 由 `scripts/verify_no_duplicate_rv32i_headers.sh` 的 C6
  快照钉住（名 + 方向 + 位宽）

#### Scenario: 观测面 MUST 让「指令退休」可被外部验证

- **WHEN** 一条固件被执行
- **THEN** 外部 MUST 能从顶层端口读出「已退休指令数」并与程序结构推算值一致
- **AND** MUST 能读出 store 的 `(地址, 数据)` 以核对 ISA 语义

#### Scenario: 端口集合变化 MUST 被抓

- **WHEN** SoC 顶层端口被改名 / 增删 / 翻方向 / 改位宽而 C6 快照未同步
- **THEN** 护栏 MUST 报违规，且 `--self-test` MUST 断言三种形态各自被抓

#### Scenario: 归位后零消费者豁免 MUST 失效

- **WHEN** `rv32i_pipeline_top.h` 获得消费者（比对测试 TU `#include` 它）
- **THEN** 护栏零消费者豁免表里的该条目 MUST 被移除，
  `EXPECTED_DEAD_ALLOWANCES` MUST 降为 0
- **AND** 条目残留 ⇒ 护栏 MUST 失败

#### Scenario: **事件**端口 MUST 是**单拍脉冲**（与事件时长无关）

- **WHEN** 观测面暴露「本拍**发生**了 X 次」类事件（写回 / store / 冒险 …）
- **THEN** 该事件端口 MUST 在**恰好一拍**为高
- **AND** MUST NOT 在「数据通路被反压冻结」的拍**继续保持**高
  （⇒ ⇒ **事件次数** MUST 与「事件**时长**」无关 ⇒ ⇒ 否则按事件计数的观测面
  会把**停顿拍数**算进**事件次数** ⇒ ⇒ **期望值将依赖实现时序** ⇒ ⇒ 不可手推）
- **AND** 「**级属性**」类端口（反映**流水线寄存器当前内容**的诊断口，如
  `dbg_exmem_is_store` / `dbg_id_is_store`）MUST **保持电平**
  （⇒ ⇒ **两类端口 MUST 可区分** ⇒ ⇒ 不许把级属性端口改成脉冲）

#### Scenario: **事件**端口 MUST 与「已退休指令数」**同口径**

- **WHEN** 数据通路被反压冻结（`mem_stall`）⇒ ⇒ MEM/WB 与 EX/MEM 寄存器**冻结**
- **THEN** 事件端口 MUST 在这些拍**为 0**
- **AND** 「已退休指令数」计数器 MUST 在**同一批**拍**同样不增长**
  （⇒ ⇒ 两者 MUST 是**同一个口径** ⇒ ⇒ 不许一个排除了停顿、另一个没排除）
