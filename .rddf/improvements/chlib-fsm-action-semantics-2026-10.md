---
name: chlib-fsm-action-semantics-2026-10
priority: P1
source: ADR-050（2026-10-29 立项；由 oracle 两轮复核 + metis 缺口分析 + critic 两轮对抗审稿收敛）
phase: phase-5
category: framework
type: capability-change
created_at: 2026-10-29
---

# Improvement: 实施 ADR-050 —— `ch_state_machine` 的状态动作语义 + 死/误导 API 的物理移除

- **Date drafted**: 2026-10-29
- **Drafted by**: build-agent（builder role；决策由 architect 落 ADR-050）
- **Status**: 📋 5-segment draft，**已立项**（change `chlib-fsm-action-semantics-2026-10`）
- **来源**：ADR-050（`docs/adr/ADR-050-chlib-state-machine-semantics.md`）

## 1. Why (背景与动机)

`ch_state_machine`（`include/chlib/state_machine.h`）是仓内唯一的 FSM DSL。它的
**设计意图**（2026-03-30 设计文档：`on_active` ≡ SpinalHDL `whenIsActive`
= **状态守卫**语义，§6.3 写 `if (state_reg == STATE_A) { 执行 state_A 的 on_active; }`）
与**实现**（`build()` 在 describe 期**遍历所有** state 执行各自 `on_active`
⇒ 展平）**语义分歧** ⇒ 由此产生两条此前完全未文档化的陷阱：

| 陷阱 | 后果 | 真实发生处 |
|---|---|---|
| 闭包内写寄存器 / 输出**必被覆盖**（`ch_reg::next` 与 `ch_out::operator=` 都是 `set_src(0,…)` ⇒ last-wins）| 「装载数据的分支**永不生效**」 | `uart_tx.cpp`（FSM 从未工作）· `test_verilator_complex_e2e.cpp`（SPI fixture 从未工作） |
| `transition_when` 的 target 必须是**编译期常量**（枚举形参 ⇒ 调用点固化 ⇒ 三元在**建图期**求值）| 目标状态**永不可达** ⇒ 状态卡死 | 同上（`last_bit ? STOP : TRANSFER`） |

⇒ **同一缺陷两次真实发生**，两次都被「**只有 parity**」的断言掩盖 ⇒ 两后端由
同一份 fixture 生成 ⇒ 缺陷**同构** ⇒ parity **恒绿** ⇒ 长期「假绿」。

⚠️ **只读盘点**：`ch_state_machine` 仓内实例化点仅 **5** 处、`on_active` 使用
文件仅 **3** 个 ⇒ **当前无受害站点**（两次事故已由前两个 change 修掉）⇒ ⇒
处置 = **框架能力 + 契约**，不是继续逐个修 example。

⚠️ 上一 change（`chlib-fsm-semantics-2026-10`）已把两条陷阱写成**契约** +
**静态护栏** ⇒ 把「静默」变成「可抓」；本项处理**更上位的根因**。

## 2. What Changes (变更范围)

**commit 1 —— 移除 + 硬化（先清场）**：移除 **15** 个符号（`on_exit`/`on_entry`/
`then()` 三件套/`state_changed`/`tick_state()`/`is_built()`/三宏/`parent`+`friend`）
+ `current_state()` → `entry_state()` 改名 + 3 项硬化（`entry_state` 字段初始化
（现为 **UB**）· `set_entry(非 0)` ⇒ 抛 · 枚举越界 ⇒ 抛）。

**commit 2 —— 状态守卫贡献 API + dogfood**：
`sm.contrib(target, S, value)` + `sm.resolve_hold(reg)` /
`sm.resolve_or(target, default)` ⇒ 框架只合成**一条** select 链、**用户写一次**
⇒ 迁移 `uart_tx.cpp` 与 `test_verilator_complex_e2e.cpp` 并**物理删除**旧手写链。

⇒ **保留** `transition_to()`（主 spec 第 ② 条的 MUST 与 `## Purpose` 都引用它
⇒ 删它会**冻错主 spec**）。

## 3. Acceptance (验收标准)

ADR-050 §5 的 **16** 条可勾选判据（静态护栏 / 编译期拒绝 / 运行期拒绝 /
变异验证 / 行为回归 / 计数锚点），要点：

- [ ] 编译期拒绝：`ch_in` 参与 `contrib` · 输出端口调 `resolve_hold` ·
      `resolve_or` 无 default ⇒ 均**编译不过**；
- [ ] 运行期拒绝（`REQUIRE_THROWS_AS(..., std::runtime_error)`）：重复贡献 ·
      `resolve` 后再 `contrib` · 两次 `resolve` · 未 `resolve` · 直接写并存 ·
      `state_reg` 自贡献 · 枚举越界 · `set_entry(非 0)`；
- [ ] **变异 M1 删任一 `contrib` ⇒ 转红**（证明 contrib 路径**活着**，不是旧
      手写链在兜底 ⇒ 这是「假绿」的最后一道闸）；
- [ ] `uart_tx` 与 `complex_e2e` 的既有金标**逐项不变**；双后端 parity 保留；
- [ ] **计数锚点不变**（202 / 17 / 38 / 191）。

## 4. Capabilities (涉及的能力维度)

- **MODIFIED ×1**：`chlib-state-machine-semantics` 第 ⑥ 条 ⇒ 放宽为「手写
  `select` **或** `resolve_*` 生成」+ **以 NOTE 冻结**已知盲区（「`resolve`
  之后再直接写**无法**检出」）⇒ 归档**禁** `--skip-specs`。
- ⚠️ 第 ① 条（`ch_reg` 的 last-wins）**零 MODIFIED** ⇒ 本次**不改** `ch_reg`
  语义 ⇒ ADR-050 已**关闭**「多源 next」这一悬挂议题（= 保留单源 + 库层 sugar）。

## 5. Impact (影响面分析)

- **改动文件**：`include/chlib/state_machine.h`（主）· `tests/test_state_machine.cpp` ·
  `examples/spinalhdl-ported/uart/uart_tx.cpp` · `tests/test_verilator_complex_e2e.cpp` ·
  `scripts/verify_fsm_next_wiring.sh` · capability 主 spec · `AGENTS.md` ·
  `include/chlib/AGENTS.md` · `.rddf/roadmap.md`
- **计数**：**不变**（新用例进**既有** target）
- **爆炸半径**：**小**（`core/` 与 `ch_reg`/`ch_out` 公开契约**零变化** ⇒ 不影响
  任何库 / 产品代码；仅 `ch_state_machine` 的 public surface 缩减 + 新增贡献 API）
- **风险**：**中–高** —— ① **公开 API 缩减**（15 符号 + 1 改名）⇒ 破坏性变更
  ② **新增 API 面** ⇒ 从此两种写法 ⇒ 文档 / 评审 / 教学成本上升 ③ **SpinalHDL
  parity 让步**（`onEntry`/`onExit` 不再提供；`setEntry` 硬化）④ 新增的**三条
  生命周期检出**本身是新失败模式的来源 ⇒ MUST 有各自的变异验证
- **已知的失败机制陷阱**：`CHERROR`/`CHFATAL`/`CHREQUIRE` **只打日志、不抛**
  ⇒ 用它们做安全网 = **假安全网** ⇒ MUST 用 **`CHFATAL_EXCEPTION`**
