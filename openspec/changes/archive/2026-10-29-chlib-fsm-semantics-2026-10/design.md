# Design: chlib-fsm-semantics-2026-10

## 0. 一句话结论

`ch_state_machine` 有**两条**未被文档化的陷阱（`build()` 遍历所有 state ⇒
闭包内写 `reg->next` 必被覆盖；`transition_when` 的 target 必须是编译期常量）
⇒ **同一缺陷在仓内两次真实发生**且都被 parity 掩盖 ⇒ 本 change 把它们
**写进契约**（新 capability）+ **修掉框架自己的坏示例** + **加静态护栏**
⇒ 真正的**能力变更**（`ch_reg` 多源 next）留给 ADR。

## 1. 只读盘点（**事实**，非推断）

### 1.1 全仓 `on_active` 使用点

| 位置 | 形态 | 是否受害 |
|---|---|---|
| `include/chlib/state_machine.h` | **头注释的示例** + 成员函数**定义** | ⚠️ **示例本身是坏写法**（`if (io().start) sm.transition_to(...)`）|
| `examples/spinalhdl-ported/uart/uart_tx.cpp` | 4 个闭包，**只**写 `transition_when` | ❌ 已是正确形态（`uart-tx-fsm-live-logic-2026-10` 修的）|
| `tests/test_verilator_complex_e2e.cpp` | 4 个闭包，**只**写 `transition_when` | ❌ 已是正确形态（`spi-fsm-fixture-live-logic-2026-10` 修的）|
| `tests/test_state_machine.cpp` | 5 个闭包，**只**写 C++ `bool` 标志 | ❌ 无害（不碰硬件节点）|

⇒ ⇒ **当前仓内已无受害站点** ⇒ ⇒ 这是「**框架的诱导性设计**」，不是存量 bug
⇒ ⇒ 处置 = **契约 + 文档 + 护栏**（而不是继续逐个修 example）。

### 1.2 `examples/spinalhdl-ported/spi/spi_controller.cpp`

⚠️ **它不用 `ch_state_machine`**（手写 `ch_reg<ch_uint<2>>` + `select`）⇒
**本来就是正确形态**：`state_reg->next = next_state;`（**单一**写入点）·
输出**各驱动一次** · 条件用 `select`（硬件值**不**参与 C++ 控制流）
⇒ ⇒ **不需要**修（主 spec 里的「非简化」描述指的就是这一点）。

### 1.3 两条陷阱的**机制**证据

| 陷阱 | 证据 |
|---|---|
| **A** `build()` 遍历所有 state | `include/chlib/state_machine.h` 的 `build()`：按 state index 遍历执行**每个** `states_[i].active_action()`（注释原文：「run every state's active action at describe time」）|
| **A** 覆盖语义 | `regimpl::set_next` = `set_src(0, next)`（`include/ast/ast_nodes.h:47-54`）⇒ last-wins；`ch_out::operator=` 同型（`include/core/io.h:274`）|
| **B** target 编译期常量 | `transition_when(const ch_bool&, StateEnum)`（`state_machine.h:228-231`）⇒ target 是**枚举形参** ⇒ 必须在**调用点**定死 ⇒ 三元在调用点求值 |
| **B** 三元恒假 | `ch_bool::operator uint64_t()` **只对 const 节点返真值**（`src/lnode/bool.cpp:44-49`）|

### 1.4 两次真实发生的**同型**证据

| commit | 位置 | 现象 |
|---|---|---|
| `5801911`（`uart-tx-fsm-live-logic-2026-10`） | `examples/spinalhdl-ported/uart/uart_tx.cpp` | 生成的 Verilog 里输出是**两个常量**、状态寄存器**自环**、`is_in` 比较节点**零消费者** |
| `ed5fe9c`（`spi-fsm-fixture-live-logic-2026-10`） | `tests/test_verilator_complex_e2e.cpp` | `shift` 恒 `0x00`、状态卡死 `TRANSFER`、`mosi` 恒 0、`bit_count` 从不复位 |

⇒ ⇒ 两次的断言都**只有 parity** ⇒ 两后端**同构** ⇒ **恒绿** ⇒ 「假绿」。

## 2. 契约的 6 条（= 新 capability 的 ADDED ×6）

见 `specs/chlib-state-machine-semantics/spec.md`。设计要点：

1. **`ch_reg` 的 `next` 写入是 last-wins（现状契约）** —— 把**实现语义写死**
   ⇒ 将来若要加「多源 next」，MUST 走 **MODIFIED** delta（= 一次显式裁决）
   ⇒ ⇒ **把「静默」变成「受控」**。
2. **`build()` 遍历所有 state** ⇒ 闭包内 MUST **只**写 `transition_when`。
3. **`transition_when` 的 target 编译期常量** ⇒ MUST 拆成**多条**互斥条件。
4. **`transition_when` 的条件电平敏感** ⇒ 「持续 N 拍」MUST 与计数器**合取**。
5. **硬件值 MUST NOT 参与 C++ 控制流**。
6. **寄存器与输出 MUST 有唯一写入点 / 驱动点**。

⚠️ 第 1 条与第 6 条是**同一条**的两面（第 1 条说「语义是覆盖」，第 6 条说
「因此必须单点」）；分开写是因为**将来**若改语义，只需 MODIFIED 第 1 条。

## 3. 框架头注释的改法

`include/chlib/state_machine.h` 的 `@file` 头注释：

| 现状 | 改成 |
|---|---|
| Usage Example 用 `if (io().start) sm.transition_to(...)`（**建图期恒假**） | 用 `sm.transition_when(start_sig, ...)`，且 `start_sig` 是**节点**不是 bool |
| 只有一条 NOTE（`if (ch_bool)` 陷阱） | **5 条纪律** + **2 条陷阱**（A：`build()` 遍历；B：target 编译期常量） |
| 无「现状契约」段 | 新增：把 `build()` 的 select 树合成规则 + 「后注册者优先」写死 |

⚠️ **只改注释，零行为变更** ⇒ 爆炸半径为零 ⇒ 不需要 ADR。

## 4. 静态护栏 `scripts/verify_fsm_next_wiring.sh`

### 4.1 判据

- **R1**：某个 state's `on_active` 闭包体内 MUST NOT 出现 `<reg>->next = ...`
- **R2**：某个 state's `on_active` 闭包体内 MUST NOT 出现
  `io().<port> = ...` / `io().<port> <<= ...`
  （**比较** `io().x == y` 合法 ⇒ 不算）

### 4.2 实现（`scripts/lib/fsm_wiring.py`，护栏与自检**共用同一份**）

1. **抹掉注释与字符串/字符字面量**（替换成等长空格 ⇒ 偏移量与行号不变）
   ⇒ 注释/字符串里的 `->next =` **不会**误报；
2. 找 `on_active(` ⇒ 只有**其后先出现 capture list `[`** 的才算 **lambda 实参**
   ⇒ 框架自身的成员函数**定义**（`state_def& on_active(state_action_t action)`）
   **不会**被误判；
3. 解析 capture list（支持嵌套 `[]`）⇒ 参数列表 ⇒ 花括号**配平**；
4. ⚠️ **配平后的下一个非空白字符 MUST 是 `)`** ⇒ 否则说明**配平到了错误的 `}`**
   （典型：源码花括号本身不平衡 ⇒ 配平一路吃到更外层）⇒ **fail-closed**；
5. 在闭包体内跑 R1/R2 的正则。

### 4.3 能力边界（如实声明）

- **已覆盖**：整行/行尾/跨行块注释、字符串/字符字面量、capture list 的嵌套
  `[]`、lambda 参数列表、花括号配平；
- **未覆盖（漏报方向）**：raw string（`R"(...)"`）按普通字符串处理；
  宏展开后的形态；**通过函数调用间接**写 `reg->next` / `io().x`
  （如 `on_active([&]{ helper(); })` 且 `helper()` 内部写了寄存器）；
- **方向**：除 `parse-error`（fail-closed）外，**宁可漏报、不误报**。

### 4.4 判别力证据（**12 个 fixture** + 真实缺陷）

| # | fixture | 期望 |
|---|---|---|
| 1 | `fx_clean` 闭包内只写 `transition_when`（`io().start` 作操作数） | PASS |
| 2 | `fx_reg_next` 闭包内 `reg->next = ...` | **违规**（R1） |
| 3 | `fx_io_assign` 闭包内 `io().out = ...` | **违规**（R2） |
| 4 | `fx_io_assign_shl` 闭包内 `io().out <<= ...` | **违规**（R2） |
| 5 | `fx_outside` 闭包**外**写 `reg->next` | PASS |
| 6 | `fx_compare` 闭包内 `io().x == lit`（作操作数） | PASS |
| 7 | `fx_comment` 注释里的 `->next =` | PASS |
| 8 | `fx_string` 字符串里的 `->next =` | PASS |
| 9 | `fx_nested_lambda` 闭包内嵌套 lambda，其内有 `->next` | **违规** |
| 10 | `fx_unbalanced` 花括号不配平 | **违规**（`parse-error`，fail-closed） |
| 11 | `fx_member_def` `on_active` 的**成员函数定义** | PASS |
| 12 | `fx_no_parens_lambda` `on_active([&]{...})`（无参数列表） | PASS |

⭐ **对真实历史缺陷的端到端验证**：把
`tests/test_verilator_complex_e2e.cpp` 的 `describe()` 换回 `5801911` 的旧版
⇒ 护栏报出 **5** 处 `reg-next-in-closure`
（`start_latch->next` / `shift->next` ×2 / `bit_count->next` ×2）
⇒ ⇒ 护栏**确实**能抓住当初那个**真实的**缺陷。

## 5. 待裁决（**architect / ADR**）：`ch_reg` 的「多源 next」

⚠️ **本 change 不裁决**（改 `regimpl::set_next` 的语义 = **公开契约变更** ⇒
需 ADR ⇒ 属 architect 职责）。决策材料（供 ADR 取用）：

**问题**：`ch_reg` 只能有 **1** 个 next 源 ⇒ 「每状态各写一次」无法表达
多状态 FSM ⇒ 用户被迫把所有状态逻辑**塞进一条 `select` 表达式**（可读性差）。

| 选项 | 做法 | 优点 | 代价 |
|---|---|---|---|
| **A（现状）** | 单源 + `select` 组合 | 零变更；语义简单 | 可读性差；嵌套深 |
| **B** | `ch_reg` 支持**多源** next（按注册顺序合成 select 树） | 「每状态各写一次」可表达 | **改公开语义** ⇒ 既有 last-wins 行为**静默改变** ⇒ 需全仓审计 |
| **C** | 给 `ch_state_machine` 加一个「状态局部寄存器」抽象（每状态一份 + 出口 mux） | 语义直观 | 新抽象 ⇒ 新 API ⇒ 更大改动 |

⚠️ 选项 B 的**静默改变**是决定性代价：现有代码里「多写」的那一处**正是缺陷**
（`uart_tx` / `complex_e2e`）⇒ 若改成「多源合成」，那些代码会**变成合法的**
⇒ ⇒ 必须先有护栏（本 change 交付）⇒ ⇒ **本 change 是选项 B 的前置**。

## 6. 范围外（**明确不做**）

- **不**改框架**行为**（`ch_reg` 多源 next / `build()` 的遍历语义）⇒ ADR；
- **不**引入 `if (硬件值)` 的**运行期**支持；
- **不**给护栏加「例外机制」（当前**无**违规站点 ⇒ 不需要 ⇒ 将来真出现
  合法例外时再加，且**必须**窄化 + 显式打印）；
- **不**改 `examples/spinalhdl-ported/spi/spi_controller.cpp`（已是正确形态）。
