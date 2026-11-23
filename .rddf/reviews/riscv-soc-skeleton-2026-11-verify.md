# 验收报告：`riscv-soc-skeleton-2026-11`（S1 结构骨架 · SoC 级产品单元 `examples/riscv-soc`）

- 日期：**2026-11-23**
- 角色：**verifier**（builder 的固定审查者 · **只验不改** ⇒ 本次**未修改任何实现文件**，
  全部变异落在 `/tmp/opencode/` 的副本上；仓内 `git status` 在取证前后均为空）
- 验收对象：`main` @ `62337b0`（change delta = `4c04821..62337b0` · 2 个实施 commit：
  `2cab7fb` S1 主体 20 文件 + `62337b0` AC8 判据 3 文件）
- **判据结论**：`verification_state = passed` · `failed_acs = []` · 13/13 AC 通过
- **已用轮次**：**1**（本 change 无既有验收报告 ⇒ 首轮）
- **待修项**：4 项（**SHOULD_FIX / NITS** · 不构成 AC 失败 · 见 §4）

> ★ **分流说明**：`rddf rdd-verify --dry-run` 返回「**No eligible changes to verify**」
> （`iteration.json` 无本 change 条目 ⇒ rdd-builder P3 archive gate 未触发）
> ⇒ ⇒ **未命中** rdd-verifier skill 的 canonical preconditions ⇒ ⇒ 按 role 矩阵走
> **本地验收**（1–5 步：编译/静态检查 → 单元测试 → 集成端到端 → 边界与错误路径 → 遗留清扫复核），
> **判据来源 = proposal 的 `## 验收标准` 段（AC1–AC13）**。

---

## 1. 判据来源（按 verifier 产出契约的优先级）

| # | 来源 | 内容 |
|---|---|---|
| 1 | 用户本次明确给出的验收条件 | 无（本轮为「切 verifier 继续验收」的延续指令 · 沿用前序已确认的范围） |
| 2 | `openspec/changes/riscv-soc-skeleton-2026-11/proposal.md` §`## 验收标准` | **AC1–AC13**（本报告的逐条判定对象） |
| 3 | 本次变更自己声明的行为 | 2 个 commit 的 commit message + `tasks.md` 89/90 勾选 + `design.md` 的实施规格 |
| 4 | 关联 ADR | **ADR-077**（`Accepted` 2026-11-23 · D1 两级顶层 / D2 模块类型名 = 非契约 / D3 `prog_*` 长期保留 / D4 单行 `kind=soc`）· 签批 spec `rtl-soc-output`（观测集 ≥2 随激励变化的量 · 激励 ≥10 拍且值不同 · 粘滞信号不计入） |

> ★ **取证纪律**：AC1–AC13 **每条 ≥ 1 条真实工具输出**（`git diff` / 产物解析 / 独立编译的变异二进制 /
> 护栏 exit code / `RESULT:` 协议行）⇒ ⇒ **0 条 AC 靠 AC 文本或 builder 自述推断**。
> ⚠️ builder 的 commit message 中**一处论断不准确**（见 §4 F4）⇒ 本报告不采信其自述，全部重取。

---

## 2. 逐条判定（13 条 AC · 全部 PASS）

| AC | 判定 | 证据（工具输出摘要） |
|---|---|---|
| **AC1** 文件集齐 + 构建 0 error / 0 warning | ✅ pass | `examples/riscv-soc/` = 6 文件齐（`AGENTS.md` · `CMakeLists.txt` · `src/rv32i_soc_top.h` · `src/soc_trace_counter.h` · `tb_riscv_soc.cpp` · `tb_riscv_soc.sv`）· 设计头 `int main` 计数 = **0**（TB 有 1）⇒ 三角色分离成立。**强制重建**（`touch` 全部新文件后 `cmake --build build --target tb_riscv_soc`）⇒ exit=0 · **error = 0** · warning = 22，**归属全部** = `include/core/lnodeimpl.h`（12）+ `include/utils/logger.h`（10）· 与 riscv-soc 相关 = **0**。⚠️ **对照取证**：在被本 change 触碰的基线 target `tb_rv32i_soc` 上重建 ⇒ **同样 22 条**、warning 文本逐条同型（`-Wswitch-enum` / `-Wnonnull-compare`）⇒ ⇒ **新增 warning = 0**（22 条属库既有头噪声，范围外） |
| **AC2** 产物 module 集 == 手推集（10 · **集合相等**） | ✅ pass | verifier **独立**从产物提取：`grep -E "^module " top.v` ⇒ 10 个（`ExStage`/`ForwardingMux_32_`/`HazardUnit`/`IdStage`/`IfStage`/`MemStage`/`Rv32iCoreComplex`/`SocTraceCounter`/`WbStage`/`top`）⇒ 与 AC2 手推集 `diff` ⇒ **完全相同**（集合相等，非仅计数相等）· 库层 `CHECK 1 module 集: PASS (期望 10 个 · 实际 10 个)`。⚠️ **期望集非派生**：TB 里 `kExpectedModules` 是**字面量数组**（`constexpr` 风格硬编码）⇒ 满足「禁从成层声明派生」 |
| **AC3** 根端口表 == 手推 8 根（集合相等）· 后代未驱动 == 0 | ✅ pass | verifier **独立**提取顶层端口表 ⇒ 8 根，方向/位宽全对：`default_clock`(IN,1) `default_reset`(IN,1) `top_ext_rst`(IN,1) `top_prog_we`(IN,1) `top_prog_addr`(IN,32) `top_prog_data`(IN,32) `top_store_count`(OUT,16) `top_writeback_count`(OUT,16) ⇒ 与手推集**集合相等且无多余**。`manifest.port_table` size = **8** · names 与产物顶层端口**集合相等**。⚠️ **后代未驱动 = 0（独立复核）**：对 `Rv32iCoreComplex`（6 input）与 `SocTraceCounter`（4 input）**逐根**查「实例端口连接右侧 / assign 驱动」⇒ 未驱动 **0** 根。库层 `CHECK 2 顶层端口: PASS (端口 8/8 · 后代未驱动 0)` |
| **AC4** `check_products` CHECK 4（双形态逐拍 + 篡改自证）· `min_varying == 2` · **三条反向判据** | ✅ pass | `CHECK 4 双形态等价: PASS (ok=1 equal=1 变化量=2/2 自证=PASS [篡改被抓住（first_mismatch=cycle 0, observation 'top.store_count': flat=0x0 hier=0x65535])])`。**双形态确为两个不同产物**（取证）：`eq/flat/top.v` = **1 module** · `eq/hier/top.v` = **10 module** ⇒ 不是同一文件比两次。**篡改副本确为定点改动**：`diff hier_src.v hier_src.v.tampered` ⇒ **只差 1 行**（`assign top_trace_store_count = ~(top_trace_reg);`）。`min_varying_observations` 在 TB 里是 `constexpr = 2`（**不可调**）。★ **三条反向判据由 verifier 独立复现**（编译**变异副本**到 `/tmp`，仓内文件一字未改；每次先确认**编译 exit=0** ⇒ 排除「跑旧二进制假绿」）：① 移出 `writeback_count` 观测 ⇒ `CHECK 4 FAIL (变化量=1/1)` + `PRODUCTS-CHECK: FAIL` ② 两源**对调** ⇒ `RESULT: checks=7 fails=2`（stderr 明列 `CHECK FAIL: A3` + `CHECK FAIL: A4`）③ 两源**同源** ⇒ `RESULT: checks=7 fails=1`（stderr 明列 `CHECK FAIL: A4`）。⇒ 与期望源 §4 登记的三条实测**逐条一致**（登记值真实） |
| **AC5** 回归锚点（零改动 + 核级 24 条） | ✅ pass | ⚠️ **用「提交区间」形式取证**（`4c04821..HEAD`）⇒ 不受工作区并行 WIP 影响（见 §2.2）：`git diff --stat 4c04821..HEAD -- examples/riscv-mini/ include/` ⇒ **空** · `git diff --stat 4c04821..HEAD -- include/ src/` ⇒ **空**（库侧零改动）· 取证时刻的工作区形式（`git diff 4c04821 -- examples/riscv-mini/ include/` 与 `git status --porcelain` 同面）**亦为空**（并行 WIP 落盘于 07:50 之后，见 §2.2）· `tb_rv32i_soc` ⇒ `RESULT: checks=24 fails=0`（**在并行 WIP 落盘后重跑，仍绿**）。base SHA 落在 `4c04821`（= 实施首笔 `2cab7fb` 的父提交）⇒ 锚点判定成立 |
| **AC6** 核复合体端口契约（34 = 2 + 32）· 表写进 `AGENTS.md` · 独立提取 + 判别力自证 | ✅ pass | verifier **独立**提取产物 `Rv32iCoreComplex` 的 module 端口表 ⇒ **34** 根 ⇒ 与 `examples/riscv-soc/AGENTS.md` 的 34 行表**逐字逐序全等**（名 + 方向 + 位宽，序号 1–34 连续）⇒ 自有 **32** 根（去 `top_core_` 前缀）与 `verify_no_duplicate_rv32i_headers.sh` 的 **C6 快照**（32 条）**逐字相等** ⇒ 「头文件 → 产物 → 登记表」三方闭环。新护栏 `scripts/verify_riscv_soc_core_ports.sh` 正常模式 **V1–V5 全绿** + `--self-test` **7 条投毒各自必红**（P1 删行 / P2 改方向 / P3 改位宽 / P4 改端口名 / P5 与 C6 不符 / P6 微架构副本 / P7 兄弟边改接常量）⇒ 判别力自证成立 |
| **AC7** 双向等式（C++ L == `.sv` L == `interp_checks` == `min_checks`）· 变异自证 | ✅ pass | C++ 侧 `RESULT: checks=7 fails=0`（`./build/examples/riscv-soc/tb_riscv_soc`）· `.sv` 侧 verifier **独立**编译运行（护栏口径：TB 在前 + 产物在后 + `--binary --timing -CFLAGS -fcoroutines --x-assign unique`）⇒ 编译 exit=0 · **warning 0 条** · `RESULT: checks=7 fails=0` ⇒ **L = 7**。tsv 行：`interp_checks = 7` · `min_checks = 7` · `scenarios = 6`（必填）· `spec_checks = 3`（必填）⇒ **7 == 7 == 7 == 7**。★ **两侧同一组手推字面量**：`.sv` 的 `EXP_STORE=1` / `EXP_WB=3` ↔ C++ 的 `kExpectedStoreCount=1` / `kExpectedWritebackCount=3` ⇒ 两条计数断言**两侧同值**。★ **程序三方逐字节一致**（期望源 `.rddf/checks/soc-rtl-expectations.md#riscv-soc` ↔ `.sv` 的 `prog_word` case 表 ↔ C++ 的 `kHandEncoded`）⇒ 7 字全等（`02a00093/00000013/00b00113/00000013/002081b3/00302023/0000006f`）⇒ 期望源零新造。**变异自证（两侧各一，改 1 位期望）**：`.sv` `EXP_WB 3→4` ⇒ `RESULT: checks=7 fails=1`；C++ `kExpectedStoreCount 1→2` ⇒ `CHECK FAIL: A3` + `fails=1` ⇒ 该条必红 |
| **AC8** 兄弟边被走通（结构化证据） | ✅ pass | 产物内**结构化端口连接**（非散文）：`top.v:3453` `.top_trace_store_event(top_core_obs_data_write_en),` · `top.v:3454` `.top_trace_retire_event(top_core_dbg_wb_write_en),` ⇒ 两个事件输入**均由核复合体的输出驱动** ⇒ 真 child→child 边（ADR-070 D4③）· 顶层 input 表里 `top_trace_*` 是 `SocTraceCounter` 的**子 module 端口**（不是顶层端口）⇒ 无「顶层 input 冒充兄弟边」的形态。护栏 **V5 PASS** + **P7 投毒必红**（改接 `1'b0` ⇒ 红） |
| **AC9** 登记守恒（28 → 29 · `29 == 17 ∪ 12` · `kind=soc` · 锚点不动） | ✅ pass | CMake 跟踪数（护栏同款口径）= **29**，含 `tb_riscv_soc` ✔ · 守恒护栏 `verify_examples_rtl_sim_coverage.sh` ⇒ `① 守恒：被跟踪 29 == 已覆盖 17 ∪ 已登记落选 12（且互斥）` **PASS**（+ ② 落选登记完备 + ③ 列纯度）· tsv 新增 1 行 `kind=soc`（14 列）· `REGISTERED_SOC: 2`（1→2）· `REGISTERED_HIER: 1` **未动** · `REGISTERED_TARGETS: 15` **未动** · 根 `AGENTS.md`「examples 的 main() 程序」= **33**（实测 `grep -rl '^int main' examples --include='*.cpp' | wc -l` = **33**）· ⚠️ `CTEST_TOTAL` / `EXAMPLES_TEST_SOURCES` / `VERILATOR_E2E_TOTAL` / `DUAL_BACKEND_*` **四个锚点在本 change 的 diff 里零改动**（`git diff 4c04821..HEAD -- AGENTS.md` 仅命中 main() 计数行） |
| **AC10** 索引行（builder 义务）· 状态词 `**draft**` | ✅ pass | `improvement-suggestions.md:25` = `| [rv32i-soc-platform-2026-11](…) | P1 | … | 2026-11-22 | 📝 **draft**（立项 · …` · frontmatter `状态: draft` ⇒ **逐词一致** · `verify_improvement_index.sh` ⇒ `✅ 索引状态列与 frontmatter 状态: 逐行一致（17 行）` **PASS** |
| **AC11** 护栏 — **新面**（9 项） | ✅ pass | `check_products` 四件套 ⇒ `CHECK 1..4` 全 PASS · `verify_examples_rtl_sim.sh` ⇒ **PASS（18 项）** · `verify_examples_rtl_sim.sh --self-test` ⇒ **PASS**（21 条投毒各自转红 + 2 条必绿 + 还原复绿）· `verify_examples_rtl_sim_coverage.sh` ⇒ PASS · `…_coverage.sh --self-test` ⇒ PASS · `verify_example_only_headers` ⇒ PASS · `verify_downstream_consumer` ⇒ PASS（R1 fixture · R2 install→临时 prefix · R3 产物齐备 · R4/R5 独立工程 build+run · R6 三种持有形态）· `verify_port_naming` ⇒ PASS · `verify_component_naming` ⇒ PASS · `verify_no_duplicate_rv32i_headers` ⇒ PASS。**新护栏** `verify_riscv_soc_core_ports.sh`（正常 + `--self-test` 7 投毒）⇒ PASS |
| **AC12** 护栏 — **回归面**（11 项） | ✅ pass | `verify_example_test_wiring` PASS · `verify_test_counts` PASS · `verify_flat_rtl_emit` PASS · `verify_hierarchical_rtl` PASS（R1–R8 + R8′ 34 根手推期望集）· `verify_example_assertion_quality` PASS · `verify_guard_reachability` PASS（56 脚本 · 登记表双向 1:1）· `verify_docs_links` PASS · `verify_roadmap_fragments` PASS · `verify_agents_size` PASS · `verify_improvement_index` PASS · `./run_all_ported_tests.sh` ⇒ **PASS 29 / FAIL 0 / STALE 0 / MISSING 0**（`STALE-ARTIFACT: 16` = 旧构建目录残留二进制（target 早已删除 · 护栏自述「不是归属问题」）⇒ 与本 change 无关）。★ **另附 ctest 全量回归**（verifier 独立跑，非 AC 明列）：`ctest --output-on-failure` ⇒ **100% tests passed, 0 tests failed out of 273**（`perf_regression` 本轮 **Passed** ⇒ 印证 builder 报告的「首轮唯一失败 = 冷启动伪红」诊断：同组其它行 0.42–0.79x 更快、单独重跑即绿 ⇒ 与本 change 零 `include/`·`src/` 改动相符）。⚠️ **该结论有一处并行污染窗口**（07:51–07:53 另一会话重建了两个二进制）⇒ 详见 §2.2 的影响评估 |
| **AC13** 平台声明（CI 证据仅 Ubuntu） | ✅ pass | `examples/riscv-soc/AGENTS.md:116` = 「⚠️ `--check-products` 的 lint / 双形态依赖 verilator ⇒ **CI 证据仅 Ubuntu**（ADR-035）」⇒ 声明存在且指名 ADR-035。⚠️ 见 §4 **F4**：builder 的 commit message 称 `developer_guide §0` 也写了该句 —— **实测 §0 无此句**（该文件里的「Ubuntu」命中数为 0）⇒ 声明的**唯一**载体是 `examples/riscv-soc/AGENTS.md`（AC13 的字面要求「文档 MUST 声明」**满足**，但 commit message 的表述不准确） |

### 2.1 边界与错误路径（AC 未单列 · verifier 补做）

| 项 | 证据 |
|---|---|
| 默认模式**未知参数** ⇒ fail-closed | `./tb_riscv_soc --bogus-flag` ⇒ **exit 2** · stderr = `FATAL: 未知参数（默认模式只接受无参）：--bogus-flag` |
| 发射模式**缺 `--out`** ⇒ fail-closed | `./tb_riscv_soc --emit-rtl` ⇒ **exit 2** · stderr = `FATAL: --emit-rtl / --check-products 缺 --out <dir>` |
| 计数器**饱和**语义在产物里成立 | 产物 `SocTraceCounter`：`always_ff @(posedge default_clock or posedge default_reset)` + `if (default_reset) top_trace_reg <= 16'h0;` ⇒ 复位清零 ✔ · 饱和 mux `top_trace_eq_eq = top_trace__reg == 16'hffff` ⇒ 达上限后保持 ✔（无回绕 UB） |
| 层次化**观测盲区**（已知边界 · 如实确认） | 平形态产物 `eq/flat/top.v` 的顶层端口 = **134**（后代输出全部冒泡）· 层次形态 = **8** ⇒ 二者**端口面不等价**（行为等价、端口面不等价）⇒ 与根 `AGENTS.md` KNOWN LIMITATIONS「成层子树的 `ch_out` 被移出顶层 ⇒ 顶层读不到」**同型** ⇒ 属**已登记的已知边界**，非本 change 缺陷 |

---

### 2.2 ⚠️ 取证期间的并行会话（工作区非静 · 如实登记）

取证窗口（07:12–07:53）内**另一会话**在本仓落下**未提交 WIP**（与本 change 无关）：

| 文件 | WIP 主题 | 落盘时刻 |
|---|---|---|
| `examples/riscv-mini/tb_rv32i_soc.cpp`（+213 行） | 固件装载链路「方案 a：ELF → init_data 向量 → 核取指」 | **07:52:36** |
| `tools/rv32i-fw-dump/main.cpp`（+186/−32） | 新增 `--format mem-split` + 更严的 ELF 校验（`SHT_RELA` / 入口须 == `kPcReset`） | **07:50:55** |
| `examples/riscv-soc/src/rv32i_soc_top.h`（+25/−5） | **S1 产品自身的**「固件初值透传」：`Rv32iSocTop` ctor 增 `itcm_init`/`dtcm_init`（默认空 ⇒ 零行为变化）+ `ch_layer<Rv32iPipelineTop, const std::vector<uint32_t>&, …>`（**显式**给出 `Args…` ⇒ 类模板参数不从构造实参推导） | **07:55 前后** |
| `examples/riscv-soc/tb_riscv_soc.cpp`（+134 行） | 同主题的 TB 侧扩展（新增固件初值模式） | **07:55 前后** |

⇒ ⇒ **影响评估**（逐项核实，非推断）：

1. **S1 的产物面零影响** —— 前两处 WIP **都不在** `examples/riscv-soc/**`；后两处**虽在 S1 文件里**，但属**另一 change 的在途实现**（固件装载链路 L2）⇒ ⇒ **S1 的验收对象仍是已提交的 `4c04821..62337b0`**（本报告的判定基于提交区间，不基于工作区）。
2. ★ **WIP 与 S1 契约兼容性 = 已实测通过**（避免「他人 WIP 把 S1 打坏」而不自知）：在**含 WIP 的当前树**上重建并复跑 ⇒ 默认模式 `RESULT: checks=7 fails=0` ✔ · 34 端口契约护栏 V1–V5 **全绿** ✔ · 四件套 `CHECK 1..4` **全 PASS**（module 集 **10/10** · 端口 **8/8** · 后代未驱动 **0** · lint PASS · 变化量 **2/2** + 篡改被抓住）✔ ⇒ ⇒ **WIP 未破坏 S1 的任何契约**（`ch_layer` 的 `Args…` 显式给出后，模块类型名 `Rv32iCoreComplex` 仍在实例处给出 ⇒ 产物形态不变）。
3. **AC5 的取证改用「提交区间」形式**（`4c04821..HEAD`）⇒ 该形式**只看提交**，不受工作区 WIP 影响 ⇒ 结论成立（见 §2 的 AC5 行）。
4. **ctest 有一处污染窗口**（诚实登记）：另一会话在 **07:51–07:52** 重建了 `build/examples/riscv-mini/tb_rv32i_soc` 与 `build/tools/rv32i-fw-dump`，而 ctest 在 **07:53:21** 结束 ⇒ ⇒ 尾段约 2 分钟的用例可能落在他人重建后的二进制上 ⇒ ⇒ **273/273 的结论应读作「在并行 WIP 之下亦无回归」**，而**不是**「对 S1 基线的纯净测量」。⇒ S1 的纯净回归证据另有三条：① `run_all_ported_tests.sh` = 29/29（跑在 07:10 前，工作区尚静）② S1 的 13 条 AC 各自的独立取证（全部在 07:12–07:53 的**前半段**、工作区尚静时取得）③ 当前树复跑 `tb_riscv_soc` 7/7 + 34 端口护栏 PASS + 核级 24/24 仍绿。
5. **verifier 不触碰他人 WIP** ⇒ 未 checkout / 未 stash / 未 `git add` / 未提交这四处改动；⚠️ 归档提交时**必须按路径显式 `git add`**（仓内 AGENTS.md 已登记「多人 / 多会话并行 ⇒ `git add -A` 曾误纳他人未完成文档」的实测事故）。

---

## 3. 遗留清扫复核（verifier 独立执行 · 本 change 范围）

| 检查项 | 范围 | 结果 |
|---|---|---|
| `TODO` / `FIXME` / `XXX` / `HACK` / `WIP:` | `examples/riscv-soc/**` + `scripts/verify_riscv_soc_core_ports.sh` | **0** |
| `SKIP()` / 空 `TEST_CASE` | 同上 | **0**（本 change 无 Catch2 用例 · 判据在 TB 协议行） |
| 注释掉的代码块 | 同上 | **0** |
| 临时调试输出（`printf` / `DEBUG`） | 同上 | **0**（stderr 仅 `CHECK FAIL:` / `FATAL:` 两条协议） |
| 未替换的 mock / stub / 临时 fallback | 同上 | **0** |
| 未使用分支（`--firmware-init` 等残留） | `tb_riscv_soc.cpp` | **0** |

⇒ **判定：清扫完成，无尾巴。**

---

## 4. 待修项（SHOULD_FIX / NITS · **不构成 AC 失败** · 路由回 builder）

| # | 级别 | 项 | 证据 / 复现 | 建议 |
|---|---|---|---|---|
| **F1** | SHOULD_FIX | `docs/developer_guide/rv32i-rtl-delivery-and-sim.md:295` 的登记守恒数**陈旧**：仍写 `28 == 10 ∪ 18` ⇒ 当前真值是 **`29 == 17 ∪ 12`** | `grep -n "登记守恒" docs/developer_guide/rv32i-rtl-delivery-and-sim.md` ⇒ `| 登记守恒（\`28 == 10 ∪ 18\` · 三桶） | …` | ⚠️ 该漂移**先于**本 change 存在（base `4c04821` 同句），但**本 change 改了同一文件**且 AC9 明确写了新数字 ⇒ 归档前顺手订正为 `29 == 17 ∪ 12`（1 行） |
| **F2** | SHOULD_FIX | 新护栏的**投毒数**在 3 处仍写 **6**，实为 **7**（`62337b0` 增了 P7） | ① `docs/CHANGELOG.md:15`「…6 条投毒各自必红」② `docs/developer_guide/…:48` 行⑪「…6 条投毒各自必红」③ `openspec/changes/…/design.md:190`「含 `--self-test` 6 条投毒」⇒ **同文件 `:207` 却写 7** ⇒ 自相矛盾 | 三处统一为 **7**（并补 V5 名称） |
| **F3** | NITS | `docs/developer_guide` 行⑪ 对新护栏的描述**漏 V5**（只写「34 端口 + 无微架构副本」） | 该行描述 vs 护栏实际 V1–V5 | 补「+ 两条兄弟边（AC8）」 |
| **F4** | NITS | builder 的 commit message 论断不准确：称 `developer_guide §0` 写了「CI 证据仅 Ubuntu」 | `grep -c "Ubuntu" docs/developer_guide/rv32i-rtl-delivery-and-sim.md` = **0**（§0 无此句）· 唯一载体 = `examples/riscv-soc/AGENTS.md:116` | 声明本身**已满足 AC13**；建议后续 commit message 不要再引用不存在的载体 |

> **不阻塞归档**：F1–F4 均为**文档漂移**，不改变任何可观测行为；AC1–AC13 全部通过 ⇒ verdict = `passed`。
> 若选择归档前修，成本 ≈ 4 行文本改动（建议回 builder 用 1 个 commit 收口，避免与归档 commit 混在一起）。

---

## 5. 归档裁决建议

| 项 | 结论 |
|---|---|
| **AC 判定** | ✅ **13/13 PASS**（`failed_acs = []`） |
| **归档命令** | `openspec archive riscv-soc-skeleton-2026-11 --yes --skip-specs` |
| **`--skip-specs` 的依据** | `git diff --stat 4c04821..HEAD -- openspec/specs/` ⇒ **空**（本 change **零 spec delta** ⇒ 无需 merge）⇒ 先例一致（同仓 6/6 例外均如此） |
| **归档前建议** | ① 先由 builder 收口 F1–F4（可选）② `tasks.md` 仅剩 **7.4 归档**未勾 ⇒ 归档后勾选 |
| **归档后的状态同步** | `.rddf/improvements/rv32i-soc-platform-2026-11.md` 的 `进度:` 字段仍写「**未开工**」⇒ **planner 义务**（S1 落地后更新为「S1 已归档」）。⚠️ 该文件属 planner 写集 ⇒ verifier 只登记、不代改 |
| **未 push** | `4c04821` / `2cab7fb` / `62337b0` 三个 commit 仍在本地 ⇒ **`git push` 须用户确认**（本报告不代为 push） |
| **⚠️ 归档提交纪律** | 归档时工作区**含另一会话的未提交 WIP**（§2.2）⇒ ⇒ MUST **按路径显式 `git add`**（`openspec/changes/riscv-soc-skeleton-2026-11/` · `tasks.md`）⇒ **MUST NOT** `git add -A` / `git add .`（会把他人 WIP 一并纳入）⇒ ⇒ 归档 commit 后 `git status` 应**仍显示**那两处 WIP 未提交 |

---

## 6. 取证产物清单（`/tmp/opencode/` · 非仓内）

| 文件 | 用途 |
|---|---|
| `v_soc_chk/` | verifier 独立发射的 SoC 产物 + `manifest.json` + `eq/`（平/层次两形态 + 篡改副本 + trace） |
| `v_ac4_mutations.sh` · `v_ac4_mut/` | AC4 三条反向判据的**变异副本**（编译到 /tmp · 仓内零改动） |
| `v_ac7_check.sh` | AC7 双向等式 + 两侧各一条「改 1 位期望」变异 |
| `v_ctest.log` | `ctest` 全量回归（273 条） |
| `vg*/` | 22 个护栏的逐个日志 |

> ★ **verifier 未修改仓内任何文件** —— 全部取证前后 `git status --short` 均为空；
> 仓内护栏的 `--self-test` 会临时投毒并还原 ⇒ 本轮跑完后工作区**实测为空**。
