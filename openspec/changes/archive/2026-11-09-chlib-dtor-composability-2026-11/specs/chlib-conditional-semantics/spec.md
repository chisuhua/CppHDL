# Capability: chlib-conditional-semantics

## ADDED Requirements

### Requirement: 条件块类型 MUST NOT 被当作「有虚析构的类」的按值成员或基类

`conditional_block` 的析构是 **`noexcept(false)`**（⇒ 兜底 `end()` 可能抛 ⇒
「输出缺默认候选」的拒绝 MUST 在**析构路径上可捕获）⇒ ⇒ **本类型**：

> **析构非 `noexcept` 的类型** MUST NOT 作为「**有虚析构**的类」的**按值成员**或**基类**
> —— 派生类的隐式析构异常规格由子对象推导 ⇒ `noexcept(false)` 子对象 ⇒ 派生类析构
> `noexcept(false)` ⇒ ⇒ override 一个 `noexcept` 虚析构 ⇒ **编译错误**
> （`looser exception specification on overriding virtual function`）。

⚠️ **触发面**不限于「基类显式写了 `noexcept`」—— `virtual ~Base() = default;` 是
**隐式** `noexcept` ⇒ 同样触发 ⇒ ⇒ 真实条件 = 「**基类链上有虚析构**」。

⚠️ **本类型维持 `noexcept(false)`**（ADR-067 决策 ③）：其析构**兜底合成**是
**功能**（不只是检出）⇒ ⇒ 改 `noexcept` 会**丢功能** ⇒ ⇒ 「可组合形态」
**可行但当前不做**（无需求 + 固定成本 + 该能力线的析构路径**当前零测试背书**）
⇒ ⇒ 若日后确有需求 ⇒ **独立立项**。

⚠️ 契约**不承诺**「编译器会说人话」—— 类型系统无法在成员声明处产出**可行动**诊断
⇒ ⇒ 契约的价值在**文档 + capability + 评审**，**不是**编译期提示。

⚠️ **唯一权威** = `docs/adr/ADR-067-dtor-exception-spec-composability.md`
（决策 ③/④ · **Accepted**）。

#### Scenario: 条件块作为按值成员 MUST 编译失败（显式可查）

- **WHEN** 条件块对象被**按值**持有一个**有虚析构**的派生类里
- **THEN** 编译 MUST 失败，且诊断 MUST 指向「looser exception specification」
      （⇒ ⇒ 这就是「不可组合」的**可观测**形态 ⇒ ⇒ 用户据此改用指针持有）
- **AND** 该诊断本身**不算**「可行动」⇒ ⇒ 契约的**行动指引**在头注释与本条

#### Scenario: 条件块的析构契约 MUST 维持 noexcept(false)

- **WHEN** 条件块对象在**非展开期**被销毁且其兜底 `end()` 会抛
      （输出缺 `else_` 候选值）
- **THEN** 该异常 MUST **可捕获**（⇒ ⇒ 析构异常规格 MUST 为 `noexcept(false)`）
- **AND** MUST NOT 为换取「可组合性」而把该异常降级为 `std::terminate`
      （⇒ ⇒ 兜底合成这条**功能** MUST 保留）
