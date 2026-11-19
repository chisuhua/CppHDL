# tasks — `axi4-convergence-tail-2026-11`

> ⚠️ **设计不在本文件**：决策面 = 蓝图 `docs/architecture/2026-11-17-rtl-verification-single-source.md`
> §4.3.3（三角色职责分离）+ §7 R13（SoC 手写激励过渡态）+ capability `examples-rtl-sim-coverage`。
> ⚠️ **前置**：`example-file-boundary-2026-11`（**已归档 2026-11-18**）⇒ ⇒ 本 change 是它的收口。
> ⚠️ **顺序**：批次 A **先于**批次 B ⇒ ⇒ 理由 = 「先有守护机制，再上新产品单元」
> ⇒ ⇒ 否则 phase3a 的 tb 会**一落地就没人守**。
> ⚠️ **AC12 的 spec delta 由本 change 的实施侧产出**（`specs/**` 属 builder 写集）⇒ ⇒ planner 只起草 `proposal.md`。

## 批次 A — 判据面：C++ 侧条数的登记与守护（AC6 · AC7 · AC8 · AC9 · AC10）

| # | 任务 | 落点 | 判据 | 状态 |
|---|---|---|---|---|
| A1 | 清单**新增 `interp_checks` 列**（13 → **14 列**）：位置 = `emitter_args` **之后**；`#COLUMNS:` 同步；**所有数据行**补列（无 C++ 侧入口 ⇒ `-` 哨兵）⇒ ⇒ ⚠️ **MUST 按列名**取（既有「清单列号 MUST 从机器可读表头推导」已覆盖）⇒ ⇒ 字段数守恒的具名 fail MUST 仍生效 | `.rddf/checks/examples-rtl-sim-targets.tsv` | AC6 | ☐ |
| A2 | **回填两个自发行的值**：`tb_axi4_lite_example` = **113**（= 其 `min_checks`）· `tb_axi_soc_example` = **2**（C++ 侧实际条数 ⇒ ≠ `.sv` 侧的 14 ⇒ **不是笔误**）⇒ ⇒ 其余行 = `-` | 同上 | AC6 | ☐ |
| A3 | **退役 note 的 `interp-entry` 约定**：删 Lite 行 note 里的该标记 ⇒ ⇒ ⑨ 的作用域从「note 含标记」改为「`interp_checks` 非 `-`」⇒ ⇒ **自由文本不再承载判据** | 同上 + 护栏 | AC9 | ☐ |
| A4 | **⑨ 改造**：作用域 = `interp_checks` 非 `-`；等式改为 `checks == interp_checks`（**不是** `min_checks` ⇒ ⇒ `.sv` 侧与 C++ 侧各自登记）；失败信息 MUST 同时给出期望值与实际值 | `scripts/verify_examples_rtl_sim.sh` | AC7 | ☐ |
| A5 | **反向判据（新增）**：`emitter == target`（自发行）⇒ MUST 有 `interp_checks` 且 **> 0** ⇒ ⇒ 否则**必红**且**点名该行的 target** ⇒ ⇒ 「新增自发射 example 忘登记」抓得住 | 同上 | AC8 | ☐ |
| A6 | **`--self-test` 加 2 条投毒**：① 某自发行的 `interp_checks` 置 `-` ⇒ ⇒ 反向判据必红 ② 某自发行的 `interp_checks` 改成 ≠ 实际值 ⇒ ⇒ ⑨ 必红（⚠️ **投毒 15 的教训**：不能改 `min_checks` 冒充 —— ⑥ 会抢先命中 ⇒ ⇒ 改的是 `interp_checks` ⇒ ⇒ 且被投毒的二进制 MUST **转发真实实现**再改结论行，否则 ③ 会先报「产物缺失」）⇒ ⇒ **必绿对照**：非自发行的行 `interp_checks = -` ⇒ ⇒ 仍绿 | 同上 | AC10 | ☐ |
| A7 | **`make_list` 的 fixture 同步**（self-test 用的 3 行）⇒ ⇒ **14 列** ⇒ ⇒ 护栏自检的「必绿用例」插列逻辑 MUST 仍成立 | 同上 | AC10 | ☐ |

## 批次 B — `phase3a_complete` 拆三角色（AC1 · AC2 · AC3 · AC4 · AC5）

| # | 任务 | 落点 | 判据 | 状态 |
|---|---|---|---|---|
| B1 | **设计定义抽出**：`Phase3ATop`（7 端口 + 3 个 `ch_module`）⇒ ⇒ 新建 `examples/axi4/phase3a_top.h` ⇒ ⇒ **无 `main`** ⇒ ⇒ `namespace axi4::example` + `kPhase3aRootName` 常量（与 Lite / SoC 的根名常量**同型** ⇒ ⇒ 根名单一真相源） | `examples/axi4/phase3a_top.h` | AC1 | ☐ |
| B2 | **激励驱动抽出**：新建 `examples/axi4/tb_phase3a_example.cpp` ⇒ ⇒ `main`：写 0xAB → 读回 ⇒ ⇒ **2 条断言** ⇒ ⇒ **协议行 `RESULT: checks=2 fails=0`**（**取代**现散文「Phase 3A AXI-Lite 链路仿真通过」）⇒ ⇒ FAIL MUST 进 stderr 且影响退出码（**禁**「只打印不判定」）⇒ ⇒ 默认模式收到未知参数 ⇒ **exit 2**（与另两个 tb 同型） | `examples/axi4/tb_phase3a_example.cpp` | AC2 | ☐ |
| B3 | **删原文件** `examples/axi4/phase3a_complete.cpp` ⇒ ⇒ ⚠️ **顺带删掉假动作输出**（2026-11-19 已在别处清过一次「Generating Verilog…」；本条确保拆分后**不再引入**）⇒ ⇒ ⚠️ **本 change 不加 `--emit-rtl`** ⇒ ⇒ **不进清单** ⇒ ⇒ **不冻结** module 集 / 端口数期望（理由见 proposal「Out of Scope」） | `examples/axi4/` | AC3 | ☐ |
| B4 | **target 改名**：`phase3a_complete` → `tb_phase3a_example` ⇒ ⇒ **CTest 条目名 `spinalhdl_axi4_phase3a` 不变**（`add_spinalhdl_ported_test(axi4_phase3a tb_phase3a_example)`）⇒ ⇒ **改名不是缩面** ⇒ ⇒ CTest 总数不变 | `examples/axi4/CMakeLists.txt` · `tests/CMakeLists.txt` | AC3 · AC4 | ☐ |
| B5 | **名单面**：`CH_TRACKED_EXAMPLE_TARGETS`（**27 项不变** · 仅改名）· `install(TARGETS …)` 同名项改名 ⇒ ⇒ `run_all_ported_tests.sh` 的 STALE-ARTIFACT 判据 MUST 绿（旧二进制残留 ⇒ 必红） | `examples/CMakeLists.txt` · `examples/axi4/CMakeLists.txt` | AC5 | ☐ |
| B6 | **CMake include 路径**：新 tb 用**裸 include**（`#include "phase3a_top.h"`）⇒ ⇒ target 的 `target_include_directories` MUST 含 `${CMAKE_CURRENT_SOURCE_DIR}`（与另两个 tb 同型）⇒ ⇒ ⚠️ **禁**用「加 include 路径绕过依赖方向」的形态 | `examples/axi4/CMakeLists.txt` | AC1 | ☐ |

## 批次 C — 文档与验收（AC11 · AC12）

| # | 任务 | 落点 | 判据 | 状态 |
|---|---|---|---|---|
| C1 | **spec delta**：`specs/examples-rtl-sim-coverage/spec.md` 加 **ADDED 1 条**（「C++ 侧入口的断言条数 MUST 登记且被守护」）⇒ ⇒ ⚠️ 正文 MUST 含 SHALL/MUST · Scenario MUST 用 WHEN/THEN ⇒ ⇒ `openspec validate <change> --strict` MUST 通过 | `openspec/changes/axi4-convergence-tail-2026-11/specs/` | AC12 | ☐ |
| C2 | **文档同步**：`examples/axi4/AGENTS.md`（FILES 表 · PATTERNS 段）· `docs/CHANGELOG.md`（一行）⇒ ⇒ ⚠️ **MUST NOT** 写「phase3a 不产出 RTL 产物」为「已知缺口」而不给 follow-up 指针 | `examples/axi4/AGENTS.md` · `docs/CHANGELOG.md` | AC11 | ☐ |
| C3 | **锚点核对**：CTest 条目总数 / non-perf / `-L base` / `EXAMPLES_TEST_SOURCES` / `tests/test_*.cpp` ⇒ ⇒ **预期全部不变**（改名不增条目 · 无新增测试源）⇒ ⇒ 若有变化 MUST 按实测回填 | 根 `AGENTS.md` | AC11 | ☐ |
| C4 | **遗留清扫**：本次范围内 MUST NOT 有 TODO / 半截实现 / 注释掉的代码 / 临时日志 / 被 skip 的测试 / 未替换的 mock ⇒ ⇒ **逐项核对**并记入实施实况 | 本 change 触及的全部文件 | AC11 | ☐ |

---

## AC → 任务映射

| AC | 任务 |
|---|---|
| AC1 | B1 · B6 |
| AC2 | B2 |
| AC3 | B3 · B4 |
| AC4 | B4 |
| AC5 | B5 |
| AC6 | A1 · A2 |
| AC7 | A4 |
| AC8 | A5 |
| AC9 | A3 |
| AC10 | A6 · A7 |
| AC11 | C2 · C3 · C4 |
| AC12 | C1 |

---

## 验收取证表

| 任务 | 命令 | 期望 |
|---|---|---|
| A1–A2 | `awk -F'\t' '!/^#/ && NF>0 {print NF}' .rddf/checks/examples-rtl-sim-targets.tsv \| sort -u` | 单一值 **14** |
| A2 | `awk -F'\t' '$5==$1 {print $1, $6}' .rddf/checks/examples-rtl-sim-targets.tsv` | Lite = **113** · SoC = **2**（恰好 2 行） |
| A3 | `grep -c 'interp-entry' .rddf/checks/examples-rtl-sim-targets.tsv` | **0** |
| A4–A5 | 投毒：SoC 的 `interp_checks` 改 ≠ 2 ⇒ 跑 `scripts/verify_examples_rtl_sim.sh` | **必红**（且信息含期望值与实际值） |
| A5 | 投毒：Lite 的 `interp_checks` 置 `-` ⇒ 同上 | **必红**（且点名 target） |
| A6 | `bash scripts/verify_examples_rtl_sim.sh --self-test` | **全部投毒转红** · 必绿用例仍绿 · 还原复绿 |
| B1 | `grep -c 'int main' examples/axi4/phase3a_top.h` | **0** |
| B2 | `./build/examples/axi4/tb_phase3a_example` | `RESULT: checks=2 fails=0` · exit **0** |
| B2 | `./build/examples/axi4/tb_phase3a_example --bogus` | exit **2** |
| B3 | `ls examples/axi4/phase3a_complete.cpp` | 不存在 |
| B4 | `ctest --test-dir build -N \| grep -c spinalhdl_axi4_phase3a` | **1**（条目名未变） |
| B5 | `grep -c . examples/CMakeLists.txt` 的 tracked 块计数 | **27** |
| B5 | `./run_all_ported_tests.sh` | `PASS: 27` · `STALE-ARTIFACT: 0` |
| C1 | `openspec validate axi4-convergence-tail-2026-11 --strict` | valid |
| C1 | `bash scripts/verify_openspec_delta.sh` | 4 条全绿 |
| C2 | `grep -rn 'phase3a_complete' examples/ docs/ .rddf/checks/` | **零命中**（除归档区） |
| C3 | `bash scripts/verify_test_counts.sh` | 锚点与实测一致（**预期不变**） |
| C4 | 逐文件扫 TODO / 注释掉的代码 / `SKIP(` | 零命中 |
| 全量 | `ctest --test-dir build --output-on-failure` | **全绿** |
| 全量 | `bash scripts/verify_examples_rtl_sim.sh` · `verify_examples_rtl_sim_coverage.sh` · `verify_example_test_wiring.sh` | 全 PASS |
