# 待裁决清单 —— 三份 change 的验收收口（2026-11-18）

> ✅ **裁决状态（2026-11-18 · 用户）**：D1–D5 · P1–P3 · B1–B3 **全部按 verifier 的推荐采纳**。
> ⇒ ⇒ 执行路径见 `.rddf/reviews/implementation-path-2026-11-18.md`（阶段 1 = planner 批改 AC 措辞 ⇒ 阶段 2 = builder 修实现/记录 ⇒ 阶段 3 = verifier 第二轮复核 ⇒ 阶段 4 = 归档）。
> ⚠️ 下列「候选 / 推荐」保留原文 ⇒ ⇒ 记录**当时**的推理与备选，便于日后复盘。

> **性质**：verifier 验收（change 1 / 2 / 3，逐条 AC）产出的**裁决请求**。
> **verifier 不改 AC、不改实现**（角色边界）⇒ ⇒ 下表每条都标注了执行者。
> **依据**：`.rddf/reviews/axi4-verif-driven-cpp-2026-11-verify.md` · `.rddf/reviews/emit-cli-into-example-2026-11-verify.md` · `.rddf/reviews/example-file-boundary-2026-11-verify.md` ⇒ ⇒ **已用轮次 1/3**。

## 〇、一句话现状

三份 change 的**实现面**基本站得住（change 3 = 28 条 AC 里 23 条直接通过、change 1 = 15/17、change 2 = 12/14）；
挡路的不是代码，而是 **① 3 条 AC 措辞与已签批裁决冲突** · **② 2 处缺记录/缺证据文件** · **③ 1 处一行注释** · **④ 2 份 tasks.md 未回填**。
⇒ ⇒ **没有任何一条需要重做设计**。

---

## 一、需要**用户**裁决（5 条 · 其余都可不依赖用户）

### D1 · change 3 的 AC5：e2e 的 TEST_CASE 名被改名

| 项 | 内容 |
|---|---|
| **证据** | `git diff 55f3f5a..HEAD -- tests/test_axi_lite_verilator_e2e.cpp` ⇒ `…AxiLiteTopPortAccessSnapshot` → `…AxiLiteSlavePortAccessSnapshot`；AC5 与 `tasks.md` B1 都写「用例名零变化」 |
| **候选 A** | 改回旧名 ⇒ 与新 DUT（`AxiLiteSlave<32,32,4>`）语义不符（名字里写着已被退役的 example 类） |
| **候选 B** ⭐ | **planner 修订 AC5 的允许集**：加「TEST_CASE 名含 DUT 名 ⇒ 允许随 DUT 改名（须同批登记）」，并把本次改名登记进 `docs/CHANGELOG.md` |
| **候选 C** | 保留改名 + 只在 AC5 标「例外已批准」⇒ 例外会变成常态口子，不推荐 |
| **推荐** | **B** —— 改名本身是**语义正确**的（DUT 换了类，旧名会误导后人）；错的是 AC 把「用例名」写成了绝对零变化 |
| **若采纳 B 的改动** | planner 改 `proposal.md` AC5 一行；builder 在 `docs/CHANGELOG.md` 补一行「e2e 用例名随 DUT 改名」 |
| **不采纳的后果** | AC5 永远红 ⇒ change 3 无法归档；下一轮 verifier 会重复命中同一条 |

### D2 · change 3 的 AC25：「默认模式基线」未单独落盘，且**已无法补采**

| 项 | 内容 |
|---|---|
| **证据** | `.rddf/checks/` 只有两份记录：`product-check-equivalence`（`--check-products` 模式 ✓ 零差异）+ `product-equivalence-header-move`（SoC 基线用的是 `--hierarchical`，**不是**批次 F 之前的扁平默认产物） |
| **候选 A** ⭐ | **修订口径**：默认模式的「1 → 5」以 `eq/flat/top.v`（同设计扁平参照，1 module）+ `manifest.json` **逐字节相同**（⇒ `port_table` 逐字逐序相同）作为**等价证据**；并把「分模式两套基线」降为「**记录要求**」而非已满足的判据 |
| **候选 B** | 要求**今后**同类 change 必须落两套基线（写成 checklist / 护栏），本次以「不可补采」登记 |
| **候选 C** | 判 AC25 不成立 ⇒ ⇒ change 3 阻塞在一条**无法补做**的判据上（死结） |
| **推荐** | **A + B 组合**：本次按 A 修订（证据已在，只是记录形式不同），同时把「两套基线」写成 B 的登记要求 |
| **不采纳的后果** | AC25 悬空 ⇒ 归档无门；或被迫放宽「取证不可用 ⇒ 不判 pass」的既有纪律 |

### D3 · 既有 `UNTRACKED emit_rtl_rv32i_soc_fw_init`（使 `run_all_ported_tests.sh` exit=1）

| 项 | 内容 |
|---|---|
| **证据** | `run_all_ported_tests.sh` ⇒ `PASS: 27 FAIL: 1`，FAIL 来自 C1：`UNTRACKED emit_rtl_rv32i_soc_fw_init`；引入 commit = `a2f5075`（**远早于**这三份 change）⇒ 与本轮无关 |
| **归属面** | C1 的「归属载体」= `.rddf/checks/examples-rtl-sim-targets.tsv` 的 `emitter` 列（`run_all_ported_tests.sh:288` `rtl_emitters()`）⇒ 该 emitter 是**唯一**没进 `emitter` 列的发射器；它的真实消费者是 `scripts/verify_rv32i_fw_init_rtl.sh:40` |
| **候选 A** ⭐ | 登记进 `examples/disabled_examples.txt`（理由：「由 `verify_rv32i_fw_init_rtl.sh` 专属消费，不属 RTL 仿真产品清单」+ 归位条件）⇒ 零改动 tsv 不变量 |
| **候选 B** | 在 tsv 加一行登记它 ⇒ ❌ 会破坏 coverage 守恒（`28 == 10 ∪ 17`）与产品级护栏 ⇒ **不推荐** |
| **候选 C** | 扩展 `rtl_emitters()` 的归属面（再加一个登记文件）⇒ 过度设计 |
| **推荐** | **A**（一行登记 + 理由 + 归位条件）⇒ 之后 `run_all_ported_tests.sh` 恢复 exit 0，change 1 AC10 / change 2 AC9 的「脚本绿」部分自动成立 |
| **不采纳的后果** | 「护栏必须绿」类 AC 永远红 ⇒ 而它与这三份 change 的实现无关 ⇒ 会把无关的既有缺口算到本轮头上 |

### D4 · 三份 change 的**计数类 AC** 与已签批裁决冲突（一次批改）

| AC | 现写字面 | 实测 | 冲突源 |
|---|---|---|---|
| change 1 AC10 | 「PASS 计数仍 **28**」 | 27 | change 3 AC27（`axi4_full_example` 退役 ⇒ 28→27，**已批准**） |
| change 2 AC9 | 「PASS 计数仍 **28**」 | 27 | 同上 |
| change 3 AC10 | 「CTEST_TOTAL **268** → 268」等 | 269 / 258 / 233 / 159 / 20 | 同 change 的 F2 新增 `tests/test_product_check.cpp`（+1） |

**推荐**：planner **一次批改三处**为「以实测回填」并注明变更来源（change 3 AC27 的 −1 / F2 的 +1）；change 3 AC10 自身已写「上面的算术是推导，不是实测值 ⇒ MUST 以实测回填」⇒ 只需把字面数字改成实测值并标注来源。

### D5 · 三份 change 的**归档门**（tasks.md 账实）

| change | 任务行 | ✅ | ☐ | 说明 |
|---|---|---|---|---|
| change 1 | 29 | **0** | 29 | 交付物在（AC 15/17 通过），任务板显示「一件未做」 |
| change 2 | 30 | **0** | 30 | 同上 |
| change 3 | 39 | 35 | 2（`F4` follow-up · `F9` **实际不适用**） | `F9` 应改标 `N/A`，`F4` 应落独立立项 |

**推荐**：builder 回填 change 1 / 2 的 tasks.md（按本轮 AC 判定逐条核对后勾选，**不得**无证据批量勾）；change 3 把 `F9` 改为 `N/A`、`F4` 记入 backlog。
**另**：`.rddf/state/iteration.json` 不存在 ⇒ 本地验收的「`tasks_done == tasks_total > 0`」门不可评估 ⇒ 要么补该文件，要么在 change 记录里注明本仓以 tasks.md 为准。

---

## 二、**planner** 订正（3 条 · AC 措辞，builder 不得代改）

| # | AC | 现写法 | 问题 | 建议措辞 |
|---|---|---|---|---|
| P1 | change 3 **AC17** | 「**example 侧对每个的调用 == 1**」 | 与同一 AC 的「**编排整体下沉**」（蓝图 §4.3.1 钉死）**互斥** ⇒ 编排下沉后 `product_modules`/`tamper_hier` 只能由库层内部调用（实测 0 / 0 / 1） | 「**库层实现 TU 内**每个公开函数恰 1 处调用 + **example 侧**对 `check_products` 恰 1 处」 |
| P2 | change 3 **AC18** | CHECK 3 在 verilator 不可用时打印 `SKIP` 且不计 failures（写成**合法语义**） | 那是 **fail-open 缺陷**（实测「从 `build/` 跑必 SKIP」⇒ lint 从未被验却绿灯）⇒ 实现已**有意**改判 FAIL | 「CHECK 3 **NEVER 静默跳过**：linter 缺失 ⇒ 按失败计（fail-closed）」；并在 AC 里补「实现差异已登记于 `docs/CHANGELOG.md`」 |
| P3 | change 3 **AC22** | 「禁以裸行号 pin ⇒ 定位 MUST 用复算命令」 | 蓝图 §4.3.1 的表**仍保留** `行` 列（`:43`/`:50`…）⇒ 字面与 AC 冲突（对象文件已删 ⇒ 不会漂移，但字面不符） | 「行号列仅作**历史标注**（对象文件已删除 ⇒ 不会漂移）；**可复算定位** MUST 用新落点的 grep 命令」 |

> ⚠️ P1 / P2 **已在** `openspec/changes/example-file-boundary-2026-11/tasks.md` 的「需 planner 订正的 AC 措辞」表里登记 ⇒ 本次验收只是**再确认并补齐 P3**。

---

## 三、**builder** 修（3 条 · 代码/记录，均低风险）

| # | 目标 | 具体改动 | 判别力要求 |
|---|---|---|---|
| B1 | change 1 **AC16** | `examples/axi4/axi4_lite_example_top.h:3` 的 `examples/axi4/axi4_lite_example.cpp` → `tb_axi4_lite_example.cpp` | 一行注释；改后 `grep -rE 'axi4_lite_example\.cpp' examples scripts tests docs/*.md AGENTS.md` ⇒ 零命中 |
| B2 | change 2 **AC4** | 在 `scripts/verify_examples_rtl_sim.sh:465` 的发射调用点落两份证据：`<work>/argv.txt`（原始 argv 逐行）+ `<work>/argv.norm`（剥 `argv[0]` + 模式标记后的 flag token 序列） | 必须配一条 `--self-test` 反向判据：「`argv.norm` 含由 `emitter_args` 透传的 `-` ⇒ 必红」⇒ 否则落盘仍无判别力（AC4 原文要求的「原始 argv MUST 含 `--emit-rtl`」也要断言） |
| B3 | change 3 **AC27** | `docs/CHANGELOG.md` 补一行「`axi4_full_example` 退役（覆盖证据迁至 `tests/test_axi4_full_burst.cpp`）」 | 现状：缩面登记只落在 `examples/axi4/AGENTS.md:7` + `tasks.md` A7 + 等价记录 ⇒ AC 允许，但 CHANGELOG 是用户可见面 ⇒ 建议补 |

---

## 四、裁决后的**收口顺序**（建议）

1. **用户**裁决 D1–D5（本文件第一节）。
2. **planner** 批改 P1–P3 + D4（三处计数措辞）⇒ ⇒ 一次改完，避免每个 verifier 重复命中。
3. **builder** 修 B1–B3 + 回填 change 1 / 2 的 tasks.md（D5）⇒ ⇒ 复跑本轮已列的复现命令。
4. **verifier** 第二轮复核（预算 3 轮，已用 1 轮）：只复核「上轮不通过项 + 本轮新增 diff」。
5. 三份 change 齐 ⇒ ⇒ 归档（`openspec archive <name>`；archive 前实门 = `scripts/verify_openspec_delta.sh`）。

## 五、本次裁决请求的**边界声明**

- verifier **未改**任何实现代码 / AC 文本（`git status` 仅新增三份报告）。
- 本清单是**建议**，不是已生效的决策；每条都给出了「若采纳」的**具体落点**，便于你一句话批准后直接执行。
- 若你只批准一部分（D1/D2/D3/D4/D5 中任一子集），请指明范围 ⇒ 未批准项将保持 open，并会进入下一轮验收的命中列表。

---

## 六、**D6（新增 · 2026-11-18 用户裁决）**：归档顺序与两份已签批 gate 互斥

> **来源**：critic 的 plan 档 **BLOCKER**（`pip` 无关 —— 判据面 = `openspec/changes/**`）。
> **事实（已核实）**：① change 1 **G-1** 的可执行前置 `test -f openspec/specs/examples-rtl-sim-coverage/spec.md` **不满足**（capability 不存在）② change 2 **G-3** = 「前置 change 先归档」而上游未归档 ③ change 1/2 的 delta 引用的「**既有 requirement**」只存在于**未归档**的上游 delta（7 条：三方等式 / 新鲜度 / 双向键集合 / 无残留 / 可判定生成物 / 生成覆盖度 / 列号推导）。

**裁决（用户 2026-11-18）：D6-1** ——

| 项 | 内容 |
|---|---|
| 本批归档 | **只归档 `example-file-boundary-2026-11`**（其 delta = 2 `MODIFIED`（capability 已存在）+ 1 `ADDED`（新建 `example-component-boundary`）⇒ **零 forward reference**） |
| 挂起 | `axi4-verif-driven-cpp-2026-11` · `emit-cli-into-example-2026-11` —— 待上游 `rtl-verification-single-source-2026-11` **验收 + 归档**后依次归档 |
| 未采用的候选 | **D6-2**（修订 G-1/G-3 接受 interim 悬空引用）= 留债；**D6-3**（先归档上游）= 把未验收的 23 条 AC 冻进正典 |
| 留痕 | 本节 + `implementation-path-2026-11-18.md` §十；change 1/2 归档时其 proposal 的 gate 行需加 dated addendum（由 planner 执行） |
