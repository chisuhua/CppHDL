# tasks — `phase3a-rtl-product-2026-11`

> ⚠️ **设计不在本文件**：决策面 = ADR-072（产品端口面 · 四件套）· ADR-075（端口具名 E1）·
> ADR-074（验证单一真相源 · D7 手写逃生口）· capability `examples-rtl-sim-coverage`。
> ⚠️ **顺序**：批次 A（**判据面**：具名 + 期望 + 清单）**先于**批次 B（**发射 + TB**）
> ⇒ ⇒ 理由 = 「先有冻结的期望，再产出产物」⇒ ⇒ 否则产物出来后才定期望 = 抄实现。
> ⚠️ **无 spec 变更** ⇒ `.openspec.yaml` 已声明 `skip_specs: true` ⇒ 归档用 `--skip-specs`。
> ⚠️ **端口数口径**：**9** = 框架 2（`default_clock` · `default_reset`）+ 根自有 7
> （SoC 同口径 · `soc-rtl-expectations.md` §1）⇒ ⇒ **不是 7**。

## 批次 A — 判据面：具名 + 手推期望 + 清单（AC1 · AC2 · AC4 · AC6 · AC7 · AC8）

| # | 任务 | 落点 | 判据 | 状态 |
|---|---|---|---|---|
| A1 | **7 个端口全部具名**（`ch_in<ch_bool> start{"start"};` 等）⇒ ⇒ 产物端口名 = `top_<成员名>` ⇒ ⇒ **硬前置**（未具名 ⇒ 产物端口名 = `top_unnamed_input_N` ⇒ TB 无法按名连线） | `examples/axi4/phase3a_top.h` | AC1 | ☐ |
| A2 | **豁免表删 7 行**（phase3a 的未具名端口）⇒ ⇒ `scripts/verify_port_naming.sh` 的 `EXPECTED_EXEMPT_PORTS` **1384 → 1377** | `.rddf/checks/port-naming-exempt.tsv` · `scripts/verify_port_naming.sh` | AC2 | ☐ |
| A3 | **期望源段**（手推 · 禁抄实现 / 禁抄仿真输出）：`## tb_phase3a_example {#phase3a-example}` ⇒ 记 **module 集 = {top}**（1 个）· **顶层端口表 = 9 行**（名/向/宽/成员/语义）· **TB 断言条数 N** 及其**逐条推导**（MUST ≥ 5：写完成 · 读回值 · `default_reset` · `rst_n` · 有界超时）⇒ ⇒ 外部规范 = AXI4-Lite（写后读回）⇒ ⇒ 该部分 `spec-derived`，复位与拍数 = impl-contract | `.rddf/checks/examples-rtl-expectations.md` | AC4 · AC7 | ☐ |
| A4 | **清单行**：`tb_phase3a_example` · `kind=target`（桶判据 = 「`soc` 桶 = 有显式 `ch_layer` 成层声明」⇒ phase3a 无 ⇒ target）· `emitter==target`（自发行）· `interp_checks=2`（C++ 侧）· `min_checks=N`（TB 侧）· `generated=no` · `scenarios` / `spec_checks` 取值 | `.rddf/checks/examples-rtl-sim-targets.tsv` | AC6 · AC8 · AC12 | ☐ |
| A5 | **`# REGISTERED_TARGETS` 10 → 11** | 同上 | AC6 | ☐ |
| A6 | **落选表删该行**（`tb_phase3a_example` · 原理由「阶段汇总回归 · 非独立产品单元」⇒ **已被本 change 取代** ⇒ 理由留痕在 proposal）⇒ ⇒ 落选 18 → 17 | `.rddf/checks/examples-rtl-sim-exclusions.tsv` | AC6 | ☐ |
| A7 | **守恒式的陈旧值同批订正**（两处写 `28 == 10 ∪ 18`，而 tracked 已是 **27** ⇒ **已旧**）⇒ ⇒ 订正为 **`27 == 11 ∪ 16`** | `.rddf/checks/examples-rtl-sim-exclusions.tsv`（头注释）· `.rddf/checks/examples-rtl-sim-exclusions.md` | AC6 · AC9 | ☐ |

## 批次 B — 发射面：`--emit-rtl` + 独立 TB（AC3 · AC5）

| # | 任务 | 落点 | 判据 | 状态 |
|---|---|---|---|---|
| B1 | **`--emit-rtl` 模式**（照 Lite / SoC 的**同型**形态 ⇒ 复用本会话已下沉库层的 `strip_mode_flags` + `require_out_dir` + `run_emit_cli`）⇒ ⇒ 默认模式（无参）行为**逐字不变**（2 条断言 + `RESULT: checks=2 fails=0`）⇒ ⇒ ⚠️ **不加** `--check-products`（phase3a 无 `ch_layer` ⇒ 层次化自检无判别力） | `examples/axi4/tb_phase3a_example.cpp` | AC3 | ☐ |
| B2 | **发射产物实测**：module 集 == **{top}**（1 个）· 顶层端口 == **9** 根且名 == `top_<成员名>` ⇒ ⇒ **实测**与 A3 的手推期望**逐字一致**（不一致 ⇒ 停，先查期望） | 发射产物 | AC4 | ☐ |
| B3 | **独立 TB**（手写 · `examples/axi4/tb_phase3a_example.sv`）：**9 根连线**（框架 2 + 根自有 7；⚠️ 框架 2 漏连 ⇒ 严格 flags 下 `%Warning-PINMISSING` **致命**）· 断言 = A3 冻结的 N 条 ⇒ ⇒ `RESULT: checks=N fails=0` ⇒ ⇒ **N MUST == 清单 `min_checks`**（护栏 ⑥ 的**等式**）· 头注释带 `// EXPECT-SRC:` 指向 `#phase3a-example` | 新文件 | AC5 | ☐ |
| B4 | **变异自证 ≥2 条**（如「`rdata` 恒 0」·「`done` 恒 0」·「复位失效」）⇒ ⇒ TB **必红**（`fails>0`）⇒ ⇒ **先确认变异后仍能编译**（否则跑的是旧二进制 ⇒ 假绿）⇒ ⇒ 形态登记在期望源段 | 临时变异 | AC11 | ☐ |
| B5 | **头注释同步**：本 change 前写的「不进产品清单 · 无发射模式」陈述**已过时** ⇒ MUST 改 | `examples/axi4/tb_phase3a_example.cpp` | AC11 | ☐ |

## 批次 C — 文档与验收（AC9 · AC10）

| # | 任务 | 落点 | 判据 | 状态 |
|---|---|---|---|---|
| C1 | **文档同步**：`examples/axi4/AGENTS.md`（FILES + PATTERNS + 「无发射模式」的旧句）· `docs/CHANGELOG.md`（**用户可见变更**节） | 同左 | AC9 | ☐ |
| C2 | **锚点核对**：`CTEST_TOTAL` / non-perf / `tests/test_*.cpp` / `EXAMPLES_TEST_SOURCES` / `DUAL_BACKEND_*` ⇒ ⇒ **预期全部不变**（无新增测试源 · 无新增 CTest 条目）⇒ ⇒ 变了的按实测回填 | 根 `AGENTS.md` | AC9 | ☐ |
| C3 | **遗留清扫**：本次范围内 MUST NOT 有 TODO / 半截实现 / 注释掉的代码 / 临时日志 / 被 skip 的测试 | 本 change 触及的全部文件 | AC9 | ☐ |
| C4 | **归档**：`openspec archive phase3a-rtl-product-2026-11 --yes --skip-specs` ⇒ ⇒ 归档后核对主 spec **零变更**（`skip_specs` 的合法性由 `.openspec.yaml` 声明支撑） | — | AC10 | ☐ |

---

## AC → 任务映射

| AC | 任务 |
|---|---|
| AC1 | A1 · B2 |
| AC2 | A2 |
| AC3 | B1 |
| AC4 | A3 · B2 |
| AC5 | B3 |
| AC6 | A4 · A5 · A6 · A7 |
| AC7 | A3 |
| AC8 | A4 |
| AC9 | A7 · C1 · C2 · C3 |
| AC10 | C4 |
| AC11 | B4 · B5 · A3 |
| AC12 | A4 · A3 |

---

## 验收取证表

| 任务 | 命令 | 期望 |
|---|---|---|
| A1 | `grep -c 'ch_bool> start{' examples/axi4/phase3a_top.h` | **1**（7 个端口全具名） |
| A2 | `grep -c 'EXPECTED_EXEMPT_PORTS=1377' scripts/verify_port_naming.sh` | **1** · 豁免表无 phase3a 行 |
| A3 | `grep -c 'phase3a-example' .rddf/checks/examples-rtl-expectations.md` | ≥ **1**（锚点存在） |
| A4 | `awk -F'\t' '$1=="tb_phase3a_example"{print $7, $11, $12, $13}' …targets.tsv` | `interp_checks=2` · `min_checks=N` · `generated=no` |
| A5–A7 | `bash scripts/verify_examples_rtl_sim_coverage.sh` | **PASS**（守恒 `27 == 11 ∪ 16`） |
| B1 | `./build/examples/axi4/tb_phase3a_example --emit-rtl --out <dir>` | exit **0** + `top.v` + `manifest.json` |
| B2 | `grep -c '^module ' <dir>/top.v` | **1** · 端口 **9** 根 |
| B3 | 护栏 `verify_examples_rtl_sim.sh` 的 ③④⑤⑥ | 编译通过 · 跑通 · `checks == N == min_checks` |
| B4 | 投毒 `rdata` 恒 0 | TB **必红**（`fails>0`） |
| C2 | `bash scripts/verify_test_counts.sh` | 锚点与实测一致（**预期不变**） |
| 全量 | `ctest --test-dir build --output-on-failure` | **全绿** |
| 全量 | `bash scripts/verify_examples_rtl_sim.sh` · `verify_examples_rtl_sim_coverage.sh` · `verify_port_naming.sh` | 全 PASS |
