# tasks — `example-file-boundary-2026-11`

> ⚠️ **设计不在本文件**：决策面 = ADR-074 / ADR-046 / ADR-075 + 蓝图 §4.3。
> ⚠️ **顺序**：本 change 在 `axi4-verif-driven-cpp` → `emit-cli-into-example` **之后**（G-2）。
> ⚠️ **3.2（批次 F）受 G-1 约束**：architect 未裁前 **MUST NOT 开工**。

## 批次 A — 头文件归位（AC1 · AC2 · AC3 · AC4 · AC7）

| # | 任务 | 落点 | 判据 | 状态 |
|---|---|---|---|---|
| A1 | `git mv` 两个顶层头到 `examples/axi4/`（保留历史） | `include/axi4/` → `examples/axi4/` | AC1 | ☐ |
| A2 | **3 个 examples 消费者改裸 include**：`tb_axi4_lite_example.cpp` · `axi_soc_example.cpp` · `emit_rtl_soc.cpp`（**`emit_rtl.cpp` 已被前序 change 删除**） | `examples/axi4/` | AC3 | ☐ |
| A3 | 库公开面归零核验 | `include/**` | AC2 | ☐ |
| A4 | 根 `tests/` 不再引用这两个头（由 B1–B4 完成后核验） | `tests/**` | AC7 | ☐ |
| A5 | **搬入的 2 个测试**（`examples/axi4/tests/*.cpp`）也用裸 include（靠 helper 的 `-I examples/axi4`） | `examples/axi4/tests/` | AC3 | ☐ |
| A6 | **C2 扩展（最小面 + 剥注释）**：扫描面仍只 `tests/CMakeLists.txt`，匹配串 `examples/riscv-mini/src` → `examples/`，**移植 C7 的剥注释 + 括号配平**；新增 fixtures：① 注释里出现 `examples/` ⇒ **绿** ② `target_include_directories(… examples/axi4)` ⇒ **红** ⇒ 并在 AC4 里点名 C7 一并绿 | `scripts/verify_no_duplicate_rv32i_headers.sh` | AC4 | ☐ |

## 批次 B — 消费者归位 / 换 DUT（AC5 · AC6 · AC8 · AC9）

| # | 任务 | 落点 | 判据 | 状态 |
|---|---|---|---|---|
| B1 | `tests/test_axi_lite_verilator_e2e.cpp`：删顶层头 include ⇒ `using` 改库组件 ⇒ DUT 换 `AxiLiteSlave<32,32,4>` ⇒ 顶部注释同步（**把陈旧的「10 inputs / 7 outputs」订正为 12 / 8**）⇒ 路径 / 用例名 / SECTION / 断言**零改动** | 该文件 | AC5 | ☐ |
| B2 | 该 e2e 仍绿（verilator 在 `build/verilator-install/bin` 或 PATH） | ctest | AC6 | ☐ |
| B3 | `tests/test_axi_lite_test_master.cpp` → `examples/axi4/tests/`（**CTest 条目名不变**） | `examples/axi4/tests/` | AC9 | ☐ |
| B4 | `tests/test_verif_single_source.cpp` **按准则拆分**：凡以 `ch_device<axi4::example::AxiLiteTop>` / `LateAxiLiteTop` 为 DUT 的用例 → `examples/axi4/tests/test_verif_axi4_lite.cpp`；其余（扫描器用例）留 `tests/` ⇒ **共享 helper 一并搬**（`verif_path()` / `LateAxiLiteTop` / `read_file()`） | 两处 | AC8 | ☐ |
| B5 | **TEST_CASE 名集合守恒（判据）**：拆分前后差集 == ∅（源码解析或 `ctest -N` 对照）⇒ 结果记录在 note | 记录 | AC8 | ☐ |

## 批次 C — 构建与锚点（AC8 · AC9 · AC10 · AC20）

| # | 任务 | 落点 | 判据 | 状态 |
|---|---|---|---|---|
| C1 | `examples/axi4/CMakeLists.txt` 增 `axi4_catch_test` helper：链 `catch_amalgamated` + include 集（`include` · `tests` · `examples/axi4`）+ `add_test` + **`LABELS base`** + `TIMEOUT 300` + **`CPPHDL_SOURCE_DIR` 编译定义** | 同上 | AC8 · AC9 | ☐ |
| C2 | 根 `tests/CMakeLists.txt` 去掉迁走的注册项（**不得**重复注册） | `tests/CMakeLists.txt` | AC9 | ☐ |
| C3 | **锚点按实测回填**：`CTEST_TOTAL` 268 → **269** · non-perf **257 → 258** · `-L base` **231 → 232** · `EXAMPLES_TEST_SOURCES` 18 → **20** | 根 `AGENTS.md` | AC10 | ☐ |
| C4 | **手工改 `tests/test_*.cpp` 计数行**：现写 **145** 但实测 **159** ⇒ 搬走 1 个后为 **158**（**MUST 用实测值，禁「原值 −1」**） | 根 `AGENTS.md` | AC10 | ☐ |
| C5 | **登记 `CPPHDL_BUILD_EXAMPLES=OFF` 的接受项**（被搬走的 2 个条目消失 ⇒ 锚点只在默认配置成立）⇒ 写在该锚点旁 + 指向本 change | 根 `AGENTS.md` / `.rddf/checks/` | AC20 | ☐ |

## 批次 D — 登记面（AC11 · AC12 · AC13 · AC14）

| # | 任务 | 落点 | 判据 | 状态 |
|---|---|---|---|---|
| D1 | 棘轮豁免表 **23 行**只改第一列路径（**不删行**） | `.rddf/checks/port-naming-exempt.tsv` | AC11 | ☐ |
| D2 | **公开面移除登记 5 条**：2 条 `nofile`（两个头的 glob）+ 3 条 `removed`（类名 · `scope=include/`）⇒ 同步 `EXPECTED_PROBES` 46 → **51**、`EXPECTED_REMOVAL_ROWS` 25 → **30**（登记行 MUST 用 `**移除**`）⇒ `public-api-surface.md` 同章逐条登记 | `.rddf/checks/public-api-removals.tsv` · `public-api-surface.md` | AC12 | ☐ |
| D3 | **输入端口登记表的 3 行保留**：把 `input_port_registry.py` 的 `header_of()` 搜索根从 `include/**` 扩到 `include/**` + `examples/**`（否则 R1 报「登记过时」）⇒ 只更新这 3 行的**消费者路径**列 | `scripts/lib/input_port_registry.py` + 登记表 | AC13 | ☐ |
| D4 | **零缩面核对**：`verify_rst_n_wiring.sh` 的 `COMPONENTS_W_RST_N` 组件数 **搬家前后相同**（3 个组件仍在表内）⇒ 若数量变化 ⇒ 说明走了「删行」路线 ⇒ 回退 | `scripts/verify_rst_n_wiring.sh` | AC13 | ☐ |
| D5 | `CH4_ANCHOR` **+2 行**（两个新 `examples/axi4/tests/*.cpp` 的断言数下限） | `scripts/verify_example_assertion_quality.sh` | AC14 | ☐ |
| D6 | **新建 capability 的护栏**：`scripts/verify_example_only_headers.sh`（末段匹配 · 作用域 `include/ src/ tests/ examples/ tools/` · 三态区分孤儿/歧义/违规 · 登记表双向**且反向为红**）+ 登记表 `.rddf/checks/example-only-headers.tsv` + `--self-test` **6 条投毒**（含 2 条**必绿**：孤儿头 / 歧义 ⇒ 退出码 0）⇒ 并挂进 CI | 新增脚本 + 登记表 | AC21 | ☐ |

## 批次 E — 文档与产物等价（AC15 · AC16）

| # | 任务 | 落点 | 判据 | 状态 |
|---|---|---|---|---|
| E1 | 文档同步 + **旧路径归零**：三份 `AGENTS.md` · `docs/usage_guide/**` · `.rddf/checks/**`（新增记录）；**排除** `openspec/changes/**`（含本 change 与**仍活跃的**前置 change）· `docs/archive/**` · `docs/architecture/**` · `docs/adr/**` · `docs/reports/**` · **整份 `docs/CHANGELOG.md`** · `.rddf/improvements/**` · `.rddf/reviews/**` | 同上 | AC15 | ☐ |
| E2 | **产物等价基线**：搬前产 4 个 sha256（AXI4-Lite / SoC × `top.v` + `manifest.json`）+ 复算命令 + 前提（同 build dir / 同 emitter / 前置已完成的 HEAD）记入 `.rddf/checks/` | `.rddf/checks/` | AC16 | ☐ |
| E3 | 搬后重产 ⇒ 4 个 sha256 **相同** | diff | AC16 | ☐ |

## 批次 F — 3.2 SoC checker 机制下沉（**G-1 未闭 ⇒ 不开工**）

| # | 任务 | 判据 | 状态 |
|---|---|---|---|
| F1 | **architect 确认**（蓝图 §4.3 口径是否仍有效 / 是否允许本 change 内落地；**以实测 388 行为准**，蓝图写的 483 已过期）⇒ 落 ADR 确认或修订 | — | ☐ |
| F2 | 3 个机制函数下沉到**公开头** `include/rtl_emit/*.h`（产物派生 module 集 · 顶层端口计数 · 变异自证编排）⇒ **linter 路径作为参数**（**不下沉** `find_verilator`）⇒ ⇒ **只下沉编排，判据值留 example**；该新头 MUST 在 `example-only-headers.tsv` 走**豁免通道**登记 | AC17 | ☐ |
| F3 | `emit_rtl_soc.cpp` 退化为「组装参数 + 调用」⇒ **结构判据**：3 个符号无定义 + 新 API 调用点 == 2（**不用行数靶**） | AC17 | ☐ |
| F4 | **第二消费者**：把 `examples/riscv-mini/tests/test_rv32i_hier_rtl.cpp` 的 module 集/端口计数改为调用新 API；做不到 ⇒ 明写为带门的 follow-up | — | ☐ |
| F5 | **常驻判别力回归**：4 项判据各自注入错期望 ⇒ 必红；④″ 仍执行；退出码语义不变 | AC18 | ☐ |
| F6 | **公开面新增登记**（下沉新增公开头 / CLI 选项 ⇒ `public-api-surface.md` + `CHANGELOG`） | AC19 | ☐ |

## 复核命令

```bash
cmake --build build -j$(nproc)

# include 文本归零（AC3）
grep -rn '"axi4/axi4_lite_example_top\.h"\|"axi4/axi_soc_example_top\.h"' examples tests || echo 'OK'

# 依赖方向（AC4：C2 扩展后 + C7）
bash scripts/verify_no_duplicate_rv32i_headers.sh

# 计数（AC9 · AC10；默认配置）
ctest --test-dir build -N | tail -1
ctest --test-dir build -N -LE perf | tail -1
ctest --test-dir build -N -L base | tail -1
bash scripts/verify_test_counts.sh

# 登记面（AC11–AC14）
bash scripts/verify_port_naming.sh
bash scripts/verify_public_api_visibility.sh
bash scripts/verify_input_port_required_registry.sh
bash scripts/verify_rst_n_wiring.sh
bash scripts/verify_example_assertion_quality.sh

# 运行
bash scripts/verify_example_test_wiring.sh
bash scripts/verify_examples_rtl_sim.sh
./run_all_ported_tests.sh
```

## ⚠️ 未做 / 本次不做

| 项 | 原因 |
|---|---|
| 3.2（checker 机制下沉） | **G-1 未闭 ⇒ MUST NOT 开工**（architect 域） |
| `find_verilator()` 下沉 | 仓内已有裁决（fork 外部 lint 污染失败归因）⇒ linter 路径作为参数 |
| 给 `AxiLiteSlave` 的端口具名 | 公开面变更 ⇒ 需 architect（ADR-075 域） |
| 给 `AxiLiteTestMaster` 的 23 个端口具名 | 成层子 module 端口 ⇒ 独立立项 |
| 把 `AxiLiteTop` / `AxiSocTop` 提升为库组件 | 与本 change 方向相反 ⇒ 需 ADR |
| RV32I 侧顶层组件归位 | 独立立项 |
| 删 `emit_rtl.cpp` | 属 `emit-cli-into-example`（前序 change） |
