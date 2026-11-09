---
name: mem-port-arg-cross-layer-2026-11
priority: P1
phase: phase-6
category: framework
type: feature
状态: draft
主题: mem 端口实参跨层支持
依赖: ADR-066, ADR-060
来源: ADR-066 §2.7 决策 ⑦（已裁决 · 归下游独立立项）· 台账 §7 行 20（已裁决 · 归属=下游）· phase-6 P3（起草）/ P4（architect 范围裁定 · 前置）/ P5（实施）
生成时间: 2026-11-09
evidence: unmeasured
---

# Improvement: `ch_mem` 端口实参跨层 —— 分析层粒度拆分 + 归因订正（+ 条件化的正确支持）

- **Date drafted**: 2026-11-09
- **Drafted by**: planner 角色（build-agent）
- **Status**: 📝 **草案**（⚠️ **2026-11-09 更新**：architect 的 **P4** 范围裁定**已落地**
  （ADR-066 **v1.2** + ADR-060 **v1.17** · oracle → critic **0 BLOCKER**）⇒ ⇒ 但 **P4 未裁定「支持」**
  ⇒ ⇒ 本草案的**无条件部分**（粒度拆分 + 归因订正 + 归属比对 + V20 探针）**可立项**；
  **条件化部分**（正确支持 · 产物形态）**仍未开启** ⇒ ⇒ 转 `approved` 前 MUST 由用户/architect
  决定：**只立无条件部分**（推荐）还是**同时开第二次裁定**）
- **权威决策**：`docs/adr/ADR-066-hierarchical-parent-side-landing-semantics.md` §2.7 **决策 ⑦**（**Accepted**）
  ⇒ ⇒ 本 improvement **MUST NOT** 改写 ADR-066 的决策内容；它只**执行**决策 ⑦ 的处置
  （「归下游独立立项：分析层 `:543` 的粒度拆分 + 归因订正 + **可选的**正确支持」）
- **上游台账**：台账 §7 **行 20**（`已裁决` · 归属 = **下游**）· 登记表 `.rddf/checks/arch-residual-registry.md`
  行 20「同款例外」条目（`已裁决` / 归属 `下游`）

**主题**: mem 端口实参跨层支持

## 架构依据

### 决策 ⑦ 的处置（本 improvement 的授权来源）

> ADR-066 §2.7：父层 `ch_mem` 的读/写端口，其 `addr` / `en` / `wdata` **实参**取自成层子组件的
> `ch_out` 时 ⇒ ⇒ 该形态 **MUST NOT** 按本 ADR 的「父侧落点网」处理；分析层 `:543` 的
> **跨 module `ch_mem` 闸门** MUST **维持**响亮拒绝。
> ⇒ ⇒ **处置**：台账行 20 改记「已裁决（不纳入本 ADR）」⇒ ⇒ **归下游独立立项**
> （分析层 `:543` 的**粒度拆分** + **归因订正** + **可选的**正确支持）⇒ ⇒
> 本 ADR **MUST NOT** 顺带放宽决策 ⑥ 的「跨模块 `ch_mem` 仍拒」。

⇒ ⇒ 本 improvement = 该「下游独立立项」的**规划草案**（**不是**实施；实施 = phase-6 **P5**）。

### 现状（**运行期实测**，ADR-066 §2.7 · 2026-11-08）

| # | 事实 | 证据 |
|---|---|---|
| ① | 分析层 `:542-553` 的跨 module `ch_mem` 闸门**先于**父子关系判定 ⇒ 「子 `ch_out` → mem 端口实参」被**响亮拒绝** | `src/hierarchy_analysis.cpp:542-553`（`CHFATAL_EXCEPTION("cross-module ch_mem access …")`） |
| ② | `is_mem_node()` **含** mem 读/写**端口**类型 ⇒ 「子 `ch_out` → mem 端口」**确实**是一条跨 module 边 | `src/hierarchy_analysis.cpp:43-47` |
| ③ | mem 端口的 `addr` / `en` / `wdata` 是端口的**直接 `src`** ⇒ 实参视线**就是**那条跨 module 边 | `include/ast/mem_port_impl.h:39-49,141` |
| ④ | ⇒ `print_mem_read_port` 的 early-return（`!node_names_.count(addr)` 等）**不可达** ⇒ ⇒ **台账行 20 原描述「静默不发射」已被证伪** | `src/codegen_verilog.cpp:1266-1270` |
| ⑤ | ⇒ 现状 = **响亮拒绝 + 归因错误**：诊断让用户「把 mem 与使用者放同层」，而 mem **本来**就在**同层**（父持 mem + 子 `ch_out` 驱动地址） | 同 ① 的诊断文案 |
| ⑥ | **契约层早已规定**该形态的产物规范 ⇒ ⇒ 拒绝使该规范**永不可达** | ADR-066 §3 选项 A 理由 ② |
| ⑦ | 仓内**公共库自己**在用该形态（**7 处**）⇒ ⇒ 「不支持」= 库自己的写法不被自己的特性支持 | ADR-066 §3 选项 A 理由 ①（事实 ⑫）· 典型形态 = `include/cpu/pipeline/rv32i_pipeline.h:194-197`（`reg_file.aread(id_stage.io().rs1_addr_id, "reg_file_rs1")` ⇒ 地址来自子 `ch_out`） |

⇒ ⇒ **一句话**：闸门把**两件不同的事**混成一件 ——
(i) **真跨模块访问存储体**（mem **本体**在别的 module ⇒ ADR-060 §2.1 第四行 ⇒ **必须拒**）；
(ii) **mem 端口的实参视线跨层**（mem **本体**就在父层，只是 `addr`/`en`/`wdata` 来自子的 `ch_out`
⇒ ⇒ 这**不是**跨模块访问存储体）⇒ ⇒ 归因必须按 (i)/(ii) 拆开。

## 范围

- **In Scope**：
  1. **分析层粒度拆分**（`src/hierarchy_analysis.cpp` `:542-553`）：把单条件闸门拆成两类判定 ——
     (i) mem **本体**跨 module ⇒ **维持**响亮拒绝（`cross-module ch_mem access`）；
     (ii) mem 端口**实参视线**跨层（mem 本体与使用者同层）⇒ **不**再被那条诊断拦下；
  2. **归因订正**：当处置是「拒绝」时，诊断 MUST 给出**正确改法**；
     MUST NOT 再出现「把 mem 与使用者放同层」（当 mem 本体**已经在**同层时）；
  3. **条件化的正确支持**（★ **仅当另有裁定为「支持」时**才实施 —— ⚠️ **2026-11-09 更新**：
     ADR-066 **v1.2** 明写「本 ADR **不裁定** (ii) 支持态的**产物形态** ⇒ MUST 走**独立修订**」
     ⇒ ⇒ **P4 并未裁定「支持」** ⇒ ⇒ 本项**仍未开启**；且产物形态**不得**由本草案预定）；
     mem 端口实参走**父侧落点网**（⚠️ = **候选形态**（**待独立裁定**）· 另一种候选 = `addr` 实参
     **复用** ADR-066 决策 ① 的父侧网机制（网落在**实参**上）· **不禁止**但**未裁定**）
     （网名 = 驱动端点的 ctx 全局名 ⇒ 与 ADR-066 决策 ① 的命名规则同源）⇒ ⇒ 端口**读值非 0**；
  4. **契约同步**：**spec 侧**（归 **P5 / builder**）—— `openspec/specs/rtl-hierarchical-output/spec.md`
     的 Requirement（MODIFIED 或 ADDED · 取决于 P4 的边界句是否足够独立）· 归档**禁** `--skip-specs`；
     ⚠️ **ADR 侧**（归 **P4 / architect**）—— `docs/adr/ADR-060-*.md` §2.1 矩阵**补格**
     （**逐字**含 P4 的边界句）⇒ ⇒ **builder MUST NOT** 改已签批 ADR 的决策内容（ADR-060 已 Accepted）
     ⇒ ⇒ 判据 = P4 的**双向 grep**（见 A8）；
  5. **测试 + 护栏**：正/负向用例 + **值判据**（读值非 0，不是只 lint）+ 产物级护栏与 `--self-test`。

- **Out Scope**（**MUST NOT** 静默即兴）：
  1. **真跨模块 `ch_mem` 访问**（mem 本体在子层、父层去读）⇒ **仍拒**（ADR-060 §2.1 第四行 ·
     ADR-066 决策 ⑥ **MUST NOT** 顺带放宽）；
  2. **跨多层（孙组件）**的同形态（ADR-066 决策 ⑥ 的范围边界）；
  3. **`PortPolicy` 三档分化**（台账行 15 · phase-6 **P7** 独立立项）—— 本 change **不碰**该维度；
  4. **P4 本身的 architect 范围裁定** —— 那是**前置**，不是本 improvement 的产出；
  5. `ch_mem` **本体**的跨层归属（是否允许 mem 与使用者不同层）—— 那是 (i)，**保持现状**。

## Why

1. **现状把用户引向错误的改法**（事实 ⑤）⇒ ⇒ 用户照诊断改（把 mem 挪进子 module）⇒ ⇒
   反而**破坏** ADR-060 §2.1 的「mem MUST 与使用者同层」⇒ ⇒ 一次「响亮拒绝」变成**两次**错误。
2. **契约层已有的产物规范永不可达**（事实 ⑥）⇒ ⇒ 契约与实现之间有一条**静默的债**：
   规范写了，实现不做，且**没有任何判据**指出它没做。
3. **库自己的写法不被自己的特性支持**（事实 ⑦ · 7 处）⇒ ⇒ 公共库（含 riscv-mini 的寄存器堆读点）
   在层次化发射下**必然撞墙** ⇒ ⇒ riscv-mini SoC 的成层发射**无法推进**。
4. **与已落地的决策 ① 方向一致**：ADR-066 决策 ① 已把「父侧消费子 `ch_out`」判为**合法且正确**
   ⇒ ⇒ 而 mem 端口实参是同一族里的**第三类消费者** ⇒ ⇒ 留在「一律拒绝」= 同一维度上**自相矛盾**
   （族 ①/③ 支持 · 族「mem 端口」拒绝，而三者的共同成因都是「父侧带运算/带实参」）。

## What Changes

1. **分析层**（`src/hierarchy_analysis.cpp`）
   - 把 `:542-553` 的单条件闸门拆成**按「mem 本体所在 module」vs「端口实参来源」**的两类判定；
   - 保留 (i) 的 `CHFATAL_EXCEPTION("cross-module ch_mem access …")` **原文案**（它对 (i) 仍是真的）
     ⇒ ⇒ ⚠️ 归因订正**只作用于 (ii)**（这正是 ADR-066 §2.7 的「粒度拆分」）；
   - (ii) 的处置**由 P4 的边界句决定**（见「Acceptance」A3/A4）。
2. **发射层**（`src/hierarchy_verilog.cpp` / `src/codegen_verilog.cpp`）
   - **若另有裁定为支持**：mem 端口的 `addr`/`en`/`wdata` 走**父侧落点网**（= **候选形态** · **待裁定**）（复用既有「网登记 +
     `.port(net)` + 声明 + 撞名检查」通路）⇒ ⇒ ⚠️ **`print_mem_read_port` 的
     `node_names_.count(addr)` 判据 MUST 成立** —— 否则又会退化成「静默不发射」（事实 ④ 的同型）；
   - **若 P4 裁定为不支持**：本项**零改动**（只做归因订正）。
3. **诊断**：区分 (i)/(ii) 两类拒绝并各自给出**可执行的改法**；(ii) 的改法 MUST 与 P4 的边界句一致。
4. **契约**：`rtl-hierarchical-output` capability 的 Requirement + ADR-060 §2.1 矩阵补格
   （**逐字**含边界句 ⇒ ⇒ grep 可判）。
5. **护栏**：产物级判据扩展（或新规则）+ `--self-test` 投毒 ⇒ ⇒ 「lint 绿」**不得**是唯一判据
   （事实 ④ 的教训：lint 绿 + 早退 = 产物合法但功能缺失）。

## Acceptance

- [ ] **A1（粒度拆分可判）** 分析层 MUST 能区分「mem **本体**跨 module」与「mem 端口**实参视线**跨层」
      ⇒ ⇒ 两类的处置 MUST 不同：前者**维持**响亮拒绝（错误串含 `cross-module ch_mem access`）·
      后者**不**再被那条诊断拦下 ⇒ 判据 = **正/负向用例各一**（负向 = 本体跨 module ⇒ MUST 仍红）。
- [ ] **A2（归因正确）** 当处置是「拒绝」时，诊断 MUST 给出**正确改法**；MUST NOT 出现
      「把 mem 与使用者放同层」这类**错误改法**（当 mem 本体**已经在**同层时）
      ⇒ ⇒ 判据 = 诊断文本断言 **+ 反向 grep**（该文案在「mem 本体跨 module」这一支**仍合法** ⇒
      MUST NOT 被一刀切删除）。
- [ ] **A3（P4 边界句前置）** MUST 先有 **architect 的范围裁定**（ADR 修订 · **oracle → critic** 双审
      **0 BLOCKER**），其中**逐字**写死边界句：「**mem 持有 module == 使用 module**、
      仅**实参视线**跨层」⇒ ⇒ 实施 MUST 逐字遵守 ⇒ ⇒ MUST NOT 顺带放宽 ADR-066 决策 ⑥。
- [ ] **A4（支持态的值判据）** **若另有裁定为「支持」**：mem 端口 MUST 走**已裁定**的产物形态
      （⚠️ = **候选**：「父侧落点网」**或**「`addr` 实参复用决策 ① 的网机制」⇒ ⇒ **待独立裁定** ⇒ ⇒
      MUST NOT 由本草案预定）⇒ ⇒
      **端口值 MUST == 手推期望值**（读 `addr` 与写 `wdata`/`en` **两侧**）—— ⚠️ 「非 0」只是**下界**
      ⇒ ⇒ 错值只要非 0 即过 ⇒ ⇒ MUST 断言**确定值**（**不是**只 lint 绿 —— 事实 ④ 的同型陷阱）
      ⇒ ⇒ 反向：真跨模块访问 MUST **仍被拒**（A1 的负向用例同时守住）。
- [ ] **A5（契约同步 · spec 侧 · P5 归属）** capability spec（`rtl-hierarchical-output`）MUST 有对应
      Requirement（MODIFIED 或 ADDED）⇒ ⇒ 判据 = **grep**（独立可 grep 的证据）· 归档**禁** `--skip-specs`
      （除非本 change 显式声明无 spec 变更）。
- [ ] **A6（判别力自证）** A1/A2/A4 各 MUST 有**至少一条变异**（改坏即转红）⇒ ⇒ 且每条变异
      MUST **先确认仍能编译**（编译失败 ⇒ 跑的是**旧二进制** ⇒ 假绿）⇒ ⇒ 无效变异 MUST 如实登记。
- [ ] **A7（零回归 + 锚点同步）** `ctest -LE perf` 全绿 · `scripts/verify_test_counts.sh` 绿
      （若新增 ctest 条目 ⇒ MUST 同步 `CTEST_TOTAL` / `VERILATOR_E2E_TOTAL` 等锚点）·
      `scripts/verify_hierarchical_rtl.sh`（含 `--self-test`）绿。
- [ ] **A8（ADR 矩阵补格 · P4 归属）** `ADR-060 §2.1` 矩阵的**补格**（**逐字**含 A3 的边界句）
      ⇒ ⇒ **归属 P4（architect）** ⇒ ⇒ 本 improvement 后续的 **builder 实施 MUST NOT** 改 ADR-060
      ⇒ ⇒ 判据 = A3 的**双向 grep**（**markdown 不敏感 + 限定 §2.7 区间**）：
      正 `awk '/^### 2.7/,/^---/' ADR-066 | grep -c '实参视线'` ≥ 1；
      负 `awk '/^### 2.7/,/^---/' ADR-066 | grep -c '维持.*响亮拒绝'` == 0
      ⇒ ⇒ ⚠️ **裸 `grep 'MUST 维持响亮拒绝'` 恒为 0**（旧句带 `**` 强调符）⇒ ⇒ 会**恒真**（判据失效）。
- [ ] **A9（边界清单 · MUST 覆盖）** 实施 MUST 覆盖以下边界（缺一 ⇒ ⇒ 该形态仍属「未测」）：
      ① **孙组件**（跨两层）MUST **仍被拒**（拆分 MUST NOT 顺带放宽 ADR-066 决策 ⑥）；
      ② mem **本体**与使用者同 module、但 `addr` 取自**父层自身**节点（非跨层）⇒ MUST **不误报**；
      ③ 读端口跨层 + 写端口**不**跨层的混合形态；
      ④ 多个 `ch_mem` 实例共存；
      ⑤ `aread`（0-cycle）与 `sread`（1-cycle）**两类**端口都要覆盖
      （⚠️ 二者在 TB 驱动下**不可判别** ⇒ ⇒ 判据 MUST 落在**值**上，不是「tick 之间观测」）；
      ⑥ **lint 绿 ≠ 值对** ⇒ ⇒ 值判据不可省（A4）。

## Capabilities

- **契约落点**：`rtl-hierarchical-output`（MODIFIED 或 ADDED）
  - 若 P4 的边界句**足够独立** ⇒ 新建 capability `hierarchical-mem-port-arg-cross-layer`
    （**由 P4 裁定决定**，本 improvement **不预判**）。
- **判别力底座**（**不是**本 change 的产出，是它的验收工具）：
  `tests/test_hierarchy_verilog.cpp` 的 **42 格 probe matrix**（`row2_expr` 轴已存在 ⇒ mem 端口
  这一格可挂在同轴上）· `scripts/verify_hierarchical_rtl.sh` 的 **R9**（父侧落点产物 lint 门）
  ⇒ ⇒ 新形态 MUST **进 R9 的 lint 集合**，且 MUST 配**值判据**（A4）。

## Impact

### 对用户

- **今天**：父层 `ch_mem` 端口的实参取自子 `ch_out` ⇒ **响亮拒绝** + **错误改法** ⇒ 用户照改会
  把 mem 挪进子 ⇒ 反而破坏「mem 与使用者同层」⇒ ⇒ **一次拒绝 = 两次错误**。
- **之后（若 P4 裁定为支持）**：该形态**可用**且**产物规范**与契约层早已写下的那一份**一致**
  ⇒ ⇒ 公共库的 7 处**零改写** ⇒ ⇒ riscv-mini SoC 的成层发射少一堵墙。
- **之后（若 P4 裁定为不支持）**：至少**归因正确** ⇒ 用户拿到**真改法**，且契约层那条
  「永不可达」的规范 MUST 同步订正（否则契约继续说谎）。

### 对仓内

- **受影响面**：`src/hierarchy_analysis.cpp`（一处闸门）· `src/hierarchy_verilog.cpp` /
  `src/codegen_verilog.cpp`（**仅当**支持）· `openspec/specs/rtl-hierarchical-output/spec.md` ·
  `docs/adr/ADR-060-*.md` §2.1 · `tests/test_hierarchy_verilog.cpp` · `scripts/verify_hierarchical_rtl.sh`。
- **不受影响**：扁平 codegen 路径（`toVerilog`）**零改动** ⇒ ⇒ 既有扁平产物**逐字节不变**；
  interpreter / JIT 路径零改动。

### 风险

| # | 风险 | 抓手 |
|---|---|---|
| 1 | 粒度拆分写错 ⇒ **真跨模块访问被误放行**（那是 ADR-060 §2.1 第四行明令拒绝的） | A1 的**负向用例** + A6 变异 |
| 2 | 支持形态下 `print_mem_read_port` 的 `node_names_.count(...)` 判据**不成立** ⇒ ⇒ 退化成「静默不发射」（事实 ④ 的同型） | A4 的**值判据**（读值非 0）· **不是** lint |
| 3 | 归因订正时**误删**「把 mem 与使用者放同层」（该文案对 (i) 仍是真的） | A2 的**反向 grep** |
| 4 | 借本 change **顺带放宽** ADR-066 决策 ⑥ | A3 的边界句 + ADR 修订的**取代流程** |

### 与 phase-6 其它步的关系

| 关系 | 对象 | 说明 |
|---|---|---|
| **前置** | **P4**（architect） | MUST 先落边界句（ADR 修订 · oracle → critic 0 BLOCKER）⇒ 本 improvement 才能转 `approved` |
| **下游** | **P5**（builder） | 实施 = 粒度拆分 + 归因订正 + （条件化）支持 + A15 型值判据 + 台账行 20 闭口 |
| **独立** | **P7**（`PortPolicy` 三档分化 · 台账行 15） | 不得混入本 change —— 那是**另一个维度**的分化 |
| **无关** | **P1**（A8 面 ① 机械证据） | 已完成（`6842a80`）· 本 change **MUST NOT** 顺带改 `codegen_verilog.cpp` 的 62 个取址点 |
