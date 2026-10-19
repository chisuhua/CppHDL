# CppHDL Roadmap

## 元信息
- **版本**: 1
- **创建时间**: 2026-10-19
- **最后更新**: 2026-10-19
- **当前阶段**: phase-4
- **当前 Sprint**: sprint-2026-10

## Phase Skeleton
| Phase | Theme | Status | Started | Done |
|-------|-------|--------|---------|------|
| phase-1 | 核心抽象与算子基础设施 | archived | 2026-05 | 2026-05 |
| phase-2 | 高级特性与 JIT 编译器 | archived | 2026-05 | 2026-06 |
| phase-3 | Verilator 后端集成与 Verilog codegen | archived | 2026-06 | 2026-09 |
| phase-4 | dual-backend 测试完整化 + cache 子系统重建 | active | 2026-10 | |
| phase-5 | RV32I 集成 + cache-stub 退役 + 平台矩阵 | deferred | | |
<!-- AUTO-INDEX -->

## Fragment Index (auto-generated)

### Phases
- `phase-1` — 核心抽象与算子基础设施
- `phase-2` — 高级特性与 JIT 编译器
- `phase-3` — Verilator 后端集成
- `phase-4` — dual-backend 测试完整化
- `phase-5` — RV32I 集成与 riscv-mini 落地

### Features
- `feat-cache-rewrite` — cache 子系统从 C-style 数组重写为 ch_mem（ADR-039）+ Sync/Async 双变体（ADR-041）+ stub 物理退役（ADR-037） (refs: phase-4, phase-5)
- `feat-dual-backend-coverage` — 单元测试统一跑 interpreter + verilator 双后端（5 阶段推进） (refs: phase-4, phase-5)
- `feat-hdl-logical-deprecation` — ADR-036 逻辑算子 deprecation 标记 → 全量迁移 → 移除（removal gate） (refs: phase-4, phase-5)
- `feat-platform-matrix` — CI 平台矩阵扩展（macOS-14 已启用 → Windows-2022） (refs: phase-3, phase-5)
- `feat-rv32i-integration` — RV32I 5-stage 流水线 + riscv-mini SOC 端到端集成（三路 trace parity） (refs: phase-5)

<!-- AUTO-SPRINT-START -->
## Current Sprint: sprint-2026-10

| Project | Phase | Priority | Feedback | Proposal |
|---------|-------|----------|----------|----------|
| dual-backend 测试完整化 | phase-4 | P0 | none | verilator-parity-all-tests |

### Unmapped (0)
<!-- AUTO-SPRINT-END -->

### Objectives
- `objective-cache-stub-retired` — cache stub 物理删除（ADR-037 deadline 2027-04-17）
- `objective-rv32i-e2e` — RV32I 完整 e2e 在 interpreter + verilator + JIT 三路 trace parity 通过
- `objective-test-coverage-100pct` — 单元测试 100% 双后端（interpreter + verilator）覆盖

