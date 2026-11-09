# Proposal: `ch_mem` 端口实参跨层 —— 分析层粒度拆分 + 归属比对 + `(ii)` 的正确支持

> **improvement 来源**：`.rddf/improvements/mem-port-arg-cross-layer-2026-11.md`（**approved** · 用户 2026-11-09 拍板）
> **架构权威**：
> - `docs/adr/ADR-066-...md` **v1.2**（决策 ⑦ = **边界句** + 三桶 + **节点级判别程序** + §5 **V18/V19/V20**）
> - `docs/adr/ADR-066-...md` **v1.3**（决策 ⑧ = `(ii)` 的**产物形态**：复用决策 ① 的落点网（**由登记触发**）·
>   **命名恒等** ⇒ codegen 不查表 · R2 **+1 行 = 登记义务** · `en` 槽 fail-closed · §5 **V21/V22/V23**）
> - ⚠️ `docs/adr/ADR-066-...md` **v1.4**（**事实订正** · 2026-11-09 · **决策零变更**）：§2.7 机制订正段
>   （字面 (i) 的**现状 = 已响亮拒绝** ⇒ ⇒ **无**契约-实现缺口）+ 判别程序 ③ 的**职责界定**
>   + §5 **V20** 的**预期值** ⇒ ⇒ **实施前必读**
> - `docs/adr/ADR-060-...md` **v1.17 / v1.20**（§2.1 矩阵补格 + 行 96 处置改指决策 ⑧ + **R2 表第 5 行** + R1 防御性注记）
> - ⚠️ `docs/adr/ADR-060-...md` **v1.21**（**事实订正** · 2026-11-09 · **决策零变更**）：§2.1 的
>   「现状实现缺口（v1.17）」段订正 + 「mem 端口实参跨层」行的**实测注记**
> **台账**：`docs/architecture/2026-11-02-hierarchical-rtl-output.md` §7 **行 20**（`已裁决` · 归属 `下游`）
> ⇒ ⇒ 本 change = 该行的**闭口载体**

## Why

分析层 `src/hierarchy_analysis.cpp:542-553` 的**跨 module `ch_mem` 闸门**把**两件不同的事**压成一条：

- **(i) mem 本体跨 module**（父层读 `child.mem.*`）⇒ **必须拒**（隐藏共享状态）；
- **(ii) mem 本体与使用者同 module**、仅 `addr` / `en` / `wdata` 的**实参视线**跨层
  ⇒ ⇒ **不是**跨模块访问存储体 ⇒ ⇒ 现状被误拒，且**归因错误**（诊断让用户「把 mem 与使用者放同层」，
  而 mem **本来**就在同层）。

⇒ 仓内**公共库自己**在用 `(ii)`（`include/cpu/pipeline/rv32i_pipeline.h` 的 `reg_file.aread(id_stage.io()…)`
等 **3 处** + 同类写法）⇒ ⇒ 「不支持」= 库自己的写法不被自己的特性支持。
⇒ ⇒ 架构面已由 **ADR-066 v1.2 + v1.3** 裁定（边界 + 产物形态）⇒ ⇒ 本 change = **实施**。

## What Changes

1. **分析层粒度拆分**（`:542-553`）：三桶判定 —— **(i)** ⇒ 维持响亮拒绝 · **(ii)** ⇒ 不再被该闸门拦下 ·
   **(iii)** 非相邻实参（孙 / 兄弟）⇒ 仍按 `:563` 相邻性规则拒绝。
2. **归属比对判定（新增 · ADR-066 v1.2 判别程序）**：**持有** = `port->parent()` 本体的 **inline 折叠后归属** ·
   **使用** = mem 端口节点自身的折叠后归属 ⇒ `(i) ⇔ ①≠②` · `(ii) ⇔ ①==② + 跨 module src 仅落在
   `addr`/`en`/`wdata` 三槽 + **相邻**`（⚠️ **2026-11-09 实测订正**（ADR-066 **v1.4**）：现行边闸门对字面 (i) **可达**（mem **端口**节点的 src 列表**含 mem 本体** ⇒ ⇒ 该边跨 module ⇒ ⇒ **现状 = 已响亮拒绝** ⇒ ⇒ **无**契约-实现缺口）⇒ ⇒ 该判定的价值 = **独立于边结构** + **归因精确** + **三桶可判** ⇒ ⇒ **保留**它（否则判别程序 ①②③ 落空））。
3. **登记义务（ADR-066 v1.3 ⑧①③）**：`(ii)` 的实参跨层边 MUST **登记为父侧消费边** ⇒ 由决策 ① 的机制
   合成父侧 `logic` 网（复用 `register_parent_wire` / `parent_landing_wire` / `add_conn`）
   ⇒ ⇒ **MUST NOT** 让父 module 出现 **implicit wire**（1-bit ⇒ 宽度截断 ⇒ 值错）。
   ⚠️ **2026-11-09 实测订正**（ADR-066 **v1.4**）：该登记**由既有机制自动满足**（实测产物里父 module
   **已有** `logic [3:0] <驱动端点全局名>;` 声明 + 子 output `.port(net)` + mem 端口读该名
   ⇒ ⇒ **命名恒等** ⇒ ⇒ codegen 侧确实无需查表）⇒ ⇒ ★ **本项不需要写产品代码** ⇒ ⇒ 交付物 =
   **产物级断言**（AC4：产物内该名字 MUST 有 `logic` 声明 ⇒ 非 implicit wire）。
4. **`en` 槽 fail-closed（构建 ⑧⑥ + ADR-060 R2 第 5 行）**：`enable == nullptr` ⇒ `1'b1`（合法缺省）·
   `enable != nullptr` 且**取不到名字** ⇒ **fail-closed 诊断**（**MUST NOT** 回退 `1'b1`）。
5. **归因订正**：被拒时的诊断 MUST 给出**正确改法**；MUST NOT 再出现「把 mem 与使用者放同层」
   （当 mem 本体**已在**同层时）。
6. **运行期探针（V20 · P4 遗留）**：登记**字面 (i)** 的现状处置 —— ⚠️ **2026-11-09 实测订正**
   （ADR-066 **v1.4**）：**预期 = 分析层**已**响亮拒绝**（**不是**「分析放行 + 发射期静默丢端口」
   ⇒ ⇒ **无**契约-实现缺口）⇒ ⇒ ⚠️ **禁止**把「现状 = 已拒绝」表述为「新增了拒绝能力」
   ⇒ ⇒ 结果 MUST 登记进 **台账行 20** + `.rddf/checks/arch-residual-registry.md` 的**归因描述**
   ⇒ ⇒ ★ 探针的**实质交付物** = **(i) 与 (ii) 的归因分开**（(i) 说「mem 本体跨 module」·
   (ii) 说「仅实参视线跨层」）⇒ ⇒ 且 MUST 确认新判定**不放松**该拒绝（负向用例**仍红**）。
7. **代码注释同步**：分析层 `:542`（权威链**首环**）MUST 随判定拆分改（分 (i)/(ii)/(iii)）。
8. **契约同步**：capability `rtl-hierarchical-output` 新增 / 修改 Requirement
   （`(ii)` 的**判定 + 产物形态 + 地址稳定性无关**）⇒ ⇒ 归档**禁** `--skip-specs`。

## 实施顺序（**planner 的实施规划** · TDD · 每步可独立验证）

| 步 | 内容 | 验证（机械） |
|---|---|---|
| **S0** | **基线**：`ctest -LE perf` 全绿 + 记录 `CTEST_TOTAL` / `VERILATOR_E2E_TOTAL` | 两条锚点 |
| **S1** | **探针先行（V20）**：字面 (i) 的运行期探针（现状登记）⇒ ⇒ 先**测**再改 | 探针输出显式 PASS/FAIL（登记 = 台账行 20 + 登记表归因） |
| **S2** | **判别程序 + 归属比对**（三桶）⇒ ⇒ **负向用例先红**（(i) / (iii) 必须仍拒） | 负向用例转红 ⇒ 改后仍红 |
| **S3** | **(ii) 放行 + 登记义务**（落点网）⇒ ⇒ **值判据（V21）先红后绿**（读 `addr` / 写 `wdata`·`en` 两侧**精确期望值**） | 值判据 + lint |
| **S4** | **`en` 槽 fail-closed** + **归因订正** + **代码注释同步** | 诊断断言（语义要点）+ 负向 grep |
| **S5** | **V22 变异**（不登记落点网 ⇒ 值判据转红）+ **V18 复算**（§2.7 双向 grep） | 变异 **先编译确认** · 还原**重建** |
| **S6** | **spec delta + 归档**（禁 `--skip-specs`）+ 台账行 20 闭口（`已裁决 → 已收口` · 归属 `—`） | `verify_arch_ledger_residuals` / `verify_openspec_delta` / `verify_hierarchical_rtl` |

## 验收标准（AC · 逐条映射到架构判据）

- [ ] **AC1（三桶）** `(i)` 仍**响亮拒绝** · `(ii)` **不再**被该闸门拦下 · `(iii)`（非相邻）仍拒
      ⇒ ⇒ 映射 **ADR-066 §2.7 三桶** · 判据 = 正/负向用例各一（负向 = (i)/(iii) 仍红）
      ⇒ ⇒ ⚠️ **2026-11-09 metis 订正（判别力）**：既有 `tests/test_hierarchy_analysis.cpp:666`
      只断言 `find("cross-module ch_mem")` ⇒ ⇒ **pre-scan 消息与边闸门消息都含该子串**
      ⇒ ⇒ **改前改后都绿 · 归因改错也绿** ⇒ ⇒ **零判别力** ⇒ ⇒ MUST **强化**：断言
      **持有 module 名** + **使用 module 名** + **不含**同层提示 ⇒ ⇒ **不得**只断言子串
- [ ] **AC2（归属比对）** `(i)/(ii)` 的判别 MUST 用**节点级判别程序**（持有 = `port->parent()` 折叠后归属 ·
      使用 = 端口自身归属）⇒ ⇒ **字面 (i) 的现状登记 = V20 探针** ⇒ ⇒ 映射 **V18/V20**
- [ ] **AC3（值判据）** `(ii)` 支持态 ⇒ 读 `addr` 与写 `wdata`/`en` **两侧** MUST 断言**精确期望值**
      （**非 0 只是下界**）⇒ ⇒ 映射 **V21** · 且 **lint 绿 ≠ 值对**
- [ ] **AC4（无 implicit wire）** 产物内 mem 实参引用的名字 MUST 在父 module **有 `logic` 声明**
      （⇒ 否则 = implicit wire 1-bit ⇒ 宽度截断）⇒ ⇒ 映射 **⑧②/⑧③**
- [ ] **AC5（`en` 槽）** `nullptr` ⇒ `1'b1` · 非 `nullptr` 且无名 ⇒ **fail-closed**（MUST NOT 回退）
      ⇒ ⇒ 映射 **⑧⑥ + ADR-060 R2 第 5 行**
- [ ] **AC6（归因订正）** 拒绝诊断 MUST **按分支**给**正确**归因 ⇒ ⇒ **判据（⚠️ 2026-11-09 metis
      订正：原「不含中文串『把 mem 与使用者放同层』」= **恒真** —— 该中文串全仓为 0 ⇒ ⇒ MUST 改判据）**：
      ① **(i) 支**（mem 本体跨 module）⇒ ⇒ 诊断**MUST** 含「mem 本体与使用者 MUST 同 module」
      的语义（**英文原文** = `memory and its user to be in the same module`）⇒ ⇒ **合法保留**；
      ② **(ii)/(iii) 支**（mem 本体与使用者**已在**同层 · 只是实参视线跨层 / 非相邻）⇒ ⇒
      诊断 **MUST NOT** 含该句 ⇒ ⇒ **MUST NOT** 一刀切删除 ①（否则 (i) 失去正确归因）
      ⇒ ⇒ 判据 = **消息级断言**（两个用例各断言**自己那条** message 的要点 ⇒ ⇒ 禁全文件 grep）
- [ ] **AC7（变异判别力 V22）** 移除「登记落点网」⇒ ⇒ 产物 MUST 转红（值判据 / lint）
      ⇒ ⇒ 变异 MUST **先确认仍能编译** · **还原后 MUST 重建**
- [ ] **AC8（边界清单 · improvement A9）** 孙组件仍拒 · 父层自身节点不误报 · 读写不同侧 ·
      多 `ch_mem` · `aread`/`sread` 两类 · lint 绿≠值对 ⇒ ⇒ 各有用例
      ⇒ ⇒ ⚠️ **2026-11-09 metis 订正（两条 MUST 补）**：① **新 DUT MUST 登记**进
      `tests/hier_corpus_registry.md` + 新增结构类（root 持 `ch_mem` + mem 端口实参跨层）
      ⇒ ⇒ 否则 `verify_hier_corpus_coverage.sh` 的三向核对会**转红**；
      ② **决策 ⑤ 共网用例**：同一子 `ch_out` **既**直通父 output **又**作 `(ii)` 实参
      ⇒ ⇒ 断言产物里**只有一根** `logic` 网（先例 = `ImmConsistencyDut` · `hier_corpus_registry.md:31`）
- [ ] **AC9（契约）** capability `rtl-hierarchical-output` 的 Requirement 落地 + `verify_openspec_delta` 绿
      + 归档**禁** `--skip-specs`
- [ ] **AC10（零回归 + 锚点）** `ctest -LE perf` 全绿 · `verify_test_counts.sh` 绿 ·
      `verify_hierarchical_rtl.sh`（含 `--self-test`）绿 · 台账行 20 闭口 + 护栏绿
      ⇒ ⇒ ⚠️ **2026-11-09 metis 订正**：锚点 MUST 是 **4 个**（不是 2 个）且 MUST 写**期望 delta**
      （`k = 0` 的地方也 MUST 显式写 `+0` ⇒ ⇒ 否则「全绿」检不出**静默缩面**）：
      `CTEST_TOTAL 255 + K` · `VERILATOR_E2E_TOTAL 77 + k1` · `DUAL_BACKEND_PARITY_TOTAL 66 + k2`
      · `DUAL_BACKEND_PARITY_PAIRS 289 + k3` ⇒ ⇒ 命令 = `verify_test_counts.sh` +
      `verify_dual_backend_parity.sh`
      ⇒ ⇒ **护栏清单** MUST 含：`verify_hierarchical_rtl.sh` · `verify_hierarchical_port_naming.sh`
      · `verify_hier_corpus_coverage.sh` · `verify_openspec_delta.sh` ·
      `verify_arch_ledger_residuals.sh` ·（若动公开头）`verify_public_api_visibility.sh`

## Capabilities

- **MODIFIED**：`rtl-hierarchical-output`（`(ii)` 的判定 + 产物形态）；
- **ADDED**：`rtl-hierarchical-output`（`(ii)` 的「实参落点网 + `en` fail-closed」契约）。

## Impact

- **产品代码**：`src/hierarchy_analysis.cpp`（拆分 + 归属比对 + 注释）· `src/hierarchy_verilog.cpp`（登记）·
  `src/codegen_verilog.cpp`（`en` 槽 fail-closed）⇒ ⇒ **无**公开 API 变更（⇒ ⇒ **不走** P14 登记）；
- **台账**：`.rddf/checks/arch-residual-registry.md` **行 20**（`已裁决 → 已收口`）+ 台账 §7 派生；
- **计数**：新增 ctest target（探针/用例）⇒ ⇒ **MUST** 同步 `CTEST_TOTAL` 锚点；
- **风险**：
  - ⚠️ **(i)/(ii) 判反 ⇒ 真跨模块 mem 访问被静默放行**（比误拒严重）⇒ 缓解 = AC1 负向用例 + AC2 判别程序 + AC7 变异；
  - ⚠️ **`wdata`（写侧）漏登记** ⇒ 读对写错 ⇒ 缓解 = AC3 值判据**两侧** + AC4；
  - ⚠️ **不登记 ⇒ implicit wire**（lint 可能仍绿）⇒ 缓解 = AC4（声明断言）+ AC7。

## 用户可见变更

- **`(ii)` 形态（mem 本体与使用者同 module、仅 `addr`/`en`/`wdata` 实参视线跨层）从「**被拒**」变为「**支持**」**
  ⇒ ⇒ ⚠️ **下游若把该形态当作「不合法」并据此写过 workaround（改设计 / 拆层）⇒ MUST 复核**（产物会从「报错」
  变成「可发射」）。
- **诊断文案变更**：被拒时的 `hint` 不再说「把 mem 与使用者放同层」（当 mem 本体已在同层时）
  ⇒ ⇒ 若下游脚本 grep 旧文案 ⇒ MUST 调整。
  ⇒ ⇒ ⚠️ **2026-11-09 metis 补登**：`(i)` 支（mem 本体跨 module）的**消息格式**也变了
  （旧 = `cross-module ch_mem access ('%s' in '%s' <- '%s' in '%s')` ⇒ 新 =
  `cross-module ch_mem access (ch_mem '%s' owned by module '%s', used by a port in module '%s'
  for node '%s')`）⇒ ⇒ **下游若按 `in '` 解析 ⇒ 会断** ⇒ ⇒ MUST 复核。
- ⚠️ **无**公开 API 移除 / 改名 ⇒ ⇒ 不走 P14 登记。
