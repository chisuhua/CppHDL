## MODIFIED Requirements

### Requirement: 跨层端口策略 MUST 是单一权威的线性枚举，且各取值的失败语义 MUST 明确

跨层非端口边的处理策略 MUST 由**唯一**的线性枚举 `PortPolicy` 承载
（`SynthesizeWarn` / `SynthesizeFatal` / `Reject`）
⇒ ⇒ MUST NOT 存在第二份同义枚举副本
⇒ ⇒ 「合成」MUST 是**纯内存 phase**、**先于任何文件 I/O**
⇒ ⇒ 悬空输入的策略 MUST 由 `UndrivenPolicy`（`Bubble` / `Reject`）承载。

⚠️ **诊断的计数与列举单位 MUST 是「边」（需合成的跨 module 边），MUST NOT 是
「合成物」（1 条边常合成 k 个端口 + 1 根网）** ⇒ ⇒ 否则「首行计数」与「实际列出的
条目数」**必然**不等（用户可见的错报）。

⚠️ **每条边级诊断 MUST 点名该边**（形如 `from <node> (<module>) -> to <node> (<module>)`；
`(<module>)` = 该 module 在层次树里的**实例路径**）⇒ ⇒ MUST NOT 只报合成物名
（用户据此无法定位是哪条跨 module 边）。

#### Scenario: SynthesizeWarn 是默认策略

- **WHEN** 未显式指定策略
- **THEN** 隐式端口 MUST 被**合成**并产出产物
- **AND** MUST 对**每一条**被合成的边产出一条诊断：点名该边**与**它合成出的产物名
  （端口名 / 网名）
- **AND** 该诊断 MUST **同时**出现在 `result.warnings`（公开字段）与 `CHWARN` 日志中
  ⇒ ⇒ 「合成了几条边」MUST NOT 只能靠抓 stderr 才可观测
- **AND** MUST 在产物内为**发生合成**的端口写注释

#### Scenario: 未发生合成时产物 MUST 不含合成注释

- **WHEN** 设计内**没有**任何跨 module 边被合成
- **THEN** 产物 MUST NOT 含任何合成端口注释
- **AND** 「默认（全 inline）产物与扁平产物逐字节一致」这条判据 MUST 仍然成立

#### Scenario: SynthesizeFatal 先合成完再失败

- **WHEN** 策略为 `SynthesizeFatal` 且设计含**至少两条**可合成的边
- **THEN** 发射 MUST **失败**且 MUST NOT 产出产物文件
- **AND** 失败诊断 MUST 列出**全部**被合成的边与合成端口名（不止第一条）
- **AND** 列出的**条目数** MUST == **边数**，且 MUST == 诊断首行给出的计数
- **AND** MUST NOT 留下临时文件
- **AND** MUST NOT 触碰既有同名文件

#### Scenario: Reject 只报第一条

- **WHEN** 策略为 `Reject` 且设计含可合成的边
- **THEN** 发射 MUST 失败且 MUST NOT 产出产物文件
- **AND** 诊断 MUST 报出**第一条需合成的跨 module 边**（**边级**记录，不是合成物名）
- **AND** 诊断 MUST NOT 含**合成端口名 / 产物名**（本档不合成 ⇒ ⇒ 无从列出）
- **AND** 诊断首行给出的「**列出的**条目数」MUST == 实际列出的条目数（= 1）；
  可合成边的**总数** MAY 另行给出 ⇒ 诊断 MUST NOT 把总数冒充成「列出的条目数」
