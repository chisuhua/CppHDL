# 提案：`io().X = value` 落在 `ch_in` 上 ⇒ 修掉存量静默失效

- **架构依据**：无（bugfix；不改框架、不改契约）
- **5 段草案**：`.rddf/improvements/input-port-assignment-antipattern-fix-2026-10.md`
- **roadmap 归属**：工程纪律项（`AGENTS.md §ANTI-PATTERNS` 的「新增组件输入端口 ⇒ 全部实例化点 MUST 显式驱动」）
- **规格影响**：**无** ⇒ 归档用 `--skip-specs`
- **规模 / 风险**：**低**（4 处赋值改写 + 文档）；⚠️ **不新增** ctest 条目 ⇒ 计数锚点不变

## Why

`ch_in` **无** `operator=`（`include/core/io.h:236-240`）⇒ `io().X = value` 只走
**`CHERROR("Cannot assign value to input port!")` + 无任何赋值动作**
⇒ **端口保持未驱动** ⇒ 而**编译通过 / 运行不崩 / exit 0** ⇒ 守护脚本判 PASS ⇒ ⇒ **静默失效**。

⚠️ **实测存量 10 处**：

| 位置 | 处数 | 可动？ |
|---|---|---|
| `examples/spinalhdl-ported/uart/uart_tx.cpp` | **4** | ✅ 已跟踪（受 `run_all_ported_tests.sh` 守护） |
| `samples/stream_component_example.cpp` | **4** | ❌ 在 `disabled_samples.txt`（**连编译都不过**） |
| `samples/stream_handshake_example.cpp` | **2** | ❌ 同上 |

⇒ **本 change 只修可动的 4 处**；另 6 处被 **ADR 级 API 变更**挡住（见 `## Non-Goals`）。

⚠️ **实测后果**：改动前每次运行打 **4 条** `[ERROR] Cannot assign value to input port!`，
而程序仍 exit 0 ⇒ 完整取证见 `design.md` §4。

## What Changes

### 范围内（本 change）

把 `examples/spinalhdl-ported/uart/uart_tx.cpp` 的 **4** 处
`device.instance().io().X = value` 改成**合法的 TB 驱动**：

```cpp
simulator.set_input_value(device.instance().io().frame_start, 0);   // false
simulator.set_input_value(device.instance().io().data, 85);        // 0x55
```

⚠️ **值侧 MUST 用纯整数**：`ch_bool` 的 `operator uint64_t` 是 **`explicit`**
（`include/core/bool.h`）⇒ `set_input_value(port, ch_bool(false))` **编译不过**
⇒ ⇒ 与仓内既有写法一致（`tests/test_axi4_full_burst.cpp:187`
`sim.set_input_value(io.rst_n, 1);`）。

⚠️ **不可用 `<<=`**：`<<=` 走 `inputimpl::set_driver` ⇒ 那是**内部驱动**
（组件间接线），不是 TB 驱动 ⇒ 对**顶层**端口用 `<<=` 会把它变成「有内部驱动」
⇒ ⇒ 反而**掩盖**「顶层端口未被 TB 驱动」这件事 ⇒ ⇒ MUST 用 `set_input_value`。

### 范围外（**不在**本 change）

- **6 处**（`samples/stream_component_example.cpp` 4 处 +
  `samples/stream_handshake_example.cpp` 2 处）⇒ 这两个 sample 在
  `samples/disabled_samples.txt` 里、**当前连编译都不过**
  ⇒ 其归位条件依赖「`chlib::stream_fifo` 读使能外部化」这一
  **公开 API 变更（需 ADR）**
  ⇒ ⇒ 它们的整体结构届时会重写 ⇒ ⇒ 现在改这 6 处**没有意义**
  ⇒ ⇒ **登记为独立事项**（见下），**不**在本 change 内硬改。
- **不**新增通用护栏 ⇒ 理由见 improvement 草案 §1（静态方案的成本/误报面）；
  ⇒ 但「加一个窄口径静态护栏」已**独立立项**（见下）。

### 顺带登记（**不在**本 change）

- **窄口径静态护栏**（`io().X = value` 落在 `ch_in` 上 ⇒ 报违规）
  ⇒ 独立 improvement 草案（因为它需要一份 capability 契约 + CI 挂载点）
- **端口身份问题**（`include/` 的 641 个 `ch_in` 声明**零**个带显式名字 ⇒
  运行期**无法识别端口**）⇒ ADR 级 ⇒ 已由 `input-port-driving-guard-2026-10`
  的 spike 登记

## Acceptance

- [ ] `uart_tx.cpp` 的 **4** 处**全部**改成 `set_input_value`（`grep` 归零）
- [ ] ⚠️ **收益的可观测判据（实施期订正）**：stderr 里
      `[ERROR] Cannot assign value to input port!` **由 4 条降为 0 条**
      （`HEAD` 版 vs 改动后版本，同一编译命令的 A/B 对照）
      —— ⚠️ **初稿的 AC「输出必须变化（应出现 UART 帧）」已被实测证伪**：
      该 example 的输出**不会**变化，因为它的**状态机本身从未工作过**
      （4 个状态的 `if (ch_bool)` 在 `describe()` 期恒假 ⇒ 恒走 else ⇒
      状态寄存器自环、输出是两个常量）⇒ 详见 `design.md` §4.2
      ⇒ ⇒ 该缺陷**独立立项**（`uart-tx-fsm-live-logic-2026-10`），**不在**本 change 内修
- [ ] `uart_tx.cpp` 剩下 6 处 `io().X =` 全在 `samples/` 且那 2 个 sample **不可编译**
      ⇒ **不在**本 change（见 `## Non-Goals`）
- [ ] `./run_all_ported_tests.sh` **28/28** 全绿（`uart_tx_example` 仍在跟踪集合内）
- [ ] `bash scripts/run_all_samples.sh` **35/35** 全绿（**零**样本改动 ⇒ 须仍全绿）
- [ ] `ctest -LE perf` **191/191**；**计数锚点不变**
      （`CTEST_TOTAL` 202 / `EXAMPLES_TEST_SOURCES` 17 / `verilator_e2e` 38）
- [ ] **无代码警告**（`-Wall -Wextra -Wpedantic`）
- [ ] 根 `AGENTS.md` 的 NOTES 记录本次修复 + **6 处仍待 ADR** 的事实

## Capabilities

- **不涉及**：本 change **不改任何契约**（修的是 example 的实现 bug，
  且不改变任何 example 的**可观察契约**——它此前是**空洞**的）
  ⇒ 归档用 `--skip-specs`
- ⚠️ `example-runtime-coverage` / `sample-regression-guard` 的 Requirement
  **逐字核对后**判定**无需 MODIFIED**（前者只管「被跟踪的 example 纳入运行护栏」，
  后者只管「sample 纳入回归护栏 + 禁用清单可收缩」⇒ 本 change 两者都不改）

## Impact

- **改动文件**：`examples/spinalhdl-ported/uart/uart_tx.cpp`（4 处）
- **计数**：**不变**（不新增/删除 ctest 条目；example 的**跟踪集合**不变）
- **爆炸半径**：**零**（example 代码，无库/框架改动 ⇒ 不影响任何测试）
- **风险**：**低** ⇒ 但**必须**有 before/after 的输出对比（否则「改对了」无证据）

## Non-Goals

- **不**改 `samples/` 的 6 处（等 ADR）
- **不**新增静态护栏（独立立项）
- **不**动 `ch_in` / `port` 的 API（那会让 `=` 变成合法 ⇒ 等于取消这条反模式
  ⇒ ⇒ 是**契约变更** ⇒ 需另议）
- **不**给 `ch_in` 加 `operator=` 的编译期诊断（框架层改动 ⇒ 独立立项）
