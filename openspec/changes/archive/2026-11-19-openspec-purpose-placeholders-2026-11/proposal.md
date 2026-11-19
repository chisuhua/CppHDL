# openspec-purpose-placeholders-2026-11

## Why

- **可验证目标（user story）**：任何人读这 8 个 capability 的**第一屏**（`## Purpose`）就能知道
  「它管什么 / 为什么存在 / 边界在哪」⇒ ⇒ 而不必考古归档 change ⇒ ⇒ 且**不**读到与实现相反的话。
- **驱动**：这 8 个 capability 是仓内**审查与集成最常被引用**的一批（`verilator-*` × 5 在
  ADR-035/058/060/066/067/072 与 `.rddf/reviews/*` 中反复出现）⇒ ⇒ 它们的规范失真**传播面最大**。
- **为什么现在**：CLI 版本漂移已登记为「只登记、不消除」（AGENTS.md OPENSPEC WORKFLOW 段）⇒ ⇒
  本项是**首次**真正处理这一族（不是消除全部 strict 红项 —— 那要动 20 个 spec 的 Requirement 正文）。
- **不做的代价（反证）**：下一位触碰 Verilator 后端的贡献者**照着「3 步时钟」规范改代码**
  ⇒ ⇒ 把 2-step 改回 3-step ⇒ ⇒ 或为满足 `@adr` Scenario **编造**不存在的 ADR 映射
  ⇒ ⇒ 这是 AGENTS.md「为让判据绿而误删真能力」的**镜像事故**（为让判据绿而编造规范）。

---

1. **红项事实**：openspec CLI 1.14.3 的 strict 规则新增「Purpose 仍是占位符」⇒ ⇒
   `openspec/specs/` 下 **8 个** capability 命中（实测仓级 `23 passed / 28 failed / 51 items`）：

   | capability | 源 change |
   |---|---|
   | `build-cpp-standard` | `cpp23-standard-upgrade` |
   | `jit-llvm-compat-tests` | `2026-09-20-add-jit-llvm-compat-tests` |
   | `rv32i-privilege` | `csr-mcause-write-2026-11` |
   | `verilator-backend-e2e` | `adr-035-phase-3-complete` |
   | `verilator-e2e-coverage` | `strengthen-verilator-e2e-coverage` |
   | `verilator-error-flow` | `verilator-std-expected` |
   | `verilator-hier-input` | `verilator-hier-input-flattening-fix` |
   | `verilator-reg-init` | `verilator-reset-initial-value-mismatch` |

2. **缺口不是「文案」，是「规范面无人看守」**：2026-11-19 新增的文档链接护栏
   （`scripts/verify_docs_links.sh`）在范围登记表 `.rddf/checks/docs-link-scope.md:69` 里
   **明确把 `openspec/**` 列为 out-of-scope** ⇒ ⇒ 主 spec 的内容**零机械护栏覆盖** ⇒ ⇒
   规范里的假话可以长期存活（本次实测到 **6 处**）。

3. **审查结论（oracle 判定 `REJECT`）**：8 个里 **≥6 个含与实现相反 / 不可复验 / 把一次性历史写成
   常驻义务的 Requirement** ⇒ ⇒ **只补 Purpose = strict 部分转绿而规范继续说谎 = 假绿** ⇒ ⇒
   必须把「散文补写」与「规范命题修正」**分批**。
   - 与实现相反的 2 处（**危险**）：`verilator-backend-e2e` 要求「**3 步**时钟（exactly 3 calls，
     clk=0/1/0）」而实现是 **2-step**（`src/core/verilator_backend.cpp` 的 `eval_sequential`；
     AGENTS.md 的 VERILATOR BACKEND RULES 同款）⇒ ⇒ **照规范改代码会把 2-step 改回 3-step**；
     `verilator-hier-input/spec.md:8` 的「（等价 `num_srcs() > 0`）」是**事实错误且方向危险**
     （`src/hierarchy_analysis.cpp:405-411` 明文纠正：`ch_in` 节点自身的 `num_srcs()` 恒为 0 ⇒ ⇒
     用它判会把**每一个**被正常驱动的子端口误判成「未驱动」⇒ ⇒ **会把合法设计判死**）。

4. **治理依据（为什么允许直改主 spec）**：openspec 的 delta **机制上不含 `## Purpose`**
   （CLI 提示原文 + 仓内护栏 `scripts/verify_openspec_delta.sh:112`「`## Purpose` 不会被合并 ⇒
   不算违规」双重证实）⇒ ⇒ **change 流程对 Purpose 是死路** ⇒ ⇒ 而本仓**已裁决并已执行过**该场景
   （`ADR-072` §6 ⑨「归档后手工写」+ commit `1952ba5` 的 T17）⇒ ⇒ 本项采用**同一形状**：
   openspec change 承载**任务 + 逐字终稿**，执行时**手工写进主 spec** ⇒ ⇒ 与 G3 的禁令不冲突
   （该禁令的本意是防「绕过 delta 改 Requirement」，而本项**零 Requirement 改动**）。

5. **本仓的载体先例**：`arch-residual-registry-2026-11`（同类形态）其 Status 行明写「下一步 =
   创建 openspec change（★ 按增强层工件归属 ⇒ `proposal.md` 由 `rdd-builder` **P0** 从本草稿生成
   ⇒ **不手写**）」⇒ ⇒ 本项**必须**走 improvement → P0 生成 proposal 的路径。

---

## What Changes

**In Scope**:

- 8 个 capability 的 `## Purpose` **逐字**补写（逐字终稿 = 载体 change 的 `design.md` §2，
     与审查报告 §6 **逐字一致**⇒ 单一真相源）；
- 根 `AGENTS.md`「OPENSPEC WORKFLOW」段：① strict 快照订正（`21 passed / 23 failed (44 items)`
     → 实测 `23/28/51`）② 补一行通道说明（「Purpose 无 change 通道 ⇒ 归档后手工写」⇒ 引用
     `ADR-072` §6 ⑨ + commit `1952ba5`）；
- 新增常驻护栏 `scripts/verify_openspec_purpose.sh` + 登记表
     `.rddf/checks/openspec-purpose-placeholders.tsv`（**双向**集合校验 + fail-closed + `--self-test`
     投毒）⇒ 照 `scripts/verify_docs_links.sh` 的四件套抄（薄壳 + 登记表 + 逻辑载体 + `--self-test`）
     ⇒ 并接进 CI；
- 把 **19 条**过期 / 矛盾 / 未实现的 Requirement 与 **7 条**范围外观察**登记**进既有残留登记表
     （`ledger-residual-registry` 的双向校验范式）⇒ **逐条带 owner + 触发条件** ⇒ 供批 B 逐条立项。

**Out of Scope**:

- ❌ **任何 `### Requirement:` 正文 / Scenario 改动**（AC-4 机械钉死）⇒ 批 B 逐条另立项
     （每条一个 openspec change + MODIFIED delta，标题**逐字**匹配）；
- ❌ `rv32i-privilege` **改名**（正文已有范围声明 + `.rddf/improvements/rv32i-privileged-csr-2026-11.md:146`
     已登记「扩展」⇒ 改名会作废已登记计划 ⇒ 留到真正覆盖 CSR 指令时一并处理）；
- ❌ VCD「每拍自动 dump」的**实现补齐**（生产路径从不调 `dump_vcd` ⇒ 这是**实现缺口**不是文案问题
     ⇒ 独立立项）；
- ❌ 其余 43 个 capability 的 `>500 字符` warning（= 另一个项目，且要先探针确认 CLI 的 500 是
     **字符**还是**字节** —— 中文差 3×，口径错则门永远红）；
- ❌ 仓内 `openspec validate --specs --strict` 的其余红项。
---

## Capabilities

- **MUST NOT**：任何 `### Requirement:` 正文 / Scenario 的改动（AC-4 + AC-10 双重钉死）
- **MUST**：8 段 Purpose **逐字**写入（逐字源 = 载体 change `design.md` §2 ⇒ 单一真相源 ⇒ 禁自由发挥）
- **MUST**：Purpose 段**不得**包含与实现相反的 4 条断言（VCD 每拍 dump / 3 步时钟 / 缓存对象 `Vtop` /
- **MUST**：护栏为**双向**集合校验 + fail-closed + `--self-test` 投毒
- **MUST NOT**：把护栏做成「全仓零 TBD」的**单向**判据（新建 capability 归档时 CLI 会再写占位符 ⇒
- **MUST**：批 B 的 17 条过期 Requirement **逐条立项**（每条一个 openspec change + MODIFIED delta），
- **SHOULD**：本项的载体 change 归档时用 `skip_specs`（`.openspec.yaml`）声明「no spec changes」
- **SHOULD**：`rv32i-privilege` 的**改名**留到真正覆盖 CSR 指令时一并处理

## Impact

- **仓级 strict**：`23 passed / 28 failed (51 items)` ⇒ **`27 passed / 24 failed (51 items)`**
  （只清 8 条 Purpose warning；**不是** 31/20 —— 另 4 个 spec 还有 `>500 字符` warning）
- **公开 API / 运行时行为**：**零影响**（纯文档 + 1 个新护栏脚本）
- **新增常驻护栏**：Purpose 占位符集合 ↔ 登记表**双向** ⇒ ⇒ 新建 capability 归档时若又写占位符
  ⇒ ⇒ 护栏转红 ⇒ ⇒ **防复发**（当前形态：新建即复发、无人知晓）
- **新增可见性**：`.rddf/improvements/` 本项**入库**（跨人可见）⇒ 提案层不再只活在
  `.rddf/state/`（本机 · gitignored）
- **遗留登记**：19 条 Requirement 债（其中 **2 条与实现相反** ⇒ 若被照做会造成**真实代码损害**）
  + 7 条范围外观察（O1–O7）
- **风险面**：本项若**夹带** Requirement 改动 ⇒ 会绕过「spec 变更必须走 change」⇒ ⇒ AC-4/AC-10 双重钉死
- **下一阶段**：本项批准后 ⇒ `rdd-builder` P0 生成 `proposal.md` ⇒ P1 补 `design.md`/`tasks.md`
  ⇒ P2 实施 ⇒ P3 归档 ⇒ 归档后 `verilator-error-flow` / `verilator-hier-input` /
  `verilator-reg-init` / `build-cpp-standard` 4 个 spec 的 strict 红项**翻绿**

## 用户可见变更

> ⚠️ **本段是 2026-11-20 补写的**（归档时遗漏 ⇒ `scripts/verify_public_api_visibility.sh`
> 的 **R1**（归档 change MUST 带「## 用户可见变更」段 · 机制生效日 2026-11-09）报红
> ⇒ ⇒ 补的是**归档流程的产物要求**，不改动本 change 的任何决策与实施内容）。

| 项 | 前 | 后 |
|---|---|---|
| 8 个 capability 的 `## Purpose` 段 | `TBD - created by archiving change <name>. Update Purpose after archive.`（CLI 归档时写的骨架） | **逐字终稿**（实质描述）· 涉及 `build-cpp-standard` · `jit-llvm-compat-tests` · `rv32i-privilege` · `verilator-backend-e2e` · `verilator-e2e-coverage` · `verilator-error-flow` · `verilator-hier-input` · `verilator-reg-init` |
| 根 `AGENTS.md`「OPENSPEC WORKFLOW」段 | strict 快照写作 `21 passed / 23 failed (44 items)` | 订正为**实测** `23/28/51` · 并补一行「`## Purpose` 无 change 通道 ⇒ **归档后手工写**」（引用 `ADR-072` §6 ⑨ + commit `1952ba5`） |
| CI 步骤 | 无 Purpose 判据 | **新增 2 步**：`scripts/verify_openspec_purpose.sh` + `--self-test`（占位符 0 个 · 与登记表**双向**集合校验 · fail-closed） |
| 规范债的可见性 | 19 条过期 / 矛盾 Requirement + 7 条范围外观察**散落**在 spec 正文 | 登记进 `.rddf/checks/openspec-spec-debt.tsv`（逐条带 **owner + 触发条件**）⇒ ⇒ 「规范债」成为**可查的账**而非隐形负担 |

## Acceptance

> ⚠️ **AC-0 是否决性条款**：**不得**以仓级 strict totals 当判据 ⇒ 补完 8 段 Purpose 后
> `jit-llvm-compat-tests` / `rv32i-privilege` / `verilator-backend-e2e` / `verilator-e2e-coverage`
> **仍红**（各有 `>500 字符` warning）⇒ ⇒ 真实结果 = **27 passed / 24 failed / 51 items**
> （**不是** 31/20）。

- [ ] **AC-0**：仓级 strict 结果 = **27 passed / 24 failed / 51 items**（只有 4 个 spec 翻绿）
- [ ] **AC-1**：8 个 target 的「Purpose section is still a placeholder」warning **清零**（per-item，
      逐个 `openspec validate <c> --type spec --strict`）
- [ ] **AC-2**：每个 `## Purpose` 段 ≥ **120 字符** ∧ 不含 `TBD` / `TODO` / `placeholder` /
      `created by archiving`（**与 CLI 文案解耦** ⇒ CLI 改措辞不失明）
- [ ] **AC-3′**：Purpose 段 MUST NOT 出现任一 Requirement 标题的**完整文本**；且其**任一行**与某标题
      （去 markdown 标记、去标点）的最长公共子串 / 标题长度 **MUST < 0.85**
      （⚠️ 旧版「< 10 字符」判据**已作废**：会把「 MUST NOT 」等功能词碎片算成 slop ⇒ 假阳性 3/4）
- [ ] **AC-4**：`git diff -U0` 中 8 个文件**只在 `## Purpose` 块内**有 `+` 行；且
      `grep '^[+-]### Requirement'` **零命中**
- [ ] **AC-5**：Purpose 段里的**非 glob** 反引号路径 `test -e` **必须成立**；符号必须在
      `include/` / `src/` / `tests/` / `examples/` 有定义
- [ ] **AC-6**：`openspec validate --specs`（**非 strict** · ADR 判据面的人工 MUST）仍 exit 0；
      `bash scripts/verify_openspec_delta.sh` 仍绿；`openspec validate openspec-purpose-placeholders-2026-11 --strict` valid
- [ ] **AC-7**：根 `AGENTS.md` 的 strict 快照已订正为**实测值**（`23 passed / 28 failed / 51 items`），
      且含「Purpose 无 change 通道 ⇒ 归档后手工写」的通道说明
- [ ] **AC-8**：护栏 `scripts/verify_openspec_purpose.sh` 存在且 **fail-closed**（脚本缺失 / 0 行 ⇒
      exit 2，**禁恒 0**）；R1 为**双向**集合校验（占位符集合 == 登记表行集合）；`--self-test` 投毒
      （实质 Purpose 换回占位符 ⇒ 必红；登记表插不存在的 capability ⇒ 必红）；已接进 CI
- [ ] **AC-9**：19 条过期 Requirement + 7 条范围外观察已登记进既有残留登记表，**逐条**带
      `owner` + 触发条件 ⇒ 且登记表双向校验仍绿
- [ ] **AC-10**：**零** `### Requirement:` 改动（= AC-4 的独立复核）

---

