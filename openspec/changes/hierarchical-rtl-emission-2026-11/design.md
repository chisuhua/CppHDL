# 设计：层次发射（`hierarchical-rtl-emission-2026-11`）

> 决策在 ADR-058 §5（结构判据）· ADR-060 §2.1（隐式端口方向矩阵）· ADR-062 §5；
> 本文件承载**边界情形矩阵**（= 实现时逐条要有判据的清单）。
## 5. 边界情形矩阵（实现时逐条要有判据）

| # | 情况 | 期望 |
|---|---|---|
| 1 | 子输入由父端口驱动 | 例化 port map 直连 |
| 2 | 子输入由父层表达式驱动 | 父层 wire + assign，再连 |
| 3 | 子输入由兄弟输出驱动 | 父层 wire 中转（兄弟间**不能**直连） |
| 4 | 子输入由常量驱动 | 直接写字面量 |
| 5 | 子输入悬空 | 冒泡成顶层 input + `CHWARN`（`strict` ⇒ 抛） |
| 6 | 同一输入被驱动两次 | 复刻 last-wins（只用 `driver()`） |
| 7 | 子输出无人消费 | 留在实例端口上（**不**冒泡）⇒ ⇒ 顶层端口集合比扁平**少** |
| 8 | 子输出 → 父输出 | 直连 |
| 9 | 3+ 层嵌套 | 递归，模块自底向上 |
| 10 | 同类型多实例 | **各发一份** module（名 `<type>_<序号>`）⇒ ⇒ ⚠️ 复用**不触发**（端口名含实例路径 ⇒ 签名恒不同；ADR-062 §2.6） |
| 11 | 同类型不同初值 | **不复用** ⇒ 各发一份（否则初值串味） |
| 12 | 同名不同类型 | **响亮拒绝**（MUST NOT 静默合并） |
| 13 | `ch_mem` 位于多层 | mem 与使用者**同层**；跨模块访问 ⇒ 响亮拒绝 |
| 14 | bundle 成员跨层 | 合成隐式端口（按方向）+ `CHWARN` |
| 15 | 父层直接改子层 `reg->next` | 合成隐式端口（子侧 input）；父子双写 = last-wins（行为未定义 ⇒ 文档登记） |
| 16 | 子端口由父层表达式驱动 | **发射期虚拟重定向**（把驱动搬进子模块）⇒ 子输出**不是**恒 0。⚠️ **MUST NOT** 改 IR（ADR-060 §2.1）⇒ ⇒ `set_src` 会让**扁平产物也变** ⇒ 破产「既有路径逐字节不变」 |
| 17 | 无子模块的叶子 | 普通单 module |
| 18 | 空 `describe()`（纯端口） | 空 body module |
| 19 | 顶层无 `__io`（只用成员接口） | 顶层 module 无端口（合法） |
| 20 | 端口宽度 > 64 | RTL 侧允许（64-bit cap 仅为 Verilator 侧约束）⇒ ⇒ **不可**进入等价性验证的 DUT 集合 |
| 21 | 含 `ch_mem` 但只用 `aread` | 两者都不需要。⚠️ **clock / reset MUST 分开判**：clock ⇒ `reg` ∨ 写端口 ∨ `sread`；reset ⇒ `reg` ∨ `sread`（⇒ mem 写端口的 `always_ff` 无 reset 分支，`codegen_verilog.cpp:1310`） |
| 22 | 被 inline 的组件含 `ch_reg` | 父 module 必须有 clock/reset 端口（时钟需求**向上传导**） |
| 23 | 同名实例 | 实例名去重 `u_x_2`（端口名/信号名同步去重） |
| 24 | 跨 module 组合环 | ctx 全局拓扑序仍能检出 ⇒ 但诊断 MUST 指明 module 边界 |
| 25 | **组件已 build 但未登记进组件树**（`make_unique`+`build(ctx)` 不 `add_child`） | **响亮拒绝** + `add_child` 修法（⇒ 否则整块逻辑静默消失，三条判据结构性失明） |
| 26 | **组件已登记但从未 build**（`create_child` 后不 build） | **响亮拒绝**（⇒ 有节点、无端口） |
| 27 | **inline 组件位于非顶层成层 module 内** | 其 `ch_out` 是**该 module 体内**的普通信号（不上 RTL 顶；该 module 的端口表由 ADR-060 决定） |
| 28 | **inline 组件位于 RTL 顶层** | 其 `ch_out` ⇒ **顶层 output**（⇒ 扁平规则 ⇒ 支撑「全 inline 逐字节一致」） |

---

### 风险（自 umbrella §7 分派）

| # | 风险 | 对策 |
|---|---|---|
| 3 | 隐式端口方向搞反 ⇒ 错 RTL | 方向矩阵 4 行各一用例（边界 #14/#15/#16） |
| 5 | 用户把所有组件都声明成层 ⇒ 回到碎片化 | 诊断 API（feature 的 **P6** · 待用户拍板）报告每层规模；**MUST NOT** 用启发式替用户裁剪 |

---

## 6. 实施进度（**第一部分**：核心形态已落地；隐式端口等仍待做）

落点：`include/hierarchy_verilog.h` + `src/hierarchy_verilog.cpp`
（`toVerilogHierarchical(path, root, options)` ⇒ **一个文件、多个 module**，子在前、根在后）
+ `include/codegen_verilog.h`（`ModuleView::body_epilogue` / `::suppress_driver_assign`）
+ `include/hierarchy_analysis.h`（`HierarchyAnalysis::module_of_node`）。

### 6.1 已落地（逐条对应 Acceptance）

| Acceptance 项 | 状态 | 证据 |
|---|---|---|
| 三层设计 ⇒ 3 个 module + 2 条实例化语句 | ✅ | `tests/test_hierarchy_verilog.cpp` 的 `[hierarchy][emit]` |
| RTL 顶层端口表 = 扁平端口表（逐字逐序） | ✅ | 同上；**全 inline 时整份产物与扁平逐字节相同**（实测） |
| 零跨 module `assign` | ✅ | 跨 module 的驱动边 ⇒ 值由**实例端口映射**提供（`suppress_driver_assign`） |
| clock / reset **分开判** | ✅ | 纯 RAM 写 module **只有 clock**（`[hierarchy][emit][clock]`） |
| 确定性 | ✅ | 两次生成逐字节相同 |
| 错误模型（校验先于 I/O · 临时文件→rename · 不留半套产物 · 可观测） | ✅ | `[hierarchy][emit][error]`：失败 ⇒ `ok=false` + 无文件、无 `.tmp` |
| 顶层 module 名 | ✅ | 默认 `top`（与扁平同名 ⇒ 便于 A/B 比对） |

### 6.2 实施期挖出并修掉的三个真问题

1. **跨 module 的驱动边**在子 module 内仍发 `assign <子输入> = <父层信号>` ⇒ 引用**本模块里不存在**
   的信号 ⇒ ⇒ 新增 `ModuleView::suppress_driver_assign`（跨 module ⇒ 本 module 不发这条 assign）。
   ⚠️ 扁平路径该集合**恒空** ⇒ 扁平产物逐字节不变（外部基线 3 个设计 `cmp` 全 0）。
2. **module 端口节点被重复声明**（既是端口又是内部 `logic`）⇒ Verilator 报
   `Duplicate declaration of signal` ⇒ ⇒ `print_decl` MUST 跳过 module 端口节点。
3. **实例化缺 clock/reset pin** ⇒ `PINMISSING` ⇒ ⇒ **时钟需求向上传导**（子要 clock ⇒ 父也要）
   + 实例化补连 `.default_clock(...)` / `.default_reset(...)`。

⚠️ **顺带修掉一个分析层缺陷**：`nearest_layer()` 在**根组件未声明成层**时返回 `nullptr`
⇒ 全 inline 设计的根节点**全部丢失**（产物只剩空壳）⇒ ⇒ 根组件**永远**是 RTL 顶层 module。

### 6.3 尚未落地（仍在本 change 内）

- **隐式端口合成**（ADR-060 §2.1 方向矩阵四行 + 第三轴 pass-through）⇒ 当前**响亮拒绝**
  （fail-closed：MUST NOT 产出非法 Verilog）。
- **悬空冒泡**的 `strict` 抛（`Bubble` 已由分析层提供）。
- **接口白名单**（ADR-062 §2.6）。
- 护栏脚本 `scripts/verify_hierarchical_rtl.sh` + CI 两处。
- 等价性验证（**T3-⑤**，另一个 change）。

---

## 7. 后续实施路径（T3-④ 剩余 + 后续段）

> ⚠️ 本节是**实施期整理**（2026-11-04）⇒ 只承载**排期与判据映射**，不新增裁决。

### 7.0 实施期新查明的 6 个事实（决定批次顺序与工作量）

| # | 事实 | 影响 |
|---|---|---|
| **F1** | 发射器把每个 `type_op` 节点都**物化成命名 wire**（`print_binary_op` ⇒ `assign <name> = lhs op rhs;`）⇒ ⇒ 父层驱动**总是**有一根可引用的 net | ADR-060 §2.1 第三行「子端口由父层节点驱动」**不需要内联表达式渲染** ⇒ ⇒ 「发射期虚拟重定向」降级为「实例端口映射指向父层 wire」⇒ ⇒ **该格实现量远小于设计稿暗示** |
| **F2** | 分析层的冒泡分支（`src/hierarchy_analysis.cpp:247-270`）**只覆盖 inline 组件**的悬空 `ch_in`；**成层**组件自己的悬空 `ch_in` 走 `:249` 的 `Self` 分支 ⇒ **不向上冒泡** | ⇒ 层次化产物里它只在子 module 上 ⇒ **顶层无法驱动** ⇒ ⇒ 与扁平行为分叉（扁平：`make_flat_module_view` 对**任何**无 driver 的 `input` 都发顶层端口）⇒ ⇒ **B3 必修** |
| **F3** | 分析层把「owner ≠ 本 module」的 `type_output` 一律上端口表（`InlineFolded`）⇒ ⇒ 对**非顶层**成层 module 违反 ADR-061 决策②（inline 后代的 `ch_out` 在非顶层 module 里是**体内普通信号**）⇒ ⇒ 且 `print_decl` **没有** `type_output`-as-wire 分支（`type_output` 只进 `outputs` ⇒ 无 `logic` 声明 ⇒ 体内输出 = **未声明信号**） | ⇒ ⇒ **分析层 + 发射层双重缺口** ⇒ ⇒ **B2/B3 必修** |
| **F4** | `include/hierarchy_verilog.h` 另写了一份 `HierarchyPortPolicy` / `HierarchyUndrivenPolicy` ⇒ ⇒ 与 `include/component.h:52-55` 的 `ch::PortPolicy` / `ch::UndrivenPolicy` **重复**（ADR-060 §2.4 明文警告「三份副本必然漂移」）⇒ ⇒ 且 `ch::PortPolicy` 目前**零生产使用** | ⇒ ⇒ **B1 收敛为单一权威** |
| **F5** | ADR-061 §2.5 的选项结构含 `Granularity`；仓内 `Granularity` 是**全局**（`Component::set_granularity` + RAII `GranularityScope`） | ⇒ ⇒ 两个机制并存 ⇒ ⇒ 建议**沿用全局 + RAII**（T3-⑤ 的 ForceInline/ForceLayer 直接用 `GranularityScope`）⇒ ⇒ 选项结构**不加**该字段 ⇒ ⇒ 记为**与 ADR-061 §2.5 的偏差** |
| **F6** | ADR-061 §2.5 的 `out_dir` + `one_file_per_module = true` 暗示**一 module 一文件**；T3-④ 实现的是**一个文件多 module** | ⇒ ⇒ 偏差 ⇒ ⇒ 但对 T3-⑤ 有利（Verilator backend 编译**单文件**）⇒ ⇒ 建议**保留单文件** + 登记偏差（待 architect 补一格 ADR） |

### 7.0.1 批次进度（2026-11-05 起）

| 批 | 状态 | 关键证据 |
|---|---|---|
| **B1** 策略枚举收敛 | ✅ **已落地** | 删两份重复枚举 + 删 `Component::set_granularity()` · 编译期 `static_assert` · **粒度作用域三格判据**（半生效 / 正向 ForceInline / 正向 ForceLayer）· `ctest -LE perf` **238/238** · 扁平外部基线 3 设计 `cmp` 全 0 |
| **B2a** collect 化 | ✅ **已落地** | 分析层 `ImplicitPortHandling{Reject, Record}`（**默认 Reject** ⇒ ⇒ T3-② 判据零削弱 · 新字段不进 `to_text()` ⇒ ⇒ 4 份 golden 逐字节不变）· 发射层**预检 collect**（合成类问题**全部**收集 ⇒ ⇒ 上移到发射前）· 策略驱动的诊断广度（`Reject` 只报第一条 · 其余报全部）· **变异验证**：退回首错即返 ⇒ 转红 · `ctest -LE perf` **238/238** · 扁平基线 `cmp` 全 0 |
| **B2b** 隐式端口合成 | 🚧 **拆成三批**（据实施期发现）⇒ **B2b-1 ✅**（行 2「子→父」）· **B2b-2 ✅**（行 1「父→子」· 寄存器 next 子形态 + 驱动重定向）· B2b-3 = 行 1 的其余子形态（proxy/op 源）+ 第三轴 + 兄弟边 + F3 | **B2b-2**：`ModuleView::port_drives_node`（**发射期**重定向表 · **MUST NOT** 写回 IR）· `print_reg` 命中重定向 ⇒ ⇒ 寄存器 next 改由合成 input 端口驱动（⚠️ 不重定向 ⇒ ⇒ 退化成「保持」⇒ ⇒ **静默丢驱动**）· 行 1 合成（子侧 `input __if_<n>` + 父侧 `. __if_<n>(<父层驱动>)`）· 变异 M1/M2 全抓 · lint exit 0 | **B2b-1**：分析层 `PendingImplicitEdge` 增节点指针 + 方向 + 子实例名（⚠️ 实例名**按方向**取）· 发射器 `ModuleView::synthesized_ports`（合成端口**不是 IR 节点**）· 行 2 合成（子侧 `output __x_<n>` + 体内 assign + 父侧实例映射）· **未消费的子 output 以空连接 `.port()` 出现**（⇒ 否则 `PINMISSING`）· 变异 M1/M2 全抓 · lint exit 0 |
| **B3** 冒泡 + 端口完整性 + 拒绝类 + 白名单 | ⏳ | — |
| **B4** 护栏 + CI + 归档 | ⏳ | — |

⚠️ **B2a 的「中间态」登记（必读 · 否则会误读 ADR）**：隐式端口**合成本身**属 **B2b** ⇒ ⇒
本批三档策略**一律 fail-closed**（不产出产物）⇒ ⇒ **差异只在诊断广度**
⇒ ⇒ **B2b 落地后，`SynthesizeWarn` 才会真正「合成 + `CHWARN` + 产出产物」**
⇒ ⇒ ⇒ 在 B2b 之前**不得**声称「默认策略能产出产物」。

⚠️ **B1 的两处「偏离 / 处置」登记**：
① **返回类型偏离**：ADR-058 §2.3 写 `std::expected<HierManifest, HierError>`，
   而已落地的是**同构 struct** `HierarchyEmitResult{ok, error, files, warnings}`
   ⇒ ⇒ 语义等价（失败可被调用方观测 + 诊断可承载）⇒ ⇒ **登记为实现取舍**（不改 ADR）。
② **C1 的处置**：代码里**从未**实现 `out_dir` / `one_file_per_module`
   ⇒ ⇒ 「C1 未答 ⇒ 只加不删」**无对象** ⇒ ⇒ ✅ **2026-11-05 更新：C1 已答 = 接受单文件多
   module**（ADR-058 **v2.10**）⇒ ⇒ ⇒ 「多文件模式」**不重开** ⇒ ⇒ 本段仅作**历史记录**。

### 7.1 T3-④ 剩余：4 个批次（每批一个 commit）

#### B1 策略枚举收敛（**无行为变化** · 最小）

- 删 `HierarchyPortPolicy` / `HierarchyUndrivenPolicy` ⇒ 改用 `ch::PortPolicy` / `ch::UndrivenPolicy`（F4）。
- `HierarchyEmitOptions` 的字段类型随之改。
- **判据**：编译期 `static_assert`（枚举值逐个相等）+ 现有 4 用例仍绿 + **扁平外部基线 3 个设计 `cmp` 全 0**。
- **产出**：`refactor(hierarchy): 策略枚举收敛到 ADR-060 的单一权威`。

#### B2 隐式端口合成 + 兄弟边 + 第三轴（ADR-060 §2.1 · **最大**）

| 步 | 内容 | 落点 |
|---|---|---|
| 2.1 | 分析层**降级两处 fail-closed 抛**（兄弟/非相邻边 `:349` · 隐式端口 `:371`）⇒ 改为**结构化记录**；**保留**跨模块 `ch_mem` 的拒绝（`:328`，ADR-060 的裁决就是拒绝） | `src/hierarchy_analysis.cpp` |
| 2.2 | 发射层按方向矩阵合成：行 1 父→子（子侧端点非子端口）⇒ 子侧 **input** 隐式端口 · 行 2 子→父（父层读子内部节点）⇒ 子侧 **output** 隐式端口 · 行 3 子端口由父层节点驱动 ⇒ **不合成**，复用子端口 + 实例映射指向父层 wire（F1）· 行 4 跨模块 `ch_mem` ⇒ 保持拒绝 + 两条替代路径 | `src/hierarchy_verilog.cpp` |
| 2.3 | 第三轴（driver 的 `owner` 在更上层 / `nullptr`）⇒ 沿祖先链**逐级同名 pass-through input 端口**（每级一个） | 同上 |
| 2.4 | 兄弟边 ⇒ 共同祖先 module 内 wire + 两条实例映射（兄弟间**不能**直连） | 同上 |
| 2.5 | `print_decl` 增 `type_output`-as-wire 分支（ADR-061 决策② 角色 ③）+ 分析层按**作用域**区分 inline 后代输出的落点（F3） | `src/codegen_verilog.cpp` + `src/hierarchy_analysis.cpp` |
| 2.6 | 策略：`SynthesizeWarn`（默认：合成 + `CHWARN` + 文件内注释）/ `Reject`（抛） | — |

⚠️ **B2.1 的硬约束**：分析层新增字段 **MUST NOT 进 `to_text()`** ⇒ ⇒ **T3-② 的 4 份 golden 逐字节不变** ⇒ ⇒ 归档 change 的判据不被削弱（新增信息由 T3-④ 的测试覆盖）。
⚠️ **合成端口名**：`__if_<field>` / `__x_<n>` **只允许出现在非顶层 module**（ADR-060 §2.2）⇒ 顶层端口名 MUST NOT 带任何框架前缀。

- **判据**：方向矩阵**四行各一**用例 + 第三轴一用例 + 兄弟边一用例；每个用例 = 端口表断言 + `verilator --lint-only` **exit 0** + **零跨 module assign** + 「inline 不产生隐式端口」的负向对照。

#### B3 冒泡 + 端口完整性 + 拒绝类 + 接口白名单

| 步 | 内容 |
|---|---|
| 3.1 | **冒泡**（F2 缺口）：成层组件的悬空 `ch_in` 逐级冒泡到顶层（名字 = ctx 级全局名，**逐字相同**）+ 端口旁注释 + `CHWARN` + `UndrivenPolicy::Reject` ⇒ 抛 |
| 3.2 | **端口完整性**：成层且非根 module 的端口集合 **⊇** 自身 `ch_in`/`ch_out`（**driven 与否不影响**）+ 每个端口可追溯到五类来源之一 + `type_output` 三角色（自身端口 / 隐式输出 / **体内 wire**）+ **空端口 module 合法** |
| 3.3 | **拒绝类**：跨模块 `ch_mem`（已有 · 给两条替代路径）· 自驱动 `ch_in`（分析层前置校验 ③）· 子 module 内 MUST NOT `assign <ch_in> = ...` · **同名不同类型 ⇒ `CHFATAL_EXCEPTION`** · **同类型多实例 MUST NOT 复用**（名 `<module_type_name>_<按实例路径排序的序号>`） |
| 3.4 | **接口白名单**（ADR-062 §2.6）：`ch_mem` / `ch_reg` / bundle / `ch_stream` / 显式 clock-reset 声明 / `inout` ⇒ **拒绝 + 点名端口** |

- **判据**：每条一用例（含负向对照 + **错误消息点名断言**）+ 「inline 不产生隐式端口」的负向对照。

#### B4 护栏 + CI + 归档

- 4.1 `scripts/verify_hierarchical_rtl.sh`：**R1** 零未声明引用 · **R2** 实例端口名 ∈ 目标 module 端口集 · **R3** 端口宽度一致 · **R4** 产物确定性 · **R5** 跨模块 `ch_mem` 被拒 · **R6** module 树结构同构（**从产物独立提取** ⇒ MUST NOT 复用分析层 `ModuleDef`）。
- 4.2 `--self-test`（**原地投毒** ⇒ 每条判据必红 ⇒ 还原复绿）。
- 4.3 CI 挂 **两处**（`build-and-test` job：主路径 + self-test）。
- 4.4 **归档**：8 条 delta（**禁** `--skip-specs`）+ 补写主 spec `## Purpose` + 更新 feature 进度表 + 根 `AGENTS.md` NOTES + 计数锚点同步。

### 7.1.1 B2b-1 的两条**新增口径**（实施期定 ⇒ ⇒ 契约级）

| # | 口径 | 为什么 | 落在哪 |
|---|---|---|---|
| **N1** | **未被消费的子 output 端口 MUST 以空连接出现**（`.port()`） | ADR-061 决策② ② 明文「成层组件的 `ch_out` **即使无人消费**也是端口」⇒ ⇒ 父层**必须接**它 ⇒ ⇒ 否则 Verilator 报 `PINMISSING`（warning 即 error ⇒ ⇒ lint 失败）⇒ ⇒ **实测**：空连接 ⇒ lint **exit 0** | 发射器（实例化语句）+ delta 的 Scenario |
| **N2** | **子 input 端口不在 N1 之列**：未驱动的子 input MUST **冒泡**成顶层 input 并由父层接上（ADR-060 §2.2）⇒ ⇒ **现在故意不接** | ⇒ ⇒ 让 `PINMISSING` 把这类形态**响亮暴露**（而不是静默产生 X）⇒ ⇒ 归 **B3**（冒泡） | 分析层（冒泡）+ 发射器（B3 接上） |

⚠️ **N1/N2 的分界是「谁负责提供值」**：output 端口的值**由子 module 提供**（⇒ 不接就是
「无人消费」⇒ ⇒ 空连接即可）⇒ ⇒ 而 input 端口的值**必须由父层提供**（⇒ ⇒ 不接 = X
⇒ ⇒ ⇒ 必须冒泡 ⇒ ⇒ 不能用空连接糊过去）。

### 7.1.2 B2b-2 的机制**可被 B3 复用**（⇒ ⇒ B3 不必重写一遍）

⭐ **「子侧 input 端口合成」= B2b-2 建立的那套机制** ⇒ ⇒ **B3 的冒泡需要的正是它**：
冒泡要在祖先链**每一级**造一个**同名** input 端口 + 父侧同名端口接上 ⇒ ⇒
⇒ ⇒ **B3 直接复用 B2b-2 的「合成 input 端口 + 实例端口映射」两条路径**
⇒ ⇒ （B3 额外要做的只有「**驱动重定向**」—— 冒泡的端口驱动的是「子侧那个无 driver 的
`ch_in`」，而 `ch_in` 是 `type_input` ⇒ ⇒ 重定向目标**不是寄存器** ⇒ ⇒ **B3 需要把重定向表
从「寄存器」扩到「input 节点」** ⇒ ⇒ 已在 B2b-2 把表做成**通用 map**（`节点 → 端口名`）
⇒ ⇒ **B3 只需扩 `print_input` 的一个分支**）。

### 7.2 B2b（隐式端口合成）的逐行设计（**2026-11-05 写死 · 实施时照此执行**）

> ⚠️ **为什么单独写死**：ADR-060 §2.1 的方向矩阵是**契约**，但「怎么把它变成 Verilog」
> 有多个自由度 ⇒ ⇒ 不写死 ⇒ ⇒ builder 必然即兴 ⇒ ⇒ 产物不可预期。
> ⚠️ **前提**：B2a 已把「合成类问题」**collect** 起来（`pending_implicit_edges`）
> ⇒ ⇒ 本批把「记录」变成「**合成**」⇒ ⇒ **B2a 的中间态结束**。

#### 7.2.0 需要新增的**发射器能力**（`ModuleView` 扩展 · T3-③ 的 `ModuleEmitter`）

| 能力 | 形态 | 为什么必需 |
|---|---|---|
| **驱动重定向表** | `std::unordered_map<const lnodeimpl*, const lnodeimpl*> port_drives_node;`（**发射器内部**，⇒ ⇒ **MUST NOT** 写回 `lnodeimpl`） | ⇒ ADR-060 §2.1「发射期虚拟重定向」：行 1（父→子且子侧端点**不是**子端口）⇒ ⇒ 子 module 内那个被父驱动的节点（**典型是寄存器的 next 源**）**必须**改由合成端口驱动 ⇒ ⇒ 若不重定向 ⇒ ⇒ 子 module 内的寄存器仍由**原**驱动源驱动 ⇒ ⇒ 父层的驱动**丢失** ⇒ ⇒ 静默错误 RTL |
| **合成端口表** | `std::vector<SynthPort>`（name / direction / width / 目标节点 / 注释文本） | ⇒ ⇒ 端口表要在**发射前**定稿（⇒ ⇒ 端口排序 / 宽度 / 名字表都基于它） |
| **实例端口映射的扩展** | 现有 `HierarchyConnection` 已够（`child_port` + `parent_side` + `into_child`） | ⇒ ⇒ 合成端口的映射复用同一结构 ⇒ ⇒ **零新概念** |

#### 7.2.1 方向矩阵的**逐行**落法（ADR-060 §2.1）

| 行 | 形态 | **子侧** | **父侧** | 实例映射 |
|---|---|---|---|---|
| **1** | 父 → 子（子侧端点**不是**子端口） | 合成 **input** 端口 `__if_<n>`（⇒ ⇒ 子 module 内：被驱动的节点改由该端口驱动 ⇒ ⇒ 走 `port_drives_node`） | 父侧 wire（父 module 体内） | `. __if_<n>(<parent_wire>)` |
| **2** | 子 → 父（子侧端点**不是**子端口） | 合成 **output** 端口 `__x_<n>`（⇒ ⇒ 子 module 内：`assign __x_<n> = <该内部节点>;`） | 父侧端点（**通常是父的 `ch_out`** ⇒ ⇒ 直接接父的 output 端口） | `. __x_<n>(<parent_side>)` |
| **3** | 子端口由**父层节点**驱动 | **不合成**（复用该 `ch_in` 作为子 module 的 input 端口） | 父层驱动节点**已有 wire**（⇒ ⇒ F1：每个 op 都物化成命名 wire ⇒ ⇒ **不需要内联表达式渲染**） | `. <child_port>(<parent_wire>)`（**现状已实现** ⇒ ⇒ 三层用例即此行） |
| **4** | 跨模块 `ch_mem` | — | — | ⇒ ⇒ **响亮拒绝**（ADR-060 的裁决）⇒ ⇒ **不参与合成** ⇒ ⇒ 替代路径：父层持 mem（端口下发地址/写数据、端口取回读数据）· 子模块持 mem（读结果做成自己的 `ch_out`） |

#### 7.2.2 **第三轴**（driver 的 `owner` 在更上层 / `nullptr`）的算法

```
对每条「父 → 子」的边：
  driver 所在 module = D；子侧 module = C
  若 D == C 的**直接父** ⇒ ⇒ 行 1（正常）
  若 D 在 C 的**祖先链**上（更高层）或 D == 顶层 ⇒ ⇒
      ⇒ ⇒ 沿祖先链**逐级合成同名 pass-through input 端口**：
      对祖先链上的每一级 A（自顶向下）：
          在 A 上合成 input 端口（**名字 = 该节点的 ctx 级全局名** ⇒ ⇒ 顶层逐字同名）
          ⇒ ⇒ 并把「上一级的那根同名端口」作为 A 的驱动
      ⇒ ⇒ 顶层那一级 ⇒ ⇒ **顶层 input 端口** ⇒ ⇒ 外部可驱动
```

⚠️ **名字规则**（ADR-060 §2.2）：冒泡/pass-through 端口 MUST 用 **ctx 级全局名**（**无** `__undriven_` 前缀）
⇒ ⇒ 顶层端口名与扁平产物**逐字相同** ⇒ ⇒ 来源信息用**注释**承载。
⚠️ `__if_<n>` / `__x_<n>` 这类**合成端口名只允许出现在非顶层 module**（顶层 MUST NOT 带任何框架前缀）。

#### 7.2.3 **兄弟边**（子 A 的输出 → 子 B 的输入）的算法

```
共同祖先 module P（= A、B 的最近成层祖先）：
  在 P 内造一根 wire `logic [W] <p_wire>;`
  A 侧：`assign <p_wire> = <A 的 output 端口>;`（P 内）
  B 侧：`. <B 的 input 端口>(<p_wire>)`
⇒ ⇒ **兄弟间不能直连**（Verilog 无层次引用）⇒ ⇒ 中转网是唯一路径
```

#### 7.2.4 `type_output` 的**三角色**（ADR-061 决策② · F3 的另一半）

| 角色 | 何时 | 发射形态 |
|---|---|---|
| ① 自身端口 | `owner` == 本 module 的组件 | 端口表 + 声明（现状） |
| ② 隐式输出端口 | 行 2 合成出来的 | 端口表 + 声明（合成名 `__x_<n>`） |
| ③ **体内 wire** | `owner` 是**被 inline 的**子组件、且本 module 是**非顶层**成层 module | ⚠️ **现状缺这个分支** ⇒ ⇒ `print_decl` MUST 新增 `type_output`-as-wire 分支（否则体内输出 = **未声明信号** ⇒ ⇒ Verilator 报 undeclared identifier） |

⚠️ **分析层必须按「作用域」区分**：顶层 ⇒ inline 后代的 `ch_out` **上顶层 output**；
非顶层 ⇒ **体内 wire**（**不上**该 module 的端口表）⇒ ⇒ ⇒ 与 ADR-061 决策② 的两作用域表一致。

#### 7.2.5 B2b 的**判据清单**（每条 = 一个 DUT + 一条断言链）

| # | 判据 | 形态 |
|---|---|---|
| 1 | 行 1：父 → 子（子侧端点非子端口） | 子 module 有 input 合成端口 · 子内的目标节点由该端口驱动（**变异**：不重定向 ⇒ 转红） |
| 2 | 行 2：子 → 父（父层读子内部节点） | 子 module 有 output 合成端口 `__x_<n>` + `assign __x_<n> = <内部节点>;` · 父侧实参接到父的 output 端口 |
| 3 | 行 3：**已实现**（三层用例） | 回归保护 |
| 4 | 行 4：跨模块 `ch_mem` | **仍拒绝** + 错误消息含**两条替代路径** |
| 5 | 第三轴：driver 在更上层 | 祖先链逐级同名 pass-through ⇒ ⇒ 顶层出现该名字的 input 端口 |
| 6 | 兄弟边：A.out → B.in | 父层**一根** wire + 两处实例化各连一次 + **零**隐式端口（当两端都是真端口时） |
| 7 | 合成端口命名 | `__if_` / `__x_` **只出现在非顶层** module；顶层端口名**无框架前缀** |
| 8 | 隐式端口的告警与注释 | **发生合成时**逐边 `CHWARN` + 产物内注释；**未发生合成时产物无此类注释** ⇒ ⇒ 「全 inline ⇒ 与扁平逐字节一致」仍成立 |
| 9 | `type_output` 角色 ③ | 非顶层 module 里的 inline 后代 `ch_out` ⇒ ⇒ 体内有 `logic` 声明、**不在**端口表 ⇒ ⇒ `verilator --lint-only` 零 error（**缺声明必被 lint 抓**） |
| 10 | 产物确定性 | 同一设计两次生成**逐字节相同** |

⚠️ **每条 MUST 有变异对照**（⇒ ⇒ 否则「判据空转」不可区分）。

### 7.2 后续段

| 段 | change | 关键前置 | 备注 |
|---|---|---|---|
| **T3-⑤** | `hierarchical-rtl-equivalence-2026-11` | T3-④ 归档 | 「backend 只增不改入口」**远不止加一个函数**（`build_port_access_table` / `emit_sim_main_postlude` 从 ctx 全量推 · `invoke_verilator` 编译单文件且缓存 key 只哈希一份 source）⇒ ⇒ 预估 **~7–9 天** ⇒ ⇒ ⚠️ **建议在 T3-④ 归档后单独走一次 planner 修订**（观测面的实现路径有多个候选） |
| **T3-⑥** | `hierarchical-rtl-docs-2026-11` | T3-⑤ | 纯文档 ⇒ `skip_specs: true` |
| **T2b** | `component-io-const-and-node-name-policy-2026-11` | **无** | 与主线**并行**（U-1 + U-2）⇒ ⚠️ 其 anchor（节点名 policy）**不得**被主线当作前置 |

### 7.3 全程固定动作（每批都要做）

1. `cmake --build build -j$(nproc)` ⇒ **零 warning**（`-Wall -Wextra -Wpedantic`）。
2. `ctest -LE perf`（**全量重建后**跑 ⇒ `ctest` 不会自动重建 ⇒ 假绿）。
3. **扁平外部基线**：3 个设计 `cmp` 全 0（单测只能证明两条路径互相一致 ⇒ 不能证明与拆分前一致）。
4. `bash scripts/run_all_samples.sh` + `./run_all_ported_tests.sh`。
5. 护栏：`verify_component_naming` / `verify_header_odr` / `verify_test_counts` / `verify_dual_backend_parity` / `verify_openspec_delta` / `verify_roadmap_fragments`（+ 各自的 `--self-test`）。
6. `openspec validate <change> --type change --strict` + `bash scripts/verify_openspec_delta.sh`。
7. 计数锚点同步（`CTEST_TOTAL` / non-perf / `tests/test_*.cpp`）。
8. **提交按路径显式 `git add`**（MUST NOT `git add -A` ⇒ 本仓有多会话并行写入）。

### 7.4 待用户 / architect 拍板（**MUST NOT 静默即兴**）

| # | 事项 | 建议默认 |
|---|---|---|
| 1 | `PortPolicy::SynthesizeFatal` 的确切语义（ADR-060 §2.4 未写死） | = 「**合成但抛**」（与 `Reject` 的区别：合成动作先做完再抛 ⇒ 便于诊断） |
| 2 | 单文件 vs 一 module 一文件（F6） | **保留单文件** + 登记偏差（待 architect 补一格 ADR） |
| 3 | `Granularity` 放全局还是选项字段（F5） | **沿用全局 + RAII** + 登记偏差 |
| 4 | **P6**（层次友好度诊断 API）/ **P8**（等价性 harness 公共 helper） | 维持**默认不做**（用户尚未拍板 ⇒ MUST NOT 静默塞进 T3-④ / T3-⑤） |
