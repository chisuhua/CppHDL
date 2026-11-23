## ADDED Requirements

### Requirement: D-L1 的 hit / miss MUST 由 `ready` 表达（与流水线反压配合）

`chlib::DCacheSync` 的 `ready` MUST 区分两种情形：

- **hit** ⇒ `ready` 与 `rdata` 同拍有效（**1 拍**，与 `DataTCM` 的 1 拍寄存读对齐）
- **miss** ⇒ `ready` MUST 保持 0 直到 burst fill 完成

⇒ ⇒ 流水线层看到的「多拍等待」由 `data_ready` 反压表达，**不是**「延长存储器读延迟」
（`rv32i-memory-timing` 的「TCM 固定 1 拍」约束的是 **TCM 的读口**，本 capability 不改变它）。

#### Scenario: hit 时 1 拍返回

- **WHEN** 一个请求命中（tag 匹配且 valid=1）
- **THEN** `ready` 在请求后 **1 拍**有效
- **AND** `rdata` 是同拍的对应 lane 数据

#### Scenario: miss 时反压等待

- **WHEN** 一个请求未命中
- **THEN** `ready` MUST 保持 0 直到 fill 完成
- **AND** fill 完成后 `ready` 有效且 `rdata` 正确

#### Scenario: miss 期间地址可变性的契约不变

- **WHEN** `miss` 有效期间调用方改变 `addr`
- **THEN** 已发出的 AXI 读事务 MUST 仍指向 **锁存的 line 基址**（不跟随新 `addr`）

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
