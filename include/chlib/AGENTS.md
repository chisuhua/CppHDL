# AGENTS.md - CHLib Subsystem

Generated: 2026-03-05 | See parent: `AGENTS.md` (root)

## OVERVIEW
High-level HDL components: streams, FIFOs, pipelines, bundles, arithmetic, memory, AXI4-Lite.

## STRUCTURE
```
include/chlib/
├── arithmetic.h           # Basic arithmetic ops
├── arithmetic_advance.h   # Advanced arithmetic
├── bitwise.h              # Bitwise operations
├── combinational.h        # Combinational logic
├── fifo.h                 # FIFO implementations
├── if.h / if_stmt.h       # Conditional logic
├── guard_contribution.h   # 「守卫贡献」组合器（**内部**；ADR-050 + ADR-051 共用）
├── logic.h                # Basic logic gates
├── memory.h               # Memory components
├── onehot.h               # One-hot encoding
├── selector_arbiter.h     # Selection/arbitration
├── sequential.h           # Sequential logic
├── state_machine.h        # FSM state machine DSL (ch_state_machine)
├── stream*.h              # Stream operators, pipelines, arbiters
├── switch.h               # Switch/routing logic
└── stream_width_adapter.h # Width conversion
```

## KEY FILES
| Task | File |
|------|------|
| Stream ops | chlib/stream.h, chlib/stream_operators.h |
| FIFO impl | chlib/fifo.h |
| Pipeline | chlib/stream_pipeline.h |
| Arbiter | chlib/selector_arbiter.h, chlib/stream_arbiter.h |
| Width adapter | chlib/stream_width_adapter.h |
| Sequential logic | chlib/sequential.h |
| Memory | chlib/memory.h |
| FSM state machine | chlib/state_machine.h（**契约见 capability `chlib-state-machine-semantics`**） |
| 条件构造（语句块） | chlib/if_stmt.h（**契约见 capability `chlib-conditional-semantics`**） |
| 条件构造（表达式） | chlib/if.h（`multi_if` / `if_then`） |
| 守卫贡献组合器 | chlib/guard_contribution.h（**内部**头 ⇒ **不**进 `chlib.h` 聚合器） |

## 条件构造的语义模型（`_if` 语句块 + `if_then` 表达式）

⚠️ **契约权威** = capability `chlib-conditional-semantics`（ADR-051）。以下
是给使用者的**速查**；完整推导见 `docs/adr/ADR-051-chlib-conditional-semantics.md`。

### 两种形式

| 形式 | 入口 | 语义 |
|---|---|---|
| **语句块** | `_if(c).then(body).elif(c2)...else_()...end()` | 守卫贡献（见下） |
| **表达式** | `if_then(c, v).elif(c2, v2)...else_(v3)` | 从后往前构 `select` 链 ⇒ **先匹配者胜** |

⇒ 两者**优先级顺序等价**（⚠️ **带 `else_` 的**序列才**逐位**等价 —— 无
`else_` 时表达式形式的尾部 = **末分支的值**，而语句块形式的寄存器 = **保持**
⇒ ⇒ 两者**刻意不同**）。

### 5 条纪律（写 `_if` 时 MUST 遵守）

1. **条件赋值 MUST 经 `conditional_assign(target).next() = value`** ——
   未走捕获点的普通 `reg->next = v` / `io().x = v` **不被条件化**（= **无条件**
   生效 ⇒ **静默**错误）⇒ 静态护栏 `scripts/verify_if_stmt_wiring.sh` 会检出；
2. **`end()` 是「捕获」，不是「条件成立才执行」** —— 它遍历**所有**分支体
   以收集候选值 ⇒ ⇒ 分支体在**建图期**执行 ⇒ ⇒ 分支体 MUST **无副作用**；
3. **优先级 = 先匹配者胜**（分支顺序即优先级）；
4. **默认尾部**：**寄存器**无 `else_` ⇒ 未匹配时**保持**；**输出端口** MUST 有
   `else_` 候选值 ⇒ ⇒ 缺失 ⇒ **构建期抛异常**（输出「保持」= 引用自身 =
   **组合环** ⇒ 拓扑排序 CHFATAL ⇒ **难**诊断）；
5. **嵌套** ⇒ 内层 MUST 先合成**一条**表达式、再作为外层该分支的候选值 ⇒ ⇒
   MUST 显式 `end()`。

### ⚠️ 框架层的 3 个实测约束（写 DSL / 扩 API 时）

- **`ch_in<ch_bool>` 不能绑到 `const ch_bool&`** ⇒ 条件参数 MUST 是**模板**
  （接受任意硬件操作数）—— 表达式形式 `if_then` **仍有**此限制；
- **`operator->` 只对类类型**对象**生效** ⇒ 指针上的 `reg->next` **不成立**
  ⇒ 须写 `(*reg)->next`；
- **lambda MUST 按值捕获目标指针** —— `conditional_assign(x)` 是**临时对象**
  ⇒ 捕获 `this` 会**悬垂**（实测：`regimpl_node_ is null`）。

## CONVENTIONS
- Namespace: `ch::chlib`
- Stream types: `ch_stream<T>`, `ch_flow<T>`
- Include via `include/chlib.h` (aggregator)
- Files under `bundle/` for stream/bundle types

## ANTI-PATTERNS
- **FSM 的 `on_active` 闭包内写寄存器 / 驱动输出**：`ch_state_machine::build()`
  **遍历所有** state 执行各自闭包 ⇒ 那些写入**必被覆盖**（`ch_reg::next` 是
  last-wins）⇒ 闭包内**只**写 `transition_when`，寄存器与输出写在
  `build()` **之后**（契约：`chlib-state-machine-semantics`；
  护栏：`scripts/verify_fsm_next_wiring.sh`）
- **用三元 `? :` 选择 `transition_when` 的 target**：target MUST 是**编译期常量**
  ⇒ 三元在**建图期**求值 ⇒ 另一个目标状态**永不可达**
- **条件块内裸写 `reg->next = v` / `io().x = v`**：**不被条件化**（= **无条件**
  生效）⇒ ⇒ MUST 走 `conditional_assign(target).next() = value`（契约：
  `chlib-conditional-semantics`；护栏：`scripts/verify_if_stmt_wiring.sh`）
- **给输出端口做条件赋值却漏写 `else_`**：输出「保持」= 引用自身 = **组合环**
  ⇒ ⇒ 构建期即被**显式拒绝**（**不**退化为「默认 0」）
- **`multi_if` 直接赋给 `ch_out`**：`multi_if` **不能**直接赋值（实测：
  `multi_if has no member named 'impl'`）⇒ ⇒ MUST 先显式转成目标宽度
  （`ch_uint<N>(...)`）；且 `multi_if<T>` 要求**所有分支值同类型 T**
  （`a + b` 是 9 位、`a - b` 是 8 位 ⇒ **推导冲突**）⇒ ⇒ 每个分支 MUST 显式
  收敛到同一宽度
- **Async stream patterns**: Not fully supported; avoid complex backpressure without FIFO
- **Stream without termination**: Always connect streams to sink or FIFO
- **Width mismatch**: Stream width adapters required for mismatched widths
- **Blocking in pipelines**: Use combinational/sequential components correctly

## RELATED DIRECTORIES
- tests/chlib/       # chlib component test sources (count in root AGENTS.md NOTES)
- include/bundle/    # Stream and Flow bundle definitions

## PHASE GATES
Follow root Zero-Debt Policy. chlib components are the user-facing API:
- New component → `chlib.h` aggregator update + `tests/chlib/test_*.cpp` test file
- Component uses stream → register in `chlib.h` aggregator, NOT individual include
- Width mismatches → stream_width_adapter required, document in component header
