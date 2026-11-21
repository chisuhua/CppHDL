# tasks — `spinalhdl-ported-verif-migration-2026-11`

> ⚠️ **无 spec 变更** ⇒ `.openspec.yaml` 已声明 `skip_specs: true` ⇒ 归档用
> `openspec archive spinalhdl-ported-verif-migration-2026-11 --yes --skip-specs`。

## 批次 A — 逐 target 迁移（每个 = 设计头 + 驱动 + 验证源 + 生成物 + 登记）

| # | target | 判据（AC） | 状态 |
|---|---|---|---|
| A1 | `tb_pwm_example`（29 断言 / 5 scenario） | AC1–AC5 | ☑ |
| A2 | `tb_counter_simple_example`（258 / 2） | AC1–AC5 | ☑ |
| A3 | `tb_counter_example`（9 / 5） | AC1–AC5 | ☑ |
| A4 | `tb_uart_tx_example`（49 / 3）· 位宽方法变更登记 | AC1–AC6 | ☑ |
| A5 | `tb_uart_rx_example`（32 / 6）· 采样相位夹逼 | AC1–AC6 | ☑ |
| A6 | `tb_fifo_stream_example`（20 / 4）· D5 回归 | AC1–AC5 | ☑ |
| A7 | `tb_gpio_example`（11 / 6）· spec_checks 保持 0 | AC1–AC5 | ☑ |
| A8 | `tb_quadrature_encoder_example`（15 / 5）· 往返恒等 | AC1–AC5 | ☑ |

## 批次 B — 框架前置修复（实施期发现）

| # | 任务 | 落点 | 判据 | 状态 |
|---|---|---|---|---|
| B1 | **F1 `WIDTHEXPAND`**：`u64_rhs()` 零扩展（`EXPECT_STABLE` / `RECORD` 两处） | `src/rtl_emit/verif_render.cpp` | AC2 · AC4 | ☑ |
| B2 | F1 的判据载体：`EXPECT_STABLE` 两端同结论用例 + 两处 harness **删 `-Wno-fatal`** | `tests/test_verif_wait_level.cpp` · `tests/test_verif_history_primitives.cpp` | AC5 | ☑ |
| B3 | **F2 寄存器 INIT**：框架复位后按 `get_init_val()` 回填 | `src/rtl_emit/verif_run.cpp` | AC2 · AC5 | ☑ |
| B4 | F2 的判据载体：用例「寄存器 INIT 在框架复位后生效（不是清零）」 | `tests/test_verif_wait_level.cpp` | AC5 | ☑ |

## 批次 C — 验收与归档

| # | 任务 | 落点 | 判据 | 状态 |
|---|---|---|---|---|
| C1 | 护栏：`verify_examples_rtl_sim.sh`（17 项）+ `--self-test` + `_coverage.sh` | — | AC7 | ☑ |
| C2 | 运行：`run_all_ported_tests.sh` 27/27 · `ctest -LE perf` | — | AC8 | ☑ |
| C3 | 登记表：期望源 + 清单 + 端口命名棘轮（`1311`）+ `tests/CMakeLists.txt` 的 fifo 指向修正 | 见 proposal | AC1–AC4 · AC9 | ☑ |
| C4 | 文档：`examples/spinalhdl-ported/AGENTS.md` · `docs/CHANGELOG.md` · `docs/usage_guide/04-testbench-guide.md`（如需） | — | AC9 | ☑ |
| C5 | **归档**：`openspec archive … --yes --skip-specs` ⇒ 主 spec 零变更 | — | AC9 | ☐ |
