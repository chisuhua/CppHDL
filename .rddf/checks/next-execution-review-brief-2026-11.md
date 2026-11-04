---
evidence: probe:.rddf/checks/objective-test-coverage-status-2026-11.md
---

# 下一步执行说明（供 oracle / critic 审查 · 2026-11-01）⇒ ✅ **已结案（2026-11-03 核对）**

## ✅ 结案核对（2026-11-03 · build 侧逐项核对）

⚠️ **本 brief 是 2026-11-01 的历史快照** ⇒ ⇒ 它列的「下一步」**全部已执行或已改判** ⇒ ⇒
下表是逐项的**最终归宿**：

| brief 项 | 最终归宿 |
|---|---|
| **N1**（批次 C 修正：补齐既有 `_behavior` 未覆盖的用例）| ✅ **已被更强的路线取代**：路线 A/B/C 的双后端 parity 批次（2026-11-02/03）⇒ parity 文件数 33 → **66** · pair 194 → **289** ⇒ ⇒ `AXI peripheral` 的用例已被覆盖 |
| **N2 / N2′**（判据松绑 = 「断言数(with verilator) == 2 × 断言数(interpreter-only)」）| ✅ **已落地**（`tests/dual_backend_fixture.h` + 全部 dual 文件遵循该判据）⇒ ⇒ 本仓通用的判据代号即 **N2′** |
| **N3**（B1/B6 落地）| ✅ ADR-055 + change `stream-builder-and-pipe-ready-ownership-2026-10`（2026-11-01 归档）|
| **N4**（共享 fixture）| ✅ **不再需要**：批次 B2 的 4 个 `_behavior` 文件的 DUT 复用**仓内既有**组件（`test_bool_connection_behavior` 等）⇒ ⇒ 局部类提升为共享 fixture 的必要性消失 |
| **N5**（A4 普查的待实测项：transpose / 4 个 filters / join / combine_with）| ⏳ **仍未做**（低优先；A4 审计表本身已归档 ⇒ ⇒ 可随时按该表续做）|
| **§5.3 B2**（in-force spec 的 `Purpose` 仍写已被推翻的症状）| ✅ 已处置（`chlib-stream-ready-ownership/spec.md` 的 `## Purpose` 段明写「已被实测推翻」⇒ ⇒ **段落引用**，不用行号 ⇒ ⇒ 归档后不漂）|
| **§5.3 B5 / B6**（N2′ 与排除项冲突 / B1 与 B6 的 delta 撞同一条）| ✅ 已绕开（N2′ 采纳）· ✅ 已合并成一个 change（B1+B6 同 change 落地）|
| **§5.3 SHOULD_FIX**（A4 的 N5 范围 / plan 的「DUT 数」列名 / 台账文件名 / B7·B8 的可机械核验定义 / B4 的悬空引用面）| ✅ **本轮批量订正**（2026-11-03）：plan 的成本单位已改为 **pair 数**（§3.0）⇒ **列名同步订正**；台账 `stream-builder-chain-silent-word-loss-2026-11.md` 的**标题**已订正为「上游反压丢失」并注明文件名保留原因；B7/B8 已落地为可机械核验的判据；B4 的悬空引用面已在 `fwft-fifo-physical-removal` 归档时订正 |
| **§5.3 oracle §5**（纪律机制 M：台账 `evidence:` + 护栏 + 自检）| ✅ **已实施**：`scripts/verify_ledger_evidence.sh`（fail-closed + 自检）⇒ ⇒ **2026-11-03 已把 50 个台账的 `evidence:` 补齐（缺失 50 → 0）** |
| **§5.3 oracle §4**（暂停 A5，转预算给 B1/B6）| ✅ 已被取代：objective `objective-test-coverage-100pct` **已关闭**（`status: completed`，commit `491d537`）|
| **§5.3 oracle §3**（B1 的「第三类」表述 ⇒ 改为「链内闭包 + 不对外暴露」）| ✅ 已写入 ADR-055 的裁决表述 |

⇒ ⇒ **本 brief 自此归档为决策史**；当前待裁决项集中在
`.rddf/checks/decision-request-input-port-guards-2026-11.md`（D1 / D2 两条）。

---

## （以下为 2026-11-01 的原始 brief · 保留作决策史）

## 1. 已完成（本轮，可审查）

| 项 | 产出 | 位置 |
|---|---|---|
| A6 | stream 域收口清单（17 行能力面，状态由**指针集合推导** ⇒ 不可说谎）+ 护栏 + CI 2 处 | `.rddf/checks/stream-domain-closure.md` |
| A1 | `fwft_fifo` over-promise 在**活文档 + in-force ADR-008** 上订正 | `include/chlib/fifo.h` / `docs/adr/ADR-008` |
| A3 | 「builder 静默丢字」**被实测推翻** ⇒ 真缺陷 = **上游反压从未建立**（`source.ready` 恒 0 = 死路）+ 新静态判据 R9 + 投毒 | `openspec/changes/archive/2026-11-01-...` |
| A4 | 命名诚实性**普查表** + 挖出 `stream_pipeline.h` 的**双缺口**（pipe 只在常量接线下活着 + 不在契约覆盖范围） | `.rddf/improvements/chlib-naming-honesty-audit-2026-11.md` |
| 档 B | **待 architect 裁决**清单（B1–B8，每项带实测）+ **裁决会议议程** | `.rddf/checks/architect-decisions-pending-2026-11.md` |
| A5 批次 A | `BranchConditionUnit` 1 pair（试点） | `openspec/changes/archive/2026-11-01-dual-backend-parity-batch-2026-11/` |
| A5 批次 B1 | `chlib::AssertChecker` 5 pair（纯组合） | `openspec/changes/archive/2026-11-01-dual-backend-parity-batch-b-2026-11/` |

**A5 的两个硬事实**（都实测，改变了规划）：

1. **可达上限 = 50 < 阈值 60**（⚠️ **二次订正**：初报 58 **漏排除**「`_behavior` 兄弟已存在」的 **8** 个 ⇒ ⇒ 真实可转 **21**）⇒ objective §2 自订的「阈值 ≤ 诚实保守上限」**已被违反**（幅度比初报更大）⇒ 决策门触发 ⇒ build 只交数字，不改判据。
2. **成本单位 = pair 数**（每 pair 独立 device ⇒ 缓存 key 不同 ⇒ 一次编译 10.66 s）⇒ 批次预算由 Σ 56 → ≈ 41 min。

## 2. 下一步要执行的任务内容（**本次审查对象**）

| # | 任务 | 性质 | 风险 | 前置 |
|---|---|---|---|---|
| N1 | **批次 C 修正**：⚠️ **前提已订正** —— `test_axi_gpio_behavior.cpp` **已存在**（2026-10-20 · 7 pair · 已注册）⇒ ⇒ ⇒ **不是「新建文件」而是「补齐既有 `_behavior` 未覆盖的用例」** ⇒ ⇒ ⇒ 真正缺 dual 的**恰是**被排除的多断言用例（`RVALID 保持` / `DECERR` / `awaddr 改写` / `事务后下一笔` / `rst_n 释放 busy`）⇒ ⇒ ⇒ ⇒ **⇒ N2′ 是它的前置** | 纯补覆盖 | 低 | **N2′**（判据松绑） |
| N2 | **判据松绑 = N2′**：判据改为「**断言数(with verilator) == 2 × 断言数(interpreter-only)**」（**已在既有 gpio 文件实测成立：46 == 2×23**）⇒ ⇒ **对 pair 结构零约束** ⇒ ⇒ 多断言 pair 无需拆 ⇒ ⇒ **零额外编译成本** · 负向只需把 verilator 移出 PATH（**秒级**，不是 ~40 min） | 判据改写 + 已归档批次的**注释**订正 | **低** | 无 |
| N3 | **B1/B6 落地**（裁决后） | 真缺陷修复 + **顺带产出路线 B 的 dual 文件** | 中 | architect 裁决 |
| N4 | **共享 fixture**（批次 B 的前置：6 个文件的 DUT 是局部类 ⇒ 必须提升） | 质量（消除「同一逻辑两处维护」） | 中 | 需先知 B1/B6 是否要建 fixture |
| N5 | **A4 普查的待实测项**（transpose / 4 个 filters / join / combine_with 的语义核对） | 只读 + 补判据 | 低 | 无 |

## 3. 需要裁决的内容

### 3.1 architect（8 项，议程见 `architect-decisions-pending-2026-11.md`）

B1（中间级 ready 归属，**建议：新立 ADR-055「第三类」**）· B2（demux 方向）· B3（fork 同步语义）· B4（`fwft_fifo` 删除）· B5（`sequence` 重复）· B6（pipe 族纳契约）· B7（未驱动字段诊断形态）· B8（命名审计制条款）

### 3.2 planner（2 项）

P1 **阈值 60 vs 自订不变量**（实测上限 58）⇒ 背书路线 B / 修阈值 / 重新界定「诚实上限」
P2 **CI 墙钟 45 min**（增量 ≈ 41 min，但 45 min 覆盖全部构建 + 既有 39 条目）⇒ 杠杆：缓存 `~/.cache/cpphdl/verilator`（内容寻址）/ `ctest -I` 分片

## 4. 请重点审查

1. **已完成**：A6 清单的「状态由指针集合推导」机制是否站得住；A3 的根因订正是否可信；A4 普查的覆盖面是否够。
2. **下一步**：N1 的范围（只转 5 个 1-REQUIRE 用例，排除多断言用例）是否是**掩盖问题**而非解决问题；N2 的判据升级是否值得其成本；N3/N4 的排序是否正确。
3. **待裁决**：8 项 architect 裁决的建议是否正确（尤其 B1 的「新立 ADR-055 + 第三类」）；2 项 planner 裁决的表述是否准确。
4. **纪律**：本轮已出现多次「自己的结论被自己的探针推翻」（builder 丢字、XML 判据、成本单位）⇒ 是否需要加强「结论复核」的机制。

---

## 5. ⚠️ 审查结论（oracle + critic · 2026-11-01）与**据此的修正**

### 5.1 结论

- **critic**：**REJECT** · `mergeable: no` · **6 个 BLOCKER**（B1–B6）
- **oracle**：3 个新根因（判据口径当事实 / 「第三类」开放式分类 / 假 ❌）+ 给出 **N2′** 正解

### 5.2 已在本轮**当场修掉**的

| # | BLOCKER | 修法 | 状态 |
|---|---|---|---|
| **B1** | A6 清单的 ❌ **可零成本伪造**（降级一行 + 填任意 debt）⇒ 且 `MIN_ROWS=12` vs 实际 17 ⇒ ⇒ **删 1–5 行仍绿**（静默缩面）；第 10 行是**假 ❌** | ① **计数锚点**（`TOTAL_ROWS`/`UNPAID_ROWS`）⇒ 抓静默删行/降级 ② **R2 收紧**：❌ ⇒ MUST NOT 有 `test:`（抓假 ❌）③ **新判据 `r10`**（`sync_fifo` 自保护结构）⇒ ⇒ **第 10 行假 ❌ → 真 ✅（一行收口，零 verilator 编译）** ④ **新增 2 种投毒**（降级 / 只删 1 行）⇒ ⇒ **6 种投毒全被抓** | ✅ 已修并自证 |
| **B4** | 「诚实上限 58」**漏排除已有 `_behavior` 双胞胎** ⇒ ⇒ 真实上限更低 ⇒ ⇒ planner 输入失真 | 实测重算：**真实可转 21 ⇒ 上限 50** ⇒ ⇒ plan 全部数字已改 | ✅ 已修 |
| **B3** | 批次 C 的前提错了（`test_axi_gpio_behavior.cpp` **已存在**）⇒ ⇒ 真正缺 dual 的是**被排除的多断言用例** | brief N1 + plan §4 已改写为「补齐既有 `_behavior` 未覆盖的用例」 | ✅ 已修 |

### 5.3 待处理（本轮未做 ⇒ 已登记）

| # | 项 | 归属 | 处置 |
|---|---|---|---|
| **B2** | **in-force 主 spec 的 Purpose 仍写着已被推翻的症状**（`chlib-stream-ready-ownership/spec.md:64-66` 的「静默丢字」）⇒ ⇒ **假绿固化在契约面** | architect（Purpose 订正路径：归档后手工补写，仓内先例存在） | 需专 change ⇒ ⇒ ⚠️ **2026-11-04 状态订正**：已由 change `spec-stale-closure-text-correction-2026-11` 处置（结论见本文 §5.3 表上方的 B2 行）· 本行里的 `spec.md:64-66` **行号引用作废**（主 spec 已整条替换 + 手改 Purpose） |
| **B5** | N2 的 per-Section 机制与 **in-force 契约 R6 排除项第 4 条 + fixture 的 SECTION 禁令**冲突 ⇒ ⇒ N2′（判据松绑）**绕开了它** | architect | N2′ 已绕开 ⇒ 排除项保持不动 |
| **B6** | **B1 与 B6 的 spec delta 会落在同一条 Requirement** ⇒ ⇒ MODIFIED 是整条替换 ⇒ ⇒ 后合并者**静默覆盖**前一个 ⇒ ⇒ 议程的执行顺序不可行 | architect | ⇒ ⇒ 建议**合并成一个 change**，或 B1 写成 `ADDED` 新条款 |
| SHOULD_FIX | A4 的 N5 范围缺 `stream_half_pipe` / `mux` / `demux`；plan 批次表列名仍写「DUT 数」；台账文件名仍带 `silent-word-loss`；B7/B8 选项缺可机械核验定义；B4 的悬空引用面（`ADR-008:105` + `chlib-stream-semantics` Scenario） | builder | 下一轮批量订正 |
| oracle §5 | **纪律机制 M**：台账 front-matter 必填 `evidence:`（`test:` / `probe:` / `unmeasured` 三值）+ `scripts/verify_ledger_evidence.sh`（fail-closed + 自检）⇒ ⇒ **本次三次自推翻全部发生在「结论已落台账之后」** ⇒ ⇒ 该机制**恰好能回溯命中**它们 | builder | 建议即刻立项（独立 change） |
| oracle §4 | **暂停 A5**，转预算给 B1/B6 ⇒ ⇒ A5 的计数边际价值 ≈ 0（B1/B6 直接产出路线 B 文件）+ 墙钟**负值** | planner/architect | 已在 plan §6.1.4 登记 |
| oracle §3 | **B1 的「第三类」表述** ⇒ ⇒ 开放式分类轴 ⇒ ⇒ 建议改为「**链内闭包不变式 + 不对外暴露 ⇒ 不是 ADR-054 的例外**」（主文本落 capability，ADR-055 只写推导）⇒ ⇒ **不修订已签批 ADR-054** | architect | 建议写入 B1 决策请求单 |

### 5.4 修正后的执行顺序

```
① N2′（判据松绑，10 分钟）⇒ 解锁 N1
② architect 裁 B1/B6/B4 + planner 裁阈值（用 **50**）/CI
③ B1/B6 落地 ⇒ ⇒ **顺带产出路线 B 的 dual 文件**（⇒ ⇒ 直接补 parity 计数）
④ N5（A4 待实测项，零编译）
⑤ N1（补齐既有 _behavior 未覆盖的用例）
⑥ N4（共享 fixture，若届时仍需要）
```

