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

## ADDED Requirements

### Requirement: `multi_if` 的接受面 MUST 完备（要么语义正确、要么编译期拒绝）

`multi_if<T>` 的接受面里**每一格** MUST 要么**语义正确**、要么**响亮拒绝**
（编译期 `static_assert` 或建图期 `CHFATAL_EXCEPTION`）⇒ MUST NOT 存在第三态
（「编译通过但语义错误」）。六条落实规则（宽度 / 字面量 / 单分支 / port / 算术 /
结果类型）见下方 Scenario。

⇒ ⇒ 该契约 MUST 由**机器护栏**（`scripts/verify_multi_if_width.sh`）核验；每个
「拒绝」fixture MUST 配**中性化对照**（去掉对应断言 ⇒ 该 fixture 通过 ⇒ 证明
「拒」来自该断言而非其它编译错误）。

#### Scenario: 结果宽度规则与字面量判据

- **WHEN** 声明 `multi_if<T>`（`T` = 显式模板实参，否则首分支值的类型）
- **THEN** 后续**硬件**分支宽度 MUST ≤ `width(T)`（更窄 ⇒ 零扩展；更宽 ⇒ 编译期拒绝）
- **AND** `ch_literal_impl<V,W>` MUST 按 **`W`** 参与同一条**静态**判据
- **AND** **裸算术分支** MUST NOT 按类型宽度参与静态判据（`ch_width_v<int>` = 32 是
  占位语义 ⇒ 会误拒 `elif(c, 1)`）⇒ 其值 MUST 在建图期**可容纳**于 `width(T)`
- **AND** `operator T()` 的返回类型 MUST **恒为 `T`**
  （⚠️ `select` 在 `result_width <= 1` 时返回 `ch_bool` ⇒ MUST 收敛回 `T`）

#### Scenario: 形态 A —— 后续分支更宽 MUST 编译期拒绝

- **WHEN** `T` 为 8 位（首分支决定）而某后续分支是 9 位（例 `a + b`）
- **THEN** MUST **编译失败**，且错误消息 MUST 含两个宽度数值与
  `if_then<ch_uint<W>>` 逃生口指引
- **AND** MUST NOT 静默截断（今天的行为是插入 `bits<7:0>` 节点 ⇒ 丢高位）

#### Scenario: 形态 E —— 显式 `T` 过窄 MUST 编译期拒绝

- **WHEN** `if_then<ch_uint<1>>(c, a)` 且 `a` 是 8 位
- **THEN** MUST **编译失败**（显式 `T` 与推导 `T` 适用**完全相同**的规则）

#### Scenario: 形态 B —— 单分支 MUST 拒绝

- **WHEN** `multi_if` 只有**一个**分支（无 `elif` 且无 `else_`），在转换点
- **THEN** MUST 抛可捕获异常（MUST NOT 静默返回该分支值）
- ⚠️ **反例**：`if_then(c, a).else_(b)`（两分支）与 `if_then(c,a).elif(c2,b)`
  （多分支无 `else_`）MUST **仍然合法**

#### Scenario: 形态 C —— 首分支为 port MUST 归一化

- **WHEN** 首分支是 `ch_uint<8>` 的输入端口
- **THEN** `T` MUST 归一化为 `ch_uint<8>` ⇒ 表达式 MUST **可用**
  （今天的行为是编译失败）
- **AND** `ch_in<ch_bool>` 端口 MUST 经 `lnode<ch_bool>` 兜底归一化

#### Scenario: 形态 D —— `T` 为纯算术 MUST 拒绝

- **WHEN** `if_then(c, 1)`（`T` 推导为 `int`）
- **THEN** MUST 在 **ctor** 处拒绝，消息 MUST 指引 `if_then<ch_uint<N>>(c, 1)`
- **AND** MUST NOT 保持「ctor 放行、转换处失败」的死路

#### Scenario: 以下写法 MUST 保持合法（防过度实现）

- **WHEN** 使用以下任一形态
- **THEN** MUST 编译通过且行为不变：
  ① 显式宽 `T` + 全窄分支（零扩展）② 仅 `else_` 的两分支
  ③ 多分支无 `else_`（尾部 = 末分支值）④ 每分支显式 `ch_uint<N>(...)` 收敛
  ⑤ `elif(c, 1)` 等裸整数字面量 ⑥ 首分支为 port（归一化）
- **AND** MUST NOT 把「更宽 ⇒ 拒绝」实现成「不等 ⇒ 拒绝」
