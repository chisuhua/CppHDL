---
id: feat-hierarchical-rtl-output
kind: feature
status: active
phase_refs: [phase-5]
主题: 真层次化 RTL 输出（成层粒度由用户显式声明 · 默认 inline · 与扁平逐字节一致）
---

## 概述

「输出**层次化** Verilog」当前是**框架零支持**的能力：唯一的 RTL 出口
`toVerilog(filename, context*)`（`include/codegen_verilog.h:36`）只吃 context，
固定输出**一个** `module top`（`src/codegen_verilog.cpp:301, 409`），
writer 内**零** `children()` 遍历、**零**实例化语句发射。

⇒ ⇒ 现状下想要层次化 RTL，只能「每层手写 wrapper + 手写顶层例化」⇒ ⇒
**手工量随层次深度线性增长**，且每处手工连线都是一次可错的机会。

本 feature 把「层次化 RTL 输出」变成**框架能力**，并为它补上仓内目前**完全没有**的一类
验证 —— **扁平 vs 层次 的逐拍等价性**。

### 四个决策（**本 change 的全部依据**）

| ADR | 决策 | 在本 change 中的位置 |
|---|---|---|
| **ADR-059** | 节点归属与组件命名的修正（「构造作用域」+ context 来源） | **C1 —— 硬前置**（不做 ⇒ emitter 会把子模块状态划进父模块） |
| **ADR-061** | 层级粒度由**用户显式**控制（默认 inline · 成层须声明） | **C2** |
| **ADR-058** | 层次化输出的**语义边界**（= 所有权边界）· **等价性**定义 · **API 边界** | **C3–C5** |
| **ADR-060** | 层次化输出的**端口策略**（隐式端口方向矩阵 · 悬空冒泡）· **module 复用** | **C4** |

### 为什么 C1 必须先做（**不是洁癖**）

实测（证据见 `docs/architecture/2026-11-02-hierarchical-rtl-output.md` 附录 A，**可复算**）：
成员形态的 `ch_reg` / `ch_mem` / bundle 字段归属**父组件**；成员 `ch_module` 挂**祖父**；
深度 ≥3 的实例路径**重复前缀**；成员 `ch_module` 因 `context()==nullptr` **直接早退**。

⇒ ⇒ 按「节点归属」切模块 ⇒ ⇒ **会把子模块的状态划进父模块** ⇒ ⇒
⇒ 产出的 RTL **看起来对、实际错** ⇒ ⇒ ⇒ 且**无任何告警**
⇒ ⇒ ⇒ **先修归属，再写 emitter**（ADR-058 §2.5 把这条定为硬前置）。

⚠️ **归属不是纯命名属性**：`src/simulator_init.cpp:95` 用 `!node->get_parent()`
把节点分到 `input_instr_list_` / `combinational_instr_list_`；
`include/core/io.h` 的 4 处方向 `CHREQUIRE` 也用 `get_parent()`
⇒ ⇒ ⇒ **归属真源变更 ⇒ 仿真分流可能变** ⇒ ⇒ ⇒ **「仿真逐位不变」是待验证项，
不是可假定的结论**（ADR-059 D7）。

### 下游需求（ChipForge ⇒ 本 change）

| 下游需求 | 落点 |
|---|---|
| **N1** 层 2 IP 单元 MUST 成为独立 module（可单独复用 / 交付 / 综合） | C2 + C4 |
| **N2** 装配点**无 active parent** ⇒ 成层声明与装配点 MUST 解耦 | C1（组件树正确性）+ C2 |
| **N3** Component 共用 ctx | C1（单 context 不变） |
| **N4 / N7** 顶层端口名不得与内部节点同名 · 下游按 ctx 级名解析观测点 | C1（命名）+ C4（沿用全局命名表） |
| **N5** 节点名唯一性 opt-in | ⚠️ **不在本 change** ⇒ 已在 `component-framework-hardening-2026-10`（U-1） |
| **N6** 公共头 ODR 安全（`module.h:13` 缺 `inline`） | ⚠️ **前置依赖** ⇒ 同上 change（U-4） |
| **N8** 层 2 单元接口 = **结构化端口**（`Payload<T>` 不可作接口） | ✅ **已裁决**（ADR-062）⇒ 落 ①c（成层声明）+ ④（端口完整性判据） |

---

## 实施路径

| 阶段 | 内容 | 前置 | 产出 | 验收 | 预估 |
|---|---|---|---|---|---|
| **P0** | **归属与命名修正**（ADR-059 全量）：作用域栈 · `node_owner()` · `ch_module` parent · **context 来源** · `build_hierarchical_name` · `local_name_` / `module_type_name()` | 无 | 归属正确 + 命名不重复 | ADR-059 §5 全部判据 | ~2 天 |
| **P0-0** | ✅ **spike（阻塞项 · 已完成 2026-11-02）**：`verilator --json-only` 能否提 `(parent_module, inst_name, module_name)` ⇒ **能**（`modulesp` 是列表 + CELL 的 `modp` 查表）⇒ 已固化 `scripts/lib/verilog_tree_extract.py` + `tests/fixtures/hierarchical_tree_extract/` + `scripts/verify_hierarchical_tree_extract.sh`（含负向对照）+ CI 挂载 | 无 | 可机械比对的 module 树提取器 | 3 层**手写** SV 上提取成功 ✅ | ~0.5 天（已耗） |
| **P0'** | **层级粒度声明机制**（ADR-061）：`CH_HIERARCHY_LAYER` 类级宏 · `ch_layer<T>` / `ch_inline<T>` 两个新 API 名 · `Component::is_layer()` · **无 active parent 处的装配点**（下游 N2） | P0 | 粒度可声明 · 装配点不受限 | ADR-061 §5 全部判据 | ~1.5 天 |
| **P1** | **分析层**：`ModuleDef` / `InstanceDef` / 端口表 / 隐式端口识别 / 冒泡识别；产出**可逐字节比对的 golden 文本** | P0' | 模块清单 + 端口表 + 连接表（golden） | 5 个 DUT 与 `tests/golden/` 快照**逐字节比对零差异**（人工只**生成**快照，不**判定**） | ~2 天 |
| **P2** | **emitter 拆分**：把 `verilogwriter` 拆成「接收节点子集 + 端口表 + 名字表」的 `ModuleEmitter` | P0 | 扁平产物**逐字节不变** | 相对 **P0 之后**的基线逐字节不变 + `ctest -LE perf` 全绿 | ~3 天 |
| **P3** | **层次发射**：module 主体 + 实例化 + 隐式端口（**按方向**）· 悬空冒泡 + clock/reset 贯通 + module 复用 | P1 · P2 | 完整层次化 RTL | ADR-058 §5 结构判据 + ADR-060 §5 全部判据 | ~3 天 |
| **P4** | **等价性验证**：≥5 个 DUT 的「扁平 vs 层次」逐拍比对 + **映射表（含 `node_id` / `hier_top_port_name`）** + lint + 确定性 | P3 | 等价性测试套件 + backend 的**只增不改**入口 | ADR-058 §5 效果判据全部通过 | **~7–9 天**（oracle R13：不止加一个函数） |
| **P5** | **文档与 API**：`docs/usage_guide/05-component-design.md` §5 改写 + API 参考 + 架构主题文档转正 | P4 | 文档同步 | 文档与实现一致（零漂移） | ~1 天 |

> **依赖关系**：① **P1 与 P2 都只依赖 P0** ⇒ ⇒ ⇒ **P1 ∥ P2 可并行**；
> ② **P3 依赖 P1 + P2**；**P4 依赖 P3**；**P5 依赖 P4**。

> ⚠️ **P2 不是「纯搬移」**：现有 `print_reg` / `emit_always_ff` **硬编码**
> `default_clock` / `default_reset`（`src/codegen_verilog.cpp:278-283, 755-760`）
> ⇒ ⇒ 层次化子模块的 `always_ff` 会引用**本模块内不存在**的信号名
> ⇒ ⇒ ⇒ **时钟名 MUST 参数化** ⇒ ⇒ ⇒ P2 的范围与归因必须写清。

> ⚠️ **P4 的工作量**：仓内**没有**任何「两份 Verilog 逐拍对比」的现成设施
> （`DUAL_BACKEND_TEST_PAIR` 比的是「C++ 模拟器 vs 扁平 Verilog」，**不是** Verilog A vs B）
> ⇒ ⇒ 且 Verilator backend **写死**单文件 `top.v` + `module top`
> ⇒ ⇒ ⇒ **已裁决（2026-11-02 · 用户拍板）**：给 backend 加一个**只增不改**的
> `initialize_with_verilog(path, data_map)`（既有 `initialize(ctx,…)` 行为**逐字节不变**）
> ⇒ ⇒ ⇒ ⇒ P4 的「多文件 harness」退化为「backend 加一个只增入口」
> ⇒ ⇒ ⇒ ⚠️ **但那个入口远不止加一个函数**（oracle R13 实测）：`build_port_access_table`
> （`verilator_backend.cpp:594-660`）按 **node id** 取 `data_map_`、按 **Vtop 字段名**取端口；
> `emit_sim_main_postlude`（`:193-231`）从 ctx 全量生成；`invoke_verilator`（`:490-497`）
> 编译**单文件**且缓存 key 只哈希一份 source ⇒ ⇒ ⇒ **重估 ~7–9 天**。

---

## 候选 change 分解（⇒ ⇒ 本 change 的 commit 序列）

| # | 候选 change | 范围 | 前置 | capability delta | 护栏交付物 | 验收 |
|---|---|---|---|---|---|---|
| ①a | `hierarchical-rtl-naming-fix-2026-11` | **纯命名**：`build_hierarchical_name` 改用 `name()` · `local_name_` / `module_type_name()` · `hierarchical_name()` 一并修正 | 无 | `component-ownership-and-naming`（**ADDED ×1**） | 命名回归断言（深度≥3 无重复前缀） | 零语义变更 · 独立可验收 |
| ①b | `hierarchical-rtl-ownership-fix-2026-11` | **归属**：`node_owner()` + 作用域栈（配对）· `ch_module` parent · **context 取环境值** | ①a | 同 capability（**MODIFIED ×1**） | 归属断言（`CHFATAL_EXCEPTION`）· 作用域配平断言 · **归属↔语义分流回归** | ADR-059 §5 全部判据 |
| ①c | `hierarchical-rtl-granularity-declaration-2026-11` | **粒度声明**（ADR-061）：`CH_HIERARCHY_LAYER` · `ch_layer<T>` / `ch_inline<T>`（**不动 `ch_module` 签名**）· `is_layer()` · **无 active parent 处的装配点** | ①b | `rtl-hierarchical-output`（**ADDED ×1**） | 粒度判据（类级 × 实例级三组合）· 「无 parent 处装配仍能成层」用例 | ADR-061 §5 全部判据 |
| ② | `hierarchical-rtl-analysis-2026-11` | **分析层** + **golden 文本**产出 | ①c | `rtl-hierarchical-output`（**ADDED ×1**） | golden 快照比对（5 个 DUT） | 与 `tests/golden/` 逐字节零差异 |
| ③ | `hierarchical-rtl-emitter-split-2026-11` | **emitter 拆分**（**含时钟名参数化**） | ①b | 同 capability（**MODIFIED ×1**） | 「扁平产物逐字节不变」基线脚本 | 相对 P0 之后的基线逐字节不变 |
| ④ | `hierarchical-rtl-emission-2026-11` | **层次发射**（module / 实例化 / 隐式端口按方向 / 悬空冒泡 / clock-reset 贯通 / module 复用 / **跨模块 `ch_mem` 响亮拒绝**） | ② ③ | 同 capability（**MODIFIED ×1**） | **新增** `scripts/verify_hierarchical_rtl.sh`（零未声明引用 · 实例端口名 ∈ 目标 module 端口集 · 产物确定性 · 跨模块 `ch_mem` 被拒） | ADR-058 §5 结构判据 + ADR-060 §5 全部判据 |
| ⑤ | `hierarchical-rtl-equivalence-2026-11` | **backend 只增不改入口** + ≥5 DUT 的映射表与逐拍等价 | ④ | 同 capability（**MODIFIED ×1**） | 等价性 harness 的**判别力自证**（故意破坏 ⇒ 必须转红） | ADR-058 §5 效果判据全通过 |
| ⑥ | `hierarchical-rtl-docs-2026-11` | **文档同步** | ⑤ | — | — | 文档零漂移 |

⚠️ **①a / ①b / ①c MUST 拆开**：①a 是**纯命名**（零语义变更）；①b 动**归属真源**
（耦合 simulator 分流与 io 方向守卫）；①c 动**装配面**（新增 API + 装配点形态）
⇒ ⇒ ⇒ 三者的回归判据完全不同 ⇒ ⇒ ⇒ 捆在一起 ⇒ 「回归变红」无法归因。

---

## 跨阶段拆分

### phase-5
- ⏳ ①a `hierarchical-rtl-naming-fix-2026-11`（**纯命名** · ADR-059 的命名子项 · 零语义变更）
- ⏳ ①b `hierarchical-rtl-ownership-fix-2026-11`（**归属真源** · ADR-059 的核心 · **硬前置**）
- ⏳ ①c `hierarchical-rtl-granularity-declaration-2026-11`（**层级粒度声明** · ADR-061 · 默认 inline）
- ⏳ ② `hierarchical-rtl-analysis-2026-11`（分析层 + golden 文本）
- ⏳ ③ `hierarchical-rtl-emitter-split-2026-11`（emitter 拆分 + 时钟名参数化）
- ⏳ ④ `hierarchical-rtl-emission-2026-11`（层次发射 · 隐式端口 · 冒泡 · 复用）
- ⏳ ⑤ `hierarchical-rtl-equivalence-2026-11`（backend 只增不改入口 + 等价性）
- ⏳ ⑥ `hierarchical-rtl-docs-2026-11`（文档同步）

> ⚠️ **本 feature 不阻塞其它 feature**：它是**新增能力**，不改既有 RTL 产物语义
> ⇒ ⇒ 现有 dual-backend / RV32I / AXI 线路**零依赖**。
> ⚠️ **①a/①b 会改既有扁平产物的信号名** ⇒ ⇒ ⇒ 属**全仓可见**的文本变更
> ⇒ ⇒ ⇒ MUST 在各自的 commit 里登记「文本变化面」（**至少** 4 处实测依赖：
> `tests/test_axi_spi.cpp:213-214` · `tests/benchmark/verilator_harness_tc07.cpp:33` ·
> `tc08.cpp:51` · `tests/test_verilator_backend.cpp` 的 `module top` / 端口名断言）
> ⇒ ⇒ ⇒ **MUST NOT** 用「仓内零处断言具体名字」这种未复算的结论豁免。

---

## 用户视角审查新增的两个产品缺口（⚠️ **待用户确认**）

> 来源：`docs/developer_guide/patterns/HIERARCHICAL-RTL-AUTHORING-GUIDE.md` §3
> ⇒ ⇒ ⇒ 方案的**语义**自洽可实现，但**用户面**有两条真空缺：

| # | 缺口 | 症状 | 建议 | 归属 |
|---|---|---|---|---|
| **P6** | **用户看不到自己的设计是否「层次友好」** | 用户无法预判会不会冒出一堆 `__undriven_*` / `__if_*` 端口 | ⭐ 新增「**层次友好度诊断** API」（形态照搬仓内已有的 `ch::core::diagnose_drive`：**单枚举 + 单一判定实现 + 护栏**）⇒ 报告：隐式端口数 / 悬空输入数 / 未消费子输出数 / 跨模块 `mem` 访问数 / **各层规模** | 并入 ④，或**独立 change** |
| **P8** | **用户拿不到「我的设计层次化后行为不变」的反馈** | 等价性验证只是框架自测，用户自己的 DUT 没有反馈 | ⭐ 把等价性 harness 做成**公共 helper** ⇒ 用户可在自己的 `TEST_CASE` 里逐拍比对扁平 vs 层次 | 并入 ⑤ |

⚠️ 这两条**建议在 C3/C4 落地前**补进路径（成本低、收益直接）⇒ ⇒ ⇒ **待用户拍板**。

---

## 反例

- **不要**为了"层次化"去改 `toVerilog()` 的签名或默认行为 ⇒ ⇒
  既有调用点 + Verilator backend 全部承担新分支风险 ⇒ ⇒ **新入口**（ADR-058 §2.3）。
- **不要**在 ①b 落地前开始写 emitter ⇒ ⇒ 归属错 ⇒ 产出**看起来对、实际错**的 RTL。
- **不要**把「P2 = 纯搬移」当既定事实 ⇒ ⇒ `default_clock` / `default_reset`
  **硬编码**在 `print_reg` / `emit_always_ff` 里 ⇒ ⇒ 子模块的 `always_ff` 会引用
  **不存在**的信号 ⇒ ⇒ ⇒ 时钟名**必须**参数化。
- **不要**默认把每个组件都成层 ⇒ ⇒ 小组合子全部成层 ⇒ 产物是**例化清单的噪声**
  ⇒ ⇒ ⇒ **默认 inline，成层须显式声明**（ADR-061）。
- **不要**用「按端口数 / 节点数自动推断」成层 ⇒ ⇒ **不可预测** ⇒ ⇒ 用户无法预测产物
  ⇒ ⇒ ⇒ 平台替用户猜 = 本仓反复吃过的亏。
- **不要**用 per-module 局部命名 ⇒ ⇒ 下游按 **ctx 级名**解析观测点 ⇒ ⇒ 会**打断**它
  ⇒ ⇒ ⇒ 命名权威 MUST 是 ctx 级全局表。
- **不要**用「按信号名前缀后处理切分扁平文件」当过渡方案 ⇒ ⇒
  成员节点名字挂父前缀 + 深度≥3 前缀重复 ⇒ ⇒ **切分边界本来就错**。
- **不要**让隐式端口**静默**通过 ⇒ ⇒ 默认 MUST 有 `CHWARN` + 生成文件内注释
  ⇒ ⇒ 否则「非法跨层访问」被**合法化**且无声（ADR-060 §2.1）。
- **不要**只写「子侧 input」这一种方向 ⇒ ⇒ 跨层边有**四种方向形态**
  ⇒ ⇒ ⇒ 方向矩阵见 ADR-060 §2.1 ⇒ ⇒ 照抄「只有 input」会产出**方向相反**的端口表。
- **不要**把等价性判据写成「顶层端口数相同」或「全部顶层输出全等」⇒ ⇒
  层次化**有意**不把无人消费的子输出抬成顶层 output ⇒ ⇒ 等价性 MUST 以
  **显式观测集合 + 映射表**为准（ADR-058 §2.2）。
- **不要**把 ①②③ 合进同一个 commit ⇒ ⇒ 「扁平产物逐字节不变」将**无法归因**。

---

## 验收标准

- [ ] **C1（①a）**：深度 ≥3 的实例路径**无重复前缀**；`local_name_` /
      `module_type_name()` 就位；**零语义变更**
- [ ] **C1（①b）**：Leaf 的 `ch_reg` 成员 `get_parent()` 是 Leaf；成员形态 `ch_module`
      挂到直接父组件**且可 `io()`**（不早退）；顶层 `ch_device<Pipeline>`
      **可实例化且不 SIGSEGV**（⇒ `examples/removed_examples.txt:26` 归位条件解除）；
      作用域**配平**（`create_child` 不 build 也不污染）；归属断言 MUST 用
      `CHFATAL_EXCEPTION`（**MUST NOT** 用只打日志的 `CHREQUIRE`）
- [ ] **C1（①b）**：**归属 ↔ 语义分流**回归 —— `type_input` 节点的指令表归属与
      `include/core/io.h` 四处方向守卫的**触发面**与变更前**逐项一致**
- [ ] **C1（①a/①b）**：**文本变化面**逐条登记（≥ 4 处实测依赖，见上）⇒ ⇒
      `ctest -LE perf` / `run_all_samples.sh` / `run_all_ported_tests.sh` 全绿 ·
      计数锚点同步
- [ ] **C2（①c）**：类级 / 实例级三种组合的成层结果 MUST 与声明一致；
      **无 active parent** 处装配的组件 MUST 仍能按声明成层；`ch_module` 签名**零变更**
- [ ] **C3（③）**：`toVerilog()` 的扁平产物相对 **P0 之后**的基线**逐字节不变**
- [ ] **C4（④）**：**成层且非根** module 的端口集合 **⊇** 其 `ch_in` ∪ `ch_out`（不论是否被父层驱动）
      ⇒ 且每个端口可追溯到**五类**来源（自身 / inline 后代 `ch_out`（仅根）/ 隐式 / 冒泡 / clock·reset）
      ⇒ **根 module** MUST 走扁平规则（⇒ 与「全 inline 逐字节一致」互斥，不得写成 `==`）
      ⇒ 已被父层驱动的 `ch_in` MUST 是端口 + 父层用**实例端口映射**连接
      ⇒ 判别性对照：把某单元**输入**改成「只从 host 侧键值通道读」⇒
      「体内 MUST NOT 引用非自己/非祖先的端口节点」这条 MUST 转红
      （⚠️ **不用**「端口数变 0」—— 输出方向必须先有 `ch_out` 作源 ⇒ 该判据无判别力）
- [ ] **C4（④）**：三层设计产出 **3 个 module + 2 条实例化语句**；
      **无任何** `assign` 跨 module 边界；**结构同构**成立（module 树 ≡ 成层组件子树）；
      每个实例化端口名 ∈ 目标 module 端口集合；四类隐式端口方向**各自**有判据
- [ ] **C0（spike）**：`verilator --json-only`（或 `--dump-tree`）能在**手写** 3 层 SV 上
      提 `(parent_module, inst_name, module_name)` ⇒ ⇒ 提不出 ⇒ **停，回 ADR 重设计判据**
- [ ] **C1**：成员 `ch_module` 的 `parent().lock()` **非空**；`subtree` 判定**只向下走 `children()`**；
      `context::init()` 的 clock/reset `owner == nullptr`；`build(ctx)` 的 ctx == 构造期 env ctx
- [ ] **C4（④）**：**默认（全 inline）** 的层次化产物与扁平产物**逐字节一致**
      （允许的差异 MUST 逐条列明）；悬空子输入在两份产物中**都**是顶层可驱动输入；
      inline 组件的**输出**（含已被父层消费的）在两份产物中**都**是顶层 `output`；
      **端口表收缩只发生在成层子树**；
      同类型多实例**各发一份** module（名 `<type>_<序号>`）；同名不同类型**响亮拒绝**
- [ ] **C5（⑤）**：≥5 个 DUT 的「扁平 vs 层次」**逐拍等价**（映射表声明的观测集合，
      **非空**且**至少一个随激励变化**）；**故意破坏层次化 ⇒ 必须转红**（判别力自证）；
      所有产物 `verilator --lint-only -Wall` 零 error；同一设计两次生成**字节相同**
- [ ] **C6（⑥）**：`docs/usage_guide/05-component-design.md` §5 与
      `docs/architecture/2026-11-02-hierarchical-rtl-output.md` 与实现一致（零漂移）
- [ ] **全程**：capability delta 齐备（归档**禁** `--skip-specs`）·
      `openspec validate --strict` 通过 · `ctest -LE perf` /
      `run_all_samples.sh` / `run_all_ported_tests.sh` 全绿 ·
      `scripts/verify_test_counts.sh` 三锚点同步 ·
      `scripts/verify_dual_backend_parity.sh` 通过 ·
      新增护栏 `scripts/verify_hierarchical_rtl.sh` 挂 CI 两处
