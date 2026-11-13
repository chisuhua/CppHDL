# Capability: rv32i-isa-semantics

## MODIFIED Requirements

### Requirement: 字节道写 MUST 只写选中的车道

- **MODIFIED**: 本 Requirement 的**契约面**扩展 —— 在既有内容之上追加两条 Scenario：
  ①「写使能 MUST 与被覆盖的地址同拍」（**相位**契约）；
  ②「MEM 反压期间写使能 MUST 保持」。
  理由：数据通路改**固定 1 拍寄存读**后 ⇒ ⇒ 写侧的**相位**从「看不见」变成
  「看得见的错」⇒ ⇒ 相位 MUST 成为契约的一部分；
  且 MEM 级新增反压 ⇒ ⇒ 反压期间的写行为 MUST 有明确定义。
  ⚠️ MODIFIED 会**整体替换**该 Requirement ⇒ **既有 Scenario 一条都不删**
  （「源数据低位对齐」/「掩码合并」/「整字写行为 MUST 不变」**逐字保留**）；
  ⚠️ 既有条款（WHEN/THEN/AND 全部）**逐字保留**，本 delta 只**追加**两条 Scenario
  ⇒ ⇒ 架构依据 = `docs/adr/ADR-068-dtcm-memory-timing-contract.md`（**Accepted**）。

- **WHEN** 一条 `sb` / `sh` 指令在 RV32I SoC 上执行
- **THEN** 目标字 MUST 只被修改**被选中的字节道**
- **AND** 未选中的车道 MUST 保持原值（读-改-写语义）
- **AND** 源数据 MUST **低位对齐**（RISC-V 语义）：`sb` 的源字节是 `rs2[7:0]`，
  `sh` 的源半字是 `rs2[15:0]`，由总线按 `addr[1:0]` 选车道

#### Scenario: 源数据低位对齐（lane 对齐）

- **WHEN** `sb` 写到 `addr` 且 `addr[1:0] == 1`
- **THEN** 写入的字节 MUST 落在该字的 **lane 1**
- **AND** 写入的字节值 MUST 是 `rs2[7:0]`
- **AND** 总线上的写数据 MUST 已按 `8 * addr[1:0]` 左移（否则「掩码对了、
  数据在错 lane」—— 比「字节写没生效」更隐蔽的错误）
- **AND** `sh` MUST 同样按 `8 * addr[1:0]` 左移（对齐的 `sh` ⇒ 偏移 0 或 2）

#### Scenario: 掩码合并（读-改-写）

- **WHEN** 存储子系统收到一个带字节使能的写
- **THEN** 它 MUST 先读出目标字的**当前值**
- **AND** 再把**被选中的车道**替换为写数据的对应车道
- **AND** 再把合并结果**整字**写回
- **AND** MUST NOT 把未选中的车道一并覆盖

#### Scenario: 整字写行为 MUST 不变

- **WHEN** 一条 `sw` 执行
- **THEN** 目标字 MUST 被**整字**覆盖（字节使能 = 全 1 ⇒ 合并结果 = 写数据）
- **AND** **非对齐** `sw` 的既有行为 MUST **保持不变**（不引入新的对齐检查）

#### Scenario: 写使能 MUST 与被覆盖的地址同拍（相位契约）

- **WHEN** 一条 `sb` / `sh` 在第 N 拍写地址
- **THEN** 写使能 MUST 在**同一拍**有效
- **AND** 被写入的车道 MUST 由**该拍**的 `addr[1:0]` 决定
- **AND** 判据 MUST 覆盖 `sb` / `sh` 的**四个字节车道 × 两个半字车道**组合
  （⇒ ⇒ 只覆盖 2 个用例**判别力不足**）

#### Scenario: MEM 反压期间写使能 MUST 保持

- **WHEN** MEM 级被反压（就绪信号被拉低）且当前有效链非空
- **THEN** 写使能 MUST **保持**（不重复发、不丢）
- **AND** 被写车道 MUST 仍由该拍地址决定
