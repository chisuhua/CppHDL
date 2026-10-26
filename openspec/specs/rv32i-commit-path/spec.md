# rv32i-commit-path Specification

## Purpose

规定 RV32I 5 级流水线的 **commit / valid 通路**契约 —— 即「一条指令在什么时刻
**真正生效**（退休 / 写回）」，以及「各级流水线寄存器之间 MUST NOT 串级」。

本 capability 源于 change `rv32i-pipeline-commit-path-2026-10`（2026-10-26 归档），
该 change 在为 RV32I SoC 建立**可观测端口契约**（ADR-046 **D10-rev**，32 端口）后，
逐拍追出并修复了 **17 个流水线缺陷**（`mem_valid_out` 语义错 ⇒ ALU 指令永不写回；
冒险单元的 MEM 级输入取自 ID 级 ⇒ load-use 检测退化成「与自身比较」；级间信号
串一级 ⇒ 有效位与数据错开 ⇒ 被冲刷的指令仍被写回；…）。

覆盖 6 个方面：

1. **退休语义** —— 「退休指令数」MUST 由**有效链**定义（MEM 级持有有效指令），
   MUST NOT 由「load 数据有效」定义（那会把退休数变成 load 次数）。
2. **级间信号不串级** —— EX/MEM、MEM/WB MUST 装载**与该级同拍**的信号；
   冒险单元的 MEM 级输入 MUST 取自 MEM 级。
3. **观测面契约** —— SoC 顶层 MUST 暴露可观测端口（取指地址流 / 存储流 / 性能
   计数器 / 有效链 / 诊断面），由单源化护栏的 C6 快照钉住（名 + 方向 + 位宽）。
4. **双后端比对** —— SoC 级双后端逐周期比对 MUST 覆盖**全部**输出观测值；
   「某一拍某信号无意义」这一事实 MUST 由**契约**保证，MUST NOT 由「实现碰巧
   一致」来蒙混。
5. **load-use 保持型停顿** —— 检测点 MUST 是「**EX 级**的 load vs **ID 级**的
   消费者」；停顿 MUST 以**固定延迟**解除（MUST NOT 依赖 rd 匹配 —— 那无法区分
   「那条 load 写回了」与「更老的指令写回了同一 rd」；MUST NOT 依赖「WB 级出现
   了写回」—— load 被冲刷时会死锁）；「冒险次数」与「停顿拍数」MUST 可区分。
6. **停顿 / 冲刷期间的有效链自洽** —— 被阻塞的指令 MUST 留在 ID 级；ID/EX MUST
   装**气泡**而非冻结；EX/MEM MUST 照常装载（否则 EX 级的 load 卡住 ⇒ 死锁）；
   气泡 MUST 清零控制信号（残留 `is_store` ⇒ 静默误写 dmem）。

⚠️ **已知边界**（如实登记，见 `AGENTS.md` 的 KNOWN LIMITATIONS）：当前实现会丢弃
「消费者之后的第一条指令」（imem 的 1 拍 `sread` + IF/ID 冻结 ⇒ 已提交未锁存的
那条无处可存）⇒ 固件 MUST 用 NOP 填充该位置（已有 compile-time 静态护栏）；
正解需**取指 skid buffer + PC 回退** ⇒ 独立立项。

## Requirements

### Requirement: 「退休指令数」MUST 由流水线的**有效链**定义

- **WHEN** 统计已退休指令数（`perf_instructions` / `commit_count`）
- **THEN** 「本拍有一条指令退休」MUST 等价于「MEM 级持有**有效**指令」
  （即 EX/MEM 有效寄存器为真）
- **AND** MUST NOT 等价于「本拍有一条 **load** 的数据有效」
  （`mem_valid_out` 是 MEM/WB 与 WB 的**指令有效**信号，不是「load 数据有效」信号）
- **AND** 该计数器 MUST 对 **ALU / load / store / branch** 指令一视同仁
  （⚠️ **taken 分支本身 MUST 退休**：flush 只杀「**比分支年轻**」的指令 ⇒
  分支自身正常走完 MEM/WB 并计入退休数）

#### Scenario: ALU 指令必须计入退休数

- **WHEN** 固件只含 ALU 指令（无 load）
- **THEN** 退休计数 MUST 等于被执行（未被冲刷）的 ALU 指令条数
- **AND** MUST NOT 等于 0（那是「只有 load 才退休」的症状）

#### Scenario: 有效链 MUST 逐级传递

- **WHEN** EX/MEM 有效寄存器为真
- **THEN** MEM/WB 有效寄存器 MUST 在下一拍为真
- **AND** WB 的 `write_en` MUST 以「该指令有效」为前提

### Requirement: 流水线级寄存器 MUST NOT 装载**其它级**的信号

- **WHEN** 一个 EX/MEM 或 MEM/WB 级寄存器被装载
- **THEN** 它 MUST 装载**与该寄存器同拍**的级（EX / MEM）的输出
- **AND** MUST NOT 装载 ID 级的输出（ID 级的输出比 EX/MEM 级**早一拍**）
- **AND** 冒险单元的「MEM 级」输入（`mem_rd_addr` / `mem_reg_write`）
  MUST 取自 **MEM 级**（EX/MEM 寄存器），MUST NOT 取自 ID 级
- **AND** 流水线的 load-use 检测 MUST 走 **EX 级**输入（`ex_is_load`），
  `mem_is_load` 在流水线装配中**故意不连接**（见下方 R5）

#### Scenario: 若使用 MEM 级检测路，其输入 MUST 来自 MEM 级

- **WHEN** 冒险单元的 **MEM 级**检测路被启用（**独立组件**用法；
  流水线装配**不**启用它 —— 见 R5）
- **THEN** 「有一条 load 在 MEM 级」这一事实 MUST 来自 **MEM 级**信号
- **AND** MUST NOT 来自「当前 ID 级指令自己是不是 load」
  （后者会让检测退化成「指令与自身比较」）

#### Scenario: MEM/WB 寄存器 MUST 装载 MEM 级的指令属性

- **WHEN** MEM/WB 寄存器记录「这条指令是 ALU / jump」
- **THEN** 它 MUST 记录 **MEM 级**那条指令的属性
- **AND** MUST NOT 记录 ID 级那条指令的属性

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

### Requirement: SoC 级双后端比对 MUST 覆盖**全部**观测值

- **WHEN** 对同一固件跑 interpreter 与 verilator 两个后端
- **THEN** 每拍 MUST 比对**全部**观测值（不得用掩码跳过任何一项）
- **AND** 「某一拍某信号无意义（don't-care）」这一事实 MUST 由**契约**保证
  （例如写使能为假时地址/数据无意义 ⇒ 契约 MUST 规定该拍不参与比对），
  MUST NOT 由「实现碰巧一致」来蒙混

#### Scenario: 掩码 MUST 有非空断言

- **WHEN** 比对逻辑对某些拍跳过某些观测值
- **THEN** 测试 MUST 断言「被比对的拍数 ≥ 固件中相关指令条数」
  （否则掩码可能把整条路径掩掉而测试仍绿）

#### Scenario: 架构级与寄存器级观测 MUST 同时一致

- **WHEN** 双后端逐周期比对
- **THEN** 架构级观测（取指地址流 + 计数器）MUST 逐拍一致
- **AND** 流水线寄存器裸值（存储地址 / 写数据）MUST 逐拍一致
  —— 后者是「后端实现等价」的判据，前者只是「功能一致」的判据

### Requirement: load-use 冒险 MUST 被检测到，且停顿 MUST 是**保持型**

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

- **WHEN** 在**无数据前推**的前提下，load 的数据要到 **WB** 拍才写回寄存器堆
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
  （检测点是「**EX 级**的 load vs **ID 级**的消费者」⇒ 只能覆盖**紧邻**）
- **AND** 在**无数据前推**的前提下，该消费者 MUST 读到**旧值**
  ⇒ 固件 MUST 自行保证依赖指令的**间隔**（或接入数据前推 —— ADR-046 D5）
