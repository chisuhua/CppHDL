# 变更规格：RV32I 取指的 skid buffer 与填充期语义（ADR-048）

## MODIFIED Requirements

### Requirement: 「退休指令数」MUST 由流水线的**有效链**定义

- **MODIFIED**: 本 Requirement 的**契约面**扩展 —— 新增一条 Scenario
  「取指填充期 MUST NOT 把同一条指令装进流水线两次」。
  理由：取指填充期的「同一条指令被装两次」**违反**本 Requirement 的
  「退休指令数 MUST 由**有效链**定义」（该指令被计了**两次**退休 ⇒ 计数 ≠ 有效链）。
  ⚠️ MODIFIED 会**整体替换**该 Requirement ⇒ **既有 Scenario 一条都不删**
  （「ALU 指令必须计入退休数」/「有效链 MUST 逐级传递」**逐字保留**）；
  ⚠️ 既有条款（WHEN/THEN/AND 全部）**逐字保留**，本 delta 只**追加**一条 Scenario
  ⇒ 架构依据 = `docs/adr/ADR-048-rv32i-fetch-skid-buffer.md`（D6 / D9.2）。

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

#### Scenario: 取指填充期 MUST NOT 把同一条指令装进流水线两次

- **WHEN** 复位撤除后的**首个取指周期**
- **THEN** 同一条指令 MUST NOT 被装进流水线**两次**
  （⇒ 退休数 MUST NOT 把同一条指令计**两次**）
- **AND** 该指令的**写回** MUST 只发生**一次**
- **AND** 实现 MUST 用「取指数据**已就绪**」的判据来区分「已真正取指」与
  「尚未取指」
  （⚠️ imem 是 **1-cycle `sread`** ⇒ 复位后**首拍**返回的数据**不是**取指结果
  ⇒ 该判据 MUST NOT 依赖 `instr_ready` —— 它恒为 1，不携带信息）
- **NOTE**: 现状**违反**本条：固件的第一条指令被写回两次（连续两拍 `wb_write_en=1`，
  `rd` / `data` 相同）⇒ 退休数 **+1**；⚠️ 该重复在**接上数据前推**（ADR-047）后
  **不再恒幂等**（第二条会读到第一条的前推值 ⇒ 若该指令自依赖则**非幂等**）
  ⇒ 架构依据 = ADR-048 D6（`primed` 门控）；修复后退休数 **−1**。

---

### Requirement: 停顿 / 冲刷期间流水线的**有效链** MUST 保持自洽

- **MODIFIED**: 「load-use 自保 MUST 只覆盖**紧邻**（距离 1）的依赖」这一 Scenario
  的**取值结论**被更新 —— 「无数据前推 ⇒ 读旧值 ⇒ 固件 MUST 自行保证间隔」
  改为「读到**新值**（由**前推**覆盖）⇒ 固件 MUST NOT 再依赖手工插间隔」。
  ⚠️ **停顿机制本身不变**（停顿仍只覆盖**紧邻**的 load-use 依赖）；
  ⚠️ MODIFIED 会**整体替换**该 Requirement ⇒ **既有 Scenario 一条都不删**
  （只改过时的那句）；
  ⚠️ 前推的**距离矩阵**（距离 1 / 2 / 3 / ≥ 4 各自的取值来源）MUST NOT 写在本
  capability ⇒ 那是 `rv32i-data-forwarding` 的职责（**单一权威**）⇒ 本条只**引用**。
  ⚠️ 该 Scenario 的**标题**沿用既有措辞（openspec 的 MODIFIED 规则要求既有
  Scenario **一条都不删** ⇒ 标题**不可**改写）⇒ 「距离」的**消歧**放在**正文**。
  其余条款（气泡而非冻结 / 各级行为 / flush 优先）**不变**。
  **本轮追加（ADR-048）**：
  ① 追加一条条款「IF/ID MUST NOT 因停顿而丢弃**已到达**的指令」；
  ② 「停顿 MUST 只**延迟**退休」这一 Scenario 的**例外被消除**（取指 skid buffer
  ⇒ ADR-048 D1–D5）⇒ 固件 MUST NOT 再依赖「停顿点之后那条指令是 NOP」；
  ③ 「退休指令数的推导」这一 Scenario 的 **NOTE 数字基线订正**
  （188 → 实现前的 **179**（199 − 20）→ **实现后的 178（199 − 21）**；
  数字由**实测**回填并与测试断言**同批**更新，见该 NOTE）；
  ⚠️ 「停顿 MUST 只**延迟**退休」这一 Scenario 的**标题**含
  「⚠️ **当前有一处例外**」⇒ 该短语是**过时措辞**，按 MODIFIED 规则**不可**改写
  ⇒ 消歧写在**正文**（见该 Scenario 的 NOTE）。

- **WHEN** 流水线处于停顿（stall）或冲刷（flush）状态
- **THEN** 被阻塞的指令 MUST **留在 ID 级**，MUST NOT 前进到 EX
- **AND** ID/EX 级寄存器 MUST 装载**气泡**（valid = 0），
  MUST NOT **冻结**在旧指令上
- **AND** EX/MEM 级 MUST 正常装载 EX 级的结果，MUST NOT 因停顿而冻结
- **AND** IF/ID 级与 PC MUST 冻结（取指 MUST NOT 前进）
- **AND** 消费者在停顿解除后进入 EX 时，其操作数 MUST 取自**停顿解除之后**的
  寄存器堆内容
- **AND** flush 优先于 stall（两者同时为真时 MUST 按 flush 处理）
- **AND** IF/ID MUST NOT 因停顿而**丢弃已到达**的指令
  （「已到达」= 该指令的取指地址已提交、数据已返回，而 IF/ID 因停顿未能装载
  ⇒ 该指令 MUST 被保留，并在停顿解除后按**程序顺序**进入流水线）

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
- **AND** 停顿 MUST NOT 丢弃**任何**指令
  （「消费者之后的那条指令」在取指 skid buffer 落地后 MUST 同样保留
  ⇒ 见上一条款「IF/ID MUST NOT 因停顿而丢弃已到达的指令」）
- **AND** 固件 MUST NOT 再依赖「load-use 停顿点之后的那条指令是 NOP」
  （该位置曾被丢弃 ⇒ 现 MUST 被保留 ⇒ 放 NOP 不再是**必需**，但仍是**允许**的）
- **NOTE**: ⚠️ **本 Scenario 的标题含「当前有一处例外」是过时措辞**（openspec 的
  MODIFIED 规则要求既有 Scenario **一条都不删** ⇒ 标题**不可**改写）
  ⇒ **该例外已消除**：取指侧引入 skid buffer（1 级）承接停顿期间已到达的指令
  ⇒ 架构依据 = `docs/adr/ADR-048-rv32i-fetch-skid-buffer.md`（D1–D5）；
  ⚠️ 该边界此前登记在 `AGENTS.md` 的 KNOWN LIMITATIONS 与
  `openspec/changes/archive/2026-10-26-rv32i-pipeline-commit-path-2026-10/design.md` §3.10。

#### Scenario: 退休指令数的推导 MUST 与测试断言**同义**

- **WHEN** 固件含 load-use 冒险
- **THEN** 「退休指令数」MUST 等于「**拍间隔数**（= 拍数 − 1）− **不增长**的
  拍间隔数」
  （停顿拍**不退休**，但被停顿的指令**最终仍要退休** ⇒ 停顿只是延迟）
- **AND** 该推导 MUST 与测试的断言**同义**（MUST NOT 写成与实现对不上的数字）
- **AND** 该推导 MUST 覆盖「取指填充期」的重复退休
  （⇒ 填充期的那条指令 MUST 只计一次，见「退休指令数」Requirement 的
  「取指填充期 MUST NOT 把同一条指令装进流水线两次」）
- **NOTE**: 数字口径由**实测**回填并与测试断言**同批**更新
  （MUST NOT 预先断言）⇒ **本 change 实现后**的基线：
  **199** 个拍间隔 − **21** 个不增长 ⇒ 退休 **178**
  （`examples/riscv-mini/tests/test_rv32i_soc_dual_parity.cpp:907-908`）；
  ⚠️ **该基线此前写的是 188（199 − 11）⇒ 自 2026-10-26 的 JAL/JALR change 起
  就已与测试脱节** ⇒ 本条先订正为实现前的 **179（199 − 20）**，再按本 change 的
  实测值订正为 **178（199 − 21）** ⇒ 归档前 MUST 核对二者与测试断言**逐字一致**。
  ⚠️ 更稳的锚点是 `non_retired` 的**分段分解**（fill / stall / flush 各几拍）：
  取指 skid buffer **不改**停顿拍数与冲刷拍数 ⇒ 停顿 / 冲刷段 MUST 不变
  （实测：fill 段 2 → **3** 拍 —— `primed` 门控消掉填充期的重复退休；
  停顿段 **6** 拍、冲刷段 **12** 拍 ⇒ 3 + 6 + 12 = **21**）。

#### Scenario: load-use 自保 MUST 只覆盖**紧邻**（距离 1）的依赖

- **WHEN** 固件是 `lw x5,0x100(x0)` → `nop` → `add x6,x5,x5`
  （load 与消费者之间隔了 **1 条**指令 ⇒ 按 `rv32i-data-forwarding` 的定义
  **生产者与消费者的指令距离 = 2**）
- **THEN** 该依赖**不会**被停顿机制拦住
  （检测点是「**EX 级**的 load vs **ID 级**的消费者」⇒ **停顿**只覆盖**紧邻**）
- **AND** 该消费者 MUST 读到 load 写入的**新值**
  （由**前推**覆盖 —— 前推接入后，该依赖**不再**读到旧值）
- **AND** 固件 MUST NOT 再依赖「手工插 NOP 保证间隔」来避免读到旧值
- **NOTE**: ⚠️ **本 Scenario 的标题沿用既有措辞**（openspec 的 MODIFIED 规则要求
  既有 Scenario **一条都不删** ⇒ 标题**不可**改写）⇒ **标题里的「距离 1」与
  正文的形状（距离 2）不一致**是**既有缺陷** ⇒ 消歧以**正文**与
  `rv32i-data-forwarding` 的距离矩阵为准
  ⇒ ⇒ **距离矩阵是唯一权威**（本 capability MUST NOT 自述矩阵）
