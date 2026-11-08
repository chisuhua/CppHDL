---
name: hierarchical-parent-side-landing-implementation
priority: P1
phase: phase-5
category: core-impl
type: feature
状态: implemented
主题: 真层次化 RTL 输出（成层粒度由用户显式声明 · 默认 inline · 与扁平逐字节一致）
依赖: ADR-066, ADR-060, ADR-062
来源: ADR-066（Accepted 2026-11-08）· 台账 §7 行 16 + 行 19 · 裁定输入文档 docs/architecture/2026-11-08-hierarchical-parent-side-landing.md
生成时间: 2026-11-08
evidence: adr:docs/adr/ADR-066-hierarchical-parent-side-landing-semantics.md
---

# Improvement: 父侧落点语义落地（ADR-066 决策 ①–⑥）

- **Date drafted**: 2026-11-08
- **Drafted by**: planner 角色（build-agent）
- **Status**: ✅ **已实施并归档**（2026-11-09）⇒ change =
  `openspec/changes/archive/2026-11-09-hierarchical-parent-side-landing-implementation/`
  ⇒ ⇒ **A1–A24 的落点**：A1–A14 = 本 change 的 T0–T6；A15–A24 = metis 第 1 轮补充
  （其中 **A15 随台账行 20 的下游立项** · **A8 面 ① 因「命名真源唯一」改为零变异**
  ⇒ 「无需改动」的机械证据待补 ⇒ **登记为待办**）⇒ ⇒ 审稿记录 =
  `.rddf/reviews/ADR-066-parent-side-landing.md` §7（v1.1 修订轮 · oracle + critic）
- **权威决策**：`docs/adr/ADR-066-hierarchical-parent-side-landing-semantics.md`（**Accepted** · 用户拍板 Q1=B）
  ⇒ ⇒ 本 improvement **MUST NOT** 重述或改写 ADR 的决策内容（**契约唯一权威 = ADR-066**）

**主题**: 真层次化 RTL 输出（成层粒度由用户显式声明 · 默认 inline · 与扁平逐字节一致）

## 架构依据

**ADR-066 已完成双审**（oracle 1 轮 + critic 3 轮 · 两轮 BLOCKER 全部闭合并经复审确认 · `mergeable: yes`）
⇒ ⇒ 本 improvement 只**执行**它，不重新裁定：

| ADR-066 决策 | 内容 | 本 improvement 的落地动作 |
|---|---|---|
| ① | 父侧消费点**不是**父 `output` 端口（族 ①/③）⇒ **支持**：父侧合成一根 `logic` 网 + 操作数重定向；**落点网名 = 驱动端点 ctx 全局名**（MUST NOT 用被驱动节点自体名） | 发射器：父侧网合成 + 操作数名解析守卫 |
| ② | 父侧运算消费子**非端口端点**（`ch_reg`/bundle 字段/op 节点）⇒ 读**父侧落点网名**（端口名是 `__x_<n>`，二者不同名） | 同上（**同一张**重定向表） |
| ③ | **造网要求收窄**：只在「消费点非父 `output` 端口」时 MUST 造网 | spec delta（MODIFIED）+ ADR-062 §5 反向指针 |
| ④ | 诊断可行动（点名真正消费节点 · 给改法 · 删除过时归因） | 诊断文本重写 |
| ⑤ | 单连接不变式（含 ①×④ 重叠扇出一根网） | 连接去重 + 产物级断言 |
| ⑥ | 范围边界（单层 · `ch_mem` 仍拒 · 根 · 宽度沿用 W1） | 负向用例锁边界 |

**判别力底座已就位**（**不是**本 change 的产出，是本 change 的**验收工具**）：
`tests/test_hierarchy_verilog.cpp` 的 **42 格 probe matrix**（轴 A 新增 `row2_expr` · 第四态 `Broken`）

## 范围

- **In Scope**：
  1. **发射器**（`src/hierarchy_verilog.cpp`）：父侧网合成 + 父侧操作数名解析守卫 + 诊断重写 + 连接去重；
  2. **分析层**（如需）：`src/hierarchy_analysis.cpp` 的边记录（仅当父侧消费点识别必须前移时）；
  3. **契约同步**：`openspec/specs/rtl-hierarchical-output/spec.md` 的 **MODIFIED Requirement**
     （`成层且非根的 module 端口 MUST 覆盖自身 ch_in / ch_out，且每个端口可追溯` ⇒ 收窄其
     「父层消费子输出 MUST 有连接网」Scenario + 新增「父侧带运算 MUST 合成父侧网」Scenario）；
     ADR-062 §5 该条 + spec MODIFIED delta 各加反向指针 `→ ADR-066 决策 ③ 为唯一权威`；
     ADR-060 §2.1 矩阵**补格**（新增拓扑行「子 → 父 · 父侧带运算」· 与第 2 行并列、不合并）；
  4. **测试**：矩阵的 `row2_expr × ch_out` 格从 `Reject` 变 `Ok`；`row2_expr × {ch_reg, ch_proxy, op}`
     三格从 `Broken` 变 `Ok`（**第四态 `Broken` 归零 ⇒ 从矩阵移除**）；新增逐拍等价、诊断、宽度、
     单连接（含 ①×④ 重叠）、父 `input` 拒绝等负向/正向用例；
  5. **判别力自证**：V8 的**两个面**各逐条变异（面 ① 操作数名解析 · 面 ② R2 四路径回归）。
- **Out of Scope**（★ ADR-066 决策 ⑥ 明定）：
  - ❌ **跨多层**（孙组件 output 进祖父表达式）⇒ 另立裁定；
  - ❌ **跨模块 `ch_mem`** ⇒ 仍按 ADR-060 `:69` 响亮拒绝；
  - ❌ 宽度语义改动（沿用 W1）；
  - ❌ `PortPolicy` 三档分化（**行 15** · 独立 change）；
  - ❌ 行 17 的语料登记表/断言修订在**本 change 落地后**由独立 work item 处理（其归因须按新诊断重写）。

## Why

1. **ADR-066 已 Accepted**（2026-11-08）⇒ 契约已定 ⇒ 不执行 = 契约与实现长期背离，且**公共库内 7 处**
   （`rv32i_pipeline.h:398/401/402/506/524` · `rv32i_pipeline_top.h:112/121`）写法不被自己的特性支持。
2. **行 19 与新决策 ② 是同一根因的另一面**：父侧运算消费子端点时，父侧引用 MUST 是「父侧落点网名」
   ⇒ 该修复**同时**关闭族 ①/③（被拒）与路径 B（坏产物）⇒ ⇒ 两者 MUST **同批**落地（共用一张重定向表）。
3. **判别力已备**：42 格矩阵 + 第四态把「现状」钉死 ⇒ ⇒ 本 change 的变更**可见**（格值翻转 = 预期信号）。

## What Changes

1. `src/hierarchy_verilog.cpp`：
   - 父侧网合成（复用既有网登记 + `add_conn` + `print_decl` + `clashes_with_port` 撞名检查，
     **但资格判定按 ADR-066 决策 ① 重写** ⇒ 不复用兄弟边的资格判定）；
   - **落点网名 = 驱动端点 ctx 全局名**（MUST NOT 用 `names.at(e.to_ptr)` 的自体名 ⇒ 否则自指 + 双驱动）；
   - **父侧操作数名解析守卫**（`print_unary_op` / `print_mux` 等直接取 `node_names_[` 的位置）；
   - 删除已过时归因（`which this emitter cannot synthesize`）+ 点名真正消费节点 + 给可执行改法；
   - 连接去重（同 `(实例, pin)` 相同网名幂等 · 不同网名合并为一根网扇出）。
2. `openspec/specs/rtl-hierarchical-output/spec.md` ⇒ MODIFIED Requirement（见 In Scope 3）；
3. `docs/adr/ADR-060-...md` ⇒ §2.1 矩阵补格；`docs/adr/ADR-062-...md` §5 该条 ⇒ 加反向指针；
4. `tests/test_hierarchy_verilog.cpp`：矩阵格翻转 + 计数 + 新增用例；`tests/test_hierarchy_equivalence.cpp`：逐拍等价。

## Acceptance

- [ ] **A1** 42 格矩阵：`row2_expr × ch_out` = `Ok`；`row2_expr × {ch_reg, ch_proxy, op}` = `Ok`
      ⇒ ⇒ **第四态 `Broken` 归零并从矩阵移除**；上界/三态计数同步
- [ ] **A2** 产物形态：父 module **作用域内**声明 `logic` 网 + 子 output `.port(net)` + 父侧运算读该网
      ⇒ ⇒ 用 `module_text` 提取父 body 做作用域锚定 ⇒ ⇒ ⚠️ 反断言 MUST 是「父 body 内每个标识符
      在父 module 作用域内可解析」（**MUST NOT** 写「网名不出现于子 body」—— 会对正确实现假红）
- [ ] **A3** 路径 B（决策 ②）：父 body 引用的标识符 MUST NOT 是「仅在子 module 内声明」的名字
      ⇒ ⇒ 机械判据 = **R1 lint 门零 `%Error`**
- [ ] **A4** 逐拍等价：族 ①/③ 与路径 B 的产物 MUST 与扁平产物逐拍全等（`ch::hier_eq::compare`）
      ⇒ ⇒ ★ 期望值**手推** · 被观测值引到顶层可观测
- [ ] **A5** 诊断可行动：点名**真正的父侧消费节点**（非派生节点）+ 含可执行改法
      ⇒ ⇒ MUST NOT 含「隐式端口合成不支持 / 无法合成」类归因
- [ ] **A6** 宽度：不一致 MUST 响亮拒绝 + 诊断点名**两个**宽度值
- [ ] **A7** 单连接：同一子 `ch_out` 在实例语句中至多连接一次；多消费点（含 ①×④ 重叠）扇出同一根网
      ⇒ ⇒ 产物 MUST NOT 出现 `Duplicate pin connection`
- [ ] **A8** 变异（**两个面各逐条**）：面 ① 操作数名解析（grep 锚 `node_names_[` ⇒ 变异点数 == 命中数）·
      面 ② R2 四路径 ⇒ ⇒ 每条：编译确认 → **重建** → 红 → 还原 → **重建** → 绿
- [ ] **A9** 回归锚：「默认（全 inline）产物与扁平产物逐字节一致」仍成立
- [ ] **A10** 粒度负向对照：默认粒度下产出单 module（跨层形态不出现）
- [ ] **A11** lint 门：所有新增/修改产物过 ADR-060 §2.5（零 `%Error`）
- [ ] **A12** 契约同步：spec MODIFIED Requirement 合并 + ADR-062 §5 反向指针 + ADR-060 §2.1 补格
      ⇒ ⇒ `openspec validate --specs --strict` 通过
- [ ] **A13** 父 `input` 端口被拒（决策 ① 排除项）⇒ 负向用例 + 诊断非空
- [ ] **A14** 零孤儿：新增 `.cpp` 注册进 CMake · 新增头被包含

- [ ] **A15 ★ `ch_mem` 端口不得静默不发射**：父层 `ch_mem` 读/写端口的 `addr` / `en` / `data` 若取自子 `ch_out`
      ⇒ ⇒ 父 body MUST 出现对应的 `assign` / `always_ff` 语句 ⇒ ⇒ **判据 = 读值非 0**（ⓘ 只断言 lint 干净**不够**：
      `src/codegen_verilog.cpp:1266-1270` 在 `!node_names_.count(addr)` 时 **early-return**
      ⇒ ⇒ 整条 mem 读端口**静默不发射** ⇒ ⇒ 产物 **过 lint、值恒错**）；真实动机用例 =
      `include/cpu/pipeline/rv32i_pipeline.h:194-197`（`reg_file.aread(id_stage.io().rs1_addr_id, …)`）
- [ ] **A16 ★ 行 17 的 tripwire 必须在本 change 内定值**：`tests/test_rv32i_stage_dual.cpp:430-441`
      断言 `ImmConsistencyDut` **MUST 被拒**且错误串含 `implicit-port synthesis` ⇒ ⇒ 而该 DUT 的形态正是
      「父侧带运算」（`:258` `io().equal <<= (dec.io().imm == ids.io().imm)`）⇒ ⇒ 本 change 落地后
      **必然转红** ⇒ ⇒ 新期望 MUST 在本 change 内落地（`ok==true` + 产物过 lint，或改断言新归因）
- [ ] **A17 拒绝面不萎缩**：矩阵其余 **13 个 `Reject` 格**的格值与行为断言逐条不变；并新增负向用例
      **跨多层（孙组件）** 与 **表亲边** ⇒ ⇒ 二者 MUST 仍被拒（ADR-066 决策 ⑥ 边界）
- [ ] **A18 `PortPolicy::Reject` 档**：首错 MUST = **真正的父侧消费节点**（收窄兜底检查后 `front()` 的语义变了）
- [ ] **A19 ★ 删 `Broken` 态的替代不变量**（否则是**倒退**）：`row2_expr × {ch_out, ch_reg, ch_proxy, op}`
      四格产物 MUST 进入某个 **lint 集合**（扩展 `tests/hier_rtl_products.cpp` 或让护栏 lint 矩阵输出目录）
      ⇒ ⇒ 零 `%Error` ⇒ ⇒ 该护栏 MUST 带 `--self-test` 投毒
      （ⓘ `Broken` 是仓内**唯一**表达「`ok=true` 但产物不可编译」的态；只删态不补 lint 覆盖 ⇒
      未来同类缺陷会被记成 `Ok` ⇒ 而 `test_hierarchy_verilog.cpp` **一次 verilator 都不调用**）
- [ ] **A20 台账闭口**：§7 行 **16** `未决`→`已裁决`（证据 = ADR-066）· 行 **19** `未决`→`已收口`
      （证据 = 本 change 归档目录）⇒ 登记表同步 ⇒ `verify_arch_ledger_residuals.sh` **+ `--self-test`** 转绿
- [ ] **A21 真实库站点验收**：`ForceLayer` 下 `riscv::Rv32iPipeline`（**不是** SoC —— 避免跨模块 `ch_mem`
      提前拒绝）发射 `ok==true` + 产物过 lint ⇒ ⇒ 闭合 Why 的论证
- [ ] **A22 1-bit / `ch_bool`**：`logic` 声明形态（`logic` vs `logic [0:0]`）+ `==` / `|` 组合
      （真实动机：`rv32i_pipeline.h:398/401/402/506/524`）
- [ ] **A23 root-as-parent**：端点**已在父 module 本地可见**（inline 折叠 / 父自身端口）⇒ ⇒
      MUST NOT 造网（否则重复声明）⇒ 负向用例
- [ ] **A24 撞名诊断分家**：新路径撞名（`clashes_with_port`）MUST 报**自己的**诊断
      （MUST NOT 复用 `sibling wire … collides with an existing port` ⇒ 会**指错来源**）

## Capabilities

- **MODIFIED** `rtl-hierarchical-output`（唯一 Requirement 被修订 + 新增 1 Scenario）⇒ 走 OpenSpec change；
- ⚠️ **不新增** capability；**MUST NOT** 在 spec 里复述 ADR-066 的理由（spec 只承载可判定条款，
  ADR 承载决策与理由 ⇒ ⇒ 两份并存的规范必然漂移）

## Impact

- **产品代码**：`src/hierarchy_verilog.cpp`（主要）· 可能 `src/hierarchy_analysis.cpp`；
- **契约**：spec MODIFIED · ADR-060 §2.1 补格 · ADR-062 §5 反向指针；
- **测试**：矩阵格翻转（4 格）+ 新增用例 + 逐拍等价；
- **CI**：`CTEST_TOTAL` / `VERILATOR_E2E_TOTAL` 的增减 **以实际新增 CTest 条目为准**（若复用既有 target 则不变）；
- **下游**：行 17（语料登记表 + 过时刻固化断言）在本 change 落地后修订（归因已变）；
- **风险**：① 漏改一条操作数/穿透解析路径 ⇒ **lint 响亮失败或值错**（⚠️ 非「零告警」）⇒ A8 两个面各逐条变异；
  ② 落点网名用错（被驱动节点自体名）⇒ 自指 + 双驱动 ⇒ A2/A7 抓；
  ③ 矩阵格翻转是**预期可见变更** ⇒ ⇒ 若其它用例因格值变化转红 ⇒ MUST 逐条判定是「预期」还是「回归」。
