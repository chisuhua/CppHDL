# Verification Report — examples-runtime-coverage-2026-10

> Change: `openspec/changes/examples-runtime-coverage-2026-10/`
> Date: 2026-10-22
> Baseline commit: `248af26`（复审修订后的提案）

## 1. 基线（Task 0）

⚠️ **本提案在起草时断言「落地时很可能又抓到一批既有失败（samples 侧就抓到
2 个 segfault）」—— 该推断在 `examples/` 侧【不成立】。**
2026-10-22 在 pristine HEAD 上实测 `./run_all_ported_tests.sh`：

```
PASS: 28   FAIL: 0   SKIP: 0   exit=0
```

⇒ 基线**全绿** ⇒ 豁免清单**为空** ⇒ 豁免机制落地时**没有现成探针**
⇒ 其验收 MUST 用**人为变异**（见 §3）。

### 1.1 逐例基线清单（28 行，每行含退出码与耗时）

（退出码 0 = 通过；耗时为 wall-clock，含进程启动）


**Phase 1: 基础移植验证**

| example | 退出码 | 耗时 |
|---|---|---|
| `counter_simple_example` | 0 | 0.03s |
| `counter_example` | 0 | 0.03s |
| `fifo_stream_example` | 0 | 0.02s |
| `uart_tx_example` | 0 | 0.06s |
| `uart_rx_example` | 0 | 0.03s |

**Phase 2: 核心组件**

| example | 退出码 | 耗时 |
|---|---|---|
| `pwm_example` | 0 | 0.02s |
| `gpio_example` | 0 | 0.02s |
| `spi_controller_example` | 0 | 0.03s |
| `i2c_controller_example` | 0 | 0.11s |
| `timer_example` | 0 | 0.03s |

**Phase 2: 高级外设**

| example | 退出码 | 耗时 |
|---|---|---|
| `vga_controller_example` | 0 | 0.06s |
| `ws2812_controller_example` | 0 | 0.20s |
| `quadrature_encoder_example` | 0 | 0.03s |
| `sigma_delta_dac_example` | 0 | 0.02s |

**Phase 1-3: 辅助组件**

| example | 退出码 | 耗时 |
|---|---|---|
| `width_adapter_example` | 0 | 0.02s |
| `assert_example` | 0 | 0.02s |
| `phase1c_test` | 0 | 0.04s |

**Phase 3: Stream 组件**

| example | 退出码 | 耗时 |
|---|---|---|
| `stream_mux_demo` | 0 | 0.02s |
| `stream_demux_demo` | 0 | 0.02s |
| `stream_arbiter_demo` | 0 | 0.04s |
| `stream_fork_demo` | 0 | 0.01s |

**Phase 3A: AXI4 总线**

| example | 退出码 | 耗时 |
|---|---|---|
| `axi4_lite_example` | 0 | 0.08s |
| `axi4_full_example` | 0 | 0.00s |
| `axi_soc_example` | 0 | 0.66s |
| `phase3a_complete` | 0 | 0.00s |

**RISC-V Mini**

| example | 退出码 | 耗时 |
|---|---|---|
| `rv32i_phase1_test` | 0 | 0.26s |
| `rv32i_phase2_test` | 0 | 0.21s |
| `rv32i_phase3_test` | 0 | 0.54s |

**合计**：28/28 退出码 0；总耗时 < 1 s（最坏上界仍是 28 × 30 s = 14 min）。

### 1.2 未被跟踪的 23 项（登记在 `examples/disabled_examples.txt`）

| 类别 | 数量 | 归位条件 |
|---|---|---|
| Verilator 后端 demo（`verilator_0{1,2,3}_*`） | 3 | 护栏支持「按需启用需 Verilator 的集合」 |
| `testbench_template.cpp`（未注册 target） | 1 | 改造为可运行 demo ⇒ 注册 + 补进跟踪集合；否则删除 |
| `examples/riscv-mini/` 的 Catch2 测试二进制 | 19 | 补 `add_test` 进 CTest（**不是**进 example 护栏 —— 它们是测试不是 demo） |

⚠️ 第三类是本次审计发现的**最大**运行覆盖缺口：
`grep -rn "add_test" examples/` = 0 ⇒ `ctest -N` 对它们 **0 命中**
⇒ 它们**每次 CI 都在编译、却从未被执行过**。

## 2. 实施内容

| Task | 内容 |
|---|---|
| Task 0 | 基线实测 + 本报告 |
| Task 1 | `run_all_ported_tests.sh` 重写：缺失二进制**失败且不中止**、C2 数量守恒、`timeout`/`gtimeout` 回退、逐例日志、豁免 + stale 检测、单一来源解析 |
| Task 2 | `examples/CMakeLists.txt`（**新建**）：`CH_TRACKED_EXAMPLE_TARGETS` 单一来源 + 21 个子目录迁入 + 聚合 target `all-examples`；`examples/disabled_examples.txt`（**新建**） |
| Task 3 | CI job `examples-runtime`（Linux-only，`timeout-minutes: 30`，含 LLVM + g++-13 工具链，逐例日志 artifact） |
| Task 4 | 文档同步（`AGENTS.md` 4 处 + `openspec/config.yaml` 1 处） |

## 3. 变异验证（3 项全被抓到）

| 变异 | 期望 | 实测 |
|---|---|---|
| **移走一个二进制**（`stream_mux_demo`） | ① 记为 FAILED（**不是** SKIP）② **其余 27 个仍被执行** ③ exit ≠ 0 ④ C2 报 MISMATCH | ✅ `FAILED (binary not found)` + **27 个 PASS 全部执行** + `exit=1` + `MISMATCH: 跟踪 28，实际执行 27` |
| **新增一个未跟踪的二进制**（模拟「新 example 注册进 CMake 却忘了加进护栏」） | C1 报 UNTRACKED + exit ≠ 0 | ✅ `UNTRACKED  brand_new_example` + `C1-registration-consistency` + `exit=1` |
| **缺失时不中止整轮**（本次修复的核心） | 旧行为：`set -e` + `return 2` ⇒ 脚本在第一个缺失处**中止**（exit=2、报告被截断、汇总走不到） | ✅ 新行为：缺失分支与失败分支**同构**（记 FAIL + `continue`）⇒ 整轮跑完 + 汇总完整 |

⚠️ 第 3 项是本次修复的**根因**。旧脚本 `run_all_ported_tests.sh:14` 的
`set -euo pipefail` 遇上 `run_test()` 在 `:53` 的 `return 2` ⇒ **脚本直接
中止** —— 那比 SKIP 更坏（SKIP 至少会出现在汇总里）。

## 4. 遗留（已登记，非本 change 范围）

- **19 个 Catch2 测试二进制零运行覆盖** ⇒ 归位条件已写进
  `examples/disabled_examples.txt`（「补 `add_test` 进 CTest」），
  但**补 `add_test` 本身**会改变 `CTEST_TOTAL` ⇒ 属独立立项。
- **Verilator demo 未进护栏** ⇒ 需要「按需启用需 Verilator 的集合」的机制。
