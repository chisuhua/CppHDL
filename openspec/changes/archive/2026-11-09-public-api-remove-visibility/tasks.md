# Tasks: 公开 API 移除的跨仓可见性

> 判据来源 = improvement 的 Acceptance（逐条对应 proposal §验收标准）。
> ⚠️ 纪律：改了 `tests/CMakeLists.txt` / `AGENTS.md` 的计数锚点后 MUST 重跑
> `verify_test_counts.sh`；`ctest` 前 MUST 全量重建（否则跑旧二进制 = 假绿）。

## 0. 载体（用户裁定 R1 + R2）

- [x] **T1** `docs/CHANGELOG.md`：新增 `2026-11-09` 条目（**一次性回填** 2026-10-15 以来的
      用户可见破坏性变更 + 本窗口的「可组合形态」），并写明**机制**（每个归档 change 一行）
- [x] **T2** 契约：机制生效后的归档件 MUST 带 `## 用户可见变更`
      ⇒ 首个例证 = `openspec/changes/archive/2026-11-09-chlib-dtor-composability-2026-11/proposal.md`

## 1. 判决规则边界

- [x] **T3** 根 `AGENTS.md` 的 ZERO-DEBT「No dead code」段 +1 句判决边界（**AUTO 块不动**）
- [x] **T4** `docs/adr/ADR-050-*.md` / `docs/adr/ADR-043-*.md` 各加**反向指针**
      （只加指针 ⇒ **决策内容零改动**）

## 2. 活账（符号级）

- [x] **T5** `.rddf/checks/public-api-surface.md` 新增「已移除 / 已改名的公开符号」章
      （符号 · 移除或改名 · 日期 · 依据 · **迁移指引**）+「已弃用（尚未移除）」表
- [x] **T6** `.rddf/checks/public-api-removals.tsv`（机器探针 15 条 · 四种 kind ·
      已知限制写进头注释）

## 3. 护栏

- [x] **T7** `scripts/verify_public_api_visibility.sh`（R1 归档载体 / R2 移除登记 /
      R3 判决边界 + roadmap 守恒 / R4 CHANGELOG token 清单 / R5 机械探针（剥注释）/ R6 缩面锚点）
- [x] **T8** `scripts/verify_public_api_visibility.sh --self-test`：M1–M4 **原地投毒** ⇒ 必红 ⇒ 还原复绿
- [x] **T9** `tests/downstream-consumer/{main.cpp,CMakeLists.txt}`（独立工程 · 三种持有形态）
- [x] **T10** `scripts/verify_downstream_consumer.sh`（install → 临时 prefix → 独立工程 → **运行**）
- [x] **T11** `scripts/verify_downstream_consumer.sh --self-test`：M1（投毒**已安装副本** ⇒ 必红）
      / M2（空 fixture ⇒ fail-closed）
- [x] **T12** `tests/CMakeLists.txt` 注册 `downstream_consumer` ctest 条目（⇒ `CTEST_TOTAL` 缩面锚点）
- [x] **T13** `.github/workflows/ci.yml` 的 `build-and-test` guard 段挂两个脚本（主路径 + 自检）

## 4. 锚点与全量验证

- [x] **T14** `AGENTS.md` 的 `CTEST_TOTAL` 254 → 255 · non-perf 243 → 244
- [x] **T15** 全量重建（`cmake --build build -j`）⇒ 0 error
- [x] **T16** `ctest -N` = 255 ⇒ `verify_test_counts.sh` 绿
- [x] **T17** `verify_public_api_visibility.sh`（+ `--self-test`）· `verify_downstream_consumer.sh`
      （+ `--self-test`）· `verify_roadmap_fragments.sh` · `verify_hierarchical_p8.sh` 全绿
- [x] **T18** `ctest -LE perf --output-on-failure` 全绿（含新 `downstream_consumer`）

## 5. 收口

- [x] **T19** `phase-6.md` 的 **P14** 转 ✅ + 证据路径；improvement ① 状态 → **approved + 已实施**；
      `improvement-suggestions.md` 同步
- [x] **T20** 归档（**`--skip-specs`** —— 本 change 显式声明无 spec 变更）
