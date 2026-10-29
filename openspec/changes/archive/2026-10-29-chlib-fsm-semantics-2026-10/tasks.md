# Tasks: chlib-fsm-semantics-2026-10

## 0. 准备（**只读盘点** —— 事实，非推断）
- [x] 0.1 `build()` 的遍历语义 —— 实读 `include/chlib/state_machine.h` 的
      `build()`：按 state index 遍历执行**每个** `states_[i].active_action()`
- [x] 0.2 覆盖语义 —— 实读 `regimpl::set_next` = `set_src(0, next)`
      （`include/ast/ast_nodes.h:47-54`）⇒ **last-wins**；`ch_out::operator=`
      同型（`include/core/io.h:274`）
- [x] 0.3 `transition_when` 的 target 是**枚举形参** ⇒ MUST 在调用点定死
      ⇒ **禁**三元选 target；且 `ch_bool::operator uint64_t()` 只对 const
      节点返真值（`src/lnode/bool.cpp:44-49`）⇒ 三元**建图期**恒假
- [x] 0.4 全仓 `on_active` 使用点盘点 ⇒ **3** 个文件 + `test_state_machine.cpp`
      ⇒ **当前仓内已无受害站点**（⇒ 处置 = 契约 + 文档 + 护栏）
- [x] 0.5 `examples/spinalhdl-ported/spi/spi_controller.cpp` **不用**
      `ch_state_machine` ⇒ **本就是正确形态**（单 next 写入点）⇒ **不**修

## 1. 框架文档（`include/chlib/state_machine.h` —— **只改注释**）
- [x] 1.1 **修掉坏示例**：`if (io().start) sm.transition_to(...)`
      ⇒ `sm.transition_when(start_sig, ...)`（`start_sig` 是**节点**）
- [x] 1.2 新增「写 FSM 的 **5 条纪律**」段（条件只用硬件原语 / 每寄存器**单**
      next 写入点 / 每输出**只**驱动一次 / `transition_when` 条件电平敏感 /
      不把 parity 当正确性证据）
- [x] 1.3 新增「两条**框架级**陷阱」段：
      **(A)** `build()` 遍历所有 state ⇒ 闭包内 MUST NOT 写寄存器 / 驱动输出；
      **(B)** `transition_when` 的 target MUST 是编译期常量
- [x] 1.4 新增「现状契约」段（把 select 树合成规则 + 「后注册者优先」写死
      ⇒ 将来改语义 MUST 走 MODIFIED delta）
- [x] 1.5 **零行为变更**（只改注释）⇒ 爆炸半径为零 ⇒ **不需要 ADR**

## 2. 契约（新 capability `chlib-state-machine-semantics`，ADDED ×6）
- [x] 2.1 `ch_reg` 的 `next` 写入是 **last-wins**（现状契约）
- [x] 2.2 `build()` **遍历所有** state ⇒ 闭包内 MUST **只**写
      `transition_when` / MUST NOT 写寄存器 / MUST NOT 驱动 IO
      （+ **静态护栏 MUST 检出** 的 Scenario）
- [x] 2.3 `transition_when` 的 target MUST 是**编译期常量** ⇒ 拆成**多条**
- [x] 2.4 `transition_when` 的条件**电平敏感** ⇒ 「持续 N 拍」MUST 合取
- [x] 2.5 硬件值 MUST NOT 参与 C++ 控制流
- [x] 2.6 寄存器与输出 MUST 有**唯一**写入点 / 驱动点
- [x] 2.7 归档后主 spec 的 `## Purpose` —— ⚠️ **实测订正**：CLI 对**新建**
      capability **会**用 delta 的 `## Purpose` 自动写入（本次已写入 ⇒ **无需**
      手工补写）；⚠️ 只有对**已存在**的 capability，delta 的 Purpose 才被
      **忽略**（CLI 会警告 "delta Purpose ignored"）⇒ 那时才需照
      `axi-slave-conformance` 的先例手工补写

## 3. 静态护栏（`scripts/verify_fsm_next_wiring.sh` + `scripts/lib/fsm_wiring.py`）
- [x] 3.1 分析器抽成 **lib**（护栏与 fixture 自检**共用同一份** ⇒ 不会漂移）
- [x] 3.2 抹掉注释与字符串/字符字面量（等长空格 ⇒ 行号不变）
- [x] 3.3 只把「`on_active(` 之后**先**出现 capture list `[`」的当作 lambda
      ⇒ 框架自身的成员函数**定义**不被误判
- [x] 3.4 花括号配平 + **配平后下一个非空白字符 MUST 是 `)`**
      ⇒ 否则 **fail-closed**（`parse-error`）
- [x] 3.5 R1（闭包内 `->next =`）/ R2（闭包内 `io().x =` / `<<=`）
- [x] 3.6 `--self-test` **12 个 fixture**（逐条钉住判别力）⇒ **12/12**
- [x] 3.7 主路径在**当前仓**上绿（471 个源文件 / **0** 违规）
- [x] 3.8 ⭐ **对真实历史缺陷有效**：把 `complex_e2e` 的 `describe()` 换回
      `5801911` 的旧版 ⇒ 护栏报出 **5** 处 `reg-next-in-closure`
- [x] 3.9 CI **2 处**挂载（主路径 + `--self-test`）

## 4. 验收
- [x] 4.1 12 个既有 `scripts/verify_*.sh` + 3 个 `--self-test` 仍全 exit 0
      （新护栏另计：主路径 + `--self-test`）
- [x] 4.2 `ctest -LE perf` ⇒ **191/191**
- [x] 4.3 `run_all_ported_tests.sh` ⇒ **28/28**
- [x] 4.4 `run_all_samples.sh` ⇒ **35/35**
- [x] 4.5 **计数锚点不变**：`CTEST_TOTAL` **202** / `EXAMPLES_TEST_SOURCES` **17** /
      `ctest -L verilator_e2e` **38** / non-perf **191**
      （本 change **不新增**测试文件）
- [x] 4.6 `openspec validate --specs --strict` 的 warning 属**既有 CLI 版本噪声**
      （仓钉 1.4.1 / 本机 1.14.3；CI **不**跑该命令）
- [x] 4.7 编译 0 error / 0 **新增** warning（本 change **零**代码改动 ⇒ 不必重编，
      但 `include/chlib/state_machine.h` 的注释改动 MUST 仍能编译 ⇒ 已实测）

## 5. 文档与归档
- [x] 5.1 根 `AGENTS.md` NOTES 记录
- [x] 5.2 `.rddf/roadmap.md` 的 sprint 表增行（**簿记**，非 feature `status`）
- [x] 5.3 `openspec archive chlib-fsm-semantics-2026-10 --yes`
      （**禁** `--skip-specs` —— 本 change **新增** capability）
- [x] 5.4 ⚠️ **待裁决项**（`ch_reg` 多源 next 的 ADR 输入）已写入
      `design.md` §5 ⇒ **交 architect**，本 change **不**裁决
