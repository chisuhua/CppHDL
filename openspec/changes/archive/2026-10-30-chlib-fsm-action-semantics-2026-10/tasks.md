# Tasks: chlib-fsm-action-semantics-2026-10

> 决策在 `docs/adr/ADR-050-chlib-state-machine-semantics.md`；实施细节在 `design.md`。
> ⚠️ **两个 commit**（ADR-050 D10）：commit 1 = 移除+硬化；commit 2 = 贡献 API+dogfood。

## 0. 准备（**只读**核实 —— 不猜）
- [x] 0.1 核实待移除符号的**全仓消费者**（`grep`）⇒ 记录每个符号的消费者清单
      ⇒ ⚠️ 已知：`current_state()` / `on_entry` 的**唯一**消费者是
      `tests/test_state_machine.cpp`；其余（`on_exit` / `then()` / `state_changed`
      / `tick_state()` / `is_built()` / 三宏）**零消费者**
- [x] 0.2 核实 `set_entry` / `current_state` / `on_entry` 的**实测**行为
      （`set_entry(非 0)` ⇒ 复位态仍为 0；`current_state()` ⇒ 返回**编译期** entry）
- [x] 0.3 核实 `in_static_destruction()` 的**当前**状态
      ⇒ `set_static_destruction()` 的 3 处调用点**全部被注释** ⇒ 守卫**从不生效**
- [x] 0.4 核实失败机制：`CHERROR` / `CHFATAL` / `CHREQUIRE` **只打日志**；
      **只有** `CHFATAL_EXCEPTION` 抛 `std::runtime_error` ⇒ 先例
      `src/core/context.cpp:308`、测试先例 `tests/chlib/test_forwarding.cpp:300`

## 1. commit 1 —— 移除 + 硬化（先清场）
- [x] 1.1 移除 `on_exit` / `exit_action`
- [x] 1.2 移除 `on_entry` / `entry_action` + `build()` 里的执行块
- [x] 1.3 移除 `then()` / `next_state_on_exit` / `has_exit_transition` +
      `build()` 里的对应分支 + `state_def::parent` + `friend`
- [x] 1.4 移除 `state_changed`
- [x] 1.5 移除 `tick_state()`
- [x] 1.6 移除 `is_built()`
- [x] 1.7 移除 `CH_STATE_MACHINE` / `CH_STATE` / `CH_STATE_MACHINE_BUILD`
- [x] 1.8 `current_state()` → `entry_state()`；字段 `entry_state` → `entry_state_`
      （⚠️ C++ 不允许成员与字段同名）
- [x] 1.9 **保留** `transition_to()`（⚠️ 主 spec 第 ② 条的 MUST 与 `## Purpose`
      **都引用它** ⇒ 删它会**冻错主 spec**）
- [x] 1.10 硬化：`entry_state` 字段**在 ctor 初始化列表中初始化**（修 UB）
- [x] 1.11 硬化：`set_entry(非 0 枚举)` ⇒ `CHFATAL_EXCEPTION`
- [x] 1.12 硬化：`state()`（**const 与非 const 两个**重载）/ `transition_when()`
      （**归属索引** + **target 字面量**）/ `contrib()` 的枚举越界 ⇒
      `CHFATAL_EXCEPTION`（⚠️ `states_[…]` **无边界检查** ⇒ 越界 UB；
      `static_cast<uint8_t>` 会**静默截断** ≥256 的 target）
- [x] 1.13 迁移 `tests/test_state_machine.cpp`（`current_state()` / `on_entry` 的
      **唯一**消费者）⇒ 含把「`on_entry` 在 build 期被调用」那条用例改为
      **反映新事实**（该 API 已不存在 ⇒ 用例 MUST 改为断言**运行时** `is_in()`）
- [x] 1.14 编译 0 error / 0 **新增** warning；`ctest -R state_machine` 绿

## 2. commit 2 —— 状态守卫贡献 API
- [x] 2.1 `ch_state_machine` 新增 `contribution` 结构 + `contributions_` +
      `resolved_targets_`（⚠️ target 身份 MUST 用 `impl()` 指针，**MUST NOT** 用
      节点名字 / wrapper 地址）
- [x] 2.2 `contrib(target, S, value)`：枚举越界 / 重复 `(target, S)` /
      `state_reg` 自贡献 / target 已有 next 源 ⇒ 四项**均在 contrib 调用点**
      `CHFATAL_EXCEPTION`
- [x] 2.3 `resolve_hold(reg)`：**仅 `ch_reg` 重载**（输出端口 ⇒ **编译不过**）
- [x] 2.4 `resolve_or(target, default)`：`ch_reg` 与 `port<T, output_direction>`
      **都开放**；`port<T, input_direction>` 的 `contrib` 重载 = **`= delete`**
      ⇒ `ch_in` **编译期拒绝**
- [x] 2.5 合成算法：逐态 `select(is_in(S), v, acc)`，**一条**链（⚠️ **MUST NOT**
      每状态一个 mux；⚠️ **MUST NOT** 引入优先级语义）
- [x] 2.6 宽度：返回前显式截断到 target 声明宽度
- [x] 2.7 生命周期检出：`resolve` 后再 `contrib` / 同一 target 两次 `resolve` /
      直接写与贡献写并存（`num_srcs() > 0`）⇒ `CHFATAL_EXCEPTION`
- [x] 2.8 析构检查：有贡献但未 resolve ⇒ `CHFATAL_EXCEPTION`
      （⚠️ 仅当 `std::uncaught_exceptions() == 0` 且非 static-destruction；
      ⚠️ 该窗口**当前从不打开** ⇒ MUST 另有**可测**路径）
- [x] 2.9 护栏 `scripts/verify_fsm_next_wiring.sh` 加**词法级**规则
      （`contrib-in-closure`）+ `--self-test` fixture ⇒ 计数相应更新
      （⚠️ **MUST NOT** 扩展成 contrib 语义分析）
- [x] 2.10 头注释：把「5 条纪律」改写为「贡献 API **首选** / 手写 `select` 链
      **合法但需自证正确**」+ 写明 eager 捕获的**静默过期**风险 +
      `ch_in` **不在**作用域内

## 3. dogfood 迁移（**旧链 MUST 物理删除**）
- [x] 3.1 `examples/spinalhdl-ported/uart/uart_tx.cpp`：3 个寄存器 + 2 个输出
      ⇒ 逐态 `contrib` + 一次 `resolve`；**删除**旧手写链（grep 证据）
- [x] 3.2 `tests/test_verilator_complex_e2e.cpp`：`bit_count` / `shift`
      ⇒ 逐态 `contrib` + `resolve_hold`；**删除**旧手写链
- [x] 3.3 ⚠️ `bit_count` 的「非最后一位 +1」条件 MUST 写进 **value 表达式内部**
      （`select(is_in(TRANSFER) & !last_bit, bit_count + 1, bit_count)`）
- [x] 3.4 `uart_tx` 既有金标**逐项不变**（`busy` 高 **3906** 拍 + `0xA5` 的
      8 位 LSB-first 序列）
- [x] 3.5 `complex_e2e` 既有金标**逐项不变**（`0x96` MSB-first 16 位 +
      状态序列 + `bit_count` 在第 2 次装载时复位）
- [x] 3.6 双后端 parity 保留；生成 Verilog 中每个输出**仍各 1 条** assign

## 4. 变异验证（每条**先确认编译成功** ⇒ 否则跑的是旧二进制 ⇒ 假绿）
- [x] 4.1 M1 把某条 `contrib` 的**值**中性化（bit_count 的 TRANSFER 贡献不再 +1）
      ⇒ 转红（⇒ 证明 contrib 路径**活着**，不是旧手写链在兜底）—— **「假绿」的最后一道闸**
- [x] 4.2 M2 把某条 `contrib` 的**值**改错（uart_tx 的 shift_reg DATA 贡献）⇒ 转红
- [x] 4.3 M3 `resolve_hold` 丢失 hold 尾 ⇒ 转红
- [x] 4.4 M4 `resolve_or` 忽略 default ⇒ 转红
- [x] 4.5 M5 守卫误用 `!is_in(S)` ⇒ 转红
- [x] 4.6 M7 合成链的 **state 过滤失效**（每个状态都套进全部贡献）⇒ 转红
- [x] 4.7 M6 检出用 `CHERROR` 代替 `CHFATAL_EXCEPTION` ⇒ 对应
      `REQUIRE_THROWS_AS` 用例**转红**（⇒ 证明「假安全网」被抓住）
      ⚠️ **该变异第一轮恒绿** ⇒ 定位到「用例未隔离」（那些 lambda **也**会因
      「未 resolve」在**析构**时抛 ⇒ 掩盖被测的那条检查）⇒ ⇒ 补 `resolve` 隔离
      ⇒ ⇒ 转红

## 5. 编译期/运行期拒绝的用例（进**既有** target ⇒ 计数不变）
- [x] 5.1 `ch_in` 参与 `contrib` ⇒ **编译不过**（编译期断言）
- [x] 5.2 输出端口调 `resolve_hold` ⇒ **编译不过**
- [x] 5.3 `resolve_or` 无 default 形参 ⇒ **编译不过**
- [x] 5.4 重复 `(target, state)` ⇒ 抛
- [x] 5.5 `resolve` 后再 `contrib` ⇒ 抛
- [x] 5.6 同一 target 两次 `resolve` ⇒ 抛
- [x] 5.7 `contrib` 了但从未 `resolve` ⇒ 抛（析构）
- [x] 5.8 直接写与贡献写并存 ⇒ 抛
- [x] 5.9 `contrib` 的 target 是 `state_reg` 自身 ⇒ 抛
- [x] 5.10 枚举越界（`contrib` / `state()` / `transition_when`）⇒ 抛
- [x] 5.11 `set_entry(非 0 枚举)` ⇒ 抛

## 6. 契约同步与验收
- [x] 6.1 capability `chlib-state-machine-semantics` **MODIFIED ×1**（第 ⑥ 条）：
      放宽为「手写 `select` **或** `resolve_*` 生成」+ **以 NOTE 冻结** D5 的
      已知盲区（「`resolve` 之后再直接写无法检出」）⇒ 归档**禁** `--skip-specs`
- [x] 6.2 ⚠️ 第 ① 条（「`ch_reg` 的 next 是 last-wins」）**零 MODIFIED** ——
      本次**不改** `ch_reg` 语义
- [x] 6.3 `ctest -LE perf` ⇒ **191/191**
- [x] 6.4 `run_all_ported_tests.sh` ⇒ **28/28**；`run_all_samples.sh` ⇒ **35/35**
- [x] 6.5 **计数锚点不变**：`CTEST_TOTAL` **202** / `EXAMPLES_TEST_SOURCES` **17** /
      `ctest -L verilator_e2e` **38** / non-perf **191**
- [x] 6.6 13 个 `scripts/verify_*.sh`（含 4 个 `--self-test`）全 exit 0
- [x] 6.7 `include/chlib/AGENTS.md` 与根 `AGENTS.md` NOTES 同步
- [x] 6.8 归档：`openspec archive chlib-fsm-action-semantics-2026-10 --yes`
      （**禁** `--skip-specs`）
