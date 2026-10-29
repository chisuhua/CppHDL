# ADR-050: `ch_state_machine` 的状态动作语义 —— 裁决「状态守卫的组合贡献」与死 / 误导 API 的物理移除

- Status: Accepted（proposed 2026-10-29）
- Date: 2026-10-29
- Author: build-agent（architect role）
- Parent: ADR-016（`ch_reg<T>` 继承设计 —— `next` 单源的由来）、ADR-045（`chlib` stream 语义契约 —— 「自保护优于纪律」的先例）、ADR-043（bundle 设施物理移除 —— 死 API 直接删、无别名期的先例）、ADR-033（命名空间污染）
- 关联 OpenSpec change：**待立项**（实施本 ADR）
- 关联 capability：`chlib-state-machine-semantics`（**MODIFIED ×1**：第 ⑥ 条）
- 前置审查：本 ADR 的分析经 **oracle 独立复核（两轮）+ metis 缺口分析**（5 处判断被修正、2 个 BLOCKER 被采纳；见 §7）
- 编号：⚠️ **ADR-049 已被 CLINT `mtime` 决策预留**（`.rddf/improvements/clint-mtime-autoincrement-2026-10.md:74,84`）⇒ 本 ADR 取 **050**（编号连续性缺口是**有意**的，不是缺卷）

## 1. Context

### 1.1 触发

`ch_state_machine`（`include/chlib/state_machine.h`）是仓内唯一的 FSM DSL，而它有**两条**此前完全未文档化的陷阱 ⇒ 同一缺陷在仓内**两次**真实发生，两次都被「**只有 parity**」的断言掩盖 ⇒ 长期「假绿」：

| 现象 | 机制 | 真实发生处 |
|---|---|---|
| 闭包内写寄存器 / 输出**必被覆盖** | `build()` **遍历所有** state 执行各自 `on_active`；`ch_reg::next` 与 `ch_out::operator=` 都是 `set_src(0,…)` ⇒ **last-wins** | `uart_tx.cpp`（FSM 从未工作）· `test_verilator_complex_e2e.cpp`（SPI fixture 从未工作） |
| `transition_when` 的 target 必须是**编译期常量** | target 是枚举形参 ⇒ **调用点固化** ⇒ 三元 `cond ? A : B` 在**建图期**求值（`ch_bool::operator uint64_t()` 只对 const 节点返真值）⇒ 恒取一支 ⇒ 另一目标**永不可达** | 同上（`last_bit ? STOP : TRANSFER`） |

上一 change（`chlib-fsm-semantics-2026-10`）已把这两条写成契约 + 静态护栏（`scripts/verify_fsm_next_wiring.sh`）⇒ **本 ADR 处理的是更上位的根因**。

### 1.2 根因：实现与 2026-03-30 的设计文档**语义分歧**

`docs/architecture/2026-03-30-state-machine-dsl-design.md`（状态：**设计草稿**）明确 `on_active` ≡ SpinalHDL `whenIsActive` = **状态守卫**语义（§6.3：`if (state_reg == STATE_A) { 执行 state_A 的 on_active; }`；§5 推荐实现里每个状态各自写 `io().uart_tx = …`、`bit_counter = 0`）⇒ **设计 = 守卫，实现 = 展平** ⇒ 上述现象、以及 §1.3 的全部死 / 误导 API，**都是这一个分歧的投影**。

⚠️ **对设计文档的取代关系**：本 ADR **部分取代** `2026-03-30-state-machine-dsl-design.md` —— 其 §3–§5 规定的 `on_entry` / `on_exit` / `CH_STATE_MACHINE*` 宏与 §6.3 的**运行时** entry/exit 语义**不再实现**（D2 / §4「不在本次范围」）；其 §5 示例里的 `if (io().start) sm.transition_to(...)` 是**坏写法**（建图期恒假）⇒ **不**作为正确形态的权威；`set_entry` 保留但硬化（D2）。

### 1.3 死 / 误导 API 盘点（只读盘点；消费者数已全仓核实）

| API | 缺陷 |
|---|---|
| `on_exit` / `exit_action` | **从不执行** |
| `on_entry` / `entry_action` | 只对 entry 态在 **build 期**执行一次（**不是**运行时 entry action） |
| `tick_state()` | 声称执行「当前态的 `on_active`」，但 `current_state()` 返回**编译期** entry ⇒ 永远跑 entry 态闭包；二次执行**重复发射节点** |
| `state_changed` | 声明后**从不赋值** |
| `current_state()` | 返回**编译期** entry_state（运行时状态须用 `current_state_uint()` / `is_in()`） |
| `set_entry()` | `state_reg` 初值在**构造期**定死为 `0_d` ⇒ `set_entry(非 0 枚举)` = **静默错误的复位态**；且 `entry_state` 字段**未在 ctor 初始化列表里** ⇒ 读它是 **UB** |
| `then()` / `next_state_on_exit` / `has_exit_transition` | legacy 自动退出转移；零消费者 |
| `is_built()` / `CH_STATE_MACHINE` / `CH_STATE` / `CH_STATE_MACHINE_BUILD` | 零消费者 |
| `transition_to()` | 零消费者，但**保留**（诚实 API + SpinalHDL `goto` parity + 被主 spec 第 ② 条引用 ⇒ 见 D2） |
| `state_def::parent` + `friend` | 只服务 `then()` |

⚠️ 另：`states_[static_cast<size_t>(s)]`（`state()` / `transition_when()`）**无边界检查** ⇒ 枚举值 ≥ N 是**越界 UB**。

### 1.4 规模事实

`ch_state_machine` 仓内实例化点 **5** 处 · `on_active` 使用文件 **3** 个 ⇒ ⇒ 当前**无受害站点**（两次事故已由前两个 change 修掉）⇒ ⇒ 处置 = **契约 + 文档 + 框架能力**，不是继续逐个修 example。

## 2. Decision

### D1. 状态动作的语义方向：**状态守卫的组合贡献**（不是展平 + 纪律）

`on_active` 闭包内 **MUST NOT** 写寄存器 `next`、MUST **NOT** 驱动 IO（现状契约，`chlib-state-machine-semantics` 第 ② 条**保持字面**）⇒ 框架 MUST 提供**显式贡献 API**（D4）把「每状态候选值」合成为**一条** select 链。

### D2. 死 / 误导 API **物理移除**（不「登记残留」）

依 Zero-Debt（「Delete stubs… **immediately. Never 'leave for later'**」）+ ADR-043 先例（零消费者 ⇒ 直接删、无 deprecation 别名期）。

移除集（**15 个符号**，另 `current_state()` 改名见 D3）：`on_exit`/`exit_action` · `on_entry`/`entry_action` + `build()` 执行块 · `then()`/`next_state_on_exit`/`has_exit_transition` + `build()` 分支 + `state_def::parent` + `friend` · `state_changed` · `tick_state()` · `is_built()` · `CH_STATE_MACHINE`/`CH_STATE`/`CH_STATE_MACHINE_BUILD`。

⚠️ **`transition_to()` 保留**（不删）：它是**诚实**的（= 无条件转移 = `transition_when(true, s)`）且是 SpinalHDL `goto` 的 parity 表面；更关键：capability `chlib-state-machine-semantics` 第 ② 条的 MUST 条款与 `## Purpose` **都引用它** ⇒ 删它会迫使第 ② 条 MODIFIED，且会让主 spec 的 Purpose 引用一个已删除的 API ⇒ **冻错主 spec**。

⚠️ **例外（保留 + 硬化）**：`set_entry()` 保留（SpinalHDL parity 的一部分），但 MUST：① `entry_state` 字段**在 ctor 初始化列表中初始化**（修 UB）② 入口枚举值 MUST 为 **0**，否则 `CHFATAL_EXCEPTION`（因 `state_reg` 构造期 init 恒 0）。
⚠️ **为什么不是「一并移除」**：删掉 `set_entry` 后，「入口 = 枚举值 0」会变成**隐式约束** ⇒ **同一个陷阱换个地方复发**（用户把入口枚举排到非 0 ⇒ 复位态静默错误）⇒ ⇒ 保留 + 硬化的价值正是把该隐式约束变成**显式且可检**的声明（代价：它退化为「只能传 0」的校验桩 —— 此代价**如实登记**于 §4）。

### D3. `current_state()` → `entry_state()`（改名，**不留** deprecated 别名）

理由：它返回**编译期** entry 而非运行时状态 ⇒ 名字误导；运行时访问器 `current_state_uint()` / `is_in()` 已存在且诚实。
⚠️ C++ 不允许数据成员与成员函数同名 ⇒ 字段 MUST 同时改名为 `entry_state_`。
⚠️ 仓内判例是 ADR-043 ⇒ 不设 deprecation 周期。

### D4. 贡献 API 的形态

```cpp
sm.contrib(target, S, value);        // 显式 state 参数；守卫 = 纯 is_in(S)
reg->next = sm.resolve_hold(reg);     // 仅 ch_reg 重载
out     = sm.resolve_or(out, default);// 寄存器与输出都开放
```

- **作用域**：MUST 限定为「**寄存器 + 输出**」⇒ `ch_in` MUST **编译期拒绝**（`port<T, input_direction>` 的重载 = `delete`）。
- **`contrib` MUST 在 `on_active` 闭包之外**：显式 state 形参取代闭包的隐式归属 ⇒ 保住 D1 与 capability 第 ② 条**字面**成立 ⇒ 静态护栏只需加**一条词法级**规则。
- **守卫**：MUST 恰好是 `is_in(S)`；更细的条件 MUST 写进 `value` 表达式内部。
- **value**：MUST 是**已建好的**表达式（eager 捕获，**禁** lambda 延迟求值）。
- **重复**：同一 `(target, state)` 重复贡献 MUST 在 **`contrib` 调用点**即 `CHFATAL_EXCEPTION`（早失败优于晚失败）。
- **默认尾部（两级，无二义）**：默认 MUST 是「**保持**」⇒ 寄存器用 `resolve_hold(reg)`；输出 **MUST NOT** 用 hold（输出引用自身 = **组合环** ⇒ 拓扑排序 CHFATAL）⇒ 输出 MUST 用 `resolve_or(out, default)` **显式**给默认。**若**某寄存器需要「未覆盖态取某常量」⇒ **MAY** 用 `resolve_or(reg, default)` 显式声明 ⇒ 故 `resolve_or` 对 `ch_reg` 与 `ch_out` **都开放**（两者语义不同：前者 = 显式默认常量，后者 = 组合环规避）。
- **多次 resolve** 同一 target MUST `CHFATAL_EXCEPTION`（fail-closed）。

### D5. 生命周期与混合写入的检出（**失败机制 MUST 可测**）

⚠️ `CHERROR` / `CHFATAL` / `CHREQUIRE` 实测**只打日志、不抛异常** ⇒ 用它们做安全网 = **假安全网** ⇒ ⇒ 所有新增检出 MUST 用 **`CHFATAL_EXCEPTION`**（`throw std::runtime_error`；先例 `src/core/context.cpp:308`）。

| 情形 | 检出 |
|---|---|
| 同一 (target, state) 重复贡献 | `contrib` 调用点 `CHFATAL_EXCEPTION` |
| `resolve` 之后再 `contrib` | `CHFATAL_EXCEPTION`（每 target 记 resolved 标志） |
| 同一 target 两次 `resolve` | `CHFATAL_EXCEPTION` |
| `contrib` 了但从未 `resolve` | **析构时**逐 target 检查 ⇒ `CHFATAL_EXCEPTION`（仅当 `std::uncaught_exceptions() == 0` 且非 static-destruction）**+** 静态护栏词法规则（文件级粗判） |
| 直接写与贡献写并存 | `resolve` 时检查 `regimpl::num_srcs() > 0` ⇒ `CHFATAL_EXCEPTION`；⚠️ **resolve 之后再直接写无法检出**（不动 `core/reg.h` 的前提下）⇒ **如实登记为已知盲区** |
| `contrib` 的 target 是 `state_reg` 自身 | `CHFATAL_EXCEPTION`（会覆盖 `build()` 写的转移树 ⇒ 转移全死） |
| 枚举值 ≥ N | `CHFATAL_EXCEPTION`（`state()` 的 **const 与非 const 两个重载** / `contrib()` / `transition_when()` 的**归属索引**与 **target 字面量**——后者经 `static_cast<uint8_t>` 会把 ≥ 256 的 target **静默截断** ⇒ 指向错误状态） |

### D6. 枚举与宽度防御

新代码 MUST **NOT** 复制既有的 `static_cast<uint8_t>` 截断模式（`state_machine.h:300/360/366`）⇒ 枚举越界 MUST 显式拒绝；`contrib` 的 value 宽度 MUST 与 target 相等（否则 `select` 的 `max(width)` 会静默截断 / 扩展）。

### D7. 手写 select 链**保留**为合法降级路径

MUST NOT 禁止手写链 ⇒ 但**首选**是 `contrib`/`resolve` ⇒ 框架头注释的「5 条纪律」改写为「贡献 API 优先 / 手写链合法但需自证正确」。

### D8. 能力变更的边界（**不改核心**）

- `ch_reg` 的**单 next 源**契约**不变** ⇒ `chlib-state-machine-semantics` 第 ① 条**零 MODIFIED**；
- `ch_out` 契约**不变**（`ch_logic_out::operator=` = `set_src(0,…)`，框架只写一次）；
- ⇒ ⇒ **框架 `core/` 层零改动** ⇒ 爆炸半径限于 `include/chlib/state_machine.h` + `tests/test_state_machine.cpp` + 2 个 fixture 的迁移。

### D9. 契约同步（**MUST 走 MODIFIED delta，禁 `--skip-specs`**）

`chlib-state-machine-semantics` **MODIFIED ×1**：第 ⑥ 条的 Scenario「MUST 是 `build()` 之后的**单一 `select` 表达式**」**放宽**为「手写 `select` **或** `resolve_*` 生成」，**并在该条内以 NOTE 冻结 D5 的已知盲区**（「`resolve` 之后再直接写**无法检出**」）⇒ ⇒ 否则该盲区只留在 ADR 里 ⇒ 下一任读者只能从 ADR 考古 ⇒ in-force 契约将**不含**这条已知的静默通道。
⚠️ 第 ② 条**零改动**（`transition_to` 保留 ⇒ ② 的 MUST 与 Purpose 引用仍成立）。
⚠️ 「零 MODIFIED」的早期说法**不成立**：⑥ 的字面表述会把新的贡献路径判为违规 ⇒ 主 spec 会自相矛盾。

### D10. 实施顺序与分工

一个 change、**两个 commit**：commit 1 = D2 + D3（移除 + 硬化，先清场）；commit 2 = D4–D9（贡献 API + 迁移 `uart_tx` 与 `complex_e2e` 做 dogfood）。
⚠️ commit 2 MUST **物理删除**被迁移 fixture 的旧手写链（保留两份 ⇒ 双写 ⇒ last-wins ⇒ 金标仍全绿 ⇒ contrib 路径可能是**死的**而无人知晓 ⇒ 与本决策要消灭的「假绿」同构）。

## 3. 备选方案（否决）

| 选项 | 结论 | 理由 |
|---|---|---|
| **S0** 保持展平 + 纪律（现状） | **否决** | 契约 + 护栏是**必要非充分**：它防「闭包内写」，不防「闭包外手写链写错」；两次事故证明手写嵌套 `select` 链是**人的失败点**；DSL 名字承诺了它不兑现的语义 ⇒ 第三次同型事故只是时间问题 |
| **S2** 状态局部寄存器（每状态一份 + 出口 mux） | **否决** | 每状态一份**物理寄存器** ≠ 一份寄存器分时复用 ⇒ **错误的硬件语义**（面积代价 + 语义改变）；改动面最大 |
| **S3** 透明拦截（让 `reg->next =` / `io().x =` 在闭包内自动守卫） | **否决** | 需 `next_assignment_proxy`（`core/reg.h`）感知「当前守卫上下文」⇒ core ↔ chlib **双向耦合 + 全局状态**；且会让**既有合法代码**（build 后的直接写）语义**静默改变** |
| **S4** 只改名（不改语义） | **否决** | 只消除幻觉、不消除失败形态；D2 删掉误导 API 后 `on_active` 剩余语义已被契约 + 护栏兜住 ⇒ 改名边际收益 ≈ 0 |
| 基于 `chlib/switch.h` 的 `case_` 组合子实现贡献合成 | **否决** | `switch.h` 在头文件**全局作用域**写了 `using namespace ch::core`（命名空间污染 ⇒ ADR-033 治理的类别）且其 5 组测试**整段注释**（`tests/chlib/test_arithmetic.cpp:443-700`）⇒ **不健康基座**；为一个 5 处使用的 DSL 改进去复活一个子系统 ⇒ ROI 不成立 |
| 贡献的 value 用延迟 lambda | **否决** | C++ 名字绑定已天然禁止前向引用 ⇒ 延迟求值**买不到表达力**，只买来类型擦除 + 捕获时机歧义 |
| `current_state()` 留 deprecated 别名 | **否决** | 仓内判例是 ADR-043（零/少消费者 ⇒ 直接移除，无别名期）；本项目无 formal release 节奏 ⇒ 无「外部使用者需要 deprecation 周期」的事实基础；保留别名 = 让误导性名字继续活着 |
| `set_entry()` + `current_state()` / `entry_state` 字段**一并移除** | **否决** | 「入口 = 枚举值 0」会退化为**隐式约束** ⇒ 同一「复位态静默错误」的陷阱换个地方复发 ⇒ 保留 + 硬化（`CHFATAL_EXCEPTION`，与 D5 的失败机制一致）把隐式约束变成**显式可检**；代价（退化为「只能传 0」的校验桩）已登记于 §4 |
| 框架代写 `reg->next`（原 S1a 形态） | **否决** | `state_reg` 是框架自有的（`build()` 写它合法），但用户的寄存器不是 ⇒ 所有权二义 ⇒ 冲突检测被迫改 `core/reg.h` ⇒ 那就滑向 S3 |

## 4. Consequences

### 正面

- DSL 的名字**兑现**它承诺的语义（SpinalHDL parity 恢复）⇒ 「第三次同型事故」的结构性成因被消除；
- 「每状态候选值」从**手工纪律**变成**库 API** ⇒ 消除「多写覆盖」与「手写链写错」两类失败；
- 死 / 误导 API 一次性清场 ⇒ 零消费者的 public surface 收缩；
- `ch_reg` / `ch_out` / `core/` **零改动** ⇒ capability 第 ① 条零 MODIFIED；
- capability 只 **MODIFIED ×1**（⑥）⇒ 主 spec 与实现仍**单一真相**。

### 负面 / 代价

- **公开 API 缩减**（**15 个符号**移除 + 1 项改名）⇒ 破坏性变更（仓内消费者 5 处，同 commit 改完）；
- **新增 API 面**（`contrib` / `resolve_hold` / `resolve_or`）⇒ 从此有两种写法 ⇒ 文档 / 评审 / 教学成本上升；
- `ch_state_machine` 的公开面**变窄但变真**：SpinalHDL 的 `onEntry`/`onExit` **不再提供** ⇒ **parity 让步**（理由：错误实现比缺失更糟；真运行时 entry/exit 需 **prev_state 寄存器** ⇒ 独立立项）；
- 混合写入的**可检出性不对称**（resolve 之后再直接写不可检出）⇒ 已知盲区，**如实登记**。

### 风险

- contrib 的 value 是 **eager 捕获** ⇒ 之后**重绑定**该 C++ 对象（ADR-042 意义上的 `operator=`）⇒ contrib 持旧节点 ⇒ 静默过期 ⇒ MUST 写进头注释（与全框架的 eager 语义一致）；
- 贡献表若用**节点名字**做 key ⇒ 全仓输入节点名恒为 `unnamed_input`（实测）⇒ MUST 用 `impl()` 指针；
- D5 新增的**三条生命周期检出**本身是新失败模式的来源 ⇒ MUST 有各自的变异验证；
- ⚠️ **析构检出的静默降级窗口**：`CHFATAL_EXCEPTION` 宏自带 `in_static_destruction()` 守卫 ⇒ 在**静态析构期**该宏**整块跳过**（**无日志、无抛出**）⇒ 该窗口内的检出**完全静默**。⚠️ 但**现状下该窗口从不打开**：`set_static_destruction()` 的 **3** 处调用点（`src/simulator.cpp:111`、`samples/destruction.cpp:52`、`samples/counter.cpp:84`）**全部被注释** ⇒ 该标志**从不置位**（ADR-017 §2.2 已登记此欠账）⇒ ⇒ 结论（头注释注明 + D5 自带双门）方向不变，但机制描述 MUST 如实写成「守卫**存在**但当前**从未生效**；若 ADR-017 的迁移落地 ⇒ 该窗口才会真实打开」。

### 不在本次范围

- 真运行时 entry / exit 动作（需 **prev_state 寄存器** ⇒ 面积 + 新语义）⇒ 独立立项；
- `ch_reg` 的「多源 next」作为**公开能力** ⇒ **本 ADR 关闭**该悬挂议题（= 保留单源 + 库层 sugar）；
- `if (硬件值)` 的**运行期**支持（= 更大的框架能力缺口）；
- `chlib/switch.h` 的复活 ⇒ 独立立项。

## 5. Verification（前瞻验收判据 —— **可勾选**；实施后逐条回填）

### 5.1 静态护栏

- [ ] `scripts/verify_fsm_next_wiring.sh` 主路径在仓内**绿**（471 个源文件 / 0 违规）；
- [ ] `--self-test` fixture 全绿（新增 `contrib-in-closure` 词法规则 ⇒ 计数相应更新）；
- [ ] 新增词法规则 MUST 保持「宁可漏报、不误报」方向（**不得**扩展成 contrib 语义分析）。

### 5.2 编译期拒绝（`= delete` / 无该签名 ⇒ 编译不过）

- [ ] `ch_in` 端口 `contrib`；输出端口 `resolve_hold`；`resolve_or` 无 default 形参。

### 5.3 运行期拒绝（`CHFATAL_EXCEPTION` ⇒ Catch2 `REQUIRE_THROWS_AS(..., std::runtime_error)`）

- [ ] 同一 (target, state) 重复贡献 · `resolve` 后再 `contrib` · 同一 target 两次 `resolve`；
- [ ] `contrib` 了但从未 `resolve`（析构时抛）；
- [ ] 直接写与贡献写并存（resolve 前）；
- [ ] `contrib` 的 target 是 `state_reg` 自身；枚举值 ≥ N；`set_entry(非 0 枚举)`。

### 5.4 变异验证（每条**先确认编译成功** ⇒ 否则跑的是旧二进制）

- [ ] M1 删任一 `contrib` ⇒ 指定断言**转红**（⇒ 证明 contrib 路径**活着**，不是旧手写链在兜底）；
- [ ] M2 交换两状态的贡献值；M3 `resolve_hold` 丢失 hold 尾；M4 `resolve_or` 忽略 default；M5 守卫误用 `!is_in(S)`。

### 5.5 行为回归（dogfood）

- [ ] `uart_tx` 既有金标**逐项不变**（`busy` 高 **3906** 拍 + `0xA5` 的 8 位 LSB-first 序列）；
- [ ] `test_verilator_complex_e2e` 既有金标**逐项不变**（`0x96` MSB-first 16 位 + 状态序列 + `bit_count` 复位）；
- [ ] 双后端 parity 保留；生成的 Verilog 中每个输出**仍各 1 条** assign；
- [ ] 被迁移 fixture 的旧手写链**已物理删除**（grep 证据）；
- [ ] `tests/test_state_machine.cpp` MUST 迁移（它是 `current_state()` / `on_entry` 的**唯一**消费者）⇒ 且 `fx` 护栏 fixture 只用被保留的 `set_entry` ⇒ 护栏**不受影响**（`--self-test` 仍 12/12）。

### 5.6 计数锚点

- [ ] `CTEST_TOTAL` **202** / `EXAMPLES_TEST_SOURCES` **17** / `verilator_e2e` **38** / non-perf **191**（新用例进**既有** target ⇒ 不变）。

## 6. 实施追踪

- **change**：**待立项**（本 ADR 落地时创建）⇒ 其 `design.md` 承载逐拍推导 / 变异清单 / 实施步骤 / 探针证据；`tasks.md` 承载可勾选任务（**过程日志不进本 ADR**）。
- **改动面（预期）**：`include/chlib/state_machine.h`（+ `tests/test_state_machine.cpp`、2 个 fixture 的迁移、`include/chlib/AGENTS.md`、capability delta）。
- **爆炸半径**：**零**（`core/` 与公开契约零变化 ⇒ 不影响任何库 / 产品代码）。

## 7. 参考与审查记录

- **依据**：`docs/architecture/2026-03-30-state-machine-dsl-design.md`（设计意图：状态守卫）· `include/chlib/state_machine.h`（实现：展平）· `include/ast/ast_nodes.h:47-57`（`set_next` / `get_next`）· `include/core/io.h:113-123`（`ch_logic_out::operator=`）· `include/utils/logger.h`（`CHERROR`/`CHFATAL` 只打日志、`CHFATAL_EXCEPTION` 才抛）· `openspec/specs/chlib-state-machine-semantics/spec.md`（已落地的 6 条契约）。
- **独立复核（oracle，2026-10-29，两轮）**：修正 ①「框架代写 `reg->next`」的所有权越界（⇒ 改为用户写一次）②「输出用 hold 默认」会组合环 CHFATAL ③「登记残留」违 Zero-Debt（⇒ 物理移除）④ 移除清单漏 4 项 ⑤ `set_entry` 的静默错误复位态。
- **缺口分析（metis，2026-10-29）**：修正 ①「零 MODIFIED」**不成立**（第 ⑥ 条字面冲突）② `CHERROR` 只打日志 ⇒ 安全网是假的 ③「contrib 未 resolve」原本**无检出机制** ④ contrib 必须**闭包外** ⑤ `entry_state` **未初始化** ⇒ UB ⑥ `states_[…]` 无边界检查 ⇒ 越界 UB ⑦ dogfood 须有**contrib 活性变异**。
- **对抗审稿（critic，`review_profile: arch`，2026-10-29）**：第 1 轮 **REJECT**（1 BLOCKER + 4 SHOULD_FIX + 3 NIT）⇒ BLOCKER = 「D2 移除 `transition_to` ⇒ 主 spec 第 ② 条的 MUST 与 Purpose 引用一个已删除的 API ⇒ **冻错主 spec**」⇒ 修法 = **保留 `transition_to`**（而非 MODIFIED ×2）⇒ D2/D4/D9 三者同时为真；第 2 轮 **APPROVE_WITH_CONDITIONS**（**0 BLOCKER**）⇒ 2 SHOULD_FIX + 3 NIT 同批闭合（备选表宏名对齐 / 析构降级窗口的机制描述如实化 / 盘点表标注保留 / 越界面写全 / 去重）。
- **对账**：`chlib-fsm-semantics-2026-10`（本 ADR 的前置：契约 + 护栏已落地）· `uart-tx-fsm-live-logic-2026-10` / `spi-fsm-fixture-live-logic-2026-10`（两次事故的修复）。
