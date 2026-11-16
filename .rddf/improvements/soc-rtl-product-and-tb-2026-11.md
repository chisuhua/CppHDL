---
name: soc-rtl-product-and-tb-2026-11
priority: P1
phase: phase-6
category: framework
type: feature
状态: draft
进度: **立项（2026-11-16）** · 未开工 · 🚪 开工前门 = **ADR-073 已 Accepted**（架构依据）· ⚠️ **顺序硬约束**：本项的 **T0（后代输入驱动源）MUST 先于 ADR-072（D1）的合并** · ⚠️ **metis 第 1 轮 19 条 finding 全处置**（见末节）
主题: 真层次化 RTL 输出（成层粒度由用户显式声明 · 默认 inline · 与扁平逐字节一致）—— SoC 层产物形态 + SoC 层 TB 契约
依赖: rtl-emit-product-tooling-2026-11（implemented）· examples-rtl-sim-coverage-2026-11（implemented）· rv32i-hierarchical-example-2026-11（implemented）· **ADR-073**（Accepted 2026-11-16）· **ADR-072**（Accepted · v2.1 · 本项的 D2 与它耦合）
来源: 用户 2026-11-16「SOC 的 Verilator 要怎么走？独立 TB 是继续 IP 层次的吗？SoC 如何构建 IP 互联和 SOC 层的 TB？」
生成时间: 2026-11-16
evidence: probe:scripts/verify_examples_rtl_sim.sh
---

# Improvement: SoC 层的 RTL 产物形态 + SoC 层 TB 的交付契约

**主题**: 真层次化 RTL 输出（成层粒度由用户显式声明 · 默认 inline · 与扁平逐字节一致）—— SoC 层产物形态 + SoC 层 TB 契约

---

## 架构依据

**唯一权威** = `docs/adr/ADR-073-soc-rtl-product-and-tb.md`（**Accepted** · oracle ×1 + critic ×2 复审 APPROVE · 审稿记录 `.rddf/reviews/ADR-073-soc-rtl-product-and-tb.md`）。
**方案细则** = `docs/architecture/2026-11-16-soc-rtl-product-and-tb.md`（现状快照 + 模块关系 + **可枚举改动清单**）。

### A.1 现状事实（2026-11-16 **实测** · ⚠️ 含 metis 复算订正）

| # | 事实 | 证据 |
|---|---|---|
| ① | **IP 互联机制已存在且有完整实例** —— `ch_module<T>` 实例化 + 逐信号 `<<=` 连线 | `examples/axi4/axi_soc_example.cpp:192-201`（4 个 IP）+ `:204-211` |
| ② | **SoC 顶层端口面 = 12 个扁平标量**（8 in + 4 out，零 bundle） | `examples/axi4/axi_soc_example.cpp:161-181` ⇒ ⇒ 框架 2 + 12 = **14** |
| ③ | **SoC 的 RTL 产物从未被交付** —— `examples/axi4/emit_rtl.cpp` 的 factory 造的是 `AxiLiteTop` | `examples/axi4/emit_rtl.cpp:36-40` |
| ④ | **SoC 的 IP 边界在产物里不可见** —— 4 个 IP 都用 `ch_module`（默认 inline）⇒ ⇒ 层次化产物**全 inline 成 1 个 module** | `examples/axi4/axi_soc_example.cpp:192-201` · ADR-061 · ⚠️ 4 个 IP **均无** `CH_HIERARCHY_LAYER` ⇒ ⇒ 成层须走**档 ③**（`include/module.h:171-175`） |
| ⑤ | 🔴 **SoC 有 31 个未驱动的后代 `ch_in`** —— `matrix.m1_*` **11** · `uart` 的 AXI 从机输入 **11**（该文件 13 个 `ch_in` 里 `uart_rx`/`rst_n` 已接）· `matrix.s1_*` 的**响应侧** **8**（**整个文件零 `s1_*` 连线** —— gpio 占 `s0_*`）· 🔴 **`master.rst_n` 1 个**（`axi_soc_example.cpp:47` 声明；连线段 `:265-277` **只**驱动 `matrix`/`gpio`/`uart` 的 `rst_n` ⇒ ⇒ `master.io().rst_n` **零驱动**）⇒ ⇒ **11 + 11 + 8 + 1 = 31** | ⇒ ⇒ **ADR-072 D1-f（后代悬空 ⇒ fail-closed）落地后，SoC 产物发不出来** ⇒ ⇒ 且 uart 不在总线上 ⇒ ⇒ UART 场景不可达 |
| ⑥ | ✅ **仓内已有「未用端口 tie-off」的成例惯用法** —— `interconnect.io().m1_awvalid <<= ch_bool(false);` … | `examples/axi4/phase3a_complete.cpp:259-302` ⇒ ⇒ **不需发明新机制** |
| ⑦ | ✅ **双形态逐拍等价的公共 API 已存在** —— `ch::hier_eq::emit(root, work_dir, granularity)` + `compare(ctx, work_dir, hier_path, ports, inputs, observations, stimulus)`；`ports` **MUST 由 `emit` 派生** | `include/hierarchy_equivalence.h:131,155` · A 侧 = `VerilatorBackend` 从 ctx 自生成的扁平产物（`src/hierarchy_equivalence_impl.h:258-269`）⇒ ⇒ **端口面差异结构性不影响**判定（比对只经 `inputs`/`observations` 落根端口） |
| ⑧ | ⚠️ **`backend_port_set` 只对层次化产物成立** —— 唯一重载收 `HierarchyEmitResult`；扁平 `toVerilog` 返回 `void` | `include/hierarchy_verilog.h:154` · `include/codegen_verilog.h:65,68` |
| ⑨ | 🔴 **`run_emit_cli()` 只发扁平** —— `src/rtl_emit/cli.cpp:177-187` 调 `toVerilog(path, ctx, opts)`；`CliOptions` **无**层次化选项 | ⇒ ⇒ **SoC 的发射器 MUST 自己调 `toVerilogHierarchical`** ⇒ ⇒ **不能**照抄 `emit_rtl.cpp` 的 `run_emit_cli` 形态 |
| ⑩ | ⚠️ **`HierarchyEmitResult.port_provenance` 今天就有** —— `PortProvenance{module, port, is_input, width, origin}`，`origin ∈ {Self, ClockReset, Bubbled, InlineFolded}` | `include/hierarchy_verilog.h:54-59,105` ⇒ ⇒ **J-1 的载体不必等 ADR-072 的 `manifest.port_table[]`** |
| ⑪ | 🔴 **AXI SoC 的 TB 缺失**，且排除理由本身有事实漂移（「+ **Timer**」⇒ ⇒ 实为 4 个 IP、**无 Timer**） | `.rddf/checks/examples-rtl-sim-exclusions.md:36` · 漂移源头 = `examples/axi4/axi_soc_example.cpp:4` 的源码头注释 + `:24` 的**零使用** include + `examples/axi4/AGENTS.md` 的 OVERVIEW 表 |
| ⑫ | ⚠️ **`timer_irq` 是根自有死端口** —— `:180` 声明，全文件**零消费** | ⇒ ⇒ 仍**是**根自有 `ch_in` ⇒ ⇒ **仍是顶层端口**（TB 仍必须连它）⇒ ⇒ 但**无观测意义** ⇒ ⇒ 登记为残留 |
| ⑬ | ⚠️ **`done` 是粘滞完成标志，不是单拍脉冲** —— `done_reg->next = select(write_done, true, select(read_done, true, done_reg))` ⇒ ⇒ **无清零** ⇒ ⇒ 第一笔事务后恒为 1 | `examples/axi4/axi_soc_example.cpp:149-151` ⇒ ⇒ ⇒ TB **不得**等 `done` 上升沿；⇒ ⇒ `done` **不可**作「随激励变化」的观测证据 |
| ⑭ | ⚠️ **「未映射地址 DECERR」在 SoC 顶层不可观测** —— 顶层 12 个端口里**无 `bresp`**（master 收了 `bresp` 但从不使用）⇒ ⇒ DECERR 与 OKAY **不可区分**；且 `AxiLiteMatrix2x2` 默认映射**铺满 32 位地址空间** ⇒ ⇒ matrix 侧 DECERR **结构性不可触发** | `examples/axi4/axi_soc_example.cpp:161-181` · `include/axi4/interconnect_common.h:70-77` · `include/axi4/AGENTS.md`（「默认配置下 DECERR 不可触发」） |
| ⑮ | ✅ **`axi_soc_example` 是 tracked target 且有 `main()`** | `examples/axi4/axi_soc_example.cpp:283` · `examples/CMakeLists.txt:69` |
| ⑯ | ✅ **兄弟边形态存在但已被覆盖** —— master↔matrix、matrix→gpio ⇒ ⇒ 命中 ADR-070 D1 的 H-A/H-B（已在 `rv32i-hierarchical-example` 落地）；**「双类消费者」形态不存在** | `examples/axi4/axi_soc_example.cpp:204-260` · `docs/adr/ADR-070-...md:59-60` |
| ⑰ | ⚠️ **顶层 bundle 不是 Verilog 端口** ⇒ ⇒ AXI 总线**永远不在** SoC 顶层端口上 ⇒ ⇒ **SoC TB 不验证总线协议**（协议面归 IP 级 TB） | ADR-042 决策 5（**绝对**陈述） |
| ⑱ | ⚠️ **`ch_mem` 禁令已被收窄** —— ADR-066 v1.2 §2.7 决策 ⑦：只管「mem **本体**跨 module」；「仅实参视线跨层」是**已支持**的形态 | `docs/adr/ADR-066-...md:241,246-256` · capability `rtl-hierarchical-output/spec.md:589` |

### A.2 问题

**SoC 的价值恰在「IP 互联」** ⇒ ⇒ 但 ① IP 边界在产物中**不可见**（事实 ④）⇒ ⇒ 交付物与扁平单 IP 产物**无结构区别** ⇒ ⇒ 「SoC 层 RTL 生成」**名义存在、实质缺失**（事实 ③）；② **SoC 本身还没接完**（事实 ⑤：31 个）⇒ ⇒ **它今天连「ADR-072 D1 之后能不能发射」都不成立**。

---

## 范围

### IN

| # | 内容 |
|---|---|
| **D1** | SoC 产物 = **层次化**（4 个 IP 各成一层 ⇒ **根 module 名 `top`（默认）+ 4 = 5 个 module**） |
| **D2** | SoC 的 **31 个后代 `ch_in` 全部有驱动源**：`matrix.m1_*` **11** 根 tie-off + `matrix.s1_*` ↔ `uart` **19** 根单向接线 + `master.rst_n` **1** 根透传 |
| **D3** | SoC 层 TB = **独立 Verilog TB + 外部 verilator CLI** |
| **D4** | TB 按 ADR-072 D1 后的端口面写（**14**） |
| **D5** | 期望源 = `.rddf/checks/soc-rtl-expectations.md` |
| **D6** | 产品级判据 **J-1…J-5**（载体见下） |
| **D7** | 覆盖登记 = tsv 新 kind **`kind=soc`** + **锚点按 kind 分桶（三桶）** |
| **D10** | 能力面 = **新增 capability `rtl-soc-output`**（**只放 SoC 专属条款**；通用条款**引用** `rtl-hierarchical-output`，不重述） |

### OUT

| 项 | 理由 | 归属 |
|---|---|---|
| **「对外 AXI 主口」档（T-3）** | 阻塞只剩「无 AXI 从机 BFM」+「顶层 AXI 展平策略未定义」 | **独立立项** |
| **19 个 example 的 TB** | 期望值 MUST 手推 ⇒ 人工工作 | 独立排期（ADR-072 D5） |
| **特权级** | ADR-072 D3 | `rv32i-privileged-csr-2026-11`（已 draft） |
| **`uart.irq` / 根的 `timer_irq`** | 零消费者 ⇒ ⇒ **登记为残留**（仍在端口面内，但无观测意义） | 台账 + 残留登记表 |
| **「SoC TB 验证总线协议」** | 顶层**无** AXI 端口（事实 ⑰）⇒ ⇒ **结构性不可能** | **能力面显式声明** |

---

## Why

1. **SoC 产物从未交付**（事实 ③）⇒ ⇒ 「SoC 层 RTL 生成」今天**不存在可交付物**。
2. **IP 边界不可见**（事实 ④）⇒ ⇒ 交付物与 IP 级产物**无结构区别**。
3. **SoC 本身没接完**（事实 ⑤）⇒ ⇒ **31 个后代 `ch_in` 未驱动** ⇒ ⇒ ADR-072 D1-f 落地后**产物发不出来** ⇒ ⇒ **这是「能不能交付」的问题，不是风格问题**。
4. **UART 场景不可达**（事实 ⑤ 的 `s1 → uart` 零连线）⇒ ⇒ 「多 IP + 总线互连」这个 SoC 的核心价值**今天没被验证过**。
5. **为什么现在做**：① ADR-072 D1 一旦落地，未接完的 SoC 会被**卡死** ⇒ ⇒ **SoC 侧必须先做**；② 写点**零交集** ⇒ ⇒ 可并行 ⇒ ⇒ **不增加关键路径**。

---

## What Changes

> ⚠️ **ADR-073 的 10 条决策 ⇒ ⇒ 落到 13 个 task**（T0…T12）⇒ ⇒ **顺序硬约束**：**T0 MUST 先于 ADR-072 D1 的合并**。

| task | 内容 | 写点 |
|---|---|---|
| **T0** | **后代输入驱动源**（**可发射性前置**）：`matrix.m1_*` **11** 根 tie-off（仿 `phase3a_complete.cpp:259-263`）+ `matrix.s1_*` ↔ `uart` **19** 根**单向**接线（11 正 + 8 反）+ `master.rst_n` **1** 根透传（`<<= io().rst_n`，仿 `:263-267`）⇒ ⇒ **31 全覆盖** | `examples/axi4/axi_soc_example.cpp` |
| **T1** | **成层声明**：4 个 `ch_module` → `ch_layer<T>`（**档 ③**）⇒ ⇒ **模块类型名一次定死**：`"AxiLiteTestMaster"` / `"AxiLiteMatrix2x2<32,32>"` / `"AxiLiteGpio<8>"` / `"AxiLiteUart<16>"`（**带实参** ⇒ ⇒ 产物标识符 `AxiLiteTestMaster` / `AxiLiteMatrix2x2_32_32_` / `AxiLiteGpio_8_` / `AxiLiteUart_16_` ⇒ ⇒ 保住「同 sanitized base 不同类型 ⇒ 拒绝发射」这条护栏的判别力） | 同上 |
| **T2** | **发射入口**：新增 `examples/axi4/emit_rtl_soc.cpp` —— ⚠️ **不能**用 `run_emit_cli`（事实 ⑨：它只发扁平）⇒ ⇒ **自己解析** `--out` / `--emit-hygiene` ⇒ ⇒ 调 **`toVerilogHierarchical(out+"/top.v", *root, opts)`** + `build_rtl_manifest(*root, ctx)` ⇒ ⇒ **`result.ok == false` MUST 映射为非零退出**（否则护栏读到残留文件 ⇒ 假绿）+ CMake target `emit_rtl_axi_soc` | `examples/axi4/`（2 个文件） |
| **T3** | **产物级判据**：**载体 = `tests/hier_rtl_products.cpp` 的新子命令**（该文件**已是**非-ctest 产物生成器，`tests/CMakeLists.txt:1213` 只 `add_executable`）⇒ ⇒ **不复刻第二份 SoC 定义**（⚠️ `AxiSocTop` 定义在 `.cpp` 里，`tests/` 不能 include 它 —— 仓内既有做法是复刻 fixture，那会造**第二份 SoC** ⇒ ⇒ 禁止）。断言：J-1 端口面 + J-2 module 数 + 顶端口类型非打包 | `tests/hier_rtl_products.cpp` |
| **T4** | **期望源登记**：`.rddf/checks/soc-rtl-expectations.md`（端口映射 + 地址映射 + 事务序列 + **手推结果** + **冻结的模块名期望集**（4 个 sanitize 后名字 + 计数 5）） | `.rddf/checks/` |
| **T5** | **独立 TB**：`examples/axi4/tb_axi_soc_example.sv`（**14** 个顶层端口 + **4 个可观测场景**，见 AC6） | `examples/axi4/` |
| **T6** | **覆盖登记 + 护栏**：tsv +1 行 `kind=soc`（`target` = `axi_soc_example`）+ 锚点 `# REGISTERED_SOC: 1` + 护栏 ⓪ **三桶**判定（`kind ∈ {target,dut}` == `REGISTERED_TARGETS` · `kind=soc` == `REGISTERED_SOC` · **每个 data 行的 kind MUST 属于已知集合** ⇒ ⇒ 防「未知 kind 静默缩面」） | `.rddf/checks/` + `scripts/` |
| **T7** | **双形态一致性**（J-4）：`ch::hier_eq::emit` + `compare`（`ports` **由产物派生**）· 激励 **≥10 拍且值不同**（⚠️ 一笔事务 **10 拍**）· 观测集 {`rdata`, `gpio_out`, `uart_tx`}（⚠️ **不含 `done`** —— 它粘滞 ⇒ ⇒ 不随激励变化） | `tests/` |
| **T8** | **事实漂移订正**（**四处同批**）：`examples/axi4/axi_soc_example.cpp:4` 源码头注释 + `:24` 零使用 include + `examples/axi4/AGENTS.md` 的 OVERVIEW 表 + `.rddf/checks/examples-rtl-sim-exclusions.md:36`（SoC TB 交付后该行 **MUST 移除** ⇒ ⇒ 排除理由消失 ⇒ ⇒ **禁**保留 ⇒ ⇒ 防「双重登记」） | `examples/` + `.rddf/checks/` |
| **T9** | **残留登记**：`uart.irq` + 根的 `timer_irq` ⇒ ⇒ 台账 §7 + `.rddf/checks/arch-residual-registry.md` **两侧同批** ⇒ ⇒ 判据 = `verify_arch_ledger_residuals.sh` PASS | `.rddf/checks/` + `docs/architecture/`（**architect 域**） |
| **T10** | **账本锚点同步**：若新增 ctest 条目 ⇒ ⇒ `CTEST_TOTAL` 必变；若带 `verilator_e2e` 标签 ⇒ ⇒ `VERILATOR_E2E_TOTAL` 必变 ⇒ ⇒ 判据 = `bash scripts/verify_test_counts.sh` 绿 | 根 `AGENTS.md` |
| **T11** | **文档同步**：`docs/usage_guide/05-component-design.md` 的「SoC 装配」小节（**产物怎么接线 · 端口怎么找 · TB 怎么搭** —— 这是「下游怎么用」那一环）+ `examples/axi4/AGENTS.md` 的 FILES 表 | `docs/` + `examples/` |
| **T12** | **能力面**：`rtl-soc-output` 的 ADDED Requirements（含 **SoC 专属**条款 + **引用**通用条款 + 两份遗漏声明） | `openspec/changes/.../specs/` |

---

## Acceptance

> ⚠️ **期望值 MUST 手推**（禁抄仿真输出）⇒ ⇒ 「两份同源产物一致」**≠** 正确性证据。
> ⚠️ **禁裸数字**：module 数 / 端口数 MUST 从**产物**派生。

- [ ] **AC1** 🔴 **T0 · 正臂（今天即可判）**：SoC 层次化产物**发射成功** ⇒ ⇒ **产物存在** + **发射器退出码 0** ⇒ ⇒ 且 `HierarchyEmitResult.port_provenance` 中根 module 的 `origin == Bubbled` **计数 == 0** ⇒ ⇒ **负臂（依赖 ADR-072 D1-f）**：拔掉一根 tie-off ⇒ ⇒ 发射 **fail-closed**（`ok == false` + error 点名）⇒ ⇒ **D1-f 未落地时该负臂 MUST 记为「未验证 · 不得勾选」**
- [ ] **AC2** 🔴 **T1 · IP 边界可见（双层判据）**：① **冻结期望集**（4 个 sanitize 后模块名 + 计数 **5**）写进 `.rddf/checks/soc-rtl-expectations.md`（**手推**）② 产物 module 集 **==** 期望集 ⇒ ⇒ ⚠️ **禁**用「从 `ch_layer` 调用点派生」当唯一判据（作者漏给 `uart` 加 `ch_layer` ⇒ ⇒ 期望也变 ⇒ ⇒ **恒绿**）⇒ ⇒ **负向对照**：把某个 `ch_layer` 改回 `ch_module` ⇒ ⇒ 该 IP inline ⇒ ⇒ module 数必减 ⇒ ⇒ 必红
- [ ] **AC3** 🔴 **T3 · 顶层端口面**（载体 = **今天就有**的 `HierarchyEmitResult.port_provenance`，**不**依赖尚未落地的 `manifest.port_table[]`）：根 module 端口数 == **14**（框架 2 + 根自有 12）⇒ ⇒ 且每个端口的 `origin ∈ {Self, ClockReset}` ⇒ ⇒ 且 `Bubbled` 计数 == 0 ⇒ ⇒ **负向对照**：**恢复一根未驱动输入** ⇒ ⇒ 该端口 `origin == Bubbled` / 发射失败 ⇒ ⇒ 必红 ⇒ ⇒ ⚠️ **「去掉一个 `ch_layer`」不是有效负向对照**（成层 IP 的 `ch_out` 本就不冒泡 ⇒ ⇒ 顶层端口集合不变 ⇒ ⇒ 恒绿）
- [ ] **AC4** 🔴 **J-3 · lint 干净**：`verilator --lint-only --top-module top` **exit 0** · 零 `%Error` ⇒ ⇒ **禁** `-Wall` / `-Wno-fatal` / `-Wno-<单项>` ⇒ ⇒ ⚠️ **`--top-module` 用 `top`**（层次化根 module 名默认 = `top`，**不是**组件类名 `AxiSocTop`）
- [ ] **AC5** 🔴 **J-4 · 双形态逐拍等价**：`ch::hier_eq::compare` 判**逐拍全等** ⇒ ⇒ `ports` **由 `emit` 派生**（**禁手写**）⇒ ⇒ **激励 ≥10 拍且值不同**（一笔事务 **10 拍**）⇒ ⇒ **观测集 == {`rdata`, `gpio_out`, `uart_tx`}** 且含 **≥2 个随激励变化**的量 ⇒ ⇒ ⚠️ **不含 `done`**（粘滞 ⇒ ⇒ 恒为 1 ⇒ ⇒ 不随激励变化 ⇒ ⇒ `no-varying-signal` 豁免风险）⇒ ⇒ **变异自证**：篡改层次化产物的一条 `assign` ⇒ ⇒ 必红
- [ ] **AC6** 🔴 **J-5 · 独立 TB 可跑**：`tb_axi_soc_example.sv` 独立运行 ⇒ ⇒ `RESULT: checks=<N> fails=0`（N ≥ **4**）⇒ ⇒ 失败时**非零退出** ⇒ ⇒ **4 个可观测场景**：① 写 GPIO DATA_OUT ⇒ `gpio_out` == 手推值 ② 读 GPIO DATA_IN ⇒ `rdata` == `gpio_in` ③ 写 UART TX_DATA ⇒ `uart_tx` 出现 8N1 帧（位宽 = `UART_BIT_PERIOD` = **16 周期/位**）④ `rst_n` 拉低后读 GPIO DATA_OUT ⇒ **0** ⇒ ⇒ ⚠️ **「未映射地址 DECERR」已删除**（顶层无 `bresp` ⇒ ⇒ 不可观测；且 matrix 侧结构性不可触发）⇒ ⇒ ⚠️ **完成判定 MUST 用「按已知延迟采样」**（**禁**等 `done` 上升沿 —— `done` 粘滞）
- [ ] **AC7** 🔴 **D5 · 期望源在册**：`.rddf/checks/soc-rtl-expectations.md` 有该 SoC 一节（端口映射 + 地址映射 + 事务序列 + **手推结果** + 冻结的模块名期望集）⇒ ⇒ 且 TB 头的 `EXPECT-SRC:` 指针指向它 ⇒ ⇒ ⚠️ **负向对照需要载体**：护栏 ②′ MUST 增加「`EXPECT-SRC:` 的 `path#anchor` 在目标文件里存在对应 `{#anchor}`」的判据（现状只查格式 ⇒ ⇒ 无锚点解析 ⇒ ⇒ 无载体则本负臂**不得**勾选）
- [ ] **AC8** 🔴 **D7 · 覆盖登记（三桶）**：tsv 有该 SoC 的 `kind=soc` 行（`target` = `axi_soc_example`）⇒ ⇒ 护栏认这个 kind ⇒ ⇒ 且 ① `kind ∈ {target,dut}` 行数 == `REGISTERED_TARGETS` ② `kind=soc` 行数 == `REGISTERED_SOC` ③ **每个 data 行的 `kind` MUST 属于已知集合**（否则报错）⇒ ⇒ 且 **三桶之和 == 数据行总数**
- [ ] **AC9** ⚠️ **顺序无回归（可机械判定）**：`git diff --name-only` **不含** `src/**` 与 `include/**` ⇒ ⇒ 且 `ctest -LE perf` 全绿 ⇒ ⇒ 既有 10 份 TB 零改动
- [ ] **AC10** ⚠️ **事实漂移订正（四处同批）**：`examples/axi4/axi_soc_example.cpp:4` 源码头注释 + `:24` 零使用 include + `examples/axi4/AGENTS.md` OVERVIEW 表 + `.rddf/checks/examples-rtl-sim-exclusions.md:36`（SoC TB 交付后该行 **MUST 移除**）
- [ ] **AC11** ⚠️ **残留登记**：`uart.irq` 与根的 `timer_irq`（零消费者 ⇒ ⇒ 仍**是**顶层端口但**无观测意义**）⇒ ⇒ 两侧同批登记 ⇒ ⇒ 判据 = `verify_arch_ledger_residuals.sh` PASS
- [ ] **AC12** ⚠️ **T-3 边界可见**：能力面**明文**声明「**不支持** CPU 经 AXI 访问外部存储」⇒ ⇒ 精确串可 grep
- [ ] **AC13** ⚠️ **产品性质可见（metis 的隐藏意图）**：能力面**明文**声明 ① 「**SoC TB 不验证总线协议**（协议面归 IP 级 TB）」② 「产物是**固定功能的 demo SoC**；顶层不暴露 AXI，**不支持**自定义 master / 从设备」⇒ ⇒ 精确串可 grep
- [ ] **AC14** ⚠️ **账本与文档**：T10 锚点同步 + T11 文档（**「下游怎么用」那一环**：产物怎么接线 · 端口怎么找 · TB 怎么搭）⇒ ⇒ 判据 = `verify_test_counts.sh` 绿 + `docs/usage_guide/05-component-design.md` 的「SoC 装配」小节可 grep 到

---

## Capabilities

| capability | 动作 | 内容边界 |
|---|---|---|
| **`rtl-soc-output`**（**新增**） | **ADDED** | **只放 SoC 专属条款**：① SoC 产物 MUST 是**层次化**且 **IP 边界可见** ② SoC 顶层端口面 MUST == 框架 2 + 根自有 `__io`（零冒泡）③ 后代 `ch_in` MUST 全部有驱动源 ④ SoC 层 TB MUST 独立可跑 ⑤ 期望源 MUST 是仓内登记 ⑥ 覆盖登记 MUST 用 `kind=soc`（与「首批 10」**分桶**）⑦ **观测盲区** MUST 显式登记 ⑧ **不支持**「CPU 经 AXI 访问外部存储」 ⑨ **不支持**「自定义 master / 从设备」+ 「SoC TB 不验证总线协议」<br>⚠️ **通用条款（成层组件成 module / 单文件多 module / 与扁平等价 / lint 干净）MUST 引用** `rtl-hierarchical-output` 与 `hierarchical-equivalence-public-helper`，**禁重述**（⇒ ⇒ 防**双权威**） |

> ⚠️ **新增 capability（而非并入 `rtl-hierarchical-output`）** ⇒ ⇒ 与 ADR-072 的**在途 delta 零冲突**（后者正在 MODIFIED `rtl-hierarchical-output` 与 `rtl-emit-portability`）⇒ ⇒ 两个 change 的 delta **互不覆盖**。

---

## Impact

| 面 | 影响 |
|---|---|
| **example 源码** | `examples/axi4/axi_soc_example.cpp` 改 **~31 行**（11 tie-off + 19 单向接线 + 1 透传）+ 4 个 `ch_module` → `ch_layer` ⇒ ⇒ **解释器行为不变**（tie-off 的是本来没人用的口；成层只写两个标志） |
| **新增文件** | `examples/axi4/emit_rtl_soc.cpp` · `examples/axi4/tb_axi_soc_example.sv` · `.rddf/checks/soc-rtl-expectations.md` · `openspec/specs/rtl-soc-output/spec.md`（归档后） |
| **改动文件** | `tests/hier_rtl_products.cpp`（+1 子命令）· `scripts/verify_examples_rtl_sim.sh`（三桶 + `kind=soc` + ②′ 锚点）· `.rddf/checks/examples-rtl-sim-targets.tsv`（+1 行 +1 锚点）· `examples/axi4/AGENTS.md` · `docs/usage_guide/05-component-design.md` · 根 `AGENTS.md`（锚点） |
| **与 ADR-072 D1** | **写点零交集** ⇒ ⇒ 可并行；⇒ ⇒ **但本项的 T0 MUST 先于 ADR-072 D1 的合并**（否则 SoC 侧被 fail-closed 卡死）⇒ ⇒ **该顺序约束目前无人守** ⇒ ⇒ 本项落地后**自守**（CI 的 `pr-verilator-fast` 跑 `verify_examples_rtl_sim.sh` ⇒ ⇒ 发射 `ok=false` ⇒ ⇒ 护栏红）⇒ ⇒ **前提** = SoC 发射器**必须**把 `ok == false` 映射成非零退出（T2）⇒ ⇒ 且 ADR-072 的 change MUST 留一句前置说明 |

---

## metis 第 1 轮 gap 分析与处置（2026-11-16 · planner）

> metis 输出 **19 条**（P0 × 6 · P1 × 8 · P2 × 5）⇒ ⇒ **全部处置**。
> ⚠️ metis 查出的 **3 处事实错误**（本草案初稿的）：未驱动后代输入 **30 → 31**（漏 `master.rst_n`）· 顶层端口数 **36 → 44 → 45** · `done` **不是**单拍脉冲（是**粘滞**）⇒ ⇒ 均已按逐端口复算订正。

### P0（6 条 · 全闭）

| # | finding | 处置 |
|---|---|---|
| **P0-1** | **未驱动后代输入不是 30 是 31** —— `master.rst_n`（`axi_soc_example.cpp:47`）**零驱动**（连线段只驱动 `matrix`/`gpio`/`uart` 的 `rst_n`）⇒ ⇒ 扁平顶层 = 14 + 31 = **45** | ✅ **事实 ⑤** 改 **31**（四组：m1 11 · uart 11 · s1 8 · **master.rst_n 1**）· **T0** 增 `master.rst_n` 透传（`<<= io().rst_n`，仿 `:263-267`）⇒ ⇒ **11 + 19 + 1 = 31 全覆盖** · **AC1** 的 `Bubbled == 0` 自检 |
| **P0-2** | **T2 的形态是错的** —— `run_emit_cli()` **只发扁平**（`src/rtl_emit/cli.cpp:177-187`；`CliOptions` 无层次化选项）⇒ ⇒ 照抄 `emit_rtl.cpp` 形态 ⇒ ⇒ 产出 **1 个 module** ⇒ ⇒ D1/J-2/AC2 全部落空 ⇒ ⇒ 且要修就得动 `src/**` ⇒ ⇒ 违反 AC9 | ✅ **事实 ⑨** 新增 · **T2** 改写为「**自己解析** `--out`/`--emit-hygiene` ⇒ ⇒ 调 `toVerilogHierarchical` + `build_rtl_manifest` ⇒ ⇒ **`ok == false` MUST 映射为非零退出**」⇒ ⇒ **不碰 `src/**`** ⇒ ⇒ AC9 保留（改成可机械判定的 `git diff --name-only` 形态） |
| **P0-3** | **T3/T7 无可用载体** —— `AxiSocTop` 定义在 `.cpp` 里 ⇒ ⇒ `tests/` 不能 include（会带 `main()`）⇒ ⇒ 仓内既有做法是**复刻 fixture**（`tests/test_axi_lite_verilator_e2e.cpp:71`）⇒ ⇒ 那就是**第二份 SoC 定义**（与本仓反复治的「双权威」同型） | ✅ **T3** 改为「载体 = **`tests/hier_rtl_products.cpp` 的新子命令**（该文件**已是**非-ctest 产物生成器，`tests/CMakeLists.txt:1213` 只 `add_executable` ⇒ ⇒ 不动 `CTEST_TOTAL`）⇒ ⇒ **不复刻第二份 SoC 定义**」⇒ ⇒ **T7** 同载体 |
| **P0-4** | **AC2 的期望值来源未定** ⇒ ⇒ 若「从 `ch_layer` 调用点派生」⇒ ⇒ 作者漏给 `uart` 加 `ch_layer` ⇒ ⇒ 期望也变 ⇒ ⇒ **恒绿**（恰是 AC2 想抓的缺陷） | ✅ **AC2** 改**双层判据**：① **冻结期望集**（4 个 sanitize 后模块名 + 计数 5）写进期望源（**手推**）② 产物 module 集 **==** 期望集 ⇒ ⇒ ⚠️ **禁**「从调用点派生」作唯一判据 |
| **P0-5** | **AC1 今天恒绿 + 无载体** —— 今天**没有**「后代悬空 ⇒ fail-closed」（默认 `UndrivenPolicy::Bubble` ⇒ ⇒ 只**冒泡**）⇒ ⇒ 正臂今天必然成立、负臂今天必不成立 ⇒ ⇒ 该语义属 ADR-072 D1-f（其 change **尚未开工**） | ✅ **AC1** 拆成**正臂（今天即可判）**（产物存在 + 退出码 0 + `Bubbled == 0`）+ **负臂（标注依赖 D1-f）**（D1-f 未落地时**记为「未验证 · 不得勾选」**） |
| **P0-6** | **AC6 的场景集不可观测** —— 「未映射地址 DECERR」顶层**无 `bresp`** ⇒ ⇒ 不可区分；且 matrix 默认映射**铺满 32 位**⇒ ⇒ DECERR **结构性不可触发**；⇒ ⇒ `done` 是**粘滞**（`:149-151`）⇒ ⇒ 不能等上升沿，也不能作变化量 | ✅ **AC6** 重做 4 场景（写 GPIO DATA_OUT / 读 GPIO DATA_IN / 写 UART TX_DATA / `rst_n` 拉低后读）⇒ ⇒ 删掉 DECERR ⇒ ⇒ 完成判定 MUST 用「按已知延迟采样」（**禁**等 `done` 边沿）⇒ ⇒ **事实 ⑬⑭** 新增 |

### P1（8 条 · 全闭）

| # | finding | 处置 |
|---|---|---|
| P1-7 | **AC3 的载体指向尚未落地的 `manifest.port_table[]`** | ✅ 改为 **`HierarchyEmitResult.port_provenance`**（**今天就有**，事实 ⑩）⇒ ⇒ 判据 = 根端口 == 14 且 `origin ∈ {Self, ClockReset}` 且 `Bubbled == 0` ⇒ ⇒ **不依赖** ADR-072 的 T4 |
| P1-8 | **AC11 无 task 覆盖** | ✅ 新增 **T9**（台账 + 残留登记表两侧同批 · 判据 = `verify_arch_ledger_residuals.sh` PASS） |
| P1-9 | **AC7 的负向对照无载体**（护栏只查 `EXPECT-SRC:` 格式，不解析锚点） | ✅ **AC7** 标注「负向对照**需要载体**：护栏 ②′ MUST 增加锚点存在性判据」⇒ ⇒ **无载体则该负臂不得勾选**（与 AC1 的处理同款） |
| P1-10 | **AC8 的分桶引入新盲区**（未知 `kind` 两个桶都不计 ⇒ 静默缩面） | ✅ **AC8** 改为**三桶**：① `kind ∈ {target,dut}` == `REGISTERED_TARGETS` ② `kind=soc` == `REGISTERED_SOC` ③ **每个 data 行的 kind MUST 属于已知集合** + **三桶之和 == 数据行总数** ⇒ ⇒ **T6** 同步 |
| P1-11 | **AC10 漏 `examples/axi4/AGENTS.md`** | ✅ **AC10 / T8** 改为**四处同批**（源码头注释 + 零使用 include + 该 AGENTS + 排除表行） |
| P1-12 | **capability 会造第二份契约**（`rtl-hierarchical-output` 已有「成层组件成 module」「单文件多 module」「与扁平等价」） | ✅ Capabilities 段写明**只放 SoC 专属条款**、**通用条款引用**不重述 ⇒ ⇒ **AC13** 新增两份遗漏声明（「SoC TB 不验证总线协议」/「固定功能 demo SoC，不支持自定义 master」） |
| P1-13 | **漏账本锚点 task**（`CTEST_TOTAL` / `VERILATOR_E2E_TOTAL`） | ✅ 新增 **T10** + **AC14** |
| P1-14 | **漏文档 task**（`docs/usage_guide/05-component-design.md` + `examples/axi4/AGENTS.md`） | ✅ 新增 **T11** + **AC14**（「**下游怎么用**」那一环） |

### P2（5 条 · 全闭）

| # | finding | 处置 |
|---|---|---|
| P2-15 | **模块类型名该带实参**（裸名 ⇒ 两个不同参数的同名 IP 得到**同一个** `module_type_name` ⇒ ⇒ 撞名护栏不响 ⇒ ⇒ 产物合法但语义错位） | ✅ **T1** 一次定死：`"AxiLiteTestMaster"` / `"AxiLiteMatrix2x2<32,32>"` / `"AxiLiteGpio<8>"` / `"AxiLiteUart<16>"` ⇒ ⇒ sanitize 后 `AxiLiteTestMaster` / `AxiLiteMatrix2x2_32_32_` / `AxiLiteGpio_8_` / `AxiLiteUart_16_` ⇒ ⇒ 与 ADR-070 D2 先例一致 |
| P2-16 | **ADR-073 两处措辞要订正**（「`AxiSocTop` = 5」⇒ 根 module 名是 `top`；「`done` 是单拍脉冲」⇒ 实为**粘滞**） | ⚠️ **architect 域**（ADR-073 已 Accepted）⇒ ⇒ 本草案**不单方面改** ⇒ ⇒ 列为**移交 architect 的 dated 修订项**（见 §「移交 architect」）⇒ ⇒ 本草案内**已按订正后的口径**书写（事实 ⑬ / T1 / AC4 / AC6） |
| P2-17 | **V→AC 对照表缺失** | ✅ 本草案的 **AC1…AC14** 与 ADR-073 的 **V1…V12** 的对照见 §「V→AC 对照」 |
| P2-18 | **ADR-072 的 change 要留一句前置说明** | ✅ 列为**移交 architect**（ADR-072 的 change 属 planner 的另一 change ⇒ ⇒ 由该 change 的作者落笔）⇒ ⇒ 本项落地后顺序**自守**（CI 的 `pr-verilator-fast` ⇒ ⇒ `verify_examples_rtl_sim.sh` ⇒ ⇒ 发射 `ok=false` ⇒ ⇒ 护栏红）⇒ ⇒ **前提 = T2 的非零退出映射** |
| P2-19 | **「19 根双向」措辞错**（实为 19 根**单向** `<<=`）+ 排除表标题口径 | ✅ **T0** 改「**19 根单向接线**（11 正 + 8 反）」+ 排除表行交付后 **MUST 移除**（T8）⇒ ⇒ 「首批 10」口径 = 9 `target` + 1 `dut`（`kind=soc` 另计）写入 AC8 |

### V→AC 对照（ADR-073 V ↔ 本草案 AC）

| ADR-073 | 本草案 | 备注 |
|---|---|---|
| V1（J-2 IP 边界） | **AC2** | 双层判据（冻结期望集 + 产物级比对） |
| V2（J-1 端口面） | **AC3** | 载体改 `port_provenance`；负向对照改「恢复一根未驱动输入」 |
| V3（D2 可发射性） | **AC1** | 拆正臂/负臂；数字 30 → **31** |
| V4（J-3 lint） | **AC4** | `--top-module top` |
| V5（J-4 双形态） | **AC5** | 观测集去掉 `done`；激励 ≥10 拍 |
| V6（J-5 TB） | **AC6** | 4 场景重做（删 DECERR） |
| V7（期望源） | **AC7** | 负向对照需载体 |
| V8（覆盖登记） | **AC8** | 三桶 |
| V9（顺序无回归） | **AC9** | 可机械判定形态 |
| V10（事实漂移） | **AC10** | 四处同批 |
| V11（残留登记） | **AC11** | |
| V12（T-3 边界） | **AC12** | |
| ——（metis P1-12 追加） | **AC13** | 产品性质的两份声明 |
| ——（metis P1-13/14 追加） | **AC14** | 账本 + 文档 |

### 移交 architect（**本项无权改**）

| # | 事项 | 理由 |
|---|---|---|
| ① | **ADR-073 的 dated 修订**：`「\`AxiSocTop\` = 5」` ⇒ 「根 module 名 `top`（默认）+ 4 个成层 module = 5」· 「`done` 是单拍脉冲」⇒ 「`done` 是**粘滞**完成标志（`:149-151`），故不能靠其边沿同步，也**不可**作『随激励变化』的观测证据」· **未驱动后代输入 30 → 31**（`master.rst_n`）· 扁平顶层 44 → **45** · 「19 根**双向**」⇒ 「19 根**单向**接线」 | ADR-073 已 Accepted ⇒ ⇒ 决策内容/事实的修订属 **architect 域**（须复审）⇒ ⇒ 本项**只登记不自行修改** · **✅ 已完成**（architect · 2026-11-16 · ADR-073 v1.1 · commit `57c781c` · 实为 **7 处**：额外含 V6 删 DECERR + `master.rst_n` 无行为影响 · oracle 第 2 轮复核 0 BLOCKER） |
| ② | **ADR-072 的 change 加一句前置说明**：「本 change 的 D1-f 会让 `axi_soc_example` 的发射 fail-closed ⇒ ⇒ 前置 = `soc-rtl-product-and-tb-2026-11` 的 T0」 | 属 ADR-072 那个 change 的作者（**仍待办**） |
