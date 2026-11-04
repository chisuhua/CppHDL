# Capability: component-ownership-and-naming

## Purpose

定义 CppHDL **组件实例的节点归属**与**实例路径命名**契约：节点 MUST 归属到「真正拥有它的
那个组件实例」，组件实例路径 MUST 无重复前缀 ⇒ ⇒ 任何按归属切分的工具（层次化 RTL 输出、
层次化 trace、可读性报告）MUST NOT 把子组件的状态划进父组件。

> ⚠️ **archive 注意**：OpenSpec 的 archive 流程**不合并** delta 文件的 `## Purpose`
> ⇒ 归档后 MUST 另行补写主 spec 的 `## Purpose`。
> 凡 MUST 长期存在的契约性内容一律写进 **Requirement 正文**。

## ADDED Requirements

### Requirement: 节点归属 MUST 由构造作用域唯一决定

节点的归属组件 MUST 由**建节点时所处的构造作用域**唯一决定（最内层作用域的组件）
⇒ ⇒ MUST NOT 依赖事后推断（前缀匹配 / 时间戳 / 遍历修正）⇒ ⇒ 作用域 MUST **配对**
（进入必退出）⇒ ⇒ 栈失衡 MUST 抛异常（MUST NOT 只打日志）。

#### Scenario: 成员节点的归属 MUST 是所属组件

- **WHEN** 一个组件以**成员变量**形式持有 `ch_reg` / `ch_mem` / bundle 字段
- **THEN** 这些节点的归属组件 MUST 是**该组件本身**，而不是它的父组件
- **AND** 其 Verilog 名 MUST 含该组件的路径段

#### Scenario: 构造函数抛出时 MUST 由 RAII 弹栈

- **WHEN** 一个组件的构造函数在**压栈之后**抛出异常
- **THEN** 作用域栈 MUST 由 RAII 护栏**自动回退**到进入前的深度
      （⇒ MUST NOT 残留半帧）
- **AND** 此后新建节点的归属 MUST 仍然正确（⇒ MUST NOT 被那个半构造的组件吸收）

> ⚠️ **为什么不写「栈非空 ⇒ 抛异常」**（v1.1 订正）：配对作用域成立时栈**永远**为空
> ⇒ ⇒ 该断言**不可触发**（⇒ 写进契约就是一条**永远绿**的判据）。可触发的等价形态是
> 「异常路径的栈回退」，故改成上面这条。

### Requirement: 组件的 build context MUST 由显式来源确定

组件的 build context MUST 由**显式来源**确定（环境 context · 构造实参 · `build(ctx)` 前的一次绑定）
⇒ ⇒ **MUST NOT** 取「父组件的 context」（⇒ 父组件在构造期 `context()` 为空 ⇒ ⇒ 子组件**直接消失**）。

⇒ ⇒ **环境 context 为空** ⇒ ⇒ **MUST NOT** 硬失败（⇒ ⇒ 下游 `make_unique<Unit>()` + `build(ctx)`
的装配形态 MUST 可装配）⇒ ⇒ 记为「**无约束**」。
⇒ ⇒ 取不到**任何** ctx ⇒ ⇒ MUST `CHFATAL_EXCEPTION`（MUST NOT 静默自建子 context）。

#### Scenario: 成员形态的子组件 MUST NOT 早退

- **WHEN** 一个组件以**成员变量**形式持有另一个组件（成员形态）
- **THEN** 该子组件 MUST 被正常构造并可访问其 IO
- **AND** MUST NOT 因「父组件此刻还没有 context」而早退
      （⇒ ⇒ 早退 ⇒ ⇒ 后续访问 IO ⇒ ⇒ **解引用空指针 ⇒ SIGSEGV**）

#### Scenario: 环境 context 为空 MUST NOT 硬失败

- **WHEN** 一个组件在**环境 context 为空**的位置被构造（⇒ ⇒ 下游「无 active parent 处装配」形态）
- **THEN** 构造 MUST **不抛异常**
- **AND** 该组件 MUST 能在后续 `build(ctx)` 时完成装配
      （⇒ ⇒ 否则「装配点任意」被架空 ⇒ ⇒ 只对叶子单元成立）

#### Scenario: 取不到任何 ctx MUST 被拒绝

- **WHEN** 一个组件既无环境 context、又无构造实参、也从未 `build(ctx)`
- **THEN** MUST `CHFATAL_EXCEPTION`
- **AND** MUST NOT 静默自建子 context（⇒ ⇒ 那会让节点分裂到另一个 ctx ⇒ ⇒ 宿主看不见）

### Requirement: 跨 context 再装配 MUST 被拒绝

一个组件一旦绑定了 ctx，其后 MUST NOT 以**不同**的 ctx 再 `build` ⇒ ⇒ 否则子模块节点留在
**另一个** ctx ⇒ ⇒ 宿主 **看不见**它们 ⇒ ⇒ **整块逻辑静默消失**。

⇒ ⇒ **基准 MUST 分两支**：① 构造期**有** ctx ⇒ ⇒ `build(ctx)` MUST **等于**它；
② 构造期**无** ctx（「无约束」）⇒ ⇒ 以**首次** `build(ctx)` 绑定的 ctx 为**基准**。

#### Scenario: 跨 context 装配 MUST 被拒绝

- **WHEN** 一个组件已绑定 `ctx1`，而宿主之后 `build(ctx2)` 且 `ctx1 != ctx2`
- **THEN** MUST `CHFATAL_EXCEPTION` 并点名两个 context
- **AND** MUST NOT 继续（⇒ ⇒ 子模块逻辑会留在 `ctx1` ⇒ ⇒ 宿主 ctx **看不见** ⇒ ⇒ 静默消失）

### Requirement: `ch_module` 在「无 active parent」时 MUST 响亮失败；组件 MUST 登记进组件树

⚠️ **作用面 MUST 精确**（本条**不是**「一切组件」的一般义务）：

- **`ch_module` 的构造分支**（`include/module.h` 的「无 active parent」分支）⇒ ⇒
  MUST `CHFATAL_EXCEPTION`（MUST NOT 只打日志后返回 ⇒ ⇒ 后续访问 IO ⇒ ⇒ **SIGSEGV**）。
- **根组件**（`ch_device<T>`）· **`make_unique<Unit>()` + `build(ctx)` 的手工装配** ⇒ ⇒
  **不适用**本条的前半段（它们**本来就**没有 active parent）⇒ ⇒
  它们的失败形态由「**未登记进组件树**」那条承载（见 capability `rtl-hierarchical-output`）。

⇒ ⇒ **已登记但从未 build**：该组件**有节点、无端口**（端口只在 build 期创建）⇒ ⇒ MUST 被响亮拒绝
（MUST NOT 发出一个无端口 module，也 MUST NOT 把它的节点当作空气）。

#### Scenario: `ch_module` 在无 active parent 处 MUST 抛异常

- **WHEN** 一个 `ch_module` 在**连构造作用域都为空**的位置被构造
- **THEN** MUST `CHFATAL_EXCEPTION`
- **AND** MUST NOT 只打印日志后继续

#### Scenario: 根组件与手工装配 MUST NOT 被本条拒绝

- **WHEN** 一个组件经 `ch_device<T>`（根）或 `make_unique<Unit>()` + `build(ctx)`（手工装配）被构造
- **THEN** 构造 MUST **不抛异常**（⇒ ⇒ 它们**没有** active parent 是**正常**的）
- **AND** 它们的失败形态 MUST 由「**未登记进组件树**」那条判据承载（⇒ ⇒ 不由本条承载）

#### Scenario: 未 build 的子组件 MUST 被识别

- **WHEN** 一个子组件被登记进组件树但**从未** `build()`
- **THEN** 框架 MUST 能判定它**有节点、无端口**
- **AND** 任何把它当作 module 发射的路径 MUST **响亮拒绝**

### Requirement: 组件树登记 MUST 双向完整，且归属判定 MUST NOT 误杀成员子组件

组件被登记进组件树时，**父指针 MUST 被补绑**（否则成员形态子组件的父指针**永久为空**
⇒ ⇒ 任何按**向上**父链的判定会把它误判为「不在树里」）
⇒ ⇒ ⇒ **归属判定的结果** MUST 是「成员形态子组件**在**树内」
⇒ ⇒ ⇒ ⚠️ **实现策略**（只向下走 `children()` / 不走父链）属 **ADR-059 D9** 的推导
⇒ ⇒ ⇒ 本条只约束**可观测的结果**，MUST NOT 把策略写成契约（⇒ ⇒ 白盒策略会绑死实现）。

⇒ ⇒ ⚠️ **登记先于 `build()`**（否则建节点时父指针尚未就位 ⇒ ⇒ 节点名取不到父路径）
⇒ ⇒ 且登记返回的 `shared_ptr` MUST 被续存（MUST NOT 继续持有原 `unique_ptr` ⇒ ⇒ 双重管理）。

#### Scenario: 成员子组件 MUST 有非空父指针

- **WHEN** 一个组件以成员形态持有子组件，且该子组件已被登记
- **THEN** 该子组件的父指针 MUST **非空**且指向**直接父组件**
- **AND** 它 MUST NOT 出现在**祖父**的子组件集合里

#### Scenario: 成员子组件 MUST NOT 被归属判定误杀

- **WHEN** 框架判定「某节点是否在根组件的子树内」
- **THEN** 成员形态的子组件 MUST **被正确判定为在树内**
- **AND** MUST NOT 因「父指针为空」而被判为**不在树内**（⇒ ⇒ 那会让旗舰 DUT 被误杀
      ⇒ ⇒ 且错误信息还会叫人去 `add_child`——已经调过了）

