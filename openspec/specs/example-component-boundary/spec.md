# example-component-boundary Specification

## Purpose
定义 CppHDL 的 **example 与库的职责边界** —— 一个头 / 一个测试**属于谁**，以及「跨边界使用」
必须如何被显式登记与机械判定：

- **住址**：只服务 example 的头 MUST 生在该 example 的目录内（`examples/<name>/`），
  MUST NOT 出现在库的公开面 `include/` 下 ⇒ ⇒ 库的 include 图不因示例而扩张，
  「库有多大」不再取决于「演示过多少东西」
- **例外登记**：确有跨边界用途的头 MUST 进 `.rddf/checks/example-only-headers.tsv`
  **双向**登记（表 ↔ 文件系统 1:1）⇒ ⇒ 「临时借用」不能靠忘记而长期存在
- **使用方向**：库测试 MUST NOT 引用 example 专用头 ⇒ ⇒ 依赖方向恒为
  `examples/ → 库`，不出现反向边

**为什么需要这条契约**：同一语义的多个物理载体（同一组件既住 `include/` 又住
`examples/`，或同一行为既是 example 程序又是测试）会把「改一处」变成「改三处」，
且漂移只在编译 / 链接期暴露。本 capability 把「边界在哪」写成**可机械判定**的判据
（载体 = `scripts/verify_example_only_headers.sh`），而不是靠约定或口头记忆。

## Requirements

### Requirement: 新增的 example 专用头 MUST 生在 examples 目录内

**只服务于 example**（含 example 自带测试）的头 MUST 生在该 example 的目录内 ⇒ ⇒
`include/` **MUST NOT 新增**这类头。

⇒ ⇒ **只约束新增**：目标分支 merge-base 之后（缺省 `origin/main`；取不到 ⇒ fail-closed）
新加的 `include/` 头。存量头不在本条范围。

⇒ ⇒ **判据 = include 引用路径的末段匹配**（末段 == 该头文件名，允许任意目录前缀），
作用域 `include/ src/ tests/ examples/ tools/`；**不引入** include 文法解析器。

⇒ ⇒ **三态 MUST 区分**：零 includer ⇒「孤儿头」；末段同名多处 ⇒「歧义」；
全部 includer 在 `examples/` ⇒ **违规**。前两者 MUST NOT 记为边界违规。

#### Scenario: 新增 example 专用头落在 examples 内

- **WHEN** 本次改动新增一个 `include/` 头，且其全部 includer 都在 `examples/` 下
- **THEN** 护栏 MUST 转红并点名该头
- **AND** 该头 MUST 移到对应 example 目录内，或按例外要求登记

#### Scenario: 子路径引用必须能被认出

- **WHEN** 某头被以**子路径**形式引用（引用路径带目录前缀）
- **THEN** 判据 MUST 仍能识别该 includer
- **AND** 若判据只认「引用路径里没有目录前缀」的形态 ⇒ 该实现 MUST 判为不满足本条

#### Scenario: 孤儿头与歧义必须区分

- **WHEN** 某头零 includer，或末段同名出现在多处
- **THEN** 前者 MUST 报「孤儿头」、后者 MUST 报「歧义」
- **AND** 两者 MUST NOT 被记为「边界违规」

#### Scenario: 库侧消费者排除误伤

- **WHEN** 某头同时被根 `tests/` 与 `examples/` 使用
- **THEN** 它 MUST NOT 判为 example 专用（它是库公开面的一部分）

### Requirement: example 专用头的例外 MUST 双向登记

登记表（`.rddf/checks/example-only-headers.tsv`）记录两类例外：**库层机制 API**
（消费者恰为 examples）与存量例外（历史决策留下）。

⇒ ⇒ **MUST 双向且反向判据 MUST 为红**：登记路径不存在 ⇒ **红**；登记头出现**非** examples
消费者 ⇒ **红**（具名 + 退出码非零，MUST NOT 只打印提示）（防永久豁免池）。

⇒ ⇒ **「消费者」= 使用该 API 的 TU** ⇒ ⇒ **该头自身实现 TU 的 include MUST NOT 计入**
（实现 ≠ 使用）⇒ ⇒ 否则「公开头 + `src/` 实现」形态永远无法登记。

⇒ ⇒ **MUST 对新增头开放**（否则「库层机制 API + 消费者恰为 examples」这类头
**无合规路径**）⇒ ⇒ 该通道是其唯一合规形态。

⇒ ⇒ 存量登记 MUST 记录**原因**（历史决策出处），MUST NOT 用它重新归类已签批的库组件。

#### Scenario: 库层机制 API 走豁免通道

- **WHEN** 某新增头的消费者**恰为** examples，但它提供**库层机制**（供 example 编排调用）
- **THEN** 它 MUST 在登记表登记（路径 + 原因）
- **AND** 一旦出现**非** examples 消费者 ⇒ **红**（具名 + 退出码非零）

#### Scenario: 实现 TU 不算消费者

- **WHEN** 某登记头的**实现文件**（`src/` 下的 TU）include 该公开头
- **THEN** 该 TU MUST NOT 被计为「消费者」
- **AND** 反向判据 MUST 只看 **API 使用方**
- **AND** 若实现 TU 被计入消费者 ⇒ 该实现 MUST 判为不满足本条

#### Scenario: 存量例外必须双向登记

- **WHEN** 某头只被 example 使用，但因历史决策已在 `include/` 内
- **THEN** 它 MAY 登记（路径 + 原因 + 归位条件）
- **AND** 登记路径不存在 ⇒ 红

### Requirement: 库测试 MUST NOT 引用 example 专用头

**根** `tests/` 下的任何 TU MUST NOT 引用 `examples/` 下的头 ⇒ ⇒ 依赖 example 专用组件的测试
MUST 移到该 example 目录内，且 MUST 仍进 CTest（**在默认构建配置下**）。

⇒ ⇒ **MUST NOT** 以「给根 `tests/` 加 include 路径」绕过（文本层规避，依赖方向未变）。

⇒ ⇒ 若被测组件只是**库组件的透传封装**，库测试 MAY 改为直接用库组件
（端口集合与成员名相同 ⇒ 断言零改动）⇒ ⇒ 此时无须搬测试。

#### Scenario: 依赖 example 组件的测试与其同处

- **WHEN** 某个测试需要 example 专用组件
- **THEN** 该测试 MUST 位于该 example 目录内
- **AND** 它 MUST 仍被某个已进 CTest 的 target 编译并执行

#### Scenario: 移动后计数与断言下限同步

- **WHEN** 测试因归位而增减 CTest 条目或新增 examples 测试源
- **THEN** 根计数锚点 MUST 由实测值回填
- **AND** 断言数下限表 MUST 为新源登记一行

#### Scenario: 透传封装的 DUT 可以换库组件

- **WHEN** 被测组件是库组件的纯透传封装
- **THEN** 库测试 MAY 改为直接用库组件
- **AND** 该测试的路径、用例名、结构与断言 MUST NOT 变化

#### Scenario: 关闭 examples 构建时的显式例外

- **WHEN** `CPPHDL_BUILD_EXAMPLES=OFF`
- **THEN** 上述条目消失属**已登记**的接受行为
- **AND** 锚点判据只在默认配置（ON）下成立
