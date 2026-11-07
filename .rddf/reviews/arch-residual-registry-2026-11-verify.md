# 验收报告：arch-residual-registry-2026-11（残留登记表 + 派生护栏）

- 日期：**2026-11-07**（verdict `ran_at` = 2026-11-07T17:58:30Z）
- 角色：**verifier**（builder 的固定审查者 · 只验不改 ⇒ 未修改任何实现文件）
- 验收对象：worktree 分支 `openspec/arch-residual-registry-2026-11` @ `a0379f6b`
- verdict：`.rddf/state/.ac-verdict-arch-residual-registry-2026-11.json`（`verification_state = passed` · `failed_acs = []`）
- 审计日志：`.rddf/state/.ac-verification.jsonl`

## 1. 判据来源（按优先级）

| # | 来源 | 内容 |
|---|---|---|
| 1 | 用户本次明确给出的验收条件 | 选项 B 路线（登记表 = 唯一真相 + 台账派生 + 探针升 CI 护栏）；实施顺序 S1→S2→S3→S4；CI 接线不得早于台账归一化合并 |
| 2 | `openspec/changes/arch-residual-registry-2026-11/proposal.md` §Acceptance | **20 条 AC**（本报告的逐条判定对象）|
| 3 | 本次变更自己声明的行为 | 3 个 commit：S1 `b879786`（护栏 + 登记表）· S2 `75ae567`（台账归一化）· S3 `a0379f6`（CI 接线）|
| 4 | capability `ledger-residual-registry` 的 delta（5 Requirement / 9 Scenario） | 契约条款（唯一真相 / 派生 / fail-closed / CI 时序 / 判别力边界）|

> ★ **分流说明**：本仓无 `.rddf/state/iteration.json` ⇒ `rddf rdd-verify --dry-run` 返回「No eligible changes」⇒ 且 proposal 的 AC 段头是 `## Acceptance`（rdd-verifier 的提取正则只认 `## 验收标准` / `## Acceptance Criteria`）⇒ ⇒ **队列判据与段头都不命中** ⇒ ⇒ ⇒ 按 role 矩阵走**本地验收**（1–5 步）⇒ ⇒ 但**执行 rdd-verifier 的 LLM Verification Protocol**（逐条取证 + verdict JSON + 审计日志）⇒ ⇒ verdict 与本报告同源。

## 2. 逐条判定（20 条 AC · 全部 PASS）

| AC | 判定 | 证据（工具输出摘要） |
|---|---|---|
| **AC1** | ✅ pass | [ ] **AC1 双向核对双向各红一次**：台账 §7 新增一行（登记表无）⇒ 护栏红；登记表新增一行（台账无）⇒ 护栏红（⇒ 验「守恒」而非单方向）… ⇒ 证据：投毒 1（台账多一行）exit=1（期望 1）· 投毒 2（登记表多一行）exit=1（期望 1）⇒ 双向各红一次 |
| **AC2** | ✅ pass | [ ] **AC2 多重性敏感 1:1**：台账里「删一行 + 复制同 id 行」（计数守恒、集合守恒）⇒ **必须红**（⇒ 堵住「计数守恒 ⇒ 恒绿」的漏形）… ⇒ 证据：投毒 7（删一行 + 复制同 id 行 ⇒ 计数/集合守恒）exit=1（期望 1）⇒ 多重性敏感 1:1 生效 |
| **AC3** | ✅ pass | [ ] **AC3 生成句逐字前缀**：把台账行 1 的生成句改 1 个字 ⇒ 护栏红 ⇒ ⇒ 还原后复绿… ⇒ 证据：投毒 3（生成句改 1 字）exit=1（期望 1）⇒ 逐字节前缀判定生效 |
| **AC4** | ✅ pass | [ ] ★ **AC4 反向判据**：把台账行 1 的**散文**部分（生成句之后）改 1 个字 ⇒ **必须仍绿**（⇒ 证明「散文可自由改」的设计意图成立 ⇒ 且防「为让判据绿而误删真能力」）… ⇒ 证据：投毒 8（散文改 1 字）exit=0（期望 0）⇒ ★ 反向判据：散文可自由改 ⇒ 仍绿 |
| **AC5** | ✅ pass | [ ] ★ **AC5 生成句碰撞不误报**：三行同态同日（行 1 / 1b / 11）⇒ 护栏**必须绿**（★ 因终态行后缀非空 ⇒ 生成句互不相同 ⇒ 且唯一性只在**登记表内**校验 ⇒ **不**对 §7 全文… ⇒ 证据：判定 exit=0（绿）· 行 1 / 1b / 11 的生成句互不相同：✅ 已收口（2026-11-07 · P8 公共 h… / ✅ 已收口（2026-11-07 · 语料级回归归档… / ✅ 已收口（2026-11-07 · P6 + P8… ⇒ 无碰撞；生成句唯一性**只在登记表内**校验（cmd_check 内 seen_sent）⇒ 不做 §7 全文子串断言 ⇒ 首日不误报 |
| **AC6** | ✅ pass | [ ] **AC6 证据存在性**：把行 11 的 P6 归档目录改错 ⇒ 护栏红… ⇒ 证据：投毒 4（行 11 的证据路径改错）exit=1（期望 1）⇒ 证据存在性生效；exit=1（期望 1）· 命中：['FAIL id=11 闭口证据不存在：openspec/changes/archive/2026-11-07-p6-NOPE-2026-11'] · 还原后 sha256 一致且护栏复绿（exit 0） |
| **AC7** | ✅ pass | [ ] **AC7 多证据**：行 11 的**两个**归档目录**都**被校验（改任一个都红）… ⇒ 证据：exit=1（期望 1）· 命中：['FAIL id=11 闭口证据不存在：openspec/changes/archive/2026-11-07-p8-NOPE-2026-11'] · 还原后 sha256 一致且护栏复绿（exit 0）；行 11 的 `闭口证据` 含 **2** 条路径（护栏输出「证据数=2」）⇒ ★ 两个都校验（改任一个都红） |
| **AC8** | ✅ pass | [ ] ★ **AC8 候选闭口证据**：把某 `未决` 行登记一个**已存在**的路径 ⇒ 护栏红（⇒ 判据 ②' 生效 ⇒ 覆盖「已登记候选证据但台账未更新」）… ⇒ 证据：投毒 9（未决行登记已存在路径）exit=1（期望 1）⇒ 判据 ②' 生效 |
| **AC9** | ✅ pass | [ ] **AC9 fail-closed**：把 §7 标题改名 ⇒ 护栏 **exit 2**（★ 不是 exit 0 / 不是「解析 0 行 ⇒ 全绿」）… ⇒ 证据：投毒 6（§7 标题改名）exit=2（期望 2）⇒ fail-closed |
| **AC10** | ✅ pass | [ ] ★ **AC10 登记表格式非法 ⇒ exit 2**：`状态` 取枚举外值 / `日期` 非 `YYYY-MM-DD` / `闭口证据` 列格式违规 / `未决` 行缺 `复核条件` ⇒ 护栏 **exit 2… ⇒ 证据：投毒 10（状态取枚举外值）exit=2（期望 2）· 投毒 11（未决行缺复核条件）exit=2（期望 2）⇒ 登记表格式非法 ⇒ exit 2 |
| **AC11** | ✅ pass | [ ] **AC11 结算可数**：护栏输出**每行一行判定**（id / 状态 / 证据条数 / 命中位置）⇒ 输出行数 == 登记表行数… ⇒ 证据：逐行判定行数 = 15 == 登记表行数 15（末行「PASS 残留登记一致性（15 行）」） |
| **AC12** | ✅ pass | [ ] **AC12 退出码**：不一致 ⇒ `1`；缺文件 / 解析 0 行 / 登记表格式非法 ⇒ `2`（★ MUST NOT 继承探针的 `exit 0`）… ⇒ 证据：不一致（投毒 1-5/7/9）⇒ exit 1 · fail-closed（投毒 6/10/11）⇒ exit 2 · 正常判定 ⇒ exit 0 ⇒ 三档齐备 |
| **AC13** | ✅ pass | [ ] **AC13 自检 11 条投毒全红 + 还原复绿**（逐条记录投毒内容与期望）⇒ ★ 还原后 `cmp` 与投毒前**逐字相同** ⇒ ★ 自检后 `git status --porcelain docs/ar… ⇒ 证据：11 条投毒：全部符合期望 · 脚本内含 pristine fixture 逐字节未污染断言（每次投毒后比对）⇒ 还原复绿 |
| **AC14** | ✅ pass | [ ] **AC14 行 11 进登记**：护栏 / 探针输出 MUST 含行 11 ⇒ ⇒ 「OK / 陈旧」可判… ⇒ 证据：护栏输出含 `id=11`（命中=OK）· 探针 wrapper 输出同样含 `id=11` ⇒ 「OK / 陈旧」可判 |
| **AC15** | ✅ pass | [ ] **AC15 `--render` 可用**：能打印 15 行的期望状态格前缀 ⇒ ⇒ architect 可直接 copy-paste… ⇒ 证据：打印 15 行期望状态格前缀（登记 15 行）⇒ 可直接 copy-paste |
| **AC16** | ✅ pass | [ ] **AC16 CI 接线**：`grep -n 'verify_arch_ledger_residuals' .github/workflows/ci.yml` MUST 命中 **2 处**（护栏 + `--s… ⇒ 证据：命中 2 处（护栏 + --self-test）⇒ 落在 build-and-test 的 guard 段 |
| **AC17** | ✅ pass | [ ] **AC17 探针路径存活**：`bash scripts/verify_ledger_evidence.sh` 绿（⇒ 裁决请求单的 `evidence:` 指针未断）… ⇒ 证据：exit=0 · [0;32m✅ verify_ledger_evidence: PASS[0m ⇒ 裁决请求单的 evidence: 指针未断 |
| **AC18** | ✅ pass | [ ] ★ **AC18 旧散文切界规则对本批可用**：旧状态格按「**第一个 `⇒` 之前 = 旧状态前缀**」切界 ⇒ ⇒ ★ **已静态实测**：本批 15 行旧状态格**全部含 `⇒`**（含「不做 ⇒」这类短格… ⇒ 证据：15 行旧状态格含 `⇒` 的行数 = 15/15 ⇒ 切界规则对本批全部可用 |
| **AC19** | ✅ pass | [ ] ★ **AC19 归一化「信息不丢」（机械判据）**：护栏新增 **`--check-normalization`** 只读子命令 ⇒ 逐行判定：旧散文（按 AC18 切界）再按 `⇒` 切成**段集** `S_… ⇒ 证据：exit=0（期望 0）· 归一化核对：15 行 · 被删散文段 0 个 · review=.rddf/reviews/arch-residual-registry-normalization-2026-11.md ⇒ 被删段逐条登记 ⇒ 归一化不丢信息 |
| **AC20** | ✅ pass | [ ] **AC20 前言注记去副本化**：§7 前言注记 MUST **不含任何登记表的生成句**（★ 机械形式 = 逐字子串检查）⇒ ⇒ 且 MUST 指向登记表（模板见 §What Changes §5）… ⇒ 证据：命中 = 无 ⇒ 前言注记不含任何生成句 · 且注记指向登记表：是 |

> ★ **取证纪律**：每条 AC ≥ 1 条**真实工具输出**（`--self-test` 的 11 条投毒 / 护栏判定输出 / 原地投毒 + sha256 还原核对 / `grep` / `git show` 静态实测）⇒ ⇒ **0 条 AC 靠 AC 文本推断**。

### 2.1 原地投毒的还原核对（★ AC6 / AC7）

| 投毒 | 期望 | 实际 | 还原核对 |
|---|---|---|---|
| 登记表行 11 的 **P6** 归档目录改错 | exit 1 | exit 1（`FAIL id=11 闭口证据不存在：…-p6-NOPE-2026-11`）| sha256 前后一致 + 护栏复绿（exit 0）|
| 登记表行 11 的 **P8** 归档目录改错（第二个证据）| exit 1 | exit 1（`FAIL id=11 闭口证据不存在：…-p8-NOPE-2026-11`）| sha256 前后一致 + 护栏复绿（exit 0）|

## 3. 失败项与路由建议

**无失败项**（`failed_acs = []`）⇒ ⇒ 无需 `implementation_gap` / `proposal_drift` 路由 ⇒ ⇒ ★ 无失败 ⇒ **未消耗**重试额度（上限 3）。

## 4. 遗留清扫（★ 本次范围内逐项）

| 项 | 结果 |
|---|---|
| TODO / FIXME / XXX / HACK / 占位实现 | ✅ 无（唯 2 处命中是**说明性文本**：`_is_empty` 的 `—` 占位符说明 + 临时目录注释）|
| 半截实现 / 未完成分支 | ✅ 无（`cmd_check` / `cmd_render` / `cmd_check_normalization` / `selftest` 四条路径均跑通）|
| 注释掉的代码 / 被 skip 的测试 | ✅ 无（`grep 'SKIP\|skip\|# *assert\|# *TEST'` 无命中）|
| 临时调试日志（`pdb` / `breakpoint()` / `console.log`）| ✅ 无 |
| 未替换的 mock / 临时 fallback | ✅ 无（探针已改 wrapper 转发护栏 ⇒ 手工 `RESIDUAL_EVIDENCE` 清单已退役）|
| 死代码 / 未用导入 | ✅ 已清（verifier 发现 `sentence_matches` 未被引用 ⇒ 改为 `cmd_check` 走该命名函数 ⇒ commit `a0379f6`）|
| 临时文件 / 软链残留 | ✅ 无（`build/` 软链仅用于跑端口命名护栏 ⇒ **已移除**；未入库）|
| 自检零污染 | ✅ `git status --porcelain docs/architecture .rddf/checks` 干净（自检只在**合成 fixture** 上投毒）|

## 5. 已用轮次

- 本 change 的 verifier 轮次：**1 / 3**（`RDDF_VERIFIER_MAX_LOOPS = 3`）⇒ ⇒ 失败重试额度 **2 次**未动。
- 轮次内发现并当场收口的问题：**2 项**（死代码 `sentence_matches` ⇒ `a0379f6`；临时目录硬编码 `/tmp/opencode` ⇒ `72d6f0d`）⇒ ⇒ 均**已复跑全部判据**。

## 6. 判据边界（★ 显式声明，勿过度承诺）

- 本验收只判「AC 是否满足 + 状态与证据是否自洽」⇒ ⇒ **不**判「残留状态本身对不对」（★ 靠人的复核 + architect 的行 14 裁定）。
- 「未登记候选闭口证据的未决行其实已完成」**不在**判别力内（首期无未决行 ⇒ 豁免暂无触发面）。
- `--check-normalization` 的「已登记」是**子串级**判定 ⇒ ⇒ 机械判据只证明「被删段**出现在**审稿记录里」，★ **不能**证明「它被逐条登记过」（★ 已由 oracle 指出并写入审稿记录 §7）。
- CI 接线**尚未在真实 CI 上跑过** ⇒ ⇒ 本报告的 CI 证据是**本地等价**（同一命令 + 同一退出码）⇒ ⇒ ★ 首次真实 CI 运行前，CI 红是**可能**的（★ 但台账已归一化 ⇒ 判定必绿）。

## 7. 归档前待办

- [ ] `openspec archive arch-residual-registry-2026-11`（★ **禁** `--skip-specs` ⇒ capability `ledger-residual-registry` 的 delta MUST 合入主 spec）
- [ ] worktree 分支合回 main（★ `check_worktree_commits` 要求分支有新提交 ⇒ 现有 4 个）
- [ ] push（★ 主仓 14 个未 push commit + 分支 4 个 ⇒ **等用户确认**）
