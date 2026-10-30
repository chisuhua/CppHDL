# Capability: cpu-clint-timer

## ADDED Requirements

### Requirement: `mtime` MUST 是 64 位**自由运行计数器**

`Clint` 的 `mtime` MUST 是 64 位**自由运行计数器**：每个递增拍自增 **1**
（无符号回绕），MUST NOT 是「只能由软件写」的静态值。

⇒ ⇒ 软件 MUST 能靠读 `mtime` 得到时基（MUST NOT 需要软件自己维护计数器）。

⇒ ⇒ 递增 MUST 由 RTL（寄存器的 `next`）实现，MUST NOT 由 C++ 侧的方法实现
（C++ 侧方法**不在**仿真时序里 ⇒ 上一版被删的空 `tick()` 即此形态）。

#### Scenario: `mtime` 逐拍递增

- **WHEN** `TIMER_DIVIDER == 1` 且软件**不**写 `mtime`
- **THEN** 连续 N 个时钟沿后 `mtime` MUST 等于 N（**从 0** 起）
- **AND** `mtime` MUST NOT 保持为初值

#### Scenario: 递增 MUST 由 RTL 实现

- **WHEN** 检查 `Clint` 的实现
- **THEN** 递增 MUST 由寄存器的 `next` 表达式驱动
- **AND** MUST NOT 存在 C++ 侧的 `tick()` 之类的递增入口（它**不在**仿真时序里）

### Requirement: 递增频率 MUST 是**编译期可配置**的分频

`Clint` MUST 提供编译期常量 `TIMER_DIVIDER`（默认 **1** = 每拍递增 1）⇒
`mtime` 每 `TIMER_DIVIDER` 个时钟沿递增 1。

⇒ ⇒ **分频相位** MUST 是「复位后拍数的纯函数」：内部计数器 `d` 复位为 **0**，
递增发生在 `d == TIMER_DIVIDER - 1` 的那一拍 ⇒ ⇒ **复位后第 `TIMER_DIVIDER` 拍**
发生第一次递增，随后 `d` 归 0。

⇒ ⇒ **写拍的分频计数器 MUST 照常推进**（相位与写**无关**）⇒ ⇒ 写拍若恰是递增拍，
该次递增**被丢弃**（见「写优先」条款）⇒ ⇒ MUST NOT 存在「写复位分频器」与
「写暂停分频器」两种都能满足本契约的实现。

⇒ ⇒ `TIMER_DIVIDER` MUST 是**编译期**常量（MUST NOT 新增运行期配置端口 ——
新增输入端口 ⇒ 全部实例化点 MUST 显式驱动，而未驱动端口读到**未定义值**）。

#### Scenario: 分频相位的定义

- **WHEN** `TIMER_DIVIDER == N`（N > 1）
- **THEN** **复位后第 N 拍** MUST 发生第一次递增
- **AND** 其后每 N 拍 MUST 发生一次递增

#### Scenario: 分频相位 MUST NOT 被软件写复位

- **WHEN** 软件在任意一拍写 `mtime`
- **THEN** 分频计数器 MUST **照常推进**
- **AND** 递增发生的拍序 MUST 仍是「复位后拍数的纯函数」

#### Scenario: `TIMER_DIVIDER` MUST 是编译期常量

- **WHEN** 检查 `Clint` 的接口
- **THEN** 递增频率 MUST 由**编译期**常量决定
- **AND** MUST NOT 新增运行期配置用的**输入端口**（⇒ 未驱动端口读到未定义值）

### Requirement: 软件写 `mtime` MUST **优先于**递增

软件写 `mtime`（低半或高半）的**那一拍**：

- 被写的 32 位 MUST 取**写值**；
- 另一 32 位 MUST **保持**（MUST NOT 被清零 —— lo / hi 是**两个独立可写**寄存器）；
- 本拍 MUST **不递增**（递增被丢弃）。

⇒ ⇒ 未被写且非递增拍的拍：`mtime` 按分频递增。
⇒ ⇒ 写后的值 MUST 成为**下一次**递增的起点（=「写后从写值继续递增」）。

⇒ ⇒ 递增 MUST NOT 优先于写（MUST NOT 出现「写 50 读回 51」）。

#### Scenario: 写的那一拍取写值且不递增

- **WHEN** 软件写 `mtime` 的低半（值为 V）
- **THEN** 该拍结束后 `mtime` 的低半 MUST 等于 V
- **AND** 该拍 MUST **不**递增（⇒ 写 50 ⇒ 读回 50，**不是** 51）
- **AND** 高半 MUST **保持**原值（MUST NOT 被清零）

#### Scenario: 写后的下一拍从写值继续递增

- **WHEN** 上一拍写了 `mtime` 的低半（值为 V）且下一拍无写
- **THEN** 下一拍结束后 `mtime` MUST 等于 V + 1

#### Scenario: 写拍落在递增边界

- **WHEN** 软件写 `mtime` 的那一拍**恰是**分频的递增拍
- **THEN** 该次递增 MUST **被丢弃**
- **AND** 分频相位 MUST **不变**（⇒ 后续递增的拍序不受影响）

### Requirement: `mtime` 的复位值 MUST 是 0

`mtime` 的复位值 MUST 是 **0**；`mtimecmp` 的复位值 MUST 保持**全 1**
（`0xFFFFFFFF`）⇒ ⇒ 复位后 `mtime < mtimecmp` ⇒ ⇒ **复位后 MUST NOT** 产生
定时器中断。

⇒ ⇒ `mtip` 的**比较语义**（`mtime >= mtimecmp` ⇒ 拉高）MUST **逐项不变**。

#### Scenario: 复位后不产生定时器中断

- **WHEN** 复位后（尚未发生任何写）
- **THEN** `mtime` MUST 等于 0
- **AND** `mtimecmp` MUST 等于全 1
- **AND** `mtip` MUST 为 0

#### Scenario: 比较语义逐项不变

- **WHEN** `mtime >= mtimecmp`
- **THEN** `mtip` MUST 为 1
- **AND** 该比较 MUST 与既有实现**逐项一致**（仅 `mtime` 的取值来源变了）

### Requirement: `mtime` 的 64 位读 MUST 允许**撕裂**

软件 MUST 经 32 位 `read_data` + 地址偏移分别读 `mtime` 的低 / 高半 ⇒ ⇒
两次读之间计数器可能已进位 ⇒ ⇒ 读值**可能撕裂**。

⇒ ⇒ MUST NOT 新增分离 / 原子读端口（撕裂是 RISC-V 规范**允许**的；软件应读两次
高半检测翻转）。

⇒ ⇒ `mtimecmp` 的 R/W 语义与复位值 MUST **不变**。

#### Scenario: 分两次读高半与低半

- **WHEN** 软件先读 `mtime` 的高半、再读低半
- **THEN** 两次读之间计数器 MAY 已进位
- **AND** 读值 MAY 撕裂（⇒ 这是**已知边界**，MUST NOT 被当作缺陷）

#### Scenario: MUST NOT 新增原子读端口

- **WHEN** 检查 `Clint` 的接口
- **THEN** MUST NOT 存在分离的 / 原子的 64 位读端口
- **AND** `mtimecmp` 的 R/W 语义与复位值 MUST **不变**

### Requirement: 递增与写路径的实现 MUST 规避**已实测**的三条陷阱

实现 MUST 规避下列三条**已实测**的陷阱（来源：ADR-049 §1.4 + 实施期探针）：

- **进位 / 分频边界的判定 MUST 用硬件原语**（`select`）—— C++ 三元
  （`carry ? 1_d : 0_d`）在 `ch_bool` 上是**建图期**求值 ⇒ **恒取 0** ⇒
  计数器**永远跨不过边界**；
- **「保高半换低半」MUST 用 `bits_update<32>`** —— `<<` 在 64 位上**编译不过**
  （框架 shift 用 C++ `1 << N`）；手写 `hi | lo`（**漏移位**）⇒ **静默清零高半**；
- ⚠️ **`bits_update<32>` 在 64 位 target 上返回的是 64 位值** ⇒ ⇒ MUST 用
  **64 位**变量接收（MUST NOT 用 32 位接收 ⇒ **静默截断** ⇒ 「写高半」完全失效）。

⇒ ⇒ 上述三条 MUST 各有**判别性**用例（见对应 Scenario）。

#### Scenario: 进位 / 分频边界 MUST 用硬件原语

- **WHEN** 实现分频边界判定（`d == TIMER_DIVIDER - 1`）
- **THEN** MUST 用**硬件原语**（如 `select`）
- **AND** MUST NOT 用 C++ 三元（⇒ `ch_bool` 上是**建图期**求值 ⇒ 恒取 0 ⇒
      计数器**永远跨不过边界**）

#### Scenario: 保高半换低半 MUST 用 `bits_update<32>`

- **WHEN** 实现「写低半 / 保高半」与「写高半 / 保低半」
- **THEN** MUST 用 `bits_update<32>`
- **AND** MUST NOT 用 `<<`（64 位上**编译不过**）或手写 `hi | lo`（**漏移位**
      ⇒ **静默清零高半**）

#### Scenario: `bits_update` 的返回值宽度 MUST 是 64 位

- **WHEN** 在 64 位 target 上调用 `bits_update<32>`
- **THEN** 其返回值 MUST 用 **64 位**变量接收
- **AND** MUST NOT 用 32 位接收（⇒ **静默截断** ⇒ 「写高半」完全失效）
