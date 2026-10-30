# chlib-conditional-semantics Specification

## Purpose
定义 CppHDL 的 **语句块风格条件构造**（`include/chlib/if_stmt.h` 的
`_if(cond).then(body).elif(cond)...else_()...end()`）与 **`ch_reg` / `ch_out`
的条件化写入**的契约。

> 本 capability 的目的是把「**条件被彻底忽略**」这一**最隐蔽**的失效从
> **隐性**变成**明文契约**：2026-10-30 之前，`end()` **先**置 `finalized_ = true`
> **再**遍历分支体 ⇒ 遍历期间 `is_active()` 恒为 false ⇒ 捕获机制**永不触发**
> ⇒ 走「直接赋值」分支 ⇒ ⇒ 条件块内的 `reg->next = v` **每拍无条件生效**
> ⇒ ⇒ 写 `_if(rst).then([&]{ reg->next = 0; })` 会让 `reg` **每拍被清零**
> ——**与意图相反**。
>
> ⚠️ 更糟的是：当时的单元测试**钉住了该坏行为**（用例注释明写「This test pins
> down that current … behavior so future changes are noticed」）⇒ ⇒ 任何**正确**
> 的实现都会让它变红 ⇒ ⇒ 「修好了」与「测试红了」**无法区分** ⇒ ⇒ 长期「假绿」。

⚠️ **边界**：本 capability 只管**语句块**形式；**表达式**形式
（`include/chlib/if.h` 的 `multi_if` / `if_then`）的**行为**由框架的
`select` 语义决定 ⇒ ⇒ 两者**优先级顺序**等价，但**不**逐位等价
（无 `else_` 时表达式形式的尾部 = 末分支的值，而语句块形式的寄存器 = **保持**）。

⚠️ **来源**：ADR-051（`docs/adr/ADR-051-chlib-conditional-semantics.md`，
critic `review_profile: arch` 第 1 轮 **APPROVE_WITH_CONDITIONS**，**0 BLOCKER**）。

## Requirements

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

- **WHEN** 使用者把 `multi_if` 直接赋给 `ch_out`
- **THEN** MUST **编译通过**（⚠️ **订正记录**（2026-10-30）：本 Scenario 的
  `THEN` 原写「MUST **编译不过**（`multi_if has no member named 'impl'`）」
  —— 那是**修复前**的行为 ⇒ ⇒ 现已修复 ⇒ 改为「MUST 编译通过」）
- **AND** 直接赋值 MUST 与「先转成 `ch_uint` 再赋值」**逐位等价**
- **AND** MUST 有**明文契约**记录该行为（⇒ 而非只存在于注释里）
- **AND** 该行为的**修复状态** MUST 可判定（已修 / 未修）

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
- **THEN** MUST **编译通过**（⚠️ **订正记录**（2026-10-30）：本 Scenario 的
  `THEN` 原写「MUST **编译不过**（`no matching function`）」—— 那是**修复前**
  的行为 ⇒ ⇒ 现已修复 ⇒ 改为「MUST 编译通过」）
- ⚠️ **本 Scenario 的标题已过时**（标题仍说「**不能**是」而正文说「MUST 编译
  通过」）⇒ ⇒ **标题 MUST 保留原样**：OpenSpec CLI 按**标题**匹配 Scenario
  ⇒ ⇒ 改标题会被判定为「丢弃旧 Scenario + 新增一个」⇒ ⇒ **拒绝归档**
  （实测：`omits scenario(s) the current spec still has`）⇒ ⇒ 与 ADR-052
  处理 E2 假 `THEN` 的做法**一致**（**原地订正内容**，不动标题）
- **AND** 该行为 MUST 有**明文契约**记录
- **AND** 语句块形式（`_if`）的条件参数 MUST 接受任意硬件操作数
  （⇒ ⇒ 二者的**可表达性** MUST **对齐** ⇒ ⇒ 表达式形式 MUST NOT 比语句块
  形式更受限）

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

### Requirement: 条件构造 MUST **零** `core/` 改动

拦截点 MUST 位于 `chlib` 层 ⇒ ⇒ `core/reg.h` / `ch_out` 的公开契约 MUST **零变化**
⇒ ⇒ `ch_reg` 的「`next` 单 next 源」契约 MUST 保持成立。

#### Scenario: core 契约不变

- **WHEN** 条件块内的赋值被合成为一条 `select` 链
- **THEN** 每个目标 MUST 只有**一个** `next` 写入点
- **AND** `core/` 的公开契约 MUST **零**变化

### Requirement: 条件块的析构 MUST 兼容抛出路径

条件块对象的析构函数 MUST 在**未终结**时兜底调用 `end()`（⇒ 保证候选值被
合成、目标被写入）⇒ ⇒ 而 `end()` **可能**抛出（输出端口缺 `else_` 候选值 ⇒
构建期拒绝）⇒ ⇒ 因此：

- 析构函数 MUST 声明为 **`noexcept(false)`**（⇒ 析构默认 `noexcept` ⇒
  析构期抛出会 `std::terminate` ⇒ ⇒ 检出变成 abort ⇒ **不可捕获** ⇒ **不可测**）；
- 析构 MUST 在**展开期**（`std::uncaught_exceptions() > 0`）**不抛**
  （⇒ 避免 `std::terminate`）；
- 兜底 `end()` MUST **幂等**（⇒ 重复终结 MUST NOT 重复写入）。

⇒ ⇒ 嵌套场景 MUST **显式** `end()`（兜底**不能**表达「内层先于外层合成」）。

#### Scenario: 输出缺默认候选时的拒绝在析构路径上可捕获

- **WHEN** 一个输出端口被条件赋值、且该输出**无** `else_` 候选值，
      且异常经由条件块对象的**析构**触发
- **THEN** MUST 抛出**可捕获**的异常（⇒ MUST NOT `std::terminate`）
- **AND** 展开期 MUST **不抛**

#### Scenario: end() 幂等

- **WHEN** `end()` 在条件块上被调用**两次**
- **THEN** MUST NOT 重复写入目标
- **AND** MUST NOT 抛出

### Requirement: `multi_if` 的接受面 MUST 完备（要么语义正确、要么响亮拒绝）

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
