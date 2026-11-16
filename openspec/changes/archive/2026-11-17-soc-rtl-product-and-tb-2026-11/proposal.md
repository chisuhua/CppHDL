# Proposal: SoC 层的 RTL 产物形态 + SoC 层 TB 的交付契约（`rtl-soc-output`）

> **来源**：`.rddf/improvements/soc-rtl-product-and-tb-2026-11.md`（planner · 2026-11-16 · **metis 1 轮 · 19 条 finding 全处置**）
> **架构依据**：`docs/adr/ADR-073-soc-rtl-product-and-tb.md`（**Accepted** · 2026-11-16 · oracle ×1 + critic ×2 复审 APPROVE）
> **方案细则**：`docs/architecture/2026-11-16-soc-rtl-product-and-tb.md`（现状快照 + 模块关系 + 可枚举改动清单）
> 🚪 **开工前门已闭**：ADR-073 已 Accepted（本 change 的架构依据）
> ⚠️ **顺序硬约束**：本 change 的 **T0（后代输入驱动源）MUST 先于 `rtl-productization-surface-2026-11`（ADR-072 D1）的合并**
> ⇒ ⇒ 理由：ADR-072 **D1-f** 裁定「后代悬空 `ch_in` ⇒ 发射 fail-closed」⇒ ⇒ `AxiSocTop` 今天有 **31** 个未驱动后代输入
> ⇒ ⇒ 不先做 T0 ⇒ ⇒ **SoC 侧被 fail-closed 卡死** ⇒ ⇒ 该顺序在 T2 落地后**自守**（CI 的 `pr-verilator-fast` 跑 `verify_examples_rtl_sim.sh`
> ⇒ ⇒ 发射 `ok == false` ⇒ ⇒ 护栏红）⇒ ⇒ **前提** = T2 的**非零退出映射**

---

## Why

### 缺陷（实测 · 可复算）

1. **SoC 的 RTL 产物从未被交付** —— `examples/axi4/emit_rtl.cpp` 的 factory 造的是 `AxiLiteTop`（AXI4-Lite 单 IP），
   **不是** `AxiSocTop` ⇒ ⇒ 「SoC 层 RTL 生成」这条能力**今天不存在可交付物**。
2. **IP 边界在产物里不可见** —— 4 个 IP 都用 `ch_module`（默认 inline，ADR-061）⇒ ⇒ 层次化产物**全 inline 成 1 个 module**
   ⇒ ⇒ 交付物与扁平单 IP 产物**无结构区别** ⇒ ⇒ 「SoC」这个形态**不存在于产物中**。
3. 🔴 **SoC 本身没接完 —— 31 个后代 `ch_in` 未驱动** ⇒ ⇒ `matrix.m1_*` **11** · `uart` 的 AXI 从机输入 **11** ·
   `matrix.s1_*` 的**响应侧** **8** · `master.rst_n` **1** ⇒ ⇒ **ADR-072 D1-f 落地后 SoC 产物发不出来**
   ⇒ ⇒ 且 `matrix.s1_* → uart` 的 AXI 通道**零连线** ⇒ ⇒ **uart 不在总线上** ⇒ ⇒ 「多 IP + 总线互连」这个 SoC 的核心价值**今天没被验证过**。
4. **SoC 层 TB 缺失**，且排除理由本身有事实漂移（「+ **Timer**」⇒ ⇒ 实为 4 个 IP、**无 Timer**）。

### 为什么现在做

① ADR-072 D1 一旦落地，未接完的 SoC 会被**卡死** ⇒ ⇒ **SoC 侧必须先做**（真实强度见下节 ⚠️ ②）；
② ⚠️ **「写点零交集」不成立** —— ADR-073 D10 把 ADR-072 的写点写成「`src/**` · `include/**` · 10 份既有 TB · 两份 capability」，
   那是**子集**：ADR-072 的同批必改 ④⑤⑥⑪ 与 T5/T6/T11/T13 **还要写** `.rddf/checks/**` · `scripts/**` · `tests/**` · `docs/**`
   ⇒ ⇒ 实测**共用 6 个工件** ⇒ ⇒ **「可并行」只对代码目录成立**；共用工件 MUST **串行**（见下节）。

### 跨 change 协调（与 `rtl-productization-surface-2026-11` 的共用面）

> ⚠️ **本节是 B-3 的落点**，镜像登记在对方 proposal 的同名小节 ⇒ ⇒ 两侧口径 MUST 一致。
> 依据：ADR-073 D10 的写点表**低估**了 ADR-072 的写集（ADR-072 自身同批必改 ⑤⑥⑪ + T5/T6/T11/T13 可查）。

| # | 共用工件 | 本 change 侧 | `rtl-productization-surface` 侧 | 执行顺序 |
|---|---|---|---|---|
| ① | `.rddf/checks/examples-rtl-sim-targets.tsv` | T6（+`kind=soc` 行 · `REGISTERED_SOC: 1`） | T5（`axi4_lite` `min_checks` 45→37 · `scenarios` 8→7） | **MUST 串行** —— 后落者 MUST 在同一 commit 内重算**两桶**锚点（`REGISTERED_TARGETS` / `REGISTERED_SOC`） |
| ② | `scripts/verify_examples_rtl_sim.sh` | T6（判据 ⓪ 改**三桶**） | T13 / AC8（新增四件套判据） | **MUST 串行**（不同区域，无先后）⇒ ⇒ ⚠️ **编号必顺延**：现存到 **⑦**（`scripts/verify_examples_rtl_sim.sh:14` 的 ④ 已是「TB 能编译」）⇒ ⇒ 新项是 **⑧** |
| ③ | ~~`tests/hier_rtl_products.cpp`~~ | ✅ **已消解** —— 2026-11-16 裁决「选 (a)」⇒ ⇒ 本 change 的 T3 载体落 **examples 侧**（`emit_rtl_axi_soc --check-products`）⇒ ⇒ **不再碰 `tests/`** ⇒ ⇒ 本行**不再构成共用**（共用工件 6 → **5**） | ADR-072 同批必改 ④（`:114-141` · `:476`） | — |
| ④ | `docs/usage_guide/05-component-design.md` | T11（新增「SoC 装配」小节） | T11（`:552` · `:680` 的「扁平规则」一分为二） | **可并行**（不同小节），但 MUST 同批提交评审 |
| ⑤ | 🔴 `.rddf/checks/arch-residual-registry.md` + `docs/architecture/2026-11-02-hierarchical-rtl-output.md:§7` | T9（`uart.irq` / `timer_irq` 新增残留） | T14（残差 id=23 闭口） | 🔴 **MUST 串行** —— `verify_arch_ledger_residuals.sh` 对台账 §7 ↔ 登记表做**严格 1:1 双向**校验（含状态格的生成句前缀 + fail-closed）⇒ ⇒ 并行改 ⇒ **必然红** |
| ⑥ | `docs/CHANGELOG.md` | v1.1 已加 1 条 | T9（新增公开符号 1 行） | append-only ⇒ ⇒ 后落者 MUST rebase 后重排 |

⚠️ **D10 的「T0 MUST 先于 ADR-072 D1 合并」经复核比字面弱**（**不推翻 D10**，只补事实）：

- 首批 10 里唯一的 SoC（`riscv-mini`）走**扁平发射**（`examples/riscv-mini/emit_rtl.cpp:31` 的 `run_emit_cli`）⇒ ⇒ 不经过 `toVerilogHierarchical` ⇒ ⇒ D1-f 碰不到它；
- AXI SoC 今天**没有任何**层次化发射路径（`examples/axi4/emit_rtl.cpp:36-40` 造的是 `AxiLiteTop`），`tests/hier_rtl_products.cpp` 与 `scripts/**` 里 **grep 不到** `axi_soc`；
- ⇒ ⇒ D1-f 落地**不立刻打破任何既有护栏**；真正的顺序要求是「T0 必在自己的 T2 发射之前」——**本 change 内部自成立**。

🔴 **真正硬的跨 change 耦合在另一处（= B-4）**：ADR-072 **D1-h 退役 `PortProvenanceOrigin::Bubbled`**，而本 change 的
**AC1 / AC3 直接断言 `origin == Bubbled` 计数 == 0** ⇒ ⇒ 后落的一方**编译期就红**：

| 谁先落 | 后落者 MUST 做什么 |
|---|---|
| 本 change 先 | ADR-072 的 D1-h 退役清单 MUST 补 **`examples/axi4/emit_rtl_soc.cpp` 的 `--check-products` 实现**（`Bubbled == 0` 子句删掉，只留 `origin ∈ {Self, ClockReset}`）—— ⚠️ 载体已随 2026-11-16 的「选 (a)」裁决从 `tests/hier_rtl_products.cpp` 迁到 examples 侧 |
| ADR-072 先 | 本 change 的 **AC1 / AC3** MUST 在写测试时**不引用** `Bubbled`（`origin ∈ {Self, ClockReset}` 已等价且更强） |

⇒ ⇒ **裁决（本 proposal 侧）**：AC1 / AC3 的 `Bubbled == 0` 子句标为**条件性子句** —— ADR-072 D1-h 落地后 MUST 删除，
且删除**不削弱**判据（`origin ∈ {Self, ClockReset}` 覆盖同一集合）。

---

## What Changes

### A. **任务映射**（ADR-073 的 10 条决策 ⇒ ⇒ 13 个 task）

| task | 内容 | 写点 | AC |
|---|---|---|---|
| **T0** | **后代输入驱动源**（**可发射性前置**）：`matrix.m1_*` **11** 根 tie-off（⚠️ **成例只覆盖 5/11**：`examples/axi4/phase3a_complete.cpp:259-263` 只 tie 了 `awvalid/wvalid/bready/arvalid/rready` ⇒ 另 **6 根 payload**（`awaddr`/`awprot`/`wdata`/`wstrb`/`araddr`/`arprot`）MUST 用**带位宽常量**，照抄会留 6 根悬空）+ `matrix.s1_*` ↔ `uart` **19** 根**单向**接线（11 正 + 8 反）+ `master.rst_n` **1** 根透传（`<<= io().rst_n`，仿 `:263-267`）⇒ ⇒ **31 全覆盖** | `examples/axi4/axi_soc_example.cpp` | AC1 |
| **T1** | **成层声明**：4 个 `ch_module` → `ch_layer<T>`（**档 ③**）⇒ ⇒ **模块类型名一次定死**：`"AxiLiteTestMaster"` / `"AxiLiteMatrix2x2<32,32>"` / `"AxiLiteGpio<8>"` / `"AxiLiteUart<16>"` ⇒ ⇒ 产物标识符 `AxiLiteTestMaster` / `AxiLiteMatrix2x2_32_32_` / `AxiLiteGpio_8_` / `AxiLiteUart_16_` | 同上 | AC2 |
| **T2** | **发射入口**：`examples/axi4/emit_rtl_soc.cpp` —— ⚠️ **不**用 `run_emit_cli`（它**只发扁平**：`src/rtl_emit/cli.cpp:177-187`）⇒ ⇒ 自己解析 `--out` / `--emit-hygiene` ⇒ ⇒ 调 **`toVerilogHierarchical(out+"/top.v", *root, opts)`** + `build_rtl_manifest(*root, ctx)` ⇒ ⇒ **`result.ok == false` MUST 映射为非零退出**（否则护栏读到残留文件 ⇒ 假绿）+ CMake target `emit_rtl_axi_soc`<br>⇒ ⇒ ✅ **复用 example 的同一份定义（零结构改动）**：照 `examples/axi4/emit_rtl.cpp:22-24` 的既有先例 —— `#define main cpphdl_example_main_disabled` + `#include "axi_soc_example.cpp"` + `#undef main` ⇒ ⇒ `AxiSocTop`（定义在 `axi_soc_example.cpp:159-277`）**不需要搬进头文件** ⇒ ⇒ example 源码**零结构改动** | `examples/axi4/`（2 个文件） | AC1 AC3 |
| **T3** | ✅ **产物级判据（载体 = examples 侧自检子命令 · 2026-11-16 用户裁决「选 (a)」）**<br>⇒ ⇒ **载体 = `emit_rtl_axi_soc --check-products`**（与 T2 同一个 target ⇒ ⇒ **不碰 `tests/`** ⇒ ⇒ 不触发 `verify_no_duplicate_rv32i_headers.sh` 的 **C3**（`:998` 禁 `tests/` include `examples/`）与 **C7**（`:170-196` 禁 tests 编译 examples 源）⇒ ⇒ 也**不需要**复刻 fixture ⇒ ⇒ 无双权威）<br>⇒ ⇒ **`--check-products` MUST 逐条输出并以非零退出**：① 产物 module 数 == **成层 IP 数 + 1**（5）② 顶层端口数 == **14** 且 `origin ∈ {Self, ClockReset}` ③ `verilator --lint-only --top-module top` **exit 0** · 零 `%Error` ④ 双形态逐拍等价（`ch::hier_eq::emit` + `compare`，`ports` **由产物派生**）<br>⇒ ⇒ ⚠️ **产物目录 MUST 独立**（如 `build/rtl_products/soc/`）—— 主产物目录受 `scripts/verify_hierarchical_rtl.sh:94-97` 的「**恰好 3 个 `.v`**」约束 ⇒ ⇒ 放进去会连带红 R4/R6/R8<br>⇒ ⇒ ⚠️ **AC4 的 lint 执行者 = `scripts/verify_examples_rtl_sim.sh` 的 kind=soc 专用判据**（与 T6 同批）—— 否则独立目录**没人 lint**（R1–R3 只遍历主目录）<br>⇒ ⇒ 📌 **裁决记录（2026-11-16 · 用户）**：原方案「载体 = `tests/hier_rtl_products.cpp` 的新子命令」**在仓规下不可行** —— ① C3（`:998`）禁 `tests/` include examples ② C7（`:170-196`）禁根 `tests/CMakeLists.txt` 编译 examples 源 ③ `tests/CMakeLists.txt:1214` 的 `hier_rtl_products` 只挂 `include/` ④ 「复刻 fixture」被本 proposal 自己禁止 ⇒ ⇒ **四条路全堵** ⇒ ⇒ **选 (a)**：载体落到 **examples 侧**，复用 `emit_rtl.cpp:22-24` 的 `#define main` + `#include` 先例（**不抽头文件 · example 源码零结构改动**） | `examples/axi4/` | AC2 AC3 **AC4** |
| **T4** | **期望源登记**：`.rddf/checks/soc-rtl-expectations.md`（端口映射 + 地址映射 + 事务序列 + **手推结果** + **冻结的模块名期望集**）<br>⇒ ⇒ 🔴 **登记一处 ADR 订正（architect 域）**：ADR-073 §4.3 风险④（`ADR-073:205`）写「期望值 MUST 与成层声明处同源（从 `ch_layer` 调用点派生，禁手写常量）」 ⇒ ⇒ 但本 change 的 delta（`specs/rtl-soc-output/spec.md:98-100`）与 AC2 写「MUST **手推并冻结** · MUST NOT 从成层声明处派生」 ⇒ ⇒ **delta 是正确的**（否则「漏给某 IP 加成层声明」会同时改变期望 ⇒ AC2/V1 的负向对照**恒绿**）⇒ ⇒ ADR 该行 MUST 走 dated 订正 | `.rddf/checks/` | AC2 AC7 |
| **T5** | **独立 TB**：`examples/axi4/tb_axi_soc_example.sv`（**14** 个顶层端口 + 4 个可观测场景） | `examples/axi4/` | AC6 |
| **T6** | **覆盖登记 + 护栏**：tsv +1 行 `kind=soc`（`target` = `axi_soc_example`）+ 锚点 `# REGISTERED_SOC: 1` + 护栏 ⓪ **三桶**判定 + `EXPECT-SRC:` **锚点存在性**判据 | `.rddf/checks/` + `scripts/` | AC7 AC8 |
| **T7** | **双形态一致性**（J-4）：`ch::hier_eq::emit` + `compare`（`ports` **由产物派生**）· 激励 **≥10 拍**（一笔事务 **10 拍**）· 观测集 {`rdata`, `gpio_out`, `uart_tx`}（⚠️ **不含 `done`** —— 它**粘滞** ⇒ ⇒ 恒为 1 ⇒ ⇒ 不随激励变化）<br>⇒ ⇒ ✅ **载体随 T3 落在 examples 侧**（`emit_rtl_axi_soc --check-products` 的第 ④ 项）⇒ ⇒ **不碰 `tests/`** ⇒ ⇒ 不触发 C3 / C7 | `examples/axi4/` | AC5 |
| **T8** | **事实漂移订正**（**四处同批**）：`examples/axi4/axi_soc_example.cpp:4` 源码头注释 + `:24` 零使用 include + `examples/axi4/AGENTS.md` OVERVIEW 表 + `.rddf/checks/examples-rtl-sim-exclusions.md:36`（SoC TB 交付后该行 **MUST 移除**） | `examples/` + `.rddf/checks/` | AC10 |
| **T9** | **残留登记**：`uart.irq` + 根的 `timer_irq` ⇒ ⇒ 台账 §7 + `.rddf/checks/arch-residual-registry.md` **两侧同批** | `.rddf/checks/` + `docs/architecture/`（**architect 域**） | AC11 |
| **T10** | **账本锚点与全局校验**：① `CTEST_TOTAL` / `VERILATOR_E2E_TOTAL`（若带标签；T7 的双形态用例会增 CTEST 条目）⇒ 锚点值**从实测派生**（可用 `scripts/update_test_count_anchors.sh`）② **顺序无回归复核**：`git diff --name-only` **不含** `src/**` / `include/**` + `ctest -LE perf` 全绿 | 根 `AGENTS.md` | AC14 **AC9** |
| **T11** | **文档同步**：① `docs/usage_guide/05-component-design.md` 的「SoC 装配」小节（**「下游怎么用」那一环**：产物怎么接线 · 端口怎么找 · TB 怎么搭）② `examples/axi4/AGENTS.md` 的 FILES 表 ③ 🔴 **根 `AGENTS.md:196` 的 capability 计数** —— ⚠️ **实测当前已是 48**（`openspec/specs/` 下的 capability 目录数**实测 = 48**，但 AGENTS.md 写 47 ⇒ **该处本身已陈旧**）⇒ ⇒ 归档后应为 **49** ⇒ MUST 同批把「47」订正为「48」再追加「48 → 49」，或直接写「当前 49」（**禁**只写 `47 → 48` —— 那会继续错一格） | `docs/` + `examples/` + 根 `AGENTS.md` | AC14 |
| **T12** | **能力面**：`rtl-soc-output` 的 ADDED Requirements<br>⇒ ⇒ ⚠️ **两处措辞义务**（**D1-h 退役后的必然结果**）：① 正文**MUST NOT** 指称已退役符号（`UndrivenPolicy::Bubble` / `Bubbled`）——「冒泡端口」只可作为**禁止态措辞**出现（见 DoD 的范围圈定） ② **`## Purpose` 归档后手工写**（`openspec archive` **不合并** delta 的 Purpose ⇒ 新 capability 会留 `TBD - created by archiving …` 占位符） | `openspec/changes/.../specs/` | AC12 AC13 |

### B. **DoD（Definition of Done）**

- [ ] 全部 **13 个 task** 完成，且 **AC1–AC14** 逐条有**工具证据**（⚠️ **例外**：**AC1 的负臂**是**跨 change 条件项** —— 依赖 ADR-072 D1-f；D1-f 未落地时该负臂记为「未验证」且**不计入** DoD 的逐条要求，否则 DoD **结构上不可满足**）
- [ ] `ctest -LE perf` 全绿（既有 10 份 TB 零改动）
- [ ] `bash scripts/verify_examples_rtl_sim.sh`（**含 `--self-test`**）绿
- [ ] `bash scripts/verify_test_counts.sh` 绿
- [ ] `bash scripts/verify_arch_ledger_residuals.sh` PASS ⇒ ⇒ **行数由台账派生**（**禁写死**：本 change 新增 **2** 行 —— `uart.irq` + 根的 `timer_irq`；实际基线随 `rtl-productization-surface` 的 T14 先后而变）
- [ ] `bash scripts/verify_openspec_delta.sh` 绿 + `openspec validate <change>` 非零错误
- [ ] `bash scripts/verify_hierarchical_rtl.sh`（**含 `--self-test`**）绿（层次化面零回归）
- [ ] `emit_rtl_axi_soc --check-products` **exit 0**（4 项逐条 PASS：module 数 5 · 顶层端口 14 · lint 绿 · 双形态等价）⇒ ⇒ 且产物落在**独立目录**（不污染主产物目录的「恰好 3 个 `.v`」）
- [ ] 归档后 `grep -nE "UndrivenPolicy::Bubble|Bubbled" openspec/specs/rtl-soc-output/spec.md` **零命中**（⚠️ **范围圈定**：只查**已退役符号名**；中文「冒泡端口」是**禁止态措辞**（描述「MUST 为 0」的态），**不属该判据** —— 本 delta 现有 4 处「冒泡」（`:25` `:31` `:35` `:37`）全部属于这一类 ⇒ 若按字面零命中**归档后必然自红**）
- [ ] 归档后 `grep -rn "AxiSocTop\|AxiLiteMatrix2x2" openspec/specs/rtl-soc-output/spec.md` **零命中**（模块名是**明文非契约**）
- [ ] 归档后 `openspec/specs/rtl-soc-output/spec.md` 的 `## Purpose` **非占位符**（MUST NOT 含 `TBD - created by archiving`）⇒ ⇒ 内容 MUST 说明「SoC 层 RTL 产物的形态与 TB 契约」

---

## 验收标准

> ⚠️ **期望值 MUST 手推**（禁抄仿真输出）⇒ ⇒ 「两份同源产物一致」**≠** 正确性证据。
> ⚠️ **禁裸数字**：module 数 / 端口数 MUST 从**产物**派生。

- [ ] **AC1** 🔴 **T0 · 正臂（今天即可判）**：SoC 层次化产物**发射成功** ⇒ ⇒ **产物存在** + **发射器退出码 0** ⇒ ⇒ 且 `HierarchyEmitResult.port_provenance` 中根 module 的 `origin == Bubbled` **计数 == 0**（⚠️ **条件性子句**：ADR-072 D1-h 退役 `PortProvenanceOrigin::Bubbled` 后 MUST 删除；删除不削弱判据 —— `origin ∈ {Self, ClockReset}`（见 AC3）覆盖同一集合）⇒ ⇒ **负臂（依赖 ADR-072 D1-f）**：拔掉一根 tie-off ⇒ ⇒ 发射 **fail-closed**（`ok == false` + error 点名）⇒ ⇒ **D1-f 未落地时该负臂 MUST 记为「未验证 · 不得勾选」**
- [ ] **AC2** 🔴 **T1 · IP 边界可见（双层判据）**：① **冻结期望集**（4 个 sanitize 后模块名 + 计数 **5**）写进期望源（**手推**）② 产物 module 集 **==** 期望集 ⇒ ⇒ ⚠️ **禁**「从 `ch_layer` 调用点派生」作唯一判据（漏给 `uart` 加 `ch_layer` ⇒ ⇒ 期望也变 ⇒ ⇒ **恒绿**）⇒ ⇒ **负向对照**：改回 `ch_module` ⇒ ⇒ module 数必减 ⇒ ⇒ 必红
- [ ] **AC3** 🔴 **T3 · 顶层端口面**（载体 = **今天就有**的 `HierarchyEmitResult.port_provenance`）：根 module 端口数 == **14**（框架 2 + 根自有 12）⇒ ⇒ 且每个端口 `origin ∈ {Self, ClockReset}` ⇒ ⇒ 且 `Bubbled == 0`（⚠️ **条件性子句 · 同 AC1**：ADR-072 D1-h 退役 `PortProvenanceOrigin::Bubbled` 后 MUST 删除；删除不削弱判据）⇒ ⇒ **负向对照**：**恢复一根未驱动输入** ⇒ ⇒ 必红 ⇒ ⇒ ⚠️ **「去掉一个 `ch_layer`」不是有效负向对照**
- [ ] **AC4** 🔴 **J-3 · lint 干净**：**执行者 = `scripts/verify_examples_rtl_sim.sh` 的 kind=soc 专用判据**（与 T6 同批）⇒ ⇒ 跑 `emit_rtl_axi_soc --check-products` 的第 ③ 项 ⇒ ⇒ `verilator --lint-only --top-module top` **exit 0** · 零 `%Error` ⇒ ⇒ **禁** `-Wall` / `-Wno-fatal` / `-Wno-<单项>` ⇒ ⇒ ⚠️ **`--top-module` 用 `top`**（根 module 名默认 = `top`，**不是**组件类名）
- [ ] **AC5** 🔴 **J-4 · 双形态逐拍等价**（载体 = `emit_rtl_axi_soc --check-products` 第 ④ 项）⇒ ⇒ ⚠️ **这只是 parity，不是正确性** —— A 侧是 `VerilatorBackend` 从**同一份 ctx** 生成的扁平产物（`src/hierarchy_equivalence_impl.h:258-269`）⇒ ⇒ 对「发射底座算错」结构性失明（根 `AGENTS.md`：「两后端一致 ≠ 正确性证据」）⇒ ⇒ **正确性锚点唯一地压在 AC6 的手推断言上**；本 AC 只判「两份同源产物不背离」⇒ ⇒ `ch::hier_eq::compare` 判**逐拍全等** ⇒ ⇒ `ports` **由 `emit` 派生** ⇒ ⇒ **激励 ≥10 拍且值不同** ⇒ ⇒ **观测集 == {`rdata`, `gpio_out`, `uart_tx`}** 且含 **≥2 个随激励变化**的量 ⇒ ⇒ ⚠️ **不含 `done`**（粘滞 ⇒ ⇒ 恒为 1）⇒ ⇒ **变异自证**：篡改层次化产物的一条 `assign` ⇒ ⇒ 必红
- [ ] **AC6** 🔴 **J-5 · 独立 TB 可跑**：`tb_axi_soc_example.sv` 独立运行 ⇒ ⇒ `RESULT: checks=<N> fails=0`（N ≥ **4**）⇒ ⇒ 失败时**非零退出** ⇒ ⇒ **4 个可观测场景**：① 写 GPIO DATA_OUT ⇒ `gpio_out` == 手推值 ② 读 GPIO DATA_IN ⇒ `rdata` == `gpio_in` ③ 写 UART TX_DATA ⇒ `uart_tx` 出现 8N1 帧（`UART_BIT_PERIOD` = **16 周期/位**）④ `rst_n` 拉低后读 GPIO DATA_OUT ⇒ **0** ⇒ ⇒ ⚠️ **完成判定 MUST 用「按已知延迟采样」**（**禁**等 `done` 上升沿）<br>⇒ ⇒ ⚠️ **14 个端口 MUST 全部连接**（否则严格 flags 下 `%Warning-PINMISSING` **致命** —— 仓内实证 `examples/axi4/tb_axi4_lite_example.sv:33-34`）⇒ 其中 **`timer_irq`（零消费死端口）接 `0`** · **`uart_rx` 接 `1`**（否则 RX 引擎把低电平当起始位）· **`gpio_in` 给固定值**（AC6 ② 才有可观测值）⇒ ⇒ UART 场景需 **≥160 拍**（8N1 = 10 bit × `UART_BIT_PERIOD` = 16）
- [ ] **AC7** 🔴 **D5 · 期望源在册**：期望源有该 SoC 一节 + TB 头的 `EXPECT-SRC:` 指针指向它 ⇒ ⇒ ⚠️ **负向对照需要载体**（护栏 ②′ MUST 增加锚点存在性判据 ⇒ ⇒ 无载体则该负臂**不得**勾选）
- [ ] **AC8** 🔴 **D7 · 覆盖登记（三桶）**：tsv 有 `kind=soc` 行 ⇒ ⇒ ① `kind ∈ {target,dut}` 行数 == `REGISTERED_TARGETS` ② `kind=soc` 行数 == `REGISTERED_SOC` ③ **每个 data 行的 `kind` MUST 属于已知集合**（否则报错）⇒ ⇒ 且 **三桶之和 == 数据行总数**
- [ ] **AC9** ⚠️ **顺序无回归（可机械判定）**：`git diff --name-only` **不含** `src/**` 与 `include/**` ⇒ ⇒ 且 `ctest -LE perf` 全绿
- [ ] **AC10** ⚠️ **事实漂移订正（四处同批 · 逐条可判）**：① `examples/axi4/axi_soc_example.cpp:4` 的头注释**不含** `+ Timer` ② `:24` **无** `#include "axi4/peripherals/axi_timer.h"` ③ `examples/axi4/AGENTS.md` 的 OVERVIEW 表**无** Timer 行 ④ `.rddf/checks/examples-rtl-sim-exclusions.md` **无** `axi_soc_example` 行
- [ ] **AC11** ⚠️ **残留登记**：`uart.irq` 与根的 `timer_irq` ⇒ ⇒ 两侧同批 ⇒ ⇒ `verify_arch_ledger_residuals.sh` PASS
- [ ] **AC12** ⚠️ **T-3 边界可见**：能力面**明文**声明「**不支持** CPU 经 AXI 访问外部存储」⇒ ⇒ 精确串可 grep（⚠️ **今天已绿** —— delta 在册，`spec.md:163` 已含该串 ⇒ 本 AC = **防漂**，不是新建）
- [ ] **AC13** ⚠️ **产品性质可见**（⚠️ **今天已绿** —— `spec.md:163-165` 已含三串 ⇒ 本 AC = **防漂**）：能力面**明文**声明 ① 「**SoC TB 不验证总线协议**（协议面归 IP 级 TB）」② 「产物是**固定功能的 demo SoC**；顶层不暴露 AXI，**不支持**自定义 master / 从设备」⇒ ⇒ 精确串可 grep
- [ ] **AC14** ⚠️ **账本与文档**：T10 锚点同步 + T11 文档（「SoC 装配」小节可 grep 到）

---

## Capabilities

| capability | 动作 | 内容边界 |
|---|---|---|
| **`rtl-soc-output`**（**新增**） | **ADDED** | **只放 SoC 专属条款**；通用条款（成层组件成 module / 单文件多 module / 与扁平等价 / lint 干净）MUST **引用** `rtl-hierarchical-output` 与 `hierarchical-equivalence-public-helper`，**禁重述** |

> ⚠️ **新增而非并入** ⇒ ⇒ 与 `rtl-productization-surface-2026-11` 的**在途 delta 零冲突**（后者正在 MODIFIED `rtl-hierarchical-output` 与 `rtl-emit-portability`）⇒ ⇒ 两个 change 的 delta **互不覆盖**。

---

## 用户可见变更

> ⚠️ 护栏 `verify_public_api_visibility.sh` 的 R1 载体（归档时 MUST 非空）。

1. **新增 SoC 层 RTL 产物** —— `build/examples/axi4/emit_rtl_axi_soc --out <dir>` ⇒ ⇒ 产出 **5 个 module** 的单文件 Verilog（4 个 IP 各一层 + 根 `top`）⇒ ⇒ **失败时非零退出**（`ok == false` ⇒ ⇒ **不写任何文件**）。
2. **新增独立 Verilog TB** —— `examples/axi4/tb_axi_soc_example.sv` ⇒ ⇒ `RESULT: checks=<N> fails=0` ⇒ ⇒ 失败**非零退出**。
3. **`AxiSocTop` 的内部连线被补完** —— 31 个此前未驱动的后代输入现在有驱动源，**分三组、行为影响不同**：
   - `matrix.m1_*` **11 根 tie-off** + `master.rst_n` **1 根透传** ⇒ ⇒ **无行为影响**（m1 无人使用；`master.rst_n` 的 `AxiLiteTestMaster::describe()` **零引用** `rst_n`，是**声明但未消费**的输入 ⇒ ⇒ 补驱动不改变任何行为）
   - `matrix.s1_* ↔ uart` **19 根单向接线** ⇒ ⇒ **有行为变化**（uart 此前**不在总线上**，接通后经总线可访问 ⇒ ⇒ 这是本 change 的目的之一）
   ⇒ ⇒ ⚠️ **不涉及库公开符号**：改动只在 **example 文件**（`examples/axi4/axi_soc_example.cpp`），不碰 `include/**` / `src/**`
   ⇒ ⇒ **无需公开面（`public-api-surface.md`）登记**（非符号变更）
   ⇒ ⇒ ⚠️ 但 **example 有行为变化**（uart 可达）：既有 `main()` 断言 MUST 仍绿（builder 在 T0 负责）
4. **tsv 新增 `kind=soc`** ⇒ ⇒ 消费该清单的下游工具 MUST 认这个 kind（护栏已扩展）。
5. **新增 capability `rtl-soc-output`** ⇒ ⇒ 归档后主 spec 多一个 capability（47 → **48**）。
6. **产品边界（明示）** —— ① SoC TB **不验证总线协议**（协议面归 `tb_axi4_lite_example.sv`）② 产物是**固定功能的 demo SoC**，顶层**不暴露 AXI**，**不支持**自定义 master / 从设备 ③ **不支持**「CPU 经 AXI 访问外部存储」。

---

## Out of Scope

| 项 | 理由 | 归属 |
|---|---|---|
| 「对外 AXI 主口」档（T-3） | 阻塞只剩「无 AXI 从机 BFM」+「顶层 AXI 展平策略未定义」 | **独立立项**（触发条件 = 用户要求「CPU 经 AXI 访问外部存储」） |
| 19 个 example 的 TB | 期望值 MUST 手推 ⇒ 人工工作 | 独立排期（ADR-072 D5） |
| 特权级 | ADR-072 D3 | `rv32i-privileged-csr-2026-11`（已 draft） |
| `ch_mem` 跨层的形态 | ADR-066 v1.2 已收窄（禁令只管「mem 本体跨 module」）⇒ ⇒ **不是**本 change 的阻塞 | —— |
| `uart.irq` / 根的 `timer_irq` 的**功能** | 零消费者 ⇒ ⇒ 登记为残留（仍在端口面内） | 台账 + 残留登记表 |

---

## 🚪 门

| # | 事项 | 归属 | 状态 |
|---|---|---|---|
| **G-0** | **ADR-073 Accepted** | architect | ✅ 已闭（2026-11-16） |
| **G-1** | **spec delta 写齐**（`rtl-soc-output` 的 9 条 ADDED Requirements）⇒ ⇒ `verify_openspec_delta.sh` 绿 + `openspec validate <change>` 非零错误 | builder | ✅ **已闭** —— delta 已落（`specs/rtl-soc-output/spec.md` · `openspec validate` = valid · delta 自检 4/4）⇒ ⇒ **T12 的性质随之改变**：不再是「写 ADDED」，而是「**校验并保持未漂**」（任何改动 MUST 重跑两条校验） |
| **G-2** | ~~**公开面前置登记**（第 3 条「行为变更」）~~ ⇒ ⇒ **核查结论：不需要登记**（改动只在 example 文件，无 `include/**` / `src/**` 公开符号变更；`master.rst_n` 补驱动**无行为影响**；`s1↔uart` 的行为变化限于 example 内部 ⇒ ⇒ **G-2 关闭**） | planner | ✅ 已闭（2026-11-16 · 原「行为变更登记」撤销） |
| **G-3** | ⚠️ **architect 域的 dated 修订** —— **ADR-073 的 7 处事实订正已完成**（commit `57c781c` · v1.1）⇒ ⇒ **剩余**：ADR-072 的 change 加一句前置说明（「本 change 的 D1-f 会让 `axi_soc_example` 发射 fail-closed ⇒ ⇒ 前置 = 本 change 的 T0」）⇒ ⇒ 归 ADR-072 那个 change 的作者 | architect | 🟡 **部分已闭**（ADR-073 部分 ✅；ADR-072 部分 ⬜，**不阻塞** T0–T3） |
