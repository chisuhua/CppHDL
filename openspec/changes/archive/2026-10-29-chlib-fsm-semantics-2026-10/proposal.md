# 提案：FSM 的「正确形态」从隐性陷阱变成明文契约（+ 静态护栏）

- **架构依据**：**无 ADR**（本 change **不改任何框架行为**，只把**现状语义**
  写进契约 + 补文档 + 加护栏；真正的**能力变更**见 §「待裁决」，交 architect）
- **5 段草案**：`.rddf/improvements/chlib-fsm-semantics-2026-10.md`
- **roadmap 归属**：`feat-dual-backend-coverage` 的「**覆盖质量**」缺口
- **规格影响**：**新增** capability `chlib-state-machine-semantics`（ADDED ×6）
  ⇒ 归档**禁** `--skip-specs`
- **规模 / 风险**：**中**（1 个框架头注释 + 1 个新护栏 + 1 个新 capability）

## Why

`ch_state_machine` DSL（`include/chlib/state_machine.h`）是仓内**唯一**的
FSM 状态机 DSL，而它有**两条**未被文档化的陷阱 —— **同一缺陷在仓内两次真实
发生**，两次都被「**只有 parity**」的断言掩盖 ⇒ 长期「假绿」：

| # | 陷阱 | 后果 | 真实发生处 |
|---|---|---|---|
| A | `build()` **遍历所有** state 执行各自 `on_active` ⇒ 写在闭包内的 `reg->next` / 输出驱动**必被覆盖**（last-wins） | 「装载数据的分支**永不生效**」 | `uart_tx.cpp`（FSM 从未工作）· `test_verilator_complex_e2e.cpp`（SPI fixture 从未工作） |
| B | `transition_when` 的 target MUST 是**编译期常量** ⇒ **禁**用三元选 target | 目标状态**永不可达** ⇒ 状态卡死 | 同上（`last_bit ? STOP : TRANSFER`） |

⚠️ **框架自己的头注释就演示了坏写法**（`state_machine.h` 的 Usage Example）：

```cpp
sm.state(MyState::IDLE).on_active([this, &sm]() {
    if (io().start) sm.transition_to(MyState::RUNNING);   // ← 建图期恒假
});
```

⇒ ⇒ **新用户照抄示例就会写错** ⇒ ⇒ 「正确形态的 5 条纪律」只存在于
`AGENTS.md` 的 NOTES 与两个 example 的注释里，**框架文档里没有**。

⚠️ **只读盘点结论（重要）**：全仓 `on_active` 使用点只有 **3** 个文件
（框架头注释 · `uart_tx.cpp` · `test_verilator_complex_e2e.cpp`）+
`tests/test_state_machine.cpp`（只写 C++ 标志，无害）；
`examples/spinalhdl-ported/spi/spi_controller.cpp` **不用** `ch_state_machine`
（手写 `ch_reg` + `select`）⇒ **它本来就是正确形态**（单 next 写入点）⇒
⇒ **当前仓内已无受害站点**（`uart_tx` 与 `complex_e2e` 已由
`uart-tx-fsm-live-logic-2026-10` / `spi-fsm-fixture-live-logic-2026-10` 修掉）
⇒ ⇒ **本缺陷是「框架的诱导性设计」，不是存量 bug** ⇒ ⇒ 正确的处置是
**把契约写死 + 加护栏**，**不是**继续逐个修 example。

## What Changes

### 范围内

1. **框架头注释**（`include/chlib/state_machine.h`）：
   - **修掉坏示例**（`if (io().start) sm.transition_to(...)` → `transition_when`）；
   - **明文化** 5 条纪律 + 2 条陷阱（A/B）+ 「现状契约」段（把实现语义写死
     ⇒ 将来改语义 MUST 走 MODIFIED delta）。
2. **新 capability** `chlib-state-machine-semantics`（ADDED ×6）⇒ 把上面这些
   写成**契约**（含 Scenario）。
3. **新静态护栏** `scripts/verify_fsm_next_wiring.sh`（+ `--self-test`
   12 fixture）⇒ 把「闭包内不得写寄存器 / 不得驱动 IO」变成**CI 可抓**；
   分析器抽到 `scripts/lib/fsm_wiring.py`（护栏与 fixture 自检**共用同一份**）。
4. **CI 挂载** 2 处（与其它护栏同型）。

### 范围外

- **不**改 `ch_reg` / `ch_state_machine` 的**行为**（= 能力变更 ⇒ 需 ADR ⇒
  architect；决策材料见 `design.md` §5）；
- **不**改 `examples/spinalhdl-ported/spi/spi_controller.cpp`（已是正确形态）；
- **不**新增/修改任何测试用例（⇒ 计数锚点不变）。

## Acceptance

- [ ] `include/chlib/state_machine.h` 的头注释：**坏示例已修** + 5 条纪律 +
      2 条陷阱 + 现状契约；
- [ ] **新 capability** `chlib-state-machine-semantics`（ADDED ×6）⇒ 归档后
      主 spec 的 `## Purpose` MUST 已补写（CLI 不会自动写）；
- [ ] **新护栏** `scripts/verify_fsm_next_wiring.sh`：
      - 主路径在**当前仓**上 **绿**（471 个源文件、**0** 违规）；
      - `--self-test` **12/12 fixture 全绿**；
      - ⭐ **对真实历史缺陷有效**：把 `test_verilator_complex_e2e.cpp` 的
        `describe()` 换回 `5801911` 的旧版 ⇒ MUST 报出 **5** 处
        `reg-next-in-closure`；
- [ ] CI **2 处**挂载（与其它护栏同型）；
- [ ] 12 个既有护栏 + 3 个 `--self-test` 仍全 exit 0；
- [ ] `ctest -LE perf` **191/191**；`run_all_ported_tests.sh` **28/28**；
      `run_all_samples.sh` **35/35**；
- [ ] **计数锚点不变**：`CTEST_TOTAL` 202 / `EXAMPLES_TEST_SOURCES` 17 /
      `verilator_e2e` 38 / non-perf 191。

## Capabilities

- **ADDED ×6**（**新** capability）`chlib-state-machine-semantics`：
  ① `ch_reg` 的 `next` 写入是 **last-wins**（现状契约 ⇒ 未来改语义 MUST 走
  MODIFIED）· ② `build()` **遍历所有** state ⇒ 闭包内 MUST NOT 写寄存器 /
  驱动 IO（+ **静态护栏 MUST 检出**）· ③ `transition_when` 的 target MUST 是
  **编译期常量** · ④ `transition_when` 的条件**电平敏感** · ⑤ 硬件值 MUST NOT
  参与 C++ 控制流 · ⑥ FSM 的寄存器与输出 MUST 有**唯一**写入点 / 驱动点。

## Impact

- **改动文件**：`include/chlib/state_machine.h`（注释）·
  `scripts/lib/fsm_wiring.py`（新）· `scripts/verify_fsm_next_wiring.sh`（新）·
  `.github/workflows/ci.yml` · `openspec/specs/chlib-state-machine-semantics/`
  （新）· `AGENTS.md` · `.rddf/roadmap.md`
- **计数**：**不变**（不新增 ctest 条目 / 不新增测试文件）
- **爆炸半径**：**零**（只改**注释** + 加**只读**护栏 ⇒ 不影响任何运行时代码）
- **风险**：**低–中** —— 护栏是**自研的 C/C++ 子集分析器** ⇒ 「不可能被完全
  闭合」⇒ 故：① 方向定为「**宁可漏报、不误报**」；② `parse-error` **fail-closed**；
  ③ **12 个 fixture** 逐条钉住判别力；④ **对真实历史缺陷**做端到端验证。

## Non-Goals

- **不**改框架**行为**（`ch_reg` 多源 next / `build()` 的遍历语义）⇒ ADR；
- **不**引入 `if (硬件值)` 的**运行期**支持（= 更大的框架能力缺口）；
- **不**给护栏加「例外机制」（当前**无**违规站点 ⇒ 不需要）；
- **不**改 `examples/spinalhdl-ported/spi/spi_controller.cpp`。
