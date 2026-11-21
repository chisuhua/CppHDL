# Proposal: `examples/spinalhdl-ported/` 8 个产品面 target 迁移到 `.verif` 单一真相源

- **状态**: **实施完成（2026-11-21）** · 待归档
- **架构依据**: `docs/adr/ADR-074-rtl-verification-single-source.md`（`.verif` 单一真相源）+
  `docs/adr/ADR-076-verif-history-primitives.md`（历史/序列原语 —— 本迁移的**框架前置**）
- **用户裁决**（2026-11-21）：**范围 = 8 个已登记产品面 target 全迁**；判据面处置 =
  **先扩展 `.verif` 原语集**（⇒ `verif-history-primitives-2026-11`）
- **推导与实现细则**: `design.md`（同目录）

---

## Why

`examples/spinalhdl-ported/` 的 8 个已登记 target 仍是「手写 C++ `main` + 手写 `.sv` TB +
`emit_rtl*.cpp` 薄壳」形态 ⇒ ⇒ **同一组期望值存在两份手写件**（C++ / Verilog）⇒ ⇒ 两侧
可静默漂移，且 C++ 侧往往**根本没有断言**（旧 `counter` 行的登记项就点过这一点）。

`examples/axi4/` 与 `examples/stream/` 已迁成**三角色分离**：
设计定义 `*_top.h`（无 `main`）+ 激励驱动 `tb_*.cpp`（默认模式读 `.verif` 跑断言；
`--emit-rtl` 发射 RTL/TB）+ 验证源 `tb_*.verif`（**唯一语义来源**）⇒ ⇒ 解释器与渲染 TB
由**同一份 IR** 派生 ⇒ ⇒ 断言不可能漂移。

⇒ ⇒ 本 change = 把该形态铺到**剩余的 8 个**产品面 target。

## What Changes

| target | 设计定义 | 激励驱动 / 验证源 | 断言数 | scenario 数 |
|---|---|---|---|---|
| `tb_pwm_example` | `pwm/pwm_top.h` | `tb_pwm_example.cpp/.verif` | 29 | 5 |
| `tb_counter_simple_example` | `counter/counter_simple_top.h` | `tb_counter_simple_example.cpp/.verif` | 258 | 2 |
| `tb_counter_example` | `counter/counter_top.h` | `tb_counter_example.cpp/.verif` | 9 | 5 |
| `tb_uart_tx_example` | `uart/uart_top.h` | `tb_uart_tx_example.cpp/.verif` | 49 | 3 |
| `tb_uart_rx_example` | `uart_rx/uart_rx_top.h` | `tb_uart_rx_example.cpp/.verif` | 32 | 6 |
| `tb_fifo_stream_example` | `fifo/fifo_top.h` | `tb_fifo_stream_example.cpp/.verif` | 20 | 4 |
| `tb_gpio_example` | `gpio/gpio_top.h` | `tb_gpio_example.cpp/.verif` | 11 | 6 |
| `tb_quadrature_encoder_example` | `quadrature_encoder/quadrature_encoder_top.h` | `tb_quadrature_encoder_example.cpp/.verif` | 15 | 5 |

共性改动（每个 target 一份）：

- 端口**全部具名**（ADR-075 E1）；删除 `<impl>.cpp` 与 `emit_rtl*.cpp`（发射入口 = TB 自身）。
- target 改名 `X_example` → `tb_X_example`（CTest 条目名不变）；同步
  `examples/CMakeLists.txt` / `tests/CMakeLists.txt`。
- 期望源登记（`.rddf/checks/examples-rtl-expectations.md` 的 `VERIF-CHECKS:` +
  `#### scenario:` 双向 1:1）与清单行（`.rddf/checks/examples-rtl-sim-targets.tsv`）。
- 端口命名棘轮登记表同步（未具名端口豁免**只减不增**）。

## 框架前置（`verif-history-primitives-2026-11` 之后，本迁移实施期新增）

| # | 缺陷 | 修法 | 判据载体 |
|---|---|---|---|
| F1 | 渲染侧把**窄端口**直接赋给 `logic [63:0]` ⇒ `WIDTHEXPAND` ⇒ 护栏 ④（禁 `-Wno-fatal`）下 **TB 编译失败** | `u64_rhs()` 零扩展（`EXPECT_STABLE` / `RECORD` 两处） | `tests/test_verif_wait_level.cpp` 新用例 + pwm 产物 |
| F2 | 解释器框架复位只调 `Simulator::reset()`（清 0）⇒ `INIT != 0` 的寄存器起在**错误初态** ⇒ 与 RTL 分叉 | 复位后按 `regimpl::get_init_val()` 回填 | 新用例「寄存器 INIT 在框架复位后生效」+ uart_rx 解码 |

## 已知局限（登记，不修）

1. **`EXPECT` 字面量 MUST 落在端口位宽内**：渲染侧按位宽截断（`8'd0`），解释器按 64 位原值
   比较 ⇒ `EXPECT value 256`（8 位端口）会让**两端结论相反** ⇒ ⇒ 迁移的所有期望值都在范围内。
2. **周期 256 抓不到步长 2**（128 整除 256）⇒ 由 `counter_simple` 的绝对判据承担
   （原手写 TB 同局限 ⇒ 未降级）。
3. **UART TX 的位宽常量 434 成为 impl-contract**：`.verif` 无时间戳 ⇒ 用 434 锚定位窗 +
   每窗首/尾双端断言（判别力不降，但改位宽必须同步改 `.verif`）。

## Acceptance Criteria

- [x] **AC1** 8 个 target 全部变为 `generated=yes`（`.verif` 存在 + TB 含生成器标记 +
      清单列一致）⇒ 护栏 ⑥⁗
- [x] **AC2** 每个 target 的 `RESULT: checks` == 清单 `min_checks` == 期望源 `VERIF-CHECKS`
      == C++ 侧 `interp_checks`（三方等式 + 双侧各自登记）
- [x] **AC3** 期望源锚点 ↔ 验证源 `@scenario` **双向 1:1**（⑥''）· TB 无 TODO 残留（⑥‴）
- [x] **AC4** 生成物**逐字节重现**（⑥' 新鲜度）· 端口表四件套一致（⑧）· 生成物形态（⑧''）
- [x] **AC5** 每条被测性质都有**至少一条变异**使其转红（逐 target 的变异表见期望源登记）；
      **无效变异如实登记**
- [x] **AC6** 判据面**不降级**：原手写 TB 的 metamorphic 相对判据在 `.verif` 中有对应形态
      （counter_simple 周期 256 · counter Δ==enable · pwm 窗口计数 · fifo 序列恒等 ·
      quadrature 往返恒等 · uart_rx 序列恒等 + 单拍脉冲）；方法确实变更的（uart_tx 位宽测量）
      在文档中**显式登记**并说明判别力补偿
- [x] **AC7** `verify_examples_rtl_sim.sh` PASS（17 项）+ `--self-test` PASS +
      `verify_examples_rtl_sim_coverage.sh` PASS
- [x] **AC8** `run_all_ported_tests.sh` 27/27 PASS · `verify_port_naming.sh` PASS（锚点回填）·
      `verify_example_test_wiring.sh` / `verify_example_assertion_quality.sh` PASS
- [x] **AC9** 文档同步：根 `AGENTS.md`（如需）· `examples/spinalhdl-ported/AGENTS.md` ·
      `docs/CHANGELOG.md` · 期望源登记 · 清单 · 棘轮登记表

## 用户可见变更

| 项 | 前 | 后 |
|---|---|---|
| `examples/spinalhdl-ported/` 的产品面 target | 8 个仍是「手写 C++ `main` + 手写 `.sv` + `emit_rtl*.cpp`」 | **8 个全部**三角色分离（`*_top.h` + `tb_*.cpp` + `tb_*.verif`）· 发射入口 = TB 自身的 `--emit-rtl` |
| target 名 | `pwm_example` / `counter_example` / `counter_simple_example` / `uart_tx_example` / `uart_rx_example` / `fifo_stream_example` / `gpio_example` / `quadrature_encoder_example` | `tb_pwm_example` / `tb_counter_example` / `tb_counter_simple_example` / `tb_uart_tx_example` / `tb_uart_rx_example` / `tb_fifo_stream_example` / `tb_gpio_example` / `tb_quadrature_encoder_example`（**CTest 条目名不变**） |
| 顶层端口 | 多为 `top_unnamed_input_*` / `top_unnamed_output` | **全部具名**（`top_<C++ 成员名>`）⇒ 验证源可按名寻址 |
| 期望值 | C++ 侧通常**无断言**；Verilog 侧手写 | C++ 侧与 TB 侧**同源**（同一份 `.verif` ⇒ 同一份 IR）⇒ 断言不可能漂移 |
| 断言条数 | 旧手写 TB 口径（如 pwm 13 / counter 54 / counter_simple 768） | 新口径（pwm 29 / counter 9 / counter_simple 258 / uart_tx 49 / uart_rx 32 / fifo 20 / gpio 11 / quadrature 15）⇒ 每行均登记 `min_checks` == `interp_checks` |
| `EXPECT` 的字面量 | 未声明范围约束 | **MUST 落在端口位宽内**（渲染侧按位宽截断、解释器按 64 位比较 ⇒ 越界值两端结论相反）⇒ 已写入 `docs/usage_guide/04-testbench-guide.md` |
| UART TX 的位宽 | 由 TB 用边沿时间戳「测出来」 | **434 成为 impl-contract**（`.verif` 无时间戳 ⇒ 用 434 锚定位窗 + 首/尾双端断言）⇒ 改位宽 MUST 同步改 `.verif` |
