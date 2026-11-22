---
name: rv32i-soc-platform-2026-11
priority: P1
phase: phase-6
category: framework
type: feature
状态: draft
进度: "**立项（2026-11-22 · planner）** · 未开工 · 计划 **4 项**：**S0 决策门（architect）** → **S1 结构骨架** → **S2 MEM 反压** → **S3 总线主口 + L1** · ✅ **S0 决策门已闭（ADR-077 `Accepted` 2026-11-23 · G-1…G-4 = D1…D4）⇒ S1 可开工** · ✅ **metis 第 1 轮已处置（4 P0 + 7 P1 + 5 P2 · 见文末）** · ✅ **2026-11-23 proposal 已同步 ADR-077 的权威数字**（端口 8 · `min_varying == 2` · `SocTraceCounter` · 7 字程序）"
主题: RV32I 从「示例 SoC」走向「可组合 SoC 平台」—— 核心复合体 / SoC 根两级产品单元 + 总线化演进
依赖: **ADR-077**（两级顶层 + SoC 组合的四条形态裁决 · **Accepted** 2026-11-23 · **S0 决策门的载体**）· ADR-046（微架构唯一权威 · §D1 已由 ADR-077 追记细分）· ADR-061（成层粒度显式声明）· ADR-070（RV32I 成层 + 兄弟边 · D4 条件③）· ADR-072（产品端口面 + 四件套）· ADR-073（SoC 产物形态 + SoC 级 TB）· ADR-074（验证单一真相源）· `rv32i-privileged-csr-2026-11`（**S5 的硬前置** · draft · 不属本项）
来源: 用户 2026-11-22「想把 RV32I 核做成真实可替换单元 —— 后续加 L1/L2 缓冲、MMU、内存控制器、总线」+「riscv-mini 可看成 riscv-soc 的 spike」+「riscv-mini 改造成核复合体，另建 examples/riscv-soc 实施 Rv32iSocTop」
生成时间: 2026-11-22
evidence: probe:scripts/probe_r21_hier_granularity.sh
---

# Improvement: RV32I 可组合 SoC 平台（核心复合体 ↔ SoC 根 · 总线化演进）

**主题**: 真层次化 RTL 输出

> 具体对象 = 把「一个组件既是核又是 SoC」拆成 **两个产品单元**，并给出通往 L1/L2 · 总线的分批路径。
> （frontmatter 的 `主题:` 是完整表述；本行取**路线图主题**以过 `add-improve` 的 HARD-GATE 匹配。）

---

## 架构依据

| # | 权威 | 对本项的约束 |
|---|---|---|
| ⓪ | **ADR-077**（**Accepted** 2026-11-23 · **S0 决策门**） | **D1** 两级顶层 + 判据分域（「8」只约束核复合体 · SoC 根 = 10）· **D2** 模块类型名 = 非契约（登记在产品登记面）· **D3** `prog_*` 长期保留根端口 · **D4** 单行 `kind=soc` ⇒ ⇒ **本项的权威数字来源 = ADR-077 的 V1 / V2 / V4** |
| ① | **ADR-046**（`include/cpu/` = 微架构唯一权威；`examples/riscv-mini/src/` 收敛后**只留 SoC 顶层**） | ✅ **字面张力已闭**：ADR-077 把它解释为「消除同名重复」而非「全仓只许一个 example 顶层」，并以 dated addendum（2026-11-23）把第 1 类条目细分为**两个装配层级**（**G-1** 已裁） |
| ② | **ADR-061 决策①**（成层**显式**声明 · 默认 inline） | 新根与新 IP 的成层声明 MUST 显式写 |
| ③ | **ADR-070 D2 / D3 / D4** | D2 的 7 个成层点是既有裁决（**不改**）· D3：**模块类型名 = 明文非契约**（用户 2026-11-15 裁决 · 改名**不**构成破坏性变更，但结构判据 MUST 重算）· D4 条件③：产物 MUST 有**至少一条真实兄弟边**（S1 = **两条**） |
| ④ | **ADR-072 D1 / D1-h** | 产品端口面 = 框架 2 + **根自有 IO**；后代 `ch_out` 降为体内 `logic`；**冒泡机制已退役** |
| ⑤ | **ADR-073**（SoC 产物 = 层次化 · 每 IP 一 module · **每个后代 `ch_in` 必须有驱动或 tie-off** · SoC 级 TB = 独立 Verilog TB） | 新 SoC 根按同一纪律建；模板 = `examples/axi4/axi_soc_example_top.h` |
| ⑥ | **ADR-074** | 期望值 MUST 手推；`.sv` 生成不了（RV32I 的 4 个形态）⇒ 手写 + `generated=no` 终态 |
| ⑦ | **`check_products`**（`include/rtl_emit/product_check.h`） | 四件套自检 = ① module 集 ② 顶层端口数 + 后代未驱动 == 0 ③ lint ④ 双形态逐拍 + **篡改自证** ⇒ S1 MUST 走它（先例 `tb_axi_soc_example.cpp:60-86`） |

### A.1 本轮**实测**事实（2026-11-22 · 非推断）

| # | 事实 | 证据 |
|---|---|---|
| ① | 现状产品 = **34 端口 / 8 module**（7 个 `ch_layer` stage + `top`） | `build/rv32i-rtl-sim/hier/top.v` |
| ② | 现有 24 条判据**全部是核级观测**（`dbg_id_*` / `dbg_wb_*` / `obs_data_*` / `obs_perf_instructions` / `prog_*`） | `examples/riscv-mini/tb_rv32i_soc.cpp:222-258` |
| ③ | **子层不会折叠**：`Rv32iPipeline` 成层 ⇒ 9 module（其 7 个子层**仍在**）⇒ ⇒ 新 SoC 的 module 集 = **10** | `/tmp` 对照实验 + `rv32i_pipeline.h:140-153` |
| ④ | `InstrTCM`/`DataTCM` 成层 ⇒ 10 module（mem 数组 + `initial` 搬进子 module） | 同上 |
| ⑤ | `Granularity::ForceLayer` ⇒ **`ok=true`**（ADR-070 当时记录的 23 条 fail-closed **已不成立**） | `bash scripts/probe_r21_hier_granularity.sh` |
| ⑥ | **MEM 级无反压**：`ch_in<ch_bool> data_ready` 声明于 `rv32i_pipeline.h:69` 但**零消费者** | `grep -n data_ready` 仅命中声明行 |
| ⑦ | **缓存 wrapper 无 burst 口**（只有标量 `axi_addr/axi_valid/axi_ready/axi_data`） | `include/cpu/cache/i_cache_prefetch.h:75-78` |
| ⑧ | `hier_eq::compare` 机械拒绝：**观测集为空** / **无随激励变化信号** / 观测**非根 `ch_out`** | `include/hierarchy_equivalence.h:43-51` |
| ⑨ | TCM 的 `ready` **恒 1** ⇒ 用现有产品**永远**进不了 stall 分支 | `include/cpu/pipeline/rv32i_tcm.h:73,168` |
| ⑩ | 测试台硬编码 module 数 = 8（改名/加层必红） | `examples/riscv-mini/tests/test_rv32i_hier_rtl.cpp:169` |

### A.2 已**否掉**的形态（不要重提）

| 形态 | 为什么否 |
|---|---|
| **只把 TCM 成层**（`pipeline` 仍 inline） | **ADR-070 D4 明文反例**：`dtcm ↔ pipeline` 成父子边 ⇒ 兄弟边 0 ⇒ 不满足条件③ |
| **S1 用 `AxiLiteGpio` 全输入 tie-off 占位** | ① 制造**恒 0 死组件**（ZERO-DEBT 反面）② **兄弟边仍为 0**（tie-off 与兄弟边无关，是"没有 child→child 连线"）③ 根无变化输出 ⇒ `hier_eq` 机械拒绝（事实⑧） |
| 一步到位（骨架 + 总线 + 缓存同批） | 反压缺失（事实⑥）⇒ 只能"假 ready" ⇒ **行为错但零报错** |
| 把 S4（L2/内存控制器）/ S5（MMU）写进本项 | 过度规划：S5 已归 `rv32i-privileged-csr-2026-11`；S4 现写不出机械 AC ⇒ **只留索引行** |

---

## 术语（本文件与 proposal 的唯一口径）

| 术语 | 定义 |
|---|---|
| **核复合体** | `Rv32iPipelineTop`（pipeline + ITCM + DTCM）· 34 端口 · `examples/riscv-mini` 的产品单元 |
| **SoC 根** | `Rv32iSocTop` 所在的根组件 · 产品端口面 = 框架 2 + **根 `__io` 声明数** |
| **产品单元** | 一个「四件套」（manifest + RTL 产物 + 独立 TB + 覆盖登记） |
| **形态行** | `examples-rtl-sim-targets.tsv` 里的一行（`kind` ∈ {target, soc, hier, dut}） |
| **根自有 N** | 根组件 `__io(...)` 里声明的端口数（**不含**框架时钟/复位） |
| **兄弟边** | 两个**成层子 module 之间**的连线（ADR-070 D1 修复的那条分支） |

---

## 范围

### IN

- **S0 决策门**（architect）：G-1 … G-4（见文末）
- **S1 结构骨架**：`examples/riscv-soc/` + `Rv32iSocTop`（核作 `ch_layer`）+ **`SocTraceCounter`**（核→兄弟的两条真实连线）+ SoC 级 TB（走 `check_products` 四件套）+ 登记（**核零改动**）
- **S2 MEM 反压**：`data_ready` 真消费（`ready ≡ 1` 时**时序等价**）
- **S3 总线主口 + L1**：数据侧总线化（**新增**总线路径、**不删**根 `prog_*`）+ L1 接入（吃 S2 的反压）

### OUT（不做 · 已登记归属）

| 项 | 归属 |
|---|---|
| 特权级 / CSR / trap / 中断 / **MMU** | `rv32i-privileged-csr-2026-11`（draft · **S5 硬前置**） |
| **L2 / 内存控制器** | **后续立项索引**（本项只留一行；现无可写机械 AC） |
| 总线互联 IP 的新实现 | 复用 `include/axi4/**` |
| 缓存替换策略 / 一致性协议 | 各自独立立项 |
| 改名（`Rv32iPipelineTop` → `Rv32iCoreTop`；`tb_rv32i_soc` → `tb_rv32i_core`） | **单列一批**（R3） |

### 写面（按批分级 · 修正原「`include/cpu/**` 一字不动」的自相矛盾）

| 批 | 允许触及 |
|---|---|
| **S1** | 仅 `examples/riscv-soc/**` + 登记面 + 文档（`include/**` 与 `examples/riscv-mini/**` **零改动**） |
| **S2** | + `include/cpu/pipeline/**`（消费 `data_ready`） |
| **S3** | + `include/cpu/cache/**`（burst 口）+ 需 `rv32i-memory-timing` 的 delta |

---

## Why

1. **"核"今天不可替换**：`Rv32iPipelineTop` 同时持有 pipeline 与 ITCM/DTCM ⇒ 换缓存化的核要连带换存储器 / TB / 登记。
2. **后续每个诉求都卡在同一处**：L1/L2 要总线口 · MMU 要特权级 · 内存控制器要总线 —— 而今天**没有"核的对外接口"这个概念**（对外只有内部 `dbg_*`）。
3. **验证面可零重写**：24 条判据全部观测核级信号 ⇒ 核复合体留 `examples/riscv-mini` ⇒ **文件与判据零改动**（AC5 钉死）。
4. **框架已具备**：`ch_layer` + ADR-073 纪律 + `check_products` ⇒ 缺的是**装配与登记**。

---

## What Changes

### 批次表（每批一个 change · 每批独立可验收）

| 批 | change（拟定） | 内容 | 行为变化 | 前置 |
|---|---|---|---|---|
| **S0** | 决策单（architect · 非 change） | G-1…G-4 | 无 | — |
| **S1** | `riscv-soc-skeleton-2026-11` | 新目录 + `Rv32iSocTop`（`ch_layer<Rv32iPipelineTop>` + `ch_layer<SocTraceCounter>`）+ SoC 级 TB（`check_products` 4 项 + 解释器路径手推断言）+ 登记 | **零**（核不动） | S0 ✅（ADR-077 Accepted 2026-11-23） |
| **S2** | `rv32i-mem-backpressure-2026-11` | `data_ready` → hazard/IF 联动（`ready ≡ 1` ⇒ 时序不变）+ **`ready=0` 的测试 wrapper**（TCM ready 恒 1 ⇒ 事实⑨） | **功能等价**（`ready≡1` 下） | S0 |
| **S3** | `rv32i-bus-master-and-l1-2026-11` | 数据侧 AXI4 主口 + L1 + TCM 旁路开关（**新增**路径、**不删** `prog_*` 根端口） | 新增时序契约（hit/miss 拍数） | S2 |
| **S4/S5** | 索引行（不属本项） | L2/内存控制器 → `rv32i-privileged-csr-2026-11` | — | — |

### S1 的变更面

| # | 变更 | 落点 |
|---|---|---|
| S1-① | `Rv32iSocTop`（根 · 无 `main`） | `examples/riscv-soc/src/rv32i_soc_top.h` |
| S1-② | `ch_layer<Rv32iPipelineTop> core{"core", "Rv32iCoreComplex"}`（**类型名 = 产物级契约名** · ADR-070 D3） | 同上 |
| S1-③ | `ch_layer<SocTraceCounter> trace{"trace", "SocTraceCounter"}`：`store_event ← core.io().obs_data_write_en` + `retire_event ← core.io().dbg_wb_write_en` ⇒ **两条真实兄弟边** · 两个计数 → 根 `ch_out` | 同上 + `examples/riscv-soc/src/soc_trace_counter.h` |
| S1-④ | 根 `__io`（手推 **6** 根：`ext_rst` / `prog_we` / `prog_addr[31:0]` / `prog_data[31:0]` / `store_count[15:0]` / `writeback_count[15:0]`） | 同上 |
| S1-⑤ | 激励驱动（默认 = 解释器验证 · `--emit-rtl [--hierarchical] --out` = 发射 · fail-closed） | `examples/riscv-soc/tb_riscv_soc.cpp` |
| S1-⑥ | SoC 级独立 Verilog TB（**只读根端口**） | `examples/riscv-soc/tb_riscv_soc.sv` |
| S1-⑦ | 构建 + 跟踪（`CH_TRACKED_EXAMPLE_TARGETS` 28 → 29） | `examples/riscv-soc/CMakeLists.txt` · `examples/CMakeLists.txt` |
| S1-⑧ | 覆盖登记 + 期望源 | `examples-rtl-sim-targets.tsv` 新行 · `exclusions.tsv` 守恒 · **`.rddf/checks/soc-rtl-expectations.md`** 新段 `#riscv-soc`（ADR-073 先例） |
| S1-⑨ | 文档 | `examples/riscv-soc/AGENTS.md` · root `AGENTS.md` · `docs/developer_guide/rv32i-rtl-delivery-and-sim.md` · `docs/usage_guide/02-component-design.md` · `docs/CHANGELOG.md` |

---

## Acceptance

> ⚠️ 期望值 MUST **手推**（禁抄仿真输出）· 计数 MUST **从产物/清单派生**（禁裸数字）。

### S1

- [ ] **AC1** `examples/riscv-soc/` 文件集齐（①–⑦）· `cmake --build` 0 error / 0 warning
- [ ] **AC2** 🔴 **产物 module 集 == 手推集（集合相等 · `check_products` CHECK 1）**：`{top, Rv32iCoreComplex, SocTraceCounter, IfStage, IdStage, ExStage, MemStage, WbStage, HazardUnit, ForwardingMux_32_}` = **10**（⚠️ **不是 3** —— 子层不折叠，事实③）· **期望集写死在 TB 里**（禁从成层声明派生 ⇒ 否则"漏一个声明"同时改期望 ⇒ 恒绿）
- [ ] **AC3** 🔴 **根端口表 == 手推 8 根**（框架 2 + 根自有 **6**：`ext_rst` / `prog_we` / `prog_addr` / `prog_data` / `store_count` / `writeback_count`）· **集合相等**（自动排除后代端口 ⇒ 不需要 grep `top_pipeline_*`：真实前缀是 `top_core_*`，且冒泡机制已退役，事实④）· 且 `check_products` CHECK 2：顶层端口数 == **8**（含框架 2）且**后代未驱动端口 == 0**
- [ ] **AC4** 🔴 `check_products` CHECK 4：**双形态逐拍等价 + 篡改自证**（观测 = `store_count` + `writeback_count` · 手推 **1** 与 **3**（互不相同）⇒ 满足 `hier_eq` 的非空洞条件，事实⑧）· `min_varying_observations == **2**`（签批 spec `rtl-soc-output` 硬要求）· **三条反向判据**（移出观测集 / 两源接反 / 两源同源 ⇒ 各自必红）
- [ ] **AC5** 🔴 **回归锚点**：`git diff --stat <S1-base-sha> -- examples/riscv-mini/ include/cpu/` 为空 **且** `git status --porcelain examples/riscv-mini/` 为空 **且** `examples/riscv-mini/tb_rv32i_soc` 仍 `RESULT: checks=24 fails=0`（`<S1-base-sha>` 由 proposal 钉死，MUST NOT 用「工作树为空」代替）
- [ ] **AC6** 🔴 **核复合体端口契约**：`Rv32iCoreComplex` module 的端口表 == 手推集（**34** = 框架 2 + 核自有 32 = 4 in + 28 out）· 写进 `examples/riscv-soc/AGENTS.md` ⇒ 这是"可替换单元"的**唯一物证**，也是 S3 的变更基线 ⇒ ⇒ ⚠️ **判据载体 MUST 新建**（`manifest.port_table[]` 只覆盖顶层 ⇒ 子 module 端口表需独立提取 + 判别力自证）
- [ ] **AC7** 🔴 **双向等式**：解释器路径 `RESULT: checks=L fails=0` 且 `.sv` 侧 `checks=L fails=0`，其中 **`interp_checks == min_checks == L`**（同一手推字面量）· **两条计数断言两侧同值**（`store_count == 1` ∧ `writeback_count == 3`）· tsv 的 `scenarios` / `spec_checks` 两列**必填**（先例 4/10 · 6/22）· 变异自证：任一期望改 1 位 ⇒ 该条必红
- [ ] **AC8** 🔴 **兄弟边被走通**（= ADR-070 D4 条件③ 当场达标）：发射结果里**该边**出现在兄弟边分支（结构化证据：`HierarchyEmitResult` 的相关计数/`rewired_pins` 非空并点名该边）⇒ **禁止**用散文"已达标"代替
- [ ] **AC9** 登记守恒：`CH_TRACKED_EXAMPLE_TARGETS` **28 → 29** · `exclusions.tsv` 等式 **`29 == 17 ∪ 12`** · tsv 新行 `kind=soc`（`REGISTERED_SOC` 1→2，`REGISTERED_HIER` **不动** · G-4 建议）· `EXAMPLES_TEST_SOURCES` **不变**（无 Catch2）· `CTEST_TOTAL` **不变**（**不** `add_test`，同 `tb_rv32i_soc`）· root `AGENTS.md` 的「examples main() 32 → **33**」
- [ ] **AC10** **索引行**（builder 义务 · 写集约束）：S1 第一笔提交补根 `improvement-suggestions.md` 的行（状态词 MUST 是 `**draft**`，与 frontmatter `状态: draft` 逐词一致）+ `verify_improvement_index.sh` 绿（**planner 不能写根索引文件** ⇒ 见文末「待粘贴索引行」）
- [ ] **AC11** 护栏 — **新面**：`check_products`（4 项）· `verify_examples_rtl_sim`（含 `--self-test`）· `_coverage`（含 `--self-test`）· `verify_example_only_headers` · `verify_downstream_consumer` · `verify_port_naming` · `verify_component_naming` · `verify_no_duplicate_rv32i_headers`
- [ ] **AC12** 护栏 — **回归面**：`verify_example_test_wiring` · `verify_test_counts` · `verify_flat_rtl_emit` · `verify_hierarchical_rtl` · `verify_example_assertion_quality` · `verify_guard_reachability` · `verify_docs_links` · `verify_roadmap_fragments` · `verify_agents_size` · `verify_improvement_index` · `./run_all_ported_tests.sh`（**28 → 29** 全 PASS）
- [ ] **AC13** 平台声明：`hier_eq` / `check_products` 的 verilator 依赖 ⇒ AC4/AC11 的 CI 证据**仅 Ubuntu**（ADR-035 裁决），文档 MUST 声明

### 后续批的判据方向（只登记 · 不在 S1 验收）

| 批 | 主判据 |
|---|---|
| S2 | `ready ≡ 1` 时**与 S1 产物逐拍等价**（在 `examples/riscv-mini` 的 34 端口产品上判 —— S1 的 SoC 产物只有 1 个观测量，不足以判等价）· `ready=0` wrapper 下的 **stall 拍数手推值** |
| S3 | AXI4 burst 事务的**手推期望**（len/size/burst）· cache hit/miss 拍数契约（`rv32i-memory-timing` delta）· TCM 旁路后的 `prog_*` 一致性（装载写进"核实际读的存储体"）· cache "miss 期间 addr 不变"契约与反压合流 |

---

## Capabilities

| capability | 动作 | 说明 |
|---|---|---|
| `examples-rtl-sim-coverage` | **无 delta**（只新增 1 行**数据**） | 清单行是**数据**、不是 Requirement 文本 ⇒ ⇒ 归档用 `--skip-specs`。⚠️ **先例 6/6**：`stream-{mux,demux,arbiter,fork}-rtl-product` · `phase3a-rtl-product` · `spinalhdl-ported-verif-migration` **均无 `specs/`** ⇒ ⇒ 与 metis P2-20 的建议**相反**，见下方处置表 |
| `example-component-boundary` | 无 delta | 新头生在 `examples/riscv-soc/src/` 内 |
| `rtl-soc-output` · `rtl-hierarchical-output` | 无 delta | 不改变契约，只新增符合契约的产品单元 |
| `rv32i-memory-timing` | S1/S2 无 delta · **S3 再评估** | S3 的 burst / hit-miss 拍数才触发 |

---

## Impact

| 面 | 影响 |
|---|---|
| **新增** | `examples/riscv-soc/`（`rv32i_soc_top.h` · `soc_trace_counter.h` · `tb_riscv_soc.cpp` · `tb_riscv_soc.sv` · CMake · AGENTS.md） |
| **登记/守恒（8 处）** | ① CMake 跟踪（28→29）② `exclusions.tsv`（`29 == 17 ∪ 12`）③ tsv 新行 + 锚点 ④ `_coverage --self-test` 的 poison（若受影响）⑤ `EXAMPLES_TEST_SOURCES`（**不变**）⑥ `CTEST_TOTAL`（**不变**）⑦ `disabled_examples.txt`（新 TB 有 `main` ⇒ 必须被跟踪）⑧ `verify_guard_reachability`（若新增护栏） |
| **文档** | root `AGENTS.md`（Structure + main() 32→33）· `examples/AGENTS.md` · `docs/developer_guide/rv32i-rtl-delivery-and-sim.md` · `docs/usage_guide/02-component-design.md` · `docs/CHANGELOG.md` |
| **观测面** | 🔴 SoC 级**看不到** `dbg_*`/`obs_*`（ADR-072 D1）⇒ 核级 24 条只能留 `examples/riscv-mini` ⇒ SoC 级有**两个**根自有观测量（`store_count` / `writeback_count`）⇒ 满足签批 spec 的 ≥2 下限 |
| **零风险面** | S1 **不动** `include/**` 与 `examples/riscv-mini/**`（AC5 用 base SHA 钉死） |

---

## 风险与回滚

| # | 风险 | 处置 |
|---|---|---|
| **R1** | "零行为变化"只靠声明 | AC5（base SHA + `git status` + 24 条 `RESULT` 逐字）+ AC4（逐拍等价） |
| **R2** | **S1 变化观测量 < 2 ⇒ `hier_eq` 机械拒绝 / 违反签批 spec** | `SocTraceCounter` 的**两个**计数随激励变化（`store_count` 0→1 · `writeback_count` 0→3）⇒ 观测集非空、非恒定、且**≥2**（事实⑧ · ADR-077 D1） |
| **R3** | 命名漂移（`Rv32iPipelineTop` / `tb_rv32i_soc` 名不副实） | **S1 不改名**；改名单列一批（tsv `dut` 列 + 各 include + 文档） |
| **R4** | S2 的"功能等价"不成立 | S2 AC = 「`ready≡1` 逐拍等价」+ `ready=0` wrapper 的 stall 手推值；**不**用 S1 的 SoC 产物判等价（只有 1 个观测量） |
| **R5** | S3 撞反压不足 | S2 是 S3 **硬前置**；S2 未验收 ⇒ S3 不开工 |
| **R6** | 新增 example "新增即漏" | AC9/AC11/AC12 把 8 处登记全部列成机械判据 |
| **R7** | 装载期复位极性接反 ⇒ "装载成功但永不执行"（`prog_*` 与 `ext_rst` 无关） | AC7 的 `store_count == 1` ∧ `writeback_count == 3` 是**唯一**能发现它的判据（`top_core_*` 不可见）⇒ 两个断言 MUST 手推（装载期 `ext_rst=1`、释放后再跑） |

**回滚**：S1 全是**新增文件 + 登记行** ⇒ 删目录 + 撤登记（核零改动）。

---

## 决策门（🔴 S1 开工前 MUST 由 **architect** 裁决 · planner 不代裁）

| # | 决策 | 不能由 planner 定的理由 |
|---|---|---|
| **G-1** | **IP 级 top vs SoC 级 top 的分工**（ADR-046 dated addendum / 新 ADR） | ADR-046 原文「只留 SoC 顶层」⇒ 本项产生**两个** example 侧顶层 ⇒ 解释既有决策 = architect |
| **G-2** | **模块类型名批准权**：`Rv32iCoreComplex` / `SocTraceCounter` | ✅ **已闭**（ADR-077 D2）= **当前产品名（非契约）** · 登记在产品登记面 ⇒ 改名不构成破坏性变更，但**结构判据 MUST 重算** |
| **G-3** | **`prog_*` 装载口归属** —— 建议**收窄**为：S1/S2 保留根端口（TB 后门）· S3 **新增**总线路径而**不删**根端口 | 若 S3 删根端口 ⇒ 产品端口面破坏性变更（ADR-072 D1） |
| **G-4** | **新行的 `kind`** —— 建议单行 `kind=soc`（`REGISTERED_SOC` 1→2） | 影响 AC9 的守恒等式 |

> 裁决顺序：G-1 → G-2 → G-3 → G-4。

---

## 待粘贴索引行（builder 在 S1 第一笔提交补进根 `improvement-suggestions.md`）

> ⚠️ planner 写集不含根 `improvement-suggestions.md`（`lib/role-policy.ts:80-85`）⇒ 本行由 **builder** 粘贴，
> 并跑 `scripts/verify_improvement_index.sh`（状态词 MUST 与 frontmatter `状态: draft` 逐词一致）。

```
| [rv32i-soc-platform-2026-11](.rddf/improvements/rv32i-soc-platform-2026-11.md) | P1 | 用户 2026-11-22「核做成真实可替换单元 + L1/L2/MMU/内存控制器/总线」· S0–S3（S4/S5 只留索引） | 2026-11-22 | 📝 **draft**（立项 · ✅ S0 决策门已闭 = ADR-077 `Accepted` 2026-11-23 ⇒ S1 可开工） |
```

---

## metis 第 1 轮 gap 分析与处置（2026-11-22）

> metis 输出 **4 P0 + 7 P1 + 5 P2**。以下逐条处置（**全部已采纳**）。

| # | finding | 处置 |
|---|---|---|
| **P0-1** | AC2 的「3 module」**错**：子层不折叠 ⇒ 实为 **10** | ✅ **已改**（AC2 手推集 10 项 · 并写明"不是 3"） |
| **P0-2** | 无变化观测 ⇒ `hier_eq` **机械拒绝**（非"很弱"） | ✅ **已改**：`SocTraceCounter` 的**两个**计数 0→1 / 0→3（AC4/AC7） |
| **P0-3** | AC3 的"零后代冒泡 grep"**双重失效**（前缀是 `top_core_*` · 冒泡已退役） | ✅ **已改**：删 grep ⇒ 改「根端口**集合相等**」+ `check_products` CHECK 2 |
| **P0-4** | `AxiLiteGpio` 全 tie-off = **死组件** + 兄弟边仍 0 ⇒ 换 **`SocTraceCounter`** | ✅ **已改**（S1-③；D4③ 当场达标 ⇒ AC8 由"递延声明"改为"结构化证据"） |
| **P0-5**（并入） | AC6 只写"≥ 最小值" ⇒ 改**双向等式** + `scenarios`/`spec_checks` + 变异自证 | ✅ **已改**（AC7） |
| **P0-6** | AC7（登记）在 G-4 前**不可判** | ✅ **已改**（AC9 写死建议口径 + G-4 裁决） |
| **P0-7** | 缺**核 module 端口契约** AC | ✅ **已加**（AC6） |
| **P0-8** | `include/cpu/**` 一字不动 与 S2/S3 **自相矛盾** | ✅ **已改**（写面按批分级） |
| **P1-9** | S2 缺 `ready=0` DUT（TCM ready 恒 1） | ✅ **已改**（S2 行 + 事实⑨） |
| **P1-10** | S3 的两个隐式前置（`prog_*` 一致性 · cache addr 稳定契约） | ✅ **已补**（S3 判据方向） |
| **P1-11** | G-3「S3 起走总线」会让 S1 无法验收 | ✅ **已改**（建议收窄：新增路径、不删根端口） |
| **P1-12** | S4/S5 过度规划且与 OUT 矛盾 | ✅ **已删**（只留索引行） |
| **P1-13** | 期望源双载体（`examples-rtl-expectations.md` vs `soc-rtl-expectations.md`） | ✅ **已改**（统一到 `soc-rtl-expectations.md#riscv-soc`） |
| **P1-14** | AC8 全是回归、无一条判 S1 新面 | ✅ **已拆**（AC11 新面 / AC12 回归 · 补 5 个缺失护栏） |
| **P1-15** | AC5「`git diff` 为空」会被并发会话污染 | ✅ **已改**（钉 base SHA + `git status --porcelain`） |
| **P2-16** | 索引行写集缺口 | ✅ **已改**（AC10 = builder 义务 + 待粘贴行原文） |
| **P2-17** | 术语未定义 | ✅ **已加**（§术语表） |
| **P2-18/19** | `Rv32iCoreTop` 引用 · 问号注释 | ✅ **已在 proposal 删除** |
| **P2-20** | `--skip-specs` 悬置 | ⚠️ **不采纳 metis 的建议**（它判「MODIFIED ⇒ 正常 archive」）—— **实测先例 6/6 都是 no `specs/`**（`stream-{mux,demux,arbiter,fork}` · `phase3a` · `spinalhdl-ported-verif-migration`）；`soc-rtl-product-and-tb` 之所以需要 delta 是因为它**新增了一条 Requirement**（SoC 形态 = 「module 数 == 成层 IP 数 + 1」），本项只是**加一行数据** ⇒ **无 delta · archive 用 `--skip-specs`** |
