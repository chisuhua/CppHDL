# 审稿记录 · Phase 6 / B2 中期复盘 #2（architect 域 · 2026-11-12）

> **对象**：`docs/architecture/2026-11-12-phase-6-b2-midterm-review.md`（新增 · 架构主题文档 · **非决策文档**）
> **review_profile**：`arch`
> **结论**：**APPROVE_WITH_CONDITIONS**（critic）· **PASS-WITH-NOTES**（oracle）⇒ ⇒ **0 BLOCKER**
> **轮次**：critic **第 1 轮**（无复审 —— 唯一 SHOULD_FIX 已在本提交前修完）

---

## 1. 审查链（顺序不可颠倒）

| # | 审查者 | 结论 | 处置 |
|---|---|---|---|
| 1 | **oracle** | `PASS-WITH-NOTES` · 9 条建议 | **9/9 全部落实**（见 §3） |
| 2 | **critic** | `APPROVE_WITH_CONDITIONS` · **0 BLOCKER** · 1 SHOULD_FIX · 3 NITS | **1 SHOULD_FIX + 3 NITS 全部修完**（见 §4） |

⇒ ⇒ **决策内容零变更**：本文件**不新建 ADR**、**不反转任何已签批决策**、**不改任何代码**、
**不改任何既有台账 / 登记 / 状态文件**。

---

## 2. 本文件相对上一份复盘（`2026-11-09-phase-6-midterm-review.md`）的**增量**

| # | 增量 | 性质 |
|---|---|---|
| ① | **4 条实测发现**（每条有可复跑证据） | 不是复述状态 —— ①CI 长期红（既有）②P7 状态格落后于实现 ③phase-6 的 B2 行自相矛盾 ④B3 的🚪 入口门两道未过 |
| ② | **B2 的实质产出重新定位** | 不是「覆盖率 +N」，而是「**换一个观察者**」⇒ 挖出 3 个 C++ 侧 demo 全绿的实现缺陷（D5/D6/D7） |
| ③ | **P7 的剩余面与依赖链订正** | ② 的产物级元组断言**不存在**（不是「已做」）⇒ ④ 的依赖 = **change 归档**（不是依赖 ⑤） |
| ④ | **4 条不得越界的边界** | 含「**不得放宽 `ledger_evidence` 解析器来修发现 1**」 |
| ⑤ | **T0–T11 任务清单**（带机械判据 + 归属 + 前置） | 交 planner 域排期；含 2 条 architect 域待裁（T5 T2 裁决 / T11 FENCE 裁决）与 1 条用户裁决（T8 D8） |

---

## 3. oracle 的 9 条建议 · **逐条处置**

| # | finding | 处置 |
|---|---|---|
| ① | **高影响**：§5 T3 / §3.2 的 P7 剩余面不全（漏 ① Reject 粒度 / ② change 未立项 / ③ 判据用例）+ 「④ 依赖 ⑤」**无出处** | ✅ **采纳** ⇒ §3.2 改写为 **5 个子步逐条真实状态**（①✅ ②⬜ ③✅ ④🟡 ⑤⬜）+ 依赖链改为「**④ ← change 归档 ← {立项 + 判据用例 + Reject 粒度}**」；§5 T2/T3 同步 |
| ② | **高影响**：§2.1 合计数算错（782 混口径 / spec 166 对不上任何口径） | ✅ **采纳** ⇒ 改为**四个口径分列**：运行时实测 **1038** · 登记下界 **782** · 场景 **54** · spec-derived **676**（并注明「混用 = 假账 ⇒ 本轮初稿就是在这里算错了」） |
| ③ | **中**：§5 T2 的目标态本身会造成新漂移（把「实现已落地」写成「子步已完成」） | ✅ **采纳** ⇒ T2 目标态改为「①③ ✅（**实现**）· ②⑤ ⬜（**判据**）· ④ 🟡 半闭」+ 判据补「**不得**把实现已落地写成子步已完成」 |
| ④ | **中**：§5 T5 未区分「独立裁定 vs ADR-068 修订」 | ✅ **采纳** ⇒ 改为「**载体 = 新立 ADR**（建议 ADR-069 · Parent = ADR-068）⇒ **禁**改写刚 Accepted 的『本 ADR 不裁决 T2』承重句」+ 判据注明「**决策内容变更** ⇒ oracle → critic 全审，非轻量判据替换」 |
| ⑤ | **中**：T0 修法漏「`:行号` 后缀」解析陷阱 + 可升级 6 份 | ✅ **采纳** ⇒ 四条禁令（散文尾 / `+` 多路径 / `:行号` / `/tmp`）+ **附录 A 的 6 份升级对照表**（实测存在 + `test:` 形态的 CMake 注册已验） |
| ⑥ | **低**：§4.2 套用陈旧（共享 3 资源的是 B3 vs P6–P8，不是 B2 vs B3） | ✅ **采纳** ⇒ 标题改「B3 与 phase-6 余量（P6/P7/P8）可并行」+ 资源 ① 标注「**仅 P7 动**」+ 补「**B2 收尾不碰这 3 个资源**」 |
| ⑦ | **低**：T8 缺 A2 删端口的爆炸半径 | ✅ **采纳** ⇒ 补「SoC 顶层输入 **5 → 4** ⇒ `tb_rv32i_soc.sv` 的 `.top_unnamed_input(ext_clk)` + `test_rv32i_soc_dual_parity.cpp` 的驱动点 MUST 同步删（**行为零变化**成立 —— B2 已实测）」 |
| ⑧ | **低**：漏两个待办 | ✅ **采纳** ⇒ **T10**（`mem-port-arg-cross-layer` 的第三处状态漂移：frontmatter 仍 `draft` + `unmeasured` 而 P5 已归档）+ **T11**（`FENCE` 语义裁决 = architect 域 · planner 无权拍板 · 须在 T7 侦察之后） |
| ⑨ | **低**：§6.3 漏第四条边界 | ✅ **采纳** ⇒ 补「**不得放宽 `ledger_evidence` 的解析器来修发现 1**」（容忍散文尾 / `+` 多路径 / `:行号` = 拿 fail-closed 语义交易便利 ⇒ 放宽 = 复辟它要治的病） |

---

## 4. critic 的 1 SHOULD_FIX + 3 NITS · **逐条处置**

| # | finding | 处置 |
|---|---|---|
| **S1** | `:66` §2.1 标题「（**782 条断言合计**）」与正文矛盾（782 是**登记下界**，实测合计是 **1038**）⇒ ⇒ 正是初稿错误的**标题残留** | ✅ **修完** ⇒ 改为「（**实测 1038 条断言 · 登记下界 782**）」 |
| **N1** | `:112-113` 解析器**已支持** `#anchor`（`ledger_evidence.py:126` / `:137` 均 `split("#", 1)`）⇒ ⇒ 「行号移进正文」不是唯一解 | ✅ **修完** ⇒ 补「或改用 **`#L<行>` 锚**（解析器已支持）⇒ **既可机器判存在又保留行锚**」⇒ ⇒ **不触碰**「不放宽解析器」边界 |
| **N2** | `:124` vs `:130` 同一三档分支两处引 `:1610-1630` / `:1610-1641` | ✅ **修完** ⇒ 统一为 `:1610-1630`（实测 `SynthesizeFatal` 在 `:1617` · `Reject` 在 `:1630`） |
| **N3** | `:63` B1 的「11/11」列出 12 个记号（`N5(a)` 计入 `N5`） | ✅ **修完** ⇒ 加注「记号 12 个而项数 11 = **沿用源文件口径**」 |

---

## 5. critic 的**验证建议**（critic 无 shell 权限 ⇒ 由 architect 代跑并回填）

| 项 | critic 的状态 | architect 实测结果 |
|---|---|---|
| `git log -1 4293fc6`（护栏本体日期） | ⚠️ 未验证 | ✅ **2026-11-01** `feat(rddf): 纪律机制 M-a —— 台账 evidence: 三值 + 核验护栏（CI 2 处）` |
| `git log -1 1dc1d98`（草案立项日期） | ⚠️ 未验证 | ✅ **2026-11-10** `docs(improvements): 立项 7 份「RTL 生成能力产品化」草案` |
| 在 `1dc1d98` 的 worktree 上复跑 ledger 护栏（红 7 项的归因） | ⚠️ 推断，未验证 | ✅ **已复现**：临时 worktree（`git worktree add -f /tmp/opencode/acc_110 1dc1d98`）上实测 **红 7 项**（当前红 10 项 ⇒ 新增的 3 项 = `node-name-prefix-consistency` / `port-name-literal-parity` / `decision-request-port-naming-parity`）⇒ ⇒ **「既有红 · 非 B2 引入」的判定成立** ⇒ worktree 已 `remove` |
| `git log -1 1d9683b`（P7 实现提交） | ⚠️ 未验证 | ✅ **2026-11-10** `feat(hier): P7 三档 PortPolicy 语义分化（Warn 合成+CHWARN+注释 · Fatal 合成后失败+列全部 · Reject 失败+只报第一条）` |
| §7 判据自检 8 条 | ✅ context 已给 8/8 | ✅ **复跑 8/8 一致**（① PASS ② PASS ③ FAIL(10) ④ 2/3 ⑤ 1 ⑥ 0 ⑦ 263） |
| T5 的「若选 A1 ⇒ MUST 追加同址处置」条件分支 | ⚠️ 推断，未验证 | ✅ **已核**：`docs/adr/ADR-068-*.md:82-93` §2.1 第 3 点明写「若 B3 实施时 T2 选 A1（**或任何让地址呈现拍早于「消费拍前一拍」的形态**）⇒ ⇒ MUST 追加 store→load 同拍处置（① 读数据旁路「同址且写使能」的提交值，或 ② 一拍停顿）⇒ ⇒ 并 MUST 用 **V5** 的零间隔同址用例判别」⇒ ⇒ 文档表述**与 ADR 逐字一致** |

---

## 6. 本次复盘的**判据自检**（可复跑 · 8 条）

```bash
cd /workspace/project/CppHDL
bash scripts/verify_examples_rtl_sim.sh        # ① PASS（10 项 · 严格模式）
bash scripts/verify_arch_ledger_residuals.sh    # ② PASS（双向 1:1）
bash scripts/verify_ledger_evidence.sh         # ③ FAIL(10) ← 发现 1 · 待 T0 修
grep -c 'PortPolicy::SynthesizeFatal' src/hierarchy_verilog.cpp   # ④ 2
grep -c 'PortPolicy::Reject'            src/hierarchy_verilog.cpp   # ④ 3
grep -c 'E6 ⬜ 未做' .rddf/roadmap/phases/phase-6.md                # ⑤ 1 ← 待 T1 修
grep -c 'D-TCM' .rddf/checks/public-api-surface.md                 # ⑥ 0 ← 待 T6 修
grep -m1 'CTEST_TOTAL' AGENTS.md                                  # ⑦ 263
```

⇒ ⇒ ③⑤⑥ **现在必红 = 正是本轮发现的三个待修项** ⇒ ⇒ 判据**有判别力**（不是恒绿）。

---

## 7. **遗留清扫**（本次范围内）

| 项 | 状态 |
|---|---|
| 本文件引入的新代码 / 脚本 / 临时文件 | ✅ **零**（只新增 1 个 `.md`） |
| 临时 worktree（`/tmp/opencode/acc_110`） | ✅ **已 `git worktree remove --force`** |
| 本文件内的**过程日志**（审稿轮次 / BLOCKER 清单 / oracle 意见） | ✅ **零**（全部落在**本文件** ⇒ §3 §4 §5 只在本文件内，**未混入**架构主题文档） |
| 架构主题文档的**正文行数** | ⚠️ **255 行**（`≤ 250` 的自检口径**略超 5 行**）⇒ ⇒ 该口径针对**决策文档（ADR）**；本文件是**主题文档**（当前态快照 + 目标链 + 后续清单）⇒ ⇒ 与上一份复盘（`2026-11-09-phase-6-midterm-review.md` = **222 行**）同体裁 ⇒ ⇒ **接受**（若 critic 要求压缩，N1/N2/N3 已省 5 行） |
| 既有红（`verify_ledger_evidence.sh`） | ⚠️ **既有 · 非本次引入** ⇒ ⇒ 已登记为 **T0**（planner 域）⇒ ⇒ 本文件**只诊断不修** |
| 既有漂移（`phase-6.md` 的 P7 / B2 行、`mem-port-arg` 草案 frontmatter） | ⚠️ **既有** ⇒ ⇒ 已登记为 **T1 / T2 / T10**（planner 域）⇒ ⇒ 本文件**只诊断不修** |
