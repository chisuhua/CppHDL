# Capability: rtl-soc-output

## MODIFIED Requirements

### Requirement: SoC 层 MUST 有独立可跑的 Verilog testbench

SoC 顶层 MUST 有一个**独立**的 Verilog testbench（**不**依赖仓内仿真后端）⇒ ⇒ 它 MUST：

- 实例化 SoC 的**顶层 module**
- 驱动顶层的**输入端口**并观测**输出端口**
- **自带判据汇总**：`RESULT: checks=<N> fails=0`（`<N>` MUST ≥ **4**）
- **失败时以非零退出码结束**（⇒ ⇒ CI MUST NOT 把失败当通过）
- **由单一语义来源渲染**（**本 change 新增**）：TB MUST 由 `.verif` 验证源的 **IR** 渲染而成 ⇒ ⇒ MUST NOT 由人手写激励/断言
- **DUT 连线由 manifest 派生**（**本 change 新增**）：实例连线 MUST 由 `manifest.port_table[]` 生成 ⇒ ⇒ MUST NOT 存在手写端口映射表
- **无可解析残留**（**本 change 新增**）：TB 内 MUST NOT 出现形如 `top_unnamed_input_*` 的字面量，且 MUST NOT 留 `// TODO:`
- **新鲜度**（**本 change 新增**）：重新渲染 MUST 与原文件**逐字节相同**（⇒ ⇒ TB 是仓内工件，MUST 可重现）
- **可 review**（**本 change 新增**）：每条断言行 MUST 带 `check_id`，且 MUST 支持逐条结果输出（`RESULT: check <id> <pass|fail>`）

⇒ ⇒ ⚠️ **它 MUST NOT 依赖**仓内的仿真后端 ⇒ ⇒ 理由：后端的端口面由实现内部推导，
MUST NOT 用作交付验证的测试台。

⇒ ⇒ ⚠️ **SoC 层 TB MUST NOT 验证总线协议** ⇒ ⇒ 顶层**不暴露**总线端口（bundle 不是 Verilog 端口）
⇒ ⇒ 协议面 MUST 由 IP 级 TB 承担 ⇒ ⇒ 两者是**分工**，不是重复。

#### Scenario: SoC 层 TB 独立运行并给出判据汇总

- **WHEN** 用外部 Verilog 工具运行 SoC 层 TB
- **THEN** 它 MUST 打印 `RESULT: checks=<N> fails=0`，且 `<N>` ≥ 4
- **AND** 全部判据通过时 MUST 以退出码 **0** 结束
- **AND** 任一判据失败时 MUST 以**非零**退出码结束

#### Scenario: 生成物与手写物 MUST 可区分

- **WHEN** 检查 SoC 层 TB 的实例连线
- **THEN** 连线数与端口名 MUST 逐条等于 `manifest.port_table[]`（顺序一致）
- **AND** 文件内 MUST 存在生成器标记，且重新渲染 MUST 逐字节相同
- **AND** 若文件内出现 `top_unnamed_input_*` 字面量或 `// TODO:` ⇒ ⇒ MUST 判定为**非生成物**（不满足本条）

### Requirement: SoC 层 TB 的期望源 MUST 是仓内可审计的登记

SoC 层 TB 的期望值（端口映射 + 地址映射 + 事务序列 + **手推结果**）MUST 登记在仓内的
**期望源文件**中 ⇒ ⇒ TB 文件头 MUST 指向它 ⇒ ⇒ **TB 内的注释 MUST NOT** 是期望值的唯一载体。

⇒ ⇒ **期望值 MUST 手推** ⇒ ⇒ **MUST NOT** 抄自仿真输出 ⇒ ⇒ 「两份同源产物一致」**不是**正确性证据。

⇒ ⇒ **两层分离**（**本 change 新增**）：
- **推导层** = `.rddf/checks/soc-rtl-expectations.md`（手推叙事 + 锚点）⇒ ⇒ TB 头部的
  `EXPECT-SRC` MUST 指向它，且 MUST NOT 指向 `.verif` / `.cpp`（⇒ ⇒ 后者是机器层 ⇒ ⇒ 自证）
- **机器层** = `.verif` 的 `EXPECT` 行（机器值）⇒ ⇒ 值 MUST 是文法内**字面量表达式**，
  MUST NOT 引用实现头 / 函数 / 全局量
- ⇒ ⇒ 两层的键集合 MUST **双向 1:1**（按 `scenario` 粒度）；⚠️ **这只保证「存在」双向，不保证「数值」一致** ⇒ ⇒ 数值一致性归 process + review

⇒ ⇒ **期望源 MUST 同时登记**冻结的模块名期望集（成层 IP 的模块类型名 + 期望 module 数）
⇒ ⇒ 该集合 MUST **手推**并**冻结** ⇒ ⇒ **MUST NOT** 从「成层声明处」派生
（⇒ ⇒ 否则「漏给某个 IP 加成层声明」会同时改变期望 ⇒ ⇒ 判据**恒绿**）。

⇒ ⇒ **生成边界**（**本 change 新增**）：期望源的**期望值 / 推导叙事正文** MUST NOT 由生成器产出；
⚠️ **唯一例外** = 端口映射段（它是**端口身份**而非期望值）⇒ ⇒ MAY 由生成器产出。

#### Scenario: 期望源被登记且被 TB 引用

- **WHEN** 检查 SoC 层 TB
- **THEN** TB 文件头 MUST 指向仓内期望源文件的一个**存在的锚点**
- **AND** 期望源 MUST 含端口映射 + 地址映射 + 事务序列 + 手推结果 + 冻结的模块名期望集

#### Scenario: 锚点与 EXPECT 双向 1:1

- **WHEN** 期望源里存在一个未被任何 `.verif` `EXPECT` 覆盖的 scenario 锚点
- **THEN** MUST 判定为不满足（悬空锚点）
- **AND** 反之，`.verif` 里存在未被任何锚点覆盖的 scenario ⇒ ⇒ 同样 MUST 判定为不满足
- **AND** 期望源正文（非端口映射段）MUST NOT 带生成器标记

## ADDED Requirements

### Requirement: SoC 层验证判据 MUST 有单一语义来源（IR）

SoC 层验证判据 MUST 有**唯一语义来源**（中间表示 IR）⇒ ⇒ 所有执行后端 MUST 消费**同一份 IR**：

- **IR 解释器**（驱动仓内 `Simulator`）⇒ ⇒ 用于逻辑迭代与快速回归
- **Verilog 渲染器** ⇒ ⇒ 产出交付验证层 TB

⇒ ⇒ MUST NOT 让两个后端**各自解释**一份判据（例如「C++ 宏展开 + 另一套扫描器」）⇒ ⇒
那是**两个语义实现** ⇒ ⇒ 漂移必然。

⇒ ⇒ **时序锚点 MUST 在 IR 层唯一确定**：驱动发生在「沿后采样窗」内（不消耗拍）；
观测发生在「最近一次结算之后」⇒ ⇒ Verilog 侧 MUST 统一为 `@(posedge clk); #1;`
⇒ ⇒ **MUST NOT 用 `#0`**（它在 inactive region，非阻塞赋值未结算 ⇒ ⇒ 两后端必然分叉）。

⇒ ⇒ **序列约束 MUST fail-closed**：自上次结算以来存在驱动动作时，MUST 拒绝观测断言
（该情形不可表达，而非「约定一致」）。

#### Scenario: 两个后端对同一 IR 给出同一结论

- **WHEN** 对同一 `.verif` 分别运行 IR 解释器与渲染出的 `.sv`
- **THEN** 两侧的 `check_id` 集合 MUST 相等，且各自结论 MUST 相同
- **AND** 寄存器输出在 `TICK(1)` 后立即被观测的构例 MUST 两侧同结论（⇒ ⇒ 用于证伪 `#0` 类错误）

#### Scenario: 后端自行解释一份判据即违反

- **WHEN** 某个后端（如 C++ 侧）不消费 IR、而是用预处理器展开另一套语义
- **THEN** 该实现 MUST 被判定为**不满足**本条
