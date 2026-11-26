## ADDED Requirements

### Requirement: trap 入口 MUST 保存异常上下文并重定向 PC

RV32I 的**非法指令**（`illegal_reason ∈ {2, 3}`）MUST 按机器级 trap 处置：

- **WHEN** 一条**非法指令**进入 ID 级，且该拍**没有**控制流冲刷（`flush`）
- **THEN** 它 MUST **触发一次 trap**（取代「流水线级冻结」）
- **AND** `mepc` MUST = **该指令自身的 PC** · `mcause` MUST = **2** ⇒ ⇒ 二者 MUST 是**同一次异常**的一对
- **AND** 取指级 PC MUST **重定向**到 `mtvec` 的**基址**
- **AND** 该指令 MUST **不退休** ⇒ ⇒ IF/ID MUST 被杀 ⇒ ⇒ 在途的更老指令 MUST 照常退休


#### Scenario: 控制流冲刷 MUST 优先于 trap（错误路径不得写坏上下文）

- **WHEN** 同一拍既有 taken 分支（`flush`）又有**非法指令**（ID 级的 fall-through 槽）
- **THEN** MUST **不**触发 trap
- **AND** `mepc` / `mcause` MUST **保持不变**
  ⇒ ⇒ 理由：flush 那一拍 ID 级持有的是**错误路径**上的指令 ⇒ ⇒ 用它的 PC 写 `mepc`
  ⇒ ⇒ 会把**错误路径**的 PC 写进异常上下文

#### Scenario: trap MUST 多杀一拍（取指在途指令）

- **WHEN** 一条 trap 发生
- **THEN** ID 级 MUST **连续两拍**无效（第二拍对应**上一拍已提交给取指口**的地址）
- **AND** 取指侧的 skid 缓冲 MUST 在该窗口被**清空**
  ⇒ ⇒ 理由：取指是 1 拍 `sread` ⇒ ⇒ 只杀一拍会让在途指令**正常进 ID**

#### Scenario: CSR 存储体只有一条写口 ⇒ 陷阱上下文 MUST 优先

- **WHEN** 同拍既有 CSR 指令的 CSR 写、又有陷阱上下文的写（`mepc` / `mcause`）
- **THEN** **陷阱上下文**的写 MUST 生效
- **AND** CSR 指令的那次写 MUST 被抑制
  ⇒ ⇒ 理由：陷阱上下文是「**一次异常**」的**成对**写入 ⇒ ⇒ 丢它 ⇒ ⇒ 软件永远看不到原因
  ⇒ ⇒ ⚠️ 被抑制的那次写**不得**被「补偿重试」（单写口无队列）

#### Scenario: 本 Requirement 只覆盖「非法指令」这一个触发源（**触发集 = `{2, 3}`**）

- **WHEN** 触发源是 `ECALL` / `EBREAK` / `MRET` / `WFI`（`OP_SYSTEM` 的 `funct3 = 0`
  ⇒ `illegal_reason = 1`）或 **中断**
- **THEN** 它们 MUST **仍**按「已知未实现」处置（`illegal_reason` = 1 ⇒ ⇒ **冻结**
  ⇒ ⇒ **不**触发 trap）
  ⇒ ⇒ 那些**原因码**与**中断路径**属**独立立项**
- **AND** `mtval` MUST **保持 0**（本 Requirement **不**覆盖其真值）
- **AND** ⚠️ **触发集 MUST 显式写成 `illegal_reason ∈ {2, 3}`**（**未知 opcode**（2）·
  **ISA 保留组合**（3））⇒ ⇒ **MUST NOT** 写成「`illegal_reason != 0`」
  ⇒ ⇒ 架构依据 = ADR-048 §8.2 **DA5**（`illegal_active`（`!= 0`）**继续只服务 stall**）
