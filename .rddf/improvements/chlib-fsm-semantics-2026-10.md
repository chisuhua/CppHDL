---
name: chlib-fsm-semantics-2026-10
priority: P2
source: uart-tx-fsm-live-logic-2026-10 + spi-fsm-fixture-live-logic-2026-10（两次实施期实测挖出**同型**缺陷）
phase: phase-5
category: testing
type: capability-gap
created_at: 2026-10-29
---

# Improvement: FSM 的「正确形态」是**隐性陷阱** ⇒ 需写进契约 + 加护栏

- **Date drafted**: 2026-10-29
- **Drafted by**: build-agent (builder role)
- **Status**: 📋 5-segment draft，归属 `feat-dual-backend-coverage` 的「**覆盖质量**」缺口
- **来源**：**两次**实施期实测（`uart-tx-fsm-live-logic-2026-10` 与
  `spi-fsm-fixture-live-logic-2026-10`）⇒ **同一缺陷在仓内两次真实发生**，
  且两次都被「**只有 parity**」的断言掩盖 ⇒ 长期「假绿」。

## 1. Why (背景与动机)

`include/chlib/state_machine.h` 的 `ch_state_machine` 是仓内**唯一**的 FSM DSL，
而有**两条**未被文档化的陷阱：

| # | 陷阱 | 后果 | 真实发生处 |
|---|---|---|---|
| **A** | `build()` **遍历所有** state 执行各自 `on_active` ⇒ 写在闭包内的 `reg->next` / 输出驱动**必被覆盖**（`ch_reg::next` 是 last-wins） | 「装载数据的分支**永不生效**」 | `uart_tx.cpp`（FSM 从未工作）· `test_verilator_complex_e2e.cpp`（SPI fixture 从未工作） |
| **B** | `transition_when` 的 target MUST 是**编译期常量** ⇒ **禁**用三元选 target（三元**建图期**求值 ⇒ 恒假） | 目标状态**永不可达** ⇒ 状态卡死 | 同上（`last_bit ? STOP : TRANSFER`） |

⚠️ **框架自己的头注释就演示了坏写法**：
```cpp
sm.state(MyState::IDLE).on_active([this, &sm]() {
    if (io().start) sm.transition_to(MyState::RUNNING);   // ← 建图期恒假
});
```
⇒ ⇒ **照抄示例就会写错** ⇒ ⇒ 「正确形态的 5 条纪律」只存在于 `AGENTS.md`
的 NOTES 与两个 example 的注释里，**框架文档里没有**。

⚠️ **只读盘点**：全仓 `on_active` 使用点只有 **3** 个文件（+ 一个无害的
`test_state_machine.cpp`）⇒ ⇒ **当前仓内已无受害站点**（两次都已修）
⇒ ⇒ 这是「**框架的诱导性设计**」，不是存量 bug ⇒ ⇒ 处置 = **契约 + 文档 +
护栏**，而不是继续逐个修 example。

## 2. What Changes (变更范围)

1. **框架头注释**：修掉坏示例 + 新增「5 条纪律」+「2 条陷阱」+「现状契约」
   （**零行为变更** ⇒ 爆炸半径为零 ⇒ 不需要 ADR）；
2. **新 capability** `chlib-state-machine-semantics`（ADDED ×6）⇒ 把「现状
   语义」写死（特别是 `ch_reg::next` 的 last-wins）⇒ 将来若要改，**必须**走
   MODIFIED delta ⇒ **把「静默」变成「受控」**；
3. **新静态护栏** `scripts/verify_fsm_next_wiring.sh`（+ `scripts/lib/fsm_wiring.py`
   共享分析器 + `--self-test` 12 fixture）⇒ 把「闭包内不得写寄存器 / 不得驱动
   IO」变成 **CI 可抓**；
4. **CI 挂载** 2 处。

### 范围外

- **不**改框架**行为**（`ch_reg` 多源 next / `build()` 遍历语义）⇒ **需 ADR**
  ⇒ architect；决策材料见 change 的 `design.md` §5；
- **不**引入 `if (硬件值)` 的运行期支持；
- **不**改 `spi_controller.cpp`（**不用** `ch_state_machine` ⇒ 已是正确形态）。

## 3. Acceptance (验收标准)

- [ ] 头注释：**坏示例已修** + 5 条纪律 + 2 条陷阱 + 现状契约；
- [ ] 新 capability **6 条 Requirement** ⇒ 归档后主 spec 的 `## Purpose`
      MUST 已补写；
- [ ] 护栏：主路径绿（471 文件 / 0 违规）+ `--self-test` **12/12**；
- [ ] ⭐ **对真实历史缺陷有效**：换回 `5801911` 的旧 `describe()`
      ⇒ MUST 报出 **5** 处 `reg-next-in-closure`；
- [ ] CI **2 处**挂载；
- [ ] 既有 12 个护栏 + 3 个 `--self-test` 仍全 exit 0；
- [ ] `ctest -LE perf` **191/191** · examples **28/28** · samples **35/35**；
- [ ] **计数锚点不变**（202 / 17 / 38 / 191）。

## 4. Capabilities (涉及的能力维度)

- **ADDED ×6**（**新** capability）`chlib-state-machine-semantics`：
  ① `ch_reg::next` 是 **last-wins**（现状契约）· ② `build()` 遍历所有 state
  ⇒ 闭包内 MUST 只写 `transition_when`（+ 护栏 MUST 检出）· ③ target MUST 是
  编译期常量 · ④ 条件**电平敏感** · ⑤ 硬件值 MUST NOT 参与 C++ 控制流 ·
  ⑥ 寄存器与输出 MUST 有唯一写入点 / 驱动点。

## 5. Impact (影响面分析)

- **改动文件**：`include/chlib/state_machine.h`（**仅注释**）·
  `scripts/lib/fsm_wiring.py`（新）· `scripts/verify_fsm_next_wiring.sh`（新）·
  `.github/workflows/ci.yml` · 新 capability 主 spec · `AGENTS.md` ·
  `.rddf/roadmap.md`
- **计数**：**不变**
- **爆炸半径**：**零**（无运行时代码改动）
- **风险**：**低–中** —— 护栏是自研的 C/C++ 子集分析器 ⇒「不可能被完全闭合」
  ⇒ 方向定为「**宁可漏报、不误报**」+ `parse-error` **fail-closed** +
  12 个 fixture + 对**真实历史缺陷**的端到端验证。
- **待裁决（architect）**：`ch_reg` 的「多源 next」选项 A/B/C（见 change 的
  `design.md` §5）—— ⚠️ 选项 B 会**静默改变**既有语义 ⇒ **本 change 是它的前置**。
