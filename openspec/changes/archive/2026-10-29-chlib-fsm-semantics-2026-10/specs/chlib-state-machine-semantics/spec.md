# Capability: chlib-state-machine-semantics

## Purpose

定义 CppHDL 的 **FSM 状态机 DSL**（`include/chlib/state_machine.h` 的
`ch_state_machine`）与 **`ch_reg` 的 `next` 写入语义**的契约。

> 本 capability 的目的是把「**正确形态**」从**隐性陷阱**变成**明文契约**：
> 2026-10-29 之前，框架**头注释的示例本身**就演示了坏写法
> （`if (io().start) sm.transition_to(...)`），而「`build()` 遍历**所有** state
> ⇒ 写在 `on_active` 内的 `reg->next` 必被覆盖」这条**最隐蔽**的陷阱
> **完全未文档化** ⇒ 同一缺陷在仓内**两次**真实发生
> （`uart_tx` 的 FSM 与 `test_verilator_complex_e2e.cpp` 的 SPI fixture），
> 且两次都被「**只有 parity**」的断言掩盖 ⇒ 长期「假绿」。

## ADDED Requirements

### Requirement: `ch_reg` 的 `next` 写入 MUST 是 last-wins（覆盖）语义

`ch_reg<T>` 的 `next` **MUST** 只有 **1 个**写入源。同一寄存器被多次赋值时，
**最后一次 MUST 覆盖**前一次（`regimpl::set_next` 就是 `set_src(0, next)`）。

⇒ ⇒ 「**每个状态各写一次**」**无法**表达多状态 FSM（只有最后发射的那个状态
生效 ⇒ 典型症状 = 「装载数据的分支**永不生效**」）。

⇒ ⇒ 因此每个寄存器 MUST 有**唯一**的 `next` 写入点：各状态的候选值用
`select` 组合成**单一表达式**，写在 `sm.build()` **之后**。

⚠️ **这是现状契约**：本条把**实现语义写死** ⇒ 将来若要给 `ch_reg` 增加
「多源 next（带优先级）」的能力，MUST 先走本 capability 的 **MODIFIED** delta
（= 一次显式的架构裁决），MUST NOT 静默改变既有语义。

#### Scenario: 同一寄存器被多状态写入

- **WHEN** 同一 `ch_reg` 的 `next` 在**两个以上** state's `on_active` 闭包内被赋值
- **THEN** 只有**最后发射**的那个赋值生效（last-wins）
- **AND** 先发射的那个赋值的效果**永不可观测**
- **NOTE**: 这是**契约**（现状语义），不是「期望行为」⇒ 见上一条的 MODIFIED 要求

### Requirement: `ch_state_machine::build()` MUST 遍历所有 state 执行各自 `on_active`

`build()` MUST 按 state index **依次**执行**每一个** state 的 `on_active` 闭包
（**一次性**，在 describe 期），以便把闭包内的 `transition_when` / `transition_to`
注册进 next-state 的 select 树。

⇒ ⇒ 因此 `on_active` 闭包内：

1. MUST **只**写 `transition_when` / `transition_to`；
2. MUST **NOT** 写任何寄存器的 `next`（⇒ 见「last-wins」那条）；
3. MUST **NOT** 驱动任何 IO 端口（`ch_out` 的 `operator=` 同样是 `set_src(0, …)`
   ⇒ 多状态各驱动一次 ⇒ **最后发射者获胜**）。

⇒ ⇒ 寄存器与输出的写入 MUST 放在 `build()` **之后**。

⚠️ **静态护栏 MUST 检出**：违反第 2/3 条的写法 MUST 被静态检查检出
（`scripts/verify_fsm_next_wiring.sh`）⇒ 因为它**零编译错误、零测试失败**
（两后端由**同一份** fixture 生成 ⇒ 缺陷**同构** ⇒ parity **恒绿**）。

#### Scenario: 闭包内写寄存器

- **WHEN** 某个 state's `on_active` 闭包内出现 `<reg>->next = ...`
- **THEN** 静态护栏 MUST 报告该违规（文件 + 行号）
- **AND** 该写法 MUST NOT 被当作合法用法

#### Scenario: 闭包内驱动 IO 端口

- **WHEN** 某个 state's `on_active` 闭包内出现 `io().<port> = ...` 或
  `io().<port> <<= ...`
- **THEN** 静态护栏 MUST 报告该违规（文件 + 行号）

### Requirement: `transition_when` 的 target MUST 是编译期常量

`transition_when(cond, target)` 的 `target` MUST 是**编译期常量**（枚举值）——
转移在 `build()` 注册时就把 target **固化**了。

⇒ ⇒ MUST **NOT** 用条件表达式（三元 `cond ? A : B`）选择 target：
三元在**建图期**求值（`ch_bool::operator uint64_t()` **只对 const 节点返真值**）
⇒ 恒取其中一个分支 ⇒ **另一个目标状态永不可达** ⇒ 状态卡死。

⇒ ⇒ 条件转移 MUST 拆成**多条** `transition_when`（条件**互斥**）。

#### Scenario: 用三元选择转移目标

- **WHEN** `transition_when` 的 target 由条件表达式给出
- **THEN** 该目标状态 MUST **永不可达**（建图期已固化）
- **AND** 正确写法 MUST 是**多条**互斥条件的 `transition_when`

### Requirement: `transition_when` 的条件是电平敏感

`transition_when` 的条件 MUST 被理解为**电平敏感**（每拍求值、为真即走），
**不是**「事件 / 完成」语义。

⇒ ⇒ 「持续 N 拍后转移」MUST 与计数器条件**合取**
（例：`bit_last & baud_done`），MUST NOT 只用「计数已达上限」这一条件
（否则最后一位只发 1 拍）。

#### Scenario: 最后一位只持续一拍

- **WHEN** 转移条件只由「计数器已达上限」给出（未与「时长已满」合取）
- **THEN** 该条件一旦为真就**持续**为真 ⇒ 最后一位只持续 **1** 拍

### Requirement: 硬件值 MUST NOT 参与 C++ 控制流

硬件值（`ch_bool` / `ch_uint` 节点）MUST **NOT** 参与 C++ 控制流 ——
`if (硬件值)` / 三元 `? :` / `switch` / `while`-`for` 的行程条件 /
early `return` / 传给 `bool` 形参。

⇒ ⇒ 这些写法在**建图期**求值（`ch_bool` / `ch_uint` 的 `operator uint64_t()`
**只对 const 节点返真值**）⇒ 条件逻辑**不发射**（无 `else` 的 `if` 分支体
**永不发射**、有 `else` 的**只发射 else**）。

⇒ ⇒ 条件逻辑 MUST 只用硬件原语：`transition_when` / `select`。

#### Scenario: 无 else 的 if 分支体不发射

- **WHEN** `if (硬件值) { ... }` 的条件是非 const 节点
- **THEN** 分支体 MUST **不发射**（在 describe 期即被求值为假）

### Requirement: FSM 的寄存器与输出 MUST 有唯一写入点 / 驱动点

每个寄存器 MUST 有**唯一**的 `next` 写入点；每个输出 MUST **只**驱动一次。

⇒ ⇒ 各状态的候选值 MUST 用 `select`（按 `sm.is_in(...)` 多路选择）组合成
**单一表达式**，写在 `sm.build()` **之后**。

#### Scenario: 多状态各驱动同一输出

- **WHEN** 同一个 `ch_out` 在**两个以上** state's `on_active` 闭包内被驱动
- **THEN** 只有**最后发射**的那个驱动生效
- **AND** 正确写法 MUST 是 `build()` 之后的**单一** `select` 表达式
