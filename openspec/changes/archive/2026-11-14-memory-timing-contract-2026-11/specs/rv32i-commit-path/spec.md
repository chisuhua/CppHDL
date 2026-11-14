# Capability: rv32i-commit-path

## MODIFIED Requirements

### Requirement: 「退休指令数」MUST 由流水线的**有效链**定义

- **MODIFIED**: 本 Requirement 的**契约面**再次扩展 —— 在既有内容之上追加
  一条 Scenario「时序基线修订后的退休数与停顿拍数」。
  理由：MEM/WB 装载点后移一拍 ⇒ ⇒ **load-use 停顿 3 → 4 拍** ⇒ ⇒ 退休数与停顿计数
  的**基线值**随之变化 ⇒ ⇒ 本 Requirement 的基线 MUST 随之订正。
  ⚠️ MODIFIED 会**整体替换**该 Requirement ⇒ **既有 Scenario 一条都不删**
  （「ALU 指令必须计入退休数」/「有效链 MUST 逐级传递」/「取指填充期 MUST NOT 把同一条指令
  装进流水线两次」**逐字保留**）；
  ⚠️ 既有条款（WHEN/THEN/AND 全部）**逐字保留**，本 delta 只**追加**一条 Scenario
  ⇒ ⇒ 架构依据 = `docs/adr/ADR-069-mem-data-alignment.md`（**Accepted** · 决策 = A0）
  与 `docs/adr/ADR-047-rv32i-data-forwarding.md` §5.5（dated addendum）。

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

#### Scenario: 时序基线修订后的退休数与停顿拍数

- **WHEN** MEM/WB 装载点后移一拍后统计同一份固件的退休数与停顿拍数
- **THEN** 退休数 MUST 等于该基线下**手推**得到的值（**禁**抄仿真输出）
- **AND** `load_use_stalls` MUST 等于该基线下**手推**得到的值
- **AND** 任何基线值的变更 MUST 在 capability 的**验证记录**里给出**手推推导**
  （⇒ ⇒ 两后端同源 ⇒ ⇒ parity 对该族缺陷**结构性失明** ⇒ ⇒ parity 不构成证据）
