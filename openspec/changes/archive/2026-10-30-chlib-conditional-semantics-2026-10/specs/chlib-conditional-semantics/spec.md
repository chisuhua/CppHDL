# Capability: chlib-conditional-semantics

## ADDED Requirements

### Requirement: 条件块内的赋值 MUST 被条件化

`_if(cond).then(body).elif(cond2)...else_().end()` 内对**目标**（`ch_reg` 的
`next` / `ch_out`）的赋值 MUST 被解释为「**在该分支条件下，该目标取该值**」
⇒ ⇒ 框架 MUST 把各分支的候选值合成为**一条** `select` 链 ⇒ ⇒ 目标 MUST 只有
**一个**写入点。

⇒ ⇒ 条件块内的赋值 MUST 经由 `conditional_assign(target).next() = value` 捕获
⇒ ⇒ **未经** `conditional_assign` 的普通 `reg->next = v` / `io().x = v`
MUST **不**被条件化（⇒ 它是**无条件**的）⇒ ⇒ 该形态 MUST 有**静态护栏**检出。

#### Scenario: 条件为假时目标不被写

- **WHEN** `_if(cond).then([&]{ conditional_assign(reg).next() = V; })` 且 `cond`
  在采样时为**假**
- **THEN** `reg` MUST **不**被写入 `V`
- **AND** `reg` MUST 保持其当前值

#### Scenario: 多分支按先匹配者胜

- **WHEN** 同一个目标在**两个以上**分支里被赋值，且**两个**条件**同时**为真
- **THEN** MUST 取**分支顺序最靠前**的那个值
- **AND** 该顺序 MUST 与 `multi_if`（表达式形式）的**优先级顺序**等价

#### Scenario: 未走捕获点的赋值不被条件化

- **WHEN** 条件块内出现普通 `reg->next = v`（未经 `conditional_assign`）
- **THEN** 该赋值 MUST **不**被条件化（= **无条件**生效）
- **AND** 静态护栏 MUST 报出违规

### Requirement: 条件块 MUST 只在**捕获**意义下遍历分支体

`end()` MUST 遍历**所有**分支体以**收集候选值** ⇒ ⇒ 分支体在**建图期**执行
⇒ ⇒ 分支体 MUST **无副作用**（`std::cout` / 文件 IO / 改全局状态）。

⇒ ⇒ `finalized_` MUST 在**遍历分支体之后**才置位 ⇒ ⇒ 否则捕获机制永不触发
⇒ ⇒ 条件被**彻底忽略**。

#### Scenario: 捕获期间条件块处于激活态

- **WHEN** `end()` 正在遍历分支体
- **THEN** 捕获机制 MUST 处于**激活**态（`is_active()` MUST 为真）
- **AND** 条件块内的 `conditional_assign` MUST 把候选值**收集**而非直接写入

### Requirement: 默认尾部 MUST 区分寄存器与输出

- **寄存器**：无 `else_` 分支时，未匹配 MUST **保持**（当前值）；
- **输出端口**：MUST 有 `else_` 候选值 ⇒ ⇒ 缺失 MUST 在**构建期**被**显式拒绝**
  ⇒ ⇒ **不**降级为「默认 0」（⇒ 组合环 ⇒ 拓扑排序 CHFATAL ⇒ 难诊断）。

⚠️ 拒绝判据 MUST 是 **target 级**（被捕获的输出 target 无 `else_` 候选值），
**不**是块级（块有 `else_` 但该输出无默认候选 ⇒ 块级检查会放过 ⇒ 组合环仍可达）。

#### Scenario: 输出缺默认候选时被拒绝

- **WHEN** 某个输出端口被条件赋值、且该输出**无** `else_` 候选值
- **THEN** 构建 MUST **抛异常**
- **AND** MUST NOT 退化为「默认 0」

#### Scenario: 寄存器无 else 时保持

- **WHEN** 一个寄存器被条件赋值、且**无** `else_` 分支、所有条件均为假
- **THEN** 该寄存器 MUST **保持**其当前值

### Requirement: 嵌套条件块 MUST 以**栈**管理

内层 `_if` 的 `end()` MUST NOT 清掉外层 ⇒ ⇒ 内层 MUST 先合成**一条**表达式，
再作为外层该分支的**候选值** ⇒ ⇒ 嵌套场景 MUST 显式 `end()`。

#### Scenario: 内外两层都正确

- **WHEN** 一个 `_if` 的分支体内嵌套另一个 `_if`
- **THEN** 内层 MUST 先合成**一条**表达式
- **AND** 外层 MUST 把该表达式当作**一个**候选值参与合成
- **AND** 外层的其它分支 MUST 不受影响

### Requirement: 条件构造的**权威表面** MUST 唯一

表达式形式 MUST 是 `multi_if` / `if_then`（`include/chlib/if.h`）；
`priority_if` / `priority_if_then` MUST **不存在**（⇒ 与 `multi_if` 逐行等价
⇒ 名字承诺「优先级 / 互斥优化」但**未兑现** ⇒ Zero-Debt + ADR-043 先例）。

⇒ ⇒ 条件构造 MUST NOT 存在**两个**行为等价的入口。

#### Scenario: priority_if 不再存在

- **WHEN** 代码尝试使用 `priority_if` / `priority_if_then`
- **THEN** MUST **编译不过**（无匹配重载）

### Requirement: 条件构造 MUST **零** `core/` 改动

拦截点 MUST 位于 `chlib` 层 ⇒ ⇒ `core/reg.h` / `ch_out` 的公开契约 MUST **零变化**
⇒ ⇒ `ch_reg` 的「`next` 单 next 源」契约 MUST 保持成立。

#### Scenario: core 契约不变

- **WHEN** 条件块内的赋值被合成为一条 `select` 链
- **THEN** 每个目标 MUST 只有**一个** `next` 写入点
- **AND** `core/` 的公开契约 MUST **零**变化
