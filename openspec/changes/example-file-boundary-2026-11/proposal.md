# Proposal: example 文件职责边界 —— 顶层组件归位 + SoC checker 机制下沉（`example-file-boundary`）

- **状态**: **立项（2026-11-18 · planner）** · 未开工 · **metis 第 1 轮已处置（P0×9 / P1×4 / P2×2）**
- **前置**: `rtl-verification-single-source-2026-11`（它把两个头抽到 `include/` ⇒ 本 change 是其收口）
- **顺序**: 位于 `axi4-verif-driven-cpp` → `emit-cli-into-example` **之后**（见「🚪 门 G-3」）
- **upstream**: ADR-074（Accepted）· ADR-046（C2/C3/C7 · 依赖方向）· ADR-075（端口具名）

---

## Why

### 3.1 顶层组件住在 `include/`，但它是**示例专用**的

`include/axi4/axi4_lite_example_top.h`（`AxiLiteTop`）与 `include/axi4/axi_soc_example_top.h`
（`AxiSocTop` + `AxiLiteTestMaster`）只服务于 example ⇒ ⇒ 而 `include/` 是**库**公开面
⇒ ⇒ 用户的直觉成立：**它们更该住在 `examples/axi4/`**。

⇒ ⇒ **当时为什么只能抽到 `include/`**：`tests/**` 曾复制一份 `AxiLiteTop`（双权威）
⇒ ⇒ 消除复制的唯一合规解 = 抽到两处都能看到的地方；而 C3 禁「根 `tests/**` include `examples/**`」
⇒ ⇒ `include/` 是**当时**唯一合规落点。

⇒ ⇒ **现在拆得掉**，因为消费者只有 7 处（逐条已核）：

| 消费者 | 归位后 |
|---|---|
| `examples/axi4/tb_axi4_lite_example.cpp`（`axi4-verif-driven-cpp` 改名后）· `examples/axi4/axi_soc_example.cpp` · `examples/axi4/emit_rtl_soc.cpp` | **include 文本 MUST 改为裸文件名**（见下「事实订正」） |
| `tests/test_axi_lite_verilator_e2e.cpp:42` | **换 DUT** = 库组件 `AxiLiteSlave<32,32,4>`（端口集合与成员名逐字相同 ⇒ 断言零改动） |
| `tests/test_axi_lite_test_master.cpp:19` → `examples/axi4/tests/` | 移入后 include 改裸名（helper 注入 `-I examples/axi4`） |
| `tests/test_verif_single_source.cpp:15` 的 DUT 用例 → `examples/axi4/tests/test_verif_axi4_lite.cpp` | 同上 |

⚠️ `examples/axi4/emit_rtl.cpp` **不在本表**：它由前序 `emit-cli-into-example` 删除 ⇒ 本 change 执行时已不存在。

#### ⚠️ 事实订正（metis P0-1）

**「quoted include 的自身目录优先 ⇒ examples 内零改动」是错的**：消费者写的是
`#include "axi4/axi4_lite_example_top.h"`（**子路径**）⇒ ⇒ 自身目录搜的是
`examples/axi4/axi4/…`（不存在），`-I include` 也找不到 ⇒ ⇒ **4 个消费者 MUST 改 include 文本为裸文件名**。
（`examples/axi4/*.cpp` 的 `target_include_directories` 已含仓库根，改裸名后靠「自身目录优先」解析；
`examples/axi4/tests/*` 需要 helper 注入 `-I examples/axi4`。）

⇒ ⇒ **为什么 `test_axi_lite_verilator_e2e.cpp` 能换 DUT**：`AxiLiteTop` 是
`AxiLiteSlave<32,32,4>` 的**纯透传**封装（`axi4_lite_example_top.h:62-90`）⇒ ⇒ 换后端口集合相同、
C++ 成员名相同 ⇒ ⇒ 路径 / TEST_CASE 名 / SECTION / 断言**全不变**（该测试只**数**端口个数，不按名取）。

⇒ ⇒ **为什么 `test_verif_single_source.cpp` 的 DUT 用例不能换 DUT**：IR 解释器的端口绑定链
（`src/rtl_emit/verif_run.cpp:66-81`）对 `unnamed_input` **fail-closed**，而 `AxiLiteSlave` 的端口
**未具名** ⇒ ⇒ 必须与具名 DUT 同处 `examples/`（先例：`examples/riscv-mini/tests/`）。

### 3.2 `emit_rtl_soc.cpp` 的 checker **机制**仍自实现

`examples/axi4/emit_rtl_soc.cpp`（388 行）持有 SoC 产品级 checker。**双形态逐拍等价已走公共 helper**
`ch::hier_eq`（`:290`/`:317`）⇒ ⇒ 自实现的只有 **3 个机制函数 + 编排**（`:53` `product_modules` ·
`:218` `check_products` · `:162-217` 变异自证），以及 `:67` 的 `find_verilator()`。

⚠️ **`find_verilator()` 不下沉**：仓内已有裁决 ——
`examples/riscv-mini/tests/test_rv32i_hier_rtl.cpp:21-22` 明写「lint 门不在这里；测试里 fork 外部
lint 会让失败原因与 TIMEOUT 归因混乱」⇒ ⇒ 下沉工具发现 = 把被否决的形态升格为库能力
⇒ ⇒ **linter 路径 MUST 作为参数传入**。

⚠️ **ADR-074 的「不覆盖」是范围内声明**（蓝图 §4.3 已给出「变薄」形态 + 治理义务）
⇒ ⇒ 本批次仍需 architect **确认口径**（见 G-1），但**不是**问一个答案未知的问题。

---

## What Changes

### 3.1 顶层组件归位（**可立即实施**）

| # | 变更 |
|---|---|
| **A1** | `git mv` `include/axi4/axi4_lite_example_top.h` → `examples/axi4/` |
| **A2** | `git mv` `include/axi4/axi_soc_example_top.h` → `examples/axi4/` |
| **A3** | 4 个 examples 消费者的 include **改裸文件名** |
| **A4** | `tests/test_axi_lite_verilator_e2e.cpp`：删顶层头 include ⇒ DUT 换 `AxiLiteSlave<32,32,4>` ⇒ 顶部注释同步（**含把陈旧的「10 inputs / 7 outputs」订正为 12 / 8**）⇒ 路径 / 用例名 / SECTION / 断言**零改动** |
| **A5** | `tests/test_axi_lite_test_master.cpp` → `examples/axi4/tests/`（**CTest 条目名不变**） |
| **A6** | `tests/test_verif_single_source.cpp` **按准则拆分**（不是按计数）：凡以 `ch_device<axi4::example::AxiLiteTop>` / `LateAxiLiteTop` 为 DUT 的用例 → `examples/axi4/tests/test_verif_axi4_lite.cpp`；其余留 `tests/` |
| **A7** | `examples/axi4/CMakeLists.txt` 增 `axi4_catch_test` helper（镜像 `riscv_mini_catch_test`：链 `catch_amalgamated` + `-I examples/axi4` + `add_test` + `LABELS base` + `TIMEOUT 300` + **`CPPHDL_SOURCE_DIR` 编译定义**） |
| **A8** | **三个登记面**：① `port-naming-exempt.tsv` 23 行只改第一列路径 ② **`public-api-removals.tsv` 新增 5 条探针**（2 `nofile` + 3 `removed`）+ 同步 `EXPECTED_PROBES` / `EXPECTED_REMOVAL_ROWS`（数值见 AC12）③ **`input-port-required-registry.md` 的 3 行保留**，改为**扩展 `input_port_registry.py` 的 `header_of()` 搜索根到 `examples/**`**（现只搜 `include/`）⇒ ⇒ 登记表仍是唯一真相源、`verify_rst_n_wiring.sh` 的 per-TEST-CASE 覆盖**不缩面** |
| **A9** | **rst_n 覆盖零缩面**：`COMPONENTS_W_RST_N` 从登记表读取 ⇒ ⇒ 因为 A8 ③ 选择「扩 `header_of()` 搜索根」而非「删登记行」，3 个组件**仍留在表内** ⇒ `verify_rst_n_wiring.sh` 的 per-TEST-CASE 覆盖**不变**（⇒ 本次**不**需要组件数锚点；若将来真缩面 ⇒ 那时再加锚点） |
| **A10** | **`CH4_ANCHOR` +2 行**（`verify_example_assertion_quality.sh`：`examples/**/tests/*` 的每个源 MUST 有断言数下限，缺行即 violation） |
| **A11** | 文档：`include/axi4/AGENTS.md`（顶层头段落删除）· `examples/axi4/AGENTS.md`（新位置）· 根 `AGENTS.md` · `docs/CHANGELOG.md` |
| **A12** | **产物逐字节不变**：搬前/搬后 `top.v` + `manifest.json` 的 sha256 MUST 相同（记录在 `.rddf/checks/`） |

### 3.2 SoC checker 机制下沉（**🚪 G-1 未闭 ⇒ MUST NOT 开工**）

| # | 变更 |
|---|---|
| **B1** | 3 个机制函数下沉到 `src/rtl_emit/`（产物派生 module 集 · 顶层端口计数 · 变异自证编排）⇒ ⇒ SoC 专属期望值 / 激励面 / 观测面 / 变异目标改为**参数**；**linter 路径由调用方传入**（不下沉 `find_verilator`）⇒ ⇒ ⚠️ **只下沉「编排」**，`--check-products` 的 4 项**判据值**属产品专属期望（ADR-074 的「不覆盖」）⇒ ⇒ MUST NOT 搬进库 |
| **B2** | `emit_rtl_soc.cpp` 退化为「组装参数 + 调用」⇒ ⇒ **结构判据**（零自实现机制函数 + 单一调用点 + `--check-products` 的四项判据各自仍可红），**不用行数靶** |
| **B3** | **第二消费者**：本 change 内把 `examples/riscv-mini/tests/test_rv32i_hier_rtl.cpp` 的 module 集/端口计数改为调用新 API；**做不到** ⇒ 明写为带门的 follow-up（否则下沉 = 第 5 份实现） |
| **B4** | 下沉后的机制 MUST 有**常驻**判别力回归（注入错期望 ⇒ 必红），且 `verify_examples_rtl_sim.sh` ④″ 仍执行、`--check-products` 退出码语义不变 |

---

## 验收标准

**18 条**（3.1 = AC1–AC13 · 3.2 = AC14–AC18）

| AC | 判据（机械） | 载体 |
|---|---|---|
| AC1 | 两个头 MUST **不在** `include/axi4/` 且**在** `examples/axi4/` | shell |
| AC2 | 库公开面归零：`AxiLiteTop`/`AxiSocTop`/`AxiLiteTestMaster` 在 `include/` grep == 0 | grep |
| AC3 | **include 文本归零**：`grep -rn '"axi4/axi4_lite_example_top\.h"\|"axi4/axi_soc_example_top\.h"' examples tests` == 0（消费者必须改裸文件名） | grep |
| AC4 | **依赖方向有真判据**：C3（反向 `#include`）绿 · **C2 扩展**（扫描面仍只 `tests/CMakeLists.txt`，匹配串 `examples/riscv-mini/src` → `examples/`，**并移植 C7 的剥注释 + 括号配平**）· **C7**（`tests/CMakeLists.txt` MUST NOT 编译 examples 源）绿 · **fixtures**：注释里出现 `examples/` ⇒ 绿；`target_include_directories(… examples/axi4)` ⇒ 红 | 投毒 |
| AC5 | `tests/test_axi_lite_verilator_e2e.cpp` 的**路径 / TEST_CASE 名 / SECTION / 断言**零变化（diff 只含：删顶层头 include · `using` 改库组件 · DUT 类型 · 顶部注释含端口计数订正） | diff |
| AC6 | 该 e2e 测试仍绿（verilator 在 `build/verilator-install/bin` 或 PATH ⇒ 本机可取证） | ctest |
| AC7 | 根 `tests/` 下 MUST NOT 再有引用这两个头的 TU（`grep -rn … tests/` == 0） | grep |
| AC8 | **拆分守恒（判据，不是记录）**：拆分前后 **TEST_CASE 名集合差集 == ∅**（用 `--list-tests` 或源码解析；**`ctest -N` 不可用** —— 它列的是 CTest 条目名而非 `TEST_CASE` 名）；共享 helper（`verif_path()` / `LateAxiLiteTop` / `read_file()`）随用例一起搬；新 helper MUST 带 `LABELS base` | shell |
| AC9 | 归位后测试仍进 CTest：`ctest -N` 含 `test_axi_lite_test_master` · `test_verif_single_source` · `test_verif_axi4_lite` ⇒ **仅在 `CPPHDL_BUILD_EXAMPLES=ON`（默认）成立** | shell |
| AC10 | **计数锚点按实测重算（不是原值 −1）**：`CTEST_TOTAL` 268 → **269** · non-perf **257 → 258** · `-L base` **231 → 232** · `EXAMPLES_TEST_SOURCES` 18 → **20** · `tests/test_*.cpp` **159 → 158**（AGENTS.md 现写 145 —— **已陈旧**，本次必须用实测值 158 回填） | shell |
| AC11 | 棘轮豁免表：23 行**只改第一列**（**不删行**）⇒ `verify_port_naming.sh` 绿 且 `EXPECTED_EXEMPT_PORTS=1415` **不变** | 护栏 |
| AC12 | **公开面移除登记**：`public-api-removals.tsv` 新增 **5 条**（2 条 `nofile`：两个头的 glob；3 条 `removed`：三个类名 · `scope=include/`）⇒ `EXPECTED_PROBES` 46 → **51**、`EXPECTED_REMOVAL_ROWS` 25 → **30**（登记行 MUST 用 `**移除**` 关键字）⇒ `public-api-surface.md` 同章逐条登记 | 护栏 |
| AC13 | 输入端口登记表 **3 行保留** ⇒ 把 `input_port_registry.py` 的 `header_of()` 搜索根从 `include/**` 扩到 `include/**` + `examples/**` ⇒ ⇒ 护栏绿且 **rst_n 的 per-TEST-CASE 覆盖零缩面**（这比「删行 + 加锚点接受缩面」更省） | 护栏 |
| AC14 | `CH4_ANCHOR` **+2 行**（两个新 `examples/axi4/tests/*.cpp` 的断言数下限）⇒ `verify_example_assertion_quality.sh` 绿 | 护栏 |
| AC15 | **文档零旧路径 = 独立 AC**：作用域 = 三份 `AGENTS.md` + `docs/usage_guide/**` + `.rddf/checks/**`（**新增记录必须写旧路径**）；**排除** = `openspec/changes/**`（含本 change 自身与**仍活跃的**前置 change）· `docs/archive/**` · `docs/architecture/**` · `docs/adr/**` · `docs/reports/**` · **整份 `docs/CHANGELOG.md`** · `.rddf/improvements/**` · `.rddf/reviews/**` | grep |
| AC16 | **产物等价（可复算）**：搬前产 4 个 sha256（AXI4-Lite / SoC × `top.v` + `manifest.json`）记入 `.rddf/checks/`，含复算命令与前提（同 build dir / 同 emitter / 前置已完成的 HEAD）⇒ 搬后重产 sha256 相同 | diff |
| AC17 | 3.2 的机制函数在**公开头**（`include/rtl_emit/*.h`，否则第二消费者不可达）⇒ `emit_rtl_soc.cpp` **零**自实现机制函数（列出 3 个符号名并断言无定义）+ 新 API 调用点 == 2 | grep |
| AC18 | `--check-products` 的 **4 项判据各自**有常驻判别力回归（逐项注入错期望 ⇒ 必红）；`verify_examples_rtl_sim.sh` ④″ 仍执行；退出码语义（0/1）不变 | 投毒 |
| AC19 | 3.2 若落地 ⇒ **公开面新增** MUST 登记（`public-api-surface.md` + `CHANGELOG`；ADR-074 明文要求） | 登记 |
| AC20 | `CPPHDL_BUILD_EXAMPLES=OFF` 下被搬走的 2 个条目消失 ⇒ **显式接受**并登记（AC9/AC10 的锚点只在默认配置成立） | 登记 |
| AC21 | **新 capability 的护栏存在且可自证**：`scripts/verify_example_only_headers.sh` + 登记表 + `--self-test`（**6 条投毒**：① 新头落 `include/` ⇒ 红 ② **子路径引用被认出** ⇒ 绿 ③ 登记路径不存在 ⇒ 红 ④ 登记头新增库侧消费者 ⇒ **红** ⑤ **孤儿头 ⇒ 不得红（退出码 0）** ⑥ **末段同名歧义 ⇒ 不得红**）⇒ ⇒ `--self-test` 整轮绿 | 护栏 |

### AC ↔ task

| AC | task |
|---|---|
| AC1 · AC2 · AC3 · AC7 | A1–A3 · A5 |
| AC4 | A6（C2 扩展 + fixtures） |
| AC5 · AC6 | B1 · B2 |
| AC8 · AC9 | B3–B5 · C1–C3 |
| AC10 | C3 · C4 |
| AC11 · AC12 · AC13 · AC14 | D1–D5 |
| AC15 | E1 |
| AC16 | E2 · E3 |
| AC17 · AC18 · AC19 | F1–F5 |
| AC20 | C5 |
| AC21 | D6 |

---

## Capabilities

- **新增 `example-component-boundary`**（本 change 首次 ADDED）⇒ **三条** requirement，
  **判据 = 事件驱动棘轮**（只约束**新增** `include/` 头；**不**做 include 文法解析）：
  · R1「新增的 example 专用头 MUST 生在 examples 目录内」：merge-base 之后新增的 `include/`
    头 ⇒ **末段匹配**统计 includer（作用域 `include/ src/ tests/ examples/ tools/`）⇒
    全部在 `examples/` ⇒ 红；三态区分「孤儿头 / 歧义 / 违规」
  · R2「例外 MUST 双向登记」：`example-only-headers.tsv` ⇒ 路径不存在 ⇒ 红；出现非 examples
    消费者 ⇒ 报「可移出登记表」；**且对新增头开放**（这是「库层机制 API」的唯一合规路径）
  · R3「库测试 MUST NOT 引用 example 专用头」
  · 初始登记（实测）：`include/cpu/cache/i_cache_prefetch.h` ·
    `include/cpu/cache/d_cache_write_buffer.h`（**ADR-046 D3 #9/#10 判为库组件** ⇒ 登记是记录
    「现存但不在本条范围」，**不是**重新归类）· `include/axi4/axi_to_axilite.h`
  · ⚠️ `include/axi4/axi4_lite_matrix.h` **不登记** —— 根 `tests/test_axi_lite_matrix_2x2.cpp`
    是它的合法库侧消费者
  · ⚠️ **与 3.2 的关系**：3.2 若落地会新增「库层机制 API + 消费者恰为 examples」的公开头
    ⇒ MUST 走 R2 的登记通道（这正是该通道存在的理由）
  · 护栏 `scripts/verify_example_only_headers.sh` + `--self-test`（4 条投毒：新头落 `include/`
    ⇒ 红 · 子路径引用被认出 ⇒ 绿 · 登记路径不存在 ⇒ 红 · 登记头新增库侧消费者 ⇒ 提示/红）
- **MODIFIED `dual-backend-test-coverage`**：增一条**受限例外**（标题逐字不变）——
  例外清单 MUST 显式含 A4 实际要做的三类 diff：**删顶层头 include** · **`using` 改库组件** ·
  **顶部注释含端口计数订正（10/7 → 12/8）**
- **MODIFIED `verilator-backend-e2e`**：fixture 描述由「包着 `AxiLiteSlave` 的 `AxiLiteTop`」
  改为「**就是**库组件 `AxiLiteSlave`」（标题逐字不变）

---

## 用户可见变更

| 项 | 前 | 后 |
|---|---|---|
| 库公开面 | 含 2 个 example 专用头（3 个类） | **不含**（下游改用 `examples/axi4/` 新位置，或直接改用 `AxiLiteSlave`） |
| `tests/test_axi_lite_test_master.cpp` | 根 `tests/` | `examples/axi4/tests/`（**CTest 条目名不变**） |
| `tests/test_verif_single_source.cpp` | 13 个用例 | 拆分：`tests/` 留扫描器用例 + `examples/axi4/tests/test_verif_axi4_lite.cpp` 承接 DUT 用例 |
| e2e 测试的 DUT | example 透传封装（端口**具名**，产物 `top_awaddr`） | 库组件（端口**未具名**，产物 `top_unnamed_input_N`）⇒ ⇒ **该测试不按名取端口**，行为不变；但这是「产品端口面在测试侧的**回退**」⇒ ⇒ MUST 登记（若将来升级为 IR 解释器形态会 fail-closed） |
| 产物 | — | **逐字节不变**（AC13） |

---

## Out of Scope（明确不做）

| 项 | 原因 |
|---|---|
| 给库组件 `AxiLiteSlave` 的端口具名 | 会改所有用它的产品的 Verilog 端口名 ⇒ 公开面变更 ⇒ 需 architect（ADR-075 域）；本 change 靠「测试归位」绕开 |
| 给 `AxiLiteTestMaster` 的 23 个端口具名 | 成层子 module 端口 ⇒ 独立立项 |
| 把 `AxiLiteTop` / `AxiSocTop` **提升**为库组件 | 与本 change 方向相反 ⇒ 需 ADR |
| `emit_rtl_soc.cpp` **整个文件**的合并 | 合并的前提是 3.2 ⇒ 本 change 只做 3.2（若 G-1 闭） |
| `find_verilator()` 下沉 | 仓内已有裁决（fork 外部 lint 会污染失败归因）⇒ linter 路径作为参数 |
| RV32I 侧顶层组件归位 | 另一批 example ⇒ 独立立项 |
| 删 `emit_rtl.cpp` | 属 `emit-cli-into-example` |
| 把 `examples/axi4/emit_rtl_soc.cpp` 在 `emit-cli-into-example` 的「零 diff」判据下保持 | **单向顺序**：本 change 的 3.2 若先行 ⇒ 该判据失效 ⇒ ⇒ **3.2 MUST 在 `emit-cli-into-example` 之后** |

---

## 🚪 门（**开工前 MUST 闭**）

| 门 | 内容 | 闭法 |
|---|---|---|
| **G-1** | **3.2 的口径确认**：蓝图 §4.3（「483 → ~60 行」+ 公开面登记 + `--check-products` 退出码逐字保持）是否仍有效、以及是否允许在**本 change 内**落地 | architect 确认或修订 ADR-074 的「不覆盖」表述；**裁决前 3.1 可独立推进** |
| **G-2** | **单向顺序**：3.2 与 `emit-cli-into-example` 的「`emit_rtl_soc.cpp` 零 diff」互斥 | 顺序 MUST 为 `axi4-verif-driven-cpp` → `emit-cli-into-example` → 本 change（3.1）→ 3.2 |
| **G-3** | **测试归位登记**：新增的 `examples/axi4/tests/*.cpp` MUST 进 `EXAMPLES_TEST_SOURCES`（18 → 20）与 `CH4_ANCHOR` | 实施期 T5/T6 |

---

## 顺序与所有权（**替代「建议串行」**）

| 文件 | `axi4-verif-driven-cpp` | `emit-cli-into-example` | 本 change |
|---|---|---|---|
| `examples/axi4/tb_axi4_lite_example.cpp` | owner（驱动 + 改名） | 加 `--emit-rtl` | — |
| `examples/axi4/emit_rtl.cpp` | — | **删** | — |
| `examples/axi4/emit_rtl_soc.cpp` | — | **零 diff** | 3.2 owner |
| `tests/CMakeLists.txt:1455` | 改第二参数 | — | — |
| `tests/test_verif_single_source.cpp` | **零改动** | — | owner（拆分） |
| `examples/axi4/CMakeLists.txt` | target/install | 删 emitter target | 加 helper |
| `examples-rtl-sim-targets.tsv` 第 53 行 | `target`/`source` | `emitter`/`emitter_args` | — |

⇒ ⇒ **最小改纪律**：该行 / `examples/axi4/CMakeLists.txt` 被多方改 ⇒ ⇒ MUST **按列、按块**最小编辑，
**MUST NOT** 整行 / 整文件替换（两个 worktree 并行时会丢改）。

---

## 风险与回滚

| 风险 | 处置 | 回滚 |
|---|---|---|
| include 文本没改 ⇒ 编译红 | A2/A5（3 个 example + 2 个搬入测试改裸名）+ **AC3** 的 grep | 反向 `git mv` |
| 用「加 include 路径」绕过依赖方向 | **AC4** 的 C2 扩展 + 投毒（现有 C2 只匹配 `examples/riscv-mini/src`） | — |
| 棘轮豁免表顺手删行 ⇒ `EXPECTED_EXEMPT_PORTS` 红 | **AC11**：只改第一列，**不删行** | — |
| 公开面移除漏登记 ⇒ R2/R6 锚点红 | AC12：`public-api-removals.tsv` **5 条探针**（2 `nofile` + 3 `removed`）+ 两个锚点同步（46→51 / 25→30） | — |
| `CH4_ANCHOR` 缺行 ⇒ 断言质量护栏 exit 1 | **AC14** | — |
| 产物不等价（搬家意外改语义） | **AC16** 的 sha256（同 build dir / 同 emitter / 串行前置 HEAD） | — |
| 3.2 下沉后判据变恒绿 | **AC18** 的逐项常驻投毒 | 回退 3.2 |
| `CPPHDL_BUILD_EXAMPLES=OFF` 下锚点不匹配 | **AC20** 显式接受 + 注释指引 | — |

## 关联 change

- **前置**: `rtl-verification-single-source-2026-11`
- **前序（硬顺序）**: `axi4-verif-driven-cpp-2026-11` → `emit-cli-into-example-2026-11` → 本 change
