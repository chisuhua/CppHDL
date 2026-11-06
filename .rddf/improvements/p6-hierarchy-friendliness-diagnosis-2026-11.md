---
name: p6-hierarchy-friendliness-diagnosis-2026-11
priority: P2
source: 台账 §7 残留 11 + §11.2（形态约束）+ roadmap `feat-hierarchical-rtl-output` §③ 后续立项清单（P6 行）
phase: phase-5
category: api
type: feature
created_at: 2026-11-06
evidence: doc:docs/architecture/2026-11-02-hierarchical-rtl-output.md#7·#11.2
---

# Improvement: 层次友好度诊断 API（P6）

- **Date drafted**: 2026-11-06（v3 —— v2 吸收 metis 第 1 轮 gap 分析；v3 落下用户拍板）
- **Drafted by**: build-agent（planner 角色）
- **Status**: 📝 草稿 v3（**设计已拍板** ⇒ 待 `rdd-builder` P0 走 5 选项批准门）
- **权威登记**：台账 §7 残留 11（= P6 诊断 API）· 形态约束 = 台账 §11.2 的 P6 行
  （**唯一权威** ⇒ 本文件只做指针与展开，**不复制全文**）
- **裁决**：§11.1 **A4**（2026-11-05）⇒ 独立立项 · **不为此二项立新 ADR**

## 1. Why (背景与动机)

### 1.1 用户拿不到「我的设计层次友好吗」的反馈

唯一出口 `ch::analyze_hierarchy(Component&, ImplicitPortHandling)`
（`include/hierarchy_analysis.h:175`），默认 `ImplicitPortHandling::Reject`
（:86-92）⇒ 用户在设计早期只能「试发射 → 读错误串 → 改 → 再试」。

⚠️ **本 change 的边界（MUST 明说，否则验收通过会让人误判痛点已消除）**：
- ✅ 解决的是**「隐式边家族」的完整性**（一次拿到全部隐式边 + 全部悬空输入 + 全部未消费子输出）
- ❌ **不解决**「前置校验首错即抛」—— 3 条前置校验（归属空洞 / 未 `build` /
  自成层驱动自己 `ch_in`）+ `ensure_bubbled_port` 撞名 + `module_index_or_throw` +
  `plan_pass_chain` 空 ⇒ ⇒ 仍是**命中第一条就抛** ⇒ 迭代次数仍与 precheck 缺陷数成正比。
  该缺口记为 backlog，**不在本 change 范围**。

### 1.2 为什么现在

- 分析层产物 `HierarchyAnalysis`（:135-163）已稳定（T3-②/③ 归档）。
- 归位条件已达成（T3-④ 已归档）。

### 1.3 用户真正要的是**成本**，不是「糟不糟」

§1.1 的痛点是「迭代次数与缺陷数成正比」，但更根本的需求是**成层粒度的成本-收益判断**
（ADR-058 决策的落点）。一个「有 7 条隐式边」的报告回答不了「值不值得成层」：
- `sibling / non-adjacent` 类的代价是「**每跳一个端口 × 链长**」（`synth_wires` + `add_pass_port`）
- `child side is not a port` 类的代价是「**1 个端口**」

⇒ ⇒ 同一个「边数」背后差一个数量级。**故本 change 的报告 MUST 同时给出
「合成端口数」（≈ 代价）与「边数」，并在 `details` 里带 `reason` 分类**（见 §2.2）。

## 2. What Changes (变更范围)

### 2.1 新增只读诊断入口

**落点**：`include/hierarchy_analysis.h`（与 `analyze_hierarchy` 同源同文件 ⇒ 不新增公共头）。

⚠️ **该头不在 `ch.hpp` 聚合器里** ⇒ 文档必须给显式 `#include "hierarchy_analysis.h"`。

**三件产出**：

| 件 | 形态 |
|---|---|
| 枚举 `HierarchyFriendliness` | **3 档**（见下，**不是 5 档**） |
| 报告结构 `HierarchyDiagnosis` | 5 大项事实 + `details`（结构化）+ `to_text()` |
| 函数 `diagnose_hierarchy(Component& root)` | **单一判定实现**（内部只调一次 `analyze_hierarchy(root, Record)`） |

#### 为什么是 **3 档**而不是 5 档（v1 的错误）

v1 想给 5 档（显式 / 需隐式端口 / 有悬空输入 / 有未消费输出 / 不可分析）。
❌ **那是伪全序**：后 4 个是**正交事实**，谁更糟**没有客观依据**（默认
`PortPolicy::SynthesizeWarn` + `UndrivenPolicy::Bubble` ⇒ 隐式边与悬空输入**都是可发射的告警**）。

`drive_state` 之所以能用单枚举，是因为它回答的是一条**有序判定链**
（`include/core/lnodeimpl.h:236-280`）⇒ ⇒ **照搬形态 ≠ 照搬档位数**。

⇒ ⇒ 正确形态：**3 档**（`kClean` / `kNeedsWork` / `kNotAnalyzable`）+ 明细里给 5 项事实。
（✅ 档位名已由用户 2026-11-06 拍板。）

### 2.2 五个报告项（逐项给**可判定**定义）

| # | 报告项 | 定义（可机械核验） | 可判定性 |
|---|---|---|---|
| 1a | `implicit_edge_count` | `pending_implicit_edges.size()`（:147）⇒ ⇒ **单位 = 边，不是端口** | ✅ 现成（需 `Record`） |
| 1b | `synthesized_port_count` | 需合成的**端口**数（发射器按 `(子 module, 节点)` 去重，`synth_port_of`）⇒ ⇒ **判据 = emit 后 `port_provenance` 里 `origin == Synthesized` 的条数** | ⚠️ 需推导（见 §2.3③） |
| 2 | `dangling_input_count` | `bubbled_inputs` 中**跨 module 边界**的部分（`b.module != root_key`，判法同 `src/hierarchy_verilog.cpp:146-150`） | ✅ 现成 |
| 3 | `unconsumed_child_output_count` | 见 §2.3③ 的**并集推导**（**MUST NOT** 重遍历 IR） | ⚠️ 需推导（数据齐） |
| 4 | `cross_module_mem_count` | 见 §2.4 —— ⚠️ **前提待重新确认** | ⚠️ 见 §2.4 |
| 5 | `module_count` + 每层 `body_size` / `instance_count` / `port_count` | `HierarchyModule::body` / `instances` / `ports` 的 `size()`（:76-79）⇒ ⇒ **三个数分开给，不合并成 "scale"**（pass-through 层 body=0 但端口很多 ⇒ 合并成单值必误导） | ✅ 现成 |

### 2.3 六个**承重**设计点（MUST 进 change 的 tasks）

| # | 点 | 不这么做的后果 |
|---|---|---|
| **①** | `ImplicitPortHandling::Record` 是**唯一**调用形态 | 不用它 ⇒ 遇到第一条隐式边就抛 ⇒ 被 ② 捕获后**退化为「不可分析 + 第一条边」** ⇒ 与 §1.1 的痛点同构 ⇒ **不是「诊断 API 直接抛」**（v1 措辞错，已订正） |
| **②** | 异常捕获 MUST **只捕 `std::runtime_error`**，其它 MUST 重抛 | `catch (const std::exception&)` 会把 `module_index.at()` 的 `out_of_range`、`bad_alloc` 伪装成「你的设计不可分析」⇒ **吞掉框架 bug** |
| **③** | 项 1b / 项 3 MUST **从既有 vectors 并集推导**，MUST NOT 重新遍历 IR/ctx | 重走节点图 = 第二份判定 ⇒ 与发射器漂移（正是台账 §11.2 要防的）。**数据全都在**：`modules[].ports`（`node`/`is_input`/`origin`）+ `instances[].connections`（`child_port`/`into_child`）+ `edges` + `pending_implicit_edges` |
| **④** | `kNotAnalyzable` 时 4 个计数 MUST 用 `std::optional`（或等价的 `kUnknown`）⇒ **MUST NOT 呈现为 0** | 「全 0 + 不可分析」能通过「字段存在」类断言 ⇒ 与 §2.3 自己批判的「恒 0 字段误导」同构 |
| **⑤** | `details` 的节点名 MUST 用 `build_verilog_name_table()` 的 **Verilog 名** | `node->name()` 对**所有** `ch_in` 恒为 `"unnamed_input"`（`include/core/io.h:143`）⇒ 明细是一串同名条目 ⇒ 而「断言子串」**照样全绿** |
| **⑥** | `details` 顺序 MUST **确定性**（两次调用逐字节相同） | `module_of_node` 是 `unordered_map` ⇒ 按它遍历顺序不定 |

**项 3 的推导口径（四条排除规则，缺一必假阳性）**：

对每个非根 module `m`、每个 `!p.is_input` 且 `origin ∈ {Self, InlineFolded}` 的端口 `p`：
1. 若 `edges` 有 `e.from_node == p.name && e.from_module == m.key` ⇒ **已消费**
2. 若 `pending_implicit_edges` 有 `pe.from_node == p.name && pe.from_module == m.key` ⇒ **已消费**
   （⚠️ **必须并集**：兄弟/非相邻边在 `src/hierarchy_analysis.cpp:482-504`、隐式边在 `:519-553`
   都是 `continue` **在** `edges.push_back` **之前** ⇒ ⇒ 只看 `edges` 一定把「被兄弟层消费」误判成未消费）
3. 排除 `into_child == true` 的连接（那是父驱动的，不是消费子输出）
4. **按 `(instance.module_key, instance.instance_name, child_port)` 判**，**不是**按 `module_key`
   （同一 module 多实例时，按 module 去重会把「只有一个实例消费」算成「已消费」）

### 2.4 项 4 的口径（✅ 已由用户 2026-11-06 拍板为 **A1**）

**v1 主张**：「跨模块 `mem` 结构上恒 0」。
**metis 核实结论**：**前提对，结论错**。

- ✅ **前提对**：`mem` 的读写端口都把 mem 加为 `src`（`include/ast/mem_port_impl.h:107` / `:145`）
  ⇒ 跨模块 mem **必产生**一条跨模块边 ⇒ `src/hierarchy_analysis.cpp:462` 必抛
  ⇒ ⇒ **只要 `analyze_hierarchy` 返回，该项必为 0**。
- ❌ **「结构上不可达」是假陈述**：它是「项 4 只从 `HierarchyAnalysis` 读」这个**设计选择**的产物。
  非零路径至少两条：
  1. **独立预扫描**：`mem_port_impl::parent()` + `module_of_node`
  2. **粒度相关**：`ForceInline` 下 mem 与使用者折进同层 ⇒ 0；`ForceLayer` 下 ⇒ 抛
     ⇒ ⇒ 项 4 是**粒度相关**的量，**不是结构常量**

#### 拍板口径（A1）

- 字段类型 = **`std::optional<std::size_t>`**（或等价的 `kUnknown`）
- **正常路径**（`analyze_hierarchy` 返回）⇒ 值 = `0`
- **不可分析路径** ⇒ 值 = `nullopt`，**且** MUST 由**一次独立预扫描**
  （`mem_port_impl::parent()` + `module_of_node`）报出**真实计数**写进 `details`
  ⇒ ⇒ 这样「有跨模块 mem」不会被静默表达成「0」或「不可分析」
- ⚠️ **代价（接受）**：多一处 `src/**` 内实现依赖（`ast/mem_port_impl.h`）⇒ ⇒
  MUST 在 tasks 里写明该依赖与其失效形态（`mem_port_impl` 若无 parent ⇒ 不属跨模块）
- ⚠️ **必须同时给粒度相关性**：文档 MUST 写明「同一设计在不同 `GranularityScope` 下
  该计数可能不同（`ForceInline` 折进同层 ⇒ 0）」⇒ 否则用户会以为是 bug

### 2.5 明确不做

- ✗ 不新增 ADR · ✗ 不改 `analyze_hierarchy` 默认行为 · ✗ 不动发射器与产物
- ✗ **不解决**前置校验首错即抛（§1.1 边界）
- ✗ **不在本 change** 解决「公开 API 面登记」机制（记 backlog，与 P8 同源）
- ✗ 不覆盖「子 `ch_out` 有端口但体内无驱动」这类（它**不在** 5 项里 ⇒ 必须在**文档**里写明不覆盖，
  否则用户会以为报告 = 全部问题）

## 3. Acceptance (验收判据 —— MUST 可勾选)

### 3.1 覆盖（防假绿）

- [ ] **5 大项齐全**；其中项 1a / 1b / 2 / 3 MUST 各有一个**专门 DUT 取到非零值**
      （⚠️ 项 5 对任何多模块设计都非零 ⇒ **不能**用「非零」当判据 ⇒ 判据是「**三个数分别正确**」）
- [ ] **项 1b 与发射器对得上**：同一 DUT（同一 ctx）下，
      `diagnose.synthesized_port_count == ` emit 后 `port_provenance` 里 `origin == Synthesized` 的条数
- [ ] **边数 ≠ 端口数** 的 DUT：一个子内节点驱动**两个**父节点 ⇒ 边数 2 / 端口数 1
      （⇒ 这条才真正证明单位没搞错；「== `pending_implicit_edges.size()`」是**自指**判据，
      抓不到单位错）
- [ ] **项 3 三条消费路径全覆盖**：直接父子消费（不计）/ 隐式边消费（不计）/
      兄弟边消费（不计）+ 真未消费（计）；再补**多实例** DUT（两实例只有一个消费 ⇒ 计数按实例判）
- [ ] **`details` 名称可区分**：两个悬空 `ch_in` ⇒ 两条明细的节点名**互不相同**
      （⇒ 用 `node->name()` 的实现会给出两个 `unnamed_input` ⇒ 转红）
- [ ] **`details` 带 `reason` 分类**（三类隐式边各自点名），否则用户拿到 7 条边不知道修法

### 3.2 不可分析路径

- [ ] **`kNotAnalyzable` MUST NOT 抛**：构造一个会抛的设计（如成层组件自驱自己的 `ch_in`、
      或跨模块 `mem`）⇒ 返回报告 + `details` 点明原因与位置
- [ ] **未测项可区分**：`kNotAnalyzable` 时 4 个计数 MUST 为 `nullopt`/`kUnknown`
      ⇒ ⇒ 判据 = 「未测得」与「真实为 0」在报告上**可区分**（打印/`to_text()` 断言）
- [ ] **异常不吞框架 bug**：注入一个非 `runtime_error` 的异常 ⇒ MUST **重抛**
      （⇒ 「伪装成不可分析」转红）
- [ ] **日志侧效应定案**：`CHFATAL_EXCEPTION` 是**先 `log_message(fatal)` 再 throw**
      （`include/utils/logger.h:235-244`）⇒ `kNotAnalyzable` 路径**必然**在 stderr 出现 `FATAL:`。
      ⇒ ⇒ 二选一并断言其一：**(a)** 文档写明「诊断会打 FATAL」；**(b)** 分析层加一条不 log 的入口
      （⇒ 这会扩大范围 ⇒ 若选 (b) MUST 在 §2.5 显式登记）

- [ ] **项 4 走 A1 口径**：正常路径 `0` / 不可分析路径 `nullopt` **且** `details` 有真实计数
      （⇒ 判据 = 一个「有跨模块 mem」的 DUT 在 `kNotAnalyzable` 下**仍能报出 ≥1**，
      **MUST NOT** 是 `nullopt` + 无明细）
- [ ] **项 4 的粒度相关性**：同一 DUT 在 `ForceInline` 与 `ForceLayer` 下结果**不同**
      （0 vs 抛/不可分析）⇒ 证明它是粒度相关量而非结构常量

### 3.3 只读与确定性

- [ ] **只读（加强）**：`get_eval_list().size()` 只能抓「新建节点」⇒ 判据 MUST 为
      **全节点 `(num_srcs, src(i)…)` 快照前后相同** + 前后两次 `analyze_hierarchy().to_text()` 相同
- [ ] **确定性**：同一 DUT 连续两次诊断 ⇒ `to_text()` **逐字节相同**（多模块 DUT 上验）

### 3.4 判别力自证

- [ ] 逐项变异 ⇒ 对应用例**转红**，至少 5 条：① 删检测 ② 计数恒定 0 ③ 分档改错
      ④ `Record` → `Reject` ⑤ 项 3 只查 `edges`（不并 `pending_implicit_edges`）
      ⇒ 还原后**逐字节比对 + 重建 + 复绿**（⚠️ 变异前确认仍能编译；还原源码 ≠ 复原）

### 3.5 零回归与登记

- [ ] `ctest -LE perf` 全绿 · samples · examples · 各护栏 PASS
- [ ] 计数锚点：`CTEST_TOTAL` +1（**不带** `verilator_e2e` ⇒ `VERILATOR_E2E_TOTAL` 不变）；
      **`AGENTS.md` Test counts 表的 `tests/test_*.cpp` 也要 +1**（143 → 144）
- [ ] 「No orphan files」：新测试文件 MUST 进 `tests/CMakeLists.txt` 的注册
- [ ] **文档**：用法指南补一节（**显式 include** + 5 项含义 + 3 档处置建议 +
      「结果依赖当前 `GranularityScope`」+ 「不覆盖体内无驱动的子输出」）；
      示例 MUST 实际编译通过

## 4. Capabilities (契约影响)

**建议新增** capability `hierarchy-friendliness-diagnosis`（待用户确认）。

契约条目（每条 MUST 可判定）：

1. **入口存在且只读** —— MUST NOT 修改图 / ctx / 产物（判据 = §3.3 的加强版快照）
2. **5 项定义** —— §2.2 表即定义；项 1 的**单位**（边 vs 端口）MUST 在契约里写明
3. **不可分析 ≠ 抛异常**，且**未测项 MUST NOT 呈现为 0**
4. **单一判定实现** —— 与 `analyze_hierarchy` 同源（判据 = 项 1b 与 `port_provenance` 对得上，
   **不是**「== `pending_implicit_edges.size()`」这种自指等式）
5. **报告快照 `to_text()`** —— 确定性排序 ⇒ ⇒ 「报告字段只增不改」这条兼容承诺才**可机械核验**
   （否则它只是一句话）

## 5. Impact (影响面)

| 面 | 影响 |
|---|---|
| 运行时 | 零（只读，仅调用时遍历） |
| 产物 | 零 |
| 测试 | +1 ctest 条目 ⇒ 计数锚点（含 `AGENTS.md` 143→144） |
| 公开 API | +1 枚举 + 1 结构 + 1 函数（落在既有头 ⇒ 不新增头文件） |
| 兼容负担 | 落在**报告结构**上 ⇒ 用 `to_text()` 快照让它可判定 |
| 风险 | ① **项 3 口径**若定错（例如把「父侧不读但传给孙子」误判未消费）⇒ 误导性计数 ⇒ MUST 配四条排除规则的反向 DUT<br>② **`kNotAnalyzable` + 全 0** 是本 change 最容易写出的假绿<br>③ 项 4 的粒度相关性若不在文档写明 ⇒ 用户会以为两次结果不同是 bug |
| 依赖 | `analyze_hierarchy` / `HierarchyAnalysis` · `diagnose_drive`（形态范式）· `build_verilog_name_table()`（`src/hierarchy_verilog.cpp:112`，明细取名） |

## 6. metis 第 1 轮 gap 分析的处置（逐条）

| # | metis finding | 处置 |
|---|---|---|
| 1 | §2.4 的「结构上恒 0」是**设计选择**，非结构事实；有两条非零路径 | ✅ **采纳** ⇒ §2.4 重写 ⇒ **已拍板 A1**（见 §6 末） |
| 2 | 项 1 的单位错（边 ≠ 端口），且 §3 的「== pending_implicit_edges.size()」是**自指**判据 | ✅ **采纳** ⇒ §2.2 拆 1a/1b + §3.1 加「边数≠端口数」DUT |
| 3 | 项 3 数据全在，**不需**重遍历 IR；只看 `edges` 必假阳性 | ✅ **采纳** ⇒ §2.3③ 写明并集推导 + 四条排除 + 按实例判 |
| 4 | 5 档是伪全序 | ✅ **采纳** ⇒ 收敛为 **3 档** |
| 5 | `kNotAnalyzable` 时 4 项取什么值未定 ⇒ 全 0 可过 AC | ✅ **采纳** ⇒ `std::optional` + §3.2 判据 |
| 6 | `details` 用 `node->name()` 恒 `unnamed_input` | ✅ **采纳** ⇒ §2.3⑤ `build_verilog_name_table()` |
| 7 | 只读判据太弱（`get_eval_list().size()`） | ✅ **采纳** ⇒ §3.3 全节点 src 快照 |
| 8 | `catch(exception)` 吞掉框架 bug | ✅ **采纳** ⇒ §2.3② 只捕 `runtime_error` |
| 9 | 日志侧效应（FATAL 必打）未定案 | ✅ **采纳** ⇒ §3.2 二选一 |
| 10 | §1.1 声称「一次报全」但只对隐式边家族成立 | ✅ **采纳** ⇒ §1.1 加显式边界 |
| 11 | 项 5 的「scale」定义未定 | ✅ **采纳** ⇒ §2.2 项 5 拆三个数 |
| 12 | `hierarchy_analysis.h` 不在 `ch.hpp` ⇒ 文档需显式 include | ✅ **采纳** ⇒ §2.1 + §3.5 |
| 13 | 缺 `HierarchyDiagnoseOptions` / 结果依赖 ambient `GranularityScope` | ✅ **采纳**（文档层）⇒ §3.5 + §5 风险 ③。**不加** options 参数（签名由实施者定稿） |
| 14 | 「5 项里没有成本指标」 | ✅ **部分采纳** ⇒ 不新增第 6 项（台账指定 5 项 + 用户拍板），但项 1 **拆出** `synthesized_port_count`（≈ 代价）⇒ §1.3 + §2.2 |
| 15 | 缺「子 `ch_out` 有端口但体内无驱动」的覆盖 | ✅ **采纳**（文档层）⇒ §2.5 写明**不覆盖** |

### 已拍板（2026-11-06）

**A. 项 4（跨模块 mem）** ⇒ **A1**：`std::optional` + 独立预扫描（`mem_port_impl::parent()` +
`module_of_node`）⇒ 见 §2.4。用户明确选择「不把该项退化成不可证伪的恒 0 字段」。

**B. 3 档档位名** ⇒ **`kClean` / `kNeedsWork` / `kNotAnalyzable`**。

⇒ ⇒ 两项均已落进正文（§2.1 / §2.4 / §3.2），**无遗留待定项**。下一步 = `rdd-builder` P0。
