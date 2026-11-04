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
