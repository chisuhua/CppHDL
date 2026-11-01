# Design：未驱动字段的只读诊断 API（B7）

> 决策正本 = `.rddf/checks/architect-decisions-pending-2026-11.md` 的 **B7 裁决**
> （只读诊断 API · 不改既有语义 · 「所有护栏受益」须配可机械核验的判据）；
> 本文件承载**实测事实 / 手推金标 / 实现裁决**。

## 1. 实测事实（本次读代码 + 探针确认，**不是推理**）

### 1.1 「静默」的机制

`ch_uint::operator<<=`（`include/core/uint.h:147`）：

```cpp
if (this->node_impl_) {
    this->node_impl_->set_src(0, src_lnode.impl());      // ① 传播给调用方
} else {
    this->node_impl_ = node_builder::instance()
        .build_unary_operation(ch_op::assign, src_lnode, N, ...);  // ② 另建节点
}                                                        //    ⇒ 只绑本地副本
```

⇒ ⇒ **目标没有节点**时，`<<=` 会**另建**一个 assign 节点并**只绑到本地副本**
⇒ ⇒ 调用方**看不到**任何效果（**静默失效**）；
⇒ ⇒ **目标有节点**时 ⇒ ⇒ `set_src(0, …)` ⇒ ⇒ **传播**。

⚠️ 而 `logic_buffer()` 的默认构造 = `node_impl_(nullptr)`
（`include/lnode/logic_buffer.tpp:13`）⇒ ⇒ **「没有节点」是可达状态**
⇒ ⇒ 「缺 `as_master()`」这一形态确实存在（`no_node`）。

### 1.2 「有源但未连到顶层」**无法**与「完全无源」区分

框架**不跟踪**「某节点是否已连到任何顶层输入」⇒ ⇒ 诊断**不**回答
「这个设计会不会跑起来」（spec 的「已知边界」Scenario 已如实登记）。

## 2. 实现裁决：为什么是**枚举**而不是三个 bool

两个「响亮拒绝」的实现需要区分**四种**情形：

| 情形 | helper 的动作 |
|---|---|
| `no_node`（无节点） | **抛**（`<<=` 会静默另建节点） |
| `literal`（自身是字面量） | **抛**（驱动它无效） |
| `driven`（被真信号驱动） | **抛**（再驱动 = last-wins ⇒ 静默接管） |
| `undriven`（有节点、无源） | **允许**（`set_src` 会传播） |
| `driven_by_literal`（被字面量驱动 = 组合子的默认值） | **允许**（ADR-054 形态 E 的覆盖） |

⇒ 若用三个 bool 组合表达 ⇒ ⇒ **两份实现（`stream.h` 与 `stream_pipeline.h`）
必然漂移** ⇒ ⇒ ⇒ 用**同一个枚举** ⇒ ⇒ **判据只有一处** ⇒ ⇒
护栏 R11 / R13 追加「MUST 用 `diagnose_drive(` 且 MUST NOT 重新实现
（body 里出现 `type_lit` / `num_srcs()` ⇒ 红）」。

## 3. 手推金标（五态的构造方式）

| 状态 | 构造 | 读到的 `impl()` |
|---|---|---|
| `no_node` | `ch_bool never_used;`（从未用过） | `nullptr` |
| `literal` | `ch_bool lit(true);` | literal 节点 |
| `undriven` | `ch_stream<T> s; s.as_master();` ⇒ `s.ready` | proxy，**无 src** |
| `driven_by_literal` | `s.ready <<= lit;` | proxy，src 是 literal |
| `driven` | `s.ready <<= real;`（`real` 是 `op` 节点） | proxy，src 是真信号 |

## 4. 变异验证（**手动投毒**，手动 ⇒ 因为 `diagnose_drive` 在 `core/`，
而既有投毒框架只覆盖 chlib 的四个头 ⇒ ⇒ 记录结果，见 `tasks.md` C3）

| 变异 | 改法 | 诊断判据 | 既有 `test_stream_builder` |
|---|---|---|---|
| **M1** | `undriven` → `driven` | **红**（2 用例 failed） | 绿 |
| **M2** | `no_node` → `driven` | **红**（1 用例 failed） | 绿 |

⚠️⚠️ **实测撞出来的关键事实**：M2 期间 `test_stream_builder` 仍绿 ⇒ ⇒
**`no_node` 与 `driven` 在 helper 里都导致抛异常** ⇒ ⇒ **helper 无法区分
二者** ⇒ ⇒ ⇒ **只有诊断判据能区分** ⇒ ⇒ ⇒ **诊断判据不是冗余的**
（这条已写进 spec 的 Scenario「『未驱动』MUST NOT 被读成『已驱动』」）。

⚠️ **M1 不影响 `test_stream_builder` 的原因**：该文件里的「缺 `as_master()`」
用例走的是 **`no_node`**（`impl() == nullptr`）⇒ ⇒ 与 `undriven` **正交**
⇒ ⇒ 两个状态**各自**需要**各自**的变异 ⇒ ⇒ 只做一个变异会**漏掉**另一格。

## 5. 已知边界（如实登记）

- 诊断**不**回答「这个设计会不会跑起来」（框架不跟踪「是否连到顶层」）；
- 诊断**不**改变读取行为（读未驱动字段仍返回 0）⇒ ⇒ **零回归**；
- 「被字面量驱动」与「被真信号驱动」的区别**依赖** `src(0)` 的类型 ⇒ ⇒
  若某个组合子把默认值接成**非** literal 的常量节点 ⇒ ⇒ 诊断会报 `driven`
  ⇒ ⇒ 那是**该组合子的形态问题**，不是诊断的问题（⇒ ⇒ 靠 R2「默认驱动用
  `<<=`（目标 = 非 literal 的 field proxy）」那类判据守住）。
