# rtl-soc-output Specification

## Purpose
定义 CppHDL **SoC 层 RTL 产物**的交付契约 —— 一份「多 IP 装配」的设计产出**什么形态**的
Verilog、以及它的**独立测试台**要满足什么条件：

- **产物形态**：SoC 的交付产物 MUST 是**层次化**产物（每个**显式成层**的 IP 一个独立
  `module`）⇒ ⇒ `module` 数 MUST 等于「**成层 IP 实例数 + 1**」（+1 = 根 `module`）
  ⇒ ⇒ MUST NOT 用「全部 inline 成 1 个 `module`」的形态交付（那种形态与单 IP 产物
  **无结构区别** ⇒ ⇒「SoC」这个形态在产物里不存在）。
- **顶层端口面**：SoC 顶层端口表 MUST 等于「`default_clock` → `default_reset` → **根组件
  自有 `__io`**」⇒ ⇒ **后代组件的 `ch_out` MUST NOT** 出现在顶层端口表
  ⇒ ⇒ 顶层端口的来源类别 MUST 只有**框架基础设施**与**自身**。
- **可发射性**：SoC 装配的**每一个后代组件输入端口** MUST 有驱动源
  ⇒ ⇒ 未驱动的后代输入 MUST 使发射 **fail-closed**（返回失败 + 点名该端口 +
  **不产出任何文件**）⇒ ⇒ MUST NOT 静默降格为顶层端口 / 体内未驱动 wire / tie-off。
  ⚠️ **用户主动 tie-off**（接常量）⇒ ⇒ 该输入**有内部 driver** ⇒ ⇒ 不触发本条。
- **独立可跑 TB**：SoC 层 MUST 有**独立可跑的 Verilog testbench**
  ⇒ ⇒ MUST 输出 `RESULT: checks=<N> fails=0` ⇒ ⇒ 失败 MUST **非零退出**。
- **期望源登记**：SoC 层的期望值 MUST 是**仓内可审计的登记**（端口映射 + 地址映射 +
  事务序列 + **手推结果**）⇒ ⇒ TB 头 MUST 指向它 ⇒ ⇒ **禁**把被测实现写成出处
  （那是**转录**不是推导 ⇒ ⇒ 循环论证）。
- **覆盖登记**：SoC 层覆盖 MUST 用**独立 kind**（`kind=soc`）⇒ ⇒ MUST 与「首批覆盖面」
  **分桶**（不混进首批 10 的计数与 `status` 判定面）。
- **双形态一致性**：SoC 产物与**扁平产物** MUST 可机械判定为**逐拍全等**
  ⇒ ⇒ 端口集合 MUST 由**产物**派生（禁手写）⇒ ⇒ 观测集 MUST 含**至少 2 个随激励变化**
  的观测量 ⇒ ⇒ ⚠️ **这只是 parity，不是正确性**（两侧同源 ⇒ 对「发射底座算错」结构性失明）
  ⇒ ⇒ 正确性锚点 MUST 落在**手推断言**上。
- **观测盲区**：SoC 交付物 MUST **显式登记**其观测盲区（层次化只覆盖**顶层可见行为**
  ⇒ ⇒ 成层子树的 `ch_out` 在顶层不可观测）。
- **能力边界**：SoC 交付物 MUST **明文声明**其不支持的能力（例如「CPU 经 AXI 访问外部
  存储」）⇒ ⇒ 不得让下游误以为支持。

## Requirements

### Requirement: SoC 层的 RTL 产物 MUST 是层次化的，且 IP 边界 MUST 可见

一个 SoC 顶层（由**多个** IP 组件装配而成）的交付产物 MUST 是**层次化**产物 ⇒ ⇒
**每个被声明为成层的 IP MUST 成为一个独立 module** ⇒ ⇒ 产物的 module 数 MUST 等于
「**成层 IP 实例数 + 1**」（+1 = 根 module）。

⇒ ⇒ **不得**用「全部 inline 成 1 个 module」的形态交付 SoC ⇒ ⇒ 理由：该形态与 IP 级产物
**无结构区别** ⇒ ⇒ 「SoC」这个形态在产物中不存在。

⇒ ⇒ ⚠️ **通用条款引用**：「成层组件 MUST 形成独立 module」等条款归 capability
`rtl-hierarchical-output` ⇒ ⇒ 本条**不重述**它们 ⇒ ⇒ 只增加 **SoC 专属**的
「module 数 == 成层 IP 数 + 1」这一可机械判定。

#### Scenario: SoC 产物的 module 数等于成层 IP 数加一

- **WHEN** 发射一个含 4 个成层 IP 的 SoC 顶层
- **THEN** 产物 MUST 含 **5** 个 module（4 个 IP + 根 module）
- **AND** 根 module 的名字 MUST 是**发射入口配置的名字**（默认 `top`）⇒ ⇒ **MUST NOT** 是组件类名
- **AND** 每个 IP 的 module 类型名 MUST **逐字**出现在产物中

### Requirement: SoC 顶层端口面 MUST 是「框架基础设施 + 根组件自有 IO」，且 MUST NOT 含冒泡端口

SoC 顶层产物的端口表 MUST 等于：`default_clock` → `default_reset` → **根组件自有 `__io`**
（无内部 driver 的 `ch_in` + 全部 `ch_out`）⇒ ⇒ **后代组件的 `ch_out` MUST NOT** 出现在顶层端口表。

⇒ ⇒ 顶层端口的**来源类别** MUST 只有两种：**框架基础设施**（clock/reset）与**自身**（`Self`）；
⇒ ⇒ **冒泡**来源（后代悬空输入被提升到顶层）的端口数 MUST 为 **0**。

⇒ ⇒ ⚠️ **通用条款引用**：「顶层端口表」与「端口来源类别」的通用定义归 capability
`rtl-hierarchical-output` ⇒ ⇒ 本条**不重述**它们 ⇒ ⇒ 只增加 **SoC 专属**的
「SoC 顶层 MUST NOT 含冒泡端口」这一可机械判定。

#### Scenario: SoC 顶层端口表不含冒泡端口

- **WHEN** 发射一个 SoC 顶层
- **THEN** 顶层 module 的端口来源类别 MUST 只含**框架基础设施**与**自身**
- **AND** 顶层端口数 MUST 等于「框架基础设施数 + 根组件自有 `__io` 数」
- **AND** 端口表中 MUST NOT 出现任何后代 IP 的端口名

### Requirement: SoC 设计的后代输入 MUST 全部有驱动源，否则发射 MUST 失败

SoC 顶层装配的**每一个后代组件输入端口** MUST 有驱动源 ⇒ ⇒ 二选一：
① 接到上游 IP 的输出（真实互联）② 由父层给出**常量**（tie-off）。

⇒ ⇒ **未驱动的后代输入 MUST 使发射 fail-closed**（返回失败 + 点名该端口 + **不产出任何文件**）
⇒ ⇒ MUST NOT 静默降格为顶层端口、体内未驱动 wire 或 tie-off。

⇒ ⇒ ⚠️ **tie-off 与 fail-closed 是相反动作**：本条约束的是**发射器 MUST NOT** 替用户接常量；
用户**主动**接常量（tie-off）⇒ ⇒ 该输入**有内部 driver** ⇒ ⇒ 它**不是**未驱动输入 ⇒ ⇒ 不触发本条。

#### Scenario: 未驱动的后代输入使发射失败

- **WHEN** 一个 SoC 设计含一个未被驱动的后代组件输入端口
- **THEN** 层次化发射 MUST 返回失败
- **AND** 失败信息 MUST 点名该端口
- **AND** **MUST NOT** 产出任何产物文件

#### Scenario: 用户主动 tie-off 的后代输入可正常发射

- **WHEN** 用户把一个后代组件输入端口接到常量
- **THEN** 该端口 MUST 被视为**有驱动源**
- **AND** 层次化发射 MUST 成功
- **AND** 该端口 MUST NOT 出现在顶层端口表

### Requirement: SoC 层 MUST 有独立可跑的 Verilog testbench

SoC 顶层 MUST 有一个**独立**的 Verilog testbench（**不**依赖仓内仿真后端）⇒ ⇒ 它 MUST：

- 实例化 SoC 的**顶层 module**
- 驱动顶层的**输入端口**并观测**输出端口**
- **自带判据汇总**：`RESULT: checks=<N> fails=0`（`<N>` MUST ≥ **4**）
- **失败时以非零退出码结束**（⇒ ⇒ CI MUST NOT 把失败当通过）

⇒ ⇒ ⚠️ **它 MUST NOT 依赖**仓内的仿真后端 ⇒ ⇒ 理由：后端的端口面由实现内部推导，
MUST NOT 用作交付验证的测试台。

⇒ ⇒ ⚠️ **SoC 层 TB MUST NOT 验证总线协议** ⇒ ⇒ 顶层**不暴露**总线端口（bundle 不是 Verilog 端口）
⇒ ⇒ 协议面 MUST 由 IP 级 TB 承担 ⇒ ⇒ 两者是**分工**，不是重复。

#### Scenario: SoC 层 TB 独立运行并给出判据汇总

- **WHEN** 用外部 Verilog 工具运行 SoC 层 TB
- **THEN** 它 MUST 打印 `RESULT: checks=<N> fails=0`，且 `<N>` ≥ 4
- **AND** 全部判据通过时 MUST 以退出码 **0** 结束
- **AND** 任一判据失败时 MUST 以**非零**退出码结束

### Requirement: SoC 层 TB 的期望源 MUST 是仓内可审计的登记

SoC 层 TB 的期望值（端口映射 + 地址映射 + 事务序列 + **手推结果**）MUST 登记在仓内的
**期望源文件**中 ⇒ ⇒ TB 文件头 MUST 指向它 ⇒ ⇒ **TB 内的注释 MUST NOT** 是期望值的唯一载体。

⇒ ⇒ **期望值 MUST 手推** ⇒ ⇒ **MUST NOT** 抄自仿真输出 ⇒ ⇒ 「两份同源产物一致」**不是**正确性证据。

⇒ ⇒ 期望源 MUST 同时登记**冻结的模块名期望集**（成层 IP 的模块类型名 + 期望 module 数）
⇒ ⇒ 该集合 MUST **手推**并**冻结** ⇒ ⇒ **MUST NOT** 从「成层声明处」派生
（⇒ ⇒ 否则「漏给某个 IP 加成层声明」会同时改变期望 ⇒ ⇒ 判据**恒绿**）。

#### Scenario: 期望源被登记且被 TB 引用

- **WHEN** 检查 SoC 层 TB
- **THEN** TB 文件头 MUST 指向仓内期望源文件的一个**存在的锚点**
- **AND** 期望源 MUST 含端口映射 + 地址映射 + 事务序列 + 手推结果 + 冻结的模块名期望集

### Requirement: SoC 层覆盖登记 MUST 用独立 kind，且 MUST 与「首批覆盖面」分桶

SoC 层产物的覆盖登记 MUST 使用**独立的 kind** ⇒ ⇒ MUST NOT 混入「首批覆盖面」的计数
⇒ ⇒ 清单 MUST 同时声明**两个锚点**：首批覆盖面数与 SoC 数 ⇒ ⇒ 校验 MUST **按 kind 分桶**判定。

⇒ ⇒ 校验 MUST 额外断言：**每一行的 kind MUST 属于已知集合** ⇒ ⇒ 且各桶之和 MUST 等于数据行总数
⇒ ⇒ 理由：否则「未知 kind」的行**两个桶都不计** ⇒ ⇒ **静默缩面**。

⇒ ⇒ ⚠️ 「首批覆盖面」的口径 = `kind ∈ {target, dut}` 的行数 ⇒ ⇒ **MUST NOT** 写成「清单总行数」。

#### Scenario: 未知 kind 不得静默通过

- **WHEN** 清单里有一行的 kind 不属于已知集合
- **THEN** 校验 MUST 失败
- **AND** 失败信息 MUST 点名该行

### Requirement: SoC 产物的双形态一致性 MUST 可机械判定

SoC 产物 MUST 有「**扁平 ≡ 层次化**」的**逐拍全等**判据 ⇒ ⇒ 该判据 MUST 由**发射产物派生**端口集合
⇒ ⇒ **MUST NOT** 手写端口集合 ⇒ ⇒ 激励 MUST **多拍**且**值不同** ⇒ ⇒ 观测集 MUST 含
**至少两个随激励变化**的量。

⇒ ⇒ ⚠️ 粘滞信号（**置位后不清零**的完成标志）**MUST NOT** 计入「随激励变化」的观测量
⇒ ⇒ 理由：它恒为 1 ⇒ ⇒ 恒不变化 ⇒ ⇒ 会使「观测是否随激励变化」的判据**恒绿**。

⇒ ⇒ ⚠️ **通用条款引用**：「层次化产物 MUST 与扁平产物等价（可机械判定）」归 capability
`rtl-hierarchical-output`，其公共 helper 归 `hierarchical-equivalence-public-helper` ⇒ ⇒ 本条**不重述**。

#### Scenario: 双形态逐拍全等且观测量确实变化

- **WHEN** 对 SoC 产物做扁平 vs 层次化的逐拍比对
- **THEN** 比对 MUST 判**逐拍全等**
- **AND** 端口集合 MUST 由**发射产物**派生
- **AND** 激励 MUST ≥ 10 拍且**值不同**
- **AND** 观测集 MUST 含 **≥2 个随激励变化**的量（⇒ ⇒ 粘滞信号 MUST NOT 计入）

### Requirement: SoC 交付物的**观测盲区** MUST 显式登记

SoC 产物的顶层观测面**只覆盖根可见行为** ⇒ ⇒ 成层子树的内部信号在层次化顶层**不可观测**
⇒ ⇒ 「子树内部算错但顶层输出恰好相同」这类缺陷**结构性测不出** ⇒ ⇒ 该盲区 MUST **显式登记**。

⇒ ⇒ ⚠️ **通用条款引用**：该盲区的通用表述归 capability `rtl-hierarchical-output` ⇒ ⇒ 本条
只增加 **SoC 专属**的「该盲区在 SoC 交付物上同样成立」的登记。

#### Scenario: 盲区在 SoC 交付物上同样成立

- **WHEN** 检查 SoC 交付物的观测能力
- **THEN** 顶层可观测的信号 MUST 只包含**根可见**的那一部分
- **AND** 成层子树内部的信号 MUST **不可**从顶层观测
- **AND** 该限制 MUST 在能力面中**显式登记**（⇒ ⇒ 不得让下游误以为顶层观测覆盖全部行为）

### Requirement: SoC 交付物的**能力边界** MUST 显式声明

SoC 交付物 MUST 显式声明以下**不支持**项 ⇒ ⇒ 不得让下游误以为支持：

1. **不支持**「CPU 经 AXI 访问外部存储」（⇒ ⇒ 外部存储模型与顶层总线展平策略均未定义）
2. **不支持**「自定义 master / 从设备」（⇒ ⇒ 产物是**固定功能**的 demo SoC；顶层**不暴露**总线端口）
3. SoC 层 TB **不验证**总线协议（⇒ ⇒ 协议面归 IP 级 TB）

⇒ ⇒ 三项 MUST 以**可 grep 的精确文本**出现在能力面里 ⇒ ⇒ 理由：它们是**运行时可观测的性质**
⇒ ⇒ 不声明 = **误导性宣传**。

#### Scenario: 三条能力边界可被检索

- **WHEN** 检查 SoC 能力面的文本
- **THEN** MUST 能检索到「不支持 CPU 经 AXI 访问外部存储」
- **AND** MUST 能检索到「不支持自定义 master / 从设备」
- **AND** MUST 能检索到「SoC 层 TB 不验证总线协议」
