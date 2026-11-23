# rv32i-dcache-timing Specification

## Purpose

约束 RV32I 的 **D-L1 数据缓存**（`chlib::DCacheSync`）对流水线暴露的**握手与时序契约**：
`ready` / `miss` 是**组合**信号（对当拍 `addr`）· `rdata` 是**保持寄存器** · `req` 是真握手；
line = 16 B 且 4 个 lane 各自可寻址；miss 的 line 填充是 **4-beat AXI4 burst**；
字节使能（`strbe`）贯穿到 line 更新与 AXI `w_strb`；以及核把它接进 MEM 级时 MUST 满足的
两条接线前提（**零请求放行** + **访存请求门控**）。

⚠️ **不含**：TCM 自身的读口时序（由 `rv32i-memory-timing` 约束 —— D-L1 是 TCM **之上**的层次，
流水线看到的「多拍等待」由 `data_ready` 反压表达，不是「延长存储器读延迟」）、
write-back 策略、L2 / DRAM、取指侧的 I-L1（各有独立 capability）。

## Requirements

### Requirement: D-L1 的握手 MUST 逐请求对齐（`ready` / `miss` 是**组合**，`rdata` 是**保持寄存器**）

`chlib::DCacheSync` 的前端 MUST 是**逐请求对齐**的真握手：

- `ready` / `miss` MUST 是**组合**输出，对**当拍 `addr`** 成立
  （⇒ 反压判定 MUST NOT 建立在延迟一拍的 `miss` 之上）
- `req` MUST 由调用方**保持**到 `ready=1`（真握手；不是「固定延迟」）
- `rdata` MUST 是**寄存器**：在**完成那拍**捕获，并**保持**到下一次读完成

⇒ ⇒ `ready` 的语义：**读** = `hit | fill_complete`；**写** = AXI `AW` 握手成立。
⇒ ⇒ 流水线层的「多拍等待」由 `data_ready` 反压表达，**不是**「延长存储器读延迟」
（`rv32i-memory-timing` 的「TCM 固定 1 拍」约束的是 **TCM 的读口**，本 capability 不改变它）。

#### Scenario: hit ⇒ `ready` 与请求**同拍**（1 拍内可判）

- **WHEN** 一个读请求命中（tag 匹配且 valid=1）
- **THEN** `ready` 在**请求拍**即为 1（不是下一拍）
- **AND** `rdata` 在**下一拍**给出该请求对应 lane 的数据

#### Scenario: `rdata` MUST 保持到下一次读完成（S2 冻结语义强制）

- **WHEN** 一条 load 在拍 N 完成（`ready=1`），随后流水线因**下一条**访存的反压被冻结
- **THEN** `rdata` MUST 在解冻那拍仍等于**拍 N 那条 load** 的数据
- **AND** MUST NOT 因 `addr` 变化而改变
  （理由：`rv32i-memory-timing` 要求「MEM/WB 寄存器 MUST 被冻结」⇒ ⇒ load 的结果
  可能要在若干拍之后才装进 MEM/WB ⇒ ⇒ 数据 MUST 可被重新读出）

#### Scenario: miss 时反压等待

- **WHEN** 一个请求未命中
- **THEN** `miss` 当拍为 1 且 `ready` MUST 保持 0 直到 burst fill 完成
- **AND** fill 完成那拍 `ready=1`，且 `rdata` 在下一拍给出目标 lane 的数据

#### Scenario: 连续两条访存 MUST NOT 错位

- **WHEN** 两条访存请求背靠背（前一条完成的当拍后一条开始）
- **THEN** 后一条的 `ready` 判定 MUST 只由**它自己的** `addr` 决定
- **AND** MUST NOT 复用前一条的结果（⇒ 反压 MUST NOT 建立在延迟信号上）

#### Scenario: fill 期间 `addr` 变化的契约

- **WHEN** fill 进行中调用方改变 `addr`（违反握手契约）
- **THEN** 已发出的 AXI 读事务 MUST 仍指向 **AR 握手那拍锁存的 index/tag**
  （⇒ 后续请求 MUST NOT 被这条 fill 污染）

### Requirement: line 的 4 个 lane MUST 各自可寻址

line = 16 B（4 × 32-bit）。`addr[3:2]` MUST 参与数据选择：读按 lane 选、
写按 lane 更新（其余 lane 保持）。

#### Scenario: 同 line 的 4 个 offset 读回不同值

- **WHEN** line 的 4 个 lane 各写入不同值后，按 offset `0` / `4` / `8` / `12` 读回
- **THEN** 4 次读回值**各不相同**，且各自等于对应 lane 的写入值

#### Scenario: 写只更新目标 lane

- **WHEN** 对 line 内某地址写入新值
- **THEN** 该 lane 更新
- **AND** line 内其余 3 个 lane **保持原值**

### Requirement: 字节写 MUST 按 `strbe` 合并（前端 MUST 有字节使能）

前端 MUST 暴露 `strbe[4]`（字节使能），且：

- **line 更新** MUST 在目标 lane 内按 `strbe` 合并（`SB` / `SH` MUST NOT 整字覆盖）
- **AXI 写** MUST 把同一 `strbe` 送到 `w_strb`（write-through 的字节语义）

#### Scenario: 字节写不破坏同字的其余字节

- **WHEN** 对某字先写整字、再对其中一个字节做 `SB`
- **THEN** 读回该字时，被写的字节是新值，其余 3 个字节仍是旧值

### Requirement: line 填充 MUST 用 4-beat burst

miss 时的 line 填充 MUST 是单次 AXI4 burst：`ARLEN=3` · `ARSIZE=2` · `ARBURST=INCR`，
地址为 line 对齐基址（`addr & ~0xF`）。

#### Scenario: burst 参数可观测

- **WHEN** 一个读 miss 发生
- **THEN** `ar_len == 3` 且 `ar_size == 2`（4 字节）且 `ar_burst == INCR`
- **AND** `ar_addr == (addr & 0xF...F0)`

#### Scenario: 4 个 beat 全部落进 line

- **WHEN** 4 个 `r` beat 依次到达（第 4 个 `r_last=1`）
- **THEN** line 的 4 个 lane 各自持有对应 beat 的数据
- **AND** 不存在「某 lane 恒为 0」的状态

### Requirement: 核接线 MUST 具备「零请求放行」与「访存请求门控」

核把 `DCacheSync` 接进 `Rv32iPipeline` 的数据通路时：

- `data_ready` MUST 为 `!data_req | cache.ready`
  （⇒ `data_ready` 是**无条件**冻结信号 ⇒ 空闲拍若被 cache 的 `ready=0` 拖低 ⇒
  **整条流水线永久冻结**）
- `pipeline.data_req` MUST 只在「MEM 级有效 且 是 load/store」时为 1
  （⇒ cache 的读有**副作用**：miss 会发 AXI burst ⇒ MUST NOT 对 ALU/NOP 发起读）

#### Scenario: 空闲拍不冻结

- **WHEN** MEM 级没有访存请求（ALU / NOP / 气泡）
- **THEN** `data_ready` MUST 为 1（流水线不得被 cache 的空闲态冻结）

#### Scenario: 非访存指令不触发 AXI 事务

- **WHEN** 程序连续执行 ALU 指令（无 load/store）
- **THEN** cache MUST NOT 发起任何 AXI 读事务
