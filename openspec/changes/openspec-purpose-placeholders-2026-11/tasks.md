# openspec-purpose-placeholders-2026-11 — Tasks

## T0 —— 【硬门】用户批准

> 📌 **批准对象 = 权威提案层** `.rddf/improvements/openspec-purpose-placeholders-2026-11.md`
> （7 段草稿 · 已过 `pre_create_brainstorm_check.sh` HARD-GATE · 已 `rddf planner attach` 到 phase-6）。
> 本 change 的 `proposal.md` = 由该 improvement 经 `generate_full_proposal.py`（rdd-builder P0）
> **生成** ⇒ ⇒ **逐字终稿在 `design.md` §2**（与审查报告 §6 逐字一致）⇒ ⇒ **改字只改那一份**。

- [ ] **T0.1** 用户已批准批 A（8 段 Purpose 直改主 spec + AGENTS.md 快照订正 + 护栏 + 批 B/C 登记）
      - 批准语句（逐字记录）：`<待填>`
      - 批准日期：`<待填>`
      - 批准人：`<待填>`
- [ ] **T0.2** 用户已阅 `design.md` §2 的 8 段逐字终稿（**禁自由发挥** ⇒ 执行时逐字抄）
- [ ] **T0.3** 用户已确认 `design.md` §4 的三条风险（R1 散文注水 / R2 把假话写进第一屏 / R3 假绿）
- [ ] **T0.4** 用户已阅 `.rddf/improvements/openspec-purpose-placeholders-2026-11.md` 的
      `## Acceptance`（AC-0…AC-10）⇒ ⇒ **AC 的权威源 = improvement**，本 `tasks.md` 是其投影

> ⚠️ **T0 未勾选 ⇒ T1…T19 一律不得执行**（改主 spec / 改 AGENTS.md / 落护栏 / 提交）。

---

## 批 A —— 8 段 Purpose（**零 Requirement 改动**）

- [ ] **T1** 前置检查：`git status` 干净；`openspec list` ⇒「No active changes found.」（**权威口径**：
      `openspec/changes/` 下只有 `archive/` ⇒ 零 active change ⇒ 无并发归档冲突）
- [ ] **T1.1** **机械预检**：对 `design.md` §2 的 8 段跑 AC-2 / AC-3′ / AC-5 ⇒ 8/8 全绿
      （预检脚本落地位置 = `scripts/verify_openspec_purpose.sh` 的前身；2026-11-19 已实跑全绿 ⇒
      脚本随 T12 一起入库）
- [ ] **T2** 逐字写入 `build-cpp-standard` 的 `## Purpose`（文本 = `design.md` §2.1）
- [ ] **T3** 逐字写入 `jit-llvm-compat-tests`（§2.2）
- [ ] **T4** 逐字写入 `rv32i-privilege`（§2.3）
- [ ] **T5** 逐字写入 `verilator-hier-input`（§2.4）
- [ ] **T6** 逐字写入 `verilator-error-flow`（§2.5）
- [ ] **T7** 逐字写入 `verilator-backend-e2e`（§2.6）
- [ ] **T8** 逐字写入 `verilator-e2e-coverage`（§2.7）
- [ ] **T9** 逐字写入 `verilator-reg-init`（§2.8）
      （建议顺序 = 热身优先：`verilator-reg-init` → `build-cpp-standard` → `rv32i-privilege`
      → `verilator-hier-input` → `verilator-error-flow` → `verilator-backend-e2e`
      → `verilator-e2e-coverage` → `jit-llvm-compat-tests`）
- [ ] **T10** 根 `AGENTS.md`「OPENSPEC WORKFLOW」段：① strict 快照订正
      （`21 passed / 23 failed (44 items)` → 实测 `23/28/51`）② 补一行通道说明
      （「Purpose 无 change 通道 ⇒ 归档后手工写」⇒ 引用 ADR-072 §6 ⑨ + commit `1952ba5`）

## 护栏（防复发：新建 capability 归档时 CLI 会再写占位符）

- [ ] **T11** `.rddf/checks/openspec-purpose-placeholders.tsv`（登记表：`capability` / `owner` /
      `why_open` / `source_change`）—— 批 A 完成后**应为空集**（8 条全部清零 ⇒ 登记表留表头 + 0 行）
      ⚠️ 登记表**不得**删掉（它是**双向校验的另一半**；删掉 ⇒ 护栏失去对照面）
- [ ] **T12** `scripts/verify_openspec_purpose.sh`（照 `scripts/verify_docs_links.sh` 四件套抄：
      薄壳 + 登记表 + 逻辑载体 + `--self-test`）
      - **R1 双向**：`grep -l "^TBD - created by archiving" openspec/specs/*/spec.md` 的集合
        **==** 登记表行集合（**双向**，不是单向）
      - **R2**：登记表每行有 `owner` / `why_open` / `source_change`
      - **R3**：`--self-test` 投毒（把一个实质 Purpose 换回占位符 ⇒ 必红；登记表插一行不存在的
        capability ⇒ 必红）
      - **R4** fail-closed（脚本缺失 / 0 行 ⇒ exit 2，**禁恒 0**）
      - ⚠️ **不要**做「全仓零 TBD」的单向判据（下次归档时它会自己转红）
- [ ] **T13** T12 接进 CI（`build-and-test` 的 guard 段，照 `verify_docs_links.sh` 的接法）
- [ ] **T14** 护栏自检：`bash scripts/verify_openspec_purpose.sh` ⇒ PASS；
      `bash scripts/verify_openspec_purpose.sh --self-test` ⇒ PASS

## 收口

- [ ] **T15** 验收判据全过（见下 AC-0…AC-7）
- [ ] **T16** 单 commit 落库（**按路径显式 `git add`**，禁 `git add -A`；仓内多人/多会话并行）
      ⇒ commit message 逐条列出「改了哪 8 个 capability 的 Purpose + AGENTS.md 哪一段 + 护栏」

---

## 验收判据（AC）

**AC-0（否决性）**：**不得**以仓级 strict totals 当判据 ⇒ 补完 8 段 Purpose 后
`jit-llvm-compat-tests` / `rv32i-privilege` / `verilator-backend-e2e` / `verilator-e2e-coverage`
**仍红**（各有 `>500 字符` warning）⇒ 真实结果 = **27 passed / 24 failed / 51 items**（**不是** 31/20）。

| # | AC | 判定方式 |
|:-:|---|---|
| AC-1 | 8 个 target 的 **Purpose 占位符 warning 清零**（per-item） | 对 8 个 target 逐个 `openspec validate <c> --type spec --strict` ⇒「Purpose section is still a placeholder」**零命中** |
| AC-2 | Purpose 段非占位符（**与 CLI 文案解耦**） | 8 个文件的 `## Purpose` 段：长度 ≥ 120 字符 ∧ 不含 `TBD`/`TODO`/`placeholder`/`created by archiving` |
| AC-3′ | **不是标题拼接**（防「散文注水」）| ① Purpose 段 MUST NOT 出现任一 Requirement 标题的**完整文本**；② Purpose 的**任一行**与某标题（去 markdown 标记、去标点）的最长公共子串 / 标题长度 **MUST < 0.85**<br>⇒ ⚠️ **旧版「最长公共子串 < 10 字符」已作废**（会把「 MUST NOT 」等功能词碎片算成 slop ⇒ 假阳性 3/4）⇒ 详见审查报告 §11.8 |
| AC-4 | **纯散文改动** | `git diff -U0` 中 8 个文件**只在 `## Purpose` 块内**有 `+` 行；`grep '^[+-]### Requirement'` **零命中** |
| AC-5 | **指针对得着** | Purpose 里的反引号路径 `test -e` 成立；符号在 `include/`/`src/`/`tests/`/`examples/` 有定义 |
| AC-6 | **不破坏既有门** | `openspec validate --specs`（**非 strict**）仍 exit 0；`bash scripts/verify_openspec_delta.sh` 仍绿 |
| AC-7 | **簿记一致** | `AGENTS.md` 的 strict 快照已订正为实测值（见 T10） |

---

## 批 B —— 过期 / 矛盾 / 未实现的 Requirement（**本 change 不做，逐条立项**）

> 逐条判据 + 证据：`.rddf/reviews/openspec-purpose-placeholders-review-2026-11.md` §4.2（19 条）。
> 每条一个 openspec change + MODIFIED delta（`### Requirement:` 标题**逐字**匹配）。
> 优先级 = 危险度 > 内部矛盾 > 数量。

| 序 | capability | 位置 | 问题 | 建议动作 |
|:-:|---|---|---|---|
| 1 | `verilator-hier-input` | `spec.md:8` | 「等价 `num_srcs() > 0`」是**事实错误且方向危险**（`src/hierarchy_analysis.cpp:405-411` 明文纠正）⇒ 按它改代码会把**合法设计**判死 | 只描述**行为** + **删掉**该括号（另：机制位置已移到 `codegen_verilog.cpp:484`） |
| 2 | `verilator-e2e-coverage` | `spec.md:162-169` | 与 `:184-186` **互斥**（`popen` vs 共享 helper）⇒ 规范自相矛盾 | MODIFIED `:162` 为「共享 helper 形态」；保留「≥8 个用例真 lint」的行为判据 |
| 3 | `verilator-backend-e2e` | `spec.md:183-197` | 「3 步时钟（exactly 3 calls, clk=0/1/0）」与实现**相反**（2-step；AGENTS.md 也写 2-step） | 改写为 2-step + dated 订正注记 + 点名守卫 `TwoStepClockToggleIncrementOnePerTick`；⚠️ 需求 #4「1 cycle = 3 次 eval」**仍成立**，别一起改 |
| 4 | `verilator-backend-e2e` | `spec.md:9` | 「5 个符号」实为 **7**（正文自相矛盾）；`obj_dir/Vtop` 已被两阶段 `libVtop.so` 取代 | 改 7 + 改两阶段形态 |
| 5 | `verilator-backend-e2e` | `spec.md:54` | 缓存对象写成 `Vtop`（实现是 `libVtop.so`） | 改 |
| 6 | `verilator-backend-e2e` | `spec.md:79-89` | 引用**跨仓** ChipForge 文件 + `rv32ui-p-add` ELF ⇒ 本仓**不可复验** | 改指本仓形态（`verilator-rv32i-e2e` + `scripts/rv32i_rtl_sim_env.sh`） |
| 7 | `verilator-backend-e2e` | `spec.md:67-77` | VCD「每拍自动 dump」**未被实现满足**（生产路径从不调 `dump_vcd`） | **文案改写 + 实现缺口独立立项**（两件事，别混） |
| 8 | `verilator-error-flow` | `spec.md:52` + `:56` | 三载体矛盾（`design.md` / `/// @adr ADR-035 §5 R<n>` / 实现 `// @adr no-ADR` × 5） | 对齐实现（推荐）或删该 Scenario |
| 9 | `jit-llvm-compat-tests` | `spec.md:66-89` | Req 3 = **某次 commit 的 diff 快照** ⇒ 恒假 | 删该 Scenario（或改写为行为判据） |
| 10 | `build-cpp-standard` | `spec.md:17` | 「25 个二级 CMakeLists」实为 **23**；且**派生计数**本身是类别错误 | 改写为**继承判据**（根 CMake 三元组为唯一权威 + 子目录 MUST NOT 覆写）+ 枚举守卫 |
| 11 | `build-cpp-standard` | `spec.md:21-30` | 「8 破坏点审计记 commit message」= 一次性历史 ⇒ 不可复验 | 降级为 Purpose 里的一句 NOTE |
| 12 | `build-cpp-standard` | `spec.md:33` | 硬编码 6 个文档路径 ⇒ 新增文档即「违规」 | 改为「四类载体」不点具体文件 |
| 13 | `verilator-reg-init` | `spec.md:56` | 告警文本与实现不符 | 逐字对齐（或改判据为「MUST 产出含 `init_node is not type_lit` 的告警」） |
| 14 | `verilator-e2e-coverage` | `spec.md:199-218` | 「改名」要求**已完全落地** ⇒ 一次性动作留在常驻规范 = 纯历史 | 删 |
| 15 | `verilator-e2e-coverage` | `spec.md:237-245` | 「8 阶段 tasks.md」= **过程日志** | 删 |
| 16 | `verilator-e2e-coverage` | `spec.md:247-251` | 断言「strict 8/8 passed」**已可观测为假** | 删 |
| 17 | `verilator-e2e-coverage` | `spec.md:95` | 引用**不存在**的 `examples/axi4/axi4_full_example.cpp` | 改指现存文件 |

## 批 C —— 登记（不在本批修）

- [ ] **T17** 把批 B 的 17 条 + 审查报告 §8 的 O1–O7 登记进 `.rddf/checks/` 既有残留登记表
      （照 `ledger-residual-registry` 的双向校验范式）⇒ **每条有 owner + 触发条件**
- [ ] **T18** O7（根 `AGENTS.md` 家族被链接护栏显式排除 ⇒ 本次 `05→02` 路径修正是人工做的）
      ⇒ 登记「根 AGENTS.md 路径引用无护栏覆盖」
- [ ] **T19** VCD 实现缺口（批 B #7 的实现侧）单独立项
