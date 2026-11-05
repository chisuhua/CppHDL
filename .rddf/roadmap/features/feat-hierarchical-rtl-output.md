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

### 五个决策（**本 feature 的全部依据** · 均 **Accepted 2026-11-04**）

| ADR | 决策 | 在路径图中的位置 |
|---|---|---|
| **ADR-059** | 节点归属与组件命名的修正（「构造作用域」+ context 来源 + **模块类型名四档来源**） | **T3-①a（命名）/ T3-①b（归属）—— 硬前置** |
| **ADR-061** | 层级粒度由**用户显式**控制（默认 inline · 成层须声明 · 合并宏 `CH_HIERARCHY_LAYER("name")`） | **T3-①a（宏）/ T3-①c（粒度解析）** |
| **ADR-058** | 层次化输出的**语义边界**（= 所有权边界）· **等价性**定义 · **API 边界** | **T3-②–T3-⑤** |
| **ADR-060** | 层次化输出的**端口策略**（隐式端口方向矩阵 + 第三轴 · 悬空冒泡）· **module 复用（MUST NOT）** | **T3-④** |
| **ADR-062** | 层次化单元的**接口类型**（结构化端口 · `Payload` 不可作接口 · **接口白名单**） | **T3-②（白名单判据）/ T3-④（发射时拒绝）** |

> ⚠️ **ADR 状态已转正**（2026-11-04 · 用户拍板）：058 v2.8 · 059 v2.3.1 · 060 v1.6 · 061 v1.6.1 · 062 v1.4。
> ⚠️ **架构评审（2026-11-04）的完整结论**见
> `docs/architecture/2026-11-02-hierarchical-rtl-output.md` **§10** —— 本文件只承载**排期与验收**。

### 为什么归属修正（**T3-①b**）必须先做（**不是洁癖**）

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

### 下游需求（ChipForge ⇒ 本 feature）

| 下游需求 | 落点 |
|---|---|
| **N1** 层 2 IP 单元 MUST 成为独立 module（可单独复用 / 交付 / 综合） | **T3-①c + T3-④** |
| **N2** 装配点**无 active parent** ⇒ 成层声明与装配点 MUST 解耦 | **T3-①b（组件树正确性）+ T3-①c** |
| **N3** Component 共用 ctx | **T3-①b**（单 context 不变） |
| **N4 / N7** 顶层端口名不得与内部节点同名 · 下游按 ctx 级名解析观测点 | **T3-①a**（命名）+ **T3-④**（沿用全局命名表） |
| **N5** 节点名唯一性 opt-in | ⚠️ **不在本 feature 主线** ⇒ **T2b**（U-1）· 与主线**无依赖**，可并行 |
| **N6** 公共头 ODR 安全（`module.h:13` 缺 `inline`） | ⚠️ **T2a —— 唯一的硬前置**（1 词 + 1 fixture） |
| **N8** 层 2 单元接口 = **结构化端口**（`Payload<T>` 不可作接口） | ✅ **已裁决**（ADR-062）⇒ 落 **T3-①c**（成层声明）+ **T3-④**（接口白名单 + 端口完整性） |

---

## 实施路径（**10 个 change** · 权威 = 架构文档 §10.4）

> ⚠️ **一个 change = 一组回归判据**：判据不同的裁决 MUST 分开 ⇒ ⇒ 否则「回归变红」**无法归因**
> （沿用本文件既有裁决「①a/①b/①c MUST 拆开」）。前置依赖**显式化**：只有 **T2a** 是硬前置。

### 第 0 步：**拆分现有单人 change** —— ✅ **已完成（2026-11-04）**

> 原先是**一个** change —— `openspec/changes/hierarchical-rtl-output-2026-11/`
> （proposal / design / tasks + 2 份 delta）⇒ ⇒ 已按本表拆成 **9 个 active change**
> （+ **T2a** 已单独实施并归档）⇒ ⇒ 下面 10 行**不再是纸面**。

| # | 动作 | 落点 |
|---|---|---|
| 0-1 | 建 change 目录 | `openspec/changes/<name>/` —— ✅ **9 个 active**（T2a 已归档） |
| 0-2 | **切开两份 delta** ⇒ 每条 Requirement 只落到**一个** change | ✅ **ownership 8/8 · hierarchical 14/14**（零重复 · 零遗漏） |
| 0-3 | 执行架构文档 **§10.6（25 项）/ §10.7（① ②）** 的 change 文档修订 | ✅ §10.6-A 的 5 处口径已随 design.md 分派到对应 change · §10.6-B 的 9 条判据已进各 change 的 Acceptance |
| 0-4 | `T3-③` / `T3-⑥` 显式 `skip_specs: true`（纯重构 / 纯文档） | ✅ 两者 `openspec validate` 均 valid |
| 0-5 | **退役**两个被拆空的 legacy change | ✅ `hierarchical-rtl-output-2026-11`（内容全部分派到 8 个新 change）· `component-framework-hardening-2026-10`（U-4 → T2a 已归档 · U-1/U-2 → T2b，实测证据搬进 T2b 的附录） |

### 实施进度（**每段一个 change，逐个验收/归档**）

| 段 | change | 状态 | 关键证据 |
|---|---|---|---|
| **T2a** | `header-odr-inline-fix-2026-11` | ✅ **已归档**（2026-11-04） | 链接期判据 `scripts/verify_header_odr.sh`（含 `--self-test`）· capability `core-header-odr-safety` |
| **T3-①a** | `hierarchical-rtl-naming-fix-2026-11` | ✅ **已归档**（2026-11-04） | `tests/test_component_naming.cpp` + 3 个编译期 fixture + `scripts/verify_component_naming.sh`（R0–R4 + `--self-test`）· 深度 ≥3 = `top.m.lf` |
| **T3-①b** | `hierarchical-rtl-ownership-fix-2026-11` | ✅ **已归档**（2026-11-04） | `tests/test_component_ownership.cpp`（8 用例 / 76 断言）· **`ch_device<Rv32iPipeline>` 不 SIGSEGV** · 变异 6/6 全抓 · 3 处实施期订正见 `design.md` §4 |
| **T3-①c** | `hierarchical-rtl-granularity-declaration-2026-11` | ✅ **已归档**（2026-11-04） | `tests/test_component_granularity.cpp`（5 用例 + 7 条 `static_assert`）· 四档来源 + `Granularity` 总开关 + 无 parent 装配 |
| **T3-②** | `hierarchical-rtl-analysis-2026-11` | ✅ **已归档**（2026-11-04） | `tests/test_hierarchy_analysis.cpp`（11 用例 / 87 断言 + **4 份 golden**，其中 `three_layers.txt` 为**手推**）· 变异 **12/12** · 隐式端口合成**显式分派给 T3-④**（本层 fail-closed） |
| **T3-③** | `hierarchical-rtl-emitter-split-2026-11` | ✅ **已归档**（2026-11-04） | `ModuleEmitter` + `ModuleView` + 薄适配器 `verilogwriter` · 3 个设计 `cmp` 逐字节相同（含 145 KB RV32iPipeline）· 时钟名参数化 4 处 · **M1 ⇒ lint exit 1 / 对照 exit 0** |
| **T3-④** | `hierarchical-rtl-emission-2026-11` | ✅ **已归档**（2026-11-05 · **13/13 批**：B1 · B2a · B2b-1..6 · T0h · B3a · B3b · B3c · B4 ⇒ 多 module + 实例化 + 顶层端口=扁平 + clock/reset 分开判 + 隐式端口行 1/行 2 + F3 + 第三轴 + 兄弟边 + 宽度守卫 + 冒泡（G3）/`UndrivenPolicy` + 向上 pass-through + 产物级护栏 R1–R7 + 探针矩阵 36 格） | `toVerilogHierarchical` · `tests/test_hierarchy_verilog.cpp`（**28 用例 / 495 断言**）· `scripts/verify_hierarchical_rtl.sh`（R1–R7 + `--self-test` 9/9）· 全 inline ⇒ 与扁平逐字节相同 · 全部层次化产物 `verilator --lint-only` exit 0 · 扁平外部基线 3 设计 `cmp` 全 0 |
| T3-⑤ | `hierarchical-rtl-equivalence-2026-11` | ⏳ | — |
| T3-⑥ | `hierarchical-rtl-docs-2026-11` | ⏳ | — |
| T2b | `component-io-const-and-node-name-policy-2026-11` | ⏳（与主线并行，无依赖） | — |

### T3-④ 的批次拆分（2026-11-05 · 据架构裁决重排 · **13 个批次 · 逐个 commit**）

> ⚠️ **2026-11-05 第二次重排（planner · 据 ADR-060 v1.13 / ADR-058 v2.12 + metis gap 分析）**：
> 原 B3 拆成 **B3a / B3b / B3c**（三个机制、三组回归判据），并新增 **B2b-6**（宽度守卫 + 两条变异欠账）。
> **原 B3c「拒绝类」被取消** —— 它的四项里三项**已落地**（跨模块 `ch_mem` 0.7 / 自驱动 `ch_in` 前置校验③ /
> 同名不同类型 T0h），按「实现」排期会二次实现 ⇒ 只保留**点名断言测试**并归入 B3b。
> **批次总数 10 → 13**；顺序不可换（B3b 的完整性校验依赖 B3a 的端口登记）。

> ⚠️ **一个批次 = 一个 commit** ⇒ ⇒ 「回归变红」**可归因**（沿用本 feature「①a/①b/①c MUST 拆开」的裁决）。
> ⚠️ **B2b 已拆 5 批**（2026-11-05 · ADR-060 v1.9 定死形态边界后重排）：B2b-1 行 2 ✅ ·
> B2b-2 行 1 寄存器 ✅ · B2b-3 proxy + 去重 + 探针骨架 ✅ · B2b-4 F3 ✅ · T0h 命名序号 ✅ ·
> B2b-5 第三轴 + 兄弟边 ✅
> ⇒ ⇒ **顺序不可换**（B3 的端口完整性与冒泡**依赖 F3**）。
> ⚠️ **T0h 是 B2b-5 之前插入的独立小批**（2026-11-05 · ADR-060 **v1.12** §2.3 的 **G1**）：
> 同类型多实例的序号改按 `HierarchyModule.key` 的**字节字典序** ⇒ ⇒ **零行为变化**（现状
> 无同类型多实例用例 ⇒ 对现有产物**逐字节无影响**）⇒ ⇒ MUST 在 **B3 加「同类型多实例」判据
> 之前**落地（否则要动既有判据）。
> ⚠️ **排期顺序由 oracle 实测决定**（台账 §11.5 **O3**）：**先 collect 化、再做 F2/F3**
> ⇒ ⇒ 否则 `SynthesizeFatal` 的诊断收集与 F2/F3 改**同一个发射循环** ⇒ ⇒ 二次返工。

| 批 | 内容 | 依赖 | 关键判据 |
|---|---|---|---|
| **B1** | **策略枚举收敛**（零行为变化）：删 `HierarchyPortPolicy` / `HierarchyUndrivenPolicy` ⇒ 用 `ch::PortPolicy` / `ch::UndrivenPolicy`（ADR-060 §2.4 的**唯一权威**）· **删 `Component::set_granularity()`**（零调用点 ⇒ 绕过 RAII 的泄漏通道）· `HierarchyEmitOptions` 加 `top_module_name`、**不加** `granularity`（全局权威） ⇒ ⇒ ✅ **C1 已答（2026-11-05 · ADR-058 v2.10）= 接受单文件多 module** ⇒ ⇒ 本批「只加不删」的兜底**未触发**（代码里**从未**实现 `out_dir` / `one_file_per_module` ⇒ ⇒ 无对象）· ⚠️ **返回类型偏离登记**（ADR-058 §2.3 写 `std::expected<HierManifest, HierError>` ⇒ 已落地的是同构 struct `HierarchyEmitResult` ⇒ ⇒ 本批登记为「实现取同构 struct · 语义等价」） | — | 编译期 `static_assert`（枚举逐个相等）+ 既有 4 用例绿 + 扁平外部基线 3 设计 `cmp` 全 0 |
| **B2a** | **发射循环 collect 化**（**零行为重构** ⇒ ⇒ 单独成 commit ⇒ ⇒ 兑现「一个批次 = 一组回归判据 ⇒ 可归因」）：把「合成类」边的诊断从「首错即返」改为 **collect**（`SynthesizeFatal` 的诊断载体）⇒ ⇒ **前置校验类**错误**保持首错即返**（对齐 `Reject` 语义） | B1 | 既有用例全绿 + 3 设计 `cmp` 全 0 + **`SynthesizeFatal` 多越界边 ⇒ 诊断含全部边与端口名**（对照：只报第一条 ⇒ 转红）|
| **B2b-1** ✅ | **行 2（子 → 父）**：子侧 `output __x_<n>` + 体内 `assign` + 父侧实例映射 · **未消费的子 output 以空连接 `.port()` 出现** | B2a | 变异 M1/M2 全抓 · lint（§2.5 门）exit 0 |
| **B2b-2** ✅ | **行 1（父 → 子）寄存器形态**：子侧 `input __if_<n>` + `print_reg` **发射期**驱动重定向（**MUST NOT** 写回 IR） | B2b-1 | 变异 M1/M2 全抓 · 判据含「**不得**出现 `Register update (no next)`」 |
| **B2b-3** ✅ | **proxy 子形态**（ADR-060 v1.9 行 1：**仅** reg/proxy 在矩阵内）+ 合成名去重接缝 + **探针矩阵骨架**（`成员类型 × 操作 × 方向`，每格三态之一） | B2b-2 | ⭐ **值判据**（proxy 漏接时 lint **看不见** —— 默认集不查 `UNDRIVEN`）· op 操作数 ⇒ **响亮拒绝**（第七格）· 父驱动子 `ch_out` ⇒ **响亮拒绝**（第八格） |
| **B2b-4** ✅ | **F3**：inline 后代 `ch_out` 按**作用域**落点（顶层 ⇒ output / 非顶层成层 ⇒ 体内 wire）+ `print_decl` 的 `type_output`-as-wire | B2b-3 | 非顶层 ⇒ `logic` 声明 + `assign` 且**不**进端口表 · 顶层 ⇒ 仍是 output（负向对照） |
| **T0h**（= B2b-4b） | **G1 命名序号**（ADR-060 **v1.12** §2.3 · design §7.1.4）：序号改按 **`HierarchyModule.key` 的字节字典序**（组内；先按 sanitized `module_type_name` 分组）⇒ ⇒ **MUST NOT 重排 post-order 发射序**（只改名字）· **同 base 不同类型 MUST 在编号前判身份**（撞 ⇒ `CHFATAL_EXCEPTION`） | B2b-4 | 两个同类型成层组件 ⇒ `<type>_0` / `_1`；**交换组件创建顺序 MUST NOT 改这两个名字**；变异（退回遍历序）⇒ 转红 · 对现有产物 **`cmp` 全 0**（现状无同类型多实例用例） |
| **B2b-5** ✅ | **第三轴**（driver 的 `owner` 在更上层 ⇒ 沿祖先链**逐级同名** input 端口）+ **兄弟边**（共同祖先内一根 wire + 两处实例映射，**MUST NOT** 兄弟直连） ⇒ ⇒ ⚠️ **MUST 用 G0 的 chain builder**（ADR-060 **v1.12** §2.3 · design §7.1.4）：**一个** chain builder（返回 `(module, port, dir, width, origin, target)` · 幂等 · 撞名/宽度错 fail-closed）+ **两种 leaf adapter**（第三轴叶 = reg/proxy ⇒ **需驱动重定向**）⇒ ⇒ **MUST NOT** 写成「起点节点 + 方向」单函数 | T0h | 第三轴：每级一个同名端口 + 顶层可驱动（值判据）· 兄弟边：恰好一根连接网 + **零**隐式端口 · 变异 3/4 抓（M4 = 等价变异，已登记）· 三个产物的 `verilator --lint-only` exit 0 |
| **B2b-6** | **宽度守卫（W1 的实现追赶）+ B2b-3 的两条欠账**：① 直接行 1 / 行 2 路径**补宽度校验** —— 现状合成端口宽度取子侧、父侧实参宽度**从不比较**，而产物关掉 `WIDTHEXPAND`/`WIDTHTRUNC` ⇒ **静默扩展/截断**；守卫 MUST 抽成**单一 helper**（所有合成 site 同一调用点），MUST NOT 每个分支各写一份；② 0.24c 的负向用例：**双向**（9→8 截断 / 8→9 扩展）× {`ch_reg` next, proxy}；③ 守卫自身变异（去掉守卫 ⇒ 用例转红）；④ 0.24b：`print_input` driver walk 与 `print_proxy` 的 `src(0)` 两条路径**逐条变异**（证明每条 `src(0)` 解析路径独立受约束） ⇒ ⇒ ⚠️ **本批不收**：失败消息措辞（B3a 后才是终态）· `undriven_policy` 过渡说明（ADR-060 v1.13 已权威登记，不在代码里加第二份） | B2b-5 | 四类负向用例（双向 × 两形态）+ 诊断点名两端与两个宽度值 + 建议替代路径（显式切片）+ **不留文件** + 守卫变异转红 + 0.24b 两条变异转红 · `ctest -LE perf` 238/238 · 扁平基线 `cmp` 全 0 |
| **B3a** ✅ | **冒泡（G3）+ `UndrivenPolicy` 语义**：① 成层组件的悬空 `ch_in` ⇒ **分析层**登记 `Bubbled`（逐级同名，G3-1）；② **连接也由分析层生成**（G3-2）—— ⚠️ **同时修存量 inline 路径**：现有 inline 冒泡**只登记端口、不生成逐跳连接** ⇒ 折进**非根**成层 module 时中间层空连接 ⇒ 值中途断线且零报错；③ **有 driver 的向上 pass-through**（`target_is_anc` 现在 fail-closed，消息自称「B3」）⇒ 同批用同一链条实现（登记各层端口 + 逐跳连接），并删该 fail-closed 分支；④ 链条 MUST 是 **同一份实现**（抽内部 helper，第三轴/兄弟边与冒泡两个调用点，G3-6）；⑤ `Bubble` / `Reject` 两格语义（§2.4.2：`Reject` 只对 inline 折叠后 module 归属为**非根成层 module** 的悬空 `ch_in`）；⑥ **顶层 `synthesized_ports` 恒空**（G3-5） ⇒ ⇒ ⚠️ **golden 影响面先测后改**：F2 会新增 `origin=bubbled` 行 ⇒ 先跑 `tests/test_hierarchy_analysis.cpp` 记基线，若有 diff ⇒ **同 commit** 更新 | B2b-6 | ⭐ **值判据**（顶层可驱动 + 中间祖先实连 `.din(din)` 而非 `.din()`；去掉分析层连接 ⇒ 值判据必红 —— 结构/lint 均看不见）· **顶层端口顺序**（bubbled input MUST 排在 `ch_out` 之前；把冒泡实现成发射期顶层合成 ⇒ 转红）· 每输入**恰好一条** `CHWARN` + 端口旁注释 · `Reject` 列**全部**（**双实例**用例）+ **防误拒负向对照**（根自己的 / inline 折叠进根的悬空 `ch_in` ⇒ `Reject` 下仍产出）· 无悬空输入时 `Reject` 仍成功 ⇒ ⇒ ✅ **落地证据**：`[bubble]`×3 + `[upward]`×1（**23 用例 / 232 断言**）· 变异 **5/5** · lint 29/29 exit 0 · 扁平基线 `cmp` 全 0 ⇒ ⇒ ⚠️ **诚实边界**：本批判据是**连通性 + 零父层引用**的结构形态；**逐拍值判据**归 **T3-⑤**（design §7.1.3 已登记） |
| **B3b** ✅ | **端口完整性（G2）+ 拒绝类点名断言**：① `HierarchyEmitResult.port_provenance`（**value-only**：名 + 方向 + 宽度 + origin 枚举，**含注入 clock/reset**；**不带节点指针、不带连接关系**）；② 发射期、I/O 之前校验 `expected ⊆ actual` + **名字唯一性**（`expected` = 分析层端口 ∪ 合成端口 ∪ 注入 clock/reset；`actual` = `ModuleView.ports` ∪ `synthesized_ports`）；③ 分析层 `to_text()` 与既有字段顺序/命名 **MUST NOT 动**；④ 拒绝类的**点名断言测试**（实现已存在，**不重写**）：跨模块 `ch_mem` 两条替代路径 · 自驱动 `ch_in` · 同名不同类型 · 同类型多实例**各发一份** · 父驱动子 `ch_out`（第八格） | B3a | 完整性守卫**自身变异**（去掉 `expected ⊆ actual` ⇒ 构造缺端口 module 转红）· `port_provenance` 两次生成**顺序确定** · 五类来源各一例 · 每条拒绝**错误消息子串**点名端口/名字（MUST NOT 只抓 stderr）· `to_text()` 零改动（4 份 golden 逐字节） ⇒ ⇒ ✅ **落地证据**：`[provenance]`×2 + `[reject][naming]`×1（**26 用例 / 360 断言**）· 变异 **4/4** · lint 33/33 exit 0 |
| **B3c** ✅ | **接口白名单（ADR-062 §2.6）—— 先证可达性再实现**：① **可达性 spike（timeboxed）**：为 {`ch_mem` · `ch_reg` · bundle 切片 · `ch_stream`} 各写一条「该类型节点如何到达端口表」的具体路径；② **明文限定**：白名单只查 **`Self` origin 的声明式接口**（`ch_in` / `ch_out`）⇒ ⇒ **合成端口（`__if_` / `__x_`，含 B2b-3 的 proxy/bundle 字段）不在其列**（否则与 B2b-3 已落地形态冲突）；③ 三种结论的处置：**可达** ⇒ DUT + 守卫 + 点名；**仅内部可达**（无公开 API 形态）⇒ 守卫 + **内部谓词单测** + 登记；**不可达** ⇒ 守卫占位 + **登记「当前不可达」**（不硬造 DUT）⇒ ⇒ ⚠️ 若「不可达」导致 proposal Acceptance 的「违反 ⇒ 拒绝」无法演示 ⇒ **停下交 architect**，不得自行改 Acceptance | B3b | 每条可达路径一用例 + 点名端口 · 白名单**不得**对合成端口报错（负向对照：B2b-3 的 proxy 形态 MUST 仍产出）· 不可达类型如实登记（不假装有判据） ⇒ ⇒ ✅ **落地证据**：可达性 spike = 四类**均不可达**（不硬造 DUT）· `[whitelist]` 决策表 4 允许 + 10 拒绝 · 变异 2/2 · ⚠️ 「接入点删调用」无判据（不可达）已登记 |
| **B4** | **护栏 + 归档** `scripts/verify_hierarchical_rtl.sh`（R1 零未声明引用 · R2 实例端口名 ∈ 目标端口集 · R3 端口宽度一致 · R4 确定性 · R5 跨模块 `ch_mem` 被拒 · R6 module 树结构同构〔从产物独立提取〕· **R7 层次化产物路径 ≠ 既有扁平产物路径**〔⚠️ **适用面 = 仓内调用点**〕）+ **探针矩阵补全**（MUST 给出**格子清单**：成员类型 × 操作 × 方向 × 每格三态 + 总数；无上界的「补全」不可判定）+ `--self-test`（**R1–R7 逐条投毒映射表**，≥7 个 fixture）+ **CI 两处** + **归档 11 条 delta**（**禁** `--skip-specs`）+ 文档订正（design.md「剩余 6 个批次」「8 条 delta」· tasks.md 表头「ADDED ×7」· **proposal/tasks 的旧错误载体措辞**：「抛」「`CHFATAL_EXCEPTION`」⇒ 对外 = `ok=false` + 点名） | B3c | 每条护栏判据**原地投毒必红** ⇒ 还原复绿 · ⚠️ **lint 门 MUST 按 ADR-060 §2.5 的可执行定义**（不加 `-Wall` · 捕获退出码 · stderr 不含 `%Error` · `lint_off` 白名单 · **禁用 `-Wno-fatal`**）—— 实测 `-Wall` 对本设计的**合法**产物报 4 类 warning（`DECLFILENAME` / `PINCONNECTEMPTY` / `UNUSEDSIGNAL`×2）|

> ⚠️ **B2 的硬约束**：分析层新增字段 **MUST NOT** 进 `to_text()` ⇒ ⇒ **T3-② 的 4 份 golden 逐字节不变**
> ⇒ ⇒ 归档 change 的判据**不被削弱**（新增信息由 T3-④ 自己的测试覆盖）。
> ⚠️ **B1 顺带落地 ADR-061 v1.6.3 的判别性判据**：「scope **只**包构造 ⇒ 产物 MUST = `RespectDeclarations` 形态」
> ⇒ ⇒ 该判据**仅在设计含类级/实例级声明时**才有判别力 ⇒ ⇒ 用例 MUST 含声明，否则退化为恒真。

> ⚠️ **①b 带来的两条待跟踪事项**（后续段 MUST 携带）：
> ① **直接构造**（不经 `ch_device` / `ch_module` / `create_child`）的组件，其构造作用域帧保留到析构
>    ⇒ 该对象存活期间「组件外建的节点」会归属它（`design.md` §4 偏离 1 的已知限制）。
> ② **构造期建的成员节点名**现在带组件全路径 ⇒ Verilator 缓存 key 全失效
>    （首轮 `ctest -LE perf` 墙钟 357 → 714 sec*proc，**冷编译**，非性能回归）。

> ⚠️ **R 编号不是稳定 ID**：本文件的 R1…R8 / R1…R14 **= 该 delta 文件内 Requirement 的**出现顺序**。
> ⇒ ⇒ **这份映射在「第 0 步」执行时冻结** ⇒ ⇒ 之后任何 Requirement 增删 MUST **同步本文件的表**，
> 否则「哪条归哪个 change」会静默漂移。

---

### T3-⑤ 的 P0 文档修订（**归档阻断项** · builder 执行 · 无代码）

> ⚠️ **为什么单列成 P0**：这四项不改，T3-⑤ **归档必然被卡**（`verify_openspec_delta.sh` / V1 两条
> grep / delta 计数校验）⇒ ⇒ 它们与 B1–B7 的代码批次**同权重**，MUST NOT 当成「builder 顺手做掉」。

| # | 修订 | 判据 |
|---|---|---|
| **P0-1** | `tasks.md:3` 的「ADDED ×3」⇒ **×2**（delta 实际只有 2 条）；**删** `tasks.md:13` 的 1.4「`invoke_verilator` 支持多文件 + 缓存 key 纳入全部 source」（ADR-058 v2.10 单文件模式已封死）；`design.md` 的对应表述改为「不含多文件编译（单文件多 module）」并补 E1–E3 的工作量来源。⚠️ **工期估算：维持 ~7–9 天，不回缩** —— 删 1.4 省下多文件编译 + 缓存 key（−1~2 天），但 ADR-058 v2.13 的 **E1–E3 + 顶层名 fail-early + 映射表字段**是**新加的**工作量（+1~2 天）⇒ ⇒ 净变化 ≈ 0；**不得**因为删了一个任务就把缓冲清零 | 与 `proposal.md` 的「ADDED ×2」一致；**可执行判据**：`grep -rn "缓存 key 纳入\|invoke_verilator.*多文件"` = **0** 且 `grep -n "^- \[ \].*多文件" tasks.md` = **0**（⇒ 允许保留「不另起自建多文件 harness」「不含多文件编译」这类**理由句**） |
| **P0-2** | `tasks.md:34` / `proposal.md:50` / `design.md:11` 三处 `-Wall` ⇒ 改按 **ADR-060 §2.5**（唯一权威：不加 `-Wall` · 捕获退出码 · stderr 无 `%Error` · `lint_off` 白名单 · 禁 `-Wno-fatal`） | **可执行判据**：`grep -rn -- '--lint-only -Wall'` 在该 change 内 **0** 命中（**除** V1 检查脚本自身那一行）—— ⚠️ 不是「`-Wall` 字面量零命中」：改后的正文必然出现「**不加** `-Wall`」这类否定句 |
| **P0-3** | delta 加 **MODIFIED ×5**（对应主 spec 的 5 条 Requirement：`-Wall` 1 · 「MUST 抛异常」4 · `strict` 死引用 2，共 7 行）⇒ ⚠️ **标题 MUST 从主 spec 现有 Requirement 标题逐字复制**（否则 `verify_openspec_delta.sh` 的 R3 必红）；并补两句正文：① 观测盲区（ADR-058 §4 风险 6）→ 写进**等价性 Requirement 正文**；② **V1 两条 grep 进 tasks 的 verify 块**（正向 7 行→0 · 反向 `抛异常` 行数→1） | 落地后重跑 `bash scripts/verify_openspec_delta.sh`（4 条全绿）+ V1 两条实测为 0 / 1 |
| **P0-4** | **观测盲区三处登记的第三处（用法指南）点名到 T3-006 的 D1**（ADR-058 §4 风险 6 写的是「ADR · capability spec · 用法指南」三处同步）⇒ ⇒ 只在 P0-3 补 spec 那一处 ⇒ 归档时与 ADR 的登记义务**对不上账** | T3-006 的 D1 任务行**显式含**「风险 6 边界段」；归档前人工核对三处齐 |

### T3-⑤ 的批次拆分（2026-11-05 · planner · 据 metis gap 分析 · **7 批 + P0**）

> ⚠️ **一个批次 = 一个 commit = 一组回归判据**（沿用 T3-④ 的裁决）⇒ ⇒ 「回归变红」可归因。
> ⚠️ **顺序不可换**：B2（结构面）在 B3 之前 —— 端口集合判据**不需要 backend 仿真**就能跑，
> 早跑早抓漂移；B3 之后 B4–B6 才有可编译的产物可比。

| 批 | 内容 | 依赖 | 判据（**每条含负向对照**） | 快速门 |
|---|---|---|---|---|
| **B1** | **映射表字段（emitter 产出）**：`HierarchyEmitResult` 新增公开字段（value-only）= `node_id` + 设计级信号名 + 扁平位置 + 层次化位置集合（含 `hier_top_port_name`，**允许为空** = 该信号不在层次化顶层） | — | ① 同**一次** emit 内产出（结构上无法自建）；② harness 断言**两次 emit 共享同一 ctx**（`context::get_eval_list()` 按 `node_id` 反查 ⇒ 不许发明更弱的比对）；③ **删一项 ⇒ 转红**（变异点 = 填充处少写一行）；④ 两次生成顺序确定（逐字段比对 `vector` 顺序，不是「集合相等」）；⑤ `hier_top_port_name` 为空 ⇒ **合法**；**意外为空**（该信号其实在顶层）⇒ **必红** | `bash scripts/verify_hierarchical_rtl.sh`（R1–R7 + `--self-test`） |
| **B2** | **端口集合两条精确判据（结构面，零仿真）**：① 输入集合**相等**（名字 + 宽度逐项配对）；② 输出集合 = **精确期望集**（扁平输出集 − {`ch_out` 的**最近成层祖先 ≠ 根**}）⇒ ⇒ ⚠️ **扁平输出集的来源 MUST 是解析扁平 `.v` 的端口表**（独立载体），**MUST NOT** 从 ctx 第二次推导（否则与层次化侧同构缺陷双盲） | B1 | ① 漏发一个**输入**端口 ⇒ 红；② **不减法**（把成层祖先的 `ch_out` 留在顶层）⇒ 判据②红 ⇒ ⇒ **正反两面**缺一不可（只验「漏发」会放过「减法写错成恒等」）；③ `node_id` 索引逐项比名字 ⇒ **对调两个名字**必红（**MUST NOT** 只比集合交集） | B1 的门 |
| **B3** | **backend 显式端口集合（E1）**：新入口收显式集合 ⇒ 集合由 **B1 产物派生**（**MUST NOT** 手写/手工列举）；`build_port_access_table` / `emit_sim_main_postlude` 走该集合；**既有 `initialize(ctx,…)` 逐字节不变** | B2 | ① 集合漏 **clock/reset** ⇒ 寄存器冻结 ⇒ B5 的「随激励变化」判据**必红**（这是等价性最大恒绿源的检测器）；② 既有路径零变化 ⇒ 75 个 `verilator_e2e` 全绿 + 扁平外部基线 3 设计 `cmp` 全 0；③ 手写集合（绕过 emitter 产物）⇒ **评审项**（结构上无法机械判 ⇒ 记入 diff review checklist） | `ctest -L verilator_e2e` + 扁平冒烟 |
| **B4** | **隔离与失败语义（E2/E3）+ 顶层名 fail-early**：双 `work_dir`；失败 MUST 返回 `VerilatorError`；顶层名 ≠ `top` ⇒ 入口拒绝且**不留文件** | B3 | ① **模拟一次 Verilator 编译失败 ⇒ harness MUST 红**（**MUST NOT** SKIP）—— 这是 E3 的投毒形态，缺它则「失败静默 ⇒ 套件恒绿」无判据；② 两个 backend 共用 work_dir ⇒ 产物互踩（不是静默通过）；③ 顶层名 ≠ `top` ⇒ `ok=false` + 点名 + **零新文件** | B3 的门 + E3 用例 |
| **B5** | **等价性骨架 + 字节一致性 + P8 两层结构（前移）**：两层 DUT；`O` 手写声明（⊇ 全部 `ch_out`）；**默认全 inline ⇒ `cmp` flat.v 与 hier.v 退出码 0**；harness **当场**落成「`tests/` 内部实现 + 同文件薄包装」两层（**不暴露**公共 API） | B4 | ① `cmp` 退出码 0；**负向对照** = 在 hier 产物加一行注释 ⇒ 红；② 「随激励变化」机械判据（两组激励取值集合不同）；③ 判别力自证 ≥2 形态：**删一条连线** / **改一个隐式端口方向** ⇒ 两红 ⇒ 还原复绿；④ 激励施加 MUST 用 `set_input_value(port, <纯整数>)`（`<<=` 会把顶层端口变成内部驱动 ⇒ 观测面不一致 ⇒ 假分歧） | B4 的门 + 新用例 |
| **B6** | **DUT 矩阵补齐（其余 4 个）+ 粒度矩阵**：三层+兄弟连线 / bundle 成员跨层 / 含 `ch_mem` / **真实最深设计** | B5 | ⚠️ **粒度矩阵的裁决（不许 builder 临场发明）**：**含 `ch_mem` 的 DUT 与深设计用 `RespectDeclarations`**（ForceLayer 会把 mem 持有者成层 ⇒ 跨模块 `ch_mem` 被**合法拒绝** ⇒ 该变体**无可执行对象**）；`ForceInline` vs `ForceLayer` 的逐拍对照**只用于不含跨模块 mem 读的小 DUT**；**深设计** = 裁剪版 RV32I 核（reg + imem/dmem + 2–3 个**声明成层**的流水级），**逐顶层端口核宽度 ≤ 64**（>64 ⇒ backend 合法拒绝 ⇒ 判据空转） | `ctest -LE perf` 全量 |
| **B7** | **确定性 + 零回归 + P8 机械守卫**：同 ABI 两次生成字节相同；**计数锚点按新增用例同步**；P8 守卫 = 本 change 在 `include/` 只动 `hierarchy_verilog.h` 且**无新公共头** | B6 | ① 确定性：引入一处顺序依赖 ⇒ 红；② 锚点：`CTEST_TOTAL` / non-perf / **`VERILATOR_E2E_TOTAL`** 三处按实际增量更新（⇒ ⚠️ **本文件的验收段原写「锚点不变」是错的** —— 新增 `verilator_e2e` 用例必然改数）；parity 两锚点（66 / 289）**不变**（等价性用例不是 `DUAL_BACKEND_TEST_PAIR`）；③ P8：`git diff --name-only` 断言 | 全量 |

**三个横切投毒（缺一条就是恒绿源）**：

1. **删 reset 端口** ⇒ B5 的「随激励变化」判据**必红**（否则「寄存器冻结 ⇒ 逐拍全等恒绿」无判据）。
2. **输出集合不减法** ⇒ B2 判据②红（正反两面）。
3. **模拟 Verilator 编译失败** ⇒ harness 红（**非 SKIP**）（E3 的投毒形态）。

⚠️ **等价性用例的标签口径（裁决）**：等价性用例依赖 Verilator backend ⇒ **MUST 打 `verilator_e2e`**
（`BUILD_VERILATOR=OFF` 时随标签排除，与既有 75 个同构）⇒ ⇒ 于是 `VERILATOR_E2E_TOTAL` **必变**。

### T3-⑥ 的批次（3 批 · 前置 T3-⑤）

| 批 | 内容 | 判据 |
|---|---|---|
| **D1** | 用户面文档：`docs/usage_guide/05-component-design.md` §5 改写 · `HIERARCHICAL-RTL-AUTHORING-GUIDE.md`（删 `CH_MODULE_NAME` · 改 `CH_HIERARCHY_LAYER("name")` · 补档 ③ · **补 ADR-058 §4 风险 6 的观测盲区边界段**〔P0-4 的第三处登记〕）· 合并宏**签名面 5 处** | 示例**照抄即可编译**；指南内 `CH_MODULE_NAME` 零命中 |
| **D2** | 架构主题文档转正 + 根 `AGENTS.md`「WHERE TO LOOK」补层次化入口 | `verify_agents_size.sh` 通过（≤ 72 KiB）；入口路径可解析到实际文件 |
| **D3** | 零漂移 | `ctest -LE perf` / samples / examples 全绿（文档 change ⇒ 理论零代码影响） |

### T2b 的批次（3 批 · 与主线无依赖 · 可并行）

| 批 | 内容 | 判据（含负向对照） |
|---|---|---|
| **U1** | `io() const`（`const io_type& io() const`） | 编译期探针：`const` 成员函数内可读 `impl()` / `name()` / `is_valid()`；**破坏性对照** = 移除 const 重载 ⇒ 探针**编译失败** |
| **U2** | `context` 策略枚举（`kAllowDuplicates` 默认 / `kRejectDuplicates`）+ 建点检测 + 两层护栏 | ① on ⇒ **抛**（不静默加后缀）+ 错误信息含**冲突的节点名**；② 默认 off ⇒ 不抛 **且产物 `cmp` 0**；③ **判据 MUST 用专门构造的重名 DUT**（仓内 `include/` 641 个 `ch_in` 零显式名字 ⇒ 拿现有设计当 DUT，全仓转红是规格预期）；④ ⚠️ **诊断变量禁名 `msg`**（`CHERROR`/`CHREQUIRE`/`CHFATAL_*` 宏内部声明 `std::string msg` ⇒ 会绑定到未初始化变量 ⇒ 实测 SIGSEGV / 乱码）；⑤ 「on 抛」在**静态析构期**结构上失效（所有宏都有 `in_static_destruction()` 守卫）⇒ MUST 明确定义降级行为（记录 + 失败结果），否则「off 不抛」与「on 抛」之间有未定义带 |
| **R** | 零回归 + 锚点 | `ctest -LE perf` / samples / examples 全绿 · `verify_test_counts.sh` 两条锚点同步 |

⚠️ **U2 的错误模型分层（写在 proposal 里，别留给实施者即兴）**：建图期 fail-fast（抛）与
**发射期**错误模型（`HierarchyEmitResult` 结构体承载、发射器内 MUST NOT 抛）是**两层**
⇒ ⇒ 若混写，会被 ADR-058 §2.3 ⑤ 的「发射器内不依赖异常」读成「T2b 不许抛」。

| 阶段 | change | 内容 | 前置 | 产出 | 验收 | 预估 |
|---|---|---|---|---|---|---|
| **S0** | ✅ **spike（已完成 2026-11-02）** | `verilator --json-only` 能否提 `(parent_module, inst_name, module_name)` ⇒ **能**（`modulesp` 列表 + CELL 的 `modp` 查表）⇒ 已固化 `scripts/lib/verilog_tree_extract.py` + `tests/fixtures/hierarchical_tree_extract/` + `scripts/verify_hierarchical_tree_extract.sh`（含负向对照）+ CI 挂载 | 无 | 可机械比对的 module 树提取器 | 3 层**手写** SV 上提取成功 ✅ | ~0.5 天（已耗） |
| **T2a** | `header-odr-inline-fix-2026-11` | `include/module.h:13` 的 `build_hierarchical_name` 加 `inline`（⇒ 它是 `ch.hpp` 闭包内**唯一**非 inline 自由函数） | 无 | ODR 安全的公共头 + **multi-source 链接 fixture** | 两个 TU 同时 inclusion 后**链接成功**（无重复定义）· 既有单 TU 产物**零变化** | **1 词 + 1 fixture** |
| **T2b** | `component-io-const-and-node-name-policy-2026-11` | **U-2**（`io() const`）+ **U-1**（opt-in 节点名唯一性） | **无（与主线并行）** | `io() const` · `context::node_name_policy` | `const` 成员函数可读 `io()`；policy=**off**（默认）时产物**逐字节不变**；policy=on 时重名**被拒** | 小 |
| **T3-①a** ✅ | `hierarchical-rtl-naming-fix-2026-11` | **纯命名**：`build_hierarchical_name` 改用 `name()` · `local_name_` / `module_type_name()` · **合并宏** `CH_HIERARCHY_LAYER("name")`（= 声明 + 命名一体）· 命名权威仍是 ctx 级全局表 | **T2a** | 归属**命名**正确（无重复前缀）· 类级缺名**编译期不可表达** | 深度 ≥3 实例路径**无重复前缀**；**零语义变更**（文本**会变** ⇒ 变化面逐条登记） | 小 |
| **T3-①b** ✅ | `hierarchical-rtl-ownership-fix-2026-11` | **归属真源**：`node_owner()` + 构造作用域栈（RAII 配对）· `ch_module` parent · **build ctx 显式来源 + 跨 ctx 守卫** · 组件树登记双向完整 | **T3-①a** | 归属正确 · 组件树完整 | ADR-059 §5 全部判据 + **归属↔语义分流回归**（`simulator_init.cpp:95` 的指令表归属逐项一致 · `io.h` 4 处方向守卫**改前改后均无触发**） | 中（~1.5 天） |
| **T3-①c** ✅ | `hierarchical-rtl-granularity-declaration-2026-11` | **粒度声明**：`CH_HIERARCHY_LAYER("name")` 落地 · `ch_layer<T>` / `ch_inline<T>`（**不动 `ch_module` 签名**）· `is_layer()` · **无 active parent 处的装配点** | T3-①b | 粒度可声明 · 装配点不受限 | ADR-061 §5 全部判据（类级 × 实例级三组合 + 无 parent 装配仍成层） | 中（~1.5 天） |
| **T3-②** | `hierarchical-rtl-analysis-2026-11` | **分析层**：`ModuleDef` / `InstanceDef` / 端口表 / 隐式端口识别 / 冒泡识别 / 无主节点归顶层 | T3-①c | 模块清单 + 端口表 + 连接表（**golden**，结构化形态） | **手推** golden（≥1 个 DUT 的三层端口表 MUST 人手推导，MUST NOT 全部由实现生成）+ 其余快照逐字节零差异 | 中（~2 天） |
| **T3-③** | `hierarchical-rtl-emitter-split-2026-11` | **emitter 拆分**：`verilogwriter` → `ModuleEmitter`（接收节点子集 + 端口表 + 名字表）· **时钟/复位名参数化** | T3-①b | 可复用的单 module 发射器 | 扁平产物相对 **T3-①b 之后**的基线**逐字节不变** · `ctest -LE perf` 全绿 | 中（~3 天） |
| **T3-④** | `hierarchical-rtl-emission-2026-11` | **层次发射**：module 主体 + 实例化 + **隐式端口按方向** + 悬空冒泡 + clock/reset 贯通 + **同类型多实例不复用** + **跨模块 `ch_mem` / 非白名单接口响亮拒绝** ⇒ ⇒ **13 个批次（全部 ✅）**：B1 / B2a / B2b-1..6 / **T0h** / B3a / B3b / B3c / B4（见「T3-④ 的批次拆分」） | T3-② + T3-③ | 完整层次化 RTL | ADR-058 §5 结构判据 + ADR-060 §5（**v1.13**：G0 chain builder · G1 命名序号 · G2 端口来源 · G3 冒泡层位 · §2.4.2 `UndrivenPolicy`） + ADR-062 §5 白名单 + **ADR-061 v1.6.3 的新增判据** | 大（~3 天 ⇒ **B1 小 · B2 大 · B3a 大 · B3b 中 · B3c 小/中 · B4 中**；T0h / B2b-6 小）|
| **T3-⑤** | `hierarchical-rtl-equivalence-2026-11` | **backend 只增不改入口** + ≥5 DUT 的**映射表**与逐拍等价 + 确定性 + lint ⇒ ⇒ ⚠️ **观测面实现路径须先经 planner 修订**（台账 §11.4 行 2）⇒ ⇒ **harness MUST 写成「可公共化」形态但不暴露 API**（P8 形态约束） | T3-④ 归档（**C1 已闭合 ⇒ 「编译单文件」的 backend 假设确定** · ⚠️ **C2 与该假设无关** ⇒ ⇒ MUST NOT 再列为前置）| 等价性测试套件 | ADR-058 §5 效果判据**全通过**（见下方验收段） | **~7–9 天**（oracle R13：远不止加一个函数）|
| **T3-⑥** | `hierarchical-rtl-docs-2026-11` | **文档同步**：`docs/usage_guide/05-component-design.md` §5 · API 参考 · 架构主题文档转正 | T3-⑤ | 文档与实现一致 | 零漂移（含用户面指南**不含** `CH_MODULE_NAME`——该宏**不存在**） | 小（~1 天） |

```
T2a（1 词 · 硬前置）
  └─▶ T3-①a（纯命名 · 零语义变更）
        └─▶ T3-①b（归属真源 · 耦合仿真分流 ⇒ 独立回归面）
              ├─▶ T3-①c（粒度声明）──▶ T3-②（分析层 + golden）──┐
              └─▶ T3-③（emitter 拆分）───────────────────────────┴─▶ T3-④（层次发射）
                                                                        └─▶ T3-⑤（等价性）
                                                                              └─▶ T3-⑥（文档）

T2b（U-1 + U-2）—— **与主线无依赖** ⇒ 可全程并行
```

> ⚠️ **T2a 的「1 词」不等于「1 小时的活」**：代码改动是 1 个 `inline`，但它要配**全套 change 流程**
> （新建 `core-header-odr-safety` capability delta + 归档**禁** `--skip-specs` + 链接 fixture）
> ⇒ ⇒ **流程文档成本 ≫ 代码成本** ⇒ ⇒ 排期时 MUST 按「一个小 change」而非「改一个词」估。
> ⚠️ **T2b 的 `policy=on` 会引爆全仓**：实测 `include/` 的 **641** 个 `ch_in` 声明**零**显式名字
> ⇒ ⇒ on 一开，现有设计的节点名**全部重名** ⇒ ⇒ **这是规格预期，不是缺陷**
> ⇒ ⇒ on 的判据 MUST 用**专门构造的重名 DUT**，**MUST NOT** 拿现有设计当 DUT（否则会把预期红当 bug「修」）。

> ⚠️ **T3-③ 不是「纯搬移」**：`print_reg` / `emit_always_ff` **硬编码** `default_clock` / `default_reset`
> （`src/codegen_verilog.cpp:278-283, 755-760`）⇒ ⇒ 子模块的 `always_ff` 会引用**本模块内不存在**的信号名
> ⇒ ⇒ **时钟名 MUST 参数化** ⇒ ⇒ T3-③ 的范围与归因必须写清（不含参数化 = 层次发射时必炸）。

> ⚠️ **T3-⑤ 的工作量**：仓内**没有**「两份 Verilog 逐拍对比」的现成设施（`DUAL_BACKEND_TEST_PAIR`
> 比的是「C++ 模拟器 vs 扁平 Verilog」，**不是** Verilog A vs B）⇒ ⇒ 且 Verilator backend **写死**
> 单文件 `top.v` + `module top` ⇒ ⇒ 裁决（2026-11-02）是给 backend 加**只增不改**的
> `initialize_with_verilog(path, data_map)`（既有 `initialize(ctx,…)` **逐字节不变**）
> ⇒ ⇒ ⚠️ **但它远不止加一个函数**：`build_port_access_table`（`verilator_backend.cpp:594-660`）按
> **node id** 取 `data_map_`、按 **Vtop 字段名**取端口；`emit_sim_main_postlude`（`:193-231`）从 ctx
> 全量生成；`invoke_verilator`（`:490-497`）编译**单文件**且缓存 key 只哈希一份 source ⇒ ⇒ **重估 ~7–9 天**。

> ⚠️ **T3-② 与 T3-③ 可并行**（都只依赖 T3-①b）—— 但 **T3-② 还需 T3-①c**（粒度决定模块边界）
> ⇒ ⇒ 实际可并行的窗口 = 「T3-①c 完成后，T3-② 与 T3-③ 同时进行」。

---

## 架构裁决（2026-11-05 · architect · 台账 §11）—— 对本路径的影响

> ⚠️ 完整理由与处置见 `docs/architecture/2026-11-02-hierarchical-rtl-output.md` **§11**
> ⇒ ⇒ 与 `docs/adr/ADR-058*.md`（v2.9）· `ADR-059*.md`（v2.3.2）· `ADR-060*.md`（v1.7）·
> `ADR-061*.md`（v1.6.3）**同步**。

| 裁决 | 内容 | 对本路径的影响 |
|---|---|---|
| **A1** | `PortPolicy` 三取值的确切语义（ADR-060 §2.4.1）⇒ `SynthesizeFatal` = **先合成完再失败**（诊断含全部被合成的边与端口名）；**「合成」MUST 是纯内存 phase · 先于任何文件 I/O** | ⇒ ⇒ **B2 必须先 collect 化**（O3）⇒ ⇒ ⚠️ **collect 的口径 MUST 写死**（ADR-060 §2.4.1 的
`Reject` = 「只报第一条」⇒ ⇒ **前置校验类**错误（跨模块 `ch_mem` / 自驱动 `ch_in` / 白名单违反 /
已 build 未 `add_child`）**首错即返**；**仅「合成类」边**（隐式端口 / 冒泡）走 collect ⇒ ⇒
⇒ ⇒ **MUST NOT** 把它扩写成「所有错误统一 collect」（那会与 `Reject` 的语义冲突） |
| **A2** | 产物形态 = **单文件多 module**（撤销目录模式 ⇒ 撤销 `out_dir` / `one_file_per_module`） | ⇒ ⇒ 本路径**不再**有多文件变体 ⇒ ⇒ T3-⑤ 的 backend 假设「编译单文件」**与当前实现一致**（**C1 已闭合 2026-11-05 ⇒ 前提已兑现**）⇒ ⇒ 「同名不同物」由「文件名显式指定」消解 ⇒ ⇒ **B4 护栏 MUST 含 R7**（路径 ≠ 扁平产物路径）⇒ ⇒ ✅ **C1 已闭合**（ADR-058 v2.10 ⇒ 本裁决**不再是条件条款**） |
| **A3** | `Granularity` 权威 = **全局**（唯一入口 = RAII `Component::GranularityScope`）⇒ 选项结构**无** `granularity` 字段 ⇒ **`Component::set_granularity()` MUST 删除** ⇒ ⚠️ 理由经 oracle 实测订正（原「装配期烘焙 ⇒ 无法生效」**证伪**） | ⇒ ⇒ **B1 删 setter** + 删重复枚举 ⇒ ⇒ 补**判别性判据**「scope 只包构造 ⇒ `RespectDeclarations` 形态」⇒ ⇒ `ForceInline`/`ForceLayer` MUST 用 `GranularityScope` 且**覆盖构造 + 发射两段** |
| **A4** | **P6 / P8 均独立立项** + 各带形态约束 | ⇒ ⇒ 本路径**不含**二者 ⇒ ⇒ 但 **T3-⑤ 的 harness MUST 写成「可公共化」形态**（**不暴露** API）⇒ ⇒ 记入 T3-⑤ 的 tasks |
| **B1 / B4** | 根组件的 `CH_HIERARCHY_LAYER` = **no-op**；ADR-059 §5 措辞漂移**已订正** | ⇒ ⇒ 本文件的边界表 **B1 / B4 已闭环** |
| **O1–O5** | oracle 的 5 条发现（理由订正 / setter 零调用点 / collect 化先行 / 同名覆盖 footgun / N1 的两条命名代价） | ⇒ ⇒ 全部已落到 B1–B4 与台账 |
| **C1 / C2** | **下游确认项**：**C1 = 交付管线是否接受单文件多 module** ⇒ ⇒ ✅ **已答**（2026-11-05 · 用户拍板：**接受**）⇒ ⇒ ⇒ ADR-058 **v2.10** ⇒ ⇒ 「单文件唯一模式」**不再是条件条款**（「多文件模式」不重开 ⇒ ⇒ T3-⑤ 的 backend **不需要**文件列表 + 缓存 key 改造）⇒ ⇒ **B1 的「只加不删」兜底未触发**（代码里从未实现那两个字段） | ⇒ ⇒ **C2 = 是否拼接多份产物**⇒ ⇒ ⚠️ **仍未答** ⇒ ⇒ 但**不阻塞主线**（台账 §11.6）⇒ ⇒ 撞名有**两轴**：轴① 顶层名**已可配置**（`top_module_name`）· 轴② 内部 module 名**当前无机制** ⇒ ⇒ **预裁** = 产物级 module 名前缀选项（默认空 · 纯增量 · 事后可加 · **只作用于非根 module**）⇒ ⇒ ⚠️ **MUST NOT** 列为 T3-⑤ 的前置（与「编译单文件」无关） |

---

## 候选 change 分解（⇒ ⇒ commit 序列 · **与 §10.4 逐项对齐**）

| # | change | 范围 | 前置 | capability delta（**= 该 change 自己的 delta 文件里的 Requirement**，带标题） | 护栏交付物 | 验收 |
|---|---|---|---|---|---|---|
| **T2a** | `header-odr-inline-fix-2026-11` | **U-4 only**：`module.h:13` 加 `inline` | 无 | `core-header-odr-safety`（**ADDED ×1**） | **multi-source 链接 fixture**（两个 TU 同时 inclusion ⇒ 链接） | 链接成功 + 既有产物零变化 |
| **T2b** | `component-io-const-and-node-name-policy-2026-11` | **U-2** + **U-1** | 无 | `component-io-const-access`（**ADDED ×1**）· `core-ctx-node-name-uniqueness`（**ADDED ×1**） | `io() const` 编译期 fixture · policy=off/on 双态用例 | policy=off ⇒ 逐字节不变；policy=on ⇒ 重名被拒 |
| **T3-①a** | `hierarchical-rtl-naming-fix-2026-11` | **纯命名**（ADR-059 **D3 / D4 的类级部分**）：`build_hierarchical_name` 用 `name()` · `local_name_` / `module_type_name()` 两个维度 · **合并宏** `CH_HIERARCHY_LAYER("name")` 的定义 · 命名权威 = ctx 级全局表 | T2a | `component-ownership-and-naming` **ADDED ×2**（组件实例路径 MUST 无重复前缀 · 组件实例 MUST 提供局部名与模块类型名两个命名维度）· `rtl-hierarchical-output` **无**（命名权威是 emitter 级属性 ⇒ 归 ④） | 命名回归断言（深度 ≥3 无重复前缀）· 类级缺名**编译期**拒绝对照 | 零**语义**变更（**允许文本变化** · 仿真逐位不变） |
| **T3-①b** ✅ | `hierarchical-rtl-ownership-fix-2026-11` | **归属真源**（ADR-059 D1 / D2′ / D2″ / D6 / D7 / D8 / D9 / D10） | T3-①a | `component-ownership-and-naming` **ADDED ×5**（节点归属 MUST 由构造作用域唯一决定 · build context MUST 由显式来源确定 · 跨 context 再装配 MUST 被拒绝 · `ch_module` 无 active parent MUST 响亮失败 · 组件树登记 MUST 双向完整） | 归属断言（`CHFATAL_EXCEPTION`）· 作用域配平断言 · **归属↔语义分流回归** | ADR-059 §5 全部判据 |
| **T3-①c** | `hierarchical-rtl-granularity-declaration-2026-11` | **粒度声明**（ADR-061 决策①⑤⑥）+ **D4 的四档解析**（`ch_layer<T>` / `ch_inline<T>` / `ForceLayer` ⇒ 档 ②③④） | T3-①b | `component-ownership-and-naming` **ADDED ×1**（成层组件的模块类型名 MUST 按来源优先级确定）+ `rtl-hierarchical-output` **ADDED ×2**（层级粒度 MUST 由显式声明决定 · 成层判定 MUST 与装配点解耦） | 粒度判据（类级 × 实例级三组合）· 「无 parent 处装配仍能成层」用例 · 缺名**编译期**拒绝对照 | ADR-061 §5 全部判据 |
| **T3-②** | `hierarchical-rtl-analysis-2026-11` | **分析层** + **golden**（结构化，非自由文本） | T3-①c | `rtl-hierarchical-output` **ADDED ×1**（不属于任何组件的节点 MUST 归顶层 module） | golden 快照比对（5 个 DUT）+ **手推 golden** 至少 1 份 | 与 `tests/golden/` 逐字节零差异 · 手推那份能抓实现错 |
| **T3-③** | `hierarchical-rtl-emitter-split-2026-11` | **emitter 拆分**（**含时钟名参数化**） | T3-①b | **无 delta**（纯重构 ⇒ `.openspec.yaml` 显式 `skip_specs: true`） | 「扁平产物逐字节不变」基线脚本 | 相对 T3-①b 之后的基线逐字节不变 |
| **T3-④** | `hierarchical-rtl-emission-2026-11` | **层次发射**（module / 实例化 / 隐式端口按方向 / 悬空冒泡 / clock-reset 贯通 / **MUST NOT 复用** / 接口白名单拒绝） | T3-② · T3-③ | `rtl-hierarchical-output` **ADDED ×11**（成层组件 MUST 形成独立 module · **命名权威 MUST 是 ctx 级全局表** · 跨层非端口边 MUST 按方向合成隐式端口 · 悬空子输入 MUST 向上冒泡 · 时序模块 MUST 有 clock/reset 端口 · 成层且非根 module 端口 MUST 覆盖自身 `ch_in`/`ch_out` 且可追溯 · 单元体内 MUST NOT 引用「非自己、非祖先」的端口节点 · **层次化 MUST 有独立的公开入口，既有路径 MUST 逐字节不变** · 层次化单元的接口类型 MUST 限于白名单 · **单文件多 module + 顶层名可配置 + 路径不得与扁平同名** · **跨层端口策略 MUST 是单一权威的线性枚举** · **层级粒度 MUST 由全局唯一权威决定且作用域覆盖构造与发射**） | **新增** `scripts/verify_hierarchical_rtl.sh`（R1 lint 门 = ADR-060 §2.5 · R2 pin 名 · R3 宽度 · R4 确定性 · R5 跨模块 `ch_mem` 被拒 · R6 树同构 · R7 路径分离 + `--self-test` 8 fixture） | ADR-058 §5 结构判据 + ADR-060 §5 + ADR-062 §5 |
| **T3-⑤** | `hierarchical-rtl-equivalence-2026-11` | **backend 只增不改入口** + ≥5 DUT 的映射表与逐拍等价 | T3-④ | `rtl-hierarchical-output` **ADDED ×2**（默认（全 inline）产物 MUST 与扁平产物逐字节一致 · 层次化产物 MUST 与扁平产物等价（可机械判定）） | 等价性 harness 的**判别力自证**（故意破坏 ⇒ 必须转红）+ **公共 helper** | ADR-058 §5 效果判据全通过 |
| **T3-⑥** | `hierarchical-rtl-docs-2026-11` | **文档同步** | T3-⑤ | **无 delta**（`skip_specs: true`） | — | 文档零漂移 |

> **delta 覆盖校验**（✅ **已执行 · 2026-11-04**；⚠️ **2026-11-05 更新**：④ 由 8 → **11**（metis blocker ⇒ delta MUST 跟 Acceptance）⇒ ⇒ 合计 14 → **17**））：`rtl-hierarchical-output` = ①c 2 + ② 1 + ④ **11** + ⑤ 2 = **17** ✔ ·
> `component-ownership-and-naming` = ①a 2 + ①b 5 + ①c 1 = **8** ✔ ⇒ ⇒ **无一条 Requirement 落在多个 change**。
>
> ⚠️ **「模块类型名四档」归 ①c 而非 ①a**：档 ②③④ 需要 `ch_layer<T>` / `ForceLayer`，而它们由 ①c 引入
> ⇒ ⇒ 若把该条放进 ①a，该 change **无法自证自己的判据**（档 ②–④ 届时还不可表达）。

⚠️ **①a / ①b / ①c MUST 拆开**：①a 是**纯命名**（零语义变更）；①b 动**归属真源**
（耦合 simulator 分流与 io 方向守卫）；①c 动**装配面**（新增 API + 装配点形态）
⇒ ⇒ ⇒ 三者的回归判据完全不同 ⇒ ⇒ ⇒ 捆在一起 ⇒ 「回归变红」无法归因。

---

## 跨阶段拆分

### phase-5
- ⏳ **T2a** `header-odr-inline-fix-2026-11`（**唯一硬前置** · 1 词 + 1 fixture）
- ⏳ **T2b** `component-io-const-and-node-name-policy-2026-11`（**与主线并行** · U-1 + U-2）
- ⏳ **T3-①a** `hierarchical-rtl-naming-fix-2026-11`（**纯命名** · 零语义变更）
- ⏳ **T3-①b** `hierarchical-rtl-ownership-fix-2026-11`（**归属真源** · 耦合仿真分流）
- ⏳ **T3-①c** `hierarchical-rtl-granularity-declaration-2026-11`（粒度声明 · 默认 inline）
- ⏳ **T3-②** `hierarchical-rtl-analysis-2026-11`（分析层 + golden 文本）
- ⏳ **T3-③** `hierarchical-rtl-emitter-split-2026-11`（emitter 拆分 + 时钟名参数化）
- ⏳ **T3-④** `hierarchical-rtl-emission-2026-11`（层次发射 · 隐式端口 · 冒泡 · 不复用）
- ⏳ **T3-⑤** `hierarchical-rtl-equivalence-2026-11`（backend 只增不改入口 + 等价性）
- ⏳ **T3-⑥** `hierarchical-rtl-docs-2026-11`（文档同步）

> ⚠️ **本 feature 不阻塞其它 feature**：它是**新增能力**，不改既有 RTL 产物语义
> ⇒ ⇒ 现有 dual-backend / RV32I / AXI 线路**零依赖**。
> ⚠️ **①a / ①b / ①c 会改既有扁平产物的信号名** ⇒ ⇒ ⇒ 属**全仓可见**的文本变更
> ⇒ ⇒ ⇒ MUST 在各自的 change 里登记「文本变化面」（**至少** 4 处实测依赖：
> `tests/test_axi_spi.cpp:213-214` · `tests/benchmark/verilator_harness_tc07.cpp:33` ·
> `tc08.cpp:51` · `tests/test_verilator_backend.cpp` 的 `module top` / 端口名断言）
> ⇒ ⇒ ⇒ **MUST NOT** 用「仓内零处断言具体名字」这种未复算的结论豁免。
> ⚠️ **T2b 与主线无依赖** ⇒ ⇒ 它的 anchor（节点名 policy）**不得**被主线当作前置引用。

---

## 用户视角审查新增的两个产品缺口（⚠️ **仍待用户拍板 · 不阻断主线**）

> 来源：`docs/developer_guide/patterns/HIERARCHICAL-RTL-AUTHORING-GUIDE.md` §3
> ⇒ ⇒ ⇒ 方案的**语义**自洽可实现，但**用户面**有两条真空缺：

| # | 缺口 | 症状 | 建议 | 归属 |
|---|---|---|---|---|
| **P6** | **用户看不到自己的设计是否「层次友好」** | 无法预判会不会冒出一堆 `__undriven_*` / `__if_*` 端口 | ⭐ 「**层次友好度诊断** API」（形态照搬 `ch::core::diagnose_drive`：**单枚举 + 单一判定实现 + 护栏**）⇒ 报告隐式端口数 / 悬空输入数 / 未消费子输出数 / 跨模块 `mem` 访问数 / **各层规模** | ✅ **独立 change**（2026-11-05 · A4）⇒ ⇒ 形态约束见下 |
| **P8** | **用户拿不到「层次化后行为不变」的反馈** | 等价性验证只是框架自测 | ⭐ 等价性 harness 做成**公共 helper** ⇒ 用户可在自己的 `TEST_CASE` 里逐拍比对扁平 vs 层次 | ✅ **独立 change**（2026-11-05 · A4）⇒ ⇒ T3-⑤ 只做「可公共化」形态，**不暴露** API |

✅ **已裁决**（2026-11-05 · architect · 台账 §11.1 **A4** / §11.2）：**二者均「独立立项」**
⇒ ⇒ **不进** T3-④ / T3-⑤ 的实施范围（**MUST NOT** 静默塞入）⇒ ⇒ 但**各带一条形态约束**：
① **P6** MUST 照搬 `ch::core::diagnose_drive` 的形态（**单枚举 + 单一判定实现 + 护栏**），
   报告隐式端口数 / 悬空输入数 / 未消费子输出数 / 跨模块 `mem` 访问数 / **各层规模**；
② **P8** ⇒ T3-⑤ 的 harness MUST 写成「**可公共化**」形态（内部实现与公共包装**分离**），
   但 T3-⑤ **MUST NOT** 暴露公共 API（⇒ ⇒ 公共化 = 独立 change）。
⇒ ⇒ **不为此二项立新 ADR**（API 尚未设计 ⇒ 为未设计的 API 写 ADR = 过早固化）。
⇒ ⇒ 归位条件：**P6** 待 T3-④ 归档后由 planner 单独立项；**P8** 待 T3-⑤ 的 harness 落地后立项。

---

## 反例

- **不要**为了"层次化"去改 `toVerilog()` 的签名或默认行为 ⇒ ⇒
  既有调用点 + Verilator backend 全部承担新分支风险 ⇒ ⇒ **新入口**（ADR-058 §2.3）。
- **不要**在 T3-①b 落地前开始写 emitter ⇒ ⇒ 归属错 ⇒ 产出**看起来对、实际错**的 RTL。
- **不要**把「T3-③ = 纯搬移」当既定事实 ⇒ ⇒ `default_clock` / `default_reset`
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
- **不要**用「允许的差异 MUST 逐条列明」这类**可被平凡满足**的措辞 ⇒ ⇒
  ADR-058 的裁决是**允许差异 = 空集**（`cmp` 退出码 **0**）。
- **不要**把 ①a / ①b / ①c 合进同一个 change ⇒ ⇒ 三者回归判据不同
  ⇒ ⇒ 「扁平产物逐字节不变」将**无法归因**。
- **不要**把 T2b（U-1 / U-2）挂成主线前置 ⇒ ⇒ 它与层次化**无依赖**
  ⇒ ⇒ 挂上去只会延长关键路径（ADR-062 决策⑤ **只**点名 U-4）。

---

## 验收标准

> ⚠️ **判据三条纪律**：
> ① 「逐字节一致」MUST 是**可机械判定**的（`cmp` / `diff` 退出码），MUST NOT 是「差异逐条列明」；
> ② 每条判据 MUST 有**判别性对照**（mutation ⇒ 必须转红），否则无法区分「判据通过」与「判据空转」；
> ③ 涉及产物结构的判据 MUST 基于 **`verilator --json-only` 的结构化提取**
> （复用 `scripts/lib/verilog_tree_extract.py`）⇒ ⇒ **MUST NOT** 自研 Verilog 文本解析器
> （本仓已两次证明自研分析器「不可能被完全闭合」）。

### 前置（T2a / T2b）

- [ ] **T2a**：`build_hierarchical_name` 已是 `inline`；**两个 TU 同时 inclusion 后链接成功**
      （无 `multiple definition`）；既有单 TU 产物**零变化**
- [ ] **T2a**：fixture MUST **fail-closed** —— fixture 文件**缺失**时判据 MUST 报「缺 fixture」，
      **MUST NOT** 报成「链接失败」（⇒ 先例：`verify_conditional_assign_integrity.sh` 的 R0
      踩过「fixture 缺失被当成编译失败」⇒ 两条判据同时假绿）
- [ ] **T2b**：`io() const` 可在 `const` 成员函数中读取
- [ ] **T2b**：`node_name_policy == off`（**默认**）时产物**逐字节不变**（`cmp` 0）
      ⇒ ⇒ **mutation**：把 off 误实现为 on ⇒ 该判据**必转红**（否则「零调用点改动 ⇒ 恒绿」）
- [ ] **T2b**：policy=on 时重名**被拒**（**不是**静默加后缀）
      ⇒ ⚠️ 判据 MUST 用**专门构造的重名 DUT** —— 仓内 `include/` 的 **641** 个 `ch_in`
      声明**零**显式名字 ⇒ 拿现有设计当 DUT ⇒ 全仓转红是**规格预期**，不是缺陷

### 归属与命名（T3-①a / ①b / ①c）

- [ ] **①a**：深度 ≥3 的实例路径**无重复前缀**；`local_name_` / `module_type_name()` 就位
- [ ] **①a**：**零语义变更** ⇒ ⚠️ **文本会变**（信号名变化是 ①a 的**目的**）⇒ 判据 =
      **仿真结果逐位不变** + 文本变化面**逐条登记**（MUST NOT 写成「逐字节不变」）
- [ ] **①b**：Leaf 的 `ch_reg` 成员 `get_parent()` 是 Leaf；成员形态 `ch_module` 挂到**直接父组件**
      且可 `io()`（不早退）；**顶层 `ch_device<Pipeline>` 可实例化且不 SIGSEGV**
      （⇒ `examples/removed_examples.txt:26` 归位条件解除）；作用域**配平**
      （`create_child` 不 build 也不污染；构造期抛出 ⇒ RAII 自动回退）；归属断言 MUST 用
      `CHFATAL_EXCEPTION`（**MUST NOT** 用只打日志的 `CHREQUIRE`）
- [ ] **①b**：**归属 ↔ 语义分流**回归 —— `type_input` 节点在 `input_instr_list_` /
      `combinational_instr_list_` 的归属 MUST 与变更前**逐项一致**
- [ ] **①b**：`include/core/io.h` 的 4 处方向守卫判据 = **「改前改后均无触发」**
      （ADR-059 D7 **v2.2**）⇒ ⇒ ⚠️ **MUST NOT** 用「触发面逐项一致」—— 那 4 处判的是
      `current_component()`（与归属真源**分叉**）+ `CHREQUIRE` 不抛 ⇒ ⇒ 「相同」永远收敛不了
      （新误报让 diff 恒不等）
- [ ] **①b**：**跨 ctx 守卫的基准分两支** —— 构造期有 env ctx ⇒ `build(ctx)` 必须等于它；
      构造期无 env ctx ⇒ 以**首次** `build(ctx)` 为基准；其后换 ctx ⇒ **抛**
      ⇒ **mutation**：去掉「以首次 build 为基准」⇒「换 ctx」用例**必转红**
- [ ] **①c**：模块类型名的**四档来源**可判定 —— ① 类级 `CH_HIERARCHY_LAYER("name")` 缺名
      **编译期不可表达**；② `ch_layer<T>`（`T` 已声明）**继承**其名（MUST NOT 另给）；
      ③ `ch_layer<Helper>` **实例处给名**，缺名**编译期不可表达**；④ `ForceLayer` 回退 +
      **`CHWARN`** ⇒ **mutation**：把 ①③ 的缺名改成运行期抛 ⇒ **编译期 fixture 必转红**
- [ ] **①c**：类级 / 实例级三组合的成层结果 MUST 与声明一致；**无 active parent** 处装配的组件
      MUST 仍能按声明成层；`ch_module` 签名**零变更**
- [ ] **①a/①b/①c**：**文本变化面**逐条登记 —— ①a 的 4 处实测依赖（见「跨阶段拆分」段）
      **+ ①b 的 trace 输出**（归属变化会改 `simulator_init.cpp:95` 的指令表分桶 ⇒
      `Simulator` 的 trace 文件也是产物 ⇒ MUST 一并复核）⇒ 各自 change 内落一条登记，
      **MUST NOT** 用「仓内零处断言具体名字」这种未复算的结论豁免

### 分析层（T3-②）

- [ ] **golden 的形态 MUST 是结构化的**（建议复用 S0 的 `.tree.json` 同族形态），
      **MUST NOT** 是自由文本（⇒ 否则「逐字节」只对**发明它的那个 builder** 有意义）
- [ ] **golden MUST 含至少 1 份「手推」期望**：选 1 个 DUT，其三层端口表
      （名字 / 方向 / 宽度 / 顺序）MUST 由**人按 ADR-060 §2.1 矩阵逐项推导**写成，
      **MUST NOT** 由实现生成
      ⇒ ⇒ **理由**：全部由实现生成的 golden 会把**分析层的 bug 合法化**（端口方向抄错 ⇒
      golden 记下错的值 ⇒ 之后所有改动对着错基准绿）
      ⇒ ⇒ **判别力对照**：把分析层的某端口方向写反 ⇒ 手推那份 golden **必转红**
- [ ] golden 的**生成与比对** MUST 限定「**同编译器 / 同 ABI 内**」（`typeid` 拼写实现相关）

### emitter 拆分（T3-③）

- [ ] `toVerilog()` 的扁平产物相对 **T3-①b 之后**的基线**逐字节不变**（`cmp` 退出码 **0**）
- [ ] 时钟/复位名已参数化 ⇒ **mutation**：**去掉参数传递**（让子模块仍用硬编码
      `default_clock`）⇒ 生成物引用**未声明信号** ⇒ `verilator --lint-only` **必报错**
      （⚠️ 现状写的是「给子模块改名」—— 那是**使用说明**不是 mutation，实现根本不会跑到）

### 层次发射（T3-④）

**端口覆盖与端口表**

- [ ] **端口覆盖 = `⊇` + 五类来源**（⚠️ **MUST NOT 写成「等于」** · ADR-062 §5 oracle R1）：
      **成层且非根** module 的端口集合 MUST **⊇** 自身 `ch_in` ∪ `ch_out`（**driven 与否不影响**）
      ⇒ ⇒ 且每个端口 MUST 可追溯到五类之一：① 自身 `ch_in`/`ch_out`；② **inline 后代**的
      `ch_out`（**仅根**允许）；③ ADR-060 §2.1 隐式端口；④ ADR-060 §2.2 冒泡端口；
      ⑤ 时序基础设施 `default_clock` / `default_reset`
      ⇒ ⇒ **写成「等于」的实现为了变绿只能砍掉 ADR-060 的三条裁决**
- [ ] **RTL 顶层 module 端口表 MUST 等于扁平端口表**（**逐字逐序**）：
      `clock → reset → 无 driver 的 `input` → **全部** `ch_out`（含 inline 后代的）
      ⇒ ⇒ 本条**取代**「⊇」在**根**上的适用（二者对根**互斥**）
- [ ] **输入集合 MUST 相等**（名字 + 宽度逐项配对）；**输出集合 MUST 等于「精确期望集」**
      （= 扁平输出集 − {`ch_out` 的**最近成层祖先 ≠ 根**} 的全部节点）
      ⇒ ⇒ **MUST NOT** 只判「期望集 ⊆ 实际集」（丢端口正是子集方向）
- [ ] **空端口 module 是合法形态**：成层组件无 `ch_in`/`ch_out` ⇒ 端口 = ∅ 或
      {clock[, reset]} ⇒ ⇒ 与「⊇」**不矛盾** ⇒ **MUST NOT** 用「端口数 ≥ 1」当隐含前提
- [ ] **非根 module MUST NOT 把 inline 后代的 `ch_out` 上端口表**（它们是体内普通信号，
      ADR-061 决策② 行 2）
- [ ] **`type_output` 三角色**：① 自身端口 ⇒ 在端口表；② 隐式输出端口 ⇒ 在端口表且名字是
      合成名；③ **体内 wire** ⇒ **有 `logic` 声明**且**不在**端口表
      （⇒ 缺声明由 **ADR-060 §2.5 的 lint 门**负责 —— ⚠️ 该门**不加 `-Wall`**；
      本仓的合法产物会触发 `DECLFILENAME` / `PINCONNECTEMPTY` / `UNUSEDSIGNAL`）

**clock / reset 发射规则**

- [ ] **根 module MUST 无条件**发射 `default_clock` / `default_reset`
      （扁平 `print_header` 就是无条件的 ⇒ **纯组合设计的扁平产物也有这两端口**）
      ⇒ ⇒ **只有非根**才按 `has_seq` 决定
- [ ] **clock 与 reset MUST 分开判**：**clock** = 含 `ch_reg` **或** mem 写端口 **或** `sread`；
      **reset** = 含 `ch_reg` **或** `sread` —— ⚠️ **mem 写端口的 `always_ff` 没有 reset 分支**
      ⇒ ⇒ **纯 RAM 写 module MUST 只发 clock**
      ⇒ ⇒ 两条失败模式各要一条判据：漏**写端口** ⇒ `has_seq=false` ⇒ 引用未声明 clock ⇒ lint 报错；
      用**一元** `has_seq` ⇒ 纯 RAM 写 module 得到一根**永不使用**的 reset 端口

**连接与结构**

- [ ] **零跨 module `assign`**：`assign` 的 LHS 与所有 RHS 引用 MUST 同属**一个** module
      ⇒ ⇒ 跨 module 连接 MUST 只经**实例端口映射**
- [ ] **实例端口宽度一致**：每条实例化语句的每个端口映射，**两侧位宽 MUST 相等**
      （⇒ 不等会被 Verilog 静默截断/补零）
- [ ] **子 module 内 MUST NOT 出现** `assign <ch_in> = ...`（对 `input` 赋值非法）
- [ ] **父 module MUST 为子 `ch_out` 声明连接网**：接 **output** 的那个 `.port(...)` 实参
      MUST 是该 module 内**已声明**的 `logic`；接 **input** 的实参**可以是表达式**
      ⇒ ⇒ **MUST NOT** 强制造网
- [ ] **inline 不产生隐式端口**：被 inline 的组件体内节点 MUST 并入父 module，
      **MUST NOT** 产生 `__if_*` / `__undriven_*` 之类端口
      ⇒ ⇒ **mutation**：**在 inline 路径强制合成隐式端口** ⇒ 该判据**必转红**
      （⚠️ 现状写「把 inline 改成成层 ⇒ 端口表必变」—— 那测的是「成层会产生端口」，
      一个「inline 也合成端口」的坏实现**照样通过**）
- [ ] **结构同构**：module 树 ≡ **被声明为成层**的组件子树（节点数 / 父子关系 / 实例名
      **一一对应** ⇒ 计数对但树形错 ⇒ **不通过**）
      ⇒ ⇒ ⚠️ module 树 MUST 从**产物本身**独立提取（`verilator --json-only`），
      **MUST NOT** 复用分析层的 `ModuleDef` / `InstanceDef`（否则判据**自证**）
- [ ] 三层设计产出 **3 个 module + 2 条实例化语句**
- [ ] **同类型多实例 MUST NOT 复用 module 定义**：各发一份（名 `<module_type_name>_<序号>`，
      序号按**实例路径**排序）；**同名不同类型 ⇒ `CHFATAL_EXCEPTION`**（MUST NOT 静默合并）

**隐式端口四方向矩阵 + 第三轴**（ADR-060 §2.1 · **逐行**都要有判据）

- [ ] **父 → 子**（子侧端点**不是**子端口）：合成在**子侧**、方向 **input**
- [ ] **子 → 父**（父层读子成员的 `reg` / bundle 字段）：合成在**子侧**、方向 **output**
      （形态：`io().y <<= c.instance().acc`）
- [ ] **子端口由父层表达式驱动**：子侧复用该 `ch_in` 作 input + 父层驱动成为**实例端口连接**
      ⇒ ⇒ 子输出**不是**恒 0
- [ ] **第三轴（driver 的 `owner` 在更上层 / `nullptr`）**：沿祖先链**逐级合成 pass-through
      input 端口**（每级一个同名端口），或按 `Reject` 策略拒绝 ⇒ **MUST NOT** 静默丢
      （⇒ 真实下游形态：`setup()` 里建的 driver + 更深的 `ch_in`）
- [ ] **跨模块 `ch_mem` ⇒ `CHFATAL_EXCEPTION`**（MUST NOT 合成隐式端口）
      ⇒ ⇒ ⚠️ **「跨模块」的判定口径 MUST 用「inline 折叠后」的 module 归属** ——
      被同一 inline 组件/父层包住（折叠后同层）⇒ **不是**跨模块，**MUST NOT** 拒绝
      （否则 `chlib::memory` / `riscv2i_tcm` 被 inline wrapper 包住的**合法**写法被误拒）
      ⇒ ⇒ 拒绝时 MUST 一并给出两条替代路径（父层持有 mem / 子模块持有 mem）

**冒泡**

- [ ] **悬空子输入**在层次化产物中 MUST 是**顶层可驱动输入** ⇒ 名字 MUST 与扁平产物
      **逐字相同**（= ctx 级全局名，**无** `__undriven_` 前缀）
- [ ] 该端口旁 MUST 有**注释**标明来源组件；MUST 有 `CHWARN`；`strict` 策略下
      MUST `CHFATAL_EXCEPTION`

**归属边界的发射侧**

- [ ] **`owner == nullptr`**：`default_clock` / `default_reset` 与用户在 `main()` 里
      **不经组件**建的节点 MUST 出现在**顶层 module**（MUST NOT 被拒绝）
      ⇒ ⇒ 用户节点 MUST 有 `CHWARN` 点名 ⇒ ⇒ **clock/reset MUST NOT 告警**（否则恒响）
- [ ] **`owner ∉ subtree(top)`**：已 build 但**未 `add_child`** 的组件 ⇒ emitter MUST **抛异常**
      （MUST NOT 静默并入顶层）⇒ 错误信息 MUST 给出 `add_child` 修法
      ⇒ ⇒ **负向对照**：补 `add_child` 后同一设计 MUST 正常产出
- [ ] **自驱动 `ch_in` MUST 被拒绝**：成层组件自己驱动自己的 `ch_in` ⇒
      emitter MUST `CHFATAL_EXCEPTION`
- [ ] **已登记但从未 build** 的子组件（有节点、无端口）⇒ emitter MUST **响亮拒绝**
      （MUST NOT 发出无端口 module，也 MUST NOT 把它的节点当空气）

**错误模型**（ADR-058 §2.3）

- [ ] 所有校验 MUST 在**第一次 I/O 之前**完成 ⇒ 校验失败 MUST **不产生任何**新文件
- [ ] 写出 MUST 走「**临时目录 → 成功后 rename**」⇒ 中途失败 MUST 不留下半套产物
      **且** MUST NOT 破坏既有同名文件
- [ ] 校验 MUST **NOT** 落在 `catch(...)` 作用域内（现状 `print()` 的 header / body / footer
      各自 catch ⇒ 三条 `CHFATAL_EXCEPTION` 会被**静默吞掉**）
- [ ] **静态析构期**的失败 MUST 以**返回码 / 错误类型**呈现（MUST NOT 依赖抛异常 ⇒ 会
      `std::terminate`）⇒ ⇒ `toVerilogHierarchical` 的 `std::expected` 返回形态 MUST 有判据

### 等价性与确定性（T3-⑤）

- [ ] **默认（全 inline）** 的层次化产物与扁平产物**逐字节一致** ⇒
      **允许差异 = 空集**（`cmp` 退出码 **0**）
- [ ] **5 个 DUT 的清单 MUST 锁定** = ADR-058 §5 的那 5 个：两层 / 三层+兄弟连线 /
      **bundle 成员跨层** / **含 `ch_mem`** / 真实最深设计 ⇒ ⇒ **MUST NOT** 用 5 个 trivial
      两层组合凑数（否则 bundle 跨层 / mem / 深层场景全被绕开）
- [ ] **观测集合 `O` 的下限**：`O` MUST ⊇ DUT 的**全部** `ch_out`（⇒ 只观测 1 个信号
      会让映射表判据与等价性判据同时空转）
- [ ] **映射表覆盖被观测的全部信号**：声明的观测集合 MUST 覆盖 harness 实际读取的
      **每一个**信号（缺失一项 = 静默漏测）；映射表 MUST 是**四元组**
      （`node_id` → 设计级名 → 扁平位置 → 层次位置）
- [ ] **「至少一个随激励变化」的机械判据**：`O` 中 MUST 存在一个信号，在**两组不同激励**
      下的取值集合**不同**（⇒ MUST NOT 只写「随激励变化」这种不可判定的措辞）
- [ ] **ForceInline vs ForceLayer 逐拍全等** ⇒ ⚠️ 激励 MUST 是**多拍序列**且 DUT MUST
      **至少含一个寄存器**（⇒ 纯组合 + 单拍 ⇒ 两种粒度 trivially 全等）
- [ ] **同一 `node_id` 在两条路径的名字 MUST 逐字相同**（ADR-061 §5 oracle R6）
      ⇒ ⇒ 判据用**映射表的 `node_id` 索引**逐项比对名字；
      **MUST NOT** 只比「名字集合交集非空 / 交集大小一致」（⇒ 名字**对调**也能通过）
- [ ] **判别力自证 ≥2 形态**（ADR-058 §5）：① **删一条连线**；② **改一个隐式端口的
      方向** ⇒ ⇒ 两种都要转红，还原后复绿（只验一种 ⇒ harness 可能对方向路径**失明**）
- [ ] **确定性**：同一设计连续生成两次**字节相同** ⇒ ⚠️ 限定「**同编译器 / 同 ABI 内**」
      （`typeid` 拼写实现相关 · ADR-059 §2.2.1 ④）
- [ ] 所有产物通过 **ADR-060 §2.5 的 lint 门**（退出码 + 无 `%Error` + `lint_off` 白名单；
      ⚠️ **不加 `-Wall`**、**禁 `-Wno-fatal`**）
- [ ] **既有 backend 入口逐字节不变**：`initialize(ctx, …)` 的既有行为 MUST 不变
      （⇒ T3-⑤「只增不改」的**另一半**：新增 `initialize_with_verilog` 不得改动既有路径）

### 全阶段

- [ ] **T3-⑥**：`docs/usage_guide/05-component-design.md` §5 与
      `docs/architecture/2026-11-02-hierarchical-rtl-output.md` 与实现一致（零漂移）；
      用户面指南**不含** `CH_MODULE_NAME`（该宏**不存在**）；`CH_HIERARCHY_LAYER` MUST
      写**带名字**的形态
- [ ] **全程**：capability delta 齐备（归档**禁** `--skip-specs`，除 T3-③ / T3-⑥ 显式
      `skip_specs: true`）· `openspec validate --strict` 通过 · `ctest -LE perf` /
      `run_all_samples.sh` / `run_all_ported_tests.sh` 全绿 ·
      `scripts/verify_test_counts.sh` **两条**锚点同步（`CTEST_TOTAL` + `VERILATOR_E2E_TOTAL`
      —— ⚠️ 新增用例必然改数，**不是**「保持不变」）·
      `scripts/verify_dual_backend_parity.sh` 通过（**仅当**新增了 `DUAL_BACKEND_TEST_PAIR`
      时其两锚点才变；等价性用例**不是** parity pair ⇒ 66 / 289 不变）·
      新增护栏 `scripts/verify_hierarchical_rtl.sh` 挂 CI **两处**
- [ ] **护栏实现约束**：`verify_hierarchical_rtl.sh` 的判据 MUST 基于
      `verilator --json-only` 的结构化提取（复用 `scripts/lib/verilog_tree_extract.py`）
      ⇒ ⇒ **MUST NOT** 自研 Verilog 文本解析器（「零跨 module assign」「端口宽度一致」
      用文本解析实现 = 第三个不可闭合的自研分析器）

---

## 边界与待裁决（**MUST NOT 静默即兴**）

| # | 情形 | 本文件的立场 | 归属 |
|---|---|---|---|
| B1 | **根组件自己写 `CH_HIERARCHY_LAYER("x")`** | ✅ **已裁决**（2026-11-05 · ADR-061 v1.6.2 §2.5.2）= **no-op**（根永远是 RTL 顶层 module）⇒ ⇒ 顶层名**只**由 `top_module_name` 决定 ⇒ ⇒ **不告警**（避免恒响）⇒ ⇒ 档 ④（typeid 回退）**不适用于根** | ✅ 闭环 |
| B2 | **层次深度 > 3** | 命名/归属/冒泡都机械可推广 ⇒ 但「逐级 pass-through 端口」**恰好是深度 ≥3 才触发** ⇒ ⇒ 5 个 DUT 里 MUST 至少一个是 **3 层以上** | T3-⑤ |
| B3 | **多 context 共存**（两个独立 `ch_device` 接错线） | **不在本 feature 范围**（①b 只覆盖「同一组件换 ctx ⇒ 抛」） | 明确排除 |
| B4 | **ADR-059 §5 仍写「触发面 MUST 不变」** | ✅ **已订正**（2026-11-05 · ADR-059 v2.3.2）= 按 D7 **v2.2**（「改前改后均无触发」）⇒ ⇒ **执行既有裁决，非新增决策** | ✅ 闭环 |

