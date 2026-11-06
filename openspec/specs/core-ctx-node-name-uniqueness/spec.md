# core-ctx-node-name-uniqueness Specification

## Purpose
定义 `ch::core::context` 内**节点名唯一性**的可控契约：★ ctx MUST 提供 **opt-in** 的
唯一性策略 ⇒ ★ 开启后同名节点 MUST 在**创建点** fail-fast ⇒ ★ 从而消除「解释器侧
不改名 / Verilator 侧自动改名」这一**两后端节点名分歧**的前提。

## Requirements

### Requirement: ctx MUST 提供 opt-in 的节点名唯一性策略

`ch::core::context` MUST 提供一个**可显式开启**的节点名唯一性策略（★ 例：
`kAllowDuplicates` / `kRejectDuplicates`）⇒ ★ ⇒ ★ **默认 MUST 为宽松**
（★ `kAllowDuplicates`）⇒ ★ ★ 默认行为 MUST NOT 改变既有代码。

⇒ ⇒ 策略开启后，`create_input` / `create_output` 等**创建点**检测到同名节点时
MUST **fail-fast**（★ 抛异常中止 ⇒ ★ **MUST NOT** 只打日志 —— ★ 日志不中止 ⇒ 不构成
安全网）。

⇒ ⇒ 该契约 MUST 由**两层**护栏核验：★ ① 开启策略后同名 MUST 抛异常（★ 行为层）；
★ ② 默认策略下同名 MUST **不抛**（★ 默认层 · 护住「不破坏既有代码」）。

⇒ ⇒ ⚠️ **静态析构期 MUST 有已定义的降级行为** ⇒ 见同名 Scenario（「记录 + 失败结果」）。

#### Scenario: 开启策略后同名节点 MUST 在创建点抛异常

- **WHEN** 唯一性策略已开启，★ 且创建了一个与既有节点**同名**的新节点
- **THEN** MUST **抛出异常**并中止（⇒ 不得只打印日志后继续）
- **AND** 错误信息 MUST 含**冲突的节点名**

#### Scenario: 默认策略 MUST 保持宽松

- **WHEN** **未**显式开启唯一性策略，★ 且创建了一个同名节点
- **THEN** MUST **不抛异常**（⇒ 既有代码行为不变）
- **AND** 该 ctx MUST 允许同名节点共存

#### Scenario: ctx MUST NOT 自动改名

- **WHEN** ctx 内出现同名节点
- **THEN** ctx MUST **不改名**（★ 既不自动加后缀，★ 也不静默替换）
- **AND** 名字冲突 MUST 只由「放行」或「报错」两种方式处理
      （★ 自动改名会让不同后端看到不同名字 ⇒ 契约按名匹配失效）

#### Scenario: 静态析构期 MUST 降级为「记录 + 失败结果」

- **WHEN** 唯一性策略已开启，且同名冲突发生在**静态析构期**
  （该期抛异常会 `std::terminate` ⇒ 既有 `CHFATAL_*` 一族宏在该期一律**不抛**）
- **THEN** ctx MUST **不抛异常**（MUST NOT 让异常逃出析构路径）
- **AND** 该冲突 MUST 被**记录**在 ctx 上（可查询）⇒ **MUST NOT** 静默丢弃
- **AND** 该次创建 MUST **失败**（节点 MUST NOT 进入 ctx）⇒ ⇒ 「记录 + 失败结果」二者
  缺一不可 ⇒ 否则该期仍是**未定义带**
- **AND** 该降级 MUST 只覆盖**静态析构期** ⇒ 正常期仍 MUST **抛异常**（fail-fast）

#### Scenario: 摘掉检测 MUST 使行为层护栏失败

- **WHEN** 创建点的同名检测被移除
- **THEN** 开启策略后的同名创建 MUST **不再抛异常**
- **AND** 行为层护栏 MUST 因此**失败**（⇒ 构成**判别性对照**）
