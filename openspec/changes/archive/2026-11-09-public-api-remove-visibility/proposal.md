# Proposal: 公开 API 移除的跨仓可见性（载体 + 判决规则边界 + 下游编译面）

> **improvement 来源**：`.rddf/improvements/public-api-remove-visibility-2026-11.md`
> （7 段 · **approved（用户拍板 R1 + R2）** ⇒ 载体 = 逐 change 一行 + 一次性回填）
>
> **上游 issue**：`https://github.com/chisuhua/CppHDL/issues/28` 第 ① 半
> （② 半 = 析构可组合性 ⇒ 已由 ADR-067 / change `2026-11-09-chlib-dtor-composability-2026-11` 收口）
>
> **本 change 无 spec 变更**（improvement §Capabilities：流程 / 台账类改动 ⇒ **不新增 / 不 MODIFIED**
> capability）⇒ ⇒ 归档时 **使用 `--skip-specs`**（这是本仓允许的唯一形态：change 显式声明无 spec 变更）。

## Why

上游的 Zero-Debt 判决规则「**零消费者 ⇒ 直接删**」的证据源**只有仓内 `grep`** ⇒ ⇒ 对
**跨仓下游**结构性失明。实证：`ch_state_machine::is_built()` 于 `c7b0b28`（ADR-050 D2）
被移除 ⇒ 上游全绿，而下游 ChipForge 于 **2026-11-08** 因编译失败才发现 ⇒ 上游 issue #28。

三笔账（improvement §架构依据，均已实测）：

1. **用户可见 CHANGELOG 自 2026-10-15 整体停更**（6 条 `##` 条目 vs **120** 个 2026-10-16 之后归档的 change ⇒ 立项时快照，当时归档总数 **145**）
   ⇒ 破坏性变更**没有任何载体**；
2. **无下游兼容 CI** ⇒ 下游被 break 时上游**零信号**；
3. **已有的活账只到头级**（`.rddf/checks/public-api-surface.md` 只登记顶层头）
   ⇒ ⇒ **符号级**移除不在任何机械判据的覆盖内。

## What Changes

1. **CHANGELOG 载体**（用户裁定 **R1 + R2**）：
   - **R2 一次性回填**：`docs/CHANGELOG.md` 新增 `2026-11-09` 条目，列出 2026-10-15 以来
     的**用户可见**破坏性变更（至少 ADR-050 的 15 符号移除 / `current_state()` 改名 /
     ADR-037 stub 族删除 / ADR-053 入口收敛）+ 本窗口新增的「可组合形态」；
   - **R1 逐 change 一行**：每个**机制生效后**归档的 change MUST 带非空
     `## 用户可见变更` 段（载体 = 归档件本身）⇒ 由护栏 R1 守。
2. **判决规则边界**：根 `AGENTS.md` 的 ZERO-DEBT「No dead code」段 +1 句
   ——「零『**仓内**』消费者的『**公开**』符号不算 dead code」⇒ 删它 MUST 走「移除 / 改名登记」。
3. **活账扩到符号级 + 机械探针**：
   - `.rddf/checks/public-api-surface.md` 新增「已移除 / 已改名的公开符号」章
     （符号 · 移除或改名 · 日期 · 依据 · **迁移指引**）+「已弃用（尚未移除）」表；
   - 新增 `.rddf/checks/public-api-removals.tsv`（**机器探针**：id / kind / scope / extra /
     evidence）⇒ 判定在**剥注释与字符串字面量之后**进行。
4. **下游编译面进 CI**：新增 `tests/downstream-consumer/{main.cpp,CMakeLists.txt}`
   （**独立** CMake 工程 · 只消费 `cmake --install` 的产物）+ `scripts/verify_downstream_consumer.sh`
   ⇒ 装到**临时 prefix** ⇒ 编译 + **运行** ⇒ 覆盖三种持有形态（局部变量 / `unique_ptr` /
   **按值成员 + 虚析构基类**）。
5. **两个新护栏 + `--self-test`**（`scripts/verify_public_api_visibility.sh` R1–R6 ·
   `scripts/verify_downstream_consumer.sh` R1–R6）⇒ 挂进 CI 的 `build-and-test` guard 段。
6. **反向指针**：ADR-043 / ADR-050 各加一条「适用范围已由本 change 收窄」注记
   （⚠️ **只加指针** ⇒ 已签批 ADR 的**决策内容零改动**）。

## Impact

- **产品代码**：**零**（`include/**` / `src/**` 不改）⇒ 无行为变更、无 ABI 影响；
- **流程 / 台账**：根 `AGENTS.md`（+1 句 · **AUTO 块不动**）· `docs/CHANGELOG.md` ·
  `.rddf/checks/{public-api-surface.md,public-api-removals.tsv}` · 两个新脚本 · 一个新 ctest 条目；
- **计数**：`CTEST_TOTAL` **254 → 255**（`downstream_consumer`）· non-perf **243 → 244**；
- **风险**：
  - ⚠️ 根 `AGENTS.md` 每次会话都注入 ⇒ 只加**一句**，不触 AUTO 块（`verify_roadmap_fragments.sh` 守）；
  - ⚠️ 下游 fixture **真实安装**上游 ⇒ MUST 用**临时 prefix**（禁污染工作树）· CI 时间 +≈5s；
  - ⚠️ 探针覆盖「**可机械判定**」的子集（`then()` 这类泛名、`state_def::parent` 这类泛化成员名
    不设探针）⇒ 该子集外的行仍靠人读 + 评审 ⇒ 已在脚本头注释与本文件**如实登记**；
- **回滚**：纯增量（新增文件 + 文档）⇒ 删除即可；CHANGELOG 条目保留（它是**历史记录**）。

## 验收标准

- [ ] **R1 载体可判**：`verify_public_api_visibility.sh` R1 绿（机制生效后归档的 change
      必须有非空 `## 用户可见变更`）；祖父条款**双向**（条目不存在 ⇒ 红）。
- [ ] **R4 CHANGELOG 载体可判（token 清单，禁日期判据）**：`is_built` / `current_state` /
      `resolve_hold` / `noexcept(false)` 各 ≥ 1 次。
- [ ] **判决边界句 `grep` 可判**：根 `AGENTS.md` 含「零「仓内」消费者的「公开」符号不算 dead code」，
      且 `verify_roadmap_fragments.sh` 绿（AUTO 块未被扰动）。
- [ ] **符号级双向可判**：`verify_public_api_visibility.sh` 的 R2/R5/R6 绿；
      `--self-test` 的 **M1–M4** 各使对应规则转红 ⇒ 还原复绿。
- [ ] **下游 fixture fail-closed**：`verify_downstream_consumer.sh` 绿（install → 独立工程 →
      **运行**）；`--self-test` 的 **M1**（投毒**已安装副本** ⇒ 下游编译必红 ⇒ 即 issue #28 复现）
      与 **M2**（空 fixture ⇒ R1 红）都成立。
- [ ] **缩面锚点**：`CTEST_TOTAL` 254→255 同步 ⇒ `verify_test_counts.sh` 绿。
- [ ] **反向指针**：ADR-050 / ADR-043 各含指针句 ⇒ 且 `git diff` 证明**决策内容零改动**。

## 用户可见变更

（本段按 `.rddf/checks/public-api-surface.md` 的载体机制填写 —— 每个归档 change MUST 一行以上。）

- **库代码 / 公开 API：无变更**（`include/**` 与 `src/**` 零改动 ⇒ 无行为、无 ABI 影响）。
- **流程（下游相关，**值得知道**）**：自本 change 起，**每个归档 change MUST 带一节
  `## 用户可见变更`**；**公开符号**的移除 / 改名 MUST 登记进
  `.rddf/checks/public-api-surface.md` 的「已移除 / 已改名的公开符号」章
  （含**迁移指引**）并同步 `docs/CHANGELOG.md` ⇒ ⇒ 下游升级前应先读这两处。
- **CI**：新增两步护栏（`verify_public_api_visibility.sh` / `verify_downstream_consumer.sh`），
  后者会用 `cmake --install` 的产物编译并**运行**一个下游视角 TU。
