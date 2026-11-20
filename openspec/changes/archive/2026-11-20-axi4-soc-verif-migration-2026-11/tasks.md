# Tasks — SoC 迁 `.verif`（R13 闭口）

> 依据 = ADR-074 §7.6（AC-R13′-1 / AC-R13′-2）+ §7.7 的实施拆分（第 2 个 change）。
> ⚠️ **原子性**：批次 B 的「验证源 + example + 生成物 + tsv 四件套」MUST 落在**同一 commit**
> —— 任何中间态都会让 `verify_verif_migration_state.sh` 判「半迁」（那是它存在的目的）。

## 批次 0 — 前置核对（不改产品面）

| # | 任务 | 落点 | 判据 | 状态 |
|---|---|---|---|---|
| 0a | **逐条抄录**旧断言清单：手写 TB 的 14 条（起始/8 数据/停止/两场景值/复位 2 条）+ C++ 侧 2 条 ⇒ ⇒ 作为映射表的左列 | 本文件 §映射表 | AC5 | ☑（旧 14 + 旧 C++ 2 已逐条抄入 §映射表） |
| 0b | **手推**新断言数 N 与每条的拍数偏移（bit 周期 16 拍 ⇒ 位中点 = 沿 + 8 + 16k）⇒ ⇒ 与期望源 §场景③ 的推导**逐字对齐** | `.rddf/checks/soc-rtl-expectations.md#soc-example` | AC9 | ☑（手推 N=15 = 1+1+11+2） |
| 0c | 核对 D3′ 合法性：`WAIT uart_tx low` 的**入口电平**（空闲高 ⇒ low 未成立）⇒ 合法；`WAIT uart_tx high` **禁** | 同上 | AC2 | ☑（入口空闲高 ⇒ `low` 未成立 ⇒ 合法；`high` 禁） |

## 批次 A — 验证源

| # | 任务 | 落点 | 判据 | 状态 |
|---|---|---|---|---|
| A1 | 写 `tb_axi_soc_example.verif`：4 个 scenario（`bringup_and_gpio_write` / `gpio_read_data_in` / `uart_tx_frame_8n1` / `low_active_reset_clears_gpio`）· 只用封闭原语集 | `examples/axi4/tb_axi_soc_example.verif`（新） | AC1 · AC2 | ☑（4 scenario · 只用封闭原语集） |
| A2 | 帧场景：`WAIT uart_tx low 240` → `TICK 8` + `EXPECT uart_tx 0`（起始位）→ 8×( `TICK 16` + `EXPECT` 数据位 k（LSB 先出）) → `TICK 16` + `EXPECT uart_tx 1`（停止位） | 同上 | AC1 | ☑（`WAIT uart_tx low 240` → TICK 8 → 8×(TICK 16 + EXPECT) → TICK 16 停止位） |
| A3 | 期望源补齐：`VERIF-CHECKS: N`（**人写**）+ 4 个 `#### scenario:` 锚点 + scenario→判据出处表 | `.rddf/checks/soc-rtl-expectations.md#soc-example` | AC9 | ☑（`VERIF-CHECKS: 15` + 4 锚点） |

## 批次 B — 原子翻转（四件套同一 commit）

| # | 任务 | 落点 | 判据 | 状态 |
|---|---|---|---|---|
| B1 | `tb_axi_soc_example.cpp` 默认模式改调 `rtl_emit::run_verif_file(...)`（删掉两段手写断言 + `done` 轮询）· `--emit-rtl` / `--check-products` 路径**不动** | `examples/axi4/tb_axi_soc_example.cpp` | AC6 | ☑（RESULT 2 → 15 · `--emit-rtl` / `--check-products` 不变） |
| B2 | 重新生成 `tb_axi_soc_example.sv`（替换手写版）⇒ ⇒ 产物与验证源**逐字节一致** | `examples/axi4/tb_axi_soc_example.sv` | AC3 | ☑（196 行 · ⑥' 逐字节重现 ✓） |
| B3 | tsv SoC 行翻转：`generated=no→yes` · `verif_path` · `interp_checks 2→N` · `min_checks 14→N` · `status` · 备注列改述（删「手写 TB」的旧理由，写「生成式 · 帧起点由 `WAIT low` 锚定」） | `.rddf/checks/examples-rtl-sim-targets.tsv` | AC4 | ☑（generated=yes · interp 2→15 · min 14→15 · 备注改述） |
| B4 | 映射表落盘（旧 14 + 旧 C++ 2 → 新 N），每条标注去向 | 本文件 §映射表 | AC5 | ☑（见 §映射表） |

## 批次 C — 判别力与验收

| # | 任务 | 落点 | 判据 | 状态 |
|---|---|---|---|---|
| C1 | **变异 1**：改 `.verif` 某数据位期望（`0x5A` 的 bit1：`1`→`0`）⇒ 至少一条转红 ⇒ 还原 | 临时 | AC10 | ☑（`#3` 转红（bit1 期望 1→0）） |
| C2 | **变异 2（判别力 · 两条）**：① `WAIT uart_tx low 240` 的 `max` → `0` ⇒ `#0` 必红（证明**至少推进 1 拍**、不是 no-op）② `WAIT` **前**插 `TICK 8` ⇒ 锚点偏移 ⇒ 数据位判据必红（证明锚定决定后续偏移）⇒ 均还原。⚠️ 实测：`max` → **1** **不**转红（帧起点 1 拍内即出现）⇒ 不作判据 | 临时 | AC11 | ☑（max=0 ⇒ `#0` 红；前插 TICK 8 ⇒ 4 条数据位红；max=1 **不**红（已登记）） |
| C3 | 护栏：`verify_examples_rtl_sim.sh`（全量）· `--self-test` · `verify_verif_migration_state.sh`（两态 + `--require-closed`） | shell | AC7 · AC8 | ☑（全量 13 项 PASS · self-test 21 投毒 + 2 必绿 PASS · `--require-closed` 态 B · 两侧 15） |
| C4 | 全量回归：`ctest` · `verify_test_counts.sh` · 遗留清扫（TODO / 注释代码 / 临时日志 / 未用 include） | shell | AC12 | ☑（ctest 全绿；`perf_regression` 的并行调度竞态已单独取证：`-L perf -j1` 顺序跑 11/11 PASS） |
| C5 | 归档：`openspec archive axi4-soc-verif-migration-2026-11 --yes --skip-specs` ⇒ 主 spec 零变更 · `verify_public_api_visibility.sh` PASS | shell | AC13 | ☑（归档为 `2026-11-20-axi4-soc-verif-migration-2026-11` · `openspec/specs/**` sha256 前后相同 `b9ce9aca…` · `verify_openspec_delta`/`verify_public_api_visibility` PASS） |

---

## AC → 任务映射

| AC | 任务 |
|---|---|
| AC1 | 0c · A1 · A2 |
| AC2 | 0c · A1 |
| AC3 | B2 |
| AC4 | B3 |
| AC5 | 0a · B4 |
| AC6 | B1 |
| AC7 | C3 |
| AC8 | C3 |
| AC9 | 0b · A3 |
| AC10 | C1 |
| AC11 | C2 |
| AC12 | C4 |
| AC13 | C5 |

---

## 映射表（旧 → 新）· 由 0a 抄录、B4 定稿

**旧手写 TB（14 条）**

| # | 旧断言 | 新载体 |
|---|---|---|
| 1 | `expect_eq8(gpio_out, 0xA5)`（场景①） | `bringup_and_gpio_write` 的 `EXPECT gpio_out 0xA5` |
| 2 | `expect_eq32(rdata, 0x5A)`（场景②） | `gpio_read_data_in` 的 `EXPECT rdata 0x5A` |
| 3 | `expect_bit(tx_samples[first_low+8], 0)`（起始位） | `uart_tx_frame_8n1` 的 `WAIT uart_tx low` + `TICK 8` + `EXPECT uart_tx 0` |
| 4–11 | 8 个数据位 `expect_bit(tx_samples[first_low+8+16*(k+1)], exp_bits[k])` | 8 × (`TICK 16` + `EXPECT uart_tx <bit_k>`) |
| 12 | `expect_bit(tx_samples[first_low+8+16*9], 1)`（停止位） | `TICK 16` + `EXPECT uart_tx 1` |
| 13 | `expect_eq8(gpio_out, 0x00)`（场景④） | `low_active_reset_clears_gpio` 的 `EXPECT gpio_out 0` |
| 14 | `expect_bit(uart_tx, 1)`（场景④ 空闲高） | `low_active_reset_clears_gpio` 的 `EXPECT uart_tx 1` |

**旧 C++ 侧（2 条）**

| # | 旧断言 | 去向 |
|---|---|---|
| 1 | `done==true && gpio_out==0xAB` | 值侧 → `bringup_and_gpio_write` 的 `EXPECT gpio_out 0xA5`（**值收敛**：旧两侧 0xAB/0xA5 分叉 ⇒ 现统一 0xA5）；`done` 侧 → **移除**：`done` 是**粘滞**信号（首笔后恒 1 ⇒ 上升沿不可判，期望源已登记）⇒ 值正确已蕴含事务完成 |
| 2 | `done==true && rdata==0x5A` | 值侧 → `gpio_read_data_in` 的 `EXPECT rdata 0x5A`；`done` 侧同上去向 |

⇒ ⇒ **新增**（旧侧无对应）：`WAIT uart_tx low 240` 自身那条断言（帧起点必须在 240 拍内出现）
—— 旧版对应物是「窗口里没有起始位 ⇒ 记 1 红」的兜底分支（同一语义，现由原语承载）。

---

## 验收取证表

| 任务 | 命令 | 期望 |
|---|---|---|
| A1 | `grep -c "^@scenario" examples/axi4/tb_axi_soc_example.verif` | 4 |
| A2 | `grep -c "WAIT uart_tx low" …` / `grep -c "WAIT uart_tx high" …` | ≥1 / **0** |
| A3 | `grep -c "^VERIF-CHECKS: " .rddf/checks/soc-rtl-expectations.md` | 含 SoC 段（人写） |
| B1 | `grep -c "run_verif_file" examples/axi4/tb_axi_soc_example.cpp` | ≥1 |
| B3 | awk 取 tsv SoC 行的 `generated` / `interp_checks` / `min_checks` | `yes` / 相等 |
| C3 | `bash scripts/verify_examples_rtl_sim.sh` | PASS（13 项 + SoC 新路径） |
| C3 | `bash scripts/verify_verif_migration_state.sh --require-closed` | PASS（态 B） |
| C4 | `ctest --test-dir build --output-on-failure` | 全绿 |
