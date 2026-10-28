---
name: input-port-assignment-antipattern-fix-2026-10
priority: P2
source: input-port-driving-guard-2026-10（spike 顺手发现的存量缺陷）
phase: phase-5
category: testing
type: bugfix
created_at: 2026-10-28
---

# Improvement: `io().X = value` 落在 `ch_in` 上 ⇒ 10 处**静默失效**的存量缺陷

- **Date drafted**: 2026-10-28
- **Drafted by**: build-agent (builder role)
- **Status**: 📋 5-segment draft，**独立可做**（**不需要**任何通用护栏）
- **来源**：`input-port-driving-guard-2026-10` 的 spike 实测
  ⇒ 交付物 = `openspec/changes/archive/2026-10-28-input-port-driving-guard-2026-10/design.md`
  §3.3

## 1. Why (背景与动机)

`ch_in` **没有** `operator=`（`include/core/io.h` 的 `port<..., input_direction>`
只对 `ch_out` 提供 `operator=`）⇒ 对 `ch_in` 赋值会走
**`CHERROR("Cannot assign value to input port!")` + 不做任何事**
⇒ ⇒ **端口保持未驱动** ⇒ ⇒ **静默失效**：编译通过、仿真不报错、但那个输入
**根本没被驱动** ⇒ 测试可能「**碰巧**通过」。

⚠️ **实测的 10 处存量缺陷**（`examples/` 4 + `samples/` 6；**`tests/` 0 处**）：

| # | 位置 | 形态 |
|---|---|---|
| 1–4 | `examples/spinalhdl-ported/uart/uart_tx.cpp:193,194,203,205` | `device.instance().io().frame_start = ch_bool(false);` 等 |
| 5–8 | `samples/stream_component_example.cpp:78,79,80,119` | `fifo_comp.io().clk = clk;` / `.rst` / `.input_stream` |
| 9–10 | `samples/stream_handshake_example.cpp:92,131` | `fifo_comp.io().input_stream = input_stream;` |

⇒ ⇒ 这正是「新增输入端口 MUST 显式驱动」要抓的东西 ⇒ ⇒ **但它们不需要通用护栏**
⇒ ⇒ **直接修**即可 ⇒ ⇒ 与「事件驱动增量检查」「必需端口注册表」**解耦**。

⚠️ **注意**：`ch_out` 上的 `io().X = value`（仓内 **235** 处）是**能工作**的
（`outputimpl::set_src(0, src)`）⇒ ⇒ **不能**用「有 `io().X =` 就违规」的平规则
⇒ ⇒ 修的时候**只**碰那 10 处（**逐处核实端口方向**）⇒
⇒ ⚠️ **正则无法区分 `ch_in` / `ch_out`**（`io().rdata = ...` 落在 `ch_out` 上合法）
⇒ ⇒ 必须**逐处人工核实**（本 spike 已核实并留痕）。

## 2. What Changes (变更范围)

1. 把那 **10** 处 `io().X = value`（落在 `ch_in` 上）改成**合法**的驱动形态：
   - 若 DUT 的该端口是**顶层**输入 ⇒ 用 `sim.set_input_value(dev.instance().io().X, v)`；
   - 若该 `ch_in` 是**组件间**连线（wrapper 接线）⇒ 用 `sub.io().X <<= value;`
     （⇒ `inputimpl::set_driver` ⇒ 内部驱动 ⇒ **不是** TB 驱动）
   ⇒ ⇒ **逐处判定**（不能一刀切）⇒ 这 10 处的语义**各不相同**（见 §5 的逐处要求）；
2. **零断言变更**（`tests/` 0 处 ⇒ 不涉及测试）；
3. ⚠️ `samples/` 与 `examples/` 的这些程序**由 `scripts/run_all_samples.sh` 与
   `run_all_ported_tests.sh` 守护** ⇒ 改完 MUST 跑这两个护栏 ⇒
   ⚠️ **注意**：其中 3 个 sample 登记在 `samples/disabled_samples.txt`
   （`stream_component_example` / `stream_handshake_example` 属「需框架层新能力」类）
   ⇒ ⇒ 改它们**可能**需要先解决那个缺口 ⇒ ⇒ **逐处判定时先查禁用清单的归位条件**。

## 3. Acceptance (验收标准)

- [ ] 那 **10** 处**逐处**改成合法驱动形态（**不得**一刀切）
- [ ] 每处改动的**语义等价性**有证据（改前/改后该输入的值序列相同）
- [ ] `bash scripts/run_all_samples.sh` 全绿（**35** 个已注册 sample）
- [ ] `./run_all_ported_tests.sh` 全绿（**28** 个跟踪 example）
- [ ] 若某处**依赖框架层缺口**（如禁用 sample 的归位条件）⇒ 登记为**独立**事项，
      **不**在本 change 内硬改
- [ ] `ctest -LE perf` 零回归（**191/191**）；**计数锚点不变**
      （`CTEST_TOTAL` 202 / `EXAMPLES_TEST_SOURCES` 17）

## 4. Capabilities (涉及的能力维度)

- `example-runtime-coverage` —— 可能相关（改的是 `examples/` 与 `samples/` 的程序）
  ⇒ **待实施时判定**
- `sample-regression-guard` —— 可能相关（`samples/` 的运行护栏）
  ⇒ **待实施时判定**
- ⚠️ **不涉及** `dual-backend-test-coverage`（那管 `tests/` 的双后端覆盖）

## 5. Impact (影响面分析)

- **改动文件**：`examples/spinalhdl-ported/uart/uart_tx.cpp`（4 处）·
  `samples/stream_component_example.cpp`（4 处）·
  `samples/stream_handshake_example.cpp`（2 处）
- **计数**：**不变**
- **风险**：**低**（examples/samples 代码，无测试依赖）⇒ 但**逐处语义判定**
  是主要工作量（不是机械替换）
- **与其它 change 的关系**：**独立** ⇒ 不被「事件驱动增量检查」/
  「必需端口注册表」阻塞 ⇒ ⇒ 可**立即**做
