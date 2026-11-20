# tasks — `stream-demux-rtl-product-2026-11`

> ⚠️ **设计不在本文件**：判据面 = ADR-074（验证单一真相源）· ADR-072（产品端口面 · 四件套）·
> ADR-075（端口具名 E1）· ADR-056（demux ready 方向）· capability `chlib-stream-ready-ownership`。
> ⚠️ **顺序**：批次 A（设计 + 期望）先于批次 B（发射 + TB）⇒ ⇒ 先冻结期望，再产出产物。
> ⚠️ **无 spec 变更** ⇒ 归档用 `openspec archive … --yes --skip-specs`。
> ⚠️ **端口数口径**：**18** = 框架 2（`default_clock` · `default_reset`）+ 根自有 **16**
> （输入流 {payload[7:0], valid, ready} + `select[1:0]` + 4 路输出流 × {payload[7:0], valid, ready}）。

## 批次 A — 设计定义 + 判据面（AC1 · AC3 · AC5 · AC6 · AC7）

| # | 任务 | 落点 | 判据 | 状态 |
|---|---|---|---|---|
| A1 | **顶层组件**（`chlib::stream_demux` 包装 · 16 端口具名 · 输入流先 `as_master()` · ready 链 `<<=` · 消费端 `<<=` 覆盖输出 ready） | `examples/stream/stream_demux_top.h` | AC1 | ✅ |
| A2 | **期望源段**（手推）：module 集 `{top}` · 端口表 **18** · 4 场景 / **38** 条 · `VERIF-CHECKS: 38` · 4 个 `#### scenario:` 锚点 · **越界条不可达如实登记** | `.rddf/checks/examples-rtl-expectations.md` | AC6 | ✅ |
| A3 | **清单行**（`kind=target` · 自发行 · `interp_checks=38` · `min_checks=38` · `generated=yes` · `scenarios=4` · `spec_checks=32`） | `.rddf/checks/examples-rtl-sim-targets.tsv` | AC5 · AC7 | ✅ |
| A4 | **`# REGISTERED_TARGETS` 12 → 13** | 同上 | AC5 | ✅ |
| A5 | **落选表删该行**（`stream_demux_demo`）· 守恒订正 `27 == 13 ∪ 14`（tsv + md 双面） | `.rddf/checks/examples-rtl-sim-exclusions.{tsv,md}` | AC5 | ✅ |

## 批次 B — 发射面 + 独立 TB（AC2 · AC3 · AC4 · AC8）

| # | 任务 | 落点 | 判据 | 状态 |
|---|---|---|---|---|
| B1 | **tb 双模式**（默认 `.verif` 驱动 · `--emit-rtl` 复用库层三件套 · 未知参数 exit 2） | `examples/stream/tb_stream_demux_demo.cpp` | AC2 · AC8 | ✅ |
| B2 | **验证源**（4 场景 / 38 断言 · 期望全手推） | `examples/stream/tb_stream_demux_demo.verif` | AC4 · AC6 | ✅ |
| B3 | **生成独立 TB**（`--emit-hygiene --emit-tb-verif`）⇒ 仓内 `.sv` 逐字节 == 重生成 | `examples/stream/tb_stream_demux_demo.sv` | AC4 | ✅ |
| B4 | **产物实测**：module 集 `{top}` · 顶层端口 **18** ⇒ 与 A2 手推一致 | 发射产物 | AC3 | ✅ |
| B5 | **构建接入**（target 改名 · `CPPHDL_SOURCE_DIR` 注入 · include dir） | `examples/{,stream/}CMakeLists.txt` | AC2 · AC8 | ✅ |

## 批次 C — 判别力 + 文档 + 验收（AC9 · AC10 · AC11）

| # | 任务 | 落点 | 判据 | 状态 |
|---|---|---|---|---|
| C1 | **变异自证 ≥ 3 条**（M1 valid 通路断 · M2 反压方向断 · M3 payload 广播）⇒ 全部转红 · **先确认仍能编译** | 临时产物 + Verilator | AC11 | ✅ |
| C2 | **文档同步**：`examples/stream/AGENTS.md` · 根 `AGENTS.md` 的示例路径 | 同左 | AC9 | ✅ |
| C3 | **护栏**：`verify_examples_rtl_sim.sh` · `verify_examples_rtl_sim_coverage.sh` · `verify_port_naming.sh` · `run_all_ported_tests.sh` · `verify_test_counts.sh` | — | AC9 | ✅ |
| C4 | **遗留清扫**：本次范围内 MUST NOT 有 TODO / 半截实现 / 注释掉的代码 / 临时日志 / 被 skip 的测试 / 旧 target 残留 | 本 change 触及的全部文件 | AC9 | ✅ |
| C5 | **openspec validate --strict** valid | — | AC10 | ✅ |

---

## AC → 任务映射

| AC | 任务 |
|---|---|
| AC1 | A1 |
| AC2 | B1 · B5 |
| AC3 | A2 · B4 |
| AC4 | B2 · B3 |
| AC5 | A3 · A4 · A5 |
| AC6 | A2 |
| AC7 | A3 |
| AC8 | B1 · B5 |
| AC9 | C2 · C3 · C4 |
| AC10 | C5 |
| AC11 | C1 |
