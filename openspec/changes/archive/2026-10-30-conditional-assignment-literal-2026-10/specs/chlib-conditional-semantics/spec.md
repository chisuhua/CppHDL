# Capability: chlib-conditional-semantics

## MODIFIED Requirements

### Requirement: 条件构造的**权威表面** MUST 唯一

表达式形式 MUST 是 `multi_if` / `if_then`（`include/chlib/if.h`）；
`priority_if` / `priority_if_then` MUST **不存在**（⇒ 与 `multi_if` 逐行等价
⇒ 名字承诺「优先级 / 互斥优化」但**未兑现** ⇒ Zero-Debt + ADR-043 先例）。

⇒ ⇒ 条件构造 MUST NOT 存在**两个**行为等价的入口。

⇒ ⇒ 表达式表面的**接受面限制**（能否直接赋给输出端口 / 分支值宽度 / 条件能否是
布尔输入端口）MUST 有**明文契约** ⇒ ⇒ 不得只存在于注释里。
⚠️ **分支值宽度**这条的**规范内容**已移入同 capability 的
「`multi_if` 的接受面 MUST 完备」Requirement（ADR-052）⇒ 本条款只保留
E1 / E3 两条与「权威表面唯一」本身。

⇒ ⇒ ⚠️ **`conditional_assign` 的 value MUST 接受**裸 literal**（`0_d` / `0xAA_h`）
—— 与硬件表达式（`count + 1_d`）**等价** ⇒ ⇒ 两者 MUST 都经 `to_operand` 归一化
⇒ ⇒ 实现 MUST NOT 让其中一类**编译不过**。

⇒ ⇒ ⚠️ **该归一化 MUST 覆盖**两条**写入路径**（`ch_reg` 的 `->next` 与 `ch_out`
的 `<<=`）—— ⚠️ 因为 `if` / `else` 两个分支**都是模板代码** ⇒ ⇒ **两条路径都会
被实例化** ⇒ ⇒ 只修一条 ⇒ ⇒ 另一条上的 literal 实参**编译不过**。

⇒ ⇒ MUST NOT 用 `get_lnode` 直接取 literal（`ch_literal_impl` **没有**
`get_lnode` 重载 ⇒ ⇒ 该路径对 literal **编译不过** ⇒ ⇒ 连框架**自己的头注释
示例**都会编译不过）。

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
- **THEN** 结果宽度 MUST 由 `T` 决定（显式模板实参，否则首分支类型）⇒ **更宽**的
  分支 MUST **编译期拒绝** ⇒ MUST NOT 静默截断
- **AND** 该规范的**详细规则** MUST 由本 capability 的
  「`multi_if` 的接受面 MUST 完备」Requirement 承载
- ⚠️ **订正记录**（2026-10-30，ADR-052）：本 Scenario 的 `THEN` 原写
  「`multi_if<T>` 的类型推导 MUST 失败（⇒ 无法编译）」—— 经实测**证伪**
  （今天编译通过、零告警、静默丢位）⇒ 已订正为上述形态。

#### Scenario: 表达式形式的条件不能是布尔输入端口

- **WHEN** 以 `ch_in<ch_bool>` 端口作为 `if_then` 的条件
- **THEN** MUST **编译不过**（`no matching function`）
- **AND** 该限制 MUST 有**明文契约**记录
- **AND** 语句块形式（`_if`）的条件参数 MUST 接受任意硬件操作数
  （⇒ ⇒ 二者的**可表达性**差异 MUST 是**已知且有契约**的）

#### Scenario: 裸 literal MUST 被接受

- **WHEN** 写 `conditional_assign(target).next() = 0xAA_h;`（**裸** literal）
- **THEN** 该调用 MUST **编译通过**
- **AND** MUST 与硬件表达式（`count + 1_d`）**行为一致**
- **AND** MUST 对 `ch_reg` 与 `ch_out` **两种** target 都成立

#### Scenario: 归一化 MUST 覆盖两条写入路径

- **WHEN** 检查 `conditional_assignment::assign()` 的实现
- **THEN** `ch_reg` 路径（`->next`）与 `ch_out` 路径（`<<=`）MUST **都**经
      `to_operand` 归一化
- **AND** 只修其中一条 MUST 被护栏检出（⇒ 否则另一条上的 literal 仍编译不过）
