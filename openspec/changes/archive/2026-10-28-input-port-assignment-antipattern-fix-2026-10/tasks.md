# Tasks: input-port-assignment-antipattern-fix-2026-10

## 0. 准备
- [x] 0.1 确认框架的**正确驱动形态**：`set_input_value` 的重载集合 ——
      实测 `include/simulator.h` 有 **3** 个：`:417`（`ch_in<T>` + `uint64_t`）/
      `:424`（`ch_uint<N>` + `uint64_t`）/
      `:468`（`ch_in<T>` + `ch literal`，受 `is_ch_literal_v` 约束）
      ⇒ ⚠️ `ch_bool` 的 `operator uint64_t()` 是 **`explicit`**（`include/core/bool.h:47`）
      ⇒ 值侧 MUST 用**纯整数**（仓内既有样例：`tests/test_axi4_full_burst.cpp:187`）
- [x] 0.2 确认**不能**用 `<<=` —— 实测 `<<=` 走 `inputimpl::set_driver` ⇒ 建 **src 边**
      ⇒ 该端口就不再是「顶层端口」（判据 `type_input && driver() == nullptr`，
      `src/codegen_verilog.cpp:350-357`）⇒ 等于把「TB 该驱动它」这件事**删掉**
      ⇒ 是**反向**修法
- [x] 0.3 记录**改动前**的行为 —— 实测用 `git show HEAD:<file>` 取出旧源，
      用**同一份** flags/link 命令编成 `/tmp/opencode/uart_tx_old`：
      ⇒ stderr **4 条** `[ERROR] Cannot assign value to input port!`；
      stdout `uart_tx=1, busy=0` 恒定；exit 0

## 1. 实施（**4** 处，`examples/spinalhdl-ported/uart/uart_tx.cpp`）
- [x] 1.1 `frame_start = ch_bool(false)` ⇒ `set_input_value(..., 0)`
- [x] 1.2 `data = ch_uint<8>(85_d)` ⇒ `set_input_value(..., 85)`
- [x] 1.3 `frame_start = ch_bool(true)` ⇒ `set_input_value(..., 1)`
- [x] 1.4 `frame_start = ch_bool(false)` ⇒ `set_input_value(..., 0)`
- [x] 1.5 **重新构建** ⇒ 确认 `error:` = 0；`warning:` 只剩
      `include/utils/logger.h:194 nonnull-compare`（**既有噪声**：强制重编**未改动**的
      `test_module` 同样出现 3 处 ⇒ 与本 change 无关）

## 2. 验收
- [x] 2.1 `grep` 归零（**方向感知**扫描）⇒ 实测全仓仅剩 `samples/` 的 **6** 处
      （`stream_component_example.cpp` 4 + `stream_handshake_example.cpp` 2，
      二者在 `disabled_samples.txt` 里 ⇒ **不可编译** ⇒ 见 §3.2）
- [x] 2.2 ⚠️ **A/B 输出对比** ⇒ 实测：`Cannot assign value to input port!`
      **4 条 → 0 条** ✓
      ⚠️ **但 stdout 不变**（`uart_tx` 恒 1 / `busy` 恒 0）⇒ **不是**本 change 没生效，
      而是**另一个**缺陷（见 2.2.1）⇒ 初稿 AC「应出现 UART 帧」已**被证伪并订正**
- [x] 2.2.1 ⚠️ **实测挖出的更深缺陷**（本 change **不修**，独立立项）：
      该 example 的**状态机从未工作过** —— 生成的 `uart_tx.v` 里：
      `uart_tx` = 常量 `1'b1`、`busy` = 常量 `1'b0`、
      状态寄存器 `reg_3 <= reg_3`（**自环**）、
      `is_in(状态)` 的 4 个比较节点（`eq_eq_4..7`）**零消费者**（死节点）。
      根因 = `ch_bool`/`ch_uint` 的 `operator uint64_t()` 对**非 const 节点恒返 0**
      （`src/lnode/bool.cpp:44-49`）⇒ `if (ch_bool)` 在建图期恒假 ⇒
      **无 else 的 `if` 分支体永不发射、有 else 的只发射 else**。
      ⇒ 而 4 个状态的 `on_active` **全部执行**、且都无条件写 `io().uart_tx`/`busy`
      ⇒ `ch_out::operator=` 是 `set_src(0,…)`（覆盖）⇒ **最后发射的 STOP 获胜**
      ⇒ 与实测「`uart_tx=1`/`busy=0`」**逐字吻合**。
      ⇒ 框架 `include/chlib/state_machine.h:34-38` **已文档化**这只坑（应使用
      `transition_when`）⇒ ⇒ 已立项 `uart-tx-fsm-live-logic-2026-10`
      ⇒ 详见 `design.md` §4.2
- [x] 2.3 `./run_all_ported_tests.sh` ⇒ **28/28** 全绿
- [x] 2.4 `bash scripts/run_all_samples.sh` ⇒ **35/35** 全绿（**零** sample 改动）
- [x] 2.5 `ctest -LE perf` ⇒ **191/191**
- [x] 2.6 **计数锚点不变**：`CTEST_TOTAL` 202 / `EXAMPLES_TEST_SOURCES` 17 /
      `ctest -L verilator_e2e` 38 / non-perf 191
- [x] 2.7 9 个 `scripts/verify_*.sh` + 3 个 `--self-test` 全 exit 0

## 3. 文档与归档
- [x] 3.1 根 `AGENTS.md` NOTES 记录本次修复 + **6 处仍待 ADR** + **FSM 缺陷的证据** （见下）
- [x] 3.2 ⚠️ **登记「窄口径静态护栏」与「FSM 修复」的独立立项**（不属本 change）
      ⇒ FSM 修复 = `.rddf/improvements/uart-tx-fsm-live-logic-2026-10.md`
      ⇒ 窄口径护栏 = 已由 `input-port-driving-guard-2026-10` 的 spike 判定
        「通用状态型护栏关闭」⇒ 本 change **不**新增护栏（见 `design.md` §6）
- [x] 3.3 `openspec archive input-port-assignment-antipattern-fix-2026-10 --yes --skip-specs`

## 4. ⚠️ 顺带登记的**契约边界缺口**（planner 议题，build 不擅改）
- [x] 4.1 `uart_tx_example` 是 **main() 型**程序（非 Catch2 测试）⇒
      `run_all_ported_tests.sh` 只查**退出码** ⇒ 这个「输出是常量」的 example **判 PASS**；
      而 `example-assertion-quality` capability 管的是「**examples 内的测试** MUST
      断言语义输出」⇒ 其适用范围**不含** main() 型 example
      ⇒ ⇒ **该类程序的语义输出无人校验**
      ⇒ ⇒ 属 `example-assertion-quality` 的**边界问题** ⇒ **planner 议题**
      （build **不得**自行扩 capability 范围）⇒ 已在 `design.md` §7 + NOTES 留证
