# Capability: component-io-const-access

## Purpose

定义 `ch::Component` 的 IO 访问器（★ `__io` 宏生成的 `io()`）在 **const 上下文**中的
可达性契约：★ const 对象 / const 引用 MUST 能读到其 IO ⇒ ★ 否则下游只能在 const
接口里放弃 const 正确性。

## ADDED Requirements

### Requirement: `__io` 生成的 IO 访问器 MUST 提供 const 版本

`__io(...)` 宏生成的 `io()` MUST 同时提供**非 const** 与 **const** 两个重载 ⇒ ★
`const io_type& io() const` ⇒ ★ ⇒ ★ **const 上下文 MUST 可读 IO**（★ 端口的
`impl()` / `name()` / `is_valid()`）。

⇒ ⇒ 该契约 MUST 由**编译期断言**核验（★ `static_assert` 或等价的编译期探针）。

#### Scenario: const 上下文 MUST 可读 Component 的 IO

- **WHEN** 一个 `ch::Component` 派生类对象经 **const 引用 / const 指针**访问
- **THEN** `io()` MUST 可调用并返回 **const** IO 引用
- **AND** 读取其端口的 `impl()` / `name()` / `is_valid()` MUST 编译通过

#### Scenario: 移除 const 重载 MUST 使编译期断言失败

- **WHEN** `__io` 宏的 const 重载被移除
- **THEN** 上述编译期断言 MUST **编译失败**（⇒ 构成**判别性对照**）
