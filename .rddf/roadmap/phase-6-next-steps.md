# Phase 6 后续任务清单 + 实施路径（planner 域 · **2026-11-12 修订**）

> **触发**：用户口令「切 architect 对完成和后续任务进行梳理……然后切 planner 对后续还有哪些任务内容都梳理一遍，
> 再给出后续任务内容和实施路径」。
> **上游**：**`docs/architecture/2026-11-12-phase-6-b2-midterm-review.md`**（architect 复盘 #2 ·
> 目标链 + **4 条实测发现** + T0–T11 任务清单 · oracle `PASS-WITH-NOTES` 9 条建议**全落实** +
> critic `APPROVE_WITH_CONDITIONS` **0 BLOCKER** · 1 SHOULD_FIX + 3 NITS **全修完** ·
> 审稿记录 `.rddf/reviews/phase-6-b2-midterm-review-2026-11.md`）。
> **窗口唯一真相** = `.rddf/roadmap/phases/phase-6.md`（**任务状态以 phase-6 为准**；本文件是
> **任务内容 + 实施路径**的载体，**不复制** phase-6 的状态格）。
> **本文件不新增任何架构义务** —— 所有判据均映射**既有** AC / phase-6 出口判据 / architect 复盘结论。

---

## 0. ⚠️ 编号约定（**避免与 architect 复盘的 T 编号冲突**）

| 编号 | 含义 | 出处 |
|---|---|---|
| **R0–R11** | **本文件**（2026-11-12）的后续任务清单 | 本文件 §1–§3 |
| **T0–T11** | architect 复盘 #2 的任务清单（**同一批任务**） | `docs/architecture/2026-11-12-phase-6-b2-midterm-review.md` §5 |
| **旧-T1 … 旧-T8** | **上一版**（2026-11-09）的任务清单 —— **已全部闭口或移交** | 本文件 §4 |

⇒ ⇒ **R 与 T 是一一对应的同一批任务**（R0↔T0 · R1↔T1 · … · R11↔T11）⇒ ⇒ **两处可交叉 grep**。
⇒ ⇒ ⚠️ **旧编号 MUST 带 `旧-` 前缀**（`旧-T1` … `旧-T8`）—— 否则 `T1` 在同一文档里指两个任务
（新 T1 = 订正 B2 行 / 旧 T1 = P5 收尾）⇒ ⇒ 实施者 grep `T1` 会拿到**互斥记录**。
⇒ ⇒ ⚠️ **禁止在新文本中裸用 `T1–T11`** ⇒ ⇒ 需带出处（`arch-复盘#2` 或 `旧-`）。

---

## 1. 后续任务**总览**（**按可执行性排序** · 不按编号）

| # | 任务 | 归属 | 前置 | 阻塞关系 |
|---|---|---|---|---|
| **R0** | 🔴 **修 `verify_ledger_evidence.sh` 的 10 项红**（10 份台账的 `evidence:` ⇒ 裸路径 / 升级形态）⇒ ⇒ **解 CI 红** | planner | — | ★ **唯一「现在就能做且解 CI 红」的项** ⇒ ⇒ **R1–R3 的前提**（状态订正也要跑护栏） |
| **R1** | 🟡 订正 `phase-6.md` 的 **B2 行**（删「E6 ⬜ 未做」那段陈旧快照） | planner | — | 纯状态订正 ⇒ ⇒ 与 R0 同批可做 |
| **R2** | 🟡 订正 `phase-6.md` 的 **P7 行状态格**（①③ ✅ · ②⑤ ⬜ · ④ 🟡 半闭）⇒ ⇒ 与 next-steps 的 T3 遗留段一致 | planner | — | 纯状态订正 ⇒ ⇒ **R3 的前提**（R3 的子步归属靠它） |
| **R3** | 🟡 **P7 的 ②⑤ + Reject 粒度 → change 立项/归档 → ④ 台账行 15 闭口** | builder → architect | R2 | ⇒ ⇒ **④ 的依赖 = change 归档，不是依赖 ⑤** ⇒ ⇒ 顺序：修粒度 → 补判据 → 立项归档 → 闭口行 15 |
| **R4** | 🟡 **P8 builder 步**（删 `set_ab_verification` stub + golden 下限派生 + 两份文案订正） | builder | — | 与 R3 **可并行**（写点不重叠：`include/simulator.h`+`src/simulator.cpp` vs `src/hierarchy_*.cpp`） |
| **R5** | ✅ **B3 的 T2 裁决**（MEM 数据对齐）⇒ ⇒ 载体 = **ADR-069**（**Accepted · 2026-11-12**）⇒ ⇒ 决策 = **候选 ②（A0）**（地址照旧 + MEM/WB 延后 1 拍）· oracle **×3**（第 1/2 轮 REJECT ⇒ 第 3 轮 PASS-WITH-NOTES）· critic **×1**（APPROVE_WITH_CONDITIONS · **0 BLOCKER**） | architect | 用户 2026-11-12「按你建议拍板」 | ★ **B3 的🚪 门之一 ⇒ ✅ 已闭** ⇒ ⇒ ⚠️ **B3 开工前仍欠**：① **ADR-047 的 dated addendum**（3 处「3 拍」陈述陈旧 · SF-3 强制项）② `rv32i-data-forwarding` 的 MODIFIED（3→4 · `load_use_stalls` 6→8）⇒ ⇒ 审稿记录 `.rddf/reviews/ADR-069-mem-data-alignment.md` |
| **R6** | ✅ **B3 的公开面前置门**（CHANGELOG + `public-api-surface.md` 各一行） | planner | — | ★ **B3 的🚪 门之二** ⇒ ⇒ ✅ **已登记**（2026-11-12 · 前置登记 = 预告 + 迁移指引 · 非「已发布」）⇒ ⇒ `firmware-load-chain` L1–L6（加载端口族语义变更）+ `memory-timing-contract` T1/T3（`data_ready` 语义重定义）⇒ ⇒ 护栏 `verify_public_api_visibility.sh` PASS |
| **R7** | 🟡 **P6-0 侦察**（RV32I ISA 缺口：复用面清单 + `FENCE` 裁决候选） | planner → architect | — | **只读侦察** ⇒ ⇒ 可全程并行 |
| **R8** | 🟢 **D8**（`ext_clk` 死端口）⇒ ⇒ 建议 **A2**（删端口或明确注释） | architect → 用户 | — | 需**用户裁决** ⇒ ⇒ 与所有批**无耦合**（不改任何时序金标） |
| **R9** | 🔒 **P11**（ADR-036 物理移除） | builder | 日历锁 **2027-04-17** | 本窗口**不做** |
| **R10** | 🟡 **第三处状态漂移**（`mem-port-arg` 草案 frontmatter） | planner | — | 与 R1/R2 同批 ⇒ ⇒ **一次扫干净**比留到下个窗口便宜 |
| **R11** | 🟡 **`FENCE` 语义裁决**（architect · **planner 无权拍板**） | architect | R7 | 须在 R7 侦察之后（侦察提供候选方案）⇒ ⇒ 可与 B3 并行 |

⇒ ⇒ **★ 两条 🚪 门**：**R5 + R6 之后 B3 才能开工** ⇒ ⇒ R5/R6 是 B3 的**硬前置**。
⇒ ⇒ **★ R0 排第一**：它是唯一「现在就能做且解 CI 红」的项 ⇒ ⇒ 且 R1/R2/R10 的状态订正
**也要跑护栏** ⇒ ⇒ 先解红再订正 ⇒ ⇒ 否则订正后的状态**无法被护栏确认**。

### 1.1 「可并行」的真实边界（metis 订正 · 只覆盖**写点** ⇒ ⇒ **收口阶段不并行**）

三条轨共享**五个**单点资源 ⇒ ⇒ ① `openspec/specs/rtl-hierarchical-output/spec.md`
（**仅 R3 动** —— B3 的 spec 面在 rv32i / dtcm / manifest `semantic`）② 根 `AGENTS.md` 的
**4 个计数锚点**（各 **2 处** ⇒ ⇒ 文本必冲突）③ `.rddf/checks/public-api-*`（R4 与 R6 都动
`EXPECTED_REMOVAL_ROWS` / `EXPECTED_PROBES`）④ **`.rddf/checks/arch-residual-registry.md`**
⑤ **`docs/architecture/2026-11-02-hierarchical-rtl-output.md` §7**（④⑤ 由 **R3.5** 与 **B3** 同写 ——
ADR-068 **V3** 明写「BRAM 可推断性未在 CI 验证 ⇒ MUST 登记残差」⇒ ⇒ B3 必然写这两处）⇒ ⇒
**★ 锚点编辑权 = 最后合入者** ⇒ ⇒ 合入前 MUST **全量重建并重算**（否则两条轨各自 +K ⇒ ⇒ 错的最终值）
⇒ ⇒ ⚠️ **禁写死「257」/「263」** ⇒ ⇒ 写成「**基线 + 新增 − 删除 == `ctest -N`**」⇒ ⇒ ★ **护栏清单 MUST 含
`scripts/verify_dual_backend_parity.sh`**。

⚠️ **`AGENTS.md` 的派生表是永久 stale 且无护栏**（实测：锚点 `:452/475/488/497` = **263 / 76 / 65 / 284**，
而派生表 `:516-520` = **257 / 246 / 77 / 66 / 290** ⇒ ⇒ **四对全部不一致**）⇒ ⇒
`scripts/update_test_count_anchors.sh:91-104` **只改锚点行、从不改派生表** ⇒ ⇒ 重算后 MUST 二选一：
(a) 扩脚本一并更新派生表，或 (b) 把派生表标「**历史快照**」+ 加 grep 断言它**不参与判定**。
⇒ ⇒ **真相 = 锚点**（派生表自己就写「单一真相在上面的 CI 锚点」）。

⇒ ⇒ **R0/R1/R2/R10 不碰这 3 个资源**（只动台账 frontmatter + 状态格）⇒ ⇒ 与任何实施批**无冲突**。

---

## 2. 实施路径（**每步一个可判据** · 按 R 编号）

### R0 · 修台账 evidence（**解 CI 红**）—— **已完成（2026-11-12）**

**判据出处**：`scripts/lib/ledger_evidence.py:125-147`
（`test:` ⇒ 存在 **且** CMake 注册 · `probe:` ⇒ 存在 · `#anchor` 被 `split("#", 1)` 剥离 ⇒ **可用**）。

| 步 | 内容 | 判据 |
|---|---|---|
| **R0.1** | **6 份升级到「更硬的证据」**（architect 复盘 §3.1 附表 A） | `verify_ledger_evidence.sh` 逐份不红 |
| **R0.2** | **4 份改裸路径**（`probe:` + `#L<行>` 锚）⇒ ⇒ **禁**散文尾 / `+` 多路径 / `:行号` / `/tmp` | 同上 |
| **R0.3** | **复跑护栏 + 自检** | `verify_ledger_evidence.sh` **PASS** + `--self-test` **PASS** |

**R0.1 的 6 份升级对照**（落点全部实测存在）：

| 台账 | 升级到 |
|---|---|
| `examples-rtl-sim-coverage` | `probe:scripts/verify_examples_rtl_sim.sh` |
| `rtl-emit-product-tooling` | `probe:scripts/verify_flat_rtl_emit.sh` |
| `port-naming-ratchet` | `test:tests/test_port_name_named_ctor.cpp` |
| `node-name-prefix-consistency` / `port-name-literal-parity` / `decision-request-port-naming-parity` | `test:tests/test_port_name_parity.cpp` |

⇒ ⇒ **实测结果**：`verify_ledger_evidence.sh` ⇒ **PASS（91 个台账 · 6 项待补 = WARN 不阻塞）**
⇒ ⇒ `--self-test` ⇒ **PASS**（投毒转红 + 还原复绿）⇒ ⇒ **CI 红已解**。

### R1 / R2 / R10 · 三处状态真相订正（**纯文档 · 零风险**）

| 步 | 内容 | 判据（可 grep） |
|---|---|---|
| **R1** | `phase-6.md` 的 B2 行 ⇒ 删「⇒ ⇒ **E6 ⬜ 未做**（…）· **E3 余项 + E6 + 墙钟登记 + …** = 本批剩余全部」 | `grep -c "E6 ⬜ 未做" .rddf/roadmap/phases/phase-6.md` == **0** |
| **R2** | `phase-6.md` 的 P7 行状态格 ⇒ 「① ✅ / ② ⬜ / ③ ✅ / ④ 🟡 注释已做·闭口未做 / ⑤ ⬜」+ 依赖链（④ = **change 归档**，不是依赖 ⑤） | **5 个子步各判**（⚠️ 只钉 3 个 ⇒ 把 ④/⑤ 写成 ✅ 仍绿）：**先以 P7 行为作用域**（`awk '/^\| \*\*P7\*\* \|/'`）再逐项判 ⇒ ⇒ `① ✅`=1 · `② ⬜`=1 · `③ ✅`=1 · `④ 🟡`=1 · `⑤ ⬜`=1 ⇒ ⇒ ⚠️ **不按全文计数**（全文实测 ① ✅=2 / ③ ✅=2 / ⑤ ⬜=2 —— 另一处命中在 §「证据落点」表的 P7 行，**正常**）+ 整串前缀匹配 `① ✅ / ② ⬜ / ③ ✅ / ④ 🟡` |
| **R10** | `mem-port-arg-cross-layer-2026-11.md` frontmatter ⇒ `状态: draft` → **`implemented`** · `evidence: unmeasured` → **`test:tests/test_hierarchy_analysis.cpp`** · 补 `进度:` 行 ⇒ ⇒ ⚠️ **正文仍有旧状态**：`:20` 的 `**Status**: ✅ **approved**` + `:10` 的 `来源:`（「台账行 20 已裁决 · 归属=下游」）⇒ ⇒ **MUST 用追加订正行、不改写历史叙述** | `grep -m1 "^状态:" …` == `implemented` **且** `grep -c "Status\*\*: ✅ \*\*approved"` == **0**（或已改为带日期的订正注记） |

⇒ ⇒ ⚠️ **R2 的目标态纪律**：**不得**把「实现已落地」写成「子步已完成」
（architect 复盘 §5 R2 ⇒ 判据必须区分二者 ⇒ ⇒ 否则**制造**新的漂移）。

### R3 · P7 的剩余面（**顺序固定**）

| 步 | 内容 | 判据 |
|---|---|---|
| **R3.1** | **修 `Reject` 的诊断粒度**（现状报「第一条**合成物**」（含合成物名）⇒ ⇒ ADR §2.4.1 说「只报第一条**需合成的跨 module 边**」⇒ ⇒ **有出入** ⇒ ⇒ 修法 = 补**边级**收集）⇒ ⇒ ⚠️ **只针对 `Reject` 档**：**Fatal 档 MUST 仍含端口名**（ADR-060 v1.7 要求 Fatal「列全部被合成的边 **+ 端口名**」）⇒ ⇒ 若照「三档都不许有端口名」做 ⇒ **违反 ADR** | ①（**仅 Reject 档**）`error` **不含**合成端口名 **且** 含 1 条**边级**记录（`from <node> (<module>) → to <node> (<module>)`）②（**Fatal 档**）`error` **仍含**端口名（负向：一并删掉 ⇒ 必红）③ 首行计数 == 实际列出的条目数（负向：把计数写死 1 但列 2 条 ⇒ 必红） |
| **R3.2a** 🔴 **前置：观测载体定案** | Warn 档的逐条 `CHWARN` **只进日志**（`src/hierarchy_verilog.cpp:1645-1647`），**没有** push 进 `result.warnings`（该字段**已存在**于 `include/hierarchy_verilog.h:100`）⇒ ⇒ 元组的「**边数**」在现有 API 上**不可观测**（只能抓 stderr ⇒ 脆）⇒ ⇒ **MUST 先把 `synth_events` 逐条 push 进 `result.warnings`**（**不新增公开字段** ⇒ 避免与 R4.2/R6 争 `public-api-*`） | `grep -c "result.warnings.push_back" src/hierarchy_verilog.cpp` 覆盖 synth 事件 + Warn 档测试**不抓 stderr** 也能断言行数 |
| **R3.2b** | **补判据用例**（②）：**指名样本**（仓内已有可合成样本：`two_sibling_top`（`tests/test_hierarchy_verilog.cpp:257`）/ `row2_implicit`（`:2006`）/ `row1_implicit`（`:2046`）/ `sibling_wire`（`:1512`）⇒ ⇒ **MUST 先用四约束实测选定一个**：矩阵内可合成 · **N ≥ 2** 条可合成边 · **含端口 + 网** · **无悬空 `ch_in`**）⇒ ⇒ 元组 `(产物是否存在, result.warnings 中含「synthesized」的行数, 产物注释是否存在)`：`Warn = (true, N, 有)` · `Fatal = (false, N, —)` · `Reject = (false, 1, —)` ⇒ ⇒ ⚠️ **口径**：第 1 项 MUST 断言 **`std::filesystem::remove` 成功**（Fatal 档在落盘前 `return`（`:1617-1628`）⇒ 磁盘上是**上一轮**的文件 ⇒ 「先删残留」不是保险措施，**是判据的一部分**）；Fatal 另断言 `error` 中 `^\s+\[\d+\]` 行数 == N **且**首行 `N cross-module edge(s)` 的 N 与之相等 | 元组断言转绿 + 旧样本（`TwoOpTop`，**不可合成**）换掉 |
| **R3.3** | **补计数分别判**（⑤）：`// CppHDL-synth:` 的 `implicit port` / `net` **分别**计数（⇒ 1 边 ⇒ k 端口 + 1 网 ⇒ 「计数一致」**不可满足**） | 两个计数各自判 |
| **R3.4** | **change 立项 + 实施 + 归档**（`openspec/changes/<name>/` ⇒ `proposal.md` / `design.md` / `tasks.md` / `specs/`）⇒ ⇒ ⚠️ **禁 `--skip-specs`** ⇒ ⇒ 合法例外 = 「`proposal.md` 有「**无 spec 变更**」段 **且** `changes/<name>/specs/` **为空**」（⚠️ 未写这两条判据 ⇒ 任何人写一句话就能跳过） | **归档判据不对 `verify_openspec_delta.sh`**（它的职能是**提案阶段 4 条自检**，不含「归档成功」）⇒ ⇒ 改判：`openspec/changes/archive/<name>/proposal.md` 存在 **且** 主 spec 新增 Requirement 可 grep + `verify_openspec_delta.sh` 绿 |
| **R3.5** | **台账行 15 闭口**（architect）：`已知 → 已收口` · `归属 → —` · `闭口证据` = **archive 路径** · `复核条件` 填 **`—`（不是空串）** ⇒ ⇒ ⚠️ **两侧同批**（`docs/architecture/2026-11-02-*.md` §7 + `.rddf/checks/arch-residual-registry.md`）⇒ ⇒ ⚠️ **3 条格式硬约束**（登记表表头 `:17-20` 自己写了，本文件照抄）：① `闭口证据` = `path1,path2`（**半角逗号 · 无空格 · 无反引号** · archive 类 MUST 含 `proposal.md`）② `后缀` MUST 非空且**不含证据路径、不含加粗/反引号** ③ 台账 §7 的规范句 MUST 由 `verify_arch_ledger_residuals.sh --render` **生成**（手写必红） | 登记表行 15 = `已收口` + 护栏绿 + `--render` 生成的规范句 |

⇒ ⇒ ⚠️ **R3.4 是 R3.5 的前提**（闭口证据 = archive 路径 ⇒ ⇒ MUST 先有归档件）。

### R4 · P8 builder 步（**删 stub**）

| 步 | 内容 | 判据 |
|---|---|---|
| **R4.1** | 删 `set_ab_verification` / `is_ab_verification` / `ab_verification_` / 每 tick WARN + **两个局部量**（`ab_result_a` / `run_ab_verify`）⇒ ⇒ ⚠️ **MUST NOT** 改 JIT 其余行为 | 源码面 `include/ src/` == **0** 命中 |
| **R4.2** | **公开 API 移除登记**（P14 机制）⇒ ⇒ `.rddf/checks/public-api-surface.md` 移除章 + `.rddf/checks/public-api-removals.tsv` 探针行 + `docs/CHANGELOG.md` 一行 | `verify_public_api_visibility.sh` 绿 |
| **R4.3** | **golden 覆盖下限** ⇒ ⇒ ⚠️ **分子/分母 MUST 派生，且分子 MUST 是「集合」不是「计数」**：分母 = `include/jit/jit_ir.h` 的 `enum class JitOp` 块内 `^  [A-Z_]+,?$` 行数（**实测 39** · `JitOp` 头行不计）· 分子 = **剥注释后** `TEST_CASE(` 行里出现的 `JitOp::<NAME>` **集合**（⚠️ **禁裸 `grep -c`** —— 全文件 grep 会把注释里的 `JitOp::` 算进来，且 `phase-6.md` P6 判据自己就写了「禁裸 `grep -c` 计数注释」）⇒ ⇒ 判据 = **集合相等/超集**（计数判据恒绿：把已覆盖的 `SELECT` 换成 `MUL` ⇒ 5→5 ⇒ 判不出）⇒ ⇒ 基线集作为**数据行**写进**新建** `.rddf/checks/jit-golden-coverage.md` ⇒ ⇒ ⚠️ **该文件 MUST 带 `evidence: probe:scripts/verify_jit_golden_coverage.sh`**（`ledger_evidence.py:42` 的 `LEDGER_DIRS` 是**全目录 glob** ⇒ 无 `evidence:` 会让台账 91→92 / 待补 6→7 ⇒ **与 R0 的既成事实冲突**）⇒ ⇒ 新增 `JitOp` MUST 附 golden **或** 在本文件登记未覆盖理由 ⇒ ⇒ **护栏** = 新建 `scripts/verify_jit_golden_coverage.sh` + **MUST 挂** `.github/workflows/ci.yml` 的 `build-and-test` guard 段（否则等于不存在） | 新护栏绿 + `--self-test` 红 + CI 已挂载 |
| **R4.4** | **文案清理**（根 `AGENTS.md` KNOWN LIMITATIONS + `docs/usage_guide/10-verilator-backend.md:133`）⇒ ⇒ ⚠️ 根 `AGENTS.md:313` 的「OFF 时不存在」是**事实错误**（`__has_include` 对源树物理头恒真）⇒ ⇒ **替换**（不是纯删） | **拆两条**（⚠️ 单看「全 docs 0 命中」⇒ 与 R4.2 的登记**互斥**）：① **代码面** `include/ src/` 中 `set_ab_verification\|is_ab_verification\|ab_verification_\|ab_result_a\|run_ab_verify` == **0** ② **文档面**仅 `AGENTS.md` 的 KNOWN LIMITATIONS 段 + `docs/usage_guide/10-verilator-backend.md` ⇒「不再把该 API 写成**现存**接口」（允许出现「已移除」字样）· **排除** `docs/CHANGELOG.md` 与 `.rddf/checks/public-api-*`（R4.2 的登记**必须**指名） |

### R5 · B3 的 T2 裁决（**architect 域**）

| 步 | 内容 | 判据 |
|---|---|---|
| **R5.1** | 读 ADR-068 §2.1 D4（**带条件**）⇒ ⇒ 确认「**load 的地址呈现拍 MUST 严格晚于同址 store 的提交沿**」这一条件在**三候选**下**各自是否成立** ⇒ ⇒ ⚠️ **候选不是 A/B 二选一**（`memory-timing-contract-2026-11.md:60` 明写 **三选一**）：**A1 = EX 级组合出地址** · **A2 = EX/MEM 之间再加一级寄存器** · **B = 数据晚一拍采样** ⇒ ⇒ ⚠️ **A2 不得丢**（它是不改有效链时序的候选之一，而 ADR-068 D4 的耦合条件正点名 **A1**） | 三候选逐条判定 + **每项的预测计数器表**（`load_use_stalls` / `control_stalls` / `instructions` / `non_retired`）落到 ADR |
| **R5.2** | **载体二选一（MUST 明写选择理由）**：**(i) 新立 ADR-069**（Parent = ADR-068 ⇒ 保留「本 ADR 不裁决 T2」原文，便于 T2 被独立引用 —— **建议**）· **(ii) 修订 ADR-068 → v1.1**（版本 bump + 修订行 + **就地取代**该句 —— 仓内既有惯例：ADR-060 v1.18 / ADR-035 v2.2 / ADR-067 v2）⇒ ⇒ ⚠️ 两条路都**不是**「不改 ADR」⇒ ⇒ 真正禁止的是「**改了却不记修订行**」（那才自相矛盾）⇒ ⇒ 若选 T2 = **A1** ⇒ MUST 追加同址处置（① 读数据旁路「同址且写使能」的提交值，或 ② 一拍停顿）⇒ ⇒ 并 MUST 用 **V5** 的零间隔同址用例判别 | ADR 落库（新立或版本 bump）+ 修订行 |
| **R5.3** | **oracle → critic 全审**（这是**决策内容变更** ⇒ ⇒ 不是 ADR-052 v2 那类「判据面替换」轻量修订） | 审稿记录落 `.rddf/reviews/` + **0 BLOCKER** |

⇒ ⇒ ⚠️ **R5 是 B3 的门之一** ⇒ ⇒ **未闭 ⇒ B3 不得开工**。

### R6 · B3 的公开面前置门（**登记先于开工**）

| 步 | 内容 | 判据 |
|---|---|---|
| **R6.1** | `docs/CHANGELOG.md` +1 行（`data_ready` **语义重定义** ⇒ 下游按旧语义写的 TB 会断 ⇒ ⇒ 属**行为变更（非符号移除）**） | 两文件各 1 行 |
| **R6.2** | `.rddf/checks/public-api-surface.md`「行为变更」节 +1 行 | 同上 |
| **R6.3** | 复跑护栏 | `verify_public_api_visibility.sh` 绿 |

⇒ ⇒ ⚠️ ADR-068 §5.2 **V10** 明写这是**开工前置门，不是事后补记** ⇒ ⇒ **登记未做 ⇒ B3 不得开工**。

### R7 · P6-0 侦察（**只读** · 先于任何实现）

| 步 | 内容 | 判据 |
|---|---|---|
| **R7.1** | **复用面清单**（复用 / 新增）⇒ ⇒ ⚠️ `CsrBank`（`include/cpu/riscv/rv32i_csr.h`）· `ExceptionUnit`（`rv32i_exception.h`）**已存在** ⇒ ⇒ 缺口 = **指令级集成**，**非**从零实现 | 清单落进 `rv32i-isa-gap-closure-2026-11.md` |
| **R7.2** | **`FENCE` 裁决的候选方案**（单 hart + in-order + 无写缓冲 ⇒ 可否实现为 NOP）⇒ ⇒ ⚠️ **planner 无权拍板** ⇒ ⇒ 只出候选 | 候选 + 代价对照 |

⇒ ⇒ **R7 ⇒ R11**（FENCE 裁决在侦察之后）。

### R8 · D8（`ext_clk` 死端口）—— **需用户裁决**

| 步 | 内容 | 判据 |
|---|---|---|
| **R8.1** | 裁决 **A2**（删端口或明确注释）⇒ ⇒ ⚠️ **A1「真让它生效」另独立立项**（触 ADR-008 单时钟域 ⇒ ⇒ **不是** example 修复） | 决策请求单结案 |
| **R8.2** | 若删端口 ⇒ ⇒ **爆炸半径 = 4 处写点**（不是 2 处）：① 声明 `rv32i_pipeline_top.h:38`（`ch_in<ch_bool> ext_clk;`）② **驱动 `:137`（`pipeline.io().clk <<= io().ext_clk;`）** ← ⚠️ 这一行与「死端口」结论**表面矛盾**（它确实把 ext_clk 接到 pipeline 的 clk 端口）⇒ MUST 给**针对性实测**支撑「零影响」③ TB `examples/riscv-mini/tb_rv32i_soc.sv:173`（`.top_unnamed_input(ext_clk)`）④ `examples/riscv-mini/tests/test_rv32i_soc_dual_parity.cpp:535`（`sim.set_input_value(io.ext_clk, 0u)`）⇒ ⇒ ⚠️ **端口名会整体重编号**：`ext_clk` 是 `__io` 的**第一个** input（`:36-46`）⇒ 删掉后其余 4 个 `top_unnamed_input_N` **全部前移一位** ⇒ ⇒ TB 的 5 个映射 + TB 头注释（它**就是**端口映射文档）+ `.rddf/checks/examples-rtl-sim-targets.tsv` 的 riscv 行 MUST 同步重映射 | 4 处写点同步 + `RESULT: checks=24 fails=0` + `test_rv32i_soc_dual_parity` 绿 + **删前/删后同程序逐拍 trace 等价**（⚠️ 不接受「B2 已实测」作唯一证据 —— TB 恒驱 0 只能证明**值**无关，**不能**证明**删端口**无关）+ `ext_clk` 0/1 交替翻转 ⇒ 输出不变（针对性实测，支撑 `:137` 的「零影响」） |

### R11 · `FENCE` 语义裁决（**architect 域** · 须在 R7 之后）

| 步 | 内容 | 判据 |
|---|---|---|
| **R11.1** | 基于 R7 的候选方案（单 hart + in-order + **无写缓冲** ⇒ 可否实现为 **NOP**）⇒ ⇒ ⚠️ **planner 无权拍板**（ISA 语义）⇒ ⇒ 裁决归 architect | 裁决记录落 ADR 或 capability |
| **R11.2** | **载体** = `openspec/changes/<isa-gap>/specs/rv32i-isa-semantics/spec.md` 的 **ADDED Requirement**（`FENCE` 在「单 hart + in-order + 无写缓冲」下的语义与实现形态）⇒ ⇒ 条款 MUST 含 `SHALL`/`MUST`（RFC 2119） | `grep -c "FENCE"` 该 spec ≥ 1 + `verify_openspec_delta.sh` 绿 |
| **R11.3** | **审稿** = oracle → critic（ⓘ 可与 **R5** 合并成**一轮** architect 预算 —— 见 §5 ③） | 审稿记录落 `.rddf/reviews/` + **0 BLOCKER** |

---

## 3. 批次层面的后续内容（**开工门 + 出口**）

| 批 | 内容 | 开工门 | 出口 |
|---|---|---|---|
| **B2 收尾** | ① **验收报告**（**归属 = `verifier`（只验不改）** —— 判据来源按 `b1-batch-verify-2026-11.md` §首的四级优先级：用户条件 → proposal `## 验收标准` → 变更自身声明 → ADR Verification ⇒ ⇒ 出口 = 报告落 `.rddf/reviews/b2-batch-verify-2026-11.md` + 结论行（通过/不通过 + 数字））② R0 ③ R1/R2/R10 | — | `ctest -N` 全绿（⚠️ **禁写死「263/263」** ⇒ 写成「基线 + 新增 − 删除」）+ **14 护栏逐条列名**（B1 的 12 条 + `verify_ledger_evidence`(+self-test) + `verify_examples_rtl_sim`）+ examples **28/28** + samples **39/39** + 报告落盘 |
| **B3** | T1+T3 同批（D-TCM `aread`→`sread` + `data_ready` 语义重定义）· T6（store→load 同址）· T2/T4/T5 · L1–L6（装载链路） | **R5**（T2 裁决）+ **R6**（公开面登记） | ADR-068 的 V1–V10 全绿 + 时序 golden **手推重算**（禁抄仿真输出） |
| **B4** | C1 差分（先做）· C2 ALU 编码归一 · C3/C4 组件接入 · G3→G1+G4（同批）→ 其余 | **B3 已闭**（否则时序金标算两遍） | 组件接入后 20 项 golden + 计数锚点重算 |

⇒ ⇒ **★ 硬约束：B3 → B4 不得乱序**（`phase-6-rtl-productization.md` 依据 ④⑤）
⇒ ⇒ **★ B4 的 C1 差分 MUST 在 B3 之后**（`rv32i-dead-component-consolidation` 的进度行已同步此依赖）。

### 3.0 ⚠️ 三条**负面约束**（跨批必查 · 违反 = 静默破坏）

| # | 约束 | 依据 / 后果 |
|---|---|---|
| **① 不得改 `emit_hygiene` 的默认值** | `include/hierarchy_verilog.h:37` 的 `bool emit_hygiene = false;` ⇒ ⇒ 「**层次化产物逐字节不变**」这条 AC **只在该值 off 时成立**（`phase-6-rtl-productization.md` ★3 / 依据 ⑪）⇒ ⇒ 改了 ⇒ 4 份 golden + `verify_hierarchical_rtl.sh` 的 R1–R8 **全红** |
| **② 不得**在未获批准时**缩面** | 任何 R 若导致 `CTEST_TOTAL` / `VERILATOR_E2E_TOTAL` / `DUAL_BACKEND_PARITY_TOTAL` / `DUAL_BACKEND_PARITY_PAIRS` **下降** ⇒ MUST **先取用户显式批准** + 三处同步（锚点 + 派生表 + NOTES 登记）⇒ ⇒ 先例 = B0 的 `PARITY_PAIRS 290→284`（用户 2026-11-11 批准） |
| **③ 合入纪律** | MUST 按**路径** `git add`（⛔ **禁 `git add -A`** —— 实测曾把**另一个会话**未完成的两份文档纳入 commit）· 锚点编辑权 = **最后合入者** · 合入前 MUST **全量重建并重算** |

### 3.1 两条轨的交叉点（**实测确认**）

phase-6 的 **P6**（RV32I ISA 缺口）与 B4 的 **G1–G5** 是**同一片区域的两个视角**
⇒ ⇒ **实施前 MUST 确认 R7 的侦察结论已并入** `rv32i-isa-gap-closure-2026-11.md`
⇒ ⇒ **不得两条轨各改一次 ID 级**；P6 的**判据同步修改已获批准**（用户 2026-11-11）
⇒ ⇒ 改判据时 MUST 同时记「**原判据被什么取代、为什么**」。

---

## 4. 旧 T1–T8（2026-11-09 版）的**闭口 / 移交**对照

⚠️ **旧编号已停用**（避免与 R 编号混淆）⇒ ⇒ 保留在此仅供**追溯**。

| 旧 # | 内容 | 现状 |
|---|---|---|
| **旧-T1** | P5 收尾（change `mem-port-arg-cross-layer-2026-11` · AC1–AC10） | ✅ **已闭**（2026-11-10 · change 已归档 ⇒ 台账行 20 已收口 ⇒ 草案状态已转 `implemented` = **R10**） |
| **旧-T2** | P5 文档订正的同步（phase-6 P5 行 + change 文档） | ✅ **已闭**（2026-11-09） |
| **旧-T3** | P7 builder 步（三档 `PortPolicy` 语义分化） | 🟡 **部分完成** ⇒ ⇒ **续作 = R3**（②⑤ + 粒度 + 立项归档 + 闭口行 15） |
| **旧-T4** | P8 builder 步（删 stub） | ⬜ **未做** ⇒ ⇒ **续作 = R4** |
| **旧-T5** | P6 侦察 + 立项 | ⬜ **未做** ⇒ ⇒ **续作 = R7**（+ **R11** FENCE 裁决） |
| **旧-T6** | P10 的 ADR 判据订正（ADR-052 / ADR-063 / ADR-067 的 CLI 版本判据） | ✅ **已闭**（2026-11-10 · ADR-052/063/067 **v2** + ADR-066 **v1.5**） |
| **旧-T7** | P11（ADR-036 逻辑算子物理移除） | 🔒 **日历锁**（最早 **2027-04-17**）⇒ ⇒ **R9** |
| **旧-T8** | P9 遗留（真 FIFO 写缓冲 / 真预取 / `miss` 期间 pc 稳定性核对） | ⬜ **明确不在本窗口** ⇒ ⇒ 需**新 improvement**（走 `add-improve` / HARD-GATE）⇒ ⇒ ⚠️ 若新实现需要**新的架构取舍**（wrapper `ready` 背压契约 / 预取语义）⇒ ⇒ **另需 architect 裁定** |

---

## 5. ⚠️ **须用户裁决 / 需新预算**的事项

| # | 事项 | 说明 |
|---|---|---|
| ① | **R8（D8 `ext_clk` 死端口）** | 建议 **A2**（删端口或明确注释）⇒ ⇒ **A1 另独立立项** ⇒ ⇒ 需用户点头 |
| ② | **R5（T2 裁决）的 architect 评审预算** | 属**决策内容变更** ⇒ ⇒ **oracle → critic 全审** ⇒ ⇒ 需新一轮 architect 预算（复盘 #2 的审稿预算已用于复盘本身） |
| ③ | **R11（FENCE 裁决）的 architect 评审预算** | 同上 ⇒ ⇒ 但可与 R5 合并成**一轮** architect 预算 |
| ④ | **T8（P9 遗留）的独立立项** | 真 FIFO 写缓冲 / 真预取 ⇒ ⇒ 需 planner 另起 improvement ⇒ ⇒ ⚠️ 可能需 architect 裁定 ⇒ ⇒ **本窗口不并入**（避免与 R3 并行制造文档债） |

---

## 6. 判据自检（**可复跑**）

```bash
cd /workspace/project/CppHDL
bash scripts/verify_ledger_evidence.sh              # R0 ⇒ PASS（91 台账 · 6 项待补 = WARN）
bash scripts/verify_ledger_evidence.sh --self-test   # R0 ⇒ PASS（投毒转红 + 还原复绿）
grep -c "E6 ⬜ 未做" .rddf/roadmap/phases/phase-6.md                # R1 ⇒ 0
grep -c "① ✅ / ② ⬜ / ③ ✅" .rddf/roadmap/phases/phase-6.md        # R2 ⇒ 1
grep -m1 "^状态:" .rddf/improvements/mem-port-arg-cross-layer-2026-11.md   # R10 ⇒ implemented
grep -m1 "^状态:" .rddf/improvements/examples-rtl-sim-coverage-2026-11.md # ⇒ implemented
bash scripts/verify_examples_rtl_sim.sh              # ⇒ PASS（10 项 · 严格模式）
bash scripts/verify_arch_ledger_residuals.sh          # ⇒ PASS（双向 1:1）
```

⇒ ⇒ ⚠️ **R3/R4/R5/R6/R7/R8/R11 的判据随各自实施落地**（本文件只登记路径，不预写结果）。

---

## 7. metis 审查处置记录（**planner 的固定审查者** · 2026-11-12）

> **触发**：本文件重写 **160 增 / 188 删 = 140%** ⇒ 远超「重写 ≥ 1/3 ⇒ MUST 审」的门槛。
> **metis 结论**：**「不可以直接交给 builder 实施」** ⇒ ⇒ 卡点 = **两条会让实施者做出错东西的实质缺口**
> （不是文档质量问题）⇒ ⇒ **全部 14 条建议已逐条处置** ⇒ ⇒ **现在可以交给 builder**。

| # | metis finding | 处置 |
|---|---|---|
| **P0-1** 🔴 | R3.2 的元组第 2 项「诊断列出的边数」**在现有 API 上不可观测**（Warn 档 `CHWARN` 只进日志 ⇒ `src/hierarchy_verilog.cpp:1645-1647`；`result.warnings` 只在 `:123`/`:176` 被写） | ✅ **采纳** ⇒ 新增 **R3.2a**（前置：把 `synth_events` push 进**已存在**的 `result.warnings` ⇒ **不新增公开字段**）+ R3.2b 的元组改用 `result.warnings` |
| **P0-2** 🔴 | R5 把「**三选一**」写成「A/B 二选一」⇒ **丢了 A2**，而 `memory-timing-contract:60` 明写「MUST 展开成三选一 + 每项的预测计数器表」 | ✅ **采纳** ⇒ R5.1 改 **A1 / A2 / B 三候选** + 补「每项预测计数器表（`load_use_stalls` / `control_stalls` / `instructions` / `non_retired`）」 |
| **P0-3** 🔴 | R8 爆炸半径**少算一半**（漏 `rv32i_pipeline_top.h:137` 的 `pipeline.io().clk <<= io().ext_clk;`）+ **端口名会整体重编号**（`ext_clk` 是第一个 input） | ✅ **采纳** ⇒ R8.2 改 4 处写点 + 端口前移一位说明 + 判据升级（删前/删后逐拍 trace 等价 + `ext_clk` 0/1 翻转实测） |
| **P0-4** 🔴 | R3.1 判据「诊断文本不再含合成端口名」**纯负向** ⇒ 打印空串也过；且「三档都不许有端口名」**违反 ADR-060 v1.7**（Fatal 档 MUST 列端口名） | ✅ **采纳** ⇒ R3.1 限定**仅 Reject 档** + Fatal 档例外 + 3 条判据（含负向） |
| **P1-5** | R2 的 `grep -c` 只钉 5 个子步里的 3 个 ⇒ 把 ④/⑤ 写成 ✅ **仍绿** | ✅ **采纳** ⇒ 改「P7 行为作用域 + 5 项各判」+ 记录全文基数 |
| **P1-6** | R4.3 的分子 `grep -o` **把注释算进来**（10 处匹配里 5 处在注释）+ 计数判据**恒绿**（`SELECT`↔`MUL` 换名 5→5）+ 新建台账会打破 R0 的「91 台账/6 待补」+ **没挂 CI** | ✅ **采纳** ⇒ 分子改**集合**（剥注释）+ 判据改集合相等/超集 + 基线集落盘 + **该文件 MUST 带 `evidence:`**（`ledger_evidence.py:42` 是全目录 glob）+ **MUST 挂 CI** |
| **P1-7** | R4.4「文档面 0 命中」与 R4.2「移除登记必须指名」**互斥** | ✅ **采纳** ⇒ 拆两条判据 + **排除** `docs/CHANGELOG.md` 与 `.rddf/checks/public-api-*` |
| **P1-8** | §1.1 的单点资源**漏 2 个**（`arch-residual-registry.md` + 台账 §7 —— ADR-068 **V3** 要求 B3 登记残差 ⇒ 与 R3.5 同写）；`AGENTS.md` 派生表**永久 stale 且无护栏**（实测 4 对全部不一致） | ✅ **采纳** ⇒ 资源 **3 → 5** + 派生表处置（扩脚本 或 标「历史快照」+ grep 断言） |
| **P1-9** | §3「B2 收尾」① **归属未写清**（验收报告 = **verifier 域**）+ 「12 护栏」未列名 + 「263/263」硬编码（违反自己的「禁写死」） | ✅ **采纳** ⇒ 归属 = `verifier`（只验不改）+ 判据来源四级优先级 + **14 护栏逐条列名** + 改「基线 + 新增 − 删除」 |
| **P1-10** | R11 **完全没有实现路径**（§1 只有一行） | ✅ **采纳** ⇒ 补 §2 R11.1–R11.3（载体 = ADDED Requirement + 判据 + 审稿） |
| **P2-11** | §0 与 §4 的 `T1` **指两个不同任务** | ✅ **采纳** ⇒ 旧编号全部加 **`旧-` 前缀** + §0 加禁令 + **同步 `phase-6.md:40-53` 的 T1–T8 指针**（⚠️ 这是本轮重写**新引入**的漂移） |
| **P2-12** | R10 漏了正文的旧状态（`:20` 的 `**Status**: ✅ **approved**`） | ✅ **采纳** ⇒ 追加订正行（**不改写历史叙述**）+ 判据补 `grep -c "Status\*\*: ✅ \*\*approved"` == 0 |
| **P2-13** | §3 缺**负面约束**（`emit_hygiene` 默认值 / 缩面须批准 / `git add -A` 禁令） | ✅ **采纳** ⇒ 新增 **§3.0 三条负面约束** |
| **P2-14** | R3.4 的出口判据**用错脚本**（`verify_openspec_delta.sh` 是**提案阶段**自检，不含「归档成功」）+ R3.5 缺 3 条格式硬约束 | ✅ **采纳** ⇒ R3.4 改「archive 路径存在 + 主 spec 新增 Requirement 可 grep」+ `--skip-specs` 的合法例外判据；R3.5 补 3 条格式约束（`path1,path2` / 后缀 / `--render`） |

### 7.1 metis 提的**顺手修**（已在同一批落地）

| 项 | 状态 |
|---|---|
| `stream-builder-chain-silent-word-loss-2026-11.md:9` 的 `evidence: ...cpp---` **粘连**（靠 `raw.split("---", 2)` **侥幸**解析成功） | ✅ **已修**（`---` 独立成行） |
| `AGENTS.md:516-520` 的派生表 vs 锚点（实测 4 对全部不一致） | ⚠️ **登记为待办**（§1.1 已写处置：扩脚本 或 标「历史快照」）—— ⚠️ **本文件不改**（`AGENTS.md` 的锚点编辑权 = 最后合入者 ⇒ 且需与实施批同批重算） |

### 7.2 metis 的**权限说明**（归档留痕）

> metis 报「本轮 `shell` 工具**被整体拒绝**」⇒ ⇒ 它的「实测」全部是**读源码/读脚本实现**得出的事实
> （不是执行结果）⇒ ⇒ ⚠️ 因此它的 6 条 P0/P1 关键断言**已由 planner 逐条代跑核实**：
> ① `HierarchyEmitResult` 确有 `warnings` 字段（`include/hierarchy_verilog.h:100`）且 Warn 档 CHWARN 不进它 ✅
> ② `memory-timing-contract:60` 确写「三选一」✅ ③ `rv32i_pipeline_top.h:137` 确有 `pipeline.io().clk <<= io().ext_clk;` ✅
> ④ `AGENTS.md` 锚点 263/76/65/284 vs 派生表 257/246/77/66/290 ✅ ⑤ `stream-builder` 的 `---` 粘连 ✅
> ⑥ 8 护栏全绿（`verify_ledger_evidence`+self-test / `verify_examples_rtl_sim` / `verify_arch_ledger_residuals` /
> `verify_roadmap_fragments` / `verify_public_api_visibility` / `verify_openspec_delta` / `verify_test_counts` /
> `verify_dual_backend_parity`）✅

---

## 8. 🟢 **新会话 builder 的实施入口**（RTL 轨 · 2026-11-12）

> **本节是「下一个会话从哪儿开始、读什么、按什么顺序做」的**唯一入口**。
> ⚠️ 上游 = `.rddf/checks/rtl-track-correction-list-2026-11.md`（7 份草案的**状态/簿记订正** ·
> oracle 第 1 轮 13 条 · critic 第 1 轮 2 BLOCKER → 第 2 轮 **APPROVE** · 修订执行记录见其 §9）。
> ⇒ ⇒ 本节**不复制**任何状态格 / 判据 ⇒ ⇒ 全部**指路**到唯一真相。

### 8.1 读序（4 步 · 别跳）

| 步 | 读什么 | 得到什么 |
|---:|---|---|
| **1** | 本节 §8.2 的「权威载体表」 | **每个事实的唯一出处**（避免读多份互相打架的文档） |
| **2** | `.rddf/roadmap/phase-6-rtl-productization.md`（批次表）· §0.0 状态词表 · §B0–§B4 · §4 独立立项 | **批次编排 + 哪些项归哪批** |
| **3** | 目标草案的 `## Acceptance` 段 | **逐条判据 + 勾选状态 + 证据指针 + `⇒ 归属:` 标注** |
| **4** | `.rddf/roadmap/phases/phase-6.md` | **任务状态唯一真相**（B0–B4 状态格 · 🚪 入口门） |

### 8.2 权威载体表（**别在别处找**）

| 事实 | **唯一真相** | 其它地方的角色 |
|---|---|---|
| 某个 work item **做没做完** | 对应草案 `## Acceptance` 的**勾选 + 证据指针** | 批次表只写**编排**，不复制状态格 |
| 某个 work item **归哪一批** | 草案 AC 行尾的 `⇒ 归属: **B3/B4**` | 批次表 §B3/§B4 是**交叉索引** |
| 批次**状态** | `phase-6.md` 的 B0–B4 状态格 | `phase-6-rtl-productization.md` **不复制**状态格 |
| **未纳入本轮**的缺口 | 批次表 §4「独立立项」清单 | 草案 `进度:` 字段**指路**到 §4 行 |
| 验收**证据** | `.rddf/reviews/*.md`（verifier 域） | 草案 AC **只引用**，不复制正文 |
| 测试**计数** | 根 `AGENTS.md` 的 CI 锚点（263 / 76 / 65 / 284 / 17） | ⚠️ 派生表是**历史快照**，不参与判定 |
| 端口命名**规则** | capability `rtl-hierarchical-output` | guard capability **MUST NOT** 复述 |

### 8.3 未完成 work item 的**逐条索引**（共 **70** 条 · 全部带 `归属`）

| 批次 | 草案 | 未勾 AC 数 | AC 段位置 |
|---|---|---:|---|
| **B3** | `memory-timing-contract-2026-11` | **17** | `## Acceptance`（T1–T5 + T2′ + T6 + 不回归） |
| **B3** | `firmware-load-chain-2026-11` | **18** | `## Acceptance`（L1–L6 + 不回归） |
| **B4** | `rv32i-dead-component-consolidation-2026-11` | **10** | 差分段（4）+ 接入段（6）⇒ ⇒ **C1–C4** |
| **B4** | `rv32i-isa-gap-closure-2026-11` | **22** | `## Acceptance`（G1–G5 + 边界 + 不回归） |
| **独立立项** | `port-naming-ratchet-2026-11` | **3** | 存量消化（2 · §4「N2 补遗」）+ `CHERROR` 判别力（1 · §4「N3 补遗」） |

⇒ ⇒ ⚠️ **B3 必须先于 B4**（否则时序金标算两遍）⇒ ⇒ 理由与依赖链见批次表 §2 的硬约束。

### 8.4 B3 的**开工门**（两道 · 都过才能动手）

| 门 | 任务 | 归属 | 当前态 |
|---|---|---|---|
| **门 1** | **R5** = T2 裁决（MEM 数据对齐 A/B）⇒ ⇒ 载体 = **新立 ADR-069** | architect | ⬜ **未做**（ADR-068 **已 Accepted** ⇒ 只差 T2 的 A/B 选型） |
| **门 2** | **R6** = 公开面前置门（`docs/CHANGELOG.md` + `.rddf/checks/public-api-surface.md` 各一行） | planner | ⬜ **未做** ⇒ ⇒ **登记未做 ⇒ B3 不得开工** |

⇒ ⇒ 判据见 §2 的 **R5** / **R6** 段。

### 8.5 开工前的**护栏清单**（10 个 · 全绿才算干净起点）

```bash
bash scripts/verify_agents_size.sh            # 根 AGENTS.md 体积
bash scripts/verify_test_counts.sh           # 计数锚点
bash scripts/update_test_count_anchors.sh --check
bash scripts/verify_roadmap_fragments.sh     # AUTO 块守恒
bash scripts/verify_ledger_evidence.sh       # 台账证据
bash scripts/verify_public_api_visibility.sh # 公开面登记
bash scripts/verify_flat_rtl_emit.sh         # 扁平产物护栏 F1–F8
bash scripts/verify_port_naming.sh           # 端口命名棘轮
bash scripts/verify_examples_rtl_sim.sh      # examples 独立 RTL 仿真
bash scripts/verify_hierarchical_rtl.sh      # 层次化产物 R1–R8
(cd build && ctest -N | tail -1)             # Total Tests: 263
```

⇒ ⇒ **全部在 2026-11-12 实测 exit 0** ⇒ ⇒ 下一个会话**若某条转红**，先查是不是上一会话引入的。

### 8.6 ⚠️ 三条**负面约束**（跨批必查）

| # | 约束 | 依据 |
|---|---|---|
| **N-1** | **禁**写死测试数（`257` / `263`）⇒ ⇒ 一律写「`CTEST_TOTAL` + 新增 − 删除 == `ctest -N`」 | 退役会**减少**用例数 ⇒ ⇒ 写死即过期 |
| **N-2** | **禁**用 blanket `lint_off` 掩盖宽度问题 ⇒ ⇒ `emit_hygiene` **默认 off**（默认产物逐字节不变） | 批次表 ★3 |
| **N-3** | **禁** `git add -A` ⇒ ⇒ MUST 按**路径显式** `git add <path>` | 仓内多人 / 多会话并行（曾误纳他人未完成文档） |

### 8.7 下一个会话的**验收口径**（不要照抄旧数字）

- **不要**写「既有 257 个 ctest 全绿」⇒ ⇒ 写「`CTEST_TOTAL` == `ctest -N`」。
- **不要**把「两后端 parity 全绿」当正确性证据（同源 ⇒ 同构缺陷一致地错）⇒ ⇒ 期望值 MUST **手推**。
- **不要**在 `describe()` 里用硬件值做 C++ 控制流 ⇒ ⇒ 条件恒假 ⇒ 分支体**从不发射**。
- **期望值 MUST 手推**（禁抄仿真输出）· **突变纪律**：还原源码 ≠ 复原（**二进制也是状态** ⇒ 还原后 MUST 重建）。
