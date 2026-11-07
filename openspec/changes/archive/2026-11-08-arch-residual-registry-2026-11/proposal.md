# arch-residual-registry-2026-11

## Why

### A.1 病根：「状态」被复制到 **4 处**，且**互不校验**

台账 §7 的状态格是**散文**，不是结构化字段 ⇒ 同一个「残留是否已闭口」的事实被写在 4 个地方：

| # | 载体 | 形态 | 谁维护 |
|---|---|---|---|
| 1 | 台账 §7 状态格（`docs/architecture/2026-11-02-hierarchical-rtl-output.md`） | 散文 | architect（**手工**） |
| 2 | §7 **前言的修订注记**（同文件 `:273-275`）—— ★ 逐字复述「残留 1/1b/5 改为已收口 · 11 同步更新」 | 散文（**状态的第 4 份副本**） | architect（**手工**） |
| 3 | 探针的 `RESIDUAL_EVIDENCE`（`scripts/probe_arch_ledger_residuals.sh:36-41`） | 手工清单 | builder（**手工**） |
| 4 | 裁决请求单 / 审稿记录（`.rddf/checks/**` · `.rddf/reviews/**`） | 散文 | 各角色（**手工**） |

⇒ 四处**没有任何交叉校验** ⇒ 任何一处漏改 = **静默漂移**。

### A.2 当前 HEAD 上**可复算**的事实（★ 结论不得先于测量）

| # | 事实 | 复算方式 |
|---|---|---|
| 1 | 探针**只登记 3 行**（`1` / `1b` / `5`）⇒ 台账 §7 的 **15 行里有 12 行无任何自动检出** | `grep -A6 'RESIDUAL_EVIDENCE=(' scripts/probe_arch_ledger_residuals.sh` |
| 2 | **行 11 未登记** ⇒ 它陈旧时探针**恒绿** ⇒ 「状态漂移」在这行**不可检出** | 同上 + `bash scripts/probe_arch_ledger_residuals.sh`（输出仅 `OK 1 / OK 1b / OK 5`） |
| 3 | 探针**恒 `exit 0`**（`scripts/probe_arch_ledger_residuals.sh:104`）⇒ 即使检出陈旧也**不阻断**；且探针自己打印「**没有任何护栏**会发现这件事」 | 同上（`echo $?` ⇒ 0，即使输出 STALE） |
| 4 | 探针的 STALE 词表 = `已收口 / 已解决 / 已关闭 / 已落地 / 已已完成`（`:86-90`）⇒ **不认** `已裁决 / 不做 / 已知 / 已登记` ⇒ ★ **护栏若扩展到 15 行 ⇒ 直接复用该词表会把这 4 类行全误报 STALE** | `sed -n '86,90p' scripts/probe_arch_ledger_residuals.sh` |
| 5 | ★ 探针的**第二个分支**（`:86-95`）「**证据存在但状态未闭口** ⇒ STALE」= 对「未决但现实已归档」的**部分覆盖**（覆盖 3 行）⇒ ★ 这是**有价值的判定**（它抓的正是「台账漏更新」）⇒ ⇒ v3 **保留**该判定，但**改为从登记表派生**（不再依赖手工清单 + 散文词表） | 同 #4 |
| 6 | 探针支持传台账副本路径（`LEDGER="${1:-...}"`）⇒ **自检可在副本上投毒**，不污染工作树 | 同文件 `:26` |
| 7 | 仓内自检先例 = **合成仓库 fixture + 每种缺陷一个变异**（`scripts/verify_roadmap_fragments.sh:74-96`）⇒ ★ 比「真台账副本投毒」更强（同时钉住解析格式，且**不碰** architect 域文件） | 同上 |
| 8 | 仓内「自检副本 + `trap EXIT INT TERM` + 逐条投毒 + `cmp -s` 逐字比对」先例 | `scripts/verify_input_port_required_registry.sh:88-118` |

### A.3 漂移的**历史实例**（★ 已被 2026-11-07 那轮人工同步修掉 ⇒ 作为动机保留）

| 实例 | 事实 | 发现方式 |
|---|---|---|
| 行 1 / 1b / 5 | 对应 change 已归档（`2026-11-07-p8-…` / `2026-11-07-hierarchical-corpus-regression-2026-11` / `2026-11-06-port-naming-contract-guard-2026-11`）⇒ 台账状态格仍写「⚠️ 未决」 | **人工**（裁决请求单的探针） |
| 行 11 | 仍写「待 planner 立项」⇒ 而 **P6 / P8 均已立项并归档** | **人工**（oracle 2026-11-07 审稿：`.rddf/reviews/hierarchical-ledger-residuals-sync-2026-11.md` §3 finding 2） |
| ★ **§11.11.4 行 3**（★ metis 发现 · **本轮未处理 ⇒ 范围外**） | 同文件 `:810` 仍写「⇒ ⇒ **待 planner 立项**；**P6** 仍待 planner 立项」⇒ ⇒ **同一台账的第二张「留待后续」表**同样陈旧 | **人工**（metis 2026-11-07） |

⇒ **关键结论**：行 11 = 「机器完全失明 + 靠人工兜住」的**复合失效**；§11.11.4 行 3 = 「**连人工都只扫了 §7**」的失效 ⇒ ⇒ 两者共同证明：**治理必须覆盖「同一台账的其它残留表」** ⇒ ⇒ 这是登记表 MUST 带 `section` 列的直接理由（见 §What Changes §1）。

### A.4 本仓已有「登记表 + 双向核对」的**可复用先例**（⇒ 不是发明新范式）

| 先例 | 形态 | 复用点 |
|---|---|---|
| `scripts/verify_input_port_required_registry.sh` | 登记表 `.rddf/checks/input-port-required-registry.md` ↔ 端口类别**双向校验** | 「登记表是唯一真相 + 双向核对」的**既有形态** + **自检副本投毒**形态 |
| `scripts/verify_hierarchical_p8.sh` 的 **R4②** | 活账 `.rddf/checks/public-api-surface.md` ↔ 实际公共头**双向核对** | 同上 + 「活账」文件形态 |
| `scripts/verify_hier_corpus_coverage.sh` | 登记 ↔ 用例 ↔ 语料**三向双向核对** + `--self-test` 投毒 | 同上 + **投毒自检**纪律 |
| `scripts/verify_roadmap_fragments.sh` | **渲染 + 整段逐字比对**（CI 注释：`render+compare, NOT two-way field parsing ⇒ one parser ⇒ less false-positive surface`） | ⇒ **`--render` 子命令**与「单一解析器」形态先例 |
| `.github/workflows/ci.yml` 的 guard 段（`:229-303`） | 「跑护栏 + 跑 `--self-test`」**两步**形态 | ⇒ **CI 接线形态先例** |

### A.5 治理依据

- 裁决请求单 `.rddf/checks/decision-request-hierarchical-ledger-residuals-2026-11.md` §2 **选项 B** ⇒ 用户 2026-11-07 拍板「**立项（治本，推荐）**」。
- 该请求单 §3 建议 2 的**关键约束**：「**登记表是唯一真相，台账是它的渲染** ⇒ 否则就是『用一份机器可核的东西换另一份要手工同步的东西』⇒ ⇒ 不解决根因」。
- 台账 §7 行 11 状态格已登记该缺口（`.rddf/reviews/hierarchical-ledger-residuals-sync-2026-11.md` §4 = 本稿的**授权来源**）。

## What Changes

**In Scope**:

- - **In Scope**：
  1. 新增**登记表** `.rddf/checks/arch-residual-registry.md`（**唯一真相**；含 YAML front-matter `evidence:` ⇒ 过 `verify_ledger_evidence.sh` 的台账核验；★ front-matter 的 `evidence:` **≠** 表内的「闭口证据」列 —— 见 §What Changes §1；★ **取值定为** `probe:scripts/probe_arch_ledger_residuals.sh`（★ 该路径**已存在** ⇒ 登记表可独立入库；且探针 wrapper 化**不改路径** ⇒ 指针稳定））
  2. 新增**护栏** `scripts/verify_arch_ledger_residuals.sh`（表解析 MUST 走 `scripts/lib/` 下的**独立分析器** ⇒ 与 `scripts/lib/ledger_evidence.py` 同域；★ 禁止在 shell 里做表解析 ⇒ 否则 fail-closed 语义不统一）
  3. 探针 `scripts/probe_arch_ledger_residuals.sh` **MUST 继续存在**（可做 wrapper 转发到护栏的只读子命令）⇒ ★ 它是裁决请求单 `:3` 的 `evidence:` 指针 ⇒ **改名 ⇒ 证据链断**；⇒ ★ 其「证据存在但状态未闭口」判定**保留**，但**改为从登记表派生**
  4. 护栏**进 CI**：接在 `build-and-test` 的 guard 段（`ci.yml` 既有形态）= **两步**（护栏 + `--self-test`）⇒ 无构建依赖 ⇒ ★ **时序硬约束**：护栏进 CI MUST **不早于**台账归一化合并（见 §What Changes §3 判据 ⑧）
  5. 台账 §7 **15 行状态格的归一化**（MUST 以登记表的**生成状态句**开头）⇒ ★ **措辞由 architect 提供** ⇒ builder MUST NOT 单方面改 architect 域文档 ⇒ **跨角色协作点**
  6. 台账 §7 **前言的修订注记去副本化**（★ 消除 A.1 的第 4 份副本 ⇒ 注记改为「本次同步了哪些行 id」+「状态以登记表为准」，**不再逐字复述状态**）
- **Out of Scope**：
  - **不改**任何 ADR 的决策正文；**不迁移** ADR-048 / ADR-057 等 ADR 内的「已知残留」段（★ **决策载体 ≠ 主题文档** ⇒ 两者的「残留登记」不同源 ⇒ 混在一张表里会污染 ADR 的权威性）
  - **不迁移** `docs/usage_guide/**` / `docs/developer_guide/**` 里的残留描述（★ 那类描述是**用法说明**（「已知局限」），不是「未闭口的工作项」⇒ 语义不同 ⇒ 强行纳入会污染登记表的「状态」取值域）
  - **不覆盖**本台账 §7 之外的「留待后续」表（§11.4 · §11.11.4）⇒ ★ 但 §11.11.4 行 3 的陈旧**如实登记**在 §已知边界 #7 ⇒ 交 architect 另行处理
  - **不做**「脚本整体重写台账状态格」（方案 C）⇒ ★ **完整否决理由**（oracle 补全）：① 状态格散文（判别力上限 / backlog 登记 / 收口记录路径）**必须与状态同屏供读者阅读**（行 1b 的判别力上限写在状态格正是这个用法）⇒ 模板化即丢信息；② 「builder 写 architect 域文档」的边界问题可通过「architect 手动跑 `--apply`」规避 ⇒ ⇒ **真正的承重理由是 ①，不是 ②**
  - **不采用**第四种方案「**散文移列**」（状态格只放生成句 ⇒ 散文移到描述列 / 新增备注列）⇒ ★ **误判面确实优于本方案**（单一解析器 · 无裸 `|` 约束 · 无生成句碰撞）⇒ 但**一次性搬动 15 行散文**工作量大 + 「收口记录 / 判别力边界」本质是**状态伴生信息**，塞进描述列造成语义混杂 + 描述列已极宽 ⇒ ⇒ **登记为「考虑过」**
  - **不裁定** capability 的最终归属（⇒ 归 openspec change 阶段；本稿只登记候选 + 判据 + 仓内先例证据）
  - **不迁**其它台账（★ 本仓 68 个台账中，含「已知残留」段的仅 6 个 ⇒ 首期只迁**主题文档型**的那 1 个 ⇒ 机制按「多台账可扩展」设计，迁移分批做）
  - **不判**「**未登记候选闭口证据**的未决行其实已完成」（★ 机制边界 ⇒ 见 §已知边界 #6 —— 这是 metis 指出的「更彻底诉求」被漏写的地方）

### 关键场景

> ★ proposal 生成器会把本段并入生成稿的 `## What Changes` ⇒ ⇒ 故写成**可判定的场景**（GIVEN / WHEN / THEN）。

- **S1 · 台账状态与现实脱节**
  - GIVEN 台账 §7 某行写着「⚠️ 未决」而对应 change **已归档**
  - WHEN 开发者 / 评审者读台账
  - THEN 他会以为该工作仍待做 ⇒ ⇒ **本 change 之后**：登记表该行 `状态` = `已收口` + `闭口证据` 指向归档目录 ⇒ ⇒ 证据路径不存在 ⇒ 判据 ② 红 ⇒ ⇒ 台账与登记表**不可能长期不一致**
- **S2 · 状态格漏更新（本轮的原始形态）**
  - GIVEN 行 11 的状态格仍写「待 planner 立项」而 **P6 / P8 均已归档**
  - WHEN 护栏运行
  - THEN 登记表（`已收口` + 两个归档目录）生成的生成句与台账前缀不符 ⇒ 判据 ③ 红 ⇒ ⇒ **漂移在 CI 里可见**
- **S3 · 登记表加了行但旧的探针清单没覆盖**
  - GIVEN 登记表新增一行 `未决`，并登记一个**已存在**的候选闭口证据
  - WHEN 护栏运行
  - THEN 判据 ②' 红（「该行应转 `已收口`」）⇒ ⇒ 即使这条行**不在**旧 `RESIDUAL_EVIDENCE` 清单里，也**不会漏**
- **S4 · 散文被误伤**
  - GIVEN architect 在状态格的**散文**里改 1 个字
  - WHEN 护栏运行
  - THEN **必须仍绿**（判据 ③ 只比前缀）⇒ ⇒ 「散文可自由改」的设计意图成立
- **S5 · 归一化丢了信息**
  - GIVEN architect 归一化时误删了状态格散文里的一整段
  - WHEN 跑 `--check-normalization`
  - THEN 该段落在 `S_old \ S_new` 里 ⇒ 若**未**在 architect review 记录逐条登记 ⇒ 护栏红 ⇒ ⇒ 「只增不删」由机器守
- **S6 · 工具缺失**
  - GIVEN 缺 `python3` 或缺分析器
  - WHEN 护栏运行
  - THEN **exit 2**（fail-closed）⇒ ⇒ 绝不「静默全绿」

**Out of Scope**:

- **不改**任何 ADR 的决策正文；**不迁移** ADR-048 / ADR-057 等 ADR 内的「已知残留」段（★ **决策载体 ≠ 主题文档** ⇒ 两者的「残留登记」不同源 ⇒ 混在一张表里会污染 ADR 的权威性）
- **不迁移** `docs/usage_guide/**` / `docs/developer_guide/**` 里的残留描述（★ 那类描述是**用法说明**（「已知局限」），不是「未闭口的工作项」⇒ 语义不同 ⇒ 强行纳入会污染登记表的「状态」取值域）
- **不覆盖**本台账 §7 之外的「留待后续」表（§11.4 · §11.11.4）⇒ ★ 但 §11.11.4 行 3 的陈旧**如实登记**在 §已知边界 #7 ⇒ 交 architect 另行处理
- **不做**「脚本整体重写台账状态格」（方案 C）⇒ ★ **完整否决理由**（oracle 补全）：① 状态格散文（判别力上限 / backlog 登记 / 收口记录路径）**必须与状态同屏供读者阅读**（行 1b 的判别力上限写在状态格正是这个用法）⇒ 模板化即丢信息；② 「builder 写 architect 域文档」的边界问题可通过「architect 手动跑 `--apply`」规避 ⇒ ⇒ **真正的承重理由是 ①，不是 ②**
- **不采用**第四种方案「**散文移列**」（状态格只放生成句 ⇒ 散文移到描述列 / 新增备注列）⇒ ★ **误判面确实优于本方案**（单一解析器 · 无裸 `|` 约束 · 无生成句碰撞）⇒ 但**一次性搬动 15 行散文**工作量大 + 「收口记录 / 判别力边界」本质是**状态伴生信息**，塞进描述列造成语义混杂 + 描述列已极宽 ⇒ ⇒ **登记为「考虑过」**
- **不裁定** capability 的最终归属（⇒ 归 openspec change 阶段；本稿只登记候选 + 判据 + 仓内先例证据）
- **不迁**其它台账（★ 本仓 68 个台账中，含「已知残留」段的仅 6 个 ⇒ 首期只迁**主题文档型**的那 1 个 ⇒ 机制按「多台账可扩展」设计，迁移分批做）
- **不判**「**未登记候选闭口证据**的未决行其实已完成」（★ 机制边界 ⇒ 见 §已知边界 #6 —— 这是 metis 指出的「更彻底诉求」被漏写的地方）

## Capabilities

- MUST 存在一份**唯一真相**登记表 `.rddf/checks/arch-residual-registry.md`，列 = `台账 / section / id / 状态 / 日期 / 后缀 / 闭口证据 / 复核条件 / 归属`
- MUST 台账 §7 每行状态格以登记表**确定性生成的生成句**开头（★ **逐字节前缀**比较 · 无任何归一化）
- MUST 护栏做**双向 + 多重性敏感 1:1** 核对（键 = 台账 + section + id）
- MUST 护栏在**证据路径不存在**时红（★ 仅对「需证据的态」）
- MUST 护栏在**未决行登记的候选闭口证据已存在**时红
- MUST 护栏 **fail-closed**：缺文件 / 解析 0 行 / 登记表格式非法 ⇒ **exit 2**
- MUST 护栏提供 `--render`（期望前缀）与 `--check-normalization`（归一化判据）两个**只读**子命令
- MUST `--self-test` 用**合成 fixture**（★ 不碰 architect 域文件）⇒ 11 条投毒全红 + 还原后**逐字相同**
- MUST 护栏**进 CI**（guard 段 +2 步）⇒ 且**不早于**台账归一化合并
- MUST 探针 `scripts/probe_arch_ledger_residuals.sh` **路径保留**（★ 它是裁决请求单的 `evidence:` 指针）
- MUST 台账归一化**只增不删**；删除**只允许**「因本 change 而变假」的散文 ⇒ 且**逐条登记**在 architect review 记录
- MUST §7 前言注记**不含任何生成句**且**指向登记表**

## Impact

- MUST NOT 继承探针的 `exit 0` 语义（★ 否则 CI 永远不红）
- MUST NOT 依赖**散文词表**做状态判定（★ 探针词表已证会误报 `已裁决 / 不做 / 已知 / 已登记`）
- MUST NOT 对台账 §7 全文做「生成句唯一命中」子串断言（★ 生成句碰撞 ⇒ 首日必误报）
- MUST NOT 在台账归一化合并**之前**把护栏接进 CI（★ 否则 architect 落笔前全仓恒红）
- MUST NOT 在 `--self-test` 里**原地**投毒 `docs/architecture/**`（★ 会污染 architect 域文件）
- MUST NOT 用 shell 做表格解析（★ fail-closed 语义 MUST 统一 ⇒ 走 `scripts/lib/` 独立分析器）
- MUST NOT 由 builder 单方面改台账措辞（★ 归 architect）
- MUST NOT 把「未登记候选闭口证据的未决行其实已完成」算进本契约的判别力（★ §已知边界 #6）

## Acceptance

> ★ **编号与 §What Changes §3 的「AC → 投毒 / 断言映射」表一一对应** ⇒ ⇒ **无对应投毒或断言的 AC = 退化点** ⇒ MUST 补齐。

- [ ] **AC1 双向核对双向各红一次**：台账 §7 新增一行（登记表无）⇒ 护栏红；登记表新增一行（台账无）⇒ 护栏红（⇒ 验「守恒」而非单方向）
- [ ] **AC2 多重性敏感 1:1**：台账里「删一行 + 复制同 id 行」（计数守恒、集合守恒）⇒ **必须红**（⇒ 堵住「计数守恒 ⇒ 恒绿」的漏形）
- [ ] **AC3 生成句逐字前缀**：把台账行 1 的生成句改 1 个字 ⇒ 护栏红 ⇒ ⇒ 还原后复绿
- [ ] ★ **AC4 反向判据**：把台账行 1 的**散文**部分（生成句之后）改 1 个字 ⇒ **必须仍绿**（⇒ 证明「散文可自由改」的设计意图成立 ⇒ 且防「为让判据绿而误删真能力」）
- [ ] ★ **AC5 生成句碰撞不误报**：三行同态同日（行 1 / 1b / 11）⇒ 护栏**必须绿**（★ 因终态行后缀非空 ⇒ 生成句互不相同 ⇒ 且唯一性只在**登记表内**校验 ⇒ **不**对 §7 全文做子串搜索）
- [ ] **AC6 证据存在性**：把行 11 的 P6 归档目录改错 ⇒ 护栏红
- [ ] **AC7 多证据**：行 11 的**两个**归档目录**都**被校验（改任一个都红）
- [ ] ★ **AC8 候选闭口证据**：把某 `未决` 行登记一个**已存在**的路径 ⇒ 护栏红（⇒ 判据 ②' 生效 ⇒ 覆盖「已登记候选证据但台账未更新」）
- [ ] **AC9 fail-closed**：把 §7 标题改名 ⇒ 护栏 **exit 2**（★ 不是 exit 0 / 不是「解析 0 行 ⇒ 全绿」）
- [ ] ★ **AC10 登记表格式非法 ⇒ exit 2**：`状态` 取枚举外值 / `日期` 非 `YYYY-MM-DD` / `闭口证据` 列格式违规 / `未决` 行缺 `复核条件` ⇒ 护栏 **exit 2**（★ fail-closed）
- [ ] **AC11 结算可数**：护栏输出**每行一行判定**（id / 状态 / 证据条数 / 命中位置）⇒ 输出行数 == 登记表行数
- [ ] **AC12 退出码**：不一致 ⇒ `1`；缺文件 / 解析 0 行 / 登记表格式非法 ⇒ `2`（★ MUST NOT 继承探针的 `exit 0`）
- [ ] **AC13 自检 11 条投毒全红 + 还原复绿**（逐条记录投毒内容与期望）⇒ ★ 还原后 `cmp` 与投毒前**逐字相同** ⇒ ★ 自检后 `git status --porcelain docs/architecture .rddf/checks` **为空** ⇒ ★ **优先用合成 fixture**（不碰 architect 域文件）
- [ ] **AC14 行 11 进登记**：护栏 / 探针输出 MUST 含行 11 ⇒ ⇒ 「OK / 陈旧」可判
- [ ] **AC15 `--render` 可用**：能打印 15 行的期望状态格前缀 ⇒ ⇒ architect 可直接 copy-paste
- [ ] **AC16 CI 接线**：`grep -n 'verify_arch_ledger_residuals' .github/workflows/ci.yml` MUST 命中 **2 处**（护栏 + `--self-test`）⇒ ⇒ ★ 且 **CI 接线 MUST 不早于台账归一化合并**（⇒ 由 PR 顺序保证 ⇒ 实施顺序见 §What Changes §7）
- [ ] **AC17 探针路径存活**：`bash scripts/verify_ledger_evidence.sh` 绿（⇒ 裁决请求单的 `evidence:` 指针未断）
- [ ] ★ **AC18 旧散文切界规则对本批可用**：旧状态格按「**第一个 `⇒` 之前 = 旧状态前缀**」切界 ⇒ ⇒ ★ **已静态实测**：本批 15 行旧状态格**全部含 `⇒`**（含「不做 ⇒」这类短格）⇒ ⇒ ★ **下一个台账纳入时 MUST 重验**（若某行不含 `⇒` ⇒ 该行须单独定义切界，否则归一化判据不可判定）
- [ ] ★ **AC19 归一化「信息不丢」（机械判据）**：护栏新增 **`--check-normalization`** 只读子命令 ⇒ 逐行判定：旧散文（按 AC18 切界）再按 `⇒` 切成**段集** `S_old`；新状态格同样切成 `S_new` ⇒ ⇒ 判据 = 「**`S_old \ S_new` 的每个段 MUST 在 architect review 记录里逐条登记**」⇒ ⇒ ★ 且删除**只允许**「因本 change 而变假」的散文（典型 = 行 11 的「本行不在 probe 登记内 ⇒ 归 builder 补登记」）⇒ ⇒ architect 复核作为**第二种**证据（不是唯一证据）
- [ ] **AC20 前言注记去副本化**：§7 前言注记 MUST **不含任何登记表的生成句**（★ 机械形式 = 逐字子串检查）⇒ ⇒ 且 MUST 指向登记表（模板见 §What Changes §5）

## References

- **裁决请求单**（选项 B 的来源）：`.rddf/checks/decision-request-hierarchical-ledger-residuals-2026-11.md`
- **审稿记录**（台账收口 + 行 11 缺口登记）：`.rddf/reviews/hierarchical-ledger-residuals-sync-2026-11.md`
- **台账**：`docs/architecture/2026-11-02-hierarchical-rtl-output.md`（§7 残留表 · §11.1 **A4** · §11.2 形态约束 · §11.11.4 行 3 = 已知陈旧 · 范围外）
- **仓内先例**：`scripts/verify_input_port_required_registry.sh`（登记表双向校验 + 副本投毒自检）· `scripts/verify_hierarchical_p8.sh` **R4②**（活账双向核对）· `scripts/verify_hier_corpus_coverage.sh`（三向双向核对 + 投毒自检）· `scripts/verify_roadmap_fragments.sh`（**render + 整段逐字比对** + **合成仓库 fixture 自检**）· `.github/workflows/ci.yml` guard 段（`:229-303`「跑护栏 + 跑 `--self-test`」两步形态）
- **相关 ADR**：ADR-058（边界与等价性）· ADR-059（归属与命名）· ADR-060（端口策略与复用）· ADR-061（层级粒度）· ADR-062（单元接口类型）

