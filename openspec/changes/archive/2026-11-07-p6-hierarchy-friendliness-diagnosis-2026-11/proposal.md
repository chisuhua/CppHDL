# Proposal: 层次友好度诊断 API（`p6-hierarchy-friendliness-diagnosis-2026-11`）

## Why

### 1. 用户拿不到「我的设计层次友好吗」的反馈

仓内唯一的层次化分析出口是 `ch::analyze_hierarchy(Component&, ImplicitPortHandling)`
（`include/hierarchy_analysis.h:175`），默认参数 = `ImplicitPortHandling::Reject`
（:86-92）⇒ ⇒ 用户在设计早期（**还没想好成层粒度**）想知道「哪里会不友好」时，唯一的办法是
**试发射**：

```
发射 → 分析层 Reject 抛（或 result.ok=false）→ 读错误串 → 改设计 → 再发射 → …
```

而这条路径有两个结构性缺陷：

1. **首错即抛，一次只报一类**。分析层的前置校验 + 拒绝路径共 **15 个 `CHFATAL_EXCEPTION`
   抛点**（`src/hierarchy_analysis.cpp`，实测 15 处）⇒ ⇒ 用户修掉第一类才看得到第二类
   ⇒ ⇒ **迭代次数与缺陷数成正比**。
2. **`HierarchyEmitResult.warnings` 是发射期产物**（`include/hierarchy_verilog.h:95`）
   ⇒ ⇒ 发射**成功**才有；而「不友好」恰恰是发射**不成功**的原因。

### 2. 更根本的需求是**成本**，不是「糟不糟」

成层粒度决策要的是**成本-收益**（ADR-058 决策的落点）。一个「有 7 条隐式边」的报告回答不了
「值不值得成层」：三种隐式边的代价分别是 **N / 1 / 链长**
（`sibling` 类走**祖先层中转线 + 每跳端口链**；`child side is not a port` 类只需 1 个端口）
⇒ ⇒ 同一个「边数」背后差**一个数量级**。

⇒ ⇒ 报告 MUST 同时给出「**合成端口数**」（≈ 代价）与「边数」，并在明细里带 `reason` 分类。

### 3. 权威与边界

- **权威登记**：台账 §7 残留 11（= P6）· 形态约束 = 台账 §11.2 的 P6 行
- **裁决**：§11.1 **A4**（2026-11-05）⇒ 独立立项 · **不为此立新 ADR**
- **归位条件**：✅ 已达成（T3-④ 已归档 2026-11-05）

## What Changes

1. **新增只读诊断入口**（落在 `include/hierarchy_analysis.h` · 与 `analyze_hierarchy`
   **同源同文件** ⇒ **不新增公共头**）：
   - 单枚举 `HierarchyFriendliness`（**3 档**：`kClean` / `kNeedsWork` / `kNotAnalyzable`）
   - 报告结构 `HierarchyDiagnosis`（5 大项事实 + `details` + `to_text()` 快照）
   - 函数 `diagnose_hierarchy(Component& root)`（**单一判定实现**）

2. **报告项**（每项给**可判定**定义）：

   | # | 报告项 | 定义 |
   |---|---|---|
   | 1a | `implicit_edge_count` | `pending_implicit_edges.size()`（**单位 = 边**） |
   | 1b | `synthesized_port_count` | 需合成的**端口**数（≈ 代价）⇒ 判据 = emit 后 `port_provenance` 里 `origin == Synthesized` 的条数 |
   | 2 | `dangling_input_count` | `bubbled_inputs` 中**跨 module 边界**的部分 |
   | 3 | `unconsumed_child_output_count` | 从**既有 vectors 的并集**推导（**MUST NOT** 重遍历 IR） |
   | 4 | `cross_module_mem_count` | **`std::optional`**（见 §口径订正） |
   | 5 | `module_count` + 每层 `body_size` / `instance_count` / `port_count` | 三个数**分开给**（pass-through 层 body=0 但端口很多 ⇒ 合并必误导） |

3. **口径订正（实施期实测 · 用户已拍板 A1）**：
   - 「跨模块 `mem` 访问数」在**只从 `HierarchyAnalysis` 读**的前提下**恒为 0** ——
     而那是**设计选择**（`mem` 的读写端口都把 mem 加为 `src`
     ⇒ 跨模块 mem **必产生**跨模块边 ⇒ 分析层**必抛** ⇒ ⇒ 只要返回就必为 0）
   - ⇒ **A1**：字段 = `std::optional<std::size_t>`；正常路径 `0`；不可分析路径
     `nullopt` **且**由**一次独立预扫描**（`mem_port_impl::parent()` + `module_of_node`）
     报出**真实**计数 ⇒ ⇒ 「有跨模块 mem」不会被静默表达成「0」或「不可分析」
   - ⚠️ **粒度相关性**：同一设计在 `ForceInline` 下 mem 与使用者折进同层 ⇒ 0；
     `ForceLayer` 下 ⇒ 抛 ⇒ ⇒ 该项是**粒度相关**的量，不是结构常量 ⇒ MUST 写进文档

## 不做（Non-Goals）

- ✗ **不解决**「前置校验首错即抛」（15 个抛点仍命中第一条就抛）
  ⇒ ⇒ 本 change 只解决**「隐式边家族」的完整性** ⇒ ⇒ **文档 MUST 明说这条边界**
  （否则验收通过会让人误判痛点已消除）
- ✗ **不新增 ADR**（§11.1 A4 已裁决）· ✗ **不改** `analyze_hierarchy` 默认行为
- ✗ **不动**发射器与产物
- ✗ **不覆盖**「子 `ch_out` 有端口但体内无驱动」这类（**不在** 5 项里 ⇒ 文档 MUST 写明不覆盖）
- ✗ **不在本 change** 解决「公开 API 面登记」机制（记 backlog，与 P8 同源）

## Acceptance

> 完整判据清单（19 条）见 `tasks.md` §3；此处是**摘要**。

- [ ] **5 大项齐全**；项 1a / 1b / 2 / 3 MUST 各有**专门 DUT 取到非零值**
      （⚠️ 项 5 对任何多模块设计都非零 ⇒ 不能用「非零」当判据 ⇒ 判据是「**三个数分别正确**」）
- [ ] **项 1b 与发射器对得上**：同一 ctx 下 `synthesized_port_count` ==
      emit 后 `port_provenance` 里 `origin == Synthesized` 的条数
- [ ] **边数 ≠ 端口数** 的 DUT（一个子内节点驱动两个父节点 ⇒ 边 2 / 端口 1）
      ⇒ ⇒ 这一条才真正证明单位没搞错（「== `pending_implicit_edges.size()`」是**自指**判据）
- [ ] **项 3 三条消费路径全覆盖**（直接父子 / 隐式边 / 兄弟边）+ 真未消费 + **多实例**（按实例判）
- [ ] **`details` 名称可区分**：两个悬空 `ch_in` ⇒ 两条明细的节点名**互不相同**
      （⇒ 用 `node->name()` 的实现会给出两个 `unnamed_input` ⇒ 转红）
- [ ] **`kNotAnalyzable` MUST NOT 抛**；且**未测项 MUST NOT 呈现为 0**（`nullopt`/`kUnknown`）
- [ ] **异常不吞框架 bug**：非 `std::runtime_error` MUST **重抛**
      （⇒ 「伪装成不可分析」转红）
- [ ] **只读（加强版）**：全节点 `(num_srcs, src(i)…)` 快照前后相同 +
      前后两次 `analyze_hierarchy().to_text()` 相同
- [ ] **确定性**：同一 DUT 连续两次诊断 ⇒ `to_text()` **逐字节相同**（多模块 DUT 上验）
- [ ] **判别力**：≥5 条变异（删检测 / 计数恒 0 / 分档改错 / `Record`→`Reject` /
      项 3 只查 `edges`）⇒ 对应用例**转红**；还原后**逐字节比对 + 重建 + 复绿**
- [ ] **零回归**：`ctest -LE perf` 全绿 · samples · examples · 计数锚点同步
      （`CTEST_TOTAL` +1 · `tests/test_*.cpp` +1 · `VERILATOR_E2E_TOTAL` 不变）

## Capabilities

| capability | delta |
|---|---|
| `hierarchy-friendliness-diagnosis` | **ADDED ×3**（① 入口存在且只读 ② 报告的 5 项事实与「不可分析 ≠ 0」 ③ 与发射器同源 + 确定性快照） |

⇒ 归档**禁** `--skip-specs`。

---

## 附录 A：实测证据

### A.1 分析层的 15 个抛点（全部 `CHFATAL_EXCEPTION` ⇒ `std::runtime_error`）

`src/hierarchy_analysis.cpp` 的 15 处抛点（:64 · :101 · :139 · :152 · :173 · :338 · :353 ·
:402 · :417 · :434 · :463 · :485 · :521 …）**全部**是 `CHFATAL_EXCEPTION`
（`include/utils/logger.h:235-244`）⇒ ⇒ 全部抛 `std::runtime_error`。

⇒ ⇒ **捕获策略**：诊断 MUST 只捕 `std::runtime_error` ⇒ ⇒ `module_index.at()` 的
`std::out_of_range`（继承 `logic_error`）与 `bad_alloc` 会**冒泡** ⇒ ⇒ 框架 bug
**不会**被伪装成「你的设计不可分析」。

⚠️ ⚠️ **两个副作用 MUST 写进文档**（实测自 `CHFATAL_EXCEPTION` 的定义）：

1. **日志**：该宏**先 `log_message(fatal)` 再 throw** ⇒ ⇒ 诊断路径**必然**在 stderr
   出现 `FATAL:` ⇒ ⇒ 用户设计有问题时**先喊 FATAL**。
   ⇒ ⇒ 本 change 选 **(a) 文档写明**（**不**改宏 ⇒ 零回归 · 零范围扩张）
   ⇒ ⇒ 「给 `CHFATAL_EXCEPTION` 加静默开关」记 backlog。
2. **静态析构期**：`in_static_destruction()` 守卫使该宏**一律不抛**
   ⇒ ⇒ 静态析构期的拒绝会**静默跳过** ⇒ ⇒ 诊断在该期可能得到**部分结果**
   ⇒ ⇒ MUST 在文档写明。

### A.2 项 3 的推导口径（四条排除规则 · 缺一必假阳性）

对每个非根 module `m`、每个 `!p.is_input` 且 `origin ∈ {Self, InlineFolded}` 的端口 `p`：

1. 若 `edges` 有 `e.from_node == p.name && e.from_module == m.key` ⇒ **已消费**
2. 若 `pending_implicit_edges` 有 `pe.from_node == p.name && pe.from_module == m.key`
   ⇒ **已消费**
   （⚠️ **必须并集**：兄弟/非相邻边在 `src/hierarchy_analysis.cpp:482-504`、隐式边在
   `:519-553` 都是 `continue` **在** `edges.push_back` **之前** ⇒ ⇒ 只看 `edges`
   一定把「被兄弟层消费」误判成未消费）
3. 排除 `into_child == true` 的连接（那是父驱动的，不是消费子输出）
4. **按 `(instance.module_key, instance.instance_name, child_port)` 判**，**不是**按
   `module_key`（同一 module 多实例时，按 module 去重会把「只有一个实例消费」算成「已消费」）

⇒ ⇒ **MUST NOT 重新遍历 IR/ctx**（那 = 第二份判定 ⇒ 与发射器漂移，正是 §11.2 要防的）。

### A.3 `details` 的节点名 MUST 用 **Verilog 名**

⚠️ **实施期订正（本 change 的 draft 曾写错，已订正）**：v1 的草稿断言
「`node->name()` 对所有 `ch_in` 恒为 `unnamed_input`」—— **那是 `port::name()`
（IO 访问器）的行为**（`include/core/io.h:48-68, 141-158`），**不是**
`lnodeimpl::name()`（IR 节点）。

**实测**（`src/core/lnodeimpl.cpp:32`）：`lnodeimpl::name_` = **父组件名 + "." + 创建时的
名字** ⇒ ⇒ 例如 `top.m1.leaf.unnamed_input` ⇒ ⇒ **两个不同组件里的 input 节点名字
本就不同** ⇒ ⇒ 「明细不可区分」的担忧**不成立**。

⇒ ⇒ 但「用 Verilog 名」这条要求**仍然成立**，理由改为：
★ Verilog 名与**产物里的名字同一套**（`build_verilog_name_table`）⇒ ⇒ 用户拿明细里的
名字能**直接在产物里定位** ⇒ ⇒ 而 `lnodeimpl::name()` 用的是 `.` 分隔的**组件路径**
⇒ ⇒ **不是** Verilog 里的名字 ⇒ ⇒ 用户拿它搜产物搜不到。

⇒ ⇒ 且「只用**局部名**」（`port::name()` 那种）⇒ ⇒ 两个不同组件的 input 局部名
**相同** ⇒ ⇒ 明细**不可区分** ⇒ ⇒ 变异 **M5** 实测：只取局部名 ⇒ 2 条断言**转红** ✓

### A.4 3 档而非 5 档（为什么）

台账 §11.2 要求「单枚举 + 单一判定实现 + 护栏」⇒ ⇒ `drive_state` 之所以能用单枚举，
是因为它回答的是一条**有序判定链**（`include/core/lnodeimpl.h:236-280`）⇒ ⇒
**照搬形态 ≠ 照搬档位数**。

⚠️ 5 大项里有 **4 项是正交事实**（隐式边 / 悬空输入 / 未消费子输出 / 跨模块 mem）
⇒ ⇒ 谁更糟**没有客观依据**（默认 `PortPolicy::SynthesizeWarn` +
`UndrivenPolicy::Bubble` ⇒ 隐式边与悬空输入**都是可发射的告警**）⇒ ⇒ 5 档是**伪全序**。

⇒ ⇒ 正确形态：**3 档**（`kClean` / `kNeedsWork` / `kNotAnalyzable`）+ 明细里给 5 项事实。
