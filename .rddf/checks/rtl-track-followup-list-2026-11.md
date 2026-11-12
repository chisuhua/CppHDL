---
id: rtl-track-followup-list-2026-11
日期: 2026-11-12
提出者: planner（主清单 `.rddf/checks/rtl-track-correction-list-2026-11.md` §9.5 D5 的登记义务）
状态: ✅ **已审查通过**（oracle 第 1 轮 **REJECT** → 9 条全部采纳 · critic 第 1 轮 **APPROVE_WITH_CONDITIONS** → 0 BLOCKER · 8 SHOULD_FIX + 3 NITS 全部采纳）· **修订已执行**（见 §7）
审稿记录:
  - oracle 第 1 轮（2026-11-12）⇒ **REJECT** · 9 条建议（4 条必改 + 5 条其它）⇒ ⇒ **全部采纳并已并入本文**（v2）
      （含 2 处**事实前提订正**：`进度:` **不是**可信锚（2 份各有一处不实陈述）· ≥5 条 AC 不可按「已落地」勾）
  - critic 第 1 轮（2026-11-12）⇒ ✅ **APPROVE_WITH_CONDITIONS** · `mergeable: yes` · `scope_exceeded: false` ⇒ **0 BLOCKER** · **8 SHOULD_FIX** + 3 NITS ⇒ ⇒ 全部采纳并已并入本文（v3）
关联:
  - .rddf/checks/rtl-track-correction-list-2026-11.md（主清单 · §6 边界表 #10 的登记义务 · §9.5 D5）
  - .rddf/improvements/port-name-literal-parity-2026-11.md
  - .rddf/improvements/node-name-prefix-consistency-2026-11.md
  - .rddf/roadmap/phase-6-rtl-productization.md（§0.0 状态词表 · §4 独立立项清单）
  - .rddf/reviews/b0-batch-verify-2026-11.md · b1-batch-verify-2026-11.md · port-naming-parity-critic-round{3,4}-2026-11.md
  - openspec/changes/archive/2026-11-11-port-naming-member-parity-contract-2026-11/
  - openspec/changes/archive/2026-11-11-node-name-prefix-path-contract-2026-11/
  - .rddf/checks/decision-request-port-naming-parity-2026-11.md
  - .rddf/checks/arch-residual-registry.md（行 21 / 21b）
---

# 订正清单（后继 · v2）：另外 2 份草案的**同型四通道漂移 + AC 逐条回填**

> ⚠️ **本清单是主清单 §6 边界表 #10 登记义务的兑现**，**不是**主清单的延伸补丁。
> ⚠️ **v2 的变化**：oracle 第 1 轮 **REJECT** ⇒ ⇒ **v1 的事实前提（"`进度:` 是锚 ⇒ ⇒ 全部已落地"）
> 被证伪** ⇒ ⇒ v2 改为「**逐条可满足性地图**」驱动 ⇒ ⇒ 并新增**第四种形态**（🔵）与**两条新登记义务**。

---

## 0. 总览

| # | 漂移 | 严重度 | 影响谁 | 归属 |
|---|---|---|---|---|
| **1** | 2 份草案的正文 `- **Status**` 仍是 `📝 draft`，而 frontmatter `状态:` 是 `implemented` ⇒ ⇒ **两通道不一致** | 🔴 **高** | 下一个会话的 builder 会以为「还没批准」 | planner |
| **2** | 同 2 份的 `improvement-suggestions.md` **索引表状态列**同样是 `📝 draft` ⇒ ⇒ **第三通道也不一致** | 🔴 **高** | 同上 | planner |
| **3** | 同 2 份的 `## Acceptance` 段共 **40** 条复选框，**已勾 0** ⇒ ⇒ 已落地的工作在 AC 上完全看不出来<br>⇒ ⇒ **逐条档位（§1.3）= 34 ✅ / 2 🔵 / 4 ⬜** ⇒ ⇒ **不是「40 条全做完」** | 🔴 **高** | 无法据此判断「哪些真做完」 | planner（证据来自 verifier 报告 / 已归档 change / **物理证据**） |

⇒ ⇒ **三通道（正文 / 索引表 / AC）全部漂移** ⇒ ⇒ 与主清单项 1 + 项 2 **完全同型**
⇒ ⇒ **判据面、写法、护栏形态 MUST 与主清单一致**（⇒ ⇒ 见 §3 的「判据沿用表」）。

---

## 1. 现状（2026-11-12 实测）

### 1.1 三通道对照

| 草案 | frontmatter `状态:` | 正文 `- **Status**` | 索引表状态列 | AC 已勾 / 总数 |
|---|---|---|---|---:|
| `port-name-literal-parity-2026-11` | **`implemented`** | `📝 **draft**（待用户拍板 ⇒ 拍板后转 builder 流程）` | `📝 draft（**metis 第 1 轮已处置**（28 条 finding · …）` | **0 / 25** |
| `node-name-prefix-consistency-2026-11` | **`implemented`** | `📝 **draft**（待用户拍板 ⇒ 拍板后转 builder 流程）` | `📝 draft（**metis 第 1 轮已处置**（31 条 finding · …））` | **0 / 15** |

- 复核命令：
  ```bash
  for n in port-name-literal-parity node-name-prefix-consistency; do
    f=".rddf/improvements/$n-2026-11.md"
    printf '%-34s fm=%-12s body=%-10s idx=%-10s ac=%s/%s\n' "$n" \
      "$(grep -m1 '^状态:' "$f" | sed -E 's/^状态:[[:space:]]*//')" \
      "$(grep -m1 -E '^- \*\*Status\*\*' "$f" | grep -oE '\*\*(implemented|approved|in-progress|draft)\*\*' | head -1 | tr -d '*')" \
      "$(grep "$n" improvement-suggestions.md | grep -oE '\*\*(implemented|approved|in-progress|draft)\*\*' | head -1 | tr -d '*')" \
      "$(awk '/^## metis 第 1 轮 gap 分析与处置/{exit} {print}' "$f" | grep -cE '^\s*-\s*\[[xX]\]')" \
      "$(awk '/^## metis 第 1 轮 gap 分析与处置/{exit} {print}' "$f" | grep -cE '^\s*-\s*\[[ xX]\]')"
  done
  ```

### 1.2 ⚠️ **`进度:` 只是「线索」，不是「锚」**（oracle 第 1 轮订正）

> ⚠️ **v1 的错误**：v1 写「`进度:` 维护得最好 ⇒ ⇒ 它是锚 ⇒ ⇒ `implemented` 有据」
> ⇒ ⇒ **被证伪**：`进度:` 与 frontmatter `状态:` 是**两个可独立编辑的通道**，前者**会说谎**，
> 且它**自己**就含两处不实陈述（见 §1.5）。
> ⇒ ⇒ **唯一真相 = 物理证据**（代码 / 测试 / spec / 归档 change / 登记行）⇒ ⇒ 下表即其清单。

⇒ ⇒ **两份的 `implemented` 判定**：

| 草案 | `implemented` 是否成立 | 依据 |
|---|---|---|
| `node-name-prefix-consistency` | 🟡 **大体成立 · 有 2 条缺口** | P1 / P3 / P4 全实（物理证据见 §1.4）⇒ ⇒ **P2 的「产物层」两条从未实施**（§1.5 ②）⇒ ⇒ 缺口**必须登记**（§2 F6） |
| `port-name-literal-parity` | 🟡 **大体成立 · 有 4 条形态偏差** | P1 / P2 全实（物理证据见 §1.4）⇒ ⇒ **P3 的 3 条 + 残差登记 1 条**的**交付形态已被现实改写**（§1.5 ③）⇒ ⇒ 缺口**必须登记**（§2 F3 / F7） |

### 1.3 逐条可满足性地图（**本清单的执行依据** ⇒ ⇒ 取代 v1 的「小节标题分区」）

> ⚠️ **三态判定**（沿用主清单 §2.4-1 的三档 + **新增第四种形态**）：
>
> | 档 | 判定 | 写法 |
> |---|---|---|
> | **✅ 可勾** | 有**物理证据**（代码 / 测试 / spec / 归档 change / 登记行） | `- [x] <原文> ⇒ 证据: <载体路径>` |
> | **🔵 第四种形态** | 判据**本体已被现实改写**（交付形态被替代 / 条件化未触发 / **判据所点的文件形态已不存在**）⇒ ⇒ **MUST NOT** 记成「已满足」，**MUST NOT** 记成「未开工」 | `- [ ] <原文> ⇒ 🔵 裁决注记: <被何种现实替代> ⇒ 证据: <真实载体路径>` |
> | **⬜ 真未开工** | 判据**仍成立**但**从未实施** ⇒ ⇒ MUST 有归属落点 | `- [ ] <原文> ⇒ 归属: **独立立项**（批次表 §4 第 N 行）` |

#### 1.3.1 `node-name-prefix-consistency-2026-11`（15 条）

> ⚠️ **锚的形式 = 「小节 · 第 N 条」**（**不用行号**）⇒ ⇒ 理由：主清单 critic 第 4 轮
> SHOULD_FIX-2 的教训（「`file:line` 指针**会随文件编辑漂移** ⇒ ⇒ 建议长期改为「章节名 + 锚点」」）。

| 锚 | 档 | 证据 / 处置 |
|---|:--:|---|
| **N1** 修复(P1/P2) #1 · `component.h` 的 `\|\| parent_path == "unnamed"` 已删除 | ✅ | **代码本体**已删（`include/component.h:293` = `if (parent_path.empty()) {`）⇒ ⚠️ **裸 grep 命中 1 处**（`:274` 是**记录删除历史的注释**）⇒ ⇒ 回填的证据子句 MUST 写「代码本体已删（`:293`）」**而非**「grep 0 命中」 |
| **N2** 修复(P1/P2) #2 · **函数级判据** | ✅（**语义覆盖 · 形态偏差**） | ⚠️ **指针纠正**（critic 第 1 轮）：`tests/test_component_naming.cpp:84` 的用例是「depth≥3 无重复前缀」⇒ ⇒ **依赖组件树** ⇒ ⇒ 与本判据原文「**不依赖** ctx / 组件树 / ConstructionFrame」**相悖** ⇒ ⇒ **不是**本条的载体。真正的载体 = **F-A 块**（`tests/test_component_naming.cpp:197` 起 · 无名根 fixture）+ `.rddf/reviews/b0-batch-verify-2026-11.md` §2.6（4 用例 / 27 断言）+ M3′ 变异。<br>⚠️ **形态偏差 MUST 如实写明**：判据字面要求的「纯函数级」形态（直接调 `build_hierarchical_name`）在测试文件中**不存在**（该符号仅在**注释**里命中）⇒ ⇒ 语义由 F-A 用例覆盖，形态有偏差 ⇒ ⇒ 回填的证据子句 MUST 写「语义覆盖 + 形态偏差」**而非**声称字面满足 |
| **N3** 修复(P1/P2) #3 · **不变量判据** | ✅ | `tests/test_component_naming.cpp:197` |
| **N4** 修复(P1/P2) #4 · **产物层 · 仓内**（`NamedRootArtifactsUnchanged`） | ⬜ | 🔴 **全仓 grep 该标识符 = 0 命中**（只出现在草案自身 `## 范围` 的 P2 段与本条）⇒ ⇒ **从未实施** ⇒ ⇒ `⇒ 归属: 独立立项`（§2 F6） |
| **N5** 修复(P1/P2) #5 · **产物层 · 无名根**（`UnnamedRootArtifactPinned`） | ⬜ | 同上 ⇒ ⇒ `⇒ 归属: 独立立项`（§2 F6） |
| **N6** 修复(P1/P2) #6 · 负向对照 | ✅ | `.rddf/reviews/b0-batch-verify-2026-11.md` §2.6（M3′ 变异） |
| **N7** 修复(P1/P2) #7 · 变异自证含「重建」 | ✅ | 同上 |
| **N8** 修复(P1/P2) #8 · 用例落 `tests/test_component_naming.cpp` + 引用锚点 | ✅ | 该文件**已存在且已注册**（`ctest -N` = 263） |
| **N9** 修复(P1/P2) #9 · 签名零变更 | ✅ | `include/component.h:26` 的声明未变 |
| **N10** 修复(P1/P2) #10 · 既有 ctest 全绿 + `verify_test_counts.sh` 绿 | ✅ | 实测 exit 0 |
| **N11** 契约化(P3) #1 · `component-ownership-and-naming` 追加 Requirement | ✅ | `openspec/specs/component-ownership-and-naming/spec.md`（归档 change `…node-name-prefix-path-contract-2026-11/specs/`） |
| **N12** 契约化(P3) #2 · 新 Requirement 有可构造违反样例 | ✅ | 同上 |
| **N13** 下游登记(P4) #1 · `docs/CHANGELOG.md` 一行 | ✅ | `docs/CHANGELOG.md:513`（bullet 起于 `:513` · `:514` 是续行） |
| **N14** 下游登记(P4) #2 · `public-api-surface.md` 的「行为变更（非符号移除）」节 | ✅ | `.rddf/checks/public-api-surface.md:95-96` |
| **N15** 下游登记(P4) #3 · 方向 (i)（`lnodeimpl` 也剥哨兵）需 architect | ✅ | 草案自身已记「沿用既有裁决」 |

⇒ ⇒ **小计：13 ✅ / 0 🔵 / 2 ⬜**

#### 1.3.2 `port-name-literal-parity-2026-11`（25 条）

| 锚 | 档 | 证据 / 处置 |
|---|:--:|---|
| **Q1** 探针固化(P1) #1 · `tests/test_port_name_parity.cpp` 存在且注册 | ✅ | 该文件有 **5** 个 `TEST_CASE`（`:115/:148/:172/:207/:252`）· `ctest -N` = 263 |
| **Q2**–**Q5** 探针固化(P1) #2–#5 · `Ratchet*` ①②③ + `Device*` ④ | ✅ | 同上 4 个用例 |
| **Q6** 探针固化(P1) #6 · 退化用例 ⑥ | ✅ | 同上 `:252` |
| **Q7** 探针固化(P1) #7 · oracle 建议 3 硬要求 ①（断言 module 文本） | ✅ | 走**共享 helper** `emit_module_text`（`tests/test_port_name_parity.cpp:93-98` · 内含 `verilogwriter` + `print(ostream)`）⇒ ⇒ 5 个用例经该 helper 断言 module 文本 |
| **Q8** 探针固化(P1) #8 · oracle 建议 3 硬要求 ②（三条正向变异） | ✅ | `.rddf/reviews/b1-batch-verify-2026-11.md`（M1/M2/M3 变异自证） |
| **Q9** 探针固化(P1) #9 · fixture 只含本用例所需节点 | ✅ | 同上（用例注释已登记顺序敏感性） |
| **Q10** 探针固化(P1) #10 · 零产物变更 | ✅ | `scripts/verify_hierarchical_rtl.sh` exit 0 · 产物逐字节不变 |
| **Q11** 探针固化(P1) #11 · ⑤ 目标钉登记进残差登记表 | ✅ | `.rddf/checks/arch-residual-registry.md` **行 21 / 21b**（含解除条件 + 触发条件） |
| **Q12**–**Q17** 契约化(P2) #1–#6 | ✅ | 两份主 spec 已合并（`openspec/specs/rtl-hierarchical-output/spec.md:608` + `openspec/specs/component-ownership-and-naming/spec.md`）· 归档 change `…port-naming-member-parity-contract-2026-11/specs/` · `scripts/verify_openspec_delta.sh` exit 0 |
| **Q18** 裁定包(P3) #1 · `.rddf/checks/decision-request-port-name-literal-parity-2026-11.md` 存在且含 6 项 | 🔵 | 🔴 **该文件在磁盘上不存在**（`ls` 实测）⇒ ⇒ 且现存的 `decision-request-port-naming-parity-2026-11.md`（238 行）**不含**那 6 项中的任何一项（4 个特征串 grep = 0 命中）⇒ ⇒ **交付形态已被现实替代**：实际裁定经由 **oracle 建议 1/2/3 + 用户点头**（草案 §裁决记录）+ **台账行 21** + **批次表 §4 行 180** 分散落地 ⇒ ⇒ **MUST NOT 补造该文件**（自我盖章）⇒ ⇒ `🔵 裁决注记` |
| **Q19** 裁定包(P3) #2 · 影响面量化的**枚举源**已写明 | 🔵 | 随 route (c) 判「**当前不做**（触发条件 = 登记表行 21 / 21b）」而**暂缓** ⇒ ⇒ `🔵 裁决注记`（**不是**未开工 ⇒ ⇒ **不是**独立立项）<br>⚠️ **critic 第 1 轮：依据链条补全** —— 登记表行 21 / 21b 的原文**只字未提**「枚举源 / 量化」⇒ ⇒ 该内容散在 `.rddf/checks/decision-request-port-naming-parity-2026-11.md:184` ⇒ ⇒ **注记 MUST 同时点名该行**（否则触发后「该补哪份枚举」无处可查 ⇒ ⇒ 违反 C2-f「🔵 MUST 指向真实载体」） |
| **Q20** 裁定包(P3) #3 · 「不实施」的机械判据（`git diff --stat` 不含 `src/**` / `include/**`） | ✅ | 两个归档 change 的目录**只含** `proposal.md` + `specs/` ⇒ ⇒ 零 `src/` / `include/` 改动 |
| **Q21** 裁定包(P3) #4 · 层次化侧同名兄弟疑静默覆盖 **登记为待查项** | ⬜ | 🔴 **未登记**（登记表 + 台账 + 两份 critic 报告 grep 均无此项）⇒ ⇒ ⚠️ **critic 第 1 轮改判**：判据**本体仍完全成立**，只是**没人登记** ⇒ ⇒ 这正是 **⬜** 的定义（「判据仍成立但从未实施 ⇒ MUST 有归属落点」）⇒ ⇒ 而归属落点**可以**是「F7 登记请求流程」⇒ ⇒ `⇒ 归属: 登记请求（F7）`<br>⚠️ **MUST NOT 由 planner 单改登记表**（双向 1:1 护栏 `scripts/verify_arch_ledger_residuals.sh` + `docs/architecture/**` 属 **architect 域**）⇒ ⇒ F7 的产物是**请求**（`.rddf/checks/decision-request-*.md` 既有形态），**非**登记表本体 |
| **Q22** 残差登记 · 「同名兄弟 ⇒ 必然 `_N`」+「层次化侧同名兄弟疑静默覆盖」登记进残差登记表 | ⬜ | 🟡 **半条满足**：前半（route (c) 当前不做 + 触发条件）= **登记表行 21 / 21b** ✅；后半（层次化侧同名兄弟）**未登记** ⇒ ⇒ 与 **Q21** 同处置 ⇒ ⇒ `⇒ 归属: 登记请求（F7）`（**MUST 明写「半条满足」**） |
| **Q23**–**Q25** 不回归 #1–#3 | ✅ | `ctest` 263/263 · `scripts/verify_hierarchical_rtl.sh` exit 0 · 7 份 RTL 轨草案零改动（**本清单亦须保持**） |

⇒ ⇒ **小计：21 ✅ / 2 🔵 / 2 ⬜**

#### 1.3.3 总计

| 草案 | ✅ 可勾 | 🔵 第四种形态 | ⬜ 真未开工 | 合计 |
|---|---:|---:|---:|---:|
| `node-name-prefix-consistency` | 13 | 0 | **2** | 15 |
| `port-name-literal-parity` | 21 | **2** | **2** | 25 |
| **合计** | **34** | **2** | **4** | **40** |

### 1.4 证据载体表（**逐条 AC 的证据指针来源** ⇒ ⇒ **MUST NOT 指回草案自己**）

| 事实 | 载体 |
|---|---|
| **B0 批验收（含 M3′ 变异 / 产物层探针）** | `.rddf/reviews/b0-batch-verify-2026-11.md`（§2.6 逐条 + `:115` M3′） |
| **B1 批验收（含 M1/M2/M3 变异自证）** | `.rddf/reviews/b1-batch-verify-2026-11.md` |
| 端口命名 parity 的 critic 复审 | `.rddf/reviews/port-naming-parity-critic-round3-2026-11.md` · `port-naming-parity-critic-round4-2026-11.md` · `port-naming-parity-spec-delta-review-2026-11-10.md` |
| P2 / P3 的契约化与归档 | `openspec/changes/archive/2026-11-11-port-naming-member-parity-contract-2026-11/` · `openspec/changes/archive/2026-11-11-node-name-prefix-path-contract-2026-11/` |
| 主 spec 的合并结果 | `openspec/specs/rtl-hierarchical-output/spec.md:608` · `openspec/specs/component-ownership-and-naming/spec.md` |
| 判据用例 | `tests/test_port_name_parity.cpp`（5 用例）· `tests/test_component_naming.cpp`（4 用例） |
| P4 行为变更登记 | `.rddf/checks/public-api-surface.md:95-96` · `docs/CHANGELOG.md:514` |
| 残差登记（route (c) / 目标钉） | `.rddf/checks/arch-residual-registry.md` **行 21 / 21b** |
| route (c) 的「当前不做 + 触发条件」 | `.rddf/checks/decision-request-port-naming-parity-2026-11.md` · 草案 §裁决记录 · 批次表 §4 行 180 |

### 1.5 🔴 `进度:` 的两处**不实陈述**（oracle 第 1 轮实测 ⇒ ⇒ 必须一并订正）

| # | 位置 | 不实内容 | 实测 | 处置 |
|---|---|---|---|---|
| ① | `node-name-prefix-consistency-2026-11.md:8` | 「P2 判据（`tests/test_component_naming.cpp`）**已落**」 | P2 是**三层判据**（草案 `:75`：函数级 + 不变量 + **产物层两条**）⇒ ⇒ `tests/test_component_naming.cpp` 全文件仅 **4** 个 `TEST_CASE`（`:84/:108/:131/:197`）⇒ ⇒ **无** `NamedRootArtifactsUnchanged` / `UnnamedRootArtifactPinned`（全仓 grep **0 命中**）⇒ ⇒ 归档 change 自带的 AC **也不含**产物层 ⇒ ⇒ b0 报告 §2.6 只验了 4 行 ⇒ ⇒ **产物层两条从未实施、从未验收** | `进度:` MUST 改为「P1/P3/P4 已落 · **P2 产物层两条未做 ⇒ 已登记 §4**」⇒ ⇒ **与 frontmatter `状态: implemented` 不矛盾**（主清单 §1.4 的 (b) 口径：缺口**登记**而非降档）⇒ ⇒ ⚠️ **MUST 写全落点**：`phase-6-rtl-productization.md` **批次表 §4**（F6 新增行）⇒ ⇒ **MUST NOT** 只写「已登记 §4」（悬空指针 · critic 第 1 轮） |
| ② | `port-name-literal-parity-2026-11.md:8` | 「P3 裁定包 = `.rddf/checks/decision-request-port-naming-parity-2026-11.md`」 | 该文件**不是** AC `:205` 所要求的 6 项裁定包（4 个特征串 grep = **0** 命中）⇒ ⇒ 且 AC 点名的 `decision-request-port-name-literal-parity-2026-11.md` **不存在** ⇒ ⇒ **交付形态被静默替换**（实际裁定走 oracle 建议 1/2/3 + 用户点头） | `进度:` MUST 改为「P3 裁定**已发生**（oracle 建议 1/2/3 + 用户点头）⇒ ⇒ 交付形态与 AC `:205` 的字面要求**不同** ⇒ ⇒ 见 §1.3.2 的 🔵 裁决注记」 |

---

## 2. 修正方案

| # | 动作 | 写法 | 对应主清单项 |
|---|---|---|---|
| **F1** | 2 份正文 `- **Status**` 对齐 frontmatter | `✅ **implemented**（<阶段> 已验收 · <日期>）⇒ ⇒ ⚠️ <未纳入本轮的增量>（若有 ⇒ ⇒ **已登记 <具名落点>**）`<br>⚠️ **MUST NOT** 写「已登记 §4」（本清单的 §4 是「执行顺序」）⇒ ⇒ **MUST 写具名落点**：<br>　`port-name-literal-parity` ⇒ `批次表 §4「N5 第 2 步」行 + .rddf/checks/arch-residual-registry.md 行 21/21b`<br>　`node-name-prefix-consistency` ⇒ `.rddf/checks/arch-residual-registry.md 行 21（与 sister 共用）+ 批次表 §4「新增行」（产物层双测试，见 F6）` | 项 1 |
| **F2** | 2 份索引表状态列对齐 | 同 F1 的档位词（`implemented`）⇒ ⇒ **索引表 MUST 含档位词**（当前**连档位词都没有** ⇒ grep 会漏 ⇒ ⇒ 格式 MUST 与 7 份 RTL 轨**逐字同型**） | 项 1 |
| **F3** | **40 条 AC** 逐条回填 | **三档 + 🔵 第四种形态**（写法见 §1.3 的表）⇒ ⇒ **逐条以 §1.3 的地图为准**，**MUST NOT** 按小节标题整段归类 | 项 2 |
| **F4** | ⑤「目标钉 · 条件化未触发」的登记式处置 | ⚠️ **MUST NOT** 写成「未完成」（与 `implemented` 字面互斥）⇒ ⇒ 也 **MUST NOT** 写成「已完成」⇒ ⇒ 走 **🔵 第四种形态**。<br>⚠️ **模板 MUST 含路径 token**（否则违反本清单自己的 C2-a ②）：<br>``⇒ 🔵 裁决注记: route (c) 触发条件未达成 ⇒ 当前不做 ⇒ 证据: `.rddf/checks/arch-residual-registry.md` 行 21/21b（route (c) 触发条件）`` | 项 1-b 同款陷阱 |
| **F5** | **修 `进度:` 的两处不实陈述** | 见 §1.5 的处置列（**MUST NOT** 只改 AC 留 `进度:` 漂移 ⇒ ⇒ 否则 C1-c「`进度:` MUST NOT 与 `状态:` 矛盾」当前**就不成立**） | 项 1 |
| **F6** | **批次表 §4 新增 1 行** | 对象 = `node-name-prefix-consistency` 的 **产物层双测试**（`NamedRootArtifactsUnchanged` + `UnnamedRootArtifactPinned`）⇒ ⇒ 判别力自证要求 = 照 N5 行粒度（对象 + 判别力自证 + 触发条件）⇒ ⇒ **先于或同批于** F3（否则 2 条 ⬜ AC 无归属落点 ⇒ ⇒ C2-e 无法闭合） | 项 2 / S1.5 同款 |
| **F7** | **向 architect 发登记请求** | 「层次化侧同名兄弟疑静默覆盖」（`hierarchy_verilog.cpp:247`）待查项 ⇒ ⇒ **MUST NOT 由 planner 单改**登记表（双向 1:1 护栏 `scripts/verify_arch_ledger_residuals.sh` + `docs/architecture/**` 属 architect 域）⇒ ⇒ 产出**登记请求**（载体 = `.rddf/checks/decision-request-*.md` 的既有形态）⇒ ⇒ 在此之前，AC `:208` / `:210` 走 **🔵**（**明写「半条满足 / 未登记」**） | 项 2 / 边界 |
| **F8** | **登记「索引表状态列自动生成」为独立立项** | 索引表状态列**人工维护** ⇒ ⇒ 9 份要人工同步 ⇒ ⇒ 必然漂 ⇒ ⇒ 根本修法（**从 frontmatter 生成** + 一致性护栏，类比 `scripts/verify_roadmap_fragments.sh`）**MUST NOT** 塞进本清单 ⇒ ⇒ **登记进批次表 §4** | 边界 / 长期 |

---

## 3. 判据（**沿用主清单 §2.5 的判据面** ⇒ ⇒ 写法逐字同型）

> ⚠️ **判据沿用表**（本清单 MUST NOT 发明新判据 ⇒ ⇒ 主清单的教训直接继承）：

| 判据 | 沿用主清单 | 本清单的取值 |
|---|---|---|
| **C1-a（四通道同档）** | §2.5 C1-a | 对 **2 份**逐份比对 frontmatter `状态:` / 正文 `- **Status**` / 索引表档位词 ⇒ ⇒ **0 处不一致** |
| **C1-c（`进度:` 不与 `状态:` 矛盾）** | §2.5 C1-c | 2 份的 `进度:` MUST NOT 含「未做 / 未落 / 待」等与 `implemented` 冲突的词，**除非**同行给出**具名登记落点** ⇒ ⇒ 见 F5 |
| **C2-a（checked 必有指针形态的证据）** | §2.5 C2-a **（2026-11-12 复核订正版）** | `- [x]` 行 MUST 含 ① **证据标记**（`证据[:：]` 或 `证据（…）：`）**且** ② **可定位 token**（含 `/` 的反引号路径 ∨ 以已知扩展名结尾的文件名 ∨ 反引号内的 `--<option>`/`ctest …` 命令片段）⇒ ⇒ **撤回式条目双重豁免** ⇒ ⇒ **MUST NOT** 用主清单的**旧正则**（该正则恒不命中 ⇒ 主清单 §9.5 D1）<br>⚠️ **🔵 第四种形态**条目**同样** MUST 满足 ① 与 ②（否则「裁决注记」无载体 ⇒ ⇒ 不可复核） |
| **C2-b（未开工项不得被勾）** | §2.5 C2-b | ⚠️ **本清单无 B3/B4 分区** ⇒ ⇒ 判据 = ① **真未开工**（⬜ 档）MUST 保持 `- [ ]` 且 checked == **0** ② **条件化未触发 / 形态被替代**（🔵 档）MUST 保持 `- [ ]` 且 **MUST NOT** 写成「未完成」⇒ ⇒ **⬜ 与 🔵 两档的 checked MUST == 0**（⚠️ critic 第 1 轮：原措辞「本清单的 checked 恒为 0」与 C2-b′ 的「checked == 34」**字面互斥** ⇒ ⇒ 已改为只约束两档） |
| **C2-b′（✅ 档的勾选口径）** | —（新增） | ⚠️ **34 条 ✅ 档 MUST 勾选**（它们有物理证据）⇒ ⇒ 与 C2-b 的「未开工不得勾」**不冲突**（C2-b 只管 ⬜ / 🔵 两档）⇒ ⇒ 判据 = **本清单的 40 条中：✅ 34 条 checked == 34 ∧ 🔵 4 条 checked == 0 ∧ ⬜ 2 条 checked == 0** |
| **C2-c（当前态不许写死测试数）** | §2.5 C2-c | 逐份跑主清单 §7.2 ② 的同款命令（`awk` 排除末尾 metis 段）⇒ ⇒ 命中数 **== 0** |
| **C2-e（未勾必有归属）** | §2.5 C2-e | 每条 `- [ ]` MUST 含 `归属[:：]`（独立立项者 MUST 附**具名**落点 ⇒ ⇒ **MUST NOT** 写「§4」这种悬空指针 ⇒ ⇒ 见 F1 的具名落点集合）⇒ ⇒ 缺失数 **== 0**<br>⚠️ **🔵 档由 C2-f 管辖 · 豁免本判据**（critic 第 1 轮：否则 C2-e 与 C2-f 对同一批条目给出相反结论 ⇒ ⇒ 实施方无法判定以谁为准）⇒ ⇒ C2-e 的缺失数统计 MUST **排除** 🔵 条目 |
| **C2-f（🔵 档 MUST 有裁决注记）** | —（新增） | 每条 🔵 条目 MUST 含 `🔵 裁决注记:` ⇒ ⇒ 且 MUST 指向**真实存在**的载体 ⇒ ⇒ 缺失数 == 0 |
| **C4-a（命名规则的载体唯一）** | §4.5 C4-a | 命名规则 MUST 只落 `rtl-hierarchical-output`（规范所有者）⇒ ⇒ 本清单 **MUST NOT** 在这 2 份草案里复述 ⇒ ⇒ **证据注记 MUST NOT 复述命名规则条款正文**（唯一落地文本纪律沿用） |
| **C7-b（HARD-GATE）** | §7.2 C7-b | 2 份 `pre_create_brainstorm_check.sh` 逐份 PASS |
| **C7-c（零实现面变更）** | §7.2 C7-c | `git status --short` MUST 只含**文档类**路径 ⇒ ⇒ **零** `src/**` · `include/**` · `tests/**` · `examples/**` · `scripts/**`（⚠️ 例外 = F6 的批次表 §4 行 + F7 的登记请求文件 ⇒ ⇒ 两者**仍属 `.rddf/**`** ⇒ ⇒ 不破 C7-c） |

---

## 4. 执行顺序

| 步 | 动作 | 依赖 |
|---|---|---|
| **U0** | 本清单送 **oracle → critic**（两轮）⇒ ⇒ 取得 APPROVE | — |
| **U1** | F1 + F2（正文 + 索引表对齐 `implemented`） | U0 |
| **U1.5** | **F6**（批次表 §4 新增「产物层双测试」行）+ **F7**（architect 登记请求）⇒ ⇒ **先于或同批于** U2（否则 ⬜ / 🔵 档无归属落点 ⇒ ⇒ C2-e / C2-f 无法闭合） | U1 |
| **U2** | F3（**40 条** AC 逐条回填 ⇒ ⇒ **以 §1.3 的地图为准**）+ F5（修 `进度:` 的两处不实陈述） | U1.5 |
| **U3** | F4（🔵 档的登记式措辞）+ F8（登记「索引表状态列自动生成」为独立立项） | U2 |
| **U4** | 全量复验（§3 全判据）+ 护栏复验 + HARD-GATE 复验 + `update_test_count_anchors.sh --check` | U3 |

⇒ ⇒ **无循环依赖**（U0 → U1 → U1.5 → U2 → U3 → U4 严格单向）。
⇒ ⇒ ⚠️ **原子性**：U1–U4 MUST 作为**单一原子提交单元**入库（与主清单 §7.1 的「原子性」约束同款）。

### 4.1 U4 的护栏清单（**逐个点名** ⇒ ⇒ 区分「与本清单相关」与「例行」）

| 护栏 | 与本清单的关系 |
|---|---|
| `scripts/verify_test_counts.sh` | **相关**（AC `:163` / `:165` / `:213` 引用锚点） |
| `scripts/verify_openspec_delta.sh` | **相关**（AC `:195`–`:201` 的 spec delta） |
| `scripts/verify_arch_ledger_residuals.sh` | **相关**（F6 / F7 的登记落点） |
| `scripts/verify_public_api_visibility.sh` | **相关**（AC `:169`–`:171` 的 P4 登记） |
| `scripts/verify_roadmap_fragments.sh` | **相关**（批次表 §4 改动 ⇒ ⇒ AUTO 块守恒） |
| `scripts/update_test_count_anchors.sh --check` | **相关**（AC 里的锚点引用） |
| `scripts/verify_hierarchical_rtl.sh` | **相关**（AC `:190` / `:214` 零产物变更） |
| `scripts/verify_agents_size.sh` | 例行（不得转 FAIL） |
| `scripts/verify_flat_rtl_emit.sh` · `verify_port_naming.sh` · `verify_examples_rtl_sim.sh` | 例行全绿即可 |

---

## 5. 范围边界（本清单**不做**）

| # | 不做的事 | 理由 |
|---|---|---|
| 1 | **不实现任何 work item**（P1–P4 均已落地；缺口只有 2 条产物层测试） | 本清单只是**记录订正** |
| 2 | **不补任何测试**（含 F6 的产物层双测试） | 归 builder；本清单只**登记**该缺口（落点 = 批次表 §4） |
| 3 | **不裁决 route (c)**（P3 的「当前不做」+ 触发条件） | 归 architect + 用户（登记表行 21） |
| 4 | **不单改残差登记表 / 台账**（「层次化侧同名兄弟」待查项） | 属 **architect 域**（双向 1:1 护栏 + `docs/architecture/**`）⇒ ⇒ 本清单只**发登记请求**（F7） |
| 5 | **不补造** `.rddf/checks/decision-request-port-name-literal-parity-2026-11.md` | 裁定**早已发生**且用户已点头 ⇒ ⇒ 补造 = **自我盖章** ⇒ ⇒ 走 🔵 裁决注记 |
| 6 | **不改 `phase-6.md` 的状态格** | 那些格**已正确** |
| 7 | **不改批次表 §B1/§B2 的内容列** | 已订正 ⇒ ⇒ 只改**草案本体** + 索引表 +（**例外**）§4 **新增 2 行**（F6 / F8） |
| 8 | **不追溯改写既有验收报告** | 报告是**证据**，只被引用、不被改写 |
| 9 | **不新增/删除 ctest 条目** | 本清单是纯文档修订 ⇒ ⇒ `CTEST_TOTAL` 应保持 **263** 不变 |
| 10 | ⚠️ **不覆盖主清单的 7 份** | 两清单**互斥**：主清单管 7 份 RTL 轨 ⇒ ⇒ 本清单只管这 2 份 ⇒ ⇒ 改完后 `improvement-suggestions.md` **全表**同档 |

---

## 6. 审稿记录

| 轮 | 审查者 | 结论 | 处置 |
|---:|---|---|---|
| 1 | **oracle** | 🔴 **REJECT** · 9 条建议（**4 条必改** + 5 条其它）· 核心 = 「`进度:` 不是可信锚」+「≥5 条 AC 不可按已落地勾」+「F1/F4 模板有执行陷阱」 | **9 条全部采纳**并并入本文（v2）：① §1.2 改为「`进度:` 是线索 · 唯一真相 = 物理证据」② **新增 §1.3 逐条可满足性地图**（40 行 · 34 ✅ / 4 🔵 / 2 ⬜）③ §1.4 证据载体表**补 b0/b1 报告** ④ F1/F4 模板改为**具名落点 + 必含路径 token** ⑤ **新增 F5**（修 `进度:` 的两处不实陈述）⑥ **新增 F6/F7/F8**（批次表 §4 两行 + architect 登记请求 + 索引表自动生成的立项登记）⑦ §5 边界扩（允许修 `进度:` / 批次表 §4 / 发登记请求）⑧ §3 判据增 C2-b′ / C2-f ⑨ §4.1 护栏逐个点名 |
| 1 | **critic** | ✅ **APPROVE_WITH_CONDITIONS** · `mergeable: yes` · `scope_exceeded: false` ⇒ **0 BLOCKER** · **8 SHOULD_FIX** + 3 NITS | **8 SHOULD_FIX + 3 NITS 全部采纳并已并入本文（v3）**：① N1 证据措辞（`:274` 是注释 · 代码本体在 `:293`）② N2 指针纠正 + **形态偏差明写** ③ C2-b 措辞（二义）④ C2-e 给 🔵 豁免 ⑤ §1.5 悬空指针 ⑥ Q21/Q22 改判 ⬜（+ 归属 = F7 登记请求）⑦ Q19 依据链条补 `decision-request:184` ⑧ NIT：CHANGELOG bullet 起于 `:513` ⑨ NIT：Q7 走**共享 helper** `emit_module_text`（`:93-98`）<br>⚠️ **focus 2 的核实结论**：F6/F7 **不越界 · 不阻塞**（§5 已把「批次表 §4 行」「登记请求文件」列为 `.rddf/**` 例外 ⇒ 不破 C7-c；且主清单自身签批的执行（§9.1「§4 新增 2 行」）就是 planner 往 §4 登记 builder 活的**同款先例**；F7 的产物是**请求** ⇒ U1.5 只要求请求文件**存在**作落点指针 ⇒ 非硬依赖） |
| — | **总判定** | ✅ **通过审查 ⇒ 可进入执行**（U0 闭） | 0 BLOCKER ⇒ ⇒ 按主清单 §8.2 的口径，**SHOULD_FIX / NITS 记录后放行** ⇒ ⇒ 已全部采纳并并入本文（v3）⇒ ⇒ U1–U4 可执行 |

---

## 7. 修订执行记录（2026-11-12 · planner · U1–U4）

> ⚠️ **未 commit**（全局纪律：等用户确认）。本次改动**全部是文档**（`git status` 见 §7.3）。

### 7.1 实际改动清单

| 文件 | 改动 | 对应 |
|---|---|---|
| `.rddf/improvements/port-name-literal-parity-2026-11.md` | 正文 `- **Status**` → `✅ **implemented**`（含「P3 交付形态不同」+「P4 条件化未触发」+ 2 条待登记的**具名**落点）· `进度:` 的**不实陈述**订正 · **21 条** AC 勾选 + 证据指针 · **2 条** 🔵（Q18/Q19）· **2 条** ⬜（Q21/Q22，带 `⇒ 归属:`） | F1 / F3 / F5 |
| `.rddf/improvements/node-name-prefix-consistency-2026-11.md` | 同上（`进度:` 明写「P2 仅函数级 + 不变量已落 · **产物层两条未做**」）· **13 条** AC 勾选 + 证据指针 · **2 条** ⬜（N4/N5，带 `⇒ 归属:`） | F1 / F3 / F5 |
| `improvement-suggestions.md` | 2 行索引表状态列 → `✅ **implemented**`（**含档位词** ⇒ grep 可判） | F2 |
| `.rddf/roadmap/phase-6-rtl-productization.md` | §4 **新增 2 行**：「N2 补遗 · 产物层双测试」·「索引表状态列的自动生成」 | F6 / F8 |
| `.rddf/checks/decision-request-hierarchical-sibling-name-collision-2026-11.md` | **新建**（architect 登记请求 · 3 件请求 · 附 `hierarchy_verilog.cpp:243-249` 的**静态**机制线索 + ⚠️ 明写「未展开核实」） | F7 |

⇒ ⇒ **零** `src/**` · `include/**` · `tests/**` · `examples/**` · `scripts/**` 变更（C7-c）。

### 7.2 判据实测结果（**全绿**）

| 判据 | 结果 | 说明 |
|---|---|---|
| **C1-a** 四通道同档 | ✅ **2/2** | frontmatter `状态:` / 正文 `- **Status**` / 索引表档位词逐一对齐 |
| **C1-c** `进度:` 不与 `状态:` 矛盾 | ✅ | 两份的 `进度:` 均**明写**未落部分 + 具名登记落点 ⇒ ⇒ 与 `implemented` 不冲突 |
| **C2-a** checked 有指针形态的证据 | ✅ **34/34** | 判据 = ① 证据标记 **且** ② 可定位 token（路径 / 带扩展名的文件名 / 反引号命令片段）⇒ ⇒ **0 缺口**<br>⚠️ 执行中修掉 2 条「`证据: 同上`」（无路径 token）⇒ ⇒ 换成具名路径 |
| **C2-b / C2-b′** | ✅ | **⬜ / 🔵 两档的 checked == 0**（⬜ 4 条 + 🔵 2 条）· **✅ 档 checked == 34**（13 + 21） |
| **C2-c** 当前态不许写死测试数 | ✅ **0 / 0** | 逐份（`awk` 排除末尾 metis 段） |
| **C2-e** 未勾必有归属 | ✅ **0 缺失** | 🔵 档由 C2-f 管辖并**豁免** C2-e；4 条 ⬜ 均带 `⇒ 归属: **独立立项**（…具名落点）` |
| **C2-f** 🔵 必有裁决注记 | ✅ **0 缺失** | 2 条 🔵 均含 `🔵 裁决注记:` + 具名载体 |
| **C4-a** 命名规则载体唯一 | ✅ | 回填**只加指针**，未复述规则条款正文 |
| **C7-b** HARD-GATE | ✅ **2/2** | 逐份 PASS |
| **C7-c** 零实现面变更 | ✅ | `git status` 仅**文档类**路径 |
| **U4 护栏** | ✅ **10/10 + 锚点 5/5** | `verify_test_counts` · `verify_openspec_delta` · `verify_arch_ledger_residuals` · `verify_public_api_visibility` · `verify_roadmap_fragments` · `verify_hierarchical_rtl` · `verify_agents_size` · `verify_flat_rtl_emit` · `verify_port_naming` · `verify_examples_rtl_sim` + `update_test_count_anchors.sh --check` |
| **计数** | ✅ | `ctest -N` = **263**（**零变化** ⇒ 本清单**未**增删 ctest 条目） |
| **Q25**（既有 7 份零改动） | ✅ | mtime 对照：7 份 RTL 轨草案 = 15:24–16:03（本轮之前）· 本轮触碰的 4 份 = 17:35–17:38 ⇒ ⇒ **本轮未触碰那 7 份** |

### 7.3 提交状态

```
 M .rddf/improvements/examples-rtl-sim-coverage-2026-11.md
 M .rddf/improvements/firmware-load-chain-2026-11.md
 M .rddf/improvements/memory-timing-contract-2026-11.md
 M .rddf/improvements/node-name-prefix-consistency-2026-11.md
 M .rddf/improvements/port-name-literal-parity-2026-11.md
 M .rddf/improvements/port-naming-ratchet-2026-11.md
 M .rddf/improvements/rtl-emit-product-tooling-2026-11.md
 M .rddf/improvements/rv32i-dead-component-consolidation-2026-11.md
 M .rddf/improvements/rv32i-isa-gap-closure-2026-11.md
 M .rddf/roadmap/phase-6-next-steps.md
 M .rddf/roadmap/phase-6-rtl-productization.md
 M AGENTS.md
 M improvement-suggestions.md
?? .rddf/checks/rtl-track-correction-list-2026-11.md
?? .rddf/checks/rtl-track-followup-list-2026-11.md
?? .rddf/checks/decision-request-hierarchical-sibling-name-collision-2026-11.md
```

⇒ ⇒ **未 commit**（等用户确认）。若要提交 ⇒ ⇒ 按仓内纪律**逐路径** `git add <path>`（**禁** `git add -A`）。

### 7.4 本轮**未**做（如实登记）

| # | 未做 | 理由 |
|---|---|---|
| 1 | **未**补产物层双测试（N4 / N5） | 归 builder；已登记批次表 §4「N2 补遗 · 产物层双测试」行 |
| 2 | **未**单改残差登记表 / 台账 | architect 域（双向 1:1 护栏）⇒ ⇒ 只**发登记请求**（F7）⇒ ⇒ Q21 / Q22 保持未勾 |
| 3 | **未**补造 `decision-request-port-name-literal-parity-2026-11.md` | 裁定早已发生且用户已点头 ⇒ ⇒ 补造 = 自我盖章 ⇒ ⇒ Q18 走 🔵 裁决注记 |
| 4 | **未**实现「索引表状态列自动生成」 | 已登记批次表 §4（F8）⇒ ⇒ 归 planner 后续 |
| 5 | **未**动 B3 / B4 任何 work item | 归 builder / architect |
| 6 | **未**增删 ctest 条目 | ⇒ ⇒ `CTEST_TOTAL` 仍 **263** |

### 7.5 给下一个会话的**一句话**

⇒ ⇒ 两份清单（主 + 后继）**已把 RTL 轨 9 份草案的三通道漂移闭掉** ⇒ ⇒
**下一件事** = architect 裁决 `.rddf/checks/decision-request-hierarchical-sibling-name-collision-2026-11.md`
（3 件请求）⇒ ⇒ 然后才是 B3 的两道门（R5 = ADR-069 · R6 = 公开面前置门）。
