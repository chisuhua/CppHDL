# Design: chlib-fsm-action-semantics-2026-10

> **决策不在本文** —— 决策在 `docs/adr/ADR-050-chlib-state-machine-semantics.md`。
> 本文承载 ADR 外移的**推导 / 实施设计 / 逐项清单 / 核对清单**。

## 0. 一句话结论

实施 ADR-050：commit 1 = **移除 + 硬化**（15 个符号 + 1 改名 + 3 项硬化）先清场；
commit 2 = **状态守卫贡献 API**（`contrib` / `resolve_hold` / `resolve_or`）+
迁移两个真实 fixture 做 dogfood ⇒ **`core/` 与 `ch_reg`/`ch_out` 的公开契约零变化**。

## 1. 实施面拆解

| commit | 内容 | 依赖 | 回退点 |
|---|---|---|---|
| **1** | 移除 + 改名 + 硬化 + 迁移 `tests/test_state_machine.cpp` | 无 | 单 commit 可回退 |
| **2** | 贡献 API + 生命周期检出 + 枚举/宽度防御 + dogfood 迁移 + capability delta | commit 1 已清场（⇒ 契约文本不必为死 API 措辞）| 单 commit 可回退 |

⚠️ **不得合成为一个 commit**：commit 2 的契约文本建立在「死 API 已不存在」之上
⇒ 合并 ⇒ 出现「实现已改、契约仍描述旧 API」的中间态。

## 2. 贡献 API 的实现设计

### 2.1 框架侧数据结构（`ch_state_machine` 内新增）

```cpp
struct contribution {
    lnodeimpl *target;   // 目标节点（reg 的 proxy / output 的 impl）
    StateEnum  state;    // 归属状态（守卫 = is_in(state)）
    lnodeimpl *value;    // 贡献值（eager 捕获的节点）
};
std::vector<contribution> contributions_;
std::vector<lnodeimpl *> resolved_targets_;   // 已 resolve 的 target（去重）
```

⚠️ **target 的身份 MUST 用 `impl()` 指针**（节点身份）⇒ **MUST NOT** 用节点
**名字**（实测：全仓输入节点名恒为 `unnamed_input`）⇒ **MUST NOT** 用 wrapper
对象地址（拷贝即失效）。

### 2.2 `contrib(target, S, value)`

1. 枚举越界检查：`S` MUST `< N` ⇒ 否则 `CHFATAL_EXCEPTION`；
2. 重复检查：同一 `(target, S)` 已存在 ⇒ `CHFATAL_EXCEPTION`（**在 contrib
   调用点**判 ⇒ 早失败优于晚失败）；
3. 自贡献检查：`target == state_reg` 的 impl ⇒ `CHFATAL_EXCEPTION`
   （否则会覆盖 `build()` 写的转移树 ⇒ 转移全死）；
4. 已有 next 源检查：`target` 的 `regimpl::get_next() != nullptr` ⇒
   `CHFATAL_EXCEPTION`（「先直接写后贡献」可检 ⇒ 捕获「直接写在前」这一半）；
5. 记录 contribution。

⚠️ **value MUST 是已建好的表达式**（eager 捕获，**禁** lambda）⇒ 契约写明
「contrib 在**调用点**捕获节点」⇒ 之后**重绑定**该 C++ 对象（ADR-042 意义上的
`operator=`）⇒ contrib 持旧节点 ⇒ **静默过期** ⇒ MUST 写进头注释。

### 2.3 `resolve_hold(reg)` / `resolve_or(target, default)`

- `resolve_hold`：**仅 `ch_reg<T>` 重载** ⇒ 输出端口调它 ⇒ **编译不过**
  （比 `CHFATAL_EXCEPTION` 更早 ⇒ 零运行时成本）；
- `resolve_or`：**`ch_reg<T>` 与 `port<T, output_direction>` 都有重载** ⇒
  `port<T, input_direction>` 的 `contrib` 重载 = `= delete` ⇒ `ch_in`
  **编译期拒绝**；
- 合成算法：对该 target 的全部 contribution（按 state index 升序）⇒

  ```
  acc = <起点>            // resolve_hold ⇒ acc = target 本身（保持）
                         // resolve_or   ⇒ acc = default
  for (S, v) in contributions_of_target:
      acc = select(is_in(S), v, acc)
  return acc
  ```

  ⚠️ **binary 编码 ⇒ 各 `is_in(S)` 互斥 ⇒ 链序语义无关**（只影响 Verilog
  可读性）⇒ **MUST NOT** 引入「后注册者优先」这类优先级语义（无遗留行为需对齐）；
- **默认尾部两级**（ADR-050 D4）：默认 = **保持** ⇒ `resolve_hold`；
  「未覆盖态取某常量」⇒ **MAY** 显式 `resolve_or(reg, default)`；
  输出 **MUST NOT** 用 hold（输出引用自身 = **组合环** ⇒ 拓扑排序 CHFATAL）；
- 宽度：合成链的宽度由 `select` 的 `max(width)` 决定 ⇒ **MUST** 在返回前显式
  截断到 target 的声明宽度（否则 Verilog 侧隐式截断、interpreter 侧
  `assign_truncate` ⇒ 两侧虽等价但**不可见**）；
- `resolve` 时：若该 target 已有 next 源（`num_srcs() > 0`）⇒
  `CHFATAL_EXCEPTION`（「直接写 + 贡献写并存」的**可检出半边**）；
  若该 target 已 resolve 过 ⇒ `CHFATAL_EXCEPTION`（两次 resolve ⇒ 两棵树 ⇒
  last-wins 复发）。

### 2.4 析构检查（未 resolve 的检出）

`~ch_state_machine()`：若存在「有 contribution 但未 resolve」的 target ⇒
`CHFATAL_EXCEPTION`，**且仅当** `std::uncaught_exceptions() == 0` 且
`!ch::detail::in_static_destruction()`（析构期抛异常 ⇒ 展开中 ⇒ terminate）。

⚠️ **如实登记**：`CHFATAL_EXCEPTION` 宏自带 `in_static_destruction()` 守卫 ⇒
静态析构期**整块跳过**（无日志、无抛出）⇒ 该窗口内的检出**完全静默**。
⚠️ 但**现状下该窗口从不打开**：`set_static_destruction()` 的 **3** 处调用点
（`src/simulator.cpp:111`、`samples/destruction.cpp:52`、
`samples/counter.cpp:84`）**全部被注释** ⇒ 该标志**从不置位**
（ADR-017 §2.2 已登记此欠账）⇒ ⇒ 结论方向不变（头注释注明 + 析构自带双门），
但机制描述 MUST 如实写成「守卫**存在**但当前**从未生效**」。

⚠️ **析构期 `CHFATAL_EXCEPTION` 不可测** ⇒ 该检出 MUST 另有**可测**路径
（静态护栏的词法规则：文件内出现 `contrib(` 但无对应 `resolve_*` ⇒ 违规）。

### 2.5 编译期拒绝：**无可用重载**（**不是** `= delete`）

`port<T, input_direction>` 的方向是**类型参数** ⇒ 重载即可，**零 core 改动**。

⚠️ **实施期实测的关键发现**：被**删除**的调用在 SFINAE 语境下是**硬错误**
（实测：`std::void_t` 检测不到 ⇒ 整个 TU 编译失败）⇒ ⇒ 用 `= delete` 的写法
**无法**写成**自动化**的编译期断言 ⇒ ⇒ 改用「**无可用重载**」形态：

```cpp
// ✅ 提供：寄存器 / 输出端口
template <typename T, typename V> void contrib(const ch_reg<T>&, StateEnum, const V&);
template <typename T, typename V> void contrib(const port<T, output_direction>&, StateEnum, const V&);
// ⛔ **故意不提供**：`port<T, input_direction>` ⇒ 调用「无匹配重载」⇒ 编译不过
//    （同理：`resolve_hold` 也不提供 `port<T, output_direction>` 重载）
```

⇒ ⇒ 这样「编译不过」才**可被自动化断言**（`std::void_t` 检测）⇒
`tests/test_state_machine.cpp` 的 `STATIC_REQUIRE(!can_contrib<Sm, InPort>::value)`
等 **6** 条**编译期**断言成立 ⇒ ADR-050 §5.2 的「`= delete` / **无该签名** ⇒
编译不过」取**后者**。

⚠️ **DX 代价（如实登记）**：错误信息是「no matching function」而非
「use of deleted function」⇒ 头注释里写明了原因。

取 `regimpl*` 走 `ch_reg` 的 `operator lnode<T>()` ⇒ `.impl()`（公开 API）；
「目标已有 next 源」的检查走 **代理节点的 `src(0)`**（= 寄存器本体）⇒
**零 `core/` 改动**。

## 3. 移除与硬化的逐项清单

| 类别 | 对象 | 理由 | 受影响 |
|---|---|---|---|
| 移除 | `on_exit` / `exit_action` | **从不执行** | 零消费者 |
| 移除 | `on_entry` / `entry_action` + `build()` 执行块 | 只对 entry 态在 build 期执行一次（**不是**运行时 entry action）| `tests/test_state_machine.cpp:64-86` |
| 移除 | `then()` / `next_state_on_exit` / `has_exit_transition` + `build()` 分支 | legacy 自动退出转移 | 零消费者 |
| 移除 | `state_def::parent` + `friend` | 只服务 `then()` | — |
| 移除 | `state_changed` | 声明后**从不赋值** | — |
| 移除 | `tick_state()` | 声称执行「当前态的 `on_active`」，但 `current_state()` 返回**编译期** entry ⇒ 永远跑 entry 态闭包；二次执行**重复发射节点** | 零消费者 |
| 移除 | `is_built()` | 零消费者 | 零消费者 |
| 移除 | `CH_STATE_MACHINE` / `CH_STATE` / `CH_STATE_MACHINE_BUILD` | 零消费者 | 零消费者 |
| **改名** | `current_state()` → `entry_state()`；字段 `entry_state` → `entry_state_` | 返回**编译期** entry ⇒ 名字误导；C++ 不允许成员与字段同名 | `tests/test_state_machine.cpp:46` / `:59` |
| **保留** | `transition_to()` | 诚实 API + SpinalHDL `goto` parity + **主 spec 第 ② 条的 MUST 与 `## Purpose` 都引用它** ⇒ 删它会**冻错主 spec** | — |
| **保留+硬化** | `set_entry()` | 删它会让「入口 = 枚举值 0」退化为**隐式约束** ⇒ 同一「复位态静默错误」陷阱换个地方复发 ⇒ 保留 + 硬化把它变**显式可检** | — |
| **硬化** | `entry_state` 字段初始化 | 现**未**在 ctor 初始化列表 ⇒ 读它是 **UB** | — |
| **硬化** | `set_entry(非 0 枚举)` | `state_reg` 构造期 init 恒 `0_d` ⇒ 非 0 入口 = **静默错误复位态** | — |
| **硬化** | `state()`（**const 与非 const 两个**重载）/ `transition_when()`（**归属索引** + **target 字面量**）/ `contrib()` 的枚举越界 | `states_[…]` **无边界检查** ⇒ 越界 UB；`static_cast<uint8_t>` 会**静默截断** ≥256 的 target ⇒ 指向错误状态 | — |

⚠️ **`transition_when` 的 target 字面量截断**：`ch_uint<STATE_BITS>(static_cast<uint8_t>(tr.target))`
⇒ `target ≥ 256` 被截断 ⇒ 转移指向**错误状态**且**静默** ⇒ MUST 显式拒绝。

## 4. dogfood 迁移的等价变换

### 4.1 `uart_tx.cpp`（3 个寄存器 + 2 个输出）

以 `baud_counter` 为例（**旧**）：

```cpp
baud_counter->next = select(
    sm.is_in(UartTxState::START) | sm.is_in(UartTxState::DATA) |
        sm.is_in(UartTxState::STOP),
    select(baud_done, ch_uint<16>(0_d), baud_counter + ch_uint<16>(1_d)),
    baud_counter);
```

（**新**）—— 每个贡献状态一条，末尾**一次** resolve：

```cpp
sm.contrib(baud_counter, UartTxState::START,
           select(baud_done, ch_uint<16>(0_d), baud_counter + ch_uint<16>(1_d)));
sm.contrib(baud_counter, UartTxState::DATA,  /* 同上 */);
sm.contrib(baud_counter, UartTxState::STOP,  /* 同上 */);
baud_counter->next = sm.resolve_hold(baud_counter);
```

⚠️ **等价性**：binary 编码 ⇒ 同时只有一个态激活 ⇒ 旧链的
`is_in(A)|is_in(B)|is_in(C)` 与新链的「逐态 mux」**逐拍等价** ⇒ 既有金标
（`busy` 高 **3906** 拍 + `0xA5` 的 8 位 LSB-first 序列）**MUST 逐项不变**。

⚠️ **输出**同理：`io().busy` 的多路选择 → 每态一条 `contrib` +
`io().busy = sm.resolve_or(io().busy, ch_bool(false));`（输出**必须**显式
default ⇒ 组合环）。

### 4.2 `test_verilator_complex_e2e.cpp`（2 个寄存器）

`bit_count` / `shift` 的手写链 → 逐态 `contrib` + `resolve_hold`。
⚠️ 该 fixture 的 `bit_count` 语义 = 「IDLE 且 start ⇒ 清零；TRANSFER 且非最后
一位 ⇒ +1；最后一位 ⇒ 保持 7」⇒ 「保持 7」在贡献模型里 = **不贡献**（其余态
hold）⇒ ⇒ 「非最后一位 +1」这条**必须**在 value 表达式内部带条件
（`select(is_in(TRANSFER) & !last_bit, bit_count + 1, bit_count)`）⇒ ⇒
「更细的条件写进 value 内部」这条约束的**首个真实用例**。

⚠️ **旧手写链 MUST 物理删除**（不是并存）⇒ 理由见 `proposal.md`。

## 5. 实施期 MUST 核对的清单（AI slop 高发点）

| # | 陷阱 | 后果 |
|---|---|---|
| 1 | 用 **`CHERROR` / `CHFATAL` / `CHREQUIRE`** 实现检出 | 只打日志 ⇒ **假安全网**（实测三者的行为一致）⇒ MUST 用 **`CHFATAL_EXCEPTION`** |
| 2 | 把 select 链实现成**每状态一个 mux 组件** | 重新引入多驱动 ⇒ MUST 是**一条**表达式链（与 `build()` 合成转移树同构）|
| 3 | 贡献表 key 用**节点名字** | 全仓输入节点名恒为 `unnamed_input`（实测）⇒ MUST 用 `impl()` 指针 |
| 4 | 贡献表 key 用 **wrapper 对象地址** | 拷贝即失效 ⇒ MUST 用 `impl()` 指针 |
| 5 | value 用 `std::function` 延迟求值 | 零收益 + 生命周期陷阱 + 重新引入「闭包在 build 期被执行」的心智混淆 |
| 6 | 迁移时**新旧并存** | 双写 ⇒ last-wins ⇒ 金标仍全绿 ⇒ contrib 路径可能是**死的** ⇒ MUST 物理删除旧链 + **contrib 活性变异** |
| 7 | 「顺手」让 `build()` 自动 resolve 并写 `reg->next` | 推翻 ADR-050 D4 的核心裁决（写入留在用户手里）|
| 8 | 复制 `static_cast<uint8_t>` 截断模式 | 复制既有潜在缺陷（>256 的 target 静默截断）|
| 9 | 引入「后注册者优先」优先级语义 | ADR-050 D4 已否决（无遗留行为需对齐；守卫互斥 ⇒ 链序无关）|
| 10 | 扩展护栏为 contrib **语义**分析 | 违反 `fsm_wiring.py` 自陈的「不可能被完全闭合」⇒ 新规则 MUST 限制在**词法级** |
| 11 | 移除 `transition_to` | 主 spec 第 ② 条的 MUST 与 `## Purpose` 会引用已删 API ⇒ **冻错主 spec** ⇒ MUST 保留 |

## 6. 范围外

- `core/` 的任何语义改动（`ch_reg` 多源 next **明确否决** ⇒ ADR-050 已关闭该议题）
- 真运行时 entry / exit 动作（需 prev_state 寄存器 ⇒ 独立立项）
- `chlib/switch.h` 的复活
- `if (硬件值)` 的运行期支持
- 禁止手写 `select` 链（**保留**为合法降级路径）
