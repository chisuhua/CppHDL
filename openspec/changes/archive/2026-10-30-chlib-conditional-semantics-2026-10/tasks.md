# Tasks: chlib-conditional-semantics-2026-10

## 0. 只读核实（实施前）

- [x] 0.1 核实 `_if` / `conditional_reg_assign` / `if_then` / `priority_if` 的
      **全仓消费者**（确认活消费者 = 0）
- [x] 0.2 核实 `if_stmt.h` 的 `end()` 与 `next_proxy::operator=` 的**逐行**时序
- [x] 0.3 核实 `multi_if` 与 `priority_if` 的 `operator T()` **逐行等价**
- [x] 0.4 核实 `multi_if` 的「先匹配者胜」语义（从后往前构链）
- [x] 0.5 核实 `fsm_wiring.py` 的 `find_closures` 入口表**专用性**（`on_active(`）

## 1. commit 1 —— 共享组合器 + `_if` 的守卫贡献实现

- [x] 1.1 新增 `include/chlib/guard_contribution.h`（共享组合器）
- [x] 1.2 `state_machine.h` 的 `compose()` 改用共享组合器（**纯重构**）
- [x] 1.3 `if_stmt.h`：`end()` 的 `finalized_` 置位移到遍历**之后**
- [x] 1.4 `if_stmt.h`：`conditional_reg_assign` → `conditional_assign`
      （统一 `ch_reg` / `ch_out`；**不留** deprecated 别名）
- [x] 1.5 `if_stmt.h`：实现候选值**捕获**（追加到 target 贡献表）
- [x] 1.6 `if_stmt.h`：`end()` 合成**一条** select 链 + **唯一**写入点
- [x] 1.7 `if_stmt.h`：默认尾部（寄存器=保持 / 输出=必须 `else_`，缺则**抛**）
- [x] 1.8 `if_stmt.h`：`else_` 候选**单独**作为尾部（不进链）
- [x] 1.9 `if_stmt.h`：active block 改**栈**（嵌套）
- [x] 1.10 `if_stmt.h`：析构 `noexcept(false)` + 展开期不抛
- [x] 1.11 重写 `tests/chlib/test_if_stmt.cpp`（**移除**钉住坏行为的用例
      + 补**硬件级**断言 + 编译期断言被移除的 API 不存在）

## 2. commit 2 —— 护栏 + 表面收敛 + sample 归位

- [x] 2.1 新增 `scripts/lib/if_stmt_wiring.py`（复用 `fsm_wiring` 的词法器）
- [x] 2.2 新增 `scripts/verify_if_stmt_wiring.sh`（主路径 + `--self-test`）
- [x] 2.3 移除 `include/chlib/if.h` 的 `priority_if` / `priority_if_then`
- [x] 2.4 重写 `samples/conditional_demo.cpp` + 注册进 `CH_SAMPLE_TARGETS`
- [x] 2.5 从 `samples/disabled_samples.txt` 移除 `conditional_demo` 行
- [x] 2.6 CI 挂载新护栏（主路径 + self-test）
- [x] 2.7 更新 `include/chlib/AGENTS.md` + 根 `AGENTS.md` 计数锚点

## 3. 变异验证（每条**先确认编译成功**）

- [x] 3.1 M1 `finalized_` 置回遍历之前 ⇒ 转红
- [x] 3.2 M2 捕获分支改回空实现 ⇒ 转红
- [x] 3.3 M3 链构造顺序改成「后匹配者胜」 ⇒ 转红
- [x] 3.4 M4 去掉输出「缺 `else_` ⇒ 抛异常」 ⇒ 转红
- [x] 3.5 M5 active block 改成单层 ⇒ 转红
- [x] 3.6 M6 **共享组合器**的链构造方向反转 ⇒ **`_if` 侧**「先匹配者胜」转红
      （⚠️ 初稿的「⇒ ADR-050 金标转红」**被实测证伪** ⇒ 状态守卫**互斥** ⇒
      链序对 ADR-050 **不可观测** ⇒ 详见 `design.md` §6.1）

## 4. 验收与归档

- [x] 4.1 ADR-050 的既有金标**逐项不变**
- [x] 4.2 `ctest -LE perf` 零回归；`test_if_stmt` 绿
- [x] 4.3 `run_all_ported_tests.sh` / `run_all_samples.sh` 全绿
- [x] 4.4 全部护栏（含 `--self-test`）exit 0
- [x] 4.5 计数锚点核对（**全部不变**：`CTEST_TOTAL` 202 / `EXAMPLES_TEST_SOURCES` 17 /
      non-perf 191 / `verilator_e2e` 38 ⇒ ⚠️ 初稿的「203 / 18」是**事实错误**，
      实施期已订正）⇒ + samples 注册数 35 → 36 / 禁用 5 → 4
- [x] 4.6 归档（**禁** `--skip-specs` ⇒ capability **ADDED** ⇒ 合并新 capability）
