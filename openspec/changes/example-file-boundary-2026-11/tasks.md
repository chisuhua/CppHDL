# tasks — `example-file-boundary-2026-11`

> ⚠️ **设计不在本文件**：决策面 = ADR-074 / ADR-046 / ADR-075 + 蓝图 §4.3。
> ⚠️ **顺序**：本 change 在 `axi4-verif-driven-cpp` → `emit-cli-into-example` **之后**（G-2）。
> ⚠️ **3.2（批次 F）受 G-1 约束**：**F1 本身就是 G-1 的交付物**（architect 执行）⇒ ⇒
> F1 未闭前 **F2–F9 MUST NOT 开工**；**3.1（批次 A–E）可独立推进**。

## 批次 A — 头文件归位（AC1 · AC2 · AC3 · AC4 · AC7）

| # | 任务 | 落点 | 判据 | 状态 |
|---|---|---|---|---|
| A1 | `git mv` 两个顶层头到 `examples/axi4/`（保留历史） | `include/axi4/` → `examples/axi4/` | AC1 | ✅ |
| A2 | **2 个 examples 消费者改裸 include**：`tb_axi4_lite_example.cpp` · `tb_axi_soc_example.cpp`（由 B6 改名）⇒ ⇒ ⚠️ **2026-11-18 订正**：`emit_rtl_soc.cpp` **已从本任务移除**（§4.3.3 整文件删除 ⇒ 不再是消费者）；`emit_rtl.cpp` 已被前序 change 删除 | `examples/axi4/` | AC3 | ✅ |
| A3 | 库公开面归零核验 | `include/**` | AC2 | ✅ |
| A4 | 根 `tests/` 不再引用这两个头（由 B1–B4 完成后核验） | `tests/**` | AC7 | ✅ |
| A5 | **搬入的 2 个测试**（`examples/axi4/tests/*.cpp`）也用裸 include（靠 helper 的 `-I examples/axi4`） | `examples/axi4/tests/` | AC3 | ✅ |
| A6 | **C2 扩展（最小面 + 剥注释）**：扫描面仍只 `tests/CMakeLists.txt`，匹配串 `examples/riscv-mini/src` → `examples/`，**移植 C7 的剥注释 + 括号配平**；新增 fixtures：① 注释里出现 `examples/` ⇒ **绿** ② `target_include_directories(… examples/axi4)` ⇒ **红** ⇒ 并在 AC4 里点名 C7 一并绿 | `scripts/verify_no_duplicate_rv32i_headers.sh` | AC4 | ✅ |
| **A7** | **`axi4_full_example` 退役**（2026-11-18 · §4.3.3 裁决）：删 `examples/axi4/axi4_full_example.cpp` ⇒ 连带面 = target · `add_spinalhdl_ported_test(axi4_full …)`（CTest `spinalhdl_axi4_full`）· `CH_TRACKED_EXAMPLE_TARGETS`（**28 → 27**）· `install(TARGETS …)` 删该项 · **`.rddf/checks/examples-rtl-sim-exclusions.tsv` 删该行**（coverage 的 ghost 判据）· **`.rddf/checks/port-naming-exempt.tsv` 删 31 行 + `EXPECTED_EXEMPT_PORTS` 1415 → 1384** · `examples/axi4/AGENTS.md` + `include/axi4/AGENTS.md` 的 EXAMPLES 行 ⇒ ⇒ **覆盖证据** = `tests/test_axi4_full_burst.cpp`（dual-backend） · **MUST 显式登记为缩面**（不静默） | `examples/axi4/` · `.rddf/checks/` · 根 `AGENTS.md` | AC1 · AC11 | ✅ |

## 批次 B — 消费者归位 / 换 DUT（AC5 · AC6 · AC8 · AC9）

| # | 任务 | 落点 | 判据 | 状态 |
|---|---|---|---|---|
| B1 | `tests/test_axi_lite_verilator_e2e.cpp`：删顶层头 include ⇒ `using` 改库组件 ⇒ DUT 换 `AxiLiteSlave<32,32,4>` ⇒ 顶部注释同步（**把陈旧的「10 inputs / 7 outputs」订正为 12 / 8**）⇒ 路径 / 用例名 / SECTION / 断言**零改动** | 该文件 | AC5 | ✅ |
| B2 | 该 e2e 仍绿（verilator 在 `build/verilator-install/bin` 或 PATH） | ctest | AC6 | ✅ |
| B3 | `tests/test_axi_lite_test_master.cpp` → `examples/axi4/tests/`（**CTest 条目名不变**） | `examples/axi4/tests/` | AC9 | ✅ |
| B4 | `tests/test_verif_single_source.cpp` **按准则拆分**：凡以 `ch_device<axi4::example::AxiLiteTop>` / `LateAxiLiteTop` 为 DUT 的用例 → `examples/axi4/tests/test_verif_axi4_lite.cpp`；其余（扫描器用例）留 `tests/` ⇒ **共享 helper 一并搬**（`verif_path()` / `LateAxiLiteTop` / `read_file()`） | 两处 | AC8 | ✅ |
| B5 | **TEST_CASE 名集合守恒（判据）**：拆分前后差集 == ∅（源码解析或 `ctest -N` 对照）⇒ 结果记录在 note | 记录 | AC8 | ✅ |
| **B6** | **`axi_soc_example.cpp` → `tb_axi_soc_example.cpp`**（2026-11-18 · §4.3.3）：`git mv` 改名（类已住 `axi_soc_example_top.h` ⇒ 文件只剩 main）⇒ ① 删 `toVerilog("axi_soc_example.v")`（CWD 落盘）② 补 **`RESULT: checks=N fails=0`** 协议行（**N 由 C++ 侧自定，不要求与 `.sv` 的 14 对齐**）③ 补 **`--emit-rtl <dir>`** 模式（发射）④ 补 **`--check-products`** 模式（= 发射 + 自检，内部等价 `--emit-rtl --hierarchical --check-products`；该模式下 `--out` / `--emit-hygiene` **照常透传**）⇒ ⑤ CMake target 改名（`add_spinalhdl_ported_test(axi4_soc tb_axi_soc_example)` ⇒ **CTest 条目名 `spinalhdl_axi4_soc` 不变**）· `CH_TRACKED_EXAMPLE_TARGETS` · `install(TARGETS …)` · tsv SoC 行四列 · **`.rddf/checks/input-port-required-registry.md:45` 的 `AxiSocTop` 用例路径** · **`examples/axi4/AGENTS.md` FILES 行** · **`include/axi4/AGENTS.md` EXAMPLES 行** ⇒ ⇒ ⚠️ **手写激励 = 过渡态**（R13：UART 8N1 帧解码超出 `.verif` 封闭原语集 ⇒ 独立立项） | `examples/axi4/` · `.rddf/checks/` | AC1 · AC17 · AC22 | ✅ |

## 批次 C — 构建与锚点（AC8 · AC9 · AC10 · AC20）

| # | 任务 | 落点 | 判据 | 状态 |
|---|---|---|---|---|
| C1 | `examples/axi4/CMakeLists.txt` 增 `axi4_catch_test` helper：链 `catch_amalgamated` + include 集（`include` · `tests` · `examples/axi4`）+ `add_test` + **`LABELS base`** + `TIMEOUT 300` + **`CPPHDL_SOURCE_DIR` 编译定义** | 同上 | AC8 · AC9 | ✅ |
| C2 | 根 `tests/CMakeLists.txt` 去掉迁走的注册项（**不得**重复注册） | `tests/CMakeLists.txt` | AC9 | ✅ |
| C3 | **锚点按实测回填**：⚠️ 2026-11-18 订正 = 原 `+1`（拆分新增）**− 1**（axi4_full 退役）= **净 0** ⇒ ⇒ `CTEST_TOTAL` **268 → 268** · non-perf **257 → 257** · `-L base` **231 → 231** · `EXAMPLES_TEST_SOURCES` 18 → **20** ⇒ ⇒ **MUST 以实测回填**（上面是推导） | 根 `AGENTS.md` | AC10 | ✅ |
| C4 | **手工改 `tests/test_*.cpp` 计数行**：现写 **145** 但实测 **159** ⇒ 搬走 1 个后为 **158**（**MUST 用实测值，禁「原值 −1」**） | 根 `AGENTS.md` | AC10 | ✅ |
| C5 | **登记 `CPPHDL_BUILD_EXAMPLES=OFF` 的接受项**（被搬走的 2 个条目消失 ⇒ 锚点只在默认配置成立）⇒ 写在该锚点旁 + 指向本 change | 根 `AGENTS.md` / `.rddf/checks/` | AC20 | ✅ |

## 批次 D — 登记面（AC11 · AC12 · AC13 · AC14）

| # | 任务 | 落点 | 判据 | 状态 |
|---|---|---|---|---|
| D1 | **棘轮豁免表「两段」（2026-11-18 拆分）**：① 3.1 = **23 行只改第一列路径**（**不删行**）⇒ 锚点仍 **1415** ② A7 = **删 `axi4_full_example` 的 31 行** ⇒ 锚点 **1415 → 1384** ⇒ ⇒ 两段 MUST **分开跑** | `.rddf/checks/port-naming-exempt.tsv` | AC11 | ✅ |
| D2 | **公开面移除登记 5 条**：2 条 `nofile`（两个头的 glob）+ 3 条 `removed`（类名 · `scope=include/`）⇒ 同步 `EXPECTED_PROBES` 46 → **51**、`EXPECTED_REMOVAL_ROWS` 25 → **30**（登记行 MUST 用 `**移除**`）⇒ `public-api-surface.md` 同章逐条登记 | `.rddf/checks/public-api-removals.tsv` · `public-api-surface.md` | AC12 | ✅ |
| D3 | **输入端口登记表的 3 行保留**：把 `input_port_registry.py` 的 `header_of()` 搜索根从 `include/**` 扩到 `include/**` + `examples/**`（否则 R1 报「登记过时」）⇒ 只更新这 3 行的**消费者路径**列 | `scripts/lib/input_port_registry.py` + 登记表 | AC13 | ✅ |
| D4 | **零缩面核对**：`verify_rst_n_wiring.sh` 的 `COMPONENTS_W_RST_N` 组件数 **搬家前后相同**（3 个组件仍在表内）⇒ 若数量变化 ⇒ 说明走了「删行」路线 ⇒ 回退 | `scripts/verify_rst_n_wiring.sh` | AC13 | ✅ |
| D5 | `CH4_ANCHOR` **+2 行**（两个新 `examples/axi4/tests/*.cpp` 的断言数下限） | `scripts/verify_example_assertion_quality.sh` | AC14 | ✅ |
| D6 | **新建 capability 的护栏**：`scripts/verify_example_only_headers.sh`（末段匹配 · 作用域 `include/ src/ tests/ examples/ tools/` · 三态区分孤儿/歧义/违规 · 登记表双向**且反向为红**）+ 登记表 `.rddf/checks/example-only-headers.tsv` + `--self-test` **6 条投毒**（含 2 条**必绿**：孤儿头 / 歧义 ⇒ 退出码 0）⇒ 并挂进 CI | 新增脚本 + 登记表 | AC21 | ✅ |

## 批次 E — 文档与产物等价（AC15 · AC16）

| # | 任务 | 落点 | 判据 | 状态 |
|---|---|---|---|---|
| E1 | 文档同步 + **旧路径归零**：三份 `AGENTS.md` · `docs/usage_guide/**` · `.rddf/checks/**`（新增记录）；**排除** `openspec/changes/**`（含本 change 与**仍活跃的**前置 change）· `docs/archive/**` · `docs/architecture/**` · `docs/adr/**` · `docs/reports/**` · **整份 `docs/CHANGELOG.md`** · `.rddf/improvements/**` · `.rddf/reviews/**` | 同上 | AC15 | ✅ |
| E2 | **产物等价基线**：搬前产 4 个 sha256（AXI4-Lite / SoC × `top.v` + `manifest.json`）+ 复算命令 + 前提（同 build dir / 同 emitter / 前置已完成的 HEAD）记入 `.rddf/checks/` | `.rddf/checks/` | AC16 | ✅ |
| E3 | 搬后重产 ⇒ 4 个 sha256 **相同** | diff | AC16 | ✅ |

## 批次 F — 3.2 SoC checker 机制下沉（**F1 = G-1 的交付物 · architect 执行 ⇒ 未闭前 F2–F9 不开工**）

| # | 任务 | 判据 | 状态 |
|---|---|---|---|
| F0 | **开工前准备（不受 G-1 约束 ⇒ 可与 F1 并行；MUST 在任何代码改动之前完成）**：① **采基线**：MUST 在本 change 任何代码改动之前、且 MUST 在 `emit-cli-into-example` **之后**（SoC 入口形态已变）⇒ **分模式两套**（默认模式 / `--check-products` 模式）+ 比较面 = 该 `--out` 目录**全部落盘物**（含 `eq/`）+ 记录 verilator 可用性 ⇒ 前提 = 同 build 配置 + **变量集（2026-11-18 扩为多变量）** = `emit_rtl_soc.cpp`（删）+ `axi_soc_example.cpp`（删/改名）+ `tb_axi_soc_example.cpp`（新增）+ `axi4_full_example.cpp`（退役）⇒ ⇒ **基线 MUST 在批次 F 代码改动前采集**（单次采集，否则无法归因）② **订正上游活跃 change** `rtl-verification-single-source-2026-11` 的同款陈旧目标（`proposal.md` 的 `:21` / `:34` / `:46` + `tasks.md:29`）⇒ ⇒ 该 change **尚未归档** ⇒ **不是**「历史记录」豁免 ⇒ ⇒ ✅ **已于 2026-11-18 同批订正**（planner 侧）⇒ ⇒ 本条**只剩复核**（AC23 的 grep） ⚠️ **draft 活文档**（`.rddf/improvements/rtl-verification-single-source-2026-11.md`）**已于 2026-11-18 同批订正**（planner 侧）⇒ 本条**不再包含**它 | AC23 · AC25 | ✅ |
| F1 | ✅ **architect 裁决已产出（2026-11-18）** —— 蓝图 §4.3 订正 + §4.3.1 逐符号切分表（9 符号 · 维度 = **规则 vs 值**）+ §4.3.2 形态裁决（**分支 A**）+ §7 R12 ⇒ ⇒ **builder 只验证**（AC22 / AC23）⇒ ⇒ **MUST NOT** 由 builder 代改蓝图（architect 写集） | AC22 · AC23 | architect 交付 |
| F2 | **按蓝图 §4.3.1 的切分表下沉**到**公开头** `include/rtl_emit/*.h`（实现落 `src/rtl_emit/*.cpp`）⇒ ⇒ **下沉集 = 完整 2 个（`product_modules` · `tamper_hier`）+ 拆分 1 个（`check_products` 的规则侧）** ⇒ ⇒ ⚠️ **该头的实现 TU 必然 include 它 ⇒ 按 R2 定义不算「消费者」**（实现 ≠ 使用）⇒ ⇒ 故 R2 反向判据不会误红 ⇒ ⇒ **linter 路径作为参数**（**不下沉** `find_verilator`）⇒ ⇒ **只下沉规则，值留 example**（ADR-074「不覆盖」：`kExpectedModules` · `kExpectedTopPorts` · `make_inputs` · `make_observations` · `make_stimulus` **+ 根组件名 · lint 的 `--top-module` 名 · 最小变化观测量阈值**）⇒ ⇒ **CHECK 3 的 lint 规则**归库层（工具发现留 example）⇒ ⇒ ⚠️ **库层 MUST NOT 内置字面根名（`"top"`）/ MUST NOT 内置 `>= 2` 之类阈值** ⇒ ⇒ 一律**参数传入** | AC17 | ✅ |
| F3 | **`emit_rtl_soc.cpp` 整文件删除**（2026-11-18 · §4.3.3 取代原「壳层退化」）⇒ **结构判据**：被裁下沉的符号在 example 侧**无定义**（⚠️ 拆分符号 `check_products` 判「**规则侧实现** MUST NOT 出现在 example 侧」⇒ 禁断言「无 `check_products` 定义」）+ **库层公开函数 == 3**（`product_modules` · `tamper_hier` · `check_products` 规则侧）且 example 侧**各调用 == 1**（**不用行数靶**）⇒ ⇒ 发射由 **`tb_axi_soc_example --emit-rtl`** 承担；`--check-products` = 「发射 + 自检」模式标记（内部等价 `--emit-rtl --hierarchical --check-products`）⇒ 该模式下 `--out` / `--emit-hygiene` MUST 照常透传⇒ ⇒ ⚠️ example 侧产品专属 helper（`make_inputs` / `make_observations` / `check_products` 的判据值）**无论是否被 `main` 直接调用，一律归 example**（归属以蓝图 §4.3.1 为准） | AC17 | ✅ |
| F4 | **第二消费者**：把 `examples/riscv-mini/tests/test_rv32i_hier_rtl.cpp` 的 module 集/端口计数改为调用新 API；做不到 ⇒ 明写为带门的 follow-up ⇒ ⇒ ⚠️ **`tamper_hier` 的语义归一化另立项**（SoC = 由观测信号反查子端口 → 取反 RHS；RV32I = 硬编码 3 条 `assign` 名 + RHS 置 `'0'` ⇒ **语义不同**）⇒ ⇒ 本 change **只做 `product_modules`** 的归一化 | AC17 | ☐ |
| F5 | **常驻判别力回归**：4 项判据各自注入错期望 ⇒ 必红；④″ 仍执行；退出码语义不变 ⇒ ⇒ ⚠️ **已知边界**：`emit_rtl_soc.cpp:263-273` 的 CHECK 3 在 verilator 不可用时打印 `SKIP` 且**不计 failures** ⇒ ⇒ 基线记录 MUST 含 verilator 可用性 / 版本 | AC18 | ✅ |
| F6 | **公开面登记（真载体）**：新增公开头 MUST 进 **`.rddf/checks/example-only-headers.tsv` 豁免通道** + `docs/CHANGELOG.md` 一行 ⇒ ⇒ ⚠️ **实现 TU 的 include 不算「消费者」**（R2 定义）⇒ ⇒ 该形态**可登记** ⇒ ⇒ ⚠️ **如实声明**：`public-api-surface.md` 的扫描范围 = **顶层 `include/*.h`** + 两个已裁决 core 头 ⇒ ⇒ `include/rtl_emit/*.h` 属**子目录头 ⇒ 不在扫描范围** ⇒ ⇒ 登记到那里**零判别力**（只作记录，不构成护栏）⇒ ⇒ 该事实 MUST 写进登记条目本身 | AC19 | ✅ |
| F7 | **产物形态（分支 A · 已裁决）**：SoC 发射入口**恒发层次化** ⇒ ⇒ 护栏 ③ 对 `kind=soc` 行的**发射语义 MUST 同步**（它现在发扁平产物）+ 产物形态 **MUST 登记** ⇒ ⇒ ⚠️ **护栏 ⑧' MUST 扩到 `kind=soc`**（现仅 `kind=hier`）⇒ **且 soc 行 MUST 有负向对照**（投毒 ⇒ 必红）⇒ ⇒ ⚠️ **载体**：self-test 清单**当前无 `kind=soc` fixture**（`REGISTERED_SOC` 锚点 = 0）⇒ ⇒ MUST **新增一条 soc fixture 行** **且** 把锚点 **0 → 1**（否则「⓪ SoC 桶不守恒」会红）⇒ ⇒ **CHANGELOG MUST 有一行** ⇒ ⇒ ⑧ 在层次化产物上复核（实测成立 · 工具 = **集合 + 顺序双重**判据 ⇒ 「逐字逐序」措辞**准确** ⇒ 勿弱化为只比集合）⇒ ⇒ ⚠️ **③ 无需改代码**（默认即层次化 ⇒ 变的只是**产物形态** ⇒ 但产物形态 MUST 登记） | AC24 | ✅ |
| F8 | **产物等价核对**：用 F0 采的基线逐项比对（分模式两套 + 全部落盘物）⇒ ⇒ **分支 A 例外**：默认模式的差异 MUST **恰为形态变化**且**可机器判定**（落盘物集合 == {`top.v`, `manifest.json`} ∧ `manifest.json` 逐字节相同 ∧ module 集 1 → 5 ∧ `top.v` 的顶层端口表与基线扁平产物逐字逐序相同）；`eq/` 与 `--check-products` 模式 MUST **零差异** ⇒ ⇒ **禁**「逐字节相同」一刀切（那会让分支 A 必红）⇒ ⇒ **改动过产物 ⇒ 必红** | AC25 | ✅ |
| F9 | **切分表为空的合法终态**（⚠️ **本轮裁决未触发** —— 蓝图 §4.3.1 已选出「完整 2 + 拆分 1」⇒ ⇒ 本条仅在切分表**最终**为空时生效）：⇒ ⇒ 本批次零代码改动 ⇒ ⇒ 实质结论 MUST 由 **F1** 在 **2026-11-17 蓝图 §7「已知残留与边界」** 留一条**已收口**残差（闭口证据 = **存在的路径** · 逗号分隔 · 无空格 · 无反引号）⇒ ⇒ ⚠️ **不是** `.rddf/checks/arch-residual-registry.md`：那本护栏的台账**硬编码**为**另一本**蓝图（`scripts/lib/ledger_residual.py:17`）⇒ ⇒ 往它加行**必红**（「结算行数不一致」）⇒ ⇒ 本条判据 = **蓝图 §7 段落自证** | AC26 | ☐ |
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

## 复核命令 —— 批次 F（**仅 G-1 闭后执行**）

```bash
BP=docs/architecture/2026-11-17-rtl-verification-single-source.md

# AC17 —— 被下沉的符号在 example 侧无定义（示例：product_modules）
# ⚠️ 2026-11-18：emit_rtl_soc.cpp 整文件删除 ⇒ 检查面 = 新载体（tb）里无定义
if grep -qE '(^|[^A-Za-z0-9_])product_modules[[:space:]]*\(' examples/axi4/tb_axi_soc_example.cpp 2>/dev/null; then
  echo 'FAIL: tb 侧仍有 product_modules 的定义'; exit 1
fi
echo 'OK: example 侧无定义'

# AC22 —— 切分表覆盖 9 个符号（**限定在蓝图 §4.3 段内** · 命中表格行 · **带退出码**）
SEC=$(sed -n '/^### 4.3/,/^### 4.4/p' "$BP")
for s in kExpectedModules kExpectedTopPorts product_modules find_verilator \
         make_inputs make_observations make_stimulus tamper_hier check_products; do
  n=$(printf '%s\n' "$SEC" | grep -c "^| .*$s")
  if [ "$n" -lt 1 ]; then
    echo "FAIL: §4.3 段缺 $s 的切分表行"; exit 1
  fi
done
if printf '%s\n' "$SEC" | grep -q '待定'; then
  echo 'FAIL: 切分表含「待定」'; exit 1
fi
echo 'OK: 切分表 9 符号齐备且无待定'

# AC22 —— 9 个符号的**新落点**可定位（2026-11-18：emit_rtl_soc.cpp 整文件删除）
# 值侧 6 项（含不下沉的 find_verilator）→ examples/axi4/tb_axi_soc_example.cpp
for s in kExpectedModules kExpectedTopPorts find_verilator \
         make_inputs make_observations make_stimulus; do
  n=$(grep -cE "(^|[^A-Za-z0-9_])${s}([[:space:]]*\\(|[[:space:]]*=)" examples/axi4/tb_axi_soc_example.cpp || true)
  if [ "$n" -lt 1 ]; then
    echo "FAIL: tb 侧定位不到 $s"; exit 1
  fi
done
# 规则侧 3 项 → 库层（include/rtl_emit/ + src/rtl_emit/）
for s in product_modules tamper_hier check_products; do
  n=$(grep -rcE "(^|[^A-Za-z0-9_])${s}([[:space:]]*\\(|[[:space:]]*=)" include/rtl_emit src/rtl_emit 2>/dev/null | awk -F: '{s+=$2} END{print s+0}')
  if [ "$n" -lt 1 ]; then
    echo "FAIL: 库层定位不到 $s"; exit 1
  fi
done
echo 'OK: 9 个符号在新落点均可定位'

# AC23 —— 陈旧表述清零（**按位置**限定；豁免 = 蓝图 §5.1 的估算段 ⇒ 不做全文一刀切）
if sed '/^### 5.1/,/^### 5.2/d' "$BP" | grep -q '483\|~60\|415-483'; then
  echo 'FAIL: 蓝图仍有陈旧表述（483 / ~60 / 415-483）'; exit 1
fi
if grep -q '483\|~60\|415-483' .rddf/improvements/rtl-verification-single-source-2026-11.md; then
  echo 'FAIL: draft 活文档仍有陈旧表述'; exit 1
fi
# ⚠️ **上游活跃 change**（未归档 ⇒ 不是历史记录）同样 MUST 零命中
if grep -rq '483\|~60\|415-483' openspec/changes/rtl-verification-single-source-2026-11/; then
  echo 'FAIL: 上游活跃 change 仍有陈旧表述'; exit 1
fi
echo 'OK: 陈旧表述清零'

# AC24 —— 形态（分支 A：默认模式恒层次化 ⇒ module 数 == 5 = 4 成层 IP + 根 top）
d=$(mktemp -d /tmp/opencode/soc_flat.XXXXXX)
./build/examples/axi4/emit_rtl_axi_soc --out "$d" --emit-hygiene
if test "$(grep -c '^module ' "$d/top.v")" -eq 5; then
  echo 'OK: 默认模式恒层次化'
else
  echo "FAIL: 默认模式 module 数 = $(grep -c '^module ' "$d/top.v")（期望 5）"; exit 1
fi

# AC26 —— 切分表为空的终态 ⇒ 登记在**蓝图 §7**（不是 arch-residual-registry：其台账硬编码另一本蓝图）
# ⚠️ **仅 F9 触发时执行**（F9 未触发 ⇒ 本块**不适用** ⇒ 不得当必红项）
if sed -n '/^## 7\./,$p' "$BP" | grep -q '§4.3 行 1'; then
  echo 'OK: 蓝图 §7 已登记该终态残差'
else
  echo 'FAIL: 蓝图 §7 未登记该终态残差'; exit 1
fi
```

## ⚠️ 未做 / 本次不做

| 项 | 原因 |
|---|---|
| 3.2（checker 机制下沉） | **G-1 未闭 ⇒ MUST NOT 开工**（architect 域）⇒ ⇒ 其中 **F1 本身**就是 G-1 的交付物（architect 执行）⇒ ⇒ **MUST NOT** 由 builder 代改蓝图 |
| `find_verilator()` 下沉 | 仓内已有裁决（fork 外部 lint 污染失败归因）⇒ linter 路径作为参数 |
| **默认模式扁平产物的合规化**（分支 B） | 分支 B = 改 `rtl-soc-output` 既有 MUST ⇒ ADR 修订 + MODIFIED ⇒ **不在本 change 实施面** |
| **module 集 / 顶层端口数的多载体收敛** | 现状 3 处载体（`kExpectedModules` / `kExpectedTopPorts` · `soc-rtl-expectations.md` · tsv 散文）⇒ ⇒ ADR-074「判据值留 example」⇒ example 侧常量 MUST NOT 被删 ⇒ ⇒ **登记为已知残留** ⇒ 独立立项 |
| 给 `AxiLiteSlave` 的端口具名 | 公开面变更 ⇒ 需 architect（ADR-075 域） |
| 给 `AxiLiteTestMaster` 的 23 个端口具名 | 成层子 module 端口 ⇒ 独立立项 |
| 把 `AxiLiteTop` / `AxiSocTop` 提升为库组件 | 与本 change 方向相反 ⇒ 需 ADR |
| RV32I 侧顶层组件归位 | 独立立项 |
| 删 `emit_rtl.cpp` | 属 `emit-cli-into-example`（前序 change） |

## 实施实况 —— 批次 F（F2 · 2026-11-18）

| 项 | 实况 |
|---|---|
| **F2 已落地** | 新公开头 `include/rtl_emit/product_check.h` + `src/rtl_emit/product_check.cpp`（`product_modules` · `tamper_hier` · `ProductCheckSpec/Hooks` · `check_products`）。**编排整体下沉**（4 项顺序 / `failures` 累加 / `eq/` / 结论行 / 退出码 0-1）。**值留 example**（期望 module 集 · 端口数 · 激励面 · 观测面 · 变异目标 · `--top-module` 名 · 阈值）。`tb_axi_soc_example.cpp` 543 → **388 行** |
| **owner 基准的取值方式（偏离字面「作参数传入」）** | 库层**从建出来的根组件派生**（`Component::name()`）⇒ ⇒ 比「调用方传一个字面根名」更强：根名**不可能**与实例不一致。根名常量 `kSocRootName` / `kLiteRootName` 落在**设计头**（= example 层）⇒ ⇒ `include/rtl_emit/**` 内**零**字面根名 |
| **`tamper_hier` 按 AC17 下沉** | 蓝图表明确要求沉（「第二消费者语义不同」已如实标注）⇒ ⇒ 采纳 AC17；metis 的「暂不沉」建议与 AC17 冲突 ⇒ **以 AC17 为准** |
| **`find_verilator` 不下沉，但 lint 判据下沉** | 库层收 `spec.lint` 回调（跑 linter ⇒ 返回是否干净）⇒ ⇒ 库层负责**判定与计数**、example 负责**工具发现与执行**（POSIX 进程 API 不进库层 ⇒ 保 Windows/macOS 可编译）。**回调缺失 ⇒ 按失败计**（fail-closed） |
| **⚠️ CHECK 3 语义改判（有意偏离 AC18 的「已知边界」描述）** | 搬移前：verilator 不可用 ⇒ 打印 `SKIP` 且**不计失败** ⇒ ⇒ 实测「从 `build/` 跑必 SKIP」⇒ ⇒ lint **从未被验**却显示绿灯。⇒ ⇒ 改判 **FAIL（计失败）** ⇒ ⇒ 与 AC18 的「行为逐字保持」**有意不同**，理由 = 原行为是 **fail-open 缺陷**（AC18 那条「已知边界」的措辞需 planner 订正：它把 SKIP 写成了合法语义） |
| **F4（第二消费者归一化）→ follow-up** | AC17 文中的 `examples/riscv-mini/tests/test_rv32i_hier_rtl.cpp` **不存在**（`ls` 实测）⇒ ⇒ 现存第二消费者 = `tests/test_hierarchy_verilog.cpp` 的 `count_occurrences(all,"module ")`（**内存字符串**，非文件）+ 护栏 ⑧'（**产物文件**）⇒ ⇒ 与 `product_modules(path)` 的入参形态不同 ⇒ 不是 drop-in ⇒ 记 follow-up（AC17 亦声明「第二消费者是加分项、不是门槛」） |
| **F5 判别力** | `tests/test_product_check.cpp`（10 用例 / 27 断言）**逐判据**断言（捕获 stdout ⇒ 断言具体 CHECK 行）⇒ ⇒ 变异实测：M1 `product_modules` 去行首约束 / M2 退出码 fail-open / M3 `tamper_hier` 搜索串退化 / M4 CHECK 2 恒 PASS / M5 CHECK 3 退回 SKIP ⇒ ⇒ **5 条各自转红**，还原复绿 |
| **F6** | `include/rtl_emit/product_check.h` 已登记进 `.rddf/checks/example-only-headers.tsv` 豁免通道（38 条之一）+ `docs/CHANGELOG.md` 一行。⚠️ `public-api-surface.md` 的扫描面 = 顶层 `include/*.h` + 两个已裁决 core 头 ⇒ ⇒ 子目录头**不在扫描面**（登记到那里零判别力）⇒ ⇒ 只登记在 example-only 表 |
| **F0（基线）** | 两套基线均已落盘：默认模式 = `.rddf/checks/product-equivalence-header-move-2026-11-18.md`（搬移前 4 件 sha256）· `--check-products` 模式 = `.rddf/checks/product-check-equivalence-2026-11-18.md`（含 verilator 可用性前提与复算命令）⇒ ⇒ 基线二进制由 **F2 代码改动前**的源码（`21a74ad` 的 tb）单独编译 |
| **F7/F8** | SoC 发射入口恒 `--hierarchical`（已实测 5 module）· 产物**逐字节等价**（4 件 sha256 相同）⇒ 记录 = `.rddf/checks/product-check-equivalence-2026-11-18.md` |
| **F9** | **不适用**（切分表非空 ⇒ 该条只在「最终为空」时生效） |
| **D6 已落地** | `scripts/verify_example_only_headers.sh`（三态 + 登记表双向 + 数量锚点 `EXPECTED_EXEMPT_HEADERS=38` + `--self-test` **6 条投毒**）+ `.rddf/checks/example-only-headers.tsv`（38 条）+ 挂进 CI（2 步） |

### ⚠️ 需 planner 订正的 AC 措辞（builder 不改 AC）

| AC | 问题 | 建议 |
|---|---|---|
| **AC17** | 「**example 侧对每个的调用 == 1**」与「**编排整体下沉**」（蓝图 §4.3.1 钉死）**不自洽**：编排下沉后 `product_modules` / `tamper_hier` 由**库层内部**调用 ⇒ example 侧只调 `check_products` **1 次**（另两个为 0） | 改为「**库层实现 TU 内**每个公开函数恰 1 处调用 + example 侧对 `check_products` 恰 1 处调用」 |
| **AC18** | 「`emit_rtl_soc.cpp` 的 CHECK 3 在 verilator 不可用时打印 `SKIP` 且不计 failures」被写成**合法语义**（「行为不变」）⇒ ⇒ 那其实是 **fail-open 缺陷** | 改为「CHECK 3 NEVER 静默跳过：linter 缺失 ⇒ 按失败计（fail-closed）」 |
