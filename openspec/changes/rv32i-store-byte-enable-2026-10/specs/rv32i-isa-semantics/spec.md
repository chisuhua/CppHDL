# Capability: rv32i-isa-semantics

## ADDED Requirements

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

### Requirement: 新增的输入端口 MUST 被全部实例化点显式驱动

- **WHEN** 一个组件**新增**输入端口
- **THEN** 它的**全部实例化点** MUST 显式驱动该端口
- **AND** MUST NOT 依赖「未驱动端口的默认读值」
- **AND** 该约定 MUST 如实记录在端口的注释里

#### Scenario: 未驱动端口的读值是未定义的

- **WHEN** 一个输入端口**没有任何驱动**
- **THEN** 它的读值 MUST NOT 被当作「全 0」或「全 1」的契约
- **AND** 依赖该默认值的实现 MUST 被视为**隐患**（实测：未驱动的输入端口
  读到的是**全 1** ⇒ 恰好等价于「全车道写」⇒ 整字写用例会**碰巧**通过）
- **AND** interpreter 与 verilator 两个后端的未驱动初值 MUST NOT 被假定相同
