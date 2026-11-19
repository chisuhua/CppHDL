# Capability: examples-rtl-sim-coverage

## ADDED Requirements

### Requirement: 发射器调用 MUST 支持参数化调用

清单 MUST 能表达「**发射器 target** + **调用参数**」⇒ ⇒ MUST 有一个 `emitter_args` 列。

⇒ ⇒ **列语义 MUST 唯一**：
· `emitter` = target 名（**单个** token ⇒ MUST NOT 含空格）
· `emitter_args` = **空格分隔**的参数 token 序列 ⇒ ⇒ **无参 MUST 写 `-` 哨兵**
· 单个参数 token MUST NOT 含空白（需要含空白的参数 ⇒ 另立列，不在本条范围）

⇒ ⇒ `-` 哨兵 MUST NOT 被**原样透传**给发射器；**每行字段数 MUST 等于 `#COLUMNS` 词数**
（否则 TAB 折叠会让字段左移 ⇒ 报具名错）。

⇒ ⇒ 参数**位置 MUST 无关**（调用方剥离式组装）；`#COLUMNS` 推导与「中间插列」两条
MUST 遵循本 capability 的既有 requirement ⇒ ⇒ 本条**不重述**。

#### Scenario: 参数缺失 ⇒ 以 fail-closed 结束

- **WHEN** 某**需要**参数的行被改成 `-`
- **THEN** 该次发射 MUST 以**非零**结束（默认模式收到未知参数 ⇒ 退出码 2，或产不出产物）
- **AND** 护栏 MUST 报「发射器调用」这一类错误
- **AND** 该负向对照 MUST 钉在**需要参数的行**上（无参行掉参数仍绿）

#### Scenario: 无参哨兵不得成为实参

- **WHEN** 某行参数列为 `-`
- **THEN** 该次调用的**命令行** MUST NOT 含由该列透传而来的 `-`
- **AND** 该次调用 MUST 与「不带该列参数」等价

#### Scenario: 字段数守恒可归因

- **WHEN** 某行字段数 ≠ `#COLUMNS` 词数（例如中段留空）
- **THEN** 护栏 MUST **具名**报错（指出行号与期望字段数）
- **AND** MUST NOT 以无名 `unbound variable` 中止整轮

#### Scenario: 只读 target 名的消费方零改动

- **WHEN** 清单新增一列（参数列）
- **THEN** 只读 target 名的消费方 MUST 仍绿且**无需**改代码
