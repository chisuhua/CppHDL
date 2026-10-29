# 提案：实施 ADR-050 —— `ch_state_machine` 的状态动作语义 + 死/误导 API 的物理移除

- **架构依据**：**ADR-050**（`docs/adr/ADR-050-chlib-state-machine-semantics.md`，
  2026-10-29，**2 轮对抗审稿**：REJECT → APPROVE_WITH_CONDITIONS，0 BLOCKER）
- **5 段草案**：`.rddf/improvements/chlib-fsm-action-semantics-2026-10.md`
- **roadmap 归属**：`feat-dual-backend-coverage` 的「**覆盖质量**」缺口（FSM 语义）
- **规格影响**：**MODIFIED ×1**（`chlib-state-machine-semantics` 第 ⑥ 条）
  ⇒ 归档**禁** `--skip-specs`
- **规模 / 风险**：**中–高**（公开 API 缩减 + 新增 API 面 + 框架语义变更）

## Why

`ch_state_machine`（`include/chlib/state_machine.h`）是仓内唯一的 FSM DSL。
它的**设计意图**（2026-03-30 设计文档：`on_active` ≡ SpinalHDL `whenIsActive`
= **状态守卫**语义）与**实现**（`build()` 在 describe 期**遍历所有** state 执行
各自 `on_active` ⇒ 展平）**语义分歧** ⇒ 由此产生两条此前完全未文档化的陷阱：

| 陷阱 | 后果 | 真实发生处 |
|---|---|---|
| 闭包内写寄存器 / 输出**必被覆盖**（last-wins） | 「装载数据的分支**永不生效**」 | `uart_tx.cpp`（FSM 从未工作）· `test_verilator_complex_e2e.cpp`（SPI fixture 从未工作） |
| `transition_when` 的 target 必须是**编译期常量** | 目标状态**永不可达** ⇒ 状态卡死 | 同上（`last_bit ? STOP : TRANSFER`） |

⇒ **同一缺陷两次真实发生**，两次都被「**只有 parity**」的断言掩盖 ⇒ 长期「假绿」。

上一 change（`chlib-fsm-semantics-2026-10`）已把两条陷阱写成**契约** + **静态
护栏** ⇒ 把「静默」变成「可抓」；**本 change 实施 ADR-050**，处理更上位的根因：
**裁决状态动作的语义方向**，并**清场**死 / 误导 API。

⚠️ **只读盘点结论**：`ch_state_machine` 仓内实例化点仅 **5** 处、`on_active`
使用文件仅 **3** 个 ⇒ **当前无受害站点** ⇒ ⇒ 处置 = **框架能力 + 契约**，不是
继续逐个修 example。

## What Changes

### 范围内

**commit 1 —— 移除 + 硬化（先清场）**

| 动作 | 对象 |
|---|---|
| **移除**（15 个符号）| `on_exit`/`exit_action` · `on_entry`/`entry_action` + `build()` 执行块 · `then()`/`next_state_on_exit`/`has_exit_transition` + `build()` 分支 + `state_def::parent` + `friend` · `state_changed` · `tick_state()` · `is_built()` · `CH_STATE_MACHINE`/`CH_STATE`/`CH_STATE_MACHINE_BUILD` |
| **改名** | `current_state()` → `entry_state()`（字段 `entry_state` → `entry_state_`；**不留** deprecated 别名）|
| **保留**（ADR-050 D2 明示）| `transition_to()` —— 主 spec 第 ② 条的 MUST 与 `## Purpose` **都引用它** ⇒ 删它会**冻错主 spec** |
| **硬化** | ① `entry_state` 字段**在 ctor 初始化列表中初始化**（现为 **UB**）② `set_entry(非 0 枚举)` ⇒ `CHFATAL_EXCEPTION`（`state_reg` 构造期 init 恒 `0_d` ⇒ 非 0 入口 = **静默错误复位态**）③ `state()`（const 与非 const **两个**重载）/ `transition_when()`（归属索引 + target 字面量）/ `contrib()` 的**枚举越界** ⇒ `CHFATAL_EXCEPTION`（`states_[…]` 现**无边界检查** ⇒ 越界 UB；`static_cast<uint8_t>` 会**静默截断** ≥256 的 target）|
| **迁移** | `tests/test_state_machine.cpp`（`current_state()` / `on_entry` 的**唯一**消费者）|

**commit 2 —— 状态守卫贡献 API + dogfood**

```cpp
sm.contrib(target, S, value);         // 显式 state 参数；守卫 = 纯 is_in(S)
reg->next = sm.resolve_hold(reg);      // 仅 ch_reg 重载；默认 = 保持
out     = sm.resolve_or(out, default); // 显式默认（输出 MUST NOT 用 hold ⇒ 组合环）
```

- **作用域**：**寄存器 + 输出** ⇒ `ch_in` **编译期拒绝**（重载 = `delete`）；
- **`contrib` MUST 在 `on_active` 闭包之外** ⇒ 保住 capability 第 ② 条**字面**成立；
- **value**：**eager 捕获**（禁 lambda 延迟求值）；
- **检出**（全部 MUST 用 **`CHFATAL_EXCEPTION`** —— ⚠️ 实测 `CHERROR` /
  `CHFATAL` / `CHREQUIRE` **只打日志、不抛** ⇒ 用它们做安全网 = **假安全网**）：
  重复 `(target, state)`（在 `contrib` 调用点）· `resolve` 后再 `contrib` ·
  同一 target 两次 `resolve` · `contrib` 了但从未 `resolve`（**析构时**逐 target
  检查，仅当 `std::uncaught_exceptions() == 0` 且非 static-destruction）
  · 直接写与贡献写并存（`resolve` 时查 `num_srcs() > 0`）· `contrib` 的 target
  是 `state_reg` 自身；
- **dogfood**：迁移 `uart_tx.cpp` 与 `test_verilator_complex_e2e.cpp` ⇒
  **物理删除**旧手写链（保留两份 ⇒ 双写 ⇒ last-wins ⇒ 金标仍全绿 ⇒ contrib
  路径可能是**死的**而无人知晓 ⇒ 与本 change 要消灭的「假绿」同构）；
- **capability delta**：**MODIFIED ×1**（第 ⑥ 条：放宽「单一 `select` 表达式」
  ⇒ 「或 `resolve_*` 生成」，**并以 NOTE 冻结 D5 的已知盲区**）。

### 范围外

- **不改** `core/`（`ch_reg` / `ch_out` / `core/reg.h` **零改动**）；
- **不**提供真运行时 entry / exit 动作（需 **prev_state 寄存器** ⇒ 独立立项）；
- **不**复活 `chlib/switch.h` 的 `case_` 组合子（独立立项）；
- **不**提供 `if (硬件值)` 的运行期支持；
- **不**禁用手写 `select` 链（**保留**为合法降级路径）。

## Acceptance

验收判据 = ADR-050 §5 的 **16** 条（可勾选），要点：

- [ ] **静态护栏**：主路径绿 + `--self-test` 12/12（新增 `contrib-in-closure`
      词法规则 ⇒ 计数相应更新；方向仍为「宁可漏报、不误报」）；
- [ ] **编译期拒绝**：`ch_in` 参与 `contrib`；输出端口调 `resolve_hold`；
      `resolve_or` 无 default 形参 ⇒ 均**编译不过**；
- [ ] **运行期拒绝**（`REQUIRE_THROWS_AS(..., std::runtime_error)`）：重复贡献 ·
      `resolve` 后再 `contrib` · 两次 `resolve` · 未 `resolve` · 直接写并存 ·
      `state_reg` 自贡献 · 枚举越界 · `set_entry(非 0)`；
- [ ] **变异验证**（每条**先确认编译成功**）：M1 删任一 `contrib` ⇒ 转红（证明
      contrib 路径**活着**）· M2 交换两状态贡献值 · M3 丢 hold 尾 · M4 忽略
      default · M5 守卫误用 `!is_in(S)`；
- [ ] **行为回归**：`uart_tx` 与 `test_verilator_complex_e2e` 的既有金标
      **逐项不变**；双后端 parity 保留；生成 Verilog 每个输出**仍各 1 条** assign；
      旧手写链**已物理删除**（grep 证据）；
- [ ] **计数锚点不变**：`CTEST_TOTAL` 202 / `EXAMPLES_TEST_SOURCES` 17 /
      `verilator_e2e` 38 / non-perf 191；
- [ ] `ctest -LE perf` **191/191**；`run_all_ported_tests.sh` **28/28**；
      `run_all_samples.sh` **35/35**；13 个护栏全 exit 0。

## Capabilities

- **MODIFIED ×1**：`chlib-state-machine-semantics` 第 ⑥ 条（「FSM 的寄存器与输出
  MUST 有唯一写入点 / 驱动点」）⇒ 放宽为「手写 `select` **或** `resolve_*`
  生成」+ **冻结已知盲区**（NOTE）。
- ⚠️ 第 ① 条（「`ch_reg` 的 `next` 写入是 last-wins」）**零 MODIFIED** ——
  本次**不改** `ch_reg` 的语义（框架只合成 select 链、**用户写一次**）。

## Impact

- **改动文件**：`include/chlib/state_machine.h`（主）·
  `tests/test_state_machine.cpp` · `examples/spinalhdl-ported/uart/uart_tx.cpp` ·
  `tests/test_verilator_complex_e2e.cpp` · `scripts/verify_fsm_next_wiring.sh`
  （+ `scripts/lib/fsm_wiring.py` 加一条词法规则）· capability 主 spec ·
  `AGENTS.md` · `include/chlib/AGENTS.md` · `.rddf/roadmap.md`
- **计数**：**不变**（新用例进**既有** target ⇒ `CTEST_TOTAL` / `EXAMPLES_TEST_SOURCES` 不变）
- **爆炸半径**：**小**（`core/` 与公开契约零变化 ⇒ 不影响任何库 / 产品代码；
  仅 `ch_state_machine` 的 public surface 缩减 + 新增贡献 API）
- **风险**：**中–高** —— ① **公开 API 缩减**（15 个符号 + 1 改名）⇒ 破坏性变更
  （仓内消费者 5 处，同 commit 改完）② **新增 API 面** ⇒ 从此有两种写法 ⇒
  文档 / 评审 / 教学成本上升 ③ **SpinalHDL parity 让步**（`onEntry`/`onExit`
  不再提供；`setEntry` 硬化）④ 新增的**三条生命周期检出**本身是新失败模式的
  来源 ⇒ MUST 有各自的变异验证

## Non-Goals

- **不改** `core/` 的任何语义（`ch_reg` 多源 next **明确否决** ⇒ ADR-050 已关闭
  该悬挂议题）
- **不**实现真运行时 entry / exit 动作（需 prev_state 寄存器 ⇒ 独立立项）
- **不**复活 `chlib/switch.h`
- **不**提供 `if (硬件值)` 的运行期支持
- **不**禁止手写 `select` 链
