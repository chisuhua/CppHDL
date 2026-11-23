# Proposal: RV32I 可组合 SoC 平台 —— **S1 结构骨架**（`riscv-soc-skeleton-2026-11`）

- **状态**: 立项（2026-11-22 · planner）· ✅ **开工前门已闭**：**ADR-077 `Accepted`**（2026-11-23 · G-1…G-4 = D1…D4）⇒ ⇒ 本 proposal 的权威数字以 **ADR-077 的 V1 / V2 / V4** 为准
- **用户诉求**（2026-11-22）: 「想把 RV32I 核做成**真实可替换单元** —— 后续加 L1/L2 缓冲、MMU、内存控制器、总线」+「riscv-mini 可看成 riscv-soc 的 spike」+「riscv-mini 改造成核复合体（pipeline + TCM），另建 `examples/riscv-soc` 实施 `Rv32iSocTop`」
- **上游 improvement**: `.rddf/improvements/rv32i-soc-platform-2026-11.md`（本 change = 其 **S1** 批 · metis 第 1 轮已处置）
- **参考实现**: `examples/axi4/`（`axi_soc_example_top.h` + `tb_axi_soc_example.cpp` 的 `kExpectedModules` / `kExpectedTopPorts` / `kMinVaryingObservations` 与 `check_products` 四件套）
- **架构依据**: **ADR-077**（两级顶层 + SoC 组合的四条形态裁决 · **Accepted** 2026-11-23 · **本 change 的权威数字来源**）· ADR-046（微架构唯一权威 · §D1 已由 ADR-077 追记细分）· ADR-061（成层显式声明）· ADR-070（D2 七个成层点 · **模块类型名 = 明文非契约**（用户 2026-11-15 裁决 · 取代 D3 的「产物级契约名」表述）· D4 条件③）· ADR-072 D1（产品端口面）· ADR-073（SoC 产物形态 + SoC 级 TB）· ADR-074（验证单一真相源）
- **实测基线**（2026-11-22）: riscv-mini 产品 = **34 端口**（= 框架 2 + 核自有 32 = 4 in + 28 out）/ **8 module** · 核级判据 **24 条**（`RESULT: checks=24 fails=0`）· `Rv32iPipelineTop` = pipeline + ITCM + DTCM · **子层不折叠**（`Rv32iPipeline` 成层 ⇒ 9 module）

---

## Why

1. **"核"今天不可替换**：`Rv32iPipelineTop` 同时持有 `Rv32iPipeline` 与 `InstrTCM`/`DataTCM` ⇒ 想换缓存化的核，必须连存储器、TB、登记一起换。
2. **后续每个诉求都卡在同一处**：L1/L2 需要**总线口**、MMU 需要特权级、内存控制器需要总线 —— 而今天**不存在"核的对外接口"**（对外只有内部 `dbg_*`）。
3. **验证面可以零重写**：现有 24 条判据全部观测**核级信号**（`dbg_id_*` / `dbg_wb_*` / `obs_data_*` / `obs_perf_instructions` / `prog_*`）⇒ 核复合体留在 `examples/riscv-mini` ⇒ **24 条判据与文件零改动**（AC5）。
4. **框架已具备能力**：`ch::ch_layer<T>` · ADR-073 的 SoC 纪律 · `check_products` 四件套 · 四件套登记 ⇒ 缺的只是**装配与登记**。

⇒ **S1 = 纯结构骨架**：把"核复合体"与"SoC 根"变成两个产品单元，**核零改动**，为 S2（MEM 反压）/ S3（总线主口 + L1）立住边界。

---

## What Changes

### ① `examples/riscv-soc/src/rv32i_soc_top.h`（**新增** · 设计定义 · 无 `main`）

`Rv32iSocTop`（根）= 框架 clock/reset + 根自有 **6** 根 + **两个成层 IP**：

```cpp
// 核复合体（可替换单元）：复用既有 Rv32iPipelineTop，一字不改
ch::ch_layer<Rv32iPipelineTop> core{"core", "Rv32iCoreComplex"};
// 观测 IP：吃核的两个事件 ⇒ 真实 child→child 边（ADR-070 D4③）；两个计数上抬为根 only 输出
ch::ch_layer<SocTraceCounter> trace{"trace", "SocTraceCounter"};

core.io().ext_rst   <<= io().ext_rst;        // 核自有复位（高有效）
core.io().prog_we   <<= io().prog_we;        // 固件装载后门（G-3：S1/S2 保留）
core.io().prog_addr <<= io().prog_addr;
core.io().prog_data <<= io().prog_data;
trace.io().store_event  <<= core.io().obs_data_write_en;  // ← 兄弟边（child → child）
trace.io().retire_event <<= core.io().dbg_wb_write_en;    // ← 兄弟边（child → child）
io().store_count        <<= trace.io().store_count;       // ← 根 only 输出（ADR-066 ①）
io().writeback_count    <<= trace.io().writeback_count;   // ← 根 only 输出
```

- **根 `__io` 手推 6 根**：`ext_rst` · `prog_we` · `prog_addr[31:0]` · `prog_data[31:0]` · `store_count[15:0]` · `writeback_count[15:0]` ⇒ ⇒ 产品端口 = 框架 2 + **6** = **8**
- **零未驱动后代输入**（ADR-073 D2）：核的 4 个输入全由根驱动；观测 IP 的 **2** 个输入都由核驱动

### ② `examples/riscv-soc/src/soc_trace_counter.h`（**新增**）

```cpp
__io(
    ch_in<ch_bool>        store_event;     // ← core.io().obs_data_write_en（store 进入 EX/MEM 的那拍）
    ch_in<ch_bool>        retire_event;    // ← core.io().dbg_wb_write_en（写回使能 · 已带 mem_valid 限定 ⇒ 单拍脉冲）
    ch_out<ch_uint<16>>   store_count;     // ← 根 only 输出
    ch_out<ch_uint<16>>   writeback_count; // ← 根 only 输出
)
```

两个**饱和计数**（各自输入为真则 +1）· 复位清零。**为什么是它**：
① 制造**真实兄弟边**（ADR-070 D4③ 当场达标）—— **两条**（store 事件 + 写回事件）
② 给根**两个随激励变化**的输出 ⇒ 满足签批 spec `rtl-soc-output` 的「**≥2 个**随激励变化的量」（ADR-077 D1）⇒ ⇒ `min_varying_observations == 2`
③ 让"核真的在跑"在 SoC 层**可观测**（否则 `dbg_*` 都不是顶层端口）
④ 不是死组件（`AxiLiteGpio` 全 tie-off 的方案已否 —— 恒 0 且兄弟边仍为 0）
⑤ ⚠️ **两个计数的值必须不同**（手推 **1** vs **3**）⇒ ⇒ 否则两个事件源接反时观测值不变 ⇒ 恒绿 ⇒ ⇒ 判别力要求写入 AC4

### ③ `examples/riscv-soc/tb_riscv_soc.cpp`（**新增** · 激励驱动 · 双模式）

- **默认**（无参）= 验证模式：解释器路径跑 **7 字小程序（与核级同源** ⇒ 手推台账**零新造** · 见 `.rddf/checks/examples-rtl-expectations.md#riscv-mini-rv32i-soc` 的「小程序（7 个字 · 编码手推）」）：
  `addi x1,x0,42` · `nop` · `addi x2,x0,11` · `nop` · `add x3,x1,x2` · `sw x3,0(x0)` · `jal x0,0`
  ⇒ ⇒ 断言 `store_count == 1`（1 条 `sw`）**且** `writeback_count == 3`（x1 / x2 / x3 三次写回）⇒ `RESULT: checks=<L> fails=0`
  ⚠️ **为什么不用 4 字短程序**（`addi x1,x0,42` · `nop` · `sw x1,0(x0)` · `jal x0,0`）—— **两条**理由：
  ① **判别力**：它只产生 **1 次**写回（`addi`）+ **1 次** store ⇒ 两个计数器**同为 1**
     ⇒ ⇒ 事件源接反 / 接成同源时观测值不变 ⇒ **恒绿**
  ② **手推台账零新造**：7 字是核级 24 条判据**已锁定**的形态（写回 x1=42 / x2=11 / x3=53 ·
     store data=53）；4 字版的 store 数据依赖「隔 1 个 `nop` 后 `rs2` 取自哪一档前推」的
     时序推导 ⇒ 无既有手推背书
- **`--emit-rtl [--hierarchical] --out <dir>`** = 发射模式（库层 `strip_mode_flags` / `require_out_dir` / `run_emit_cli`）+ **`check_products` 四件套**
- **fail-closed**：默认模式收到未知参数 ⇒ exit 2；发射模式缺 `--out` ⇒ exit 2

### ④ `examples/riscv-soc/tb_riscv_soc.sv`（**新增** · SoC 级独立 Verilog TB）

- **只读根端口**（**8** 根 = 框架 2 + 根自有 6；`dbg_*`/`obs_*` 已不在顶层 —— ADR-072 D1）
- 判据（手推）：复位默认值 · 装载 **7 字**后 `store_count == 1` **且** `writeback_count == 3` · `jal` 自环后计数保持（`jal x0,0` 的 rd = x0 ⇒ `rd_not_zero = 0` ⇒ 不写回 ⇒ 计数停在 3）
- `EXPECT-SRC: .rddf/checks/soc-rtl-expectations.md#riscv-soc`（**同型先例 = `#soc-example`**）· `generated=no`（手写 · ADR-074 D7 逃生口）

### ⑤ 构建与跟踪

`examples/riscv-soc/CMakeLists.txt`（新）· `examples/CMakeLists.txt` 的 `CH_TRACKED_EXAMPLE_TARGETS` **28 → 29**（新增 `tb_riscv_soc`）· **不** `add_test`（同 `tb_rv32i_soc` ⇒ `CTEST_TOTAL` 不变）

### ⑥ 覆盖登记

| 项 | 前 | 后 |
|---|---|---|
| `examples-rtl-sim-targets.tsv` | 无 riscv-soc 行 | 1 行 `kind=soc` · `interp_checks == min_checks` · `scenarios` / `spec_checks` 必填 |
| 锚点 | `REGISTERED_SOC: 1` | **2**（`REGISTERED_HIER` 不动 · G-4 建议） |
| `examples-rtl-sim-exclusions.tsv` | `28 == 16 ∪ 12` | **`29 == 17 ∪ 12`** |
| `.rddf/checks/soc-rtl-expectations.md` | 只有 `#soc-example` | 新增 `## riscv_soc {#riscv-soc}`（手推 · `VERIF-CHECKS`） |
| 根 `AGENTS.md` | examples main() = **32** | **33** |

### ⑦ 文档

`examples/riscv-soc/AGENTS.md`（新 · 含**核复合体 34 端口契约表**与"可替换单元"说明）· root `AGENTS.md`（Structure 树）· `docs/developer_guide/rv32i-rtl-delivery-and-sim.md`（核级 vs SoC 级两产品单元）· `docs/usage_guide/02-component-design.md`（IP 级 top 与 SoC 级 top 的写法）· `docs/CHANGELOG.md`

### ⑧ 零改动面（`git diff <base-sha>` 证明）

`examples/riscv-mini/**` · `include/**` ⇒ **一字不动**

---

## Capabilities

| capability | 动作 | 说明 |
|---|---|---|
| `examples-rtl-sim-coverage` | **无 delta**（只新增 1 行**数据**） | 清单行是数据、不是 Requirement 文本 ⇒ ⇒ **归档用 `--skip-specs`**（先例 6/6：`stream-{mux,demux,arbiter,fork}-rtl-product` · `phase3a-rtl-product` · `spinalhdl-ported-verif-migration` 均无 `specs/`）|
| `example-component-boundary` | 无 delta | 新头生在 `examples/riscv-soc/src/` 内 |
| `rtl-soc-output` · `rtl-hierarchical-output` | 无 delta | 本 S1 的产物**满足**既有 `rtl-soc-output` 的「module 数 == 成层 IP 数 + 1」⇒ ⇒ **按全树读法**：成层实例 = 核复合体自身 + 其内部 7 个 stage/hazard/mux + `SocTraceCounter` = **9** ⇒ 9 + 根 = **10** ⇒ ⇒ ⚠️ **措辞歧义登记**：spec 的「成层 IP 实例数」字面读作「顶层直接实例化」= 2 ⇒ 与产物 10 不符 ⇒ ⇒ **歧义只在「嵌套成层」时出现**（axi4 先例全是 leaf ⇒ 读法不可判别）⇒ ⇒ **机械判据无歧义**（`check_products` CHECK 1 比对**产物 module 集** · `src/rtl_emit/product_check.cpp:137`）⇒ ⇒ **不改 spec**；登记为「第三次装配时的措辞修订点」（同 ADR-077 的「8 module 加作用域限定」） |

> ⚠️ **`openspec validate` 的 "No deltas found" 在 S1 上是预期**（本 change 无 spec 变更）：
> 它**不在 CI 门内**（`docs/developer_guide/openspec-specs-guide.md`），实际门 = `scripts/verify_openspec_delta.sh`（本 change 已 4/4 绿）；
> 归档用 `openspec archive <name> --yes --skip-specs`（同 6 个先例）。

---

## Impact

| 面 | 影响 |
|---|---|
| **新增** | `examples/riscv-soc/`（2 个头 + 2 个 TB + CMake + AGENTS.md） |
| **登记 8 处** | CMake 跟踪（28→29）· exclusions 守恒 · tsv 新行 + 锚点 · `_coverage --self-test` poison（若受影响）· `EXAMPLES_TEST_SOURCES`（**不变**）· `CTEST_TOTAL`（**不变**）· `disabled_examples.txt`（新 TB 有 `main` ⇒ 进跟踪）· `verify_guard_reachability`（若新增护栏） |
| **观测面** | 🔴 SoC 级**看不到** `dbg_*`/`obs_*` ⇒ 核级 24 条只能留 `examples/riscv-mini`；SoC 级观测量 = **两个**（`store_count` / `writeback_count` · 同属根自有 IP `SocTraceCounter`）⇒ ⇒ 满足签批 spec 的 **≥2** 下限 |
| **零风险面** | S1 不碰 `include/**` 与 `examples/riscv-mini/**`（AC5 用 base SHA 钉死） |

---

> 📌 **2026-11-23 · dated 注记（R3a 改名批）**：本 change 的 AC2 / AC6 / design 的
> `Rv32iCoreComplex` 期望集**已被** `openspec/changes/riscv-core-rename-2026-11/`（**R3a**）
> 更新为 **`Rv32iCoreTop`**（模块类型名 = **非契约** · ADR-077 D2）⇒ ⇒
> **重新验收本 change 时**：module 集合判据按「**除名字外**相等」理解
> ⇒ ⇒ 端口表 == **34** / 集合基数 == **10** 两项**意图不变**。
> ⚠️ 本 change 的**历史正文不改写**（存量不追溯）⇒ ⇒ 上述注记是唯一的对接点。

## Out of Scope

- ❌ **MEM 级反压**（`data_ready` 真消费）⇒ **S2** `rv32i-mem-backpressure-2026-11`
- ❌ **核的总线主口 / L1 cache** ⇒ **S3** `rv32i-bus-master-and-l1-2026-11`
- ❌ **L2 / 内存控制器** ⇒ **S4**（后续立项索引）
- ❌ **特权级 / MMU** ⇒ **S5**（硬前置 `rv32i-privileged-csr-2026-11`）
- ❌ **改名**（`Rv32iPipelineTop` 仍用原名；`tb_rv32i_soc` 仍用原名）⇒ 单列一批（improvement R3）
- ❌ **让观测 IP 通过总线被核访问** ⇒ S3（S1 的连线是**直接信号**，不走总线）

---

## 用户可见变更

1. 新命令：`build/examples/riscv-soc/tb_riscv_soc`（默认 = 解释器验证 · `--emit-rtl --hierarchical --out <dir>` = SoC 级层次化产物）
2. 新产品单元：**SoC 级**层次化产物 —— **10 module**（`top` + `Rv32iCoreComplex` + `SocTraceCounter` + 7 个 stage/hazard/mux）· 顶层端口 **8**（框架 2 + 根自有 6）
3. 新目录 `examples/riscv-soc/`（可整体删除回滚，核不动）

---

## 验收标准

> 期望值 MUST **手推**（禁抄仿真输出）；计数 MUST **从产物/清单派生**（禁裸数字）。

- [ ] **AC1** `examples/riscv-soc/` 文件集齐（①–④ + CMake + AGENTS.md）· `cmake --build` 0 error / 0 warning
- [ ] **AC2** 🔴 **产物 module 集 == 手推集**（`check_products` CHECK 1 · **集合相等**）：`{top, Rv32iCoreComplex, SocTraceCounter, IfStage, IdStage, ExStage, MemStage, WbStage, HazardUnit, ForwardingMux_32_}` = **10**（⚠️ **不是 3** —— 子层不折叠）· 期望集**写死在 TB 里**（禁从成层声明派生 ⇒ 否则"漏一个声明"同时改期望 ⇒ 恒绿）
- [ ] **AC3** 🔴 **根端口表 == 手推 8 根**（框架 2 + 根自有 **6**：`ext_rst` / `prog_we` / `prog_addr` / `prog_data` / `store_count` / `writeback_count`）· **集合相等** · 且 `check_products` CHECK 2（端口数 == **8**（`manifest.port_table.size()` · 含框架 2）**且** 后代未驱动端口 == 0）
- [ ] **AC4** 🔴 `check_products` CHECK 4：**双形态逐拍等价 + 篡改自证**（观测 = `store_count` + `writeback_count` · 手推值 **1** 与 **3**（**互不相同**）· `min_varying_observations == **2**`（签批 spec `rtl-soc-output` 的硬要求，不是可调参数）· 激励 **≥10 拍且值不同** · 粘滞信号不计入）⇒ ⇒ **三条反向判据**：① 把 `writeback_count` 移出观测集 ⇒ 变化量 1 < 2 ⇒ 必红 ② **两个事件源接反** ⇒ (1,3) → (3,1) ⇒ 必红 ③ 两事件源接成同一个 ⇒ (1,1) ⇒ 必红
- [ ] **AC5** 🔴 **回归锚点**：`git diff --stat <S1-base-sha> -- examples/riscv-mini/ include/` 为空 **且** `git status --porcelain examples/riscv-mini/ include/` 为空 **且** `examples/riscv-mini/tb_rv32i_soc` 仍 `RESULT: checks=24 fails=0`（base SHA 由实施首笔提交钉死）
- [ ] **AC6** 🔴 **核复合体端口契约**：`Rv32iCoreComplex` module 的端口表 == 手推集（**34** = 框架 2 + 核自有 32 = 4 in + 28 out）· 表写进 `examples/riscv-soc/AGENTS.md`（= "可替换单元"的唯一物证 + S3 的变更基线）⇒ ⇒ ⚠️ **判据载体 MUST 新建**：`manifest.port_table[]` 只覆盖顶层 ⇒ 子 module 端口表需**独立提取** + 带**判别力自证**（删一个端口 ⇒ 判据必红）
- [ ] **AC7** 🔴 **双向等式**：解释器路径 `RESULT: checks=L fails=0` · `.sv` 侧 `checks=L fails=0` · 且 **`interp_checks == min_checks == L`**（同一手推字面量）· **两条计数断言两侧同值**（`store_count == 1` ∧ `writeback_count == 3`）· tsv 的 `scenarios` / `spec_checks` 必填 · **变异自证**：任一期望改 1 位 ⇒ 该条必红
- [ ] **AC8** 🔴 **兄弟边被走通**（ADR-070 D4③ 达标）：发射结果里该边出现在**兄弟边分支**（结构化证据 · 非散文）
- [ ] **AC9** 登记守恒：CMake 跟踪 **28 → 29** · `29 == 17 ∪ 12` · tsv `kind=soc`（`REGISTERED_SOC` 1→2）· `EXAMPLES_TEST_SOURCES` 不变 · `CTEST_TOTAL` 不变 · root `AGENTS.md` main() **32 → 33**
- [ ] **AC10** 索引行（**builder 义务** · planner 写集不含根索引）：补 `improvement-suggestions.md` 行 · 状态词 `**draft**` · `verify_improvement_index.sh` 绿
- [ ] **AC11** 护栏 — **新面**：`check_products`（4 项）· `verify_examples_rtl_sim` + `--self-test` · `_coverage` + `--self-test` · `verify_example_only_headers` · `verify_downstream_consumer` · `verify_port_naming` · `verify_component_naming` · `verify_no_duplicate_rv32i_headers`
- [ ] **AC12** 护栏 — **回归面**：`verify_example_test_wiring` · `verify_test_counts` · `verify_flat_rtl_emit` · `verify_hierarchical_rtl` · `verify_example_assertion_quality` · `verify_guard_reachability` · `verify_docs_links` · `verify_roadmap_fragments` · `verify_agents_size` · `verify_improvement_index` · `./run_all_ported_tests.sh`（28 → 29 全 PASS）
- [ ] **AC13** 平台声明：`check_products` 的 verilator 依赖 ⇒ AC4/AC11 的 CI 证据**仅 Ubuntu**（ADR-035）⇒ 文档 MUST 声明

---

## 决策门（🔴 开工前 · architect）

| # | 待裁 | 建议 | ✅ **裁决（ADR-077 · `Accepted` 2026-11-23）** |
|---|---|---|---|
| **G-1** | **IP 级 top vs SoC 级 top 的分工**（ADR-046 dated addendum / 新 ADR） | 明确"两类 top"的住址与命名规则 | **D1** = 两级顶层：核复合体（IP 级 · `riscv-mini/src/rv32i_pipeline_top.h`）+ SoC 根（`examples/riscv-soc/src/rv32i_soc_top.h`）· 判据**分域**（「8 module」只约束核复合体 · SoC 根 = 10）· ADR-046 §D1 已追记细分 |
| **G-2** | **模块类型名批准权**（`Rv32iCoreComplex` / `SocTraceCounter`） | 由 architect 批准这两个名字 | **D2** = 两个名字 = **当前产品名（非契约）**（与用户 2026-11-15 裁决一致）· 登记载体 = `soc-rtl-expectations.md#riscv-soc` + TB 冻结期望集 · **改名义务** = 结构判据重算 + 重新 lint |
| **G-3** | **`prog_*` 装载口归属** | **收窄**：S1/S2 保留根端口；S3 **新增**总线路径而**不删**根端口（避免产品端口面破坏性变更） | **D3** = `prog_*` **长期保留**为 SoC 根端口；S3 只**新增**总线路径（真实芯片形态的装载路径留给独立 ADR） |
| **G-4** | **新行 `kind`** | 单行 `kind=soc`（`REGISTERED_SOC` 1→2） | **D4** = 单行 `kind=soc`（`kind` 编码**产品类型**；`hier` 桶语义 = 非默认的另一种发射形态 ⇒ SoC 双行零信息增量） |
