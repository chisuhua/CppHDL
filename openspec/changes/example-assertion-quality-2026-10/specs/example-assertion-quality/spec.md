## ADDED Requirements

### Requirement: examples 内的测试 MUST 断言语义输出

- **WHEN** 一个 examples 内的测试 target 被注册进 CTest
- **THEN** 它的**每个**用例 MUST 至少断言一个**语义确定的输出**
  （`REQUIRE` / `CHECK` / 其 throws / nothrow / STATIC 变体）
- **AND** MUST NOT 仅以 `SUCCEED(...)` 作为用例的全部内容
  （那只能证明「构造没崩」，不能证明行为对 ⇒ 是**账面覆盖**）
- **AND** 每个测试源的可执行断言数 MUST 不低于仓库登记的下限锚点
  （机制与 `CTEST_TOTAL` 同型：任何**减**都必须显式改锚点 ⇒ 可审计的行为）

#### Scenario: 计数只统计活代码

- **WHEN** 断言出现在**不可能执行**的位置
- **THEN** 它 MUST NOT 计入可执行断言数
- **AND** 这些位置 MUST 至少包括：
  - **宏体**（`#define` 行，含 `\` 续行）—— 宏体**可能永不展开** ⇒
    计入即等于给了「灌计数」的后门
  - `#if 0` 区段
  - `#if 1` 之后的 `#elif` / `#else`（**死分支**）
  - `#ifdef` / `#ifndef` / 非常量 `#if` 的**整个区段**（连同其 `#elif` / `#else`）
- **AND** 反之，**活分支**内的断言 MUST 计入：
  - `#if 0` 之后的 `#else`
  - `#if 0 / #elif 1` 的 `#elif` 体
- **AND** 词边界 MUST 生效：`MYREQUIRE(` / `XCHECK(` 这类**非**断言宏 MUST NOT 计入

#### Scenario: 逐用例下限

- **WHEN** 一个测试文件含多个用例
- **THEN** 每个**字面**用例 MUST 各自 ≥ 1 条可执行断言
- **AND** 该检查 MUST NOT 仅做**文件级**下限 —— 文件级下限会被同文件其它用例掩盖
  （实测：往一个 52 断言的文件尾追加一个只有 `SUCCEED` 的 `TEST_CASE`，
  文件级计数不变 ⇒ 护栏全绿）

#### Scenario: 不可见的用例 MUST 被点名

- **WHEN** 某个用例宏位于被条件编译跳过的区段
- **THEN** 护栏 MUST **报出**它（不得完全静默）
- **AND** 该报告 MUST 说明它**未被**逐用例检查覆盖

#### Scenario: 能力边界 MUST 如实声明

- **WHEN** 护栏的文档描述其覆盖范围
- **THEN** 它 MUST **明确声明**不覆盖「等量替换」（把 `REQUIRE(pc == X)` 换成
  `REQUIRE(1 == 1)` 时断言数不变 ⇒ 任何计数守恒都发现不了）
- **AND** 它 MUST 明确声明该条由 **code review** 承担
- **AND** 文档 MUST NOT 把「等量替换」写进任何 MUST 的立法理由

### Requirement: 静态分析的期望值 MUST 有可执行的 fixture

- **WHEN** 断言质量护栏的计数机制被修改
- **THEN** MUST 有一组**最小 fixture** 覆盖每种形态（注释 / 宏体 / 死分支 / 活分支 /
  词边界 / throws / SUCCEED-only）
- **AND** 每个 fixture MUST 有**期望计数**（`A` 与逐用例计数）
- **AND** 自检 MUST 在 CI 中运行
- **AND** ⚠️ 该机制**不可能完全闭合** ⇒ 任何扩展 MUST 先补 fixture 再改实现

#### Scenario: fixture 覆盖每种计数形态

- **WHEN** 计数机制新增或修改一种形态（注释 / 宏体 / 死分支 / 活分支 / 词边界 /
  throws / SUCCEED-only）
- **THEN** MUST 有对应的最小 fixture 与**期望计数**
- **AND** 该 fixture MUST 参与 CI 自检
- **AND** fixture 的期望值 MUST 是**显式写死**的（不得由实现反推 —— 那会变成循环论证）
