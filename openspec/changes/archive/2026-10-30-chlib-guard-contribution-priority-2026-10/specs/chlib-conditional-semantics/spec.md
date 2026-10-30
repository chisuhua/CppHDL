# Capability: chlib-conditional-semantics

## ADDED Requirements

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

## MODIFIED Requirements

### Requirement: 条件构造的**权威表面** MUST 唯一

表达式形式 MUST 是 `multi_if` / `if_then`（`include/chlib/if.h`）；
`priority_if` / `priority_if_then` MUST **不存在**（⇒ 与 `multi_if` 逐行等价
⇒ 名字承诺「优先级 / 互斥优化」但**未兑现** ⇒ Zero-Debt + ADR-043 先例）。

⇒ ⇒ 条件构造 MUST NOT 存在**两个**行为等价的入口。

⚠️ **已知限制（表达式形式的接受面）** —— ADR-051 实施期实测（2026-10-30）：

1. **不能直接赋给输出端口**：`io().x = if_then(...)` MUST 编译不过
   （⇒ 报 `multi_if has no member named 'impl'`）⇒ ⇒ MUST 先显式转成目标宽度
   （`ch_uint<N>(...)`）；
2. **分支值 MUST 同类型**：`multi_if<T>` 要求**所有**分支值同类型 `T`
   （⇒ `a + b` 是 **9 位**、`a - b` / `a | b` 是 **8 位** ⇒ 模板推导冲突）
   ⇒ ⇒ 每个分支 MUST 显式收敛到同一宽度；
3. **条件不能是布尔输入端口**：`if_then(const ch_bool&, const T&)` 的条件 MUST
   **不能**是 `ch_in<ch_bool>` 端口（⇒ `no matching function`）
   ⇒ ⇒ 语句块形式已把条件参数**模板化**（接受任意硬件操作数），表达式形式
   **尚未** ⇒ ⇒ 二者构成**可表达性不对称**。

⚠️ 这三条是**表达式表面自身**的接受面限制 ⇒ ⇒ 「权威表面」的**可用域** MUST
被记录 ⇒ ⇒ 否则该表面的限制只活在文档注释里 ⇒ ⇒ 后续修复者**无从判断**哪些
限制是**契约**、哪些是**偶然**。

#### Scenario: priority_if 不再存在

- **WHEN** 代码尝试使用 `priority_if` / `priority_if_then`
- **THEN** MUST **编译不过**（无匹配重载）

#### Scenario: 表达式形式的接受面限制 MUST 可查

- **WHEN** 使用者尝试把 `multi_if` 直接赋给 `ch_out`、或混用不同宽度的分支值、
      或以 `ch_in<ch_bool>` 端口作为条件
- **THEN** 该限制 MUST 有**明文契约**（⇒ 而非只存在于注释里）
- **AND** 该限制的**修复状态** MUST 可判定（已修 / 未修）
