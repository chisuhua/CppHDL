# Capability: chlib-conditional-semantics

## MODIFIED Requirements

### Requirement: 条件构造的**权威表面** MUST 唯一

表达式形式 MUST 是 `multi_if` / `if_then`（`include/chlib/if.h`）；
`priority_if` / `priority_if_then` MUST **不存在**（⇒ 与 `multi_if` 逐行等价
⇒ 名字承诺「优先级 / 互斥优化」但**未兑现** ⇒ Zero-Debt + ADR-043 先例）。

⇒ ⇒ 条件构造 MUST NOT 存在**两个**行为等价的入口。

⇒ ⇒ 表达式表面的**接受面限制**（能否直接赋给输出端口 / 分支值是否须同类型 /
条件能否是布尔输入端口）MUST 有**明文契约** ⇒ ⇒ 不得只存在于注释里 ⇒ ⇒
（限制的逐条形态见下方 Scenario）。

#### Scenario: priority_if 不再存在

- **WHEN** 代码尝试使用 `priority_if` / `priority_if_then`
- **THEN** MUST **编译不过**（无匹配重载）

#### Scenario: 表达式形式的接受面限制 MUST 可查

- **WHEN** 使用者尝试把 `multi_if` 直接赋给 `ch_out`
- **THEN** MUST **编译不过**（`multi_if has no member named 'impl'`）
- **AND** MUST 有**明文契约**记录该限制（⇒ 而非只存在于注释里）
- **AND** 该限制的**修复状态** MUST 可判定（已修 / 未修）

#### Scenario: 表达式形式的分支值 MUST 同类型

- **WHEN** 各分支的值宽度**不同**（例：`a + b` 是 **9 位**、`a - b` 是 **8 位**）
- **THEN** `multi_if<T>` 的类型推导 MUST 失败（⇒ 无法编译）
- **AND** MUST 有**明文契约**记录该限制（⇒ 修复者 MUST 能区分「契约」与「偶然」）

#### Scenario: 表达式形式的条件不能是布尔输入端口

- **WHEN** 以 `ch_in<ch_bool>` 端口作为 `if_then` 的条件
- **THEN** MUST **编译不过**（`no matching function`）
- **AND** 该限制 MUST 有**明文契约**记录
- **AND** 语句块形式（`_if`）的条件参数 MUST 接受任意硬件操作数
  （⇒ ⇒ 二者的**可表达性**差异 MUST 是**已知且有契约**的）
