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

① ADR-072 D1 一旦落地，未接完的 SoC 会被**卡死** ⇒ ⇒ **SoC 侧必须先做**；
② SoC 侧与 ADR-072 的写点**零交集** ⇒ ⇒ 可并行 ⇒ ⇒ **不增加关键路径**。

---

## What Changes

### A. **任务映射**（ADR-073 的 10 条决策 ⇒ ⇒ 13 个 task）

| task | 内容 | 写点 | AC |
|---|---|---|---|
| **T0** | **后代输入驱动源**（**可发射性前置**）：`matrix.m1_*` **11** 根 tie-off（仿 `examples/axi4/phase3a_complete.cpp:259-263`）+ `matrix.s1_*` ↔ `uart` **19** 根**单向**接线（11 正 + 8 反）+ `master.rst_n` **1** 根透传（`<<= io().rst_n`，仿 `:263-267`）⇒ ⇒ **31 全覆盖** | `examples/axi4/axi_soc_example.cpp` | AC1 |
| **T1** | **成层声明**：4 个 `ch_module` → `ch_layer<T>`（**档 ③**）⇒ ⇒ **模块类型名一次定死**：`"AxiLiteTestMaster"` / `"AxiLiteMatrix2x2<32,32>"` / `"AxiLiteGpio<8>"` / `"AxiLiteUart<16>"` ⇒ ⇒ 产物标识符 `AxiLiteTestMaster` / `AxiLiteMatrix2x2_32_32_` / `AxiLiteGpio_8_` / `AxiLiteUart_16_` | 同上 | AC2 |
| **T2** | **发射入口**：`examples/axi4/emit_rtl_soc.cpp` —— ⚠️ **不**用 `run_emit_cli`（它**只发扁平**：`src/rtl_emit/cli.cpp:177-187`）⇒ ⇒ 自己解析 `--out` / `--emit-hygiene` ⇒ ⇒ 调 **`toVerilogHierarchical(out+"/top.v", *root, opts)`** + `build_rtl_manifest(*root, ctx)` ⇒ ⇒ **`result.ok == false` MUST 映射为非零退出**（否则护栏读到残留文件 ⇒ 假绿）+ CMake target `emit_rtl_axi_soc` | `examples/axi4/`（2 个文件） | AC1 AC3 |
| **T3** | **产物级判据**：**载体 = `tests/hier_rtl_products.cpp` 的新子命令**（该文件**已是**非-ctest 产物生成器 ⇒ ⇒ 不动 `CTEST_TOTAL`）⇒ ⇒ **禁**在 `tests/` 复刻第二份 SoC 定义（`AxiSocTop` 定义在 `.cpp` 里 ⇒ ⇒ 仓内既有做法 = 复刻 fixture ⇒ ⇒ 双权威） | `tests/hier_rtl_products.cpp` | AC2 AC3 |
| **T4** | **期望源登记**：`.rddf/checks/soc-rtl-expectations.md`（端口映射 + 地址映射 + 事务序列 + **手推结果** + **冻结的模块名期望集**） | `.rddf/checks/` | AC2 AC7 |
| **T5** | **独立 TB**：`examples/axi4/tb_axi_soc_example.sv`（**14** 个顶层端口 + 4 个可观测场景） | `examples/axi4/` | AC6 |
| **T6** | **覆盖登记 + 护栏**：tsv +1 行 `kind=soc`（`target` = `axi_soc_example`）+ 锚点 `# REGISTERED_SOC: 1` + 护栏 ⓪ **三桶**判定 + `EXPECT-SRC:` **锚点存在性**判据 | `.rddf/checks/` + `scripts/` | AC7 AC8 |
| **T7** | **双形态一致性**（J-4）：`ch::hier_eq::emit` + `compare`（`ports` **由产物派生**）· 激励 **≥10 拍**（一笔事务 **10 拍**）· 观测集 {`rdata`, `gpio_out`, `uart_tx`}（⚠️ **不含 `done`** —— 它**粘滞** ⇒ ⇒ 恒为 1 ⇒ ⇒ 不随激励变化） | `tests/` | AC5 |
| **T8** | **事实漂移订正**（**四处同批**）：`examples/axi4/axi_soc_example.cpp:4` 源码头注释 + `:24` 零使用 include + `examples/axi4/AGENTS.md` OVERVIEW 表 + `.rddf/checks/examples-rtl-sim-exclusions.md:36`（SoC TB 交付后该行 **MUST 移除**） | `examples/` + `.rddf/checks/` | AC10 |
| **T9** | **残留登记**：`uart.irq` + 根的 `timer_irq` ⇒ ⇒ 台账 §7 + `.rddf/checks/arch-residual-registry.md` **两侧同批** | `.rddf/checks/` + `docs/architecture/`（**architect 域**） | AC11 |
| **T10** | **账本锚点同步**：`CTEST_TOTAL` / `VERILATOR_E2E_TOTAL`（若带标签） | 根 `AGENTS.md` | AC14 |
| **T11** | **文档同步**：`docs/usage_guide/05-component-design.md` 的「SoC 装配」小节（**「下游怎么用」那一环**：产物怎么接线 · 端口怎么找 · TB 怎么搭）+ `examples/axi4/AGENTS.md` 的 FILES 表 | `docs/` + `examples/` | AC14 |
| **T12** | **能力面**：`rtl-soc-output` 的 ADDED Requirements | `openspec/changes/.../specs/` | AC12 AC13 |

### B. **DoD（Definition of Done）**

- [ ] 全部 **13 个 task** 完成，且 **AC1–AC14** 逐条有**工具证据**
- [ ] `ctest -LE perf` 全绿（既有 10 份 TB 零改动）
- [ ] `bash scripts/verify_examples_rtl_sim.sh`（**含 `--self-test`**）绿
- [ ] `bash scripts/verify_test_counts.sh` 绿
- [ ] `bash scripts/verify_arch_ledger_residuals.sh` PASS（行数 +1）
- [ ] `bash scripts/verify_openspec_delta.sh` 绿 + `openspec validate <change>` 非零错误
- [ ] `bash scripts/verify_hierarchical_rtl.sh`（**含 `--self-test`**）绿（层次化面零回归）
- [ ] 归档后 `grep -n "冒泡\|Bubble\|Bubbled" openspec/specs/rtl-soc-output/spec.md` **零命中**
- [ ] 归档后 `grep -rn "AxiSocTop\|AxiLiteMatrix2x2" openspec/specs/rtl-soc-output/spec.md` **零命中**（模块名是**明文非契约**）

---

## 验收标准

> ⚠️ **期望值 MUST 手推**（禁抄仿真输出）⇒ ⇒ 「两份同源产物一致」**≠** 正确性证据。
> ⚠️ **禁裸数字**：module 数 / 端口数 MUST 从**产物**派生。

- [ ] **AC1** 🔴 **T0 · 正臂（今天即可判）**：SoC 层次化产物**发射成功** ⇒ ⇒ **产物存在** + **发射器退出码 0** ⇒ ⇒ 且 `HierarchyEmitResult.port_provenance` 中根 module 的 `origin == Bubbled` **计数 == 0** ⇒ ⇒ **负臂（依赖 ADR-072 D1-f）**：拔掉一根 tie-off ⇒ ⇒ 发射 **fail-closed**（`ok == false` + error 点名）⇒ ⇒ **D1-f 未落地时该负臂 MUST 记为「未验证 · 不得勾选」**
- [ ] **AC2** 🔴 **T1 · IP 边界可见（双层判据）**：① **冻结期望集**（4 个 sanitize 后模块名 + 计数 **5**）写进期望源（**手推**）② 产物 module 集 **==** 期望集 ⇒ ⇒ ⚠️ **禁**「从 `ch_layer` 调用点派生」作唯一判据（漏给 `uart` 加 `ch_layer` ⇒ ⇒ 期望也变 ⇒ ⇒ **恒绿**）⇒ ⇒ **负向对照**：改回 `ch_module` ⇒ ⇒ module 数必减 ⇒ ⇒ 必红
- [ ] **AC3** 🔴 **T3 · 顶层端口面**（载体 = **今天就有**的 `HierarchyEmitResult.port_provenance`）：根 module 端口数 == **14**（框架 2 + 根自有 12）⇒ ⇒ 且每个端口 `origin ∈ {Self, ClockReset}` ⇒ ⇒ 且 `Bubbled == 0` ⇒ ⇒ **负向对照**：**恢复一根未驱动输入** ⇒ ⇒ 必红 ⇒ ⇒ ⚠️ **「去掉一个 `ch_layer`」不是有效负向对照**
- [ ] **AC4** 🔴 **J-3 · lint 干净**：`verilator --lint-only --top-module top` **exit 0** · 零 `%Error` ⇒ ⇒ **禁** `-Wall` / `-Wno-fatal` / `-Wno-<单项>` ⇒ ⇒ ⚠️ **`--top-module` 用 `top`**（根 module 名默认 = `top`，**不是**组件类名）
- [ ] **AC5** 🔴 **J-4 · 双形态逐拍等价**：`ch::hier_eq::compare` 判**逐拍全等** ⇒ ⇒ `ports` **由 `emit` 派生** ⇒ ⇒ **激励 ≥10 拍且值不同** ⇒ ⇒ **观测集 == {`rdata`, `gpio_out`, `uart_tx`}** 且含 **≥2 个随激励变化**的量 ⇒ ⇒ ⚠️ **不含 `done`**（粘滞 ⇒ ⇒ 恒为 1）⇒ ⇒ **变异自证**：篡改层次化产物的一条 `assign` ⇒ ⇒ 必红
- [ ] **AC6** 🔴 **J-5 · 独立 TB 可跑**：`tb_axi_soc_example.sv` 独立运行 ⇒ ⇒ `RESULT: checks=<N> fails=0`（N ≥ **4**）⇒ ⇒ 失败时**非零退出** ⇒ ⇒ **4 个可观测场景**：① 写 GPIO DATA_OUT ⇒ `gpio_out` == 手推值 ② 读 GPIO DATA_IN ⇒ `rdata` == `gpio_in` ③ 写 UART TX_DATA ⇒ `uart_tx` 出现 8N1 帧（`UART_BIT_PERIOD` = **16 周期/位**）④ `rst_n` 拉低后读 GPIO DATA_OUT ⇒ **0** ⇒ ⇒ ⚠️ **完成判定 MUST 用「按已知延迟采样」**（**禁**等 `done` 上升沿）
- [ ] **AC7** 🔴 **D5 · 期望源在册**：期望源有该 SoC 一节 + TB 头的 `EXPECT-SRC:` 指针指向它 ⇒ ⇒ ⚠️ **负向对照需要载体**（护栏 ②′ MUST 增加锚点存在性判据 ⇒ ⇒ 无载体则该负臂**不得**勾选）
- [ ] **AC8** 🔴 **D7 · 覆盖登记（三桶）**：tsv 有 `kind=soc` 行 ⇒ ⇒ ① `kind ∈ {target,dut}` 行数 == `REGISTERED_TARGETS` ② `kind=soc` 行数 == `REGISTERED_SOC` ③ **每个 data 行的 `kind` MUST 属于已知集合**（否则报错）⇒ ⇒ 且 **三桶之和 == 数据行总数**
- [ ] **AC9** ⚠️ **顺序无回归（可机械判定）**：`git diff --name-only` **不含** `src/**` 与 `include/**` ⇒ ⇒ 且 `ctest -LE perf` 全绿
- [ ] **AC10** ⚠️ **事实漂移订正（四处同批）**
- [ ] **AC11** ⚠️ **残留登记**：`uart.irq` 与根的 `timer_irq` ⇒ ⇒ 两侧同批 ⇒ ⇒ `verify_arch_ledger_residuals.sh` PASS
- [ ] **AC12** ⚠️ **T-3 边界可见**：能力面**明文**声明「**不支持** CPU 经 AXI 访问外部存储」⇒ ⇒ 精确串可 grep
- [ ] **AC13** ⚠️ **产品性质可见**：能力面**明文**声明 ① 「**SoC TB 不验证总线协议**（协议面归 IP 级 TB）」② 「产物是**固定功能的 demo SoC**；顶层不暴露 AXI，**不支持**自定义 master / 从设备」⇒ ⇒ 精确串可 grep
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
3. **`AxiSocTop` 的内部连线被补完** —— 31 个此前未驱动的后代输入现在有驱动源 ⇒ ⇒ **解释器与后端行为不变**（tie-off 的是本来没人用的口；`master.rst_n` 此前**零驱动**⇒ ⇒ 补上后**复位路径才真正通到 master** ⇒ ⇒ ⚠️ **这是一处行为修正**：此前 master 未收到复位 ⇒ ⇒ **下游若依赖「master 不被复位」的旧行为会受影响** ⇒ ⇒ MUST 登记为「行为变更（非符号移除）」）。
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
| **G-1** | **spec delta 写齐**（`rtl-soc-output` 的 ADDED）⇒ ⇒ `verify_openspec_delta.sh` 绿 + `openspec validate <change>` 非零错误 | builder | ⬜ 待办（T12） |
| **G-2** | **公开面前置登记**（第 3 条「行为变更」⇒ ⇒ `master.rst_n` 补驱动）⇒ ⇒ `.rddf/checks/public-api-surface.md`「行为变更」节 + `docs/CHANGELOG.md` 一行 | builder | ⬜ 待办 |
| **G-3** | ⚠️ **architect 域的 dated 修订**（ADR-073 的 5 处措辞/事实订正 + ADR-072 的 change 的前置说明）⇒ ⇒ 见 improvement 的「移交 architect」段 | architect | ⬜ 待办（**不阻塞** T0–T3） |
