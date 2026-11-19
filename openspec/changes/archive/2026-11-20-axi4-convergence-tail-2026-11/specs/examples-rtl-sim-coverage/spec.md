## ADDED Requirements

### Requirement: C++ 侧入口的断言条数 MUST 登记且被守护

清单 MUST 为每个「自发射」产品单元登记其 **C++ 侧（解释器路径）** 的期望断言条数
⇒ ⇒ 该值 MUST 由**列**承载（`interp_checks`）⇒ ⇒ MUST NOT 由自由文本承载。

⇒ ⇒ **自发射** = `emitter` 列 == `target` 列 ⇒ ⇒ 非自发行的行 MUST 写 `-` 哨兵。

⇒ ⇒ **判据 = 等式（不是下界）**：C++ 侧输出的 `RESULT: checks=<N> fails=0` MUST 满足
`N == interp_checks` ⇒ ⇒ MUST NOT 用 `.sv` 侧的 `min_checks` 代用（两者语义不同
⇒ ⇒ 手写激励的产品 MUST 各自登记）。

#### Scenario: 自发射行缺登记

- **WHEN** 某行满足 `emitter == target` 而 `interp_checks` 为 `-` 或 0
- **THEN** 护栏 MUST 判定为不满足本条
- **AND** MUST 指出该行的 target 名

#### Scenario: C++ 侧条数被删

- **WHEN** 该行 C++ 侧入口输出的 `checks` 与 `interp_checks` 不等
- **THEN** 护栏 MUST 判定为不满足本条
- **AND** MUST 报告期望值与实际值

#### Scenario: 非自发行的行

- **WHEN** 某行满足 `emitter != target`
- **THEN** 该行 MUST 把 `interp_checks` 写 `-` 哨兵
- **AND** 护栏 MUST NOT 对该行执行 C++ 侧条数判据

#### Scenario: 自由文本承载判据

- **WHEN** 判据的适用范围由 note 列的自由文本标记决定
- **THEN** 该实现 MUST 判定为不满足本条
- **AND** 该约定 MUST 已迁入 `interp_checks` 列
