# rv32i-isa-semantics Specification

## Purpose

本 capability 管辖 **RV32I 指令语义**在 CppHDL 实现上的**行为契约** —— 即
「某条指令在 SoC 上执行时，其可观测结果 MUST 与 RISC-V 规范一致」。

**它不**管辖「实现与自身是否一致」：那是 `rv32i-commit-path`（SoC 观测面契约）
与 `dual-backend-test-coverage`（双后端逐拍 parity）的职责 ——
「parity 绿」只证明 interpreter 与 verilator 给出**相同**结果，
**不**证明该结果符合 ISA。

**本 capability 的由来**（2026-10-26 的 roadmap 对齐审计）：`feat-rv32i-isa-completeness`
实测挖出 RV32I 的多处**语义缺口**（字节道写不生效 / 写数据未 lane 对齐 /
JAL·JALR 不重定向 PC / SRA 退化为 SRL）—— 此前它们只登记在根
`AGENTS.md §KNOWN LIMITATIONS`，roadmap 零覆盖。每个缺口落地后在此登记其
**长期契约**（含边界、已知限制与适用范围），避免它们随时间从文档里消失。

## Requirements

### Requirement: 字节道写 MUST 只写选中的车道

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

### Requirement: `DataTCM.strbe` MUST 被其实例化点显式驱动

- **WHEN** 一个 `DataTCM` 实例被构造
- **THEN** 它的 `strbe` 输入端口 MUST 被**显式驱动**
- **AND** MUST NOT 依赖「未驱动端口的默认读值」

#### Scenario: 未驱动端口的读值是未定义的

- **WHEN** 一个输入端口**没有任何驱动**
- **THEN** 它的读值 MUST NOT 被当作「全 0」或「全 1」的契约
- **AND** 依赖该默认值的实现 MUST 被视为**隐患**（实测：未驱动的输入端口
  读到的是**全 1** ⇒ 恰好等价于「全车道写」⇒ 整字写用例会**碰巧**通过）
- **AND** interpreter 与 verilator 两个后端的未驱动初值 MUST NOT 被假定相同

#### Scenario: 整字写与字节写走不同的车道来源

- **WHEN** 一个 fixture 同时存在「测试台直接写」与「流水线写」两条写路径
- **THEN** `strbe` MUST **随写路径切换**：直接写 ⇒ 全车道（`0xF`）；
      流水线写 ⇒ 流水线算出的车道掩码
- **AND** 恒驱动全车道 MUST NOT 被当作「整字写」的实现方式
      （那会让流水线写的 `sb` / `sh` 变成「**lane 对齐数据的整字覆盖**」）

> ⚠️ **范围限定（2026-10-26 critic SHOULD_FIX #3）**：本 Requirement 只约束
> **本 capability 引入的 `DataTCM.strbe`**（全仓 3 个实例化点，grep 可证）。
> 「**任意**新增输入端口 ⇒ 全实例化点 MUST 显式驱动」是**框架层约定**，
> 其核验需要一个「必需端口注册表」（不同端口的未驱动读值语义不同：
> `rst_n` 读 0 = 永久复位、`strbe` 读全 1 = 碰巧整字写）⇒ **独立立项**，
> 不在本 capability 内（见 `AGENTS.md` 的约定层与 backlog）。
