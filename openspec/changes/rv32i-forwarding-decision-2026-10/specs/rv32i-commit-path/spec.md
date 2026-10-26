# Capability: rv32i-commit-path

## MODIFIED Requirements

### Requirement: load-use 冒险 MUST 被检测到，且停顿 MUST 是**保持型**

- **MODIFIED**: 「停顿 MUST NOT 是单拍脉冲式」这一 Scenario 的**前提**从
  「**无数据前推**」改为「前推的**档位 1 不覆盖 load**」—— 数据前推已接入，
  但**保守形态**下 load 的数据仍要到 WB 拍才写回 ⇒ 停顿契约**本身不变**。
  其余条款（检测点 / 保持型 / 固定延迟 / 计数器可区分）**不变**。

- **WHEN** 判定 load-use 冒险
- **THEN**「有一条 load 位于 **EX 级**」MUST 取自 **EX 级**信号，
  MUST NOT 取自 **MEM 级**信号
- **AND** 检测 MUST 比较「**EX 级** load 的 rd」与「**ID 级**消费者（当前正在
  译码的那条指令）的 rs1 / rs2」
- **AND** 停顿 MUST 是**保持型**，且 MUST 以**固定延迟**解除
  （由 load 从 EX 到 WB 的级数决定）⇒ MUST NOT 依赖 rd 匹配，
  MUST NOT 依赖「WB 级出现了写回」
- **AND**「**冒险次数**」与「**停顿拍数**」MUST 是两个**可区分**的量
  ⇒ 两者 MUST NOT 共用同一信号

#### Scenario: 检测时序 MUST 对齐「EX 级 load vs ID 级消费者」

- **WHEN** 固件里一条 `lw` 紧接着一条读同一寄存器的指令
- **THEN** 冒险检测 MUST 在「`lw` 处于 EX 级、消费者处于 ID 级」的那一拍触发
- **AND** 停顿计数器 MUST 至少 +1
- **NOTE**: 用 MEM 级信号做检测会**晚一级**（消费者的操作数此时已锁存）
  ⇒ 停顿「停了个寂寞」

#### Scenario: 保持型停顿 MUST 覆盖 load 写回的那一拍

- **WHEN** 停顿已触发（在「load 处于 EX 级」的那一拍）
- **THEN** 停顿 MUST 覆盖「load 处于 MEM 级」与「load 处于 WB 级」这两拍
- **AND** 停顿 MUST NOT **早于** load 写回解除
- **AND** 停顿 MUST 覆盖到消费者**装进 ID/EX** 的那一拍**之前**
  ⇒ 消费者进 EX 时读到的 MUST 是 load 写入的**新值**
  （⚠️ 解除条件 MUST NOT 是「按 rd 匹配」也 MUST NOT 是「等任意写回」——
  两者都已被实测证伪；见下方两个 Scenario）

#### Scenario: 停顿 MUST NOT 被「较老指令写同一 rd」提前解除

- **WHEN** 固件是「较老指令与被阻塞的 load **写同一个 rd**」
- **THEN** 停顿 MUST NOT 因那个 rd 的匹配而提前解除
- **AND** 消费者 MUST 得到 load 写入的值，MUST NOT 得到 load 之前的旧值
- **NOTE**: **实际回归固件** = word 42..44 的
  `addi x10,x0,7` → `lw x10,0x110(x0)` → `add x11,x10,x10`
  ⇒ 期望 `x11 == 0`（`[0x110]` 从未被写过 ⇒ dmem 零初始化），
  MUST NOT 是 `14`（= 读到 load 之前的旧值 7）。
  ⚠️ 上面那条 `x5`/`0x100` 的写法只是**同构示例**（`[0x100]` 在真实固件里被
  golden A 写过 = 10 ⇒ 照抄会得到 20 而不是 0）

#### Scenario: 停顿 MUST NOT 是单拍脉冲式

- **WHEN** 前推的档位 `1` **不覆盖** load（保守形态）⇒ load 的数据要到 **WB** 拍
  才写回寄存器堆
- **THEN** 停顿 MUST 持续**若干拍**（实测 3 拍），MUST NOT 只延迟 1 拍
- **AND** 消费者 MUST 在停顿解除后拿到**新值**

#### Scenario: 解除 MUST NOT 依赖「目标寄存器匹配」

- **WHEN** 较老的一条指令与被阻塞的 load **写同一个 rd**
- **THEN** 那个 rd 的匹配 MUST NOT 被当作「那条 load 写回了」
- **AND** 停顿 MUST 持续到那条 load 写回为止

#### Scenario: 解除 MUST NOT 依赖「WB 级出现了写回」

- **WHEN** 被阻塞的 load 被**冲刷**（因此永远不会写回）
- **THEN** 停顿 MUST 仍然在固定延迟后解除
- **AND** MUST NOT 永久保持（否则 ⇒ 死锁）

#### Scenario: 冒险次数与停顿拍数 MUST 分别可观测

- **WHEN** 一次 load-use 冒险持续了 N 拍
- **THEN** 「冒险次数」计数器 MUST 只 +1
- **AND** 「停顿拍数」计数器 MUST +N

### Requirement: 停顿 / 冲刷期间流水线的**有效链** MUST 保持自洽

- **MODIFIED**: 「load-use 自保 MUST 只覆盖**紧邻**（距离 1）的依赖」这一 Scenario
  的**取值结论**被更新 —— 「无数据前推 ⇒ 读旧值 ⇒ 固件 MUST 自行保证间隔」
  改为「读到**新值**（由**前推**覆盖）⇒ 固件 MUST NOT 再依赖手工插间隔」。
  ⚠️ **停顿机制本身不变**（停顿仍只覆盖**紧邻**的 load-use 依赖）；
  ⚠️ MODIFIED 会**整体替换**该 Requirement ⇒ **既有 Scenario 一条都不删**
  （只改过时的那句）。其余条款（气泡而非冻结 / 各级行为 / flush 优先）**不变**。

- **WHEN** 流水线处于停顿（stall）或冲刷（flush）状态
- **THEN** 被阻塞的指令 MUST **留在 ID 级**，MUST NOT 前进到 EX
- **AND** ID/EX 级寄存器 MUST 装载**气泡**（valid = 0），
  MUST NOT **冻结**在旧指令上
- **AND** EX/MEM 级 MUST 正常装载 EX 级的结果，MUST NOT 因停顿而冻结
- **AND** IF/ID 级与 PC MUST 冻结（取指 MUST NOT 前进）
- **AND** 消费者在停顿解除后进入 EX 时，其操作数 MUST 取自**停顿解除之后**的
  寄存器堆内容
- **AND** flush 优先于 stall（两者同时为真时 MUST 按 flush 处理）

#### Scenario: 停顿 MUST NOT 把 EX 级的 load 卡住

- **WHEN** 停顿被触发的那一拍，「处于 EX 级的那条指令」**正是**那条 load
- **THEN** EX/MEM 级 MUST 正常装载该 load 的结果
- **AND** 该 load MUST 在后续拍到达 MEM 级与 WB 级
- **AND** 该 load MUST 最终写回寄存器堆（否则停顿永远不解除 ⇒ **死锁**）

#### Scenario: ID/EX MUST 装气泡而非冻结

- **WHEN** 停顿持续多拍
- **THEN** ID/EX 的 valid MUST 在每拍都为 0
- **AND** EX 级 MUST 因此是**气泡**（不执行任何指令）
- **NOTE**: 若 ID/EX 冻结在旧指令上，则 EX 级装的是**消费者**的数据，
  而 EX/MEM 装的是 **load** 的数据 ⇒ 有效位与数据**错开一级**
  ⇒ 被阻塞的指令最终仍会被写回

#### Scenario: 消费者 MUST 拿到 load 写回后的新值

- **WHEN** 一条 load 后面紧跟一条读同一寄存器的指令，且两者之间**没有** NOP
- **THEN** 消费者的结果 MUST 等于 load 写入的值
- **AND** MUST NOT 等于该寄存器在 load 之前的旧值

#### Scenario: 停顿 MUST 只**延迟**退休（MUST NOT 丢弃指令）—— ⚠️ **当前有一处例外**

- **WHEN** 停顿发生
- **THEN** 被停顿的消费者 MUST **最终仍要退休**（停顿只是延迟）
- **AND** ⚠️ **当前实现会丢弃「消费者之后的第一条指令」**
  （imem 是 1-cycle `sread` ⇒ 该指令的地址已在停顿前提交、数据在停顿首拍返回，
  而 IF/ID 在停顿期间冻结 ⇒ 该数据无处可存 ⇒ 丢弃）
  ⇒ **固件 MUST 保证该位置是 NOP**（或接入取指 skid buffer —— 独立立项）
- **NOTE**: 该边界已登记在 `AGENTS.md` 的 KNOWN LIMITATIONS 与本 change 的
  `design.md` §3.10

#### Scenario: 退休指令数的推导 MUST 与测试断言**同义**

- **WHEN** 固件含 load-use 冒险
- **THEN** 「退休指令数」MUST 等于「**拍间隔数**（= 拍数 − 1）− **不增长**的
  拍间隔数」
  （停顿拍**不退休**，但被停顿的指令**最终仍要退休** ⇒ 停顿只是延迟）
- **AND** 该推导 MUST 与测试的断言**同义**（MUST NOT 写成与实现对不上的数字）
- **NOTE**: 数字口径（当前固件 / 200 拍）：**199** 个拍间隔 − **11** 个不增长
  ⇒ 退休 **188**

#### Scenario: load-use 自保 MUST 只覆盖**紧邻**（距离 1）的依赖

- **WHEN** 固件是 `lw x5,0x100(x0)` → `nop` → `add x6,x5,x5`
  （load 与消费者之间隔了 **1 条**指令）
- **THEN** 该依赖**不会**被停顿机制拦住
  （检测点是「**EX 级**的 load vs **ID 级**的消费者」⇒ **停顿**只覆盖**紧邻**）
- **AND** 该消费者 MUST 读到 load 写入的**新值**
  （由**前推**覆盖 —— 前推接入后，该依赖**不再**读到旧值）
- **AND** 固件 MUST NOT 再依赖「手工插 NOP 保证间隔」来避免读到旧值
