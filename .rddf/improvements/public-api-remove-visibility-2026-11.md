---
name: public-api-remove-visibility-2026-11
issue_ref: 28
gh_repo: chisuhua/CppHDL
priority: P1
phase: phase-6
category: framework
type: process
状态: draft
主题: 已知功能缺口清零
依赖: ADR-050, ADR-043
来源: 上游 issue #28（下游 ChipForge 反馈 · 2026-11-08）· phase-6 P12（起草）/ P14（实施）
生成时间: 2026-11-09
evidence: adr:docs/adr/ADR-050-chlib-state-machine-semantics.md
---

# Improvement: 公开 API 移除的**跨仓可见性** —— 用户可见 CHANGELOG 载体 + 判决规则边界 + 下游编译面

- **Date drafted**: 2026-11-09
- **Drafted by**: planner 角色（build-agent）
- **Status**: 📝 **草案**（**不需架构裁定** ⇒ 可直接立项为 change；⚠️ 唯一待用户拍板项 = §What Changes 的 **CHANGELOG 载体命运**）
- **上游 issue**：`https://github.com/chisuhua/CppHDL/issues/28`（2026-11-08 · 下游 ChipForge 反馈 · 原文见附录）
- **与姊妹项的关系**：本项承接 issue #28 的**第 ① 半**（「`is_built()` 移除对跨仓下游不可见」）；
  **第 ② 半**（「析构 `noexcept(false)` ⇒ 类型不可组合」）已拆为
  `chlib-dtor-noexcept-composability-2026-11`（⇒ 需 architect 裁定 ⇒ ADR-067）
  ⇒ ⇒ **本项不被 ADR 审稿门卡住** ⇒ ⇒ 可先落地。

**主题**: 已知功能缺口清零

## 架构依据

### 根因：一条**成文判决规则**，其证据源**只有仓内 grep**

Zero-Debt 政策（根 `AGENTS.md`「No dead code」）+ ADR-043 先例 ⇒ 仓内形成的**判决规则**是：

> **零消费者 ⇒ 直接删**（不留 deprecated 别名期）

`ADR-050` §D2 把这条规则**逐字**援引为 `is_built()` 等 15 个符号的移除依据 ⇒ ⇒
**证据源 = 仓内 `grep`** ⇒ ⇒ 而 `grep` 对**跨仓下游**是**结构性失明**的
（下游仓库的调用点在上游 `grep` 的搜索范围之外 ⇒ ⇒ 「零消费者」这个事实**只在仓内成立**）。

⚠️ ADR-050 并非草率：它**显式**写过这条判决规则的适用前提 ——

> 「本项目无 formal release 节奏 ⇒ 无『外部使用者需要 deprecation 周期』的**事实基础**」

⇒ ⇒ 该**事实基础今天被证伪**：下游确实存在（`chisuhua/CppHDL` 早已为 ChipForge 提供 install 规则），
且下游**确实**消费了 `is_built()`（⇒ 移除后下游**编译失败**）⇒ ⇒
⇒ **需要修订的是这条判决规则的适用范围**（不是推翻 Zero-Debt 本身）。

### 三笔账（逐条可核）

| # | 账 | 实测 | 后果 |
|---|---|---|---|
| **1** | **用户可见 CHANGELOG 整体停更** | `docs/CHANGELOG.md` 只有 **6** 条 `##` 条目（2026-09-21 ~ 2026-10-15）⇒ 而 `openspec/changes/archive/` 已归档 **145** 个 change ⇒ ADR-050（2026-10-30）**一字未记** | 下游**没有任何渠道**得知破坏性变更 ⇒ 「补一条 CHANGELOG」只是窄缝 ⇒ **载体命运需先裁定**（见 §What Changes） |
| **2** | **无下游兼容 CI** | `.github/workflows/ci.yml` 无任何 ChipForge 联编 ⇒ 而 `CMakeLists.txt` 自 `46608a5`（2026-06-12）起提供 ChipForge 集成门控 + install 规则 ⇒ ⇒ 上游**知道**有下游，但**没有**任何信号回路 | 下游被 break 时上游**零信号** ⇒ ⇒ 破坏性变更**静默**穿透 |
| **3** | **已有的机器没被用在符号级** | `.rddf/checks/public-api-surface.md`（**活账** + `scripts/verify_hierarchical_p8.sh` 的 **R4②** 双向核对）**只登记顶层头**，`include/chlib/**` 被**明确排除**（理由：167 行清单失去可审性） | 「哪些是公开 API」有活账 ⇒ 但**只到头级** ⇒ ⇒ **符号级**的移除（`is_built()` 这类）**不在任何机械判据的覆盖内** |

### 下游是怎么被打破的（时序）

下游通过 **symlink + `ExternalProject_Add`** 消费上游**工作树** ⇒ ⇒ 上游破坏性变更**立即**生效
⇒ ⇒ 叠加「CHANGELOG 停更 + 无下游 CI」⇒ ⇒ **下游承担了全部发现成本**。

## 范围

- **In Scope**：
  1. **CHANGELOG 载体命运裁定 + 落地**（★ 唯一待用户拍板项 ⇒ 见 §What Changes）；
  2. **判决规则边界修订**：根 `AGENTS.md` 的 ZERO-DEBT 政策段增补一句 ——
     「**零『仓内』消费者的『公开』符号不算 dead code**」⇒ ⇒ 并在 `docs/adr/` 的对应 ADR（ADR-050 / ADR-043）
     加**反向指针**（⚠️ **不单方面改写已签批 ADR 的决策内容** ⇒ 只加指针与
     「适用范围已由本 change 收窄」注记 ⇒ ⇒ **不指向任何不存在的 ADR**
     ⇒ ⇒ 若实施中发现该收窄其实需要**架构裁决** ⇒ ⇒ **升级**为 ADR 并在此登记 ⇒ ⇒ 否则不得预置 ADR 编号）；
  3. **活账扩到符号级**：`.rddf/checks/public-api-surface.md` 增加**符号级**登记（至少 `chlib` 聚合面
     ⇒ 头 + 公开符号 + **移除记录**字段）⇒ ⇒ R4② 的双向核对扩到符号级
     ⇒ ⇒ 「移除**未被登记**」与「登记了**已不存在**的符号」双向可判；
  4. **消费者 fixture 进上游 CI**：用仓内**已有**的 install 规则（`CMakeLists.txt` 的
     `install(DIRECTORY include/ DESTINATION include/cpp-hdl)` + `install(TARGETS cpphdl)`）
     ⇒ 装到**临时 prefix** ⇒ 用**独立 CMake 工程**编译一个**下游视角** TU
     ⇒ ⇒ 覆盖「不同 include 前缀 + 独立工程 + `find_package`/`add_subdirectory`」三个真实差异
     ⇒ ⇒ **缺 fixture = 失败**（fail-closed）。
- **Out of Scope**：
  - ❌ **恢复 `is_built()`**（上游有反向断言 `tests/test_state_machine.cpp:201`
    的 `STATIC_REQUIRE(!has_is_built<Sm>::value)` ⇒ 恢复 = 改测试 + 动 ADR-050 ⇒ **独立决策**）；
  - ❌ **析构 `noexcept(false)` 的可组合性** ⇒ 姊妹项 `chlib-dtor-noexcept-composability-2026-11`；
  - ❌ **发布 / tag / 版本号策略**（**独立立项** —— 本项只解决「变更**不可见**」，不解决「变更**无版本**」）；
  - ❌ 下游仓库（ChipForge）的任何改动。

## Why

1. **下游承担了全部发现成本**：破坏性变更**静默**穿透（账 1 + 账 2）⇒ ⇒ 「仓内全绿」≠「下游可用」。
2. **判决规则的适用前提已被证伪**（ADR-050 自陈的前提）⇒ ⇒ 不修订 ⇒ ⇒ **下一个** ADR 会照抄同一句。
3. **已有机器只差一层**（账 3）⇒ ⇒ 扩到符号级是**小改动**（现有 R4② 的双向核对机制可直接复用）
   ⇒ ⇒ 比新造一套文档义务**便宜**。
4. **本项不被审稿门卡住**（不需 ADR）⇒ ⇒ 而它的收益（下游可发现性）**独立于**姊妹项的裁定 ⇒
   ⇒ ⇒ **捆在一起 = 让便宜的流程修复被昂贵的架构裁定拖住** ⇒ ⇒ 拆开。

## What Changes

### ★ 待用户拍板：CHANGELOG 的**载体命运**（一句话即可）

| 选项 | 做法 | 代价 | 判据 |
|---|---|---|---|
| **R1 维持 + 逐 change 一行**（**planner 建议**） | 每个归档 change MUST 留一行「用户可见变更」⇒ ⇒ 由 **archive 流程**强制 ⇒ ⇒ 载体 = 归档件（**单一真相**已在归档目录）⇒ CHANGELOG 退化为**精选摘要** | 需要**改归档流程**（新增一个 MUST） | 「每个归档 change ≥1 行用户可见变更」= **1:1 守恒** ⇒ 可数 |
| **R2 re-baseline** | 写一条 2026-11-09 汇总条目，列出 2026-10-15 以来**全部**用户可见破坏性变更（至少 ADR-050 的 15 符号移除 + `current_state()` 改名 + 析构契约变化）⇒ ⇒ 之后回到「逐 change 一行」 | 一次性工作量（145 个 change 的回填**抽样**，不是全量） | token 清单（`is_built` / `current_state` / `resolve_hold` / `noexcept(false)` 各 ≥ 1 次） |
| **R3 废弃** | 明写「本仓**不承诺**用户可见变更日志」⇒ ⇒ 下游**必须**自行 diff 上游 | 零维护成本 ⇒ 但「下游可发现性」**无解** ⇒ ⇒ 只能靠 4（消费者 fixture） | 文件删除 + 指针改指 |

⇒ ⇒ **无论选哪个，判据都 MUST 是 token 清单 / 计数守恒，禁「日期 ≥」**（写一行新日期就绿 = 无判别力）。

### 落地动作（归 builder）

1. `docs/CHANGELOG.md`：按裁定的载体命运落地（**只新增**条目 ⇒ 禁改写既有条目的事实）；
2. 根 `AGENTS.md` 的 ZERO-DEBT 政策段：增补「公开符号的 dead code 判定边界」一句（⚠️ 根 `AGENTS.md` 的
   AUTO 块**不动** ⇒ 该段不在 AUTO 块内 ⇒ 不牵动 `verify_roadmap_fragments.sh`）；
3. `.rddf/checks/public-api-surface.md`：扩到符号级（+ **移除记录**字段）+ `scripts/verify_hierarchical_p8.sh`
   的 R4② 扩到符号级（⚠️ **须同步 `--self-test` fixture** ⇒ 防「登记与清单一起删」的静默缩面）；
4. 新增**消费者 fixture** + CI 挂载（fail-closed）+ **计数锚点**（新增 `AGENTS.md` 的 NOTES 锚点
   ⇒ 由 `scripts/verify_test_counts.sh` 守 ⇒ 防「fixture 与 CMake 声明一起删」）；
5. ADR-050 / ADR-043 加**反向指针**（⚠️ 只加指针与「适用范围已收窄」注记 ⇒ **不单方面改决策内容**）。

## Acceptance

> ⚠️ 判据原则：可**机械判定** ⇒ ⇒ 禁「可行动地失败」「有结论」这类散文。

- [ ] **CHANGELOG 载体已裁定并落地**：裁定记录在 change 的 `design.md`（★ 三个选项之一 + 理由）⇒ ⇒
      按裁定的判据**绿**：R1 ⇒ 「每个归档 change ≥ 1 行用户可见变更」的**计数守恒**（可数）；
      R2 ⇒ token 清单命中（`is_built` / `current_state` / `resolve_hold` / `noexcept(false)` 各 ≥ 1 次
      + ADR-050 的 15 符号移除清单完整）；R3 ⇒ 文件**删除** + 指针改指（`grep` 可判）。
- [ ] **判决规则边界已修订**：根 `AGENTS.md` 的 ZERO-DEBT 段含「零**仓内**消费者的**公开**符号不算 dead code」
      ⇒ ⇒ `grep` 可判；⇒ ⇒ `AGENTS.md` 的 **AUTO 块逐字节不变**（`scripts/verify_roadmap_fragments.sh` 绿）。
- [ ] **活账符号级双向核对生效**：`scripts/verify_hierarchical_p8.sh` 的 R4② 在**符号级**双向判
      （登记了**不存在**的符号 ⇒ 红 · **存在未登记**的公开符号 ⇒ 红）⇒ ⇒ `--self-test` 的**原地投毒**必红
      ⇒ ⇒ 还原复绿。
- [ ] **消费者 fixture 进 CI**：CI 里把上游装到**临时 prefix** ⇒ 用**独立 CMake 工程**编译一个下游视角 TU
      ⇒ ⇒ **缺 fixture = 失败**（fail-closed，禁「跳过」）⇒ ⇒ 至少覆盖三种持有形态
      （局部变量 / `unique_ptr` / **按值成员 + 虚析构基类**）。
- [ ] **缩面护栏**：消费者 fixture 的**计数锚点**已写入 `AGENTS.md` 的 NOTES ⇒ ⇒
      `scripts/verify_test_counts.sh` 绿 ⇒ ⇒ 「fixture 与 CMake 声明一起删」⇒ 必红。
- [ ] **反向指针已加**：ADR-050 / ADR-043 各含指向本次裁定的指针 ⇒ ⇒ `grep` 可判 ⇒ ⇒
      ⚠️ 已签批 ADR 的**决策内容零改动**（`git diff` 可判）。

## Capabilities

- **涉及 capability**：**不新增**、**不 MODIFIED** ⇒ ⇒ 本项是**流程 / 台账**类改动
  ⇒ ⇒ ⚠️ 故**不需要** capability delta ⇒ ⇒ **不需要** openspec change 的 spec 合并
  ⇒ ⇒ 但仍 MUST 走 change 流程（`openspec/changes/<name>/`）⇒ 以 `proposal.md` 承载变更说明。
- **若**实施中判定「用户可见变更日志」应当成为**契约**（而不只是流程）⇒ ⇒ **必须**另立 capability
  ⇒ ⇒ 本 improvement **不预设**该结论。

## Impact

- **改动面**：根 `AGENTS.md`（ZERO-DEBT 段 · **AUTO 块不动**）· `docs/CHANGELOG.md` ·
  `.rddf/checks/public-api-surface.md` · `scripts/verify_hierarchical_p8.sh`（R4② + `--self-test`）·
  新增消费者 fixture + CI 挂载 + NOTES 锚点 · ADR-050 / ADR-043（**仅指针**）。
- **风险**：
  - ⚠️ 根 `AGENTS.md` 是**每次会话都注入**的文件 ⇒ ⇒ 改动 MUST 最小化（**只加一句边界**）⇒
    ⇒ 且**不得**触碰 AUTO 块（否则牵动 roadmap 护栏）；
  - ⚠️ 活账扩到符号级 ⇒ 清单会变长 ⇒ ⇒ 「可审性」是当初**排除** `include/chlib/**` 的理由
    ⇒ ⇒ MUST 有**范围闸门**（至少 chlib 聚合面）+ 逐条登记 + `--self-test` 兜底；
  - ⚠️ 消费者 fixture 会**真实安装**上游产物 ⇒ ⇒ CI 时间增加 ⇒ ⇒ 且 MUST 用**临时 prefix**
    （禁污染工作树）；
  - ⚠️ 「每个归档 change ≥ 1 行」的 1:1 规则若落到**历史** 145 个 change ⇒ ⇒ 工作量不可控
    ⇒ ⇒ 判据 MUST 区分「**新增**的归档」与「历史存量」（R2 的抽样回填）。
- **不做 / 延后的代价**：破坏性变更**继续**静默穿透 ⇒ 下游**继续**承担全部发现成本 ⇒
  且判决规则的适用前提**继续**被误用（下一个 ADR 会照抄 ADR-050 的同一句）⇒ ⇒ 这是**制度性**缺口，
  不是「一次性」缺口。
- **下游诉求的状态**（如实登记）：① 的**紧迫性已解除**（下游已改断言 `entry_state()` +
  `state_out().impl() != nullptr`）⇒ ⇒ 本项的价值在**下一个**下游 / 下一个破坏性变更，不在当下这次。

## 上游 issue 原文（附录 · 引用自 https://github.com/chisuhua/CppHDL/issues/28 · 2026-11-08）

**title**: `[下游反馈] ch_state_machine 的 is_built() 移除与 noexcept(false) 析构破坏跨仓下游编译`

**与本项相关的原文要点**：

- ① 「该 commit 的移除清单把 `is_built()` 判为『零消费者』。这个判定**对 CppHDL 自身仓库成立**，
  但**跨仓消费者存在** —— ChipForge 用它做『DSL 已发射』的断言。**由于本仓此前一直用旧产物跑测试，
  该消费关系在上游侧不可见。**」
  ⇒ ⇒ ★ 这句**直指根因**：上游的测试**没有**跑下游 ⇒ ⇒ 「不可见」是**测试覆盖问题**，不只是文档问题。
- ① 「**建议**：若无意恢复该查询，至少在移除清单/CHANGELOG 中标注『跨仓消费者需自行迁移』，
  或提供一个等价的状态查询。」
- 影响面：上游 `include/chlib/state_machine.h`；下游 ChipForge `include/cf/plugin/multi_cycle_fsm.h`
  （框架级 FSM 模板 · 被 3 个 CH_MEM 插件继承）。

<!-- 引用自 https://github.com/chisuhua/CppHDL/issues/28 (2026-11-09) -->
