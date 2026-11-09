# Phase 6 后续任务梳理 + 实施路径（planner 域 · 2026-11-09）

> **触发**：用户口令「然后切 planner 对后续还有哪些任务内容都梳理一遍，再给出后续任务内容和实施路径」。
> **上游**：architect 中期复盘（`docs/architecture/2026-11-09-phase-6-midterm-review.md` ·
> ADR-066 **v1.4** / ADR-060 **v1.21** 事实订正 · oracle `PASS-WITH-NOTES` ·
> critic `APPROVE_WITH_CONDITIONS`（**0 BLOCKER**））。
> **产出**：`.rddf/roadmap/phase-6-next-steps.md`（**planner 域**）+ `phase-6.md` 的 P5 行订正
> + change 文档（`proposal.md` / `design.md` / `tasks.md`）订正。
> ⚠️ **本文件是审稿记录**（`metis` 轮）⇒ ⇒ 过程日志**只**在此处，不进规划文档。

---

## 1. metis 第 1 轮（**逐条应答** · 0 遗漏）

### 1.1 P0（不修则 T1 会以**假绿**归档）

| # | finding | 处置 |
|---|---|---|
| **P0-1** | S2′ 判据**只有负向半边**（`grep -c "不产生任何跨 module 边" == 0`）⇒ ⇒ **删块也能绿** ⇒ ⇒ 违反仓内「护栏须有两面」 | ✅ **采纳** ⇒ next-steps S2′ 判据补**正向**半边：`grep -c "含 mem 本体" src/hierarchy_analysis.cpp` ≥ 1 |
| **P0-2** | 「修正 `364e213` 引入的错误注释」只在 next-steps ⇒ ⇒ **不在 change 里 ⇒ 没人会在归档前勾它** | ✅ **采纳** ⇒ `tasks.md` **S2 段**新增该子项（**本 change 自己的 commit 引入的缺陷** ⇒ 属自债） |
| **P0-3** | **AC1 零判别力**：既有 `tests/test_hierarchy_analysis.cpp:666` 只断言 `find("cross-module ch_mem")` ⇒ ⇒ pre-scan 消息与边闸门消息**都含该子串** ⇒ ⇒ **改前改后都绿 · 归因改错也绿** ⇒ ⇒ next-steps §2.3 ③「(i) 不重复造」的判断**站不住** | ✅ **采纳** ⇒ `proposal.md` **AC1** + `tasks.md` **S2 段** + next-steps 三处同步改为「既有 (i) 用例**只断言子串 ⇒ 零判别力** ⇒ ⇒ MUST **强化**（断言持有 module 名 + 使用 module 名 + 不含同层提示）」 |
| **P0-4** | **AC6 判据恒真**：「不含中文串『把 mem 与使用者放同层』」⇒ ⇒ 该中文串**全仓为 0** ⇒ ⇒ 恒绿 ⇒ ⇒ 且 (iii) 支**今天就是违规状态**（真实文案是英文 `memory and its user to be in the same module`）⇒ ⇒ 这是继 critic B1 之后**同型第二次**恒真判据 | ✅ **采纳** ⇒ `proposal.md` **AC6** + `tasks.md` **S4** 改为**分支化消息级断言**：① (i) 支**MUST 含**该英文句（合法保留）② (ii)/(iii) 支**MUST NOT 含** ⇒ ⇒ 禁全文件 grep 代替 |
| **P0-5** | `design.md:105` **指错目标行**（说 `:542`，而 `:542` 早已改完）⇒ ⇒ 真正含已证伪断言的是 **`:277-279`**（S2 自己引入） | ✅ **采纳** ⇒ `design.md` §2.4 改为 `:276-281` + 注明「该注释系 **S2（`364e213`）自己引入** ⇒ 属**本 change 的自债**」 |
| **P0-6** | `phase-6.md:64` 仍写「P5 的『正确支持』支**仍待**独立裁定」⇒ ⇒ 与 P5 行「✅ 已裁定（决策 ⑧）」**直接冲突** | ✅ **采纳** ⇒ 改为「已由 ADR-066 v1.3 决策 ⑧ 裁定 ⇒ ⇒ **不再**是『仍待独立裁定』+ v1.4 实测订正（S3 不需产品代码）」 |

### 1.2 P1（不改则判据不可判）

| # | finding | 处置 |
|---|---|---|
| **P1-1** | **AC3/AC4 无载体**：「产物级断言」没说是哪个测试文件 ⇒ ⇒ 没有载体 = 没有判据 | ✅ **采纳** ⇒ next-steps S3′ 指名载体 = `tests/test_hierarchy_verilog.cpp` 新增 CASE（写临时目录 + 读回文本）**或**扩 `tests/hier_rtl_products.cpp` + `verify_hierarchical_rtl.sh` 加 **R9′** |
| **P1-2** | **AC5 的 fail-closed 分支是否可召唤未定**：`node_names_ = view_.names` + `print_mem_*` 是私有方法 ⇒ ⇒ 测试侧无法直接喂「有 enable 但名字表缺项」的入参 | ✅ **采纳** ⇒ 登记为**开放点**（next-steps §2.2 的 S4 行 + `design.md` §3 开放点）⇒ ⇒ 若端到端构造不出 ⇒ ⇒ AC5 判定为「不可测」⇒ ⇒ 改**弱判据**（代码面 `grep -c "1'b1"` 在 `print_mem_*` 内 ≤ 1 且仅出现在 `enable == nullptr` 分支）⇒ ⇒ **不得**把合法缺省误判为回退 |
| **P1-3** | **AC10「全绿」检不出静默缩面**：锚点只有 2 个 | ✅ **采纳** ⇒ `proposal.md` **AC10** + next-steps **S0 / S6″** 补 **4 个锚点 + 期望 delta**（`k=0` 处也 MUST 显式写 `+0`）+ 护栏清单补 `verify_dual_backend_parity.sh` |
| **P1-4** | **AC8 缺两条**：新 DUT 未登记语料表（`verify_hier_corpus_coverage.sh` 会转红）· 缺决策 ⑤ 共网用例 | ✅ **采纳** ⇒ `proposal.md` **AC8** + next-steps **S5′** 各补 ①语料登记 + 结构类 ②决策 ⑤ 共网用例（同一子 `ch_out` 既直通又作实参 ⇒ 一根网） |
| **P1-5** | **T3 元组判据的提取通道未定义**（`Warn` 的边数只能从 stderr 数）⇒ 且 **Warn 侧「产物是否存在」今天恒 false**（`:952-991` 对所有 policy 都 `return result` ⇒ Warn ≡ Fatal ⇒ **这正是 P7 的病灶**）⇒ ⇒ 改前改后都是 `(false,·,·)` ⇒ ⇒ 三档不可能区分 | ✅ **采纳** ⇒ next-steps T3 ② 补**提取通道**（Warn = 重定向 `std::cerr` 数 CHWARN · Fatal/Reject = 解析 `result.error`）+ **施序 = 先修实现再判**的显式警示 |
| **P1-6** | **T3 ③「codegen 侧新增策略读取」是过度设计**：`port_policy` 只在 `HierarchyEmitOptions` 可见 ⇒ ⇒ 要么给 `ModuleEmitter` 加依赖（**与 P5 的 AC5 同文件同区域 ⇒ 真的冲突**）要么白做 | ✅ **采纳** ⇒ 改为「**策略读取留在 `hierarchy_verilog.cpp`**（`:1262` 附近）· 注释文本随 `ModuleView` 传给 codegen（codegen **只打印**）⇒ ⇒ **MUST NOT** 给 `ModuleEmitter` 加 `HierarchyEmitOptions` 依赖」 |
| **P1-7** | **T4 的 `5 / 39` 无人维护**（「一个 TEST_CASE ↔ 一个 JitOp」是**约定**非事实；`CALL_EXTERNAL`/`NOP` 永不该有 golden ⇒ ⇒ 100% 不可达） | ✅ **采纳** ⇒ 改为**分子/分母派生** + 写死两条命令 + 新建 `.rddf/checks/jit-golden-coverage.md`（⚠️ **当前不存在** ⇒ ⇒ 悬空指针 ⇒ MUST 建）+ 新建 `scripts/verify_jit_golden_coverage.sh` |
| **P1-8** | **无「S-1 提交现有工件」**⇒ ⇒ S6″ 的「零回归」**无法界定改前改后** | ✅ **采纳** ⇒ next-steps 新增 **S-1**（提交本轮工件 + 记基线 commit） |

### 1.3 P2（减层与口径）

| # | finding | 处置 |
|---|---|---|
| **P2-1** | **§4「本轮已做的订正清单」= 过程日志**（信息量为 0 ⇒ `git diff` 就是权威）⇒ ⇒ 留在规划文档里只会在下一次被误当「已完成」的证据 | ✅ **采纳** ⇒ §4 改为**证据索引**（只给**可 grep 的落点**）+ **§4.2「本轮未做完」清单**（如实登记 5 项）⇒ ⇒ 并加一句「**完成度**看 `tasks.md` 的 checkbox（**那才是** change 的权威）」 |
| **P2-2** | **§2.3「三条实施纪律」是纯复述**（① 在根 `AGENTS.md` ANTI-PATTERNS ② 一次性结论 ③ oracle 结论）⇒ ⇒ 三条都不构成「新约束」 | ✅ **采纳** ⇒ 改为**指针块**（不复制正文） |
| **P2-3** | **§6 判据自检会随无关改动转红**（`不可达\|静默丢端口` 的命中面含 `src/**` ⇒ ⇒ 超出 planner 域） | ✅ **采纳** ⇒ 该条**删除** ⇒ ⇒ 保留 3 条**不会转红**的 |
| **P2-4** | **§1 总览表把「阻塞关系」与「状态」混在一列** ⇒ ⇒ 与它自己声明的「不复制状态格」打架 | ✅ **采纳** ⇒ T2 行的状态词删掉（改为「⚠️ **部分完成**」⇒ ⇒ 指向 §4.2 的未做完清单） |
| **P2-5** | **同一事实 17 处副本**（v1.4 的订正出现在 5 个文件的 17 处）⇒ ⇒ 违反仓内自己刚立的「不复制计数 · 指向锚点」规矩 | ✅ **采纳（部分）** ⇒ next-steps 内**长内联句压成一行指针**（`⚠️ 已订正 · ADR-066 v1.4 §2.7`）⇒ ⇒ **change 文档保留**（**它们是实施者的工作面** ⇒ ⇒ 该处重复是**承重的**）⇒ ⇒ 但**头部权威指针已补 v1.4 / v1.21**（⇒ ⇒ 实施者有**单一权威入口**） |
| **P2-6** | **§5 ③「T1 是否继续」= 自设阻塞门**（用户还得再开一轮才能开工）⇒ ⇒ 用户要的是**推荐 + 代价** | ✅ **采纳** ⇒ 改为「planner **直接定** = **继续**」+ 写明**代价**（不补 AC ⇒ 归档 = 假绿 ⇒ ⇒ 代价已由 §2.2 的判据清单显式化）⇒ ⇒ 用户只需在 ①②③ 上拍板 |
| **P2-7** | **§5 ② 漏了「需 architect 裁定」**；**新增 ④**：P6 的 `FENCE` 裁决 = **ISA 语义** ⇒ ⇒ planner 无权拍板 ⇒ ⇒ 越权 | ✅ **采纳** ⇒ ② 扩为「需新 improvement **+ 可能需 architect 裁定**」· 新增 ③「`FENCE` 裁决 + 新 capability 立项需 architect 预算」⇒ ⇒ **P6-0 只做侦察**（复用面清单 + 候选方案），**不**落裁决 |
| **P2-8** | **位置**：`.rddf/roadmap/phase-6-next-steps.md` 在**根目录** ⇒ ⇒ 实证 `verify_roadmap_fragments.sh:451-484` 的 `load_features` 只枚举 `("phases","features","archive")` ⇒ ⇒ **不被扫** ⇒ ⇒ 但 `:422-433` 的 `parse_front_matter` 对「不以 `---` 开头」**直接 raise** ⇒ ⇒ 一旦被移进 `phases/` ⇒ ⇒ **立刻 fail-closed**（**脚枪**） | ✅ **采纳（位置保持 + 加护栏说明）** ⇒ ⚠️ **未**采纳「加 front-matter」：metis 自陈**无法判定** `roadmap_refresh_fragments.sh` 是否会把 `kind: note` 索引进 AUTO-INDEX ⇒ ⇒ **无法判定 + 需要什么证据** ⇒ ⇒ 保守选择 = **保持无 front-matter** + 在 §6 加**位置保护**警示（「MUST NOT 移入 `phases/`」）⇒ ⇒ **已实测**：放 `phases/` ⇒ guard 报「front-matter 解析失败」⇒ 移到根 ⇒ **PASS** |

### 1.4 metis 的**其他边界发现**（已处置 / 已登记）

| # | finding | 处置 |
|---|---|---|
| ① | **下游 grep 断裂面漏登记**：S2 **整体改了 (i) 的消息格式**（旧 `('...' in '...' <- '...' in '...')` ⇒ 新 `(ch_mem '...' owned by module '...', used by a port in module '...' for node '...')`）⇒ ⇒ 下游按 `in '` 解析会断 ⇒ ⇒ 但 `proposal.md` 的「用户可见变更」只声明了 hint 变化 | ✅ **采纳** ⇒ `proposal.md`「用户可见变更」**补登**该格式变化 + 提示 MUST 复核 |
| ② | **空 `specs/rtl-hierarchical-output/` 目录** ⇒ ⇒ `openspec archive` 对「有 capability 目录但无 `spec.md`」的行为**无法判定** | ✅ **采纳** ⇒ **已删空目录**（git 不跟踪空目录 ⇒ 零成本）⇒ ⇒ S6 再建内容 |
| ③ | **`en` 槽的合法缺省 vs 禁止回退** 在实现里是**同一行** `std::string en_str = "1'b1";` ⇒ ⇒ MUST 拆成**两个显式分支**，否则「禁止回退」会把合法缺省也判红 | ✅ **采纳** ⇒ 登记为 `design.md` §3 的**开放点** + next-steps S4 的实现提示 |
| ④ | **pre-scan 的 `dynamic_cast` 成本** = O(nodes) · 建图期一次性 ⇒ ⇒ 可接受 ⇒ ⇒ 但 AC 应写「**不进入 `tick()` 路径**」⇒ ⇒ 否则会被当成性能项返工 | ✅ **采纳** ⇒ 登记为 `design.md` §3 开放点（`dynamic_cast` 成本的说明扩充） |
| ⑤ | **锚点编辑权**：3 条轨都改 `AGENTS.md` 的 `CTEST_TOTAL`（**各 2 处**）⇒ ⇒ 文本必冲突；且各自 +K 会得到**错的最终值** | ✅ **采纳** ⇒ next-steps §1 加「**可并行的真实边界**」段（3 个共享单点资源 + **锚点编辑权 = 最后合入者** + 合入前 MUST 全量重建重算）+ 建议序列（T1 先行 ⇒ T3/T4 接手 ⇒ T5 全程并行） |
| ⑥ | **状态真相自相矛盾**：next-steps 声明「不复制状态格」但 §1/§2.1/§4 都有状态 | ✅ **采纳** ⇒ §4 改为**证据索引**（§4.2 = **未做完**清单）⇒ ⇒ 头部加「**完成度**看 `tasks.md` 的 checkbox」 |

---

## 2. 判据自检（当前实测）

```bash
grep -c "实测订正" .rddf/roadmap/phases/phase-6.md
grep -c "实测订正" openspec/changes/mem-port-arg-cross-layer-2026-11/{proposal.md,design.md}
grep -c "metis 订正" openspec/changes/mem-port-arg-cross-layer-2026-11/{proposal.md,design.md,tasks.md}
grep -c "metis" .rddf/roadmap/phase-6-next-steps.md
bash scripts/verify_openspec_delta.sh
bash scripts/verify_roadmap_fragments.sh
ls openspec/changes/mem-port-arg-cross-layer-2026-11/     # ⇒ 无空 specs 目录
```
