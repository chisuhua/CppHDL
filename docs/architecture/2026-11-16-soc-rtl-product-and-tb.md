# SoC 层的 RTL 产物与 SoC 层 TB —— 完整方案（架构主题文档）

- **Date**: 2026-11-16
- **Author**: architect role（build agent）
- **状态**: **Draft**（**oracle 第 1 轮 REJECT**（3 BLOCKER + 6 SHOULD_FIX）⇒ ⇒ 已修 ⇒ ⇒ **critic 待跑**；配套 ADR = `docs/adr/ADR-073-soc-rtl-product-and-tb.md`）
- **上游**:
  - `docs/adr/ADR-072-rtl-productization-surface-and-boundary.md`（**Accepted · v2.1**）—— 顶层端口面收敛 + 产品单元四件套
  - `docs/adr/ADR-070-rv32i-hierarchical-layering-granularity.md`（**Accepted**）—— 实例级 `ch_layer` 成层机制
  - `docs/adr/ADR-058-hierarchical-rtl-output.md`（**Accepted**）—— 层次化 RTL 输出 + 显式 backend 入口
  - `docs/adr/ADR-042-bundle-field-first-class-nodes.md` —— bundle 的端口规则
- **下游**: `.rddf/improvements/soc-rtl-product-and-tb-2026-11.md` → openspec change（planner 立）

> ⚠️ **本文件是「架构主题文档」**（当前态快照 + 模块关系 + 方案细则）⇒ ⇒ **决策在 ADR-073** ⇒ ⇒
> 本文件**不新增**架构义务，只承载**推导 / 逐文件裁决 / 可枚举改动清单**。

---

## 1. 现状快照（2026-11-16 · 实测）

### 1.1 三层结构

```
┌─ 装配层 ────────────────────────────────────────────────────────────┐
│  Component::describe() 里 ch_module<T> 实例化 + 逐信号 <<= 连线     │
│  实例: examples/axi4/axi_soc_example.cpp:192-201                    │
│        ch_module<AxiLiteTestMaster>       master{"master"}          │
│        ch_module<AxiLiteMatrix2x2<32,32>> matrix{"matrix"}          │
│        ch_module<AxiLiteGpio<8>>          gpio{"gpio"}              │
│        ch_module<AxiLiteUart<16>>         uart{"uart"}              │
│  ⚠️ 4 个 IP **均无** CH_HIERARCHY_LAYER ⇒ ⇒ 层次化需**档 ③**         │
│     （ch_layer<T>(instance, module_type_name)）                      │
└──────────────────────────────────────────────────────────────────────┘
                              ↓ 发射
┌─ 产物层 ────────────────────────────────────────────────────────────┐
│  扁平: toVerilog(path, ctx)          → 1 个 module, 顶层 44 端口     │
│      （SoC 自己的数 = 框架 2 + 根自有 12 + 30 冒泡；⚠️ 130/68 是 RV32I 的）│
│  层次化: toVerilogHierarchical(path, root) → 5 个 module, 顶层 14    │
│      （= 框架 2 + 根自有 12；**前提 = D2 的 30 个后代输入全部有驱动源**）│
│  manifest: build_rtl_manifest(root, ctx) → ports[] + (待加) port_table[]│
│  ⚠️ **SoC 目前没有 emit target**（examples/axi4/emit_rtl.cpp 只发射  │
│     AxiLiteTop）⇒ ⇒ SoC 产物**从未被交付过**                          │
└──────────────────────────────────────────────────────────────────────┘
                              ↓ 验证
┌─ 验证层 ────────────────────────────────────────────────────────────┐
│  (A) 解释器 / JIT      ch_device<T> + Simulator            ✅ 今天可用│
│  (B) 仓内 Verilator 后端  VerilatorBackend(auto-mode)      ✅ 今天可用│
│      ⚠️ 只仿**扁平遗留产物**（ADR-072 D1-a）                          │
│  (C) 独立 Verilog TB    tb_<soc>.sv + verilator CLI         ⚠️ 只有 IP│
│      ✅ 已交付: examples/riscv-mini/tb_rv32i_soc.sv（SoC 形态）        │
│      ✅ 已交付: examples/axi4/tb_axi4_lite_example.sv（IP 形态）        │
│      ❌ **缺失: AXI SoC（多 IP + 总线互连）的 TB 与 emit target**      │
│      ⚠️ 且 SoC **本身没接完**（**30** 个后代 `ch_in` 未驱动）⇒ ⇒ ADR-072     │
│         D1-f 落地后**产物发不出来** ⇒ ⇒ 见 §1.2 G-3′                   │
│  (D) 双形态一致性      ch::hier_eq::emit + compare      ✅ 公共 API  │
└──────────────────────────────────────────────────────────────────────┘
```

### 1.2 缺口清单

| # | 缺口 | 证据 |
|---|---|---|
| **G-1** | **SoC 产物从未交付** —— `examples/axi4/emit_rtl.cpp` 的 factory 造的是 `AxiLiteTop`（AXI4-Lite 单 IP），**不是** `AxiSocTop` | `examples/axi4/emit_rtl.cpp:36-40` |
| **G-2** | **SoC 的 IP 边界在产物里不可见** —— 4 个 IP 都用 `ch_module`（默认 inline，ADR-061 决策）⇒ ⇒ 层次化产物会**全 inline 成 1 个 module** | `examples/axi4/axi_soc_example.cpp:192-201` · `include/component.h:36-39`（`CH_HIERARCHY_LAYER`）· 4 个 IP 均**无**该宏 |
| **G-3** | 🔴 **SoC 层 TB 缺失** —— AXI SoC 在 19 个未覆盖项里，排除理由 =「与 `riscv-mini`（SoC）同类重复 · 规模更大 · 失败定位更难」 | `.rddf/checks/examples-rtl-sim-exclusions.md:36` · ⚠️ 该行写「AXI 矩阵 + GPIO + UART + **Timer**」⇒ ⇒ **事实漂移**：实测 `AxiSocTop` 只有 **4** 个 IP，**无 Timer**（漂移源头 = `examples/axi4/axi_soc_example.cpp:4` 的源码头注释 + `:24` 的零使用 include） |
| **G-3′** | 🔴 **SoC 本身没接完 —— 30 个后代 `ch_in` 未驱动** —— ① `matrix.m1_*` 的 **11** 个 ② `uart` 的 **11** 个 AXI 从机输入 ③ `matrix.s1_*` 的 **8** 个**响应侧**输入（**整个文件零 `s1_*` 连线** —— gpio 占的是 `s0_*`）；**且 `matrix.s1_* → uart` 的 AXI 通道零连线** ⇒ ⇒ **ADR-072 D1-f（后代悬空 ⇒ fail-closed）落地后，SoC 产物发不出来** ⇒ ⇒ 且 uart 不在总线上 ⇒ ⇒ UART 场景不可达 | `examples/axi4/axi_soc_example.cpp`（`grep 'uart\.io()'` 全文件仅 3 命中：`:258`/`:259`/`:264`）· `include/axi4/peripherals/axi_uart.h:54-76` · `include/axi4/axi4_lite_matrix.h:66-90` · ✅ **解法已有**：tie-off 惯用法 = `examples/axi4/phase3a_complete.cpp:259-302` |
| **G-4** | **SoC 的期望源无载体** —— D4 要求期望值**手推** ⇒ ⇒ 必须有一份仓内可审计的「地址映射 + 事务序列 + 手推结果」 | ADR-072 §2.4 · 现有 `.rddf/checks/examples-rtl-expectations.md` 只覆盖 IP 级 |
| **G-5** | **「对外 AXI 主口」档无 BFM** —— CPU 经 AXI 访问**外部** DDR ⇒ TB 需要实现 AXI 从机响应模型 ⇒ ⇒ 仓内**没有** | `include/axi4/` 全部是 RTL 侧（`axi4_lite.h` / `axi_interconnect_4x4.h` / `axi_to_axilite.h`），**无** testbench BFM |
| **G-6** | **SoC 产物缺「双形态一致性」判据的登记** —— `ch::hier_eq` 是公共 API，但只在 RV32I 交付物上用过 | `include/hierarchy_equivalence.h:131,155` |

### 1.3 四条会咬 SoC 层的结构约束（**固化自既有 ADR**）

| # | 约束 | 后果 | 依据 |
|---|---|---|---|
| **S-a** | **顶层端口 MUST 是扁平标量**（MUST NOT 用 bundle） | top 上声明 `AxiLite aw` ⇒ **零个 Verilog 端口生成** | ADR-042 D5：「bundle 只用于**模块间**连接」·「顶层 bundle 仍**不是** Verilog 端口」 |
| **S-b** | **`ch_mem` **本体** MUST NOT 跨 module**（⚠️ **v1.2 已收窄**：仅「mem 本体跨 module」才拒；「**仅实参视线跨层**」是**已支持**的形态） | SoC 内的共享存储 MUST 住在**同一个 module** 内；但「CPU + 外部 DDR」**不**因此被禁 | ADR-060 §2.1 · **ADR-066 v1.2 §2.7 决策 ⑦**（`:241,246-256`）· capability `rtl-hierarchical-output/spec.md:589` |
| **S-c** | **被父层驱动的 `ch_in` ⇒ 内部 wire（非端口）** | IP 的输入由父层连 ⇒ 不占顶层端口（**正确**）；但顶层若写 `io().x <<= …` ⇒ 该端口**从顶层消失** | `src/codegen_verilog.cpp:432-441` |
| **S-d** | **Verilator 后端端口 MUST ≤ 64 位** | SoC 对外暴露 >64 位端口 ⇒ 后端 refuse | `include/core/verilator_backend.h` 的 `PortBitwidthOverflow` |

⇒ ⇒ **`S-a` 是 SoC 方案的第一性约束**：它意味着 **AXI 总线永远不会出现在 SoC 顶层端口上** ⇒ ⇒
SoC 的顶层端口面 = 「控制面 + 物理外设面」的**扁平标量集合**（`AxiSocTop` = 12 个）⇒ ⇒
**D1 的端口面收敛对 SoC 不会砍掉总线**，它砍掉的是 **IP 内部信号的冒泡**。

---

## 2. 完整方案

### 2.1 装配层 —— 把 IP 边界声明出来

```cpp
// examples/axi4/axi_soc_example.cpp 的 describe() 内
ch::ch_layer<AxiLiteTestMaster>       master{"master",       "AxiLiteTestMaster"};
ch::ch_layer<AxiLiteMatrix2x2<32,32>> matrix{"matrix",       "AxiLiteMatrix2x2"};
ch::ch_layer<AxiLiteGpio<8>>          gpio{"gpio",          "AxiLiteGpio"};
ch::ch_layer<AxiLiteUart<16>>         uart{"uart",          "AxiLiteUart"};
```

⚠️ **必须是档 ③**（实例处给模块类型名）—— 4 个 IP **均无** `CH_HIERARCHY_LAYER` ⇒ ⇒
`include/module.h:171-175` 的 `requires(!has_class_layer_name<T>::value)` 强制第二个参数。

⚠️ **模块类型名的稳定性**：ADR-070 §6 ⑦ 的用户裁决 = **明文非契约**（改名不构成破坏性变更，
唯一义务 = 产物重新 lint 干净）⇒ ⇒ SoC 的模块类型名**不**进 capability 契约 ⇒ ⇒
但**它进 SoC 产物的结构判据**（§2.4 的 J-2）⇒ ⇒ **改名 ⇒ 判据要重算** ⇒ ⇒ 这是**已知代价**。

### 2.1′ 后代输入的驱动源（**可发射性的前置**）

⚠️ **每一个后代 `ch_in` MUST 有驱动源** ⇒ ⇒ 二选一：

| 组 | 处置 | 机理 |
|---|---|---|
| `matrix.m1_*`（11 个，未用主口） | **tie-off 到常量**（`x <<= ch_bool(false)` / `ch_uint<N>(0_d)`） | tie-off 使该 `ch_in` **有内部 driver** ⇒ ⇒ **发射侧**（`src/codegen_verilog.cpp:432-441`）**不成为顶层端口** ⇒ ⇒ **不冒泡**；**分析侧**（`src/hierarchy_analysis.cpp:404-415`，判据键 = `num_srcs() == 0`）**不进 `pending_bubbles`** ⇒ ⇒ **不是「悬空」** ⇒ ⇒ 不触发 ADR-072 D1-f 的 fail-closed |
| `matrix.s1_*` ↔ `uart` 的 AXI 通道（19 根双向） | **接通**（真互联） | ⇒ ⇒ uart 才在总线上 ⇒ ⇒ UART 场景可达 |

⇒ ⇒ **惯用法仓内已有**：`examples/axi4/phase3a_complete.cpp:259-302`（m1/m2/m3 五个 + s1/s2/s3 八个的同类处理）⇒ ⇒ **照抄即可**。
⚠️ **不接也不 tie-off 的后果**：ADR-072 D1-f 落地 ⇒ ⇒ **发射 fail-closed** ⇒ ⇒ SoC 产物**发不出来** ⇒ ⇒ 这是**「能不能交付」的问题**，不是风格问题。

### 2.2 产物层 —— **SoC 的产物形态 = 层次化**

| | 扁平 | **层次化（SoC 采用）** |
|---|---|---|
| module 数 | 1 | **成层 IP 数 + 1**（`AxiSocTop` = 5） |
| 顶层端口 | 框架 2 + 根自有 12 + **30 冒泡** = 44 | 框架 2 + 根自有 12 = **14**（**D2 后**） |
| IP 边界 | **不可见** | **可见**（IP 各自成 module） |
| `backend_port_set` | ❌ **无重载**（`toVerilog` 返回 `void`） | ✅ `backend_port_set(emitted, "top")` |
| `hier_eq::emit/compare` | A 侧固定是扁平 | ✅ **A 侧 = 扁平 · B 侧 = 层次化** |

⇒ ⇒ **裁决：SoC 的交付产物 = 层次化**（理由见 ADR-073 §3 选项 ①）。

**新增 emit target**（1 个文件，形态照 `examples/riscv-mini/emit_rtl.cpp`）：

```
examples/axi4/emit_rtl_soc.cpp        → CMake target emit_rtl_axi_soc
```
⚠️ `AxiSocTop` 定义在 `axi_soc_example.cpp` 里 ⇒ ⇒ **必须**照 `examples/axi4/emit_rtl.cpp`
的形态（`#define main …` + `#include "axi_soc_example.cpp"`）⇒ ⇒ **发射入口零额外副本**（无需搬组件定义）
⇒ ⇒ **但 D2 的连线改造仍落在 example 源码**（~30 行）⇒ ⇒ **MUST NOT** 读成「example 不用动」。

### 2.3 验证层 —— SoC 层 TB 的三档激励模型

| 档 | SoC 形态 | 顶层激励 | TB 要写什么 | 仓内实例 |
|---|---|---|---|---|
| **T-1** | **自含 CPU + 固件** | `prog_we` / `prog_addr` / `prog_data` | 装载小程序（**手推编码**）· 跑 · 断言写回/落 tohost | ✅ `tb_rv32i_soc.sv`（`checks=24`） |
| **T-2** | **test master 在 SoC 内部** | `start` / `write` / `addr` / `wdata` | 驱动 4 个控制信号 · 等 `done` · 断言 `rdata` / `gpio_out` / `uart_tx` | ✅ `AxiSocTop`（**最简单 ⇒ 本方案取它**）⚠️ **前提 = `s1 → uart` 接通**（G-3′）⇒ ⇒ 否则 UART 场景**不可达** |
| **T-3** | **CPU 经 AXI 访问外部 DDR** | AXI 主口（**bundle ⇒ 不在顶层**） | **需要 AXI 从机 BFM** | ❌ **无** ⇒ ⇒ **独立立项**（G-5） |

⚠️ **T-2 为何最省**：SoC 内部自带 master ⇒ ⇒ TB **不实现总线协议** ⇒ ⇒ 只需驱动控制信号。
⚠️ **T-3 的难点在 BFM**：AXI 的 `awready`/`bvalid` 等是**组合/时序混合**的握手 ⇒ ⇒ BFM 要建模
⇒ ⇒ 且 AXI bundle **不上顶层**（S-a）⇒ ⇒ TB 与 SoC 之间**只能**靠「SoC 自己把 AXI 端口展平成标量」才连得上
⇒ ⇒ **T-3 的前提是顶层展平策略**（本方案**不**做 ⇒ ⇒ 独立立项）。

### 2.4 判据层 —— 五条产品级判据

| # | 判据 | 载体 | 归属 |
|---|---|---|---|
| **J-1** | **顶层端口面**：框架 2 + 根自有 12 = **14**（**逐字逐序**）⇒ ⇒ **零冒泡端口** | `manifest.port_table[]`（ADR-072 D1-c）vs 产物端口表 | J-1 |
| **J-2** | **IP 边界可见**：产物 module 数 == 成层 IP 数 + 1 == **5** | `verilog_tree_extract`（`scripts/lib/verilog_tree_extract.py`） | 本方案 |
| **J-3** | **lint 干净**：`verilator --lint-only --top-module top` **exit 0** · 零 `%Error` · **禁** `-Wall` / `-Wno-fatal` | 照 `scripts/verify_hierarchical_rtl.sh` 的 R1 口径 | 本方案 |
| **J-4** | **双形态逐拍等价**：扁平 ≡ 层次化，**逐拍全等** | **`ch::hier_eq::emit` + `compare`**（公共 API · ports **由产物派生**） | 本方案 |
| **J-5** | **独立 TB 可跑**：`RESULT: checks=<N> fails=0` · 失败**非零退出** | `tb_axi_soc_example.sv` | 本方案 |

⚠️ **J-4 的关键**：`hier_eq::compare` 的 `ports` **MUST 由 `emit` 派生**（`include/hierarchy_equivalence.h:155` 的注释：
「★ 禁手写 ⇒ ⇒ 否则与 emitter 真相脱钩」）⇒ ⇒ **SoC 的双形态判据不需要新写任何比对代码** ⇒ ⇒
这是本方案最省力的一点。

### 2.5 期望源层 —— SoC 的「规格」载体

D4 要求期望值**手推** ⇒ ⇒ SoC 的期望源 MUST 是一份**仓内可审计**的文档（TB 里的注释不是权威载体）：

```
.rddf/checks/soc-rtl-expectations.md
├── 每个 SoC 一节：顶层端口映射表（TB 连线的唯一权威）
├── 地址映射表（AXI 从设备 0 = GPIO / 1 = UART 的窗口）
├── 事务序列（写什么 / 读什么 / 期望什么）
└── 手推结果（期望值 MUST 手推 · 禁抄仿真输出）
```

⚠️ **与 IP 级的差别**：IP 级的期望源是「一个端口的行为」；**SoC 级的是「地址映射 + 事务序列」**
⇒ ⇒ 它天然是**表驱动**的 ⇒ ⇒ 未来可机械生成 TB 的激励段（但断言仍手写）。

---

## 3. 可枚举改动清单（逐文件）

> ⚠️ 这是**方案**的文件清单，不是 task 清单（task 由 planner 在 openspec change 里落）。

| # | 文件 | 改动 | 归属 |
|---|---|---|---|
| 1 | `examples/axi4/axi_soc_example.cpp` | ① 4 个 `ch_module` → `ch_layer`（**档 ③**：给模块类型名）② `matrix.m1_*` **11 个 tie-off**（仿 `phase3a_complete.cpp:259-263`）③ `matrix.s1_* ↔ uart` **AXI 通道接通**（19 根双向）⇒ ⇒ **这是 D2 的可发射性前置** | 本方案 |
| 2 | `examples/axi4/emit_rtl_soc.cpp`（**新**） | SoC 的发射入口（`#define main` + include + `run_emit_cli`） | 本方案 |
| 3 | `examples/axi4/CMakeLists.txt` | +1 target `emit_rtl_axi_soc` | 本方案 |
| 4 | `examples/axi4/tb_axi_soc_example.sv`（**新**） | SoC 层独立 TB（**14 个顶层端口**（框架 2 + 根自有 12）+ ≥4 场景） | 本方案 |
| 5 | `.rddf/checks/soc-rtl-expectations.md`（**新**） | SoC 的期望源（端口映射 + 地址映射 + 事务序列 + 手推结果） | 本方案 |
| 6 | `.rddf/checks/examples-rtl-sim-targets.tsv` | +1 行 `kind=soc`（**新形态** ⇒ ⇒ 护栏要认这个 kind） | 本方案 |
| 7 | `scripts/verify_examples_rtl_sim.sh` | 认 `kind=soc` + 新增「SoC 专属判据」（J-2 module 数 / J-3 lint / J-4 双形态等价） | 本方案 |
| 8 | `tests/test_soc_rtl_product.cpp`（**新**） | 产物级断言（module 数 / 端口面 / 顶端口类型非打包） | 本方案 |
| 9 | `.rddf/checks/examples-rtl-sim-exclusions.md:36` | **事实订正**：「AXI 矩阵 + GPIO + UART + **Timer**」⇒ ⇒ 实为 **4** 个 IP、**无 Timer** | 本方案（顺手） |
| 10 | `docs/usage_guide/05-component-design.md` | 补「SoC 装配」小节（`ch_layer` + 顶层扁平端口的组合方式） | 本方案 |

⚠️ **与 ADR-072（D1）的写点交集 = 0** ⇒ ⇒ **可并行**：
本方案写 `examples/**` + `.rddf/checks/**` + `scripts/**` + `tests/**`；
D1 写 `src/**` + `include/**` + 10 份既有 TB + 两份 capability。

---

## 4. 已知残留与边界（如实登记）

| # | 残留 | 性质 | 处置 |
|---|---|---|---|
| ① | **T-3（对外 AXI 主口）无 BFM** | **能力缺口** | **独立立项**（G-5）⇒ ⇒ 本方案只做 T-1 / T-2 |
| ② | **SoC 的模块类型名非契约**（ADR-070 §6 ⑦ 的用户裁决）⇒ ⇒ 改名 ⇒ J-2 的期望 module 数要重算 | **已知代价** | 登记为 SoC 产物的「改名后必须重算结构判据」 |
| ③ | **`kind=soc` 是 tsv 的新形态** ⇒ ⇒ 护栏 ⓪（登记数守恒）与「status 参与判定」需扩展 | **工具面变更** | 与 T-6 同批 |
| ④ | **SoC 的双形态等价（J-4）只能覆盖顶层可见行为** | **观测盲区**（ADR-070 风险③ 同型） | **已接受**：与 `rtl-hierarchical-output` capability 的同一条边界一致 |
| ⑤ | **D1 落地后 SoC 顶层端口表会变**（冒泡端口消失）⇒ ⇒ TB 的端口映射会失效 | **排序约束** | ⇒ ⇒ **TB MUST 按 D1 后的形态写**（框架 2 + 根自有 12 = **14**）⇒ ⇒ 现在跑要先按 D2 消掉 30 个冒泡端口 |
| ⑥ | ⚠️ **原写「SoC 的 `ch_mem` 只能住一个 module ⇒ CPU + 外部 DDR 不支持」= 过强，已删** —— ADR-066 v1.2 §2.7 决策 ⑦ 已把禁令**收窄**为「mem **本体**跨 module」；「仅实参视线跨层」是**已支持**的形态（capability `rtl-hierarchical-output/spec.md:589`）⇒ ⇒ T-3 的真实阻塞只剩「无 BFM」+「顶层 AXI 展平策略未定义」 | **事实订正** | 已改（ADR-073 D9） |
| ⑦ | ⚠️ **`uart.irq` 与根的 `timer_irq` 都是零消费者的残留**（`axi_soc_example.cpp:180` 的 `timer_irq` 全文件零消费；`uart.irq` 无处可接 ⇒ ⇒ 语义不可混用） | **已知残留** | **登记**（ADR-073 D2 表末两行 + V11）⇒ ⇒ 不进 TB 的有效端口面 |

---

## 5. 推进顺序建议

```
第 1 步（架构，已闭）   ADR-073 Accepted  ← 本文件 + ADR
第 2 步（立项）         improvement → openspec change（planner）
第 3 步（实施 · 可与 ADR-072 D1 并行，但 **T0 MUST 先于 ADR-072 D1 的合并**）
   ├─ T0  后代输入驱动源（同一个文件）     ← **可发射性前置**（m1 的 11 根 tie-off + s1↔uart 的 19 根接线 = 30 全覆盖）
   │                                      ⇒ ⇒ 不做 ⇒ ADR-072 D1 落地后 SoC 侧被卡死
   ├─ T1  ch_layer 改造（同一个文件）      ← 最小可验证：看产物 module 数
   ├─ T2  emit target（2 个文件）          ← 产物出来
   ├─ T3  J-2/J-3 结构与 lint 判据         ← 产物合格
   ├─ T4  期望源登记（1 个文件）           ← 规格先行
   ├─ T5  独立 TB（1 个文件）              ← 手推判据
   ├─ T6  tsv `kind=soc` + 护栏扩展        ← 覆盖登记
   └─ T7  J-4 双形态等价（公共 API）       ← 一致性
```

⚠️ **T1 是最小可验证步**：只改 1 个文件 ⇒ ⇒ 立刻能看到产物从 1 个 module 变成 5 个 ⇒ ⇒
**先看产物再写 TB**（⇒ ⇒ 避免按错误的端口面写 TB）。
