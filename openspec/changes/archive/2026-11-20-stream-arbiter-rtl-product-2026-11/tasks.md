# tasks — `stream-arbiter-rtl-product-2026-11`

> ⚠️ **设计不在本文件**：判据面 = ADR-074（验证单一真相源）· ADR-072（产品端口面 · 四件套）·
> ADR-075（端口具名 E1）· capability `chlib-stream-arbitration`（轮转 / 无饿死 / 相位不入契约）·
> `chlib-stream-ready-ownership`（ready 归属）。
> ⚠️ **顺序**：批次 A（设计 + 期望）先于批次 B（发射 + TB）⇒ ⇒ 先冻结期望，再产出产物。
> ⚠️ **无 spec 变更** ⇒ 归档用 `openspec archive … --yes --skip-specs`。
> ⚠️ **端口数口径**：**18** = 框架 2（`default_clock` · `default_reset`）+ 根自有 **16**
> （4 路输入流 × {payload[7:0], valid, ready} + 输出流 {payload[7:0], valid, ready} + `selected[1:0]`）。
> ⚠️ **相位口径**：capability 明文「MUST NOT 要求某一具体相位」⇒ ⇒ 本 change 的判据
> **不锁**首拍授予谁 ⇒ ⇒ 逐拍金标**只在** `tests/chlib/test_stream.cpp` 的 `[rotation]`
> 判别力用例里 ⇒ ⇒ 产品面用**效果属性**（`WAIT` + 紧界）表达。

## 批次 A — 设计定义 + 判据面（AC1 · AC3 · AC5 · AC6 · AC7 · AC12）

| # | 任务 | 落点 | 判据 | 状态 |
|---|---|---|---|---|
| A1 | **顶层组件**（`chlib::stream_arbiter_round_robin` 包装 · 16 端口具名 · 输入流先 `as_master()` · ready 链 `<<=` · 消费端 `<<=` 覆盖输出 ready · `selected` 输出） | `examples/stream/stream_arbiter_top.h` | AC1 | ✅ |
| A2 | **期望源段**（手推）：module 集 `{top}` · 端口表 **18** · 5 场景 / **38** 条 · `VERIF-CHECKS: 38` · 5 个 `#### scenario:` 锚点 · **相位口径段落**（为何不锁相位 + `WAIT` 紧界的推导） | `.rddf/checks/examples-rtl-expectations.md` | AC6 · AC12 | ✅ |
| A3 | **清单行**（`kind=target` · 自发行 · `interp_checks=38` · `min_checks=38` · `generated=yes` · `scenarios=5` · `spec_checks=37`） | `.rddf/checks/examples-rtl-sim-targets.tsv` | AC5 · AC7 | ✅ |
| A4 | **`# REGISTERED_TARGETS` 14 → 15** | 同上 | AC5 | ✅ |
| A5 | **落选表删该行**（`stream_arbiter_demo` · 归位条件「待 capability 口径稳定」**已满足**）· 守恒订正 `27 == 15 ∪ 12`（tsv + md 双面） | `.rddf/checks/examples-rtl-sim-exclusions.{tsv,md}` | AC5 | ✅ |

## 批次 B — 发射面 + 独立 TB（AC2 · AC3 · AC4 · AC8）

| # | 任务 | 落点 | 判据 | 状态 |
|---|---|---|---|---|
| B1 | **tb 双模式**（默认 `.verif` 驱动 · `--emit-rtl` 复用库层三件套 · 未知参数 exit 2） | `examples/stream/tb_stream_arbiter_demo.cpp` | AC2 · AC8 | ✅ |
| B2 | **验证源**（5 场景 / 38 断言 · 期望全手推 · **相位无关**） | `examples/stream/tb_stream_arbiter_demo.verif` | AC4 · AC6 · AC12 | ✅ |
| B3 | **生成独立 TB**（`--emit-hygiene --emit-tb-verif`）⇒ 仓内 `.sv` 逐字节 == 重生成 | `examples/stream/tb_stream_arbiter_demo.sv` | AC4 | ✅ |
| B4 | **产物实测**：module 集 `{top}` · 顶层端口 **18** ⇒ 与 A2 手推一致 | 发射产物 | AC3 | ✅ |
| B5 | **构建接入**（target 改名 · `CPPHDL_SOURCE_DIR` 注入 · include dir） | `examples/{,stream/}CMakeLists.txt` | AC2 · AC8 | ✅ |

## 批次 C — codegen 缺陷修复（AC13 · **本产品的前置条件**）

| # | 任务 | 落点 | 判据 | 状态 |
|---|---|---|---|---|
| C1 | **根因定位**：hygiene 只对齐**操作数**宽 ⇒ `mod` 节点声明宽（2 bit）< 运算自然宽（3 bit）⇒ ⇒ 赋值处隐式截断 ⇒ ⇒ `WIDTHTRUNC` | `src/codegen_verilog.cpp::print_binary_op` | AC13 | ✅ |
| C2 | **修法**：补「结果宽度对齐」⇒ ⇒ 只在「运算自然宽 > 节点声明宽」时补显式 cast ⇒ ⇒ **语义零变化**（显式 ≡ 隐式截断） | 同上 | AC13 | ✅ |
| C3 | **零回归**：既有 16 个产品的护栏全绿 + `verify_flat_rtl_emit.sh` PASS + `verify_hierarchical_rtl.sh` PASS + `ctest` 全绿 | — | AC13 · AC9 | ✅ |

## 批次 D — 判别力 + 文档 + 验收（AC9 · AC10 · AC11）

| # | 任务 | 落点 | 判据 | 状态 |
|---|---|---|---|---|
| D1 | **变异自证 ≥ 3 条**（M1 `out_valid` 恒 0 · M2 `selected` 恒 0 · M3 `last_grant` 反馈恒 0 = **历史缺陷形态**）⇒ 全部转红 · **先确认仍能编译** | 临时产物 + Verilator | AC11 | ✅ |
| D2 | **文档同步**：`examples/stream/AGENTS.md`（4/4 + 语义面对照表）· 根 `AGENTS.md` · `docs/CHANGELOG.md` | 同左 | AC9 | ✅ |
| D3 | **护栏**：`verify_examples_rtl_sim.sh` · `verify_examples_rtl_sim_coverage.sh` · `verify_port_naming.sh` · `run_all_ported_tests.sh` · `verify_test_counts.sh` · `verify_docs_links.sh` · `verify_agents_size.sh` | — | AC9 | ✅ |
| D4 | **遗留清扫**：本次范围内 MUST NOT 有 TODO / 半截实现 / 注释掉的代码 / 临时日志 / 被 skip 的测试 / 旧 target 残留 | 本 change 触及的全部文件 | AC9 | ✅ |
| D5 | **openspec validate --strict** valid | — | AC10 | ✅ |
| D6 | **既有红登记**：`verify_hierarchical_p8.sh` 的 R4②（活账引用 `include/axi4/axi4_lite_example_top.h` —— 该头在 `08298a0` 已移走 ⇒ ⇒ **会话前既有**）⇒ ⇒ **只登记，不修** | 本 change 的验收报告 | AC9 | ✅ |

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
| AC9 | C3 · D2 · D3 · D4 · D6 |
| AC10 | D5 |
| AC11 | D1 |
| AC12 | A2 · B2 |
| AC13 | C1 · C2 · C3 |
