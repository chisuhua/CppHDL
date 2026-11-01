# Capability: chlib-stream-arbitration

## MODIFIED Requirements

### Requirement: 仲裁器的命名与文档 MUST 与实现行为一致

任何 `chlib` 仲裁器的**名字**与**头注释** MUST 如实描述其**实际仲裁策略**
⇒ ⇒ MUST NOT 声称轮转 / wrap-around 而实现是固定优先级
（「名字/注释 over-promise」会**诱导**调用方写出依赖不存在行为的代码）。

⇒ ⇒ 若**无法**在不制造组件重复的前提下兑现名字 ⇒ ⇒ MUST **订正文档**
（零行为变更）并把「组件去留」作为**独立架构裁决**登记，MUST NOT 在本类
缺陷修复中**顺手改行为**。

⇒ ⇒ **被改动过 / 名字说过谎**的组件 MUST 登记在**命名诚实性登记表**
（`.rddf/checks/chlib-naming-honesty-registry.md`），并 MUST 绑定一条**行为**
用例锁定其**声称语义** ⇒ ⇒ 登记漂移与判别力消失 MUST 由**自动化护栏**检出。

#### Scenario: 固定优先级仲裁器 MUST NOT 声称轮转

- **WHEN** 检查固定优先级仲裁器（现为 `stream_arbiter_priority`）的头注释
- **THEN** 它 MUST NOT 声称 round-robin / wrap-around
- **AND** 它 MUST 说明实际策略（固定优先级扫描、较高索引优先）
- **AND** `stream_arbiter_sequence` MUST **不存在**（它的注释曾谎称
  「using round-robin selection」/「wraps around」而实现是 `priority_encoder`
  ⇒ ⇒ 经**独立架构裁决**后于 2026-11-01 **物理删除**）
- **AND** 该「不得回归」MUST 由**结构护栏**守住（源文件 MUST NOT 再出现该符号）

#### Scenario: 与既有组件近乎重复时 MUST 登记而非删除

- **WHEN** 发现两个仲裁器在**仲裁逻辑上逐位相同**（`stream_arbiter_sequence`
  与 `stream_arbiter_priority` 都用 `priority_encoder(valid_vector)`，
  仅 ready 链的门控不同）
- **THEN** 该重复 MUST 作为**独立架构裁决**登记（组件身份/去留）
- **AND** MUST NOT 在该类**语义修复**中**删除**其中一个（仓内有消费者
  ⇒ 删除是 API 变更）
- **AND** 裁决为「删除」时，删除 MUST 走**独立的 change**（含消费者迁移 +
  回归禁令护栏）⇒ ⇒ MUST NOT 夹在语义修复里

#### Scenario: 被改动的组件 MUST 登记并绑定判别性行为用例

- **WHEN** 一个 change 引入 / 改名 / 订正注释 / 改动某个 `chlib` 组合子
- **THEN** 该组件 MUST 在命名诚实性登记表中有**一行**（含它**声称**的语义 +
  指向那条**行为**用例的 tag / macro）
- **AND** 该 change 的 proposal MUST 用 `<!-- touched-components: ... -->`
  声明它触及的组件 ⇒ ⇒ 那些组件 MUST 都已登记
- **AND** 自动化护栏 MUST 检出「登记的组件已不存在」与「判别性用例被删 / 改名」
- **AND** ⇒ ⇒ 本条的覆盖范围 MUST NOT 被读作「全仓普查」：既有组件**不**因
  未登记而违规 ⇒ ⇒ 「新增组件忘了登记」那一格由 **per-change 义务** + 评审抓

