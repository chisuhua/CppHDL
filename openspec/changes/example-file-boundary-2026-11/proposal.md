# Proposal: example 文件职责边界 —— 顶层组件归位 + SoC checker 机制下沉（`example-file-boundary`）

- **状态**: **立项（2026-11-18 · planner）** · 未开工 · **metis 第 1 轮已处置（P0×9 / P1×4 / P2×2）** · **第 2 轮已处置** · **第 3 轮（3.2 扩容：蓝图订正 + 形态裁决 + 切分表）已处置**
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

### 3.2-a 蓝图 §4.3 的**现状栏与目标栏双双陈旧**（实测 2026-11-18）

蓝图 `docs/architecture/2026-11-17-rtl-verification-single-source.md` 写：

| 位置 | 陈旧表述 | 实测 |
|---|---|---|
| `:19`（G-发射总表） | `emit_rtl_soc.cpp`「483 行自解析」 | **388 行**；且 `main`（`:350-388`，**39 行**）**已**在 `:383` 调库层 `ch::rtl_emit::run_emit_cli` ⇒ ⇒ **壳层归库层已完成** |
| `:43`（缺口②） | `emit_rtl_soc.cpp:5-8,64-89,415-483` | **悬空引用** —— 文件只有 388 行 ⇒ `415-483` 指向不存在的位置 |
| `:200`（§4.3 行 1） | 「483 → ~60 行」 | **算术不成立** —— 即便 checker 机制全部下沉，剩 ≈ 100 行专属内容 + 39 行壳 ≈ **140 行** |
| `:272`（§5.2 M4） | 「483 → ~60（库层 `run_emit_cli` + `--check-products` 调库层）」 | 同上 |
| `:201`（§4.3 行 2） | 「机制下沉 `src/rtl_emit/`」 | 与本 change 的落点（**公开头** `include/rtl_emit/`）**直接矛盾** ⇒ ⇒ 实施方无法判断落点 ⇒ ⇒ **必须由 F1 裁定**（critic 第 3 轮 BLOCKER 2 的根因之一） |
| `:206`（§4.3 末尾「不动」条） | 「不动：`.rddf/improvements/` 与 `openspec/changes/archive/**` 的同款表述（历史记录）」 | 与 F1 ⑤「**draft 活文档**同批订正」**直接冲突** ⇒ ⇒ 蓝图自相矛盾 ⇒ ⇒ **必须一并订正** |

⚠️ **行号会漂**：上表的 `:19` / `:43` / `:200` / `:272` / `:201` / `:206` 是**定位锚点** ⇒ ⇒
蓝图一改即漂 ⇒ ⇒ **F1 订正时 MUST 同时记锚点文本**（如「G-发射 总表行」·「缺口 ② 行」·
「§4.3 行 1」·「§4.3 行 2」·「§4.3 末尾『不动』条」·「§5.2 M4 行」）⇒ ⇒ 判据按**锚点文本**
定位，行号仅作辅助。

⚠️ **蓝图 §5.1 的一处 `~60` 估算豁免**（`:260`，非本条目标栏）⇒ ⇒ 判据 MUST **按位置**限定，
不可全文一刀切（见 AC23）。

⚠️ **`.rddf/improvements/rtl-verification-single-source-2026-11.md` 是 `draft` 活文档**
（`:43` / `:52` / `:161` 同款 `483` / `~60` / `415-483`）⇒ ⇒ **活文档 MUST 同批订正**，
不可当「历史记录」豁免（历史记录 = `docs/archive/**` · `openspec/changes/archive/**`）。

⇒ ⇒ **订正蓝图属 architect 写集**（`docs/architecture/**`）⇒ ⇒ builder 不可写
⇒ ⇒ 归 **architect 裁决的交付物**（G-1），不是实施任务。

### 3.2-b 现状是**「默认模式发扁平」**，而既有 MUST 说 SoC 交付产物 MUST 层次化

`rtl-soc-output` 的既有 requirement 明文：SoC **交付产物** MUST 是层次化产物，
module 数 MUST == 「成层 IP 实例数 + 1」（本项目 = **5**）。

⚠️ **实测现状**：`emit_rtl_soc.cpp:371-377` **只在 `--check-products` 模式**下补 `--hierarchical`
⇒ ⇒ **默认模式（无该开关）发的是扁平产物（1 个 module）** ⇒ ⇒ 而护栏 ③ 对 `kind=soc` 行
发的**正是**默认模式（`scripts/verify_examples_rtl_sim.sh:449`：`--out "$work" --emit-hygiene`），
④″（`:653`）才用 `--check-products` **覆盖**同一目录成层次化。

⇒ ⇒ **这不是措辞问题，是可能的现存违规**：SoC 发射入口存在一条**能产出非层次化 SoC 产物**的路径。
⇒ ⇒ 而蓝图 §4.3 行 1 的「**强制** `--hierarchical`」恰好就是这个意图 ⇒ ⇒ **蓝图是对的，现状是反的**。

⇒ ⇒ ✅ **已裁决（2026-11-18 · architect · 蓝图 §4.3.2）= 分支 A**：SoC 发射入口**恒发层次化**。
⇒ ⇒ **依据**（默认模式 = 5 module 早已在**四处**登记 ⇒ ⇒ **扁平才是偏离方** ⇒ ⇒ 分支 A = **恢复已登记契约**）：
`examples/axi4/tb_axi_soc_example.sv` 头注 · `examples/axi4/CMakeLists.txt` target 注 ·
`.rddf/checks/examples-rtl-sim-targets.tsv` 的 SoC 行 note · 已归档 change `soc-rtl-product-and-tb-2026-11` 的交付契约。
⇒ ⇒ **不新立 ADR** · **不改** `rtl-soc-output` 的既有 requirement · **不改** ADR-074 的「不覆盖」条。
⇒ ⇒ 后果 = 默认模式产物形态变化 ⇒ ⇒ 护栏 ③ 的发射语义 **MUST 同步** + 产物形态 **MUST 登记** +
基线 **MUST 分模式采**（默认模式与 `--check-products` 模式各一套）+ 护栏 ⑧' **MUST 扩到 `kind=soc`**。
⇒ ⇒ **分支 B（维持现状）已被否决**（它须改一条既有 MUST ⇒ ADR 修订 + capability MODIFIED ⇒ 更重且语义更弱）。
产物形态 **MUST 登记** + 基线 **MUST 分模式采**（默认模式与 `--check-products` 模式各一套）。
· **分支 B（维持现状）**：则**必须**改一条既有 MUST（把「默认模式输出」排除在「交付产物」外）
⇒ ⇒ 那是 **ADR 修订 + `rtl-soc-output` MODIFIED** ⇒ ⇒ **不在本 change 的实施面**
⇒ ⇒ 本 change **阻断**并登记为 follow-up。

⚠️ **该冲突在 G-1 裁决前持续存在** ⇒ ⇒ MUST 先行登记为**已知残留**（不得等裁决才登记 ⇒
否则裁决拖延期它会静默漂移）⇒ ⇒ 登记落点见「🚪 门 G-1」第 ⑤ 项（**2026-11-17 蓝图的 §7**，
**不是** `.rddf/checks/arch-residual-registry.md` —— 那本护栏的台账是**硬编码的另一本**
蓝图，见 `scripts/lib/ledger_residual.py:17`）。

### 3.2-c 自实现的符号不止「3 个」⇒ ⇒ 切分表 MUST 覆盖 **9 个**

「3 个机制函数」是**当前自实现的机制**，不是文件里的全部符号。实测该文件 namespace 域内 **9 个**
（`kExpectedModules:43` · `kExpectedTopPorts:50` 同样是 internal linkage，且**正是判据值本体**）：

| 符号 | 行 | 性质 | 归属 |
|---|---|---|---|
| `kExpectedModules` | `:43` | **判据值**（module 集） | ⇒ 留 example（ADR-074「不覆盖」） |
| `kExpectedTopPorts` | `:50` | **判据值**（顶层端口数 = 14） | ⇒ 留 example |
| `product_modules` | `:53` | **机制**（产物派生 module 集） | ⇒ 候选下沉 |
| `find_verilator` | `:67` | 工具发现 | ⇒ **不下沉**（见上） |
| `make_inputs` | `:90` | **SoC 专属**（收 `AxiSocTop&`） | ⇒ 留 example |
| `make_observations` | `:115` | **SoC 专属**（收 `AxiSocTop&`） | ⇒ 留 example |
| `make_stimulus` | `:138` | 形态通用（`vector<vector<uint64_t>>`） | ⇒ **待裁决** |
| `tamper_hier` | `:169` | 变异自证**编排**（机制）+ SoC 专属变异目标 | ⇒ 编排候选下沉 |
| `check_products` | `:218` | SoC checker（机制 + 4 项判据值） | ⇒ 机制候选下沉；**判据值留 example** |

⇒ ⇒ **「下沉」这一格 MUST 可反驳** ⇒ ⇒ 判据 = **具名第二消费者 + 调用点**（不是行数、不是主观归类）
⇒ ⇒ **给不出第二消费者 ⇒ 只能写「不下沉」**。

### 3.2-d 「壳层」必须**定义**，否则判据自否

`make_inputs` / `make_observations` / `check_products` 与 `main` **同在一个文件** ⇒ ⇒
「壳层」若指文件 ⇒ 现状**违反**且与「判据值留 example」自相矛盾；若指 `main` ⇒ 现状**已满足**。

⇒ ⇒ **定义**：「**壳层** = 发射入口的 `main()` 本体 + 它调用的**库层**入口」；
example 侧的产品专属 helper（`make_inputs` / `make_observations` / `check_products` 的判据值等）
**无论是否被 `main` 直接调用，一律归 example** ⇒ ⇒ 归属**以 F1 的切分表为准**，不由「谁调用谁」推。
⚠️ 「壳层」**不是**按调用关系划的（`check_products` 由 `main` 直接调用 `:387`，但它归 example）。

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

> ⚠️ **标签约定**：3.2 的 **F1–F9** = tasks.md 的**批次 F** 任务号（一一对应）⇒ ⇒ 与 3.1 的
> 「A/B/C… = 变更项编号」**不是同一套编号** ⇒ ⇒ 以 tasks.md 为准。

| # | 变更 |
|---|---|
| **F0** | **开工前准备（不受 G-1 约束 ⇒ 可与 F1 并行；MUST 在任何代码改动之前完成）**：① **采基线**：MUST 在本 change **任何代码改动之前**、且 MUST 在 `emit-cli-into-example` **之后**（SoC 入口形态已变）⇒ ⇒ **分模式两套**（默认模式 / `--check-products` 模式）+ 比较面 = 该 `--out` 目录**全部落盘物**（含 `eq/`）+ 记录 verilator 可用性 ⇒ ⇒ 前提 = 同 build 配置 + **唯一变量 = `examples/axi4/emit_rtl_soc.cpp`**（**不是**「同 emitter」）② **订正上游活跃 change** `rtl-verification-single-source-2026-11` 的同款陈旧目标（`proposal.md` 的 `:21` / `:34` / `:46` + `tasks.md:29`）⇒ ⇒ 该 change **尚未归档** ⇒ ⇒ **不是**「历史记录」豁免 ⇒ ⇒ 不订正 ⇒ 旧目标被冻进历史 |
| **F1** | ✅ **architect 裁决已产出（2026-11-18）** —— 载体 = 蓝图 `docs/architecture/2026-11-17-rtl-verification-single-source.md` 的 **§4.3 订正 + §4.3.1 逐符号切分表 + §4.3.2 形态裁决 + §7 R12** ⇒ ⇒ ① 6 处订正按**锚点文本**落地 ② 切分表维度 = **规则 vs 值**（9 符号 · **禁**「待定」；**第二消费者是加分项、不是门槛** —— `tamper_hier` 语义不同 ⇒ 如实标注）③ 形态 = **分支 A**（恒发层次化 = **恢复已登记契约** ⇒ 不改 `rtl-soc-output` · **不新立 ADR** · ADR-074「不覆盖」条**无需改动**）④ 登记载体订正（`public-api-surface.md` 只扫**顶层**头 ⇒ 子目录头**零判别力** ⇒ 真载体是**待建**的 `example-only-headers.tsv` 豁免通道）⑤ 「不动」例外**三类归属**：归档记录不动 / draft 活文档 = **planner 侧**（本轮已订正）/ 未归档 active change = **builder 的 F0 ②** ⇒ ⇒ ⚠️ 蓝图属 architect 写集 ⇒ ⇒ **builder MUST NOT 代改** ⇒ ⇒ **F2–F9 的实施判据以蓝图 §4.3.1 / §4.3.2 为准** |
| **F2** | **按 F1 的切分表下沉**到**公开头** `include/rtl_emit/*.h`（实现落 `src/rtl_emit/*.cpp`）⇒ ⇒ ⚠️ **该头的实现 TU 必然 include 它 ⇒ 按 R2 定义它不算「消费者」**（**实现 ≠ 使用**）⇒ ⇒ 故 R2 的反向判据不会误红 ⇒ ⇒ SoC 专属期望值 / 激励面 / 观测面 / 变异目标改为**参数**；**linter 路径由调用方传入**（不下沉 `find_verilator`）⇒ ⇒ ⚠️ **只下沉「编排」**，`--check-products` 的 4 项**判据值**属产品专属期望（ADR-074 的「不覆盖」）⇒ ⇒ MUST NOT 搬进库 |
| **F3** | `emit_rtl_soc.cpp` 的**壳层**（= `main()` 及其直接委派路径）退化为「组装参数 + 调用」⇒ ⇒ **结构判据**（被下沉的符号在 example 侧**无定义** + 调用点计数 + `--check-products` 的四项判据各自仍可红），**不用行数靶** ⇒ ⇒ ⚠️ **同文件内的产品专属 helper 归 example**（不受本条约束） |
| **F4** | **第二消费者**：本 change 内把 `examples/riscv-mini/tests/test_rv32i_hier_rtl.cpp` 的 module 集/端口计数改为调用新 API；**做不到** ⇒ 明写为带门的 follow-up（否则下沉 = 第 5 份实现）⇒ ⇒ ⚠️ **`tamper_hier` 的语义归一化另立项**：SoC 版 = 由观测信号反查子端口 → 取反 RHS；RV32I 版 = **硬编码 3 条 `assign` 名** + RHS 置 `'0'` ⇒ ⇒ **语义不同** ⇒ ⇒ 本 change **只做 `product_modules` 的归一化**（真同语义）⇒ ⇒ **MUST NOT** 宣称 `tamper_hier`「同一语义两份实现」 |
| **F5** | 下沉后的机制 MUST 有**常驻**判别力回归（注入错期望 ⇒ 必红），且 `verify_examples_rtl_sim.sh` ④″ 仍执行、`--check-products` 退出码语义不变 ⇒ ⇒ ⚠️ **已知边界**：`emit_rtl_soc.cpp:263-273` 的 CHECK 3 在 verilator 不可用时打印 `SKIP` 且**不计 failures** ⇒ ⇒ 基线记录 MUST 含 **verilator 可用性 / 版本** ⇒ ⇒ 「行为不变」MUST NOT 覆盖一个未验证分支 |
| **F6** | **公开面登记（真载体）**：新增的公开头 MUST 走 **`.rddf/checks/example-only-headers.tsv` 豁免通道**（消费者恰为 examples ⇒ 这是本仓**唯一**会判它的护栏）+ `docs/CHANGELOG.md` 一行 ⇒ ⇒ ⚠️ **如实声明**：`public-api-surface.md` 的扫描范围 = **顶层 `include/*.h`** + 两个已裁决 core 头 ⇒ ⇒ `include/rtl_emit/*.h` 属**子目录头 ⇒ 不在扫描范围内** ⇒ ⇒ 登记到那里**零判别力**（不红也不绿）⇒ ⇒ 该处只作**记录**，不构成护栏 |
| **F7** | **产物形态（分支 A · 2026-11-18 已裁决）**：SoC 发射入口**恒发层次化**（默认模式也层次化）⇒ ⇒ 护栏 ③ 对 `kind=soc` 行的**发射语义 MUST 同步**（它现在发的是扁平产物）+ 产物形态 **MUST 登记** ⇒ ⇒ ⚠️ **护栏 ⑧' MUST 扩到 `kind=soc`**（现仅 `kind=hier` ⇒ 否则「入口退回扁平」这条回归路径**无任何常驻判据**）+ **soc 行 MUST 有负向对照**（投毒 ⇒ 必红）⇒ ⇒ **CHANGELOG MUST 有一行**（默认模式产物形态变化 = 用户可见变更）⇒ ⇒ ⚠️ ⑧ 在层次化产物上复核：实测成立（工具用 **set** 比较 ⇒ **MUST NOT** 依赖「逐字逐序」措辞 —— 它与实现不符） |
| **F8** | **产物等价核对**：用 F0 采的基线逐项比对（分模式两套 + 全部落盘物）⇒ ⇒ **分支 A 例外**：默认模式的差异 MUST **恰为形态变化**且**可机器判定**（落盘物集合 == {`top.v`, `manifest.json`} ∧ `manifest.json` 逐字节相同 ∧ module 集 1 → 5 ∧ `top.v` 的顶层端口表与基线扁平产物逐字逐序相同）；`eq/` 与 `--check-products` 模式 MUST **零差异** ⇒ ⇒ **禁**「逐字节相同」一刀切 ⇒ ⇒ **改动过产物 ⇒ 必红** |
| **F9** | **切分表为空的合法终态**（⚠️ **本轮裁决未触发** —— 蓝图 §4.3.1 已选出「完整 2 + 拆分 1」⇒ ⇒ 本条仅在切分表**最终**为空时生效）：⇒ ⇒ 本批次零代码改动 ⇒ ⇒ 实质结论（「§4.3 行 1 无待做动作 · 目标改为结构判据」）MUST 由 **F1** 在 **2026-11-17 蓝图 §7「已知残留与边界」** 留一条**已收口**残差（闭口证据 = **存在的路径** · 逗号分隔 · 无空格 · 无反引号 ⇒ 见 `scripts/lib/ledger_residual.py` 的证据格式）⇒ ⇒ ⚠️ **不是** `.rddf/checks/arch-residual-registry.md` —— 那本护栏的台账**硬编码**为**另一本**蓝图（`scripts/lib/ledger_residual.py:17`）⇒ ⇒ 往它加行**必红**（「结算行数不一致」）⇒ ⇒ 本 change 的判据 = **蓝图 §7 段落自证** |

---

## 验收标准

**26 条**（3.1 = AC1–AC16 · AC20 · AC21 · 3.2 = AC17–AC19 · AC22–AC26）

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
| AC17 | 3.2 的**规则侧**按 **F1 切分表**下沉到 `include/rtl_emit/*.h`（**公开头**）⇒ ⇒ **下沉集 = 完整 2 个（`product_modules` · `tamper_hier`）+ 拆分 1 个（`check_products` 的规则侧）**（蓝图 §4.3.1）⇒ ⇒ `emit_rtl_soc.cpp` 的**壳层**（= `main()` 本体 + 它调用的**库层**入口）侧**零**自实现被裁下沉的符号 ⇒ ⇒ ⚠️ **拆分符号的口径**：`check_products` 在 example 侧**必然仍有一个包装函数** ⇒ ⇒ 判据 MUST 写「**规则侧实现** MUST NOT 出现在 example 侧」（禁断言「无 `check_products` 定义」）⇒ ⇒ **调用点计数口径** = **库层公开函数 == 3**（`product_modules` · `tamper_hier` · `check_products` 的规则侧）+ **example 侧对每个的调用 == 1** ⇒ ⇒ 禁写「== 2」（与「完整 2 + 拆分 1」不自洽）⇒ ⇒ ⚠️ **「下沉」的理由 = 产品无关的规则**；**第二消费者是加分项、不是门槛**（`tamper_hier` 的第二消费者**语义不同** ⇒ 已如实标注）⇒ ⇒ ⚠️ **值侧 MUST 留 example**（`kExpectedModules` · `kExpectedTopPorts` · `make_inputs` · `make_observations` · `make_stimulus` **+ 根组件名（owner 基准）· lint 的 `--top-module` 名 · 最小变化观测量阈值**）⇒ ⇒ **MUST NOT** 出现在 `include/rtl_emit/**` ⇒ ⇒ ⚠️ **库层 MUST NOT 内置字面根名（`"top"`）/ MUST NOT 内置 `>= 2` 之类阈值**（根名**可配** ⇒ 硬编码即错 ⇒ ⇒ 任何根名不同的产品会把根自有点误算成后代未驱动 ⇒ **假红**）⇒ ⇒ 一律**参数传入** | grep |
| AC18 | `--check-products` 的 **4 项判据各自**有常驻判别力回归（逐项注入错期望 ⇒ 必红）；`verify_examples_rtl_sim.sh` ④″ 仍执行；退出码语义（0/1）不变 ⇒ ⇒ ⚠️ **已知边界**：`emit_rtl_soc.cpp` 的 CHECK 3 在 verilator 不可用时打印 `SKIP` 且**不计 failures** ⇒ ⇒ 基线记录 MUST 含 **verilator 可用性 / 版本** ⇒ ⇒ 「行为不变」MUST NOT 覆盖一个未验证分支 | 投毒 |
| AC19 | 3.2 若落地 ⇒ **公开面登记 MUST 落真载体**：新增公开头 MUST 进 `.rddf/checks/example-only-headers.tsv` **豁免通道**（反向判据会报「可移出登记表」⇒ 这是**唯一**会判它的护栏）+ `docs/CHANGELOG.md` 一行 ⇒ ⇒ ⚠️ **实现 TU 的 include 不算「消费者」**（R2 定义：实现 ≠ 使用）⇒ ⇒ 故「公开头 + `src/` 实现」这一形态**可登记** ⇒ ⇒ ⚠️ **如实声明**：`public-api-surface.md` 的扫描范围 = **顶层 `include/*.h`** + 两个已裁决 core 头 ⇒ ⇒ `include/rtl_emit/*.h` 属**子目录头 ⇒ 不在扫描范围内** ⇒ ⇒ 登记到那里**零判别力**（不红也不绿）⇒ ⇒ 该处只作**记录**，**不构成**护栏 | 登记 |
| AC20 | `CPPHDL_BUILD_EXAMPLES=OFF` 下被搬走的 2 个条目消失 ⇒ **显式接受**并登记（AC9/AC10 的锚点只在默认配置成立） | 登记 |
| AC21 | **新 capability 的护栏存在且可自证**：`scripts/verify_example_only_headers.sh` + 登记表 + `--self-test`（**6 条投毒**：① 新头落 `include/` ⇒ 红 ② **子路径引用被认出** ⇒ 绿 ③ 登记路径不存在 ⇒ 红 ④ 登记头新增库侧消费者 ⇒ **红** ⑤ **孤儿头 ⇒ 不得红（退出码 0）** ⑥ **末段同名歧义 ⇒ 不得红**）⇒ ⇒ `--self-test` 整轮绿 ⇒ ⇒ ⚠️ **投毒④ 的「库侧消费者」MUST 指「API 使用方」**（不含实现 TU 的 include）⇒ ⇒ 否则它与 AC19（登记库层机制头）**互斥** | 护栏 |
| AC22 | **切分表齐备且可复算**：蓝图 §4.3 MUST 含 **9 个符号各一行**（`kExpectedModules` · `kExpectedTopPorts` · `product_modules` · `find_verilator` · `make_inputs` · `make_observations` · `make_stimulus` · `tamper_hier` · `check_products`）⇒ ⇒ 归属维度 MUST 是 **规则 vs 值**（规则 = 产品无关的判定逻辑 ⇒ 可下沉；值 = 某产品的期望 ⇒ 留 example）⇒ ⇒ 判据 MUST **限定在 §4.3 段内**（不可全文 grep ⇒ 否则无关表格行可满足）⇒ ⇒ 表内 MUST NOT 出现「待定」（同样限定在该段）⇒ ⇒ **门槛未满足者 MUST 如实标注**（`tamper_hier`：第二消费者**语义不同**）⇒ ⇒ **禁**以裸行号 pin（行号随 HEAD 漂移）⇒ ⇒ 定位 MUST 用**复算命令**（`grep -nE '(^|[^A-Za-z0-9_])<symbol>([[:space:]]*\(|[[:space:]]*=)' examples/axi4/emit_rtl_soc.cpp`（**函数定义与常量定义两种形态**）⇒ ⚠️ **禁** `grep '^<symbol>'` —— 定义以类型关键字开头 ⇒ 那样**零命中**）⇒ ⇒ 命令**必须有退出码**（计数 < 1 ⇒ 非零） | grep |
| AC23 | **陈旧表述清零（按位置限定）**：蓝图 + **`.rddf/improvements/rtl-verification-single-source-2026-11.md`（draft 活文档）**里，`483` / `~60` / `415-483` 在**指定位置**零命中 ⇒ ⇒ 定位 MUST 按**锚点文本**（行号仅辅助）⇒ ⇒ 豁免 = 蓝图 **「§5.1 N10 行」**（它估的是 `examples/axi4/verif_run.cpp` 的行数，**不是** SoC 发射入口 ⇒ 非本条目标栏）· `docs/archive/**` · `openspec/changes/archive/**` ⇒ ⇒ 判据 MUST **按位置**限定（不可全文一刀切 ⇒ 否则误伤该合法估算）⇒ ⇒ **上游活跃 change** `rtl-verification-single-source-2026-11`（**未归档 ⇒ 不是历史记录**）的同款目标 MUST 由 **F0 ②** 订正 ⇒ ⇒ 命令**必须有退出码** | grep |
| AC24 | **形态与写明一致（分支 A 专属）**：SoC 发射入口**恒发层次化** ⇒ ⇒ 默认模式产物 module 数 **== 5**（4 个成层 IP + 根 `top`；权威来源 `.rddf/checks/soc-rtl-expectations.md` 的 module 集表）⇒ ⇒ 护栏 ③ 对 `kind=soc` 行的**发射语义 MUST 已同步**（它现在发的是扁平产物）+ 产物形态 **MUST 登记** ⇒ ⇒ 护栏 ⑧（顶层端口表）在**层次化**产物上仍绿 ⇒ ⇒ 分支 B 下本条**不适用**（走 ADR 修订 + follow-up）⇒ ⇒ 判据 MUST 是**退出码**（`test "$(grep -c '^module ' …)" -eq 5`）⇒ ⇒ **禁**只打印计数让人眼比对 | shell + 护栏 |
| AC25 | **基线先行 + 分模式 + 全落盘物**：基线（**F0 ①**）MUST 在本 change **任何代码改动之前**、且 MUST 在 `emit-cli-into-example` **之后**采集 ⇒ ⇒ 默认模式与 `--check-products` 模式**各一套** ⇒ ⇒ 比较面 = 该 `--out` 目录下**全部落盘物**（`--check-products` 额外产 `eq/`）⇒ ⇒ 前提 = 同 build 配置 + **唯一变量 = `examples/axi4/emit_rtl_soc.cpp`**（**不是**「同 emitter」—— 本 change 改的就是 emitter 本体）⇒ ⇒ 记录 verilator 可用性 ⇒ ⇒ ⚠️ 3.1（批次 A–E）**不触及** SoC 发射入口的产物语义 ⇒ ⇒ 「任何代码改动之前」指**批次 F 的代码改动之前** ⇒ ⇒ ⚠️ **分支 A 例外**（形态裁决 = 恒发层次化 ⇒ 默认模式产物**必然**变）：默认模式的差异 MUST **恰为形态变化**且**可机器判定** —— 即：落盘物集合 **== {`top.v`, `manifest.json`}** ∧ `manifest.json` **逐字节相同** ∧ module 集 **1 → 5** ∧ `top.v` 的**顶层端口表**与基线扁平产物**逐字逐序相同**（⇒ 差异只在**形态**、不在**端口面**）；`eq/` 与 `--check-products` 模式 MUST **零差异** ⇒ ⇒ **禁**用「逐字节相同」一刀切（那会让分支 A 必红）⇒ ⇒ **禁**只写「可解释」（人眼判 ⇒ 无判别力）⇒ ⇒ 判据 MUST 分别列出「预期差异」与「零差异」两组 | diff |
| AC26 | **切分表为空的终态已登记**：若 F1 裁定**无符号可下沉** ⇒ ⇒ **2026-11-17 蓝图 §7「已知残留与边界」** MUST 有一条对应行（状态 = **已收口**；闭口证据 = **存在的路径** · 逗号分隔 · 无空格 · 无反引号）⇒ ⇒ ⚠️ **不是** `.rddf/checks/arch-residual-registry.md`：那本护栏的台账**硬编码**为**另一本**蓝图（`scripts/lib/ledger_residual.py:17`）⇒ ⇒ 往它加行**必红**（「结算行数不一致」）⇒ ⇒ 本条的判据 = **蓝图 §7 段落自证**（grep 该段命中对应行）⇒ ⇒ 否则是「口头上闭合、账本上没有」的静默债 | grep |

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
| AC17 · AC18 · AC19 · AC22 · AC23 · AC24 · AC25 · AC26 | F0–F9 |
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
    ⇒ MUST 走 R2 的登记通道（这正是该通道存在的理由）⇒ ⇒ 且这条通道是**唯一**会判它的护栏
    （`public-api-surface.md` 只扫**顶层** `include/*.h` ⇒ `include/rtl_emit/` 属子目录头 ⇒ 不在范围）
  · 护栏 `scripts/verify_example_only_headers.sh` + `--self-test`（**6 条投毒**：逐条见 **AC21** ⇒ ⇒ 此处不复述，避免双份维护）⇒ ⇒ ⚠️ **投毒④ 的「库侧消费者」= API 使用方**（**不含**实现 TU 的 include）⇒ ⇒ 否则与 AC19（登记库层机制头）**互斥**
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
| 产物 | — | **逐字节不变**（AC16） |
| SoC 发射入口的**产物形态**（**分支 A**） | 默认模式发**扁平**产物（1 个 module） | **恒发层次化**（module 数 == 「成层 IP 实例数 + 1」）⇒ ⇒ **用户可见**：`emit_rtl_axi_soc --out <dir>` 的产物形态变了 ⇒ ⇒ **AC24** |
| SoC checker 的**机制 API** | 自实现于 `examples/axi4/emit_rtl_soc.cpp` | 库层**公开头** `include/rtl_emit/*.h` ⇒ ⇒ 库公开面**新增** ⇒ ⇒ 登记走 `example-only-headers.tsv` 豁免通道（**AC19**） |

---

## Out of Scope（明确不做）

| 项 | 原因 |
|---|---|
| 给库组件 `AxiLiteSlave` 的端口具名 | 会改所有用它的产品的 Verilog 端口名 ⇒ 公开面变更 ⇒ 需 architect（ADR-075 域）；本 change 靠「测试归位」绕开 |
| 给 `AxiLiteTestMaster` 的 23 个端口具名 | 成层子 module 端口 ⇒ 独立立项 |
| 把 `AxiLiteTop` / `AxiSocTop` **提升**为库组件 | 与本 change 方向相反 ⇒ 需 ADR |
| `emit_rtl_soc.cpp` **整个文件**的合并 | 合并的前提是 3.2 ⇒ 本 change 只做 3.2（若 G-1 闭） |
| **默认模式扁平产物的「合规化」**（分支 B） | 分支 B = 改 `rtl-soc-output` 的既有 MUST ⇒ 走 **ADR 修订 + MODIFIED** ⇒ ⇒ **不在本 change 实施面** ⇒ ⇒ 走 follow-up |
| **module 集 / 顶层端口数的多载体收敛** | 现状 **3 处载体**：`emit_rtl_soc.cpp` 的 `kExpectedModules` / `kExpectedTopPorts` · `.rddf/checks/soc-rtl-expectations.md` · `examples-rtl-sim-targets.tsv` 的散文 ⇒ ⇒ **本 change 不收敛**（ADR-074「判据值留 example」 ⇒ example 侧常量 MUST NOT 被删）⇒ ⇒ **登记为已知残留** ⇒ 独立立项 |
| `find_verilator()` 下沉 | 仓内已有裁决（fork 外部 lint 会污染失败归因）⇒ linter 路径作为参数 |
| RV32I 侧顶层组件归位 | 另一批 example ⇒ 独立立项 |
| 删 `emit_rtl.cpp` | 属 `emit-cli-into-example` |
| 把 `examples/axi4/emit_rtl_soc.cpp` 在 `emit-cli-into-example` 的「零 diff」判据下保持 | **单向顺序**：本 change 的 3.2 若先行 ⇒ 该判据失效 ⇒ ⇒ **3.2 MUST 在 `emit-cli-into-example` 之后** |

---

## 🚪 门（**开工前 MUST 闭**）

| 门 | 内容 | 闭法 |
|---|---|---|
| **G-1** | ✅ **已闭（2026-11-18 · architect 裁决已产出）** —— 载体 = 蓝图 `docs/architecture/2026-11-17-rtl-verification-single-source.md` 的 **§4.3 订正 + §4.3.1 逐符号切分表（9 符号 · 维度 = **规则 vs 值**）+ §4.3.2 形态裁决（**分支 A**）+ §7 R12** ⇒ ⇒ ④ **ADR-074「不覆盖」条无需改动**（它把下沉**指向**蓝图 §4.3 ⇒ 订正后指向仍有效）⇒ ⇒ ⑤ draft 活文档（**planner 侧**）+ 上游 active change（**builder 的 F0 ②**）同批订正 ⇒ ⇒ ⚠️ **F2–F9 的实施判据以蓝图 §4.3.1 / §4.3.2 为准** | 已闭 ⇒ **F2–F9 可开工**（⚠️ F0 的基线采集仍 MUST 早于任何代码改动） |
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
| 3.2 下沉后判据变恒绿 | **AC18** 的逐项常驻投毒 + **AC17** 的「下沉须有具名第二消费者」 | 回退 3.2 |
| **蓝图订正漏位置**（6 处：总表 · 缺口② · §4.3 行 1 · §4.3 行 2 · §4.3 末尾「不动」条 · §5.2 M4 + draft 活文档）⇒ 文档继续说谎 | **AC23**（按**锚点文本**定位 + 活文档同批订正 + 上游活跃 change 同批订正）⇒ ⇒ **AC22** 的切分表复算 | 蓝图回滚 |
| **默认模式扁平 SoC 产物**（实现偏离**已登记的交付契约**）在 **F7 落地前持续存在** | **蓝图 §7 R12**（已登记 · 两段式状态：裁决 ✅ / 落实 ⚠️ 未闭）+ 本表 + Out of Scope **三处登记**⇒ ⇒ **闭口判据 = 护栏 ⑧' 覆盖 `kind=soc`**（⇒ 蓝图 §4.3.2 代价清单已列该后置条件）⇒ ⇒ **不得等 F7 才登记** | — |
| **切分表留「待定」** ⇒ 实施面无法机械判定 | **AC22**（表内 MUST NOT 有「待定」+ 9 符号全覆盖） | 阻断开工 |
| **默认模式改层次化** ⇒ 护栏 ③ 的发射语义漂移 / ⑧ 在新产物上失效 | **AC24**（护栏 ③ 语义同步 + ⑧ 在**层次化**产物上仍绿 + 形态登记） | 回退 F7 |
| **基线采晚**（先改后采 ⇒ sha256 必然相同 ⇒ 恒绿） | **AC25**（先行 + 分模式两套 + 全落盘物 + 唯一变量） | — |
| **公开面登记落错载体** ⇒ 零判别力（子目录头不在扫描范围） | **AC19**（`example-only-headers.tsv` = 唯一真载体 + 如实声明扫描范围） | — |
| **verilator 缺席 ⇒ CHECK 3 静默 SKIP** ⇒ 「行为不变」覆盖未验证分支 | **AC18** 的已知边界 + **AC25** 的基线记录 MUST 含 verilator 可用性 | — |
| `CPPHDL_BUILD_EXAMPLES=OFF` 下锚点不匹配 | **AC20** 显式接受 + 注释指引 | — |

## 关联 change

- **前置**: `rtl-verification-single-source-2026-11`
- **前序（硬顺序）**: `axi4-verif-driven-cpp-2026-11` → `emit-cli-into-example-2026-11` → 本 change
