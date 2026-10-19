---
id: objective-cache-stub-retired
status: active
created: 2026-10-19
last_revised: 2026-10-19
review_by: 2027-01-17
owner: rdd-planner
priority: P0
manual_deps: []
supersedes: null
theme: cache stub 物理删除（ADR-037 deadline 2027-04-17）
---

# Objective: objective-cache-stub-retired

## 1. 驱动诊断（Why now）

`include/cpu/cache/{i,d}_cache_stub.h` 是 pass-through smoke fixture（`ready := req, miss := false, data := axi_data`），2026-10-17 依 ADR-037 标记 `[[deprecated]]` 并改名 `*_stub.h`。ADR-037 定了 6-month calendar deadline（2027-04-17）物理删除，解决"≥1 release cycle"不可判定问题（项目无 formal release 节奏）。删除前提：cache-complete-rewrite（ADR-039）已完成 ✅，且 `dual-backend-cache-tests`（Phase 2）需要先让 cache 测试脱离 stub。

适用场景：cache stub 调用面清理；不适用场景：cache 功能开发（已完成）。

## 2. 目标愿景 + 完成判据

- `include/cpu/cache/{i,d}_cache_stub.h` 从 repo 物理删除（`git rm`）
- 所有 stub 消费者迁移到 `ICacheSync` / `DCacheSync`（或 `_complete.h` 兼容 alias）
- `test_cache_pipeline_integration.cpp` 重写为脱离 stub
- 完成判据（done-when）：
  - [ ] `git rm include/cpu/cache/{i,d}_cache_stub.h` 完成
  - [ ] 0 个文件 `#include "*_cache_stub.h"`
  - [ ] 2 个 SKIP-only test `git rm`
  - [ ] `test_cache_pipeline_integration.cpp` 重写并绿
  - [ ] CTEST_TOTAL 同步 + `verify_test_counts.sh` 绿

## 3. 架构依据

- **ADR-037** — cache stub 物理删除策略（6-month calendar deadline 2027-04-17）
- **ADR-039** — cache complete rewrite via ch_mem（删除前置条件）
- **ADR-041** — cache Sync/Async variant split
- `.rddf/improvements/cache-stub-physical-removal.md` — 落地 improvement
- `.rddf/improvements/cache-stub-physical-removal.research.md` — 调用面 grep 调研
- root `AGENTS.md §KNOWN LIMITATIONS` — "Cache stubs are pass-through"

## 5. 反例

- 不要在 `dual-backend-cache-tests`（Phase 2）之前执行（同一批 test 会被改两次）
- 不要为了让删除"更彻底"而顺手重构 cache 功能（scope 仅限 stub 退役）
- 不要假设 stub 消费者只有 riscv-mini——需完整 grep 调用面

## 9. 目标依赖与 Decision Gate

### 9.1 前置 objective 依赖
- `objective-test-coverage-100pct` — `dual-backend-cache-tests`（Phase 2）必须先让 cache 测试脱离 stub
- 潜在冲突：ADR-037 §2 提到 "F 路径（git rm）" 是风险变量，若走 F 路径删掉 `_complete.h` 则本 objective 目标对象消失 → 必须在 dual-backend proposal 显式声明"依赖 ADR-037 不走 F 路径"

### 9.2 Go / No-Go Decision Gate
- **Go 判据**（满足全部 2 条 → 转 completed）：
  - [ ] ADR-037 §2 前置条件（cache-complete-rewrite + dual-backend-cache-tests）满足
  - [ ] 调用面 grep 显示 0 个非 test 消费者
- **No-Go 判据**（任一成立 → defer）：
  - deadline（2027-04-17）前 dual-backend-cache-tests 未落地
  - 发现 stub 仍是某 active 路径的唯一实现

### 9.3 跨 objective 影响
- 受此 objective 影响的下游：`objective-rv32i-e2e`（cache 子系统清理）
- 受上游 objective 影响：`objective-test-coverage-100pct`（Phase 2 cache 测试迁移）

## 10. next_sprint_candidates
- sprint-2026-10: 无（等 dual-backend Phase 2 落地）
- deadline 2027-04-17 前的 sprint: cache-stub 物理删除 change

## 11. 跟踪台账（append-only）
| Sprint | kind | 内容 | Decision/调整 | 原因 |
|--------|------|------|---------------|------|
| sprint-2026-10 | sprint-review | objective 创建（roadmap 首次初始化） | 激活，deadline 2027-04-17 | cache-complete-rewrite（ADR-039）已完成，前置条件部分满足 |
| sprint-2026-10 | deferral-rationale | 实际删除推迟到 dual-backend-cache-tests 之后 | 延迟 | 避免同一批 cache test 被改两次（metis gap 分析） |
