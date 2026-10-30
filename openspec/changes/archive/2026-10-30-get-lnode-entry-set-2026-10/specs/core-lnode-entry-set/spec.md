# Capability: core-lnode-entry-set

## Purpose

定义 CppHDL 核心层 `get_lnode`（操作数 → `lnode<T>` 的唯一转换入口）的**入口集合
契约**：它只允许存在**两个**入口，MUST NOT 存在第三个**无约束**的通用入口。

> ⚠️ **archive 注意**：OpenSpec 的 archive 流程**不合并** delta 文件的 `## Purpose`
> ⇒ 归档后 MUST 另行补写主 spec 的 `## Purpose`（先例：`axi-interconnect-routing`）。
> 凡 MUST 长期存在的契约性内容一律写进 **Requirement 正文**。

## ADDED Requirements

### Requirement: `get_lnode` MUST NOT 有**无约束的通用**入口

`get_lnode` 的**通用**入口 MUST 恰为**一个**：`get_lnode(const T &)
requires { t.impl(); }`（`bundle` 亦走它）；另有**恒等**版
`get_lnode(const lnode<T> &)`。

⇒ ⇒ MUST NOT 存在**无约束的通用**入口（`template <typename T> …(const T &)`
且**无** `requires` 子句）：它对**有** `impl()` 的类型**永不选中**，对**无**
`impl()` 的类型则产生**假阳性能力探测**（返回类型显式 ⇒ `requires` **不实例化**
函数体）。

⇒ ⇒ **类型专用**入口（如 `get_lnode(const ch_uint<N> &)`）**允许**存在。

⇒ ⇒ 该契约 MUST 由**两层**可执行护栏核验（编译期 + 源码级扫描）⇒ 详见
「护栏 MUST 有两层」Scenario。

#### Scenario: 护栏 MUST 有两层（只靠编译期会漏报）

- **WHEN** 一个**无约束的通用** `get_lnode` 入口被加入某个**未被**编译期 fixture
      include 的头文件
- **THEN** **编译期**判据 MUST **看不见**它（⇒ 它只看得到被 include 的头文件）
- **AND** **源码级扫描** MUST 报违规（⇒ 覆盖**全部**声明点）
- **AND** 两者 MUST **同批**落地（⇒ 否则修复期间无网）

#### Scenario: 无约束的通用入口 MUST NOT 存在

- **WHEN** 存在一个**无约束**的 `get_lnode(const T &)` 通用模板
- **THEN** 该调用 MUST **违反**本条款（⇒ 无论其函数体是否可用）
- **AND** `get_lnode` 的**通用**入口数 MUST 恰为 **1**

#### Scenario: `requires` 探测 MUST 对无 `impl()` 的类型为 `false`

- **WHEN** 对**无** `impl()` 成员的类型 `X` 求值 `requires { get_lnode(x); }`
- **THEN** MUST 得 **`false`**（⇒ MUST NOT 因「函数体不实例化」而返回 `true`）
- **AND** 该判据 MUST 以**编译期断言**形式常驻（破坏它 ⇒ 断言转红）

#### Scenario: 对 bundle 的合法调用 MUST 仍可用

- **WHEN** 对 `bundle_base` 派生类型调用 `get_lnode`
- **THEN** MUST **编译通过**（走受约束的通用入口 —— `bundle_base` 继承
  `logic_buffer::impl()`）
- **AND** 结果 MUST 是 `lnode<该 bundle 类型>`

#### Scenario: 无匹配时的诊断 MUST 指向调用点

- **WHEN** 对无 `impl()` 的类型调用 `get_lnode`
- **THEN** 报错 MUST 指向**用户的调用点** + 列出候选并说明**约束不满足**
- **AND** MUST NOT 把错误帧指向框架内部实现文件（例：`bundle_base.h` 的某个行号）
