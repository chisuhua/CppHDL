# tasks — `stream-mux-rtl-product-2026-11`

> ⚠️ **设计不在本文件**：判据面 = ADR-074（验证单一真相源）· ADR-072（产品端口面 · 四件套）·
> ADR-075（端口具名 E1）· capability `chlib-stream-ready-ownership`。
> ⚠️ **顺序**：批次 A（设计 + 期望）先于批次 B（发射 + TB）⇒ ⇒ 先冻结期望，再产出产物。
> ⚠️ **无 spec 变更** ⇒ 归档用 `openspec archive … --yes --skip-specs`。
> ⚠️ **端口数口径**：**18** = 框架 2（`default_clock` · `default_reset`）+ 根自有 **16**
> （4 路 × {payload[7:0], valid, ready} + `select[1:0]` + {out_payload[7:0], out_valid, out_ready}）。

## 批次 A — 设计定义 + 判据面（AC1 · AC3 · AC5 · AC6 · AC7）

| # | 任务 | 落点 | 判据 | 状态 |
|---|---|---|---|---|
| A1 | **顶层组件**（`chlib::stream_mux<4, ch_uint<8>>` 包装 · 16 端口具名 · 输入流先 `as_master()` · ready 链 `<<=`） | `examples/stream/stream_mux_top.h` | AC1 | ✅ |
| A2 | **期望源段**（手推）：module 集 `{top}` · 端口表 **18** · 4 场景 / **24** 条 · `VERIF-CHECKS: 24` · 4 个 `#### scenario:` 锚点 | `.rddf/checks/examples-rtl-expectations.md` | AC6 | ✅ |
| A3 | **清单行**（`kind=target` · 自发行 · `interp_checks=24` · `min_checks=24` · `generated=yes` · `scenarios=4` · `spec_checks=18`） | `.rddf/checks/examples-rtl-sim-targets.tsv` | AC5 · AC7 | ✅ |
| A4 | **`# REGISTERED_TARGETS` 11 → 12** | 同上 | AC5 | ✅ |
| A5 | **落选表删该行**（`stream_mux_demo` · 原理由「类别已覆盖 · 期望源面窄」⇒ 已被本 change 取代 ⇒ 理由留痕在 proposal）· 守恒订正 `27 == 12 ∪ 15` | `.rddf/checks/examples-rtl-sim-exclusions.{tsv,md}` | AC5 | ✅ |

## 批次 B — 发射面 + 独立 TB（AC2 · AC3 · AC4 · AC8）

| # | 任务 | 落点 | 判据 | 状态 |
|---|---|---|---|---|
| B1 | **tb 双模式**（默认 `.verif` 驱动 · `--emit-rtl` 复用库层三件套 · 未知参数 exit 2） | `examples/stream/tb_stream_mux_demo.cpp` | AC2 · AC8 | ✅ |
| B2 | **验证源**（4 场景 / 24 断言 · 期望全手推） | `examples/stream/tb_stream_mux_demo.verif` | AC4 · AC6 | ✅ |
| B3 | **生成独立 TB**（`--emit-hygiene --emit-tb-verif`）⇒ 仓内 `.sv` 逐字节 == 重生成 | `examples/stream/tb_stream_mux_demo.sv` | AC4 | ✅ |
| B4 | **产物实测**：module 集 `{top}` · 顶层端口 **18** ⇒ 与 A2 手推一致 | 发射产物 | AC3 | ✅ |
| B5 | **构建接入**（target 改名 · `CPPHDL_SOURCE_DIR` 注入 · include dir） | `examples/{,stream/}CMakeLists.txt` | AC2 · AC8 | ✅ |

## 批次 C — 文档与验收（AC9 · AC10）

| # | 任务 | 落点 | 判据 | 状态 |
|---|---|---|---|---|
| C1 | **文档同步**：`examples/stream/AGENTS.md`（新）· 根 `AGENTS.md` 的示例路径 | 同左 | AC9 | ✅ |
| C2 | **护栏**：`verify_examples_rtl_sim.sh`（含 ⑨/⑥'/⑧⁗）· `verify_examples_rtl_sim_coverage.sh` · `verify_port_naming.sh` · `run_all_ported_tests.sh` · `verify_test_counts.sh` | — | AC9 | ✅ |
| C3 | **遗留清扫**：本次范围内 MUST NOT 有 TODO / 半截实现 / 注释掉的代码 / 临时日志 / 被 skip 的测试 / 旧 target 残留 | 本 change 触及的全部文件 | AC9 | ✅ |
| C4 | **openspec validate --strict** valid | — | AC10 | ✅ |

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
| AC9 | C1 · C2 · C3 |
| AC10 | C4 |
