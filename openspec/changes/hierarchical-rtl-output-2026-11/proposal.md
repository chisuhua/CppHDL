# 提案：层次化 RTL 输出 —— 层级粒度由用户显式声明（默认 inline）

- **提出方**：★ **下游消费者**（ChipForge 的开放核工作）+ **上游用户诉求**（2026-11-02）
- **架构依据**：ADR-058 · ADR-059 · ADR-060 · ADR-061 · **ADR-062**（接口类型，决策正本）
- **规格影响**：★ **新建** 2 个 capability（见 §Capabilities）⇒ 归档**禁** `--skip-specs`；
  ⚠️ archive **不合并** `## Purpose` ⇒ 归档后 MUST 另行补写主 spec 的 `## Purpose`
- **规模 / 风险**：中（**C0–C8 共 10 段** ⇒ commit 数由实施者按「扁平产物可归因」切分，
  至少 **8 个**；⇒ 人日：C0 0.5 · C1 ~3 · C2 ~1.5 · C2.5 ~1 · C3 ~2 · C4 ~3 · C5 ~3 ·
  C6 **~7–9** · C7 ~1 · C8 ~1 ⇒ **≈ 23–25 人日**）
  · **零行为变更**（默认路径逐字节不变）

## Why

框架**只支持扁平输出**：`ch::toVerilog(filename, context*)` 只接收 context，固定输出
**一个** `module top`（`src/codegen_verilog.cpp:301, 409`），writer 内**零**
`children()` 遍历、**零**实例化语句发射 ⇒ ⇒ 想要层次化 RTL 只能「每层手写 wrapper +
手写顶层例化」⇒ ⇒ **手工量随层次深度线性增长**。

**下游的具体痛点（ChipForge）**：

| # | 痛点 | 证据 |
|---|---|---|
| ① | **层 2 IP 单元（`ch::Component`）需要成为独立 Verilog module**（可单独复用 / 交付 / 综合）⇒ 但 `PipeBuilder::to_verilog` 直接 `ch::toVerilog(filename, ctx_)` ⇒ **整个 ctx 变成 top** ⇒ 层 2 单元被**摊平**进顶层 ⇒ 无法单独交付 | ChipForge ADR-091 §D3a.8「层次化 = 采纳」· `include/cf/plugin/pipe_builder.h:464-467` |
| ② | 层 2 单元的装配点**无 active parent**（桥接 plugin 在 `setup()` 内 `make_unique` + `build(ctx)`）⇒ ⇒ `ch_module<T>` 不可用（`module.h:42-46` 早退）⇒ ⇒ **「成层声明」与「装配点」必须解耦** | ChipForge ADR-091 v1.15 §D3a.7 三层真实理由 ① |
| ③ | 下游按 **ctx 级节点名**解析观测点（`resolve_node_id(ctx, name)` 精确匹配）⇒ ⇒ 层次化 emitter **MUST NOT** 发明 per-module 命名 | `tools/parity_runner/parity_sim_runner.cpp:249-255` |

**上游自己的阻塞**（实测，证据见 `docs/architecture/2026-11-02-hierarchical-rtl-output.md` 附录 A）：
节点归属在**成员形态**下是**错的**（成员的 `ch_reg` / `ch_mem` / bundle 字段归到**父组件**）、
成员 `ch_module` 挂**祖父**、深度 ≥3 的实例路径**重复前缀**、成员 `ch_module`
因 `context()==nullptr` **直接早退** ⇒ ⇒ ⇒ 按归属切模块会把子模块状态划进父模块
⇒ ⇒ ⇒ 产出**看起来对、实际错**且**无任何告警**的 RTL。

**用户诉求（2026-11-02）**：「让用户在写代码时选择不创建新层次或创建新层次；
特别是**小的模块**，对用户来说**默认不创建新层级**，只对**大的模块**用户选择创建新层级。」
⇒ ⇒ ⇒ 若默认「每组件一层」，则 `chlib` 的 **27** 个对外组合子全部成层 ⇒ ⇒ ⇒
产物变成**例化清单的噪声**，用户真正关心的层被淹没。

## What Changes

### 1. 归属修正（ADR-059）—— 硬前置

- **构造作用域**：`Component` 的构造函数进入作用域、`build_internal()` 进入另一作用域；
  **节点归属与 `ch_module` 的 parent 统一取「最内层作用域的组件」**（配对作用域 ⇒ 无泄漏）。
- **`ch_module` 的 build context 取环境 context**（`ch::core::ctx_curr_`）⇒ ⇒ 成员形态的
  父组件在构造期 `context()==nullptr` 不再导致早退。
- `build_hierarchical_name()` 改用 `parent->name()` ⇒ 深度 ≥3 不再重复前缀。
- 新增 `local_name_` / `module_type_name()`。

### 2. 层级粒度由用户显式声明（ADR-061）

- **默认 inline**：未声明者**不产生 module / 实例化 / 隐式端口**。其端口落点按
  「**最近的成层祖先**」分两格：祖先 = **RTL 顶层** ⇒ 照扁平规则（无驱动的 `ch_in` ⇒
  顶层 `input`；任意 `ch_out` ⇒ 顶层 `output`）；祖先 = **非顶层成层 module** ⇒ `ch_out`
  是该 module **体内的普通信号**。
  ⇒ ⇒ 「全 inline 时祖先**必然**是 RTL 顶层」⇒ ⇒ 这就是「全 inline ⇒ 与扁平逐字节一致」
  **能够成立**的原因。
- **端口表收缩只属于成层子树**（无人消费的子输出不再上 RTL 顶）⇒ ⇒ 想顶层接口干净，
  MUST 显式声明成层。
- 类级声明 `CH_HIERARCHY_LAYER`；实例级覆盖用**两个新 API 名**
  `ch_layer<T>` / `ch_inline<T>`（**不动 `ch_module` 的签名** ⇒ 零破坏）。
- **成层判定与装配点解耦** ⇒ ⇒ 无 active parent 处装配的组件仍能按声明成层（下游 N2）。
- emitter 侧 `Granularity` 枚举（`RespectDeclarations` / `ForceInline` / `ForceLayer`）。

### 3. 层次化 emitter（ADR-058 / ADR-060）

- **模块边界 = 所有权边界**；**module 树 ≡ 成层组件子树**。
- **命名权威 = ctx 级全局表** ⇒ 顶层端口名与扁平产物**逐字相同** ⇒ 下游按名解析不受影响。
- **默认（全 inline）的层次化产物 MUST 与扁平产物逐字节一致** ⇒ 零回归。
- 跨模块非端口边 ⇒ **按方向**合成隐式端口（父→子 = 子侧 input；子→父 = 子侧 output；
  子端口由父层表达式驱动 ⇒ 图重写）；跨模块 `ch_mem` ⇒ **响亮拒绝**。
- 悬空子输入 ⇒ **向上冒泡**成顶层 input（复刻扁平行为）。
- `toVerilog()`（扁平）与 Verilator backend 既有入口**一律不动**；
  backend 允许新增一个**只增不改**的 `initialize_with_verilog(path, data_map)`。

### 3.1 单元接口类型（**ADR-062**）

- **`Payload<T>` 不可充当层次化单元的接口** —— 它是 host 侧 key-value（`PayloadStore` 的
  cell 是 `std::any`，store 由 `PipeNode` 持有）⇒ ⇒ **它既不产生端口、也不构成接口声明**
  ⇒ ⇒ 一个「只用 `Payload` 读写」的单元，其端口集合**由实现细节偶然决定**
  ⇒ ⇒ **不可交付、不可综合、不可复用**。（⚠️ **不要**说成「0 端口 module」：
  CH_MEM 模式下 cell 可以装 `ch_out` 节点 ⇒ 端口数可能 > 0。）
- 单元的接口 MUST 是**结构化端口**（`__io` 的 `ch_in` / `ch_out`）；且**成层**组件的
  **全部** `ch_in` / `ch_out` MUST 成为 module 端口（**不论是否被父层驱动**）
  ⇒ 父层用**实例端口映射**连接。
- 下游的「同一字段写四遍」（`__io` / `Ports` / `ObsPorts` / payload key）建议收敛为
  **单一声明派生**（ADR-062 决策④，下游可选）。
- 下游装配形态（`make_unique` + `build(ctx)`）MUST 补 `add_child`（或用 `ch_layer<T>`）。

### 4. 验证（ADR-058 §2.2）

- **映射表**（设计级信号名 ↔ 扁平端口名 ↔ 层次化 (module, port)）作为 emitter 的产出。
- **等价性判据**：对显式声明的观测集合 `O`（**非空**且**至少一个随激励变化**），
  `flat[O] ≡ hier[O]` **逐拍全等**；且 `hier.top_ports ⊆ flat.top_ports`。
- **判别力自证**：故意破坏层次化 ⇒ 等价性测试 MUST 转红。

## 范围外

- MUST NOT 改 `toVerilog()` 的签名或默认行为；
- MUST NOT 在 ctx 层自动改名；MUST NOT 引入 per-module 局部命名；
- MUST NOT 用启发式（端口数 / 节点数）**自动推断**成层；
- MUST NOT 让隐式端口静默通过（默认 MUST 有 `CHWARN` + 生成文件内注释）；
- MUST NOT 顺带改 `ch_logic_in` / `ch_logic_out` 的拷贝赋值语义（另一条独立提案）；
- 不做：参数化 module · 多时钟域 · 黑盒 IP 集成 · 层次化产物的综合/布局评估。

## Acceptance

- [ ] **归属**：Leaf 的 `ch_reg` 成员 `get_parent()` 是 Leaf；成员形态 `ch_module` 挂到
      直接父组件**且可 `io()`**；顶层 `ch_device<Pipeline>` 可实例化且不 SIGSEGV；
      作用域配平；归属断言用 `CHFATAL_EXCEPTION`（**MUST NOT** 用只打日志的 `CHREQUIRE`）
- [ ] **归属 ↔ 语义分流**：`type_input` 节点的指令表归属与 `include/core/io.h` 四处方向守卫的
      **触发面**与变更前**逐项一致**
- [ ] **粒度**：类级 / 实例级三种组合的成层结果与声明一致；**无 active parent** 处装配的组件
      仍能按声明成层；`ch_module` 签名零变更
- [ ] **零回归（命名阶段）**：`toVerilog()` 的扁平产物相对**归属修正之后**的基线逐字节不变；
      **文本变化面**逐条登记（≥ 4 处实测依赖）
- [ ] **结构**：三层设计产出 3 个 module + 2 条实例化语句；无 `assign` 跨 module 边界；
      **结构同构**成立；实例化端口名 ∈ 目标 module 端口集合；四类隐式端口方向各自有判据
- [ ] **等价性**：≥5 个 DUT 的「扁平 vs 层次」逐拍等价（观测集合非空且含变化信号）；
      **故意破坏 ⇒ 必须转红**；所有产物 `verilator --lint-only -Wall` 零 error；
      同一设计两次生成字节相同
- [ ] **零回归（全量）**：`ctest -LE perf` · `run_all_samples.sh` ·
      `run_all_ported_tests.sh` 全绿；计数锚点同步；新增护栏
      `scripts/verify_hierarchical_rtl.sh` 挂 CI 两处
- [ ] **文档**：用法指南 / usage guide §5 / 架构主题文档与实现一致（零漂移）
- [ ] 归档（**禁** `--skip-specs`；归档后 MUST 补写 2 个新 capability 的 `## Purpose`）

## Capabilities

- **新建** `rtl-hierarchical-output` ⇒ **ADDED ×12**
  ① 层级粒度 MUST 由**显式声明**决定（默认 inline）· ② 成层组件 MUST 形成独立 module 且
  module 树 ≡ 成层组件子树 · ③ 命名权威 MUST 是 ctx 级全局表（顶层端口名与扁平逐字一致）·
  ④ 默认全 inline MUST 与扁平逐字节一致 · ⑤ 跨层非端口边 MUST 按方向合成隐式端口（默认告警）·
  悬空输入 MUST 冒泡 · ⑥ 成层判定 MUST 与装配点解耦，但组件 MUST 登记进组件树 ·
  ⑦ `owner == nullptr` 的节点 MUST 归顶层 module ·
  ⑧ 层次化与扁平 MUST **等价**（映射表四元组 · 输入集合相等 · 输出集合 = 精确期望集 · 观测集合逐拍全等）·
  ⑨ 时序 module MUST 有 clock/reset（根无条件；**clock 与 reset 分开判** —— clock 含
  `ch_reg` / mem **写端口** / `sread`；reset 只有 `ch_reg` / `sread`）·
  ⑩ 隐式端口的实现 MUST **NOT** 改写 IR ·
  ⑪ **成层且非根** module 的端口 MUST **⊇** 其 `ch_in`/`ch_out`（⇒ 已被父层驱动的也 MUST 成端口）
  且每个端口可追溯到五类来源之一 ·
  ⑫ 单元体内 MUST NOT 引用 **`owner` 既不是自己、也不是自己祖先**的端口节点
  （⇒ 这是「值经 host 侧通道进来」的可机械判据）
- **新建** `component-ownership-and-naming` ⇒ **ADDED ×1**
  「节点归属 MUST 由**构造作用域**唯一决定（MUST NOT 依赖事后推断）；组件实例路径 MUST 无重复前缀」

## Impact

- **改动文件**：`include/component.h` · `include/module.h` · `include/core/lnodeimpl.h` ·
  `include/codegen_verilog.h` · `src/codegen_verilog.cpp` · `src/core/verilator_backend.cpp` ·
  新增 fixture / 护栏 · `openspec/specs/{rtl-hierarchical-output,component-ownership-and-naming}/`
- **爆炸半径**：**中**（触碰 `Component` 生命周期 + codegen 主路径）
  ⇒ ⇒ ⇒ MUST 按 **C0（spike）/ C1（归属+命名）/ C2（粒度）/ C2.5（接口与端口完整性）/
  C3（分析）/ C4（emitter 拆分）/ C5（层次发射）/ C6（等价性）/ C7（文档）/ C8（归档）**
  切 commit（≥ **8** 个）⇒ ⇒ ⇒ 「扁平产物逐字节不变」才可**归因**
- **行为变更**：**无**（默认路径字节不变；唯一变化是「原本无法产出的东西现在能产出」）
- **下游收益**：ChipForge 的层 2 IP 单元**可成层**（N1）· 装配点不再受限（N2）·
  顶层端口名不变 ⇒ 其 parity runner **零改动**（N7）
- **风险**：**中** —— 主风险是「归属修错方向」⇒ ⇒ ⇒ 由 ①b 的判据（归属↔语义分流回归）兜住

## Non-Goals

- 不做「每组件一层」的自动推断；
- 不做 per-module 局部命名；
- 不做 ctx 层自动改名；
- 不做层次化产物替换 Verilator backend 的既有输入路径。
