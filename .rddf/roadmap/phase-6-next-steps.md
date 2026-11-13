# Phase 6 后续任务清单 + 实施路径（planner 域 · **2026-11-12 修订**）

> **触发**：用户口令「切 architect 对完成和后续任务进行梳理……然后切 planner 对后续还有哪些任务内容都梳理一遍，
> 再给出后续任务内容和实施路径」。
> **上游**：**`docs/architecture/2026-11-12-phase-6-rtl-track-review.md`**（architect 复盘 #3 · **T2 裁决后**
> · 目标链 + **8 条实测发现** + **4 条边界裁决** + T-a–T-j 任务清单 · oracle ×1（8 条建议**全落实**）+
> critic ×1（**APPROVE_WITH_CONDITIONS** · **0 BLOCKER** · 1 SHOULD_FIX + 2 NITS **全修完**））。
> ⚠️ **复盘 #2**（`2026-11-12-phase-6-b2-midterm-review.md`）= **B2 中期时点的上游快照** ⇒ ⇒ 它的 4 条发现
> **已全部闭口**（R0 / R1 / R2 / R10 + R5 / R6）⇒ ⇒ **本文件以 #3 为准**。
> **窗口唯一真相** = `.rddf/roadmap/phases/phase-6.md`（**任务状态以 phase-6 为准**；本文件是
> **任务内容 + 实施路径**的载体，**不复制** phase-6 的状态格）。
> **本文件不新增任何架构义务** —— 所有判据均映射**既有** AC / phase-6 出口判据 / architect 复盘结论。

---

## 0. ⚠️ 编号约定（**避免与 architect 复盘的 T 编号冲突**）

| 编号 | 含义 | 出处 |
|---|---|---|
| **R0–R17** | **本文件**（2026-11-12）的后续任务清单 | 本文件 §1–§3 |
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
| **R0** | ✅ **修 `verify_ledger_evidence.sh` 的 10 项红**（10 份台账的 `evidence:` ⇒ 裸路径 / 升级形态）⇒ ⇒ **解 CI 红** | planner | — | ✅ **2026-11-12 已闭**（PASS · 91 台账 · 6 项待补 = WARN · `--self-test` PASS）|
| **R1** | ✅ 订正 `phase-6.md` 的 **B2 行**（删「E6 ⬜ 未做」那段陈旧快照） | planner | — | ✅ **2026-11-12 已闭**（`grep -c "E6 ⬜ 未做"` == **0**）|
| **R2** | ✅ 订正 `phase-6.md` 的 **P7 行状态格**（①③ ✅ · ②⑤ ⬜ · ④ 🟡 半闭）⇒ 与 next-steps 的 T3 遗留段一致 | planner | — | ✅ **2026-11-12 已闭**（`grep -c "① ✅ / ② ⬜ / ③ ✅ / ④ 🟡"` == **1**）|
| **R3** | 🟡 **P7 的 ②⑤ + Reject 粒度 → change 立项/归档 → ④ 台账行 15 闭口** | builder → architect | R2 | ⇒ ⇒ **④ 的依赖 = change 归档，不是依赖 ⑤** ⇒ ⇒ 顺序：修粒度 → 补判据 → 立项归档 → 闭口行 15 |
| **R4** | 🟡 **P8 builder 步**（删 `set_ab_verification` stub + golden 下限派生 + 两份文案订正） | builder | — | 与 R3 **可并行**（写点不重叠：`include/simulator.h`+`src/simulator.cpp` vs `src/hierarchy_*.cpp`） |
| **R5** | ✅ **B3 的 T2 裁决**（MEM 数据对齐）⇒ ⇒ 载体 = **ADR-069**（**Accepted · 2026-11-12**）⇒ ⇒ 决策 = **候选 ②（A0）**（地址照旧 + MEM/WB 延后 1 拍）· oracle **×3**（第 1/2 轮 REJECT ⇒ 第 3 轮 PASS-WITH-NOTES）· critic **×1**（APPROVE_WITH_CONDITIONS · **0 BLOCKER**） | architect | 用户 2026-11-12「按你建议拍板」 | ★ **B3 的🚪 门之一 ⇒ ✅ 已闭** ⇒ ⇒ ✅ **2026-11-12 晚补齐 SF-3 强制项**：**ADR-047 §5.5 dated addendum**（3 处「3 拍」原文**逐字保留** + 三处 dated 指针）⇒ ⇒ ⚠️ **spec MODIFIED 已重分类**为 **R14 内的 change 步骤**（承载**实施后**的真实基线 ⇒ ⇒ 提前写 = 写未验证的值）⇒ ⇒ 审稿记录 `.rddf/reviews/ADR-069-mem-data-alignment.md` |
| **R6** | ✅ **B3 的公开面前置门**（CHANGELOG + `public-api-surface.md` 各一行） | planner | — | ★ **B3 的🚪 门之二** ⇒ ⇒ ✅ **已登记**（2026-11-12 · 前置登记 = 预告 + 迁移指引 · 非「已发布」）⇒ ⇒ `firmware-load-chain` L1–L6（加载端口族语义变更）+ `memory-timing-contract` T1/T3（`data_ready` 语义重定义）⇒ ⇒ 护栏 `verify_public_api_visibility.sh` PASS |
| **R7** | 🟡 **P6-0 侦察**（RV32I ISA 缺口：复用面清单 + `FENCE` 裁决候选） | planner → architect | — | **只读侦察** ⇒ ⇒ 可全程并行 |
| **R8** | 🟢 **D8**（`ext_clk` 死端口）⇒ ⇒ 建议 **A2**（删端口或明确注释） | architect → 用户 | — | 需**用户裁决** ⇒ ⇒ 与所有批**无耦合**（不改任何时序金标） |
| **R9** | 🔒 **P11**（ADR-036 物理移除） | builder | 日历锁 **2027-04-17** | 本窗口**不做** |
| **R10** | ✅ **第三处状态漂移**（`mem-port-arg` 草案 frontmatter） | planner | — | ✅ **2026-11-12 已闭**（frontmatter = `implemented`）⇒ ⇒ ⚠️ **但索引列当时漏改** ⇒ ⇒ 已并入 **R12**（发现 6）|
| **R11** | 🟡 **`FENCE` 语义裁决**（architect · **planner 无权拍板**） | architect | R7 | 须在 R7 侦察之后（侦察提供候选方案）⇒ ⇒ 可与 B3 并行 |
| **R12** | ✅ **12 条漂移一次性扫干净**（architect 复盘 #3 的发现 1/2/4/5/6/7 + 发现 8 的 5 处）⇒ ⇒ 真相源 / 状态格 / 公开面载体 / 索引列 / 指针 | planner | — | ✅ **完成态 = 已提交**（metis P2-18：判据只看**工作区文本** ⇒ ⇒ 未提交时一次 `checkout` 就把它变回「未执行」而判据仍绿）⇒ ⇒ **漂移订正 = `83cdb89`** · **清单刷新 = 本 commit** ⇒ ⇒ 判据见 §2 的 **R12**（逐条可 grep）|
| **R13** | 🔴 **B3 立项**（**两个** change：`memory-timing-contract` + `firmware-load-chain`）⇒ ⇒ 载体 = `openspec/changes/<name>/{proposal,design,tasks}.md` | planner → builder | R12 | ★ **B3 的真起点** ⇒ ⇒ 🚪 门 R5 + R6 **已闭** ⇒ ⇒ **只差立项** |
| **R14** | 🔴 **B3 实施**（T1+T3 同批 · T6 · T2/T4/T5 · L1–L6）⇒ ⇒ 出口 = **ADR-068 的 V1–V10 + ADR-069 的 V1–V15 全绿** + 时序 golden **手推重算** | builder | R13 | ⚠️ **必须在 R15 之前**（时序金标只算一遍）⇒ ⇒ **两个 change 同批**（T1 与 T3 耦合） |
| **R15** | 🔴 **B4 实施**（C1 差分 → C2 ALU 编码归一 → C3/C4 组件接入 → G3→G1+G4 同批 → 其余） | builder | **R14 已闭** | ⚠️ **必须等 R14 闭**（同依据） |
| **R16** | 🟡 **独立立项 12 条**（批次表 §4 的 **11 行** + **台账 id=22**）⇒ ⇒ 含**用户 ⑤ 的端口名消化**（⇒ ⇒ 与 B3 **同区** ⇒ ⇒ **不可并行**，见 R17） | planner → architect | — | ⚠️ **逐项写点**（metis P1-14：不可笼统写「`src/hierarchy_*`」）⇒ ⇒ **大部分**与 B3/B4 零重叠 ⇒ ⇒ **但端口名消化项落在 `examples/riscv-mini/src/rv32i_pipeline_top.h` + 10 个 TB 的 tsv** ⇒ ⇒ **与 B3 的 L1–L6 同区** ⇒ ⇒ ⚠️ **2026-11-13 +2 条**（F-1 / F-2 · B0 批退役遗留的两条 CI 红 ⇒ ⇒ 用户 2026-11-13 裁决「**独立立项 · 本轮不做**」）⇒ ⇒ ⇒ §4 由 **9 行 → 11 行** |
| **R17** | 🔴 **用户 ⑤ 的原始诉求：RTL 端口名 == C++ 成员名**（32 端口 · `Rv32iPipelineTop`）⇒ ⇒ ✅ **R17.1 已闭（2026-11-13 · architect 裁量）=「现在做」**，**放置 = 独立于 B3 的小批**（先于 B3 执行）⇒ ⇒ ⚠️ **与 B3 的 L1–L6 在 `rv32i_pipeline_top.h` 上同区 ⇒ MUST 顺序执行**（依据 `firmware-load-chain-2026-11.md:197` 的爆炸半径）⇒ ⇒ ⇒ **本轮交付 = R17.2** | architect（已闭）→ **builder** | — | ★ **用户诉求仍未在真实 CPU 上兑现**：`Rv32iPipelineTop` 的 32 个端口**全部未具名** · 棘轮豁免 = **1484/1484** ⇒ ⇒ **零组件被消费** ⇒ ⇒ ⚠️ 做完会改该 SoC 的**产物端口名** ⇒ ⇒ MUST 与 examples 的 tsv / `examples-rtl-expectations.md` / 10 个 TB **同批** ⇒ ⇒ ✅ **回退成本 = 一次 revert**（改动自洽 ⇒ ⇒ 无沉没成本 ⇒ ⇒ 用户可随时推翻）|
| **R18** | 🟡 **R1 = 端口面残留登记**（扁平顶层暴露 **80** 个内部 `ch_out` · `.v` 114 端口 vs 根组件自有 32 ⇒ ⇒ **台账无行** ⇒ ⇒ 请求补登记为已知残留） | planner → architect | **用户裁决** | ⏳ **已立单**（`decision-request-port-face-2026-11.md` R1）⇒ ⇒ ⚠️ **不是待裁决的现状**（C2 已于 2026-11-11 由用户撤回）⇒ ⇒ 只差「补台账行」这一步 |
| **R19** | 🟡 **R2 = F6 判据缺口**（护栏 F6 =「当前不适用」⇒ ⇒ 而 AC 已改成可判别形态 ⇒ ⇒ **护栏未实现 AC** ⇒ ⇒ backend 端口表与产品端口面**无判据**）⇒ ⇒ **两条路径**：① `ctest -L verilator_e2e`（**不**必须动工具设计 ⇒ ⇒ 但新增 `BUILD_VERILATOR=ON` 环境依赖 ⇒ ⇒ **fail-closed 语义待定**：无 verilator 环境 exit 0 还是 2）② 给工具自测设计加一个 `ch_reg`（⇒ ⇒ 工具产物端口面变化 ⇒ ⇒ F3 / F8 期望集要同步） | 用户 → builder | **用户裁决** | ⏳ **已立单**（R2）⇒ ⇒ ⚠️ **两条各有取舍 ⇒ ⇒ 不擅自选** |
| **R20** | 🟡 **R3 = `gpio_example` 断言强度**（`spec_checks = 0` ⇒ ⇒ 护栏**自标**「impl-contract only（只能防漂移，不能证明符合规范）」⇒ ⇒ 10 项里**唯独它**不能证明符合规范）⇒ ⇒ 建议：补 spec 层断言；**仓内若无 gpio 的规范依据 ⇒ ⇒ 退「显式标注 impl-contract only」** ⇒ ⇒ ⚠️ **禁**用「移出首批」削覆盖面 | 用户 → builder | **用户裁决** | ⏳ **已立单**（R3）|
| **R21** | 🟡 **目标 ④（RV32I 层次化 RTL 示例）= 独立立项** ⇒ ⇒ ⚠️ **范围 MUST 先侦察**（architect 裁决 ③）：**不是**「修发射器的静默退化」（默认 inline 是**契约行为**·有测试钉着）⇒ ⇒ 而是「**诉求 ④ 未兑现**」+「**跨 module 隐式端口合成未实现 ⇒ 分析层 fail-closed 抛异常**」⇒ ⇒ ⇒ **规模大于「加一行 `CH_HIERARCHY_LAYER`」** | planner → architect | — | ⬜ **未立项** ⇒ ⇒ 载体 = 新 improvement 草案（`add-improve` HARD-GATE）⇒ ⇒ ⚠️ **立项前 MUST 先做规模侦察** ⇒ ⇒ **不并入 B3 / B4**（写点 `src/hierarchy_*` 不重叠）|

⇒ ⇒ ✅ **两条 🚪 门（R5 + R6）已全闭**（2026-11-12）⇒ ⇒ **B3 只差「立项」（R13）**。
⇒ ⇒ ✅ **R0 已闭**（CI 红已解）· **R1 / R2 / R10 已闭** · **R12 已闭**（2026-11-12 晚提交 `83cdb89` / `d6c731c`）⇒ ⇒
⇒ ⇒ 🎯 **本轮 = R17.2**（architect 裁量闭 R17.1 · 2026-11-13 · 依据见 §2 的 R17.1）⇒ ⇒ **主线 = R13 → R14 → R15**（B3 未立项 ⇒ ⇒ **本轮不动主线**）
⇒ ⇒ **旁线 = R3 / R4 / R7 → R11 / R8 / R16**（写点不重叠 ⇒ ⇒ 可并行 ⇒ ⇒ 但**收口阶段不并行**，见 §1.1）
⇒ ⇒ **待用户裁决 = R8 / R18 / R19 / R20**（已立单）· **待立项 = R21**（须先侦察）。

### 1.1 「可并行」的真实边界（metis 订正 · 只覆盖**写点** ⇒ ⇒ **收口阶段不并行**）

三条轨共享**五个**单点资源 ⇒ ⇒ ① `openspec/specs/rtl-hierarchical-output/spec.md`
（**仅 R3 动** —— B3 的 spec 面在 rv32i / dtcm / manifest `semantic`）② 根 `AGENTS.md` 的
**5 个计数锚点**（`CTEST_TOTAL` / `VERILATOR_E2E_TOTAL` / `DUAL_BACKEND_PARITY_TOTAL` / `DUAL_BACKEND_PARITY_PAIRS` / `EXAMPLES_TEST_SOURCES`；前 4 个各 **2 处** ⇒ ⇒ 文本必冲突）③ `.rddf/checks/public-api-*`（R4 与 R6 都动
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

⇒ ⇒ **R0/R1/R2/R10 不碰这 5 个资源**（只动台账 frontmatter + 状态格）⇒ ⇒ 与任何实施批**无冲突**。
⇒ ⇒ ⚠️ **「5 个计数锚点」的清单**（metis P2-16）：`EXAMPLES_TEST_SOURCES` 也**必须**进「锚点编辑权 = 最后合入者」的清单
⇒ ⇒ 否则 B3/B4 新增 example 测试时它会**漏改** ⇒ ⇒ ⚠️ **禁**只盯着前 4 个。

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

### R12 · 12 条漂移扫干净（**architect 复盘 #3 的发现**）—— **已完成 2026-11-12 晚**

> ⚠️ **本节是「真相源 + 状态格 + 公开面载体 + 索引 + 指针」的统一订正** ⇒ ⇒
> ⚠️ **AC 计数 MUST 用「行首 + `<br>` 内联」双计**（仓内已确立 · `rtl-track-correction-list-2026-11.md:152`
> ⇒ 明写「只按行 grep 会漏」）⇒ ⇒ **禁**按行首单计。

| # | 位置 | 订正内容 | 判据（可 grep） |
|---|---|---|---|
| **①** | `phase-6.md:75`（B3 行） | 「② **T2 尚未裁决**」→「② **T2 已裁决**（ADR-069 · Accepted · 决策 = A0）」· 补 🚪 门 R6 已闭 + 仍欠「两个 change 立项」+ 实施期必落两份 MODIFIED | `grep -c "T2 尚未裁决" phase-6.md` == **0** |
| **②** | `next-steps.md` §1 R 表 | R0 / R1 / R2 / R10 四行加 ✅ 完成标记 | `grep -c "^| \*\*R0\*\* | ✅" …` == 1（四行逐条） |
| **③** | `next-steps.md` §1 R5 行 | 「仍欠 ① ADR-047 addendum ② spec MODIFIED」⇒ ⇒ ① 改为「✅ 已补齐」· ② 改为「**已重分类**为 R14 内的 change 步骤」 | ⚠️ **判据 MUST 限定 R5 行**（全文 grep 会命中本表自身）⇒ ⇒ `awk '/^\| \*\*R5\*\* \|/'` 后 `grep -c "仍欠"` == **0** |
| **④** | `docs/CHANGELOG.md:19` | 「ADR-069（`Draft` · **未决**）」→「ADR-069（**Accepted** · 2026-11-12 · 决策 = **A0**）」 | `grep -c "ADR-069（\`Draft\`" docs/CHANGELOG.md` == **0** |
| **⑤** | `next-steps.md` §8.3 | 补 `port-name-literal-parity`（2 条）+ `node-name-prefix-consistency`（2 条）两行；`rv32i-isa-gap-closure` 的 **22** **保留**（行 18 + 内联 4）· 合计 **70 → 74** | `grep -c "port-name-literal-parity" §8.3` ≥ 1 且合计 == **74** |
| **⑥** | `improvement-suggestions.md` 状态列 | `mem-port-arg` draft→**implemented** · `chlib-dtor` approved→**approved**（保持）· `public-api-remove-visibility` approved→**draft** ⇒ ⇒ **12 份索引与 frontmatter 全一致** | 12 份逐条比对 = **0 不一致** |
| **⑦** | `followup-list` §7.5 + `memory-timing-contract:8` 进度行 | 「下一件事 = 用户裁决 ADR-069」⇒ ⇒ 改为「B3 立项 → B3 实施 → B4 → 独立立项」；「T2 待 architect 裁定」⇒ ⇒ 「T2 已裁决（A0）」 | `grep -c "用户裁决 ADR-069" followup-list` == **0** |
| **⑧** | `improvement-suggestions.md` 两行（`firmware-load-chain` / `memory-timing-contract`） | 「🚪 入口门 R5 + R6」⇒ ⇒ 「✅ 已闭 · 只差立项」·「T2 裁决待 architect」⇒ ⇒ 「已裁决」 | 两行均含「R5 + R6 已闭」 |
| **⑨** | `firmware-load-chain:8` 进度行 | 「前置门 = 公开面登记未做 ⇒ 不得开工」⇒ ⇒ 「✅ 前置门已闭」 | `grep -c "前置门 = 公开面登记未做" …` == **0** |
| **⑩** | 根 `AGENTS.md`（RV32I 数据前推段） | 加 **2026-11-12 dated 前向注记**：「3 拍」在 B3 实施前仍属实 · 但「零改动」已前向为假（ADR-069 → 4 拍）· **B3 实施前禁改**（那是未验证的值） | `grep -c "2026-11-12 dated 前向注记" AGENTS.md` == **1** |
| **⑪** | `rv32i-isa-gap-closure:154` | 「3 拍 load-use 窗口」⇒ ⇒ 标注「B4 实施时 N = **4**」·「禁沿用 3」 | `grep -c "B4 实施时 N = 4" …` == **1** |
| **⑫** | `next-steps.md` §8.4 | B3 开工门「门 1 / 门 2 = ⬜ 未做」⇒ ⇒ 「✅ 已闭」 | `grep -c "⬜ \*\*未做\*\*" §8.4` == **0** |

### R13 · B3 立项（**B3 的真起点**）—— 🔴 **主线第 1 步**

| 步 | 内容 | 判据 |
|---|---|---|
| **R13.1** | **两个 change 立项**（原子性已裁决）：`openspec/changes/memory-timing-contract-2026-11/` + `openspec/changes/firmware-load-chain-2026-11/`，各含 `proposal.md` / `design.md` / `tasks.md` / `specs/**`<br>⚠️ **原子性裁决（metis P0-1）**：**两个 change**（能力边界不同）**但 MUST 同一窗口落地** ⇒ ⇒ **归档顺序 MUST 无中间态**（先归档 `memory-timing-contract`，**后**归档 `firmware-load-chain` ⇒ ⇒ 中间态 = 「D-TCM 已改 `sread` 但加载链路未适配」⇒ ⇒ 该中间态**只允许存在在工作区**、**禁**被归档）⇒ ⇒ ⇒ **禁**「一个已归档另一个未归档」的发布态 | 两个目录存在 · 各自的 delta 齐（见 R13.2）· **归档顺序被 tasks 钉死** |
| **R13.2** | **AC 作用域**（metis P1-7）：`memory-timing-contract` 的 proposal 映射 **ADR-068 的 V1–V10 + ADR-069 的 V1–V15**（**25 条**，V6/V7 成对 · V13–V15 与 V8 的变异叙述**不合并**）⇒ ⇒ `firmware-load-chain` 的 proposal 映射**自身 L1–L6 的 18 条 AC**（**不**映射 ADR-068/069 的判据 ⇒ ⇒ 两批判据**互不串**）⇒ ⇒ ⚠️ **反向映射**：草案里**无 ADR 判据号**的 AC MUST 登记为「**无 ADR 对应 · 自洽判据**」⇒ ⇒ ⚠️ **禁** `--skip-specs`（除非显式声明「no spec changes」且 `specs/` 为空） | 每条 AC 有 ADR 判据号**或**「自洽判据」标注 |
| **R13.2b** | 🔴 **capability 清单**（metis P1-6）—— **delta 路径不可猜**：① `mem-read-timing`（**MODIFIED**）· ② `rv32i-commit-path`（**MODIFIED** · `spec.md:230` 的「实测 3 拍」⇒ **4 拍** + `load_use_stalls` 6→8）· ③ `rv32i-data-forwarding`（**MODIFIED** · 档位 2 / D11 的**源**）· ④ `firmware-load-chain` → ⚠️ **仓内无既有 capability** ⇒ ⇒ **MUST 裁定**：**ADDED 到新 capability**（建议名 `rv32i-firmware-load`）**或**并入既有 `rv32i-canonical-home` ⇒ ⇒ **禁**留空 | 四个落点全部指名 · `verify_openspec_delta.sh` 的 R3（MODIFIED 标题**逐字**匹配）可满足 |
| **R13.2c** | ⚠️ **两份 MODIFIED 的 delta MUST 在**提案期**写进 `openspec/changes/<name>/specs/`**（metis P0-2）⇒ ⇒ 值用 **ADR-069 已推导的 4 拍**（**不是**未验证的实现值）⇒ ⇒ 实施期只做「核对 spec 与实现一致 + 归档合并成功」⇒ ⇒ ⚠️ **禁**把 delta 留到归档前才补 | `changes/*/specs/**` 里两份 MODIFIED 存在 |
| **R13.3** | **design.md** MUST 钉 **6 条**：① **M2 = 完整元数据寄存器**（含 `link_value`）② **档位 2 源改指 M2** ③ **D11 源改指「下一提交者值」** ④ **停顿链加一级**（3→4）⑤ **两份 MODIFIED 的落点**（`rv32i-commit-path/spec.md:230` + `rv32i-data-forwarding`）⑥ 🔴 **`data_ready` = 固定 1 拍延迟（不是握手）**（ADR-068 V7 胜出 ⇒ ⇒ **禁**在本批实现真反压） | design 有这 6 条 |
| **R13.4** | **tasks.md** MUST 含 **ADR-069 §5 的 V13 / V14 / V15 三条 distance 判别判据**（distance-2 ALU→消费者 · distance-3→消费者 · load-distance-2）⇒ ⇒ ⚠️ **禁**「靠现有 golden 兜底」⇒ ⇒ ⚠️ **落点**：这三条是**流水线级**（V13 的变异是「档位 2 源改回 `memwb_*`」）⇒ ⇒ **组件级骨架表达不了** ⇒ ⇒ MUST **新建 pipeline 级 fixture + 用例**（**不得**塞进 `tests/chlib/test_forwarding.cpp`）⇒ ⇒ 判别力 = 手推期望 + 变异自证 | tasks 有这 3 条**且指名新建文件** |
| **R13.5** | 回填 **ADR-069 的「关联 change」**（`backlog:` → 指向两个 change 目录）· 回填两份草案的 frontmatter `状态:` | ADR-069 无 `backlog:` 残留 |
| **R13.6** | ⚠️ **判据补强**（metis P1-2）：**禁**只拿 `verify_openspec_delta.sh` 当立项证据（它在**零 active change** 时 exit 0 且无信号 ⇒ ⇒ **零判别力的绿**；且其 R4 只是 `bash -n` ⇒ ⇒ stub 也全绿）⇒ ⇒ MUST 另跑 **每条 Requirement 含 `SHALL`/`MUST`** + **`## Purpose` 非占位符** | `openspec validate <change-name>` 非零错误 + Purpose/SHALL 逐条可查 |

### R14 · B3 实施（**主线第 2 步**）

| 步 | 内容 | 判据 |
|---|---|---|
| **R14.1** | **T1 + T3 同批**（D-TCM `aread`→`sread` + `data_ready` 语义重定义）⇒ ⇒ ⚠️ **不得拆批**（拆 ⇒ ⇒ 中间态出现「声称成立但不成立」） | ADR-068 V1–V10 绿 |
| **R14.2** | **T6**（store→load 同址 `read-new`）⇒ ⇒ 判别性用例 = **紧邻** `SW x1,0(x2)` / `LW x3,0(x2)`（⚠️ 现固件 `SW`@word16 / `LW`@word20 **隔 4 字 ⇒ 零判别力** ⇒ ⇒ MUST 补紧邻形态） | 同址零间隔用例 + **变异自证** |
| **R14.3** | **T2 / T4 / T5**（数据对齐 / 字节写相位 / `verify_dtcm_read_timing.sh`）⇒ ⇒ ⚠️ **yosys 判据降为本地/opt-in** ⇒ ⇒ 「真实 BRAM 可推断性未在 CI 验证」MUST 登记进 `.rddf/checks/arch-residual-registry.md`<br>⚠️ **禁**在本步要求「V1–V15 全绿」（metis P1-8 的**必然判红**：V8/V9 依赖**最终**停顿链与**全部** golden ⇒ ⇒ 中间态必红）⇒ ⇒ 本步只判 **V1–V7 / V10–V12 / V13–V15**（结构 + 语义 + distance 判别）⇒ ⇒ **V8 / V9 留到 R14.6** | 上述**子集**绿 + 残差登记 |
| **R14.4** | **L1–L2 先行**（地址映射 + 统一加载口）⇒ ⇒ ⚠️ **依据已裁决**（metis P0-5：批次表 §B3「本项先做」与草案「依赖 memory-timing」**相反**⇒ ⇒ **裁决 = L1–L2 先行、L3–L6 随后** ⇒ ⇒ 且 **T1 与 L1–L2 同 commit 不可拆**）⇒ ⇒ **L3–L6**（ELF · 截断等价判据）随后 ⇒ ⇒ ⚠️ **R6 的登记内容**（「加载端口族语义变更」）与 L2 的订正推荐（「不新增端口 · 新建 `Rv32iSocTopV2` · 旧顶层冻结」）**不一致** ⇒ ⇒ **二选一**（metis P1-9）⇒ ⇒ ⇒ 若新建顶层组件 ⇒ ⇒ **三处快照**必须同步：`verify_no_duplicate_rv32i_headers.sh` 的 `SOC_TOP_PORTS` · `kObsNames` / `static_assert(kObsCount == 27)` · examples 的 tsv + `examples-rtl-expectations.md` | 三处快照同步 + `verify_public_api_visibility.sh` 绿 |
| **R14.5** | ⚠️ **两份 MODIFIED 的 delta 已在 R13.2c 落盘** ⇒ ⇒ 本步只做「**核对 spec 与实现一致** + **归档合并成功**」⇒ ⇒ ⚠️ **禁**在归档前才补 delta | 归档后主 spec `grep -c "实测 4 拍"` == 1 + `rv32i-data-forwarding` 新条款可 grep |
| **R14.6** | **时序 golden 手推重算**（`perf_load_use_stalls` **6 → 8** · `non_retired` · `perf_instructions` · 24 条 `(rd,data)` 写回序列带 `SHIFT`/`STALE`/`OTHER` 归因标签）⇒ ⇒ ⚠️ **禁抄仿真输出** · ⚠️ **STALE = 一票否决**<br>⚠️ **基线归属**（metis P0-4）：**「退休数 178」不变**（指令数）· ⚠️ **`non_retired == 21` 作废**（ADR-068 V8 的该半已被 **ADR-069 §3.2-②** 收窄 ⇒ ⇒ 新值由**实施后的实测**给出 ⇒ ⇒ **禁**把 21 当不变量）⇒ ⇒ ⚠️ **L1–L6 的地址改写会**独立**改动计数器**（草案 `:148`）⇒ ⇒ ⇒ **禁**把两个 delta 相加 ⇒ ⇒ 必须是**同一份手推**同时含 T1–T3 与 L1–L6 的效应 | ADR-069 V8 / V9 / V10 + 归因标签 |

### R15 · B4 实施（**主线第 3 步**）—— ⚠️ **必须等 R14 闭**

| 步 | 内容 | 判据 |
|---|---|---|
| **R15.1** | **C1 差分（先做）**：复用既有 `kExCases`/`kDecCases`/`ImmConsistencyDut` ⇒ ⇒ **MUST NOT** 重造第二套 | 差异清单覆盖**修复前**差异全集且逐项标处置 |
| **R15.2** | **C2** ALU 编码归一（`ex_stage.h::ALU_OP_*` 为唯一权威）⇒ ⇒ MUST 同步更新 + 登记 + 改 `test_pipeline_exec.cpp:124` 的字面量 | ctest 全绿 |
| **R15.3** | **C3 / C4** 组件接入（`Rv32iAlu`→EX · `Rv32iDecoder`→ID）⇒ ⇒ ⚠️ **标志继续由 opcode 比较**（不得用 `instr_type` 派生） | 20 项 golden 全绿 + 计数锚点重算 |
| **R15.4** | **G3 → G1 + G4（同批）→ 其余**：尾部填 NOP ⇒ ⇒ 退休数 **178 不变**（指令数）· ⚠️ **`non_retired` MUST 引用「B3 后的基线」**（**禁**写死 21 —— metis P0-4：ADR-068 V8 的该半已被 ADR-069 收窄） | ⚠️ **判据 MUST 可判**（metis P1-10：「批内顺序被 tasks 钉死」是循环）⇒ ⇒ **G3 的实现 commit 早于 G1+G4 的实现 commit**（`git log` 可验）· 且退休数 == B3 后基线 |

### R17 · 用户 ⑤ 的端口名消化（✅ **R17.1 已闭 · 2026-11-13** ⇒ ⇒ 🎯 **本轮交付 = R17.2**）

| 步 | 内容 | 判据 |
|---|---|---|
| **R17.1** | ✅ **已闭（2026-11-13 · architect 裁量）=「现在做」**，**放置 = 独立于 B3 的小批**（先于 B3 执行）⇒ ⇒ ⚠️ **依据是 architect 裁量，不是「用户已裁决」** —— ⚠️ **首稿曾把 A′ 的定义读成「含『先消化首批』的时间义务」⇒ ⇒ 与源文件相反**（A′ 定义 = 「框架先行 + 基线棘轮 —— **不**做一次性全量 codemod」；首批消化被 `port-naming-ratchet-2026-11.md:8/:20/:180` **三处**显式逐出本项、归属独立立项）⇒ ⇒ **已订正**，详见 `docs/architecture/2026-11-13-phase-6-rtl-track-review.md` §4.1 ⇒ ⇒ **用户可随时推翻**（回退 = 一次 revert ⇒ ⇒ 改动自洽 ⇒ ⇒ 无沉没成本） | 裁决记录落 `.rddf/roadmap/phase-6-next-steps.md` §5 + `phase-6-rtl-productization.md` §4 + architect 复盘 §4.1.1（**4 条依据 + 落盘要求 + 回退路径**）|
| **R17.2** | 🎯 **本轮交付**：**32 端口**（`examples/riscv-mini/src/rv32i_pipeline_top.h`）全部带**显式名** ⇒ ⇒ 棘轮豁免条目**减少**（棘轮只许减）⇒ ⇒ ⚠️ **产物端口名会变** ⇒ ⇒ MUST 与 examples 的 tsv + `examples-rtl-expectations.md` + 10 个 TB **同批** ⇒ ⇒ ⚠️ **与 B3 的 L1–L6 同区 ⇒ MUST 顺序执行**（本批先做）⇒ ⇒ **两批改动正交**（本批只改 `__io` 声明文本与名字串；B3 改 `prog_addr` 语义） | 棘轮豁免数 **减少**（1484 → **1452**）· `verify_port_naming.sh` 绿 · 10 个 TB 全绿 · **产物端口名逐位置比对通过**（旧 `top_unnamed_output_N` ↔ 新 `top_<name>`，**前提 = 不中插 / 不重排端口**）|
| **R17.3** | ⚪️ **不适用**（R17.1 闭为「现在做」⇒ ⇒ 本步不触发）⇒ ⇒ **但路径仍有效**：若用户推翻 R17.1 ⇒ ⇒ 登记为**独立立项**（载体 = 新 improvement 草案）⇒ ⇒ ⚠️ **禁**写成「触发条件 = 用户要求」（**该条件已成立** ⇒ ⇒ 等一个不会来的许可） | 草案存在 + 过 `add-improve` HARD-GATE |

#### R17.4 · **实施路径**（本轮 builder 的动作清单 · **每步一个可判据**）

| 步 | 动作 | 写点 | 判据（可复跑） |
|---|---|---|---|
| **0** | ⚠️ **先提交**前一轮未提交的 **7+1 份** planner 域改动（7 份草案审计行 + `improvement-suggestions.md` + `decision-request-port-face-2026-11.md`）⇒ ⇒ 否则「会话中断」即造新漂移源（architect 复盘 §2.4 ②） | 8 个路径 | `git status --short` 只剩本批路径 |
| **1** | 32 个端口加**显式名**（`{"name"}` 形态 ⇒ ⇒ 且**标识符 == 名字串** ⇒ ⇒ 满足护栏 P1 的合规判据） | `examples/riscv-mini/src/rv32i_pipeline_top.h` | `grep -cE 'ch_(in\|out)<[^;]*>[[:space:]]*[A-Za-z_][A-Za-z0-9_]*[[:space:]]*\{"' examples/riscv-mini/src/rv32i_pipeline_top.h` == **32** |
| **2** | 删 `.rddf/checks/port-naming-exempt.tsv` 里该文件的 **32 行**（条目身份 = `<文件>\t<__io 块序号>\t<声明文本>` ⇒ ⇒ **声明文本变了 ⇒ ⇒ 旧行必须删**，否则护栏**反向红**） | `.rddf/checks/port-naming-exempt.tsv` | `grep -c 'rv32i_pipeline_top' .rddf/checks/port-naming-exempt.tsv` == **0** |
| **3** | 棘轮锚点 **1484 → 1452** | `scripts/verify_port_naming.sh:50` | `bash scripts/verify_port_naming.sh` ⇒ PASS 且 R2 报 **1452** |
| **4** | 重发产物 ⇒ ⇒ 取新端口表 ⇒ ⇒ **逐位置比对**（旧 `top_unnamed_output_N` ↔ 新 `top_<name>`）⇒ ⇒ 证明**只改名、不改序**（机制 = node id 升序 · `src/codegen_verilog.cpp:410-448`） | 临时产物（**仓外**） | 位置对应表 **27 个 output** 全部对上（数量 == 旧 `top_unnamed_output*` 个数） |
| **5** | 改 TB 连线：`.top_unnamed_input` → `.top_ext_clk` / `.top_unnamed_input_1` → `.top_ext_rst` / … ；`.top_unnamed_output_N` → `.top_<name>` ⇒ ⇒ ⚠️ **MUST 按第 4 步的位置对应表逐条改** ⇒ ⇒ **禁**凭印象改 | `examples/riscv-mini/tb_rv32i_soc.sv` | 编译通过 + `RESULT: checks=24 fails=0`（scenarios=6 · spec_checks=22）|
| **6** | 同步期望登记（若登记含端口名字段 ⇒ ⇒ MUST 同步）⇒ ⇒ 护栏严格模式 | `.rddf/checks/examples-rtl-expectations.md` | `bash scripts/verify_examples_rtl_sim.sh` ⇒ PASS（**10/10**）|
| **7** | 同步文档：端口命名指南的「现状」段（采用率不再是 0）+ `port-naming-ratchet` 草案的 AC 回填 | `docs/developer_guide/patterns/PORT-NAMING-GUIDE.md` · `.rddf/improvements/port-naming-ratchet-2026-11.md` | ⚠️ **禁**改根 `AGENTS.md`（已超 WARN 阈值 57,344 B ⇒ ⇒ 新增内容进 capability / `.rddf/checks/`）|
| **8** | 全护栏复跑（§8.5 的 11 脚本 + 1 `ctest -N`）⇒ ⇒ ⚠️ **改了 `examples/**` ⇒ ⇒ `ctest` 前 MUST 先全量重建** | — | 全绿 + `ctest -N` == `CTEST_TOTAL` |

---

### R16 · 独立立项 12 条（**旁线 · 11 条可全程并行**）

⇒ ⇒ 清单 = `phase-6-rtl-productization.md` **§4** 的 **11 行** + **台账 id=22** = **12 条** ⇒ ⇒ ⚠️ 每条 MUST 写清
「**对象 / 判据 / 触发条件 / 归属批**」（否则登记是空文）⇒ ⇒
⇒ ⇒ **新增一条（architect 裁决 ③）**：**台账 id=22**（同名兄弟静默覆盖）⇒ ⇒
载体 = 新 improvement 草案（走 `add-improve` HARD-GATE）· **本窗口不并入 B3/B4**。
⇒ ⇒ ⚠️ **2026-11-13 又新增 2 条**（**用户裁决「独立立项 · 本轮不做」**）：
**F-1**（`include/cpu/AGENTS.md` 自述计数订正）· **F-2**（`public-api-surface.md` 活账清理）
⇒ ⇒ 两者**同源** = **B0 批 C5+C6 退役后的登记未同步** ⇒ ⇒ **建议同批做**（合计约 5 行）
⇒ ⇒ ⚠️ **两者都在 CI 门内** ⇒ ⇒ **CI 当前为红**（**与 R17.2 无关** · 已实测复现于 HEAD）
⇒ ⇒ ⇒ 判据 / 复现命令见批次表 §4 的对应两行。
⇒ ⇒ ⚠️ **写点分布**（metis P1-14）：**9 条里只有端口名消化那条**落在 `examples/riscv-mini/src/rv32i_pipeline_top.h` + TB tsv
⇒ ⇒ **与 B3 的 L1–L6 同区** ⇒ ⇒ 那一条 **MUST 与 B3 顺序执行**（⇒ ⇒ 归 **R17**）⇒ ⇒ 其余 **9 条**可全程并行。

---

### R18 / R19 / R20 · **本轮新增的 3 条待裁决项**（**已立单 · 裁决前不实施**）

⇒ ⇒ **载体统一** = `.rddf/checks/decision-request-port-face-2026-11.md`（2026-11-13 planner 独立复核立单）
⇒ ⇒ ⚠️ **本单与既有 8 份 decision-request + `rtl-track-correction-list` 零重叠**（逐条核对过）
⇒ ⇒ ⇒ **裁决权在用户 / architect** ⇒ ⇒ **planner / builder 不代拍**。

| # | 事项 | 事实（实测） | 选项（决策单内详列） | 归属 |
|---|---|---|---|---|
| **R18** | **R1 = 端口面残留登记** | 扁平产物顶层 **114** 端口（7 in + 107 out）⇒ ⇒ 根组件自有仅 **32** ⇒ ⇒ **80 个内部 `ch_out` 暴露在顶层**；manifest `ports[]` = **191**（比 `.v` 宽 **77**）⇒ ⇒ ⚠️ **但这是用户 2026-11-11 已裁决的现状**（C2 撤回 · 与 `rtl-emit-portability:57-62` 一致）⇒ ⇒ **不是待裁决项** ⇒ ⇒ 只是**台账无行** | **A（建议）** = 补登记为已知残留（台账 + 登记表）⇒ ⇒ **零代码改动**；B = 推翻 C2 撤回（走 ADR 取代）；C = 不登记（与「台账 = 唯一真相源」纪律冲突）| planner → architect |
| **R19** | **R2 = F6 判据缺口** | 护栏 `verify_flat_rtl_emit.sh` 的 F6 =「**当前不适用**（不是 pass）」⇒ ⇒ 而 `rtl-emit-product-tooling` 的 AC（`:204`）已改成「`ctest -L verilator_e2e`（77 条）全绿 ＋ 端口表不失配」⇒ ⇒ **护栏未实现 AC** ⇒ ⇒ **backend 端口表与产品端口面无判据** | **A①** = 护栏直接跑 `ctest -L verilator_e2e`（**不**必须动工具设计 ⇒ ⇒ 但新增 `BUILD_VERILATOR=ON` 环境依赖 ⇒ ⇒ **fail-closed 语义待定**）；**A②** = 给工具自测设计加一个 `ch_reg`（⇒ ⇒ 「逐字节比对」重新可判 ⇒ ⇒ 但**工具产物端口面变化** ⇒ ⇒ F3 / F8 期望集要同步）| 用户 → builder |
| **R20** | **R3 = `gpio_example` 断言强度** | 护栏**自标**：`spec_checks=0 < 3 ⇒ ⇒ impl-contract only（只能防漂移，不能证明符合规范）`⇒ ⇒ 10 项里 **9 项** `spec_checks ≥ 3` ⇒ ⇒ **唯独它**不能证明符合规范 | **A** = 补 spec 层断言（需仓内有 gpio 的规范依据）；**B（建议 fallback）** = 显式标注「impl-contract only ⇒ 只防漂移」（零成本 · 缺口可见）；⚠️ **C = 移出首批 = 削覆盖面 ⇒ ⇒ 不用** | 用户 → builder |

### R21 · **目标 ④（RV32I 层次化 RTL 示例）= 独立立项**（architect 裁决 ③）

⇒ ⇒ ⚠️ **范围 MUST 先侦察**（architect 复盘 §3.3 / §4.3）：

| 项 | 事实 | ⇒ ⇒ **不是什么** / **是什么** |
|---|---|---|
| 产物形态 | `toVerilogHierarchical(Rv32iPipelineTop)` ⇒ ⇒ `ok = true` · **1 module** · 与扁平产物**逐字节相同** | ⇒ ⇒ **不是**「发射器的静默退化要修」—— **默认 inline 是已签批契约行为**（`tests/test_hierarchy_equivalence.cpp:43` 有测试钉着）⇒ ⇒ **不得**改发射器 |
| 真正的缺口 | `CH_HIERARCHY_LAYER` 在 `include/cpu/` + `examples/` = **0 处** | ⇒ ⇒ **缺口 = 用户诉求 ④ 未兑现**（RV32I 上没有成层示例）|
| ⚠️ **规模** | `src/hierarchy_analysis.cpp:7-9` + `:42-45` ⇒ ⇒ 跨 module 边需「隐式端口合成」时 **fail-closed 抛异常**（T3-④ 义务未实现） | ⇒ ⇒ **不是**「加一行 `CH_HIERARCHY_LAYER`」⇒ ⇒ **规模大于此** ⇒ ⇒ ⚠️ **立项前 MUST 先做规模侦察**（probe：给 RV32I 声明一层 ⇒ ⇒ 看它是否撞上 fail-closed）|

⇒ ⇒ **载体** = 新 improvement 草案（`add-improve` HARD-GATE）⇒ ⇒ **不并入 B3 / B4**（写点 `src/hierarchy_*` 与 `include/cpu/**` 不重叠）。

---

---

## 3. 批次层面的后续内容（**开工门 + 出口**）

| 批 | 内容 | 开工门 | 出口 |
|---|---|---|---|
| **B2 收尾** | ① **验收报告**（**归属 = `verifier`（只验不改）** —— 判据来源按 `b1-batch-verify-2026-11.md` §首的四级优先级：用户条件 → proposal `## 验收标准` → 变更自身声明 → ADR Verification ⇒ ⇒ 出口 = 报告落 `.rddf/reviews/b2-batch-verify-2026-11.md` + 结论行（通过/不通过 + 数字））② R0 ③ R1/R2/R10 | — | **`ctest -LE perf --output-on-failure` 全绿**（⚠️ **禁写死「263/263」** ⇒ 写成「基线 + 新增 − 删除」）**且** **`CTEST_TOTAL` == `ctest -N`**（⚠️ `ctest -N` 只**列出**条目、**不执行** ⇒ ⇒ **禁**说它「全绿」· metis P1-9）+ **12 护栏逐条列名**（= §8.5 的 **11 脚本 + 1 `ctest -N`** ⇒ ⇒ **禁**把两份清单相加当「14 个」）**且** `verify_ledger_evidence`(+self-test) 绿 + examples **28/28** + samples **39/39** + 报告落盘 |
| **B3** | T1+T3 同批（D-TCM `aread`→`sread` + `data_ready` 语义重定义）· T6（store→load 同址）· T2/T4/T5 · L1–L6（装载链路） | ✅ **R5**（T2 裁决 · ADR-069 Accepted）+ ✅ **R6**（公开面登记）⇒ ⇒ 🚪 **两门已闭** ⇒ ⇒ **只差 R13（两个 change 立项）** | ADR-068 的 V1–V10 + **ADR-069 的 V1–V15** 全绿 + 时序 golden **手推重算**（禁抄仿真输出）· `load_use_stalls` 6→8 |
| **B4** | C1 差分（先做）· C2 ALU 编码归一 · C3/C4 组件接入 · G3→G1+G4（同批）→ 其余 | **B3 已闭**（**R14** · 否则时序金标算两遍） | 组件接入后 20 项 golden + 计数锚点重算 |

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
| ② | ~~**R5（T2 裁决）的 architect 评审预算**~~ | ✅ **已用**（ADR-069 · oracle ×3 + critic ×1 · **已闭**）⇒ ⇒ **不再需要等预算** |
| ③ | **R11（FENCE 裁决）的 architect 评审预算** | 属**ISA 语义决策** ⇒ ⇒ **oracle → critic 全审** ⇒ ⇒ 需一轮 architect 预算（**可与 R17 的裁决合并**） |
| ④ | **T8（P9 遗留）的独立立项** | 真 FIFO 写缓冲 / 真预取 ⇒ ⇒ 需 planner 另起 improvement ⇒ ⇒ ⚠️ 可能需 architect 裁定 ⇒ ⇒ **本窗口不并入**（避免与 R3 并行制造文档债） |
| ⑤ | ✅ **R17.1（端口名消化「现在做 vs 推到 phase-7」）** | ✅ **已闭（2026-11-13 · architect 裁量）=「现在做」**，放置 = 独立小批（先于 B3）⇒ ⇒ **依据 4 条落** architect 复盘 §4.1.1 ⇒ ⇒ ⚠️ **用户可随时推翻**（回退 = 一次 revert）⇒ ⇒ **本轮交付 = R17.2** |
| ⑥ | **R18（端口面残留登记）** | **建议 A（补登记为已知残留 · 零代码改动）** ⇒ ⇒ ⚠️ 不是待裁决的现状（C2 已撤回）⇒ ⇒ 只差「补台账行」 |
| ⑦ | **R19（F6 判据缺口）** | **两条路径 A① / A②** ⇒ ⇒ ⚠️ **各有取舍**（环境依赖 vs 工具产物端口面变化）⇒ ⇒ **不擅自选** |
| ⑧ | **R20（gpio 断言强度）** | **建议 A（补 spec 层断言）；无规范依据则退 B（显式标注）** ⇒ ⇒ ⚠️ **禁** C（移出首批 = 削覆盖面） |

---

## 6. 判据自检（**可复跑** · 本轮全判据）

> ⚠️ **本段已重写**（metis P1-5）：旧版只列 R0/R1/R2/R10 的 **3 条弱判据**（其中 R2 用的是
> metis 第 1 轮 P1-5 已判过的弱形态 ⇒ ⇒ builder 跑本段会拿到**假绿**）⇒ ⇒ **旧版已删**。

```bash
cd /workspace/project/CppHDL
# ---- R0 / R1 / R2 / R10（已闭）----
bash scripts/verify_ledger_evidence.sh              # R0 ⇒ PASS（91 台账 · 6 项待补 = WARN）
bash scripts/verify_ledger_evidence.sh --self-test   # R0 ⇒ PASS（投毒转红 + 还原复绿）
grep -c "E6 ⬜ 未做" .rddf/roadmap/phases/phase-6.md                # R1 ⇒ 0
awk '/^\| \*\*P7\*\* \|/' .rddf/roadmap/phases/phase-6.md | grep -c "① ✅"  # R2① ⇒ 1
awk '/^\| \*\*P7\*\* \|/' .rddf/roadmap/phases/phase-6.md | grep -c "② ⬜"  # R2② ⇒ 1
awk '/^\| \*\*P7\*\* \|/' .rddf/roadmap/phases/phase-6.md | grep -c "③ ✅"  # R2③ ⇒ 1
awk '/^\| \*\*P7\*\* \|/' .rddf/roadmap/phases/phase-6.md | grep -c "④ 🟡"  # R2④ ⇒ 1
awk '/^\| \*\*P7\*\* \|/' .rddf/roadmap/phases/phase-6.md | grep -c "⑤ ⬜"  # R2⑤ ⇒ 1
grep -m1 "^状态:" .rddf/improvements/mem-port-arg-cross-layer-2026-11.md   # R10 ⇒ implemented

# ---- R12（12 条漂移 · 正反两面判据 · metis P1-10）----
grep -c "T2 尚未裁决" .rddf/roadmap/phases/phase-6.md                            # ① 负 ⇒ 0
grep -c "T2 已裁决" .rddf/roadmap/phases/phase-6.md                              # ① 正 ⇒ 1
grep -c "ADR-069" .rddf/roadmap/phases/phase-6.md                               # ① 正 ⇒ >=1
awk '/^\| \*\*R5\*\* \|/' .rddf/roadmap/phase-6-next-steps.md | grep -c "仍欠"    # ③ 负 ⇒ 0
awk '/^\| \*\*R5\*\* \|/' .rddf/roadmap/phase-6-next-steps.md | grep -c "补齐 SF-3"   # ③ 正 ⇒ 1
grep -c "ADR-069（.Draft" docs/CHANGELOG.md                                    # ④ 负 ⇒ 0
grep -c "ADR-069.*Accepted" docs/CHANGELOG.md                                   # ④ 正 ⇒ >=1
grep -c "用户裁决 ADR-069" .rddf/checks/rtl-track-followup-list-2026-11.md       # ⑦ 负 ⇒ 0
grep -c "B3 的两个 change 立项" .rddf/checks/rtl-track-followup-list-2026-11.md  # ⑦ 正 ⇒ 1
grep -c "前置门 = 公开面登记未做" .rddf/improvements/firmware-load-chain-2026-11.md  # ⑨ 负 ⇒ 0
grep -c "前置门已闭" .rddf/improvements/firmware-load-chain-2026-11.md            # ⑨ 正 ⇒ 1
grep -c "2026-11-12 dated 前向注记" AGENTS.md                                    # ⑩ ⇒ 1
grep -c "ADR-069" AGENTS.md                                                      # ⑩ 内容 ⇒ >=1
grep -c "B4 实施时 N = 4" .rddf/improvements/rv32i-isa-gap-closure-2026-11.md   # ⑪ ⇒ 1
grep -c "沿用 3" .rddf/improvements/rv32i-isa-gap-closure-2026-11.md               # ⑪ 内容 ⇒ 1
sed -n '/^### 8\.4 B3/,/^### 8\.5/p' .rddf/roadmap/phase-6-next-steps.md | grep -c "未做"  # ⑫ 负 ⇒ 0
sed -n '/^### 8\.4 B3/,/^### 8\.5/p' .rddf/roadmap/phase-6-next-steps.md | grep -c "已闭"  # ⑫ 正 ⇒ 2
awk -F'|' '/^### 8\.3/{f=1} /^### 8\.5/{f=0} f && /^\| \*\*/ {gsub(/[^0-9]/,"",$4); s+=$4} END{print "§8.3 合计 =", s, "(期望 74)"}' .rddf/roadmap/phase-6-next-steps.md
python3 - <<'EOF'
import pathlib, re
rows = [l for l in pathlib.Path('improvement-suggestions.md').read_text().split('\n')
        if re.match(r'^\|\s*\[[a-z0-9-]+-2026-11\]', l)]
bad = 0
for l in rows:
    n = re.match(r'^\|\s*\[([a-z0-9-]+)-2026-11\]', l).group(1)
    fm = pathlib.Path(f'.rddf/improvements/{n}-2026-11.md').read_text()
    st = re.search(r'^状态:\s*(.+)$', fm, re.M).group(1).strip()
    idx = next((s for s in ('draft','approved','in-progress','implemented') if s in l), '?')
    bad += (idx != st)
print(f'⑥ 索引 vs frontmatter：{len(rows)} 份 · 不一致 = {bad} (期望 0)')
EOF

# ---- 常驻护栏 ----
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

### 7.0 🟡 **metis 第 2 轮（2026-11-12 晚 · architect 复盘 #3 后的任务清单刷新）** 的处置

> **触发**：本轮 planner 刷新了 §1 的 R 表（新增 **R12–R17**）+ §2 的实施路径（+R12–R16 段）⇒ ⇒
> ⇒ ⇒ metis **1 轮**（19 条）⇒ ⇒ **P0 5 条全处置** · **P1 9 条处置 8 条** · **P2 5 条处置 4 条**
> ⇒ ⇒ **未处置的 2 条登记进 §7.0.1 的 backlog**（不静默丢弃）。

| # | metis finding | 处置 |
|---|---|---|
| **P0-1** | R13.1「两个 change」与 R14.1「T1+T3 同批 ⇒ 不得拆批」**直接冲突** | ✅ **裁决（两个 change · 同一窗口 · 归档顺序无中间态）** ⇒ ⇒ R13.1 已写明「先归档 `memory-timing-contract`、后归档 `firmware-load-chain`」+「禁『一个已归档另一个未归档』的发布态」 |
| **P0-2** | 两份 MODIFIED 的 delta 被放到**实施期** ⇒ ⇒ 但 OpenSpec delta **必须在提案期写** | ✅ **落点改回 R13.2c**（提案期写 delta · 值用 ADR-069 已推导的 4 拍）⇒ ⇒ R14.5 降为「核对 spec 与实现一致 + 归档合并成功」⇒ ⇒ 判据从 `validate --specs`（错对象）改为「归档后主 spec `grep -c "实测 4 拍"` == 1」 |
| **P0-3** | ADR-069 **V12 指向 `tests/chlib/test_hazard_complete.cpp`** —— 该文件**不存在**（B0 C5+C6 已退役）⇒ ⇒ **判据不可满足** | ✅ **architect 域已修**（ADR-069 V12 加 **2026-11-12 订正**）：落点改为「**新建** `HazardUnit` 组件级停顿链真值表用例」+ 明确「`test_phase4_integration.cpp` 当前**只有实例化 smoke** ⇒ ⇒ 禁只断言能实例化」 |
| **P0-4** | ADR-068 **V8**（`non_retired == 21` 不变）与 ADR-069 **§3.2-②**（`non_retired` 随之变）**互斥** | ✅ **architect 域已归一**（ADR-068 §5.5 补行）：**退休数 178 不变**（指令数）· **`non_retired` 由 ADR-069 决定**（V8 该半被收窄 · 原文逐字保留）⇒ ⇒ R14.6 / R15.4 已改为引用「B3 后的基线」· **禁**写死 21 |
| **P0-5** | L1–L6 的位置：批次表「本项先做」vs 草案「依赖 memory-timing」**方向相反** | ✅ **裁决（L1–L2 先行 · L3–L6 随后 · T1 与 L1–L2 同 commit 不可拆）** ⇒ ⇒ R14.4 已写明依据 |
| **P1-1 / P1-6 / P1-7** | R13 缺 capability 名 / AC 作用域混用 / 缺反向映射 | ✅ R13.2（作用域分家）+ **R13.2b**（capability 清单 4 项 · 含 `firmware-load-chain` 的 ADDED-or-并入裁定）+ **R13.2c**（delta 落点） |
| **P1-2** | `verify_openspec_delta.sh` 在**零 active change** 时 exit 0 ⇒ ⇒ **零判别力的绿** | ✅ **R13.6**（另跑 `openspec validate <change>` + SHALL/MUST + Purpose 非占位符） |
| **P1-3 / P1-4** | R12 的判据大面积**负向单边**（删掉整行即绿）/ 字面串存在性（写标记即绿） | ✅ **§6 已重写** ⇒ ⇒ ①③④⑦⑨⑫ 各加**正向锚** ⇒ ⇒ ⑩⑪ 各加**内容锚**（`ADR-069` / `禁沿用 3`） |
| **P1-8** | ADR-068 **V7**（`data_ready` 恒 1 · 不是握手）与 B3 草案 T3 的三条「真反压」AC **互斥** | ✅ **architect 域已裁决（V7 胜出）**（ADR-068 §5.5 补行）⇒ ⇒ **planner 域已改草案**：T3 三条降级为**登记型**判据 + 原三条**逐字保留**为历史记录 ⇒ ⇒ R13.3 的 design pins 补第 ⑥ 条 |
| **P1-9** | `ctest -N` 只**列出**条目 ⇒ ⇒ 「全绿」是错的 | ✅ §3 的 B2 出口改为「`ctest -LE perf --output-on-failure` 全绿 **且** `CTEST_TOTAL` == `ctest -N`」 |
| **P1-10** | R15.4 的判据**循环**（「批内顺序被 tasks 钉死」不是判据） | ✅ 改为「**G3 的实现 commit 早于 G1+G4 的实现 commit**（`git log` 可验）」 |
| **P1-12** | 护栏清单「12 个」含**重复**且与「14 护栏」口径打架 | ✅ §8.5 加**口径说明**（**11 脚本 + 1 `ctest -N`**）+ §3 的 B2 出口同步 |
| **P1-13** | （与 P1-9 同源）| ✅ 同上 |
| **P1-14** | R16「8 条」实为 **9 行 + id=22 = 10**；且写点不可笼统写「`src/hierarchy_*`」 | ✅ R16 改为 **10 条** + 逐项写点 + 明确**端口名消化项与 B3 同区 ⇒ 不可并行** |
| **P2-15** | 用户 ⑤ 的诉求**触发条件已成立** ⇒ ⇒ 不该等一个不会来的许可 | ✅ **新增 R17**（需用户二选一：「现在做」vs「推到 phase-7」） |
| **P2-16** | 文件内陈旧数字（§0 编号 / §1.1 资源数 / §1.1 锚点数 / §5 的 R5 预算） | ✅ 全部订正 |
| **P2-17** | §3.0 与 §8.6 两份负面约束重叠 | ⚠️ **未处置**（见 §7.0.1 backlog-1） |
| **P2-18** | 「R12 已执行」只存在于**未提交的 working tree** ⇒ ⇒ 判据只看工作区 ⇒ ⇒ 一次 `checkout` 就变回未执行而判据仍绿 | ✅ R12 行改为「**完成态 = 已提交**（提交后回填 `commit <sha>`）」 |
| **P2-19** | （metis 的权限说明）部分 `shell` 被拒 ⇒ ⇒ 「`ctest -N` = 263」「12 护栏全绿」「`verify_openspec_delta.sh` 绿」**未经 metis 亲跑** | ✅ **已由本会话代跑**（12 护栏全 exit 0 · `ctest -N` = 263 · `verify_openspec_delta.sh` = 0） |

#### 7.0.1 🟡 **未处置的 2 条（backlog · 不静默丢弃）**

| # | finding | 为何本轮不做 | 触发条件 |
|---|---|---|---|
| **backlog-1** | **§3.0 与 §8.6 两份负面约束重叠**（emit_hygiene 默认 off / 禁未批缩面 / `git add -A`）⇒ ⇒ 两份载体必然漂移 | ⚠️ 合并会**改动 §8.6 的既有引用面**（多个文档引它）⇒ ⇒ 超出本轮「刷新清单」的范围 ⇒ ⇒ **本轮只登记** | 下次改动 §3.0 或 §8.6 时二选一合并（**建议** §8.6 改成 §3.0 的指针） |
| **backlog-2** | **「禁写死数字」只对 `CTEST_TOTAL` 执行**，而文件里写死了 `178` / `21` / `6→8` / `24` / `28/28` / `39/39` / `74` | ⚠️ 全局改写会**放大本轮 diff** ⇒ ⇒ 且 `74` 是**实测快照**（§8.3 已标「真相 = 各草案的 `## Acceptance` 段」）⇒ ⇒ **本轮只登记** | 与 backlog-1 合并处理 |

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

## 8. 🟢 **新会话 builder 的实施入口**（RTL 轨 · **2026-11-13 修订**）

> **本节是「下一个会话从哪儿开始、读什么、按什么顺序做」的**唯一入口**。
> ⚠️ 上游 = `.rddf/checks/rtl-track-correction-list-2026-11.md`（7 份草案的**状态/簿记订正** ·
> oracle 第 1 轮 13 条 · critic 第 1 轮 2 BLOCKER → 第 2 轮 **APPROVE** · 修订执行记录见其 §9）。
> ⇒ ⇒ 本节**不复制**任何状态格 / 判据 ⇒ ⇒ 全部**指路**到唯一真相。

### 8.0 🎯 **本轮入口 = R17.2**（2026-11-13 · architect 裁量闭 R17.1）

| 项 | 内容 |
|---|---|
| **做什么** | `examples/riscv-mini/src/rv32i_pipeline_top.h` 的 **32 个端口**全部加**显式名** ⇒ ⇒ 连带：删 tsv 的 32 行 · 锚点 1484 → **1452** · 改 `tb_rv32i_soc.sv` 的连线 · 同步期望登记与文档 |
| **执行清单** | **§2 的 R17.4**（8 步 · 每步一个可复跑判据）⇒ ⇒ **照它走，别自创顺序** |
| **为什么** | 用户 **⑤** 的原始诉求（端口名 == C++ 成员名）**在真实 CPU 上首次兑现** ⇒ ⇒ 消掉「写 TB 必须查 port map」的痛点 ⇒ ⇒ **不依赖 B3 闭环** |
| **必须先做** | **R17.4 的第 0 步** = 提交前一轮未提交的 **7+1 份** planner 域改动 |
| **护栏清单** | §8.5（11 脚本 + 1 `ctest -N`）⇒ ⇒ ⚠️ **改了 `examples/**` ⇒ ⇒ `ctest` 前 MUST 先全量重建**（否则跑的是旧二进制 ⇒ ⇒ 假绿） |
| **回退** | 改动自洽 ⇒ ⇒ 回退 = **一次 revert** ⇒ ⇒ **用户可随时推翻 R17.1** |
| ⚠️ **不做的** | **B3（B0–B4 主线）本轮不动** —— B3 尚未立项（R13）⇒ ⇒ 且 R17 与 B3 **同区 ⇒ MUST 顺序执行**（本批先做）⇒ ⇒ **B3 实施时 MUST 复核**：R17 已把 32 端口具名 ⇒ ⇒ B3 新增加载端口 MUST 一并具名（护栏 P1 会替 B3 兜住） |

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

### 8.3 未完成 work item 的**逐条索引**（共 **74** 条 · 全部带 `归属`）

> ⚠️ **计数口径 = 「行首 + `<br>` 内联」双计**（`rtl-track-correction-list-2026-11.md:152`）⇒ ⇒ **禁**按行首单计。
> ⚠️ **本表是快照** ⇒ ⇒ **真相 = 各草案的 `## Acceptance` 段** ⇒ ⇒ **禁**按本表数字反推勾选状态。

| 批次 | 草案 | 未勾 AC 数 | AC 段位置 |
|---|---|---:|---|
| **B3** | `memory-timing-contract-2026-11` | **17** | `## Acceptance`（T1–T5 + T2′ + T6 + 不回归） |
| **B3** | `firmware-load-chain-2026-11` | **18** | `## Acceptance`（L1–L6 + 不回归） |
| **B4** | `rv32i-dead-component-consolidation-2026-11` | **10** | 差分段（4）+ 接入段（6）⇒ ⇒ **C1–C4** |
| **B4** | `rv32i-isa-gap-closure-2026-11` | **22** | `## Acceptance`（G1–G5 + 边界 + 不回归）⚠️ **22 = 行首 18 + `<br>` 内联 4**（只按行 grep 会漏） |
| **独立立项** | `port-naming-ratchet-2026-11` | **1** | ⚠️ **2026-11-13 更新**：原 **3** 条（存量消化 2 + `CHERROR` 判别力 1）⇒ ⇒ **R17.2 落地 ⇒ ⇒ 存量消化 2 条已闭** ⇒ ⇒ **只剩 `CHERROR` 判别力 1 条**（§4「N3 补遗」）⇒ ⇒ ⚠️ 该 1 条**不在**本批范围 |
| **独立立项** | `port-name-literal-parity-2026-11` | **2** | 产物层双测试（`NamedRootArtifactsUnchanged` + `UnnamedRootArtifactPinned`）· 触发条件 = route (c) 重开 |
| **独立立项** | `node-name-prefix-consistency-2026-11` | **2** | 产物层双测试（同上两标识符）· 触发条件 = route (c) 重开 |

⇒ ⇒ ⚠️ **B3 必须先于 B4**（否则时序金标算两遍）⇒ ⇒ 理由与依赖链见批次表 §2 的硬约束。

### 8.4 B3 的**开工门**（两道 · 都过才能动手）

| 门 | 任务 | 归属 | 当前态 |
|---|---|---|---|
| **门 1** | **R5** = T2 裁决（MEM 数据对齐）⇒ ⇒ 载体 = **ADR-069** | architect | ✅ **已闭**（**Accepted · 2026-11-12** · 决策 = **A0** · oracle ×3 + critic ×1 · 审稿记录 `.rddf/reviews/ADR-069-mem-data-alignment.md`） |
| **门 2** | **R6** = 公开面前置门（`docs/CHANGELOG.md` + `.rddf/checks/public-api-surface.md` 各一行） | planner | ✅ **已闭**（2026-11-12 · 各一行 · **前置登记 = 预告 + 迁移指引**，非「已发布」） |

⇒ ⇒ ✅ **两道门已全闭** ⇒ ⇒ **B3 的真起点 = R13（立项）** ⇒ ⇒ ⚠️ **不再欠文档门**（ADR-047 的 dated addendum 已于 2026-11-12 晚补齐 ⇒ ⇒ 两份 spec 的 MODIFIED 已重分类为 **R14 内**的 change 步骤）。

⇒ ⇒ 判据见 §2 的 **R5** / **R6** 段。

### 8.5 开工前的**护栏清单**（12 个 · 全绿才算干净起点）

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
bash scripts/verify_dual_backend_parity.sh   # 双后端 parity 锚点（★2 硬约束）
(cd build && ctest -N | tail -1)             # Total Tests: 263

# ⚠️ **口径（metis P1-12）**：上列 = **11 个脚本 + 1 条 `ctest -N`** ⇒ ⇒ 「12 个护栏」指这 12 条
#    ⇒ ⇒ ⚠️ **`verify_ledger_evidence.sh` 只在 §6 列一次**（不再重复）⇒ ⇒ **禁**把两份清单相加当「14 个」
# ⇒ ⇒ ⚠️ **B3/B4 还会动这两片** ⇒ ⇒ 实施期 MUST 一并跑：
#      bash scripts/verify_assertion_quality_fixtures.sh
#      bash scripts/verify_arch_ledger_residuals.sh --self-test
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
