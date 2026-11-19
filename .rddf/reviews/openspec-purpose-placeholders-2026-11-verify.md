# 验收报告：openspec-purpose-placeholders-2026-11

**日期**: 2026-11-19
**验收对象**: 载体型 change `openspec/changes/openspec-purpose-placeholders-2026-11/`
**权威提案层**: `.rddf/improvements/openspec-purpose-placeholders-2026-11.md`
**审查报告（依据）**: `.rddf/reviews/openspec-purpose-placeholders-review-2026-11.md`
**验收角色**: verifier（builder 角色内建的固定审查者）

---

## 0. 分流判定（按 verifier 产出契约）

| 条件 | 判据 | 本次结果 |
|---|---|---|
| ① 有 openspec change 且 proposal 含 AC 段 | `## Acceptance` / `## 验收标准` | ✅ 有（`## Acceptance` · AC-0…AC-10） |
| ② P3 archive gate 已触发 | `iteration.json` 的 `status ∈ {in_worktree, completed}` + `tasks_done == tasks_total > 0` + 未归档 | ❌ **未触发**（`status = proposed` · `tasks 0/24` · 未归档） |
| ③ `rdd-verifier` skill 可加载 | — | （未走到） |

⇒ **权威判定**：`rddf rdd-verify --dry-run` ⇒ 「**No eligible changes to verify (empty queue).**」
⇒ ⇒ **未命中 skill 路径** ⇒ ⇒ 走**本地验收**（契约 1–5 步）⇒ 本报告即其产物。
⇒ ⇒ 另注：`.rddf/state/verifier/` 的 legacy 状态迁移提示（dry-run 输出）**不属于本批范围**
⇒ ⇒ 未执行迁移（**不**在本批顺手动 runtime 状态）。

---

## 1. 判据来源（按契约的优先级链）

1. **用户本次明确给出的验收条件**：「批准，请实施」⇒ 范围 = 批 A + 护栏 + 批 B/C 登记
2. **变更自己声明的 AC** = **权威源 = improvement 的 `## Acceptance`（AC-0…AC-10）**
   （已登记于 `tasks.md` 的 T0.4 ⇒ 「AC 的权威源 = improvement」）
3. 本次 commit 声明的行为 ⇒ 见 §3 的逐条证据
4. ADR 的 Verification 章 ⇒ 不适用（本批无架构决策）

---

## 2. 验证顺序（契约 1–5 步 · 早失败早停）

| 步 | 内容 | 本批适用性 | 结果 |
|---|---|---|---|
| ① 编译 / 静态检查 | `python3 -m py_compile scripts/lib/openspec_purpose_guard.py`；`bash -n scripts/verify_openspec_purpose.sh`；`openspec validate <change> --strict`；既有护栏链 | 适用（新增 2 个脚本） | ✅ 全绿 |
| ② 单元测试 | 本批**零** `src/` `include/` `tests/` 改动 ⇒ ⇒ 既有 ctest 不受影响 | 不适用（已证明零改动） | ✅ `git diff` 证明 |
| ③ 集成 / 端到端 | 「归档时写出占位符 ⇒ 护栏转红」这条链 ⇒ 以 `--self-test` 的**投毒注入**端到端模拟 | 适用 | ✅ 4/4 投毒转红 + 还原复绿 |
| ④ 边界与错误路径 | fail-closed 三条 + 「零 spec ⇒ 判据失效」 | 适用 | ✅ 见 §3 的 AC-8 证据 |
| ⑤ 遗留清扫复核 | TODO / 半截实现 / 注释掉的代码 / 临时日志 / 被 skip 的测试 / 未替换的 mock / 临时 fallback | 适用 | ✅ 见 §4 |

---

## 3. 逐条判定（每条 1+ 条工具证据）

**验收器**：`/tmp/opencode/purpose/ac_runner.py`（一次性；判据的可执行形态与 §3 表格一一对应）

| AC | 判定 | 证据 |
|---|---|---|
| **AC-0**（否决性）仓级 strict = 27 passed / 24 failed / 51 items | ✅ | `openspec validate --specs --strict` ⇒ `Totals: 27 passed, 24 failed (51 items)`（**不是** 31/20 ⇒ 只有 4 个 spec 翻绿 ⇒ 与 AC-0 预期一致） |
| **AC-1** 8 个 target 的 Purpose 占位符 warning 清零（per-item） | ✅ | 逐个 `openspec validate <cap> --type spec --strict` ⇒ 「Purpose section is still a placeholder」命中 **0** 个 target |
| **AC-2** 每个 Purpose 段 ≥120 字符 ∧ 无 TBD/TODO/placeholder/created by archiving | ✅ | 不合规 **0**；长度区间 **554–837** 字符（8 段） |
| **AC-3′** 非「标题拼接」：无标题全文 ∧ 任一行与标题的 LCS/标题长 < 0.85 | ✅ | 违规 **0**；最接近的 3 条 = `0.50『Verilator』` / `0.50『Verilator』` / `0.41『verilator』`（均为术语，非标题复述） |
| **AC-4** diff 只在 `## Purpose` 块内 | ✅ | 逐段比对 HEAD vs 工作树 ⇒ 8 个 spec 的**非 Purpose 段全部逐字节相同**；净增行 = 0 |
| **AC-5** Purpose 内的非 glob 路径 MUST 存在 | ✅ | 不存在 **0** |
| **AC-6** `openspec validate --specs`（非 strict）exit 0 + `verify_openspec_delta.sh` 绿 + 本 change valid | ✅ | rc=0 / rc=0 / rc=0 |
| **AC-7** 根 AGENTS.md 快照已订正为实测值 + 含「Purpose 无 change 通道 ⇒ 归档后手工写」 | ✅ | `27 passed, 24 failed (51 items)` 在位；`Purpose 的修改通道` + `归档后手工写` 在位；旧快照 `21 passed / 23 failed (44 items)` 残留计数 = **0** |
| **AC-8** 护栏存在 + fail-closed + 双向 + `--self-test` 4/4 + 已进 CI + 可执行位 | ✅ | `check rc=0`；`--self-test rc=0`（4/4）；`ci.yml` 引用 **2** 处；`git ls-files -s` mode = **100755**；**fail-closed 实测**：缺登记表 ⇒ exit **2**、缺逻辑载体 ⇒ exit **2**、还原 ⇒ exit **0** |
| **AC-9** 19 条债 + 7 条观察已登记（逐条 owner + open 行 trigger）+ 既有台账双向仍绿 | ✅ | `debt=19` `obs=7`；缺字段 **0**；`verify_arch_ledger_residuals.sh` rc=0 |
| **AC-10** 零 `### Requirement` 改动（AC-4 的独立复核） | ✅ | `git diff -U0` 中 `^[+-]### Requirement` 命中 **0** 处 |

**验收器汇总**：`合计 11 条 AC，通过 11 条，未过 0 条`

### 3.1 护栏判别力自证（`--self-test` 4/4）

| 投毒 | 期望 | 实测 |
|---|---|---|
| ① 把一个**实质** Purpose 换回占位符 | R1 转红 | ✅ |
| ② 登记表登记一个**不存在**的 capability | R2 转红 | ✅ |
| ③ 一条 open 债的 `trigger` 清空 | R2 转红 | ✅ |
| ④ 登记表 id 重复 | R3 转红 | ✅ |
| ⑤ 还原后复绿 | exit 0 | ✅ |

> ⚠️ **自检抓到过一次「判据假阳性」的根因**：投毒脚本的 `^` 缺 `re.M` ⇒ 投毒**根本没生效**
> ⇒ ⇒ 自检会误报「判据无判别力」⇒ ⇒ **这就是自检存在的意义**（否则会把「投毒没生效」误读成
> 「判据失效」，进而错改判据）。已修（加 `re.M`）⇒ 4/4 真转红。

### 3.2 既有护栏链未受影响（逐条实跑）

| 护栏 | 结果 |
|---|---|
| `scripts/verify_agents_size.sh`（AGENTS.md 变长 ⇒ 体积上限） | ✅ PASS |
| `scripts/verify_guard_reachability.sh`（新护栏 ⇒ 引用集合增生 ⇒ 登记表须重生成） | ✅ PASS（53 个被引用脚本） |
| `scripts/verify_openspec_delta.sh` | ✅ PASS |
| `scripts/verify_arch_ledger_residuals.sh` | ✅ PASS |
| `scripts/verify_docs_links.sh` | ✅ PASS |
| `scripts/verify_openspec_purpose.sh` | ✅ PASS |
| `scripts/verify_roadmap_fragments.sh` | ✅ PASS |
| 上述 4 个护栏的 `--self-test`（guard_reachability / arch_ledger_residuals / docs_links / openspec_purpose） | ✅ 全 PASS |

---

## 4. 遗留清扫（契约第 5 步）

| 项 | 结果 | 证据 |
|---|---|---|
| TODO / FIXME / XXX / HACK / DEBUG / 「待补」 | **0** | grep 本批改动的 11 个文件 ⇒ 无命中（唯一 `TODO` 字样是护栏的**检测模式**本身） |
| 注释掉的代码 / 被 skip 的测试 / 未替换的 mock | **0** | grep 新增 2 个脚本 ⇒ 无命中 |
| 临时调试日志 | **0** | 新增脚本无 `printf`/`print` 调试残留（输出仅限判据报告） |
| 临时文件 / fallback 分支 | **0** | `git status` 无越界未跟踪文件；`__pycache__` 已被 gitignore；self-test 的备份由 `trap` 清理 ⇒ 还原后 `git status` 无残迹 |
| 仓内 AGENTS.md 旧快照残留 | **0** | grep `21 passed, 23 failed (44 items)` ⇒ 0 |
| **未完成项** | **1（T19）** | VCD 实现缺口「单独立项」⇒ **本批不做**（理由已登记）：① 「补不补实现」是**产品裁决**；② 新建 improvement 走 `add-improve` skill ⇒ 其 **HARD-GATE** 要求用户先批准该设计 ⇒ ⇒ **不替用户拍板、不伪造立项**。已登记为 `D04` + `O3`（owner = `architect` · trigger = 「用户裁决是否立项」） |

⇒ ⇒ **本批范围内无「半截实现 / 静默尾巴」**；唯一未完成项已显式登记（含 owner + trigger + 理由）。

---

## 5. 失败项与路由建议

**验收失败项：0** ⇒ 无需回 builder。

**待用户裁决（不属验收失败，属范围外裁决）**：
- **T19 / D04 / O3**：VCD「每拍自动 dump」的**实现缺口** ⇒ 补不补 ⇒ 用户裁决 ⇒ 裁决后走
  `add-improve` ⇒ 建 change ⇒ 实施。

---

## 6. 已用轮次

| 轮次 | 内容 | 结果 |
|---|---|---|
| 1 | 本地验收（契约 1–5 步 · 11 条 AC 全量取证） | ✅ 11/11 通过 · 0 失败项 · 遗留清扫 1 项已登记 |

⇒ ⇒ **未触发重试上限**（`RDDF_VERIFIER_MAX_LOOPS = 3`）⇒ ⇒ 无需 `halted`。

---

## 7. 结论

**批 A（8 段 Purpose + 根 AGENTS.md 快照订正 + Purpose 护栏 + 批 B/C 登记）验收通过。**
⇒ ⇒ 可进入归档（`openspec archive openspec-purpose-placeholders-2026-11` · `.openspec.yaml` 的
`skip_specs: true` ⇒ 归档**不合并任何 spec** ⇒ 零 Requirement 改动）。
⇒ ⇒ **唯一未完成项 T19**（VCD 实现缺口立项）已登记且**须用户先裁决** ⇒ 不阻断本批归档。
