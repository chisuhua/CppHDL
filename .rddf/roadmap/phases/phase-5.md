---
id: phase-5
kind: phase
status: deferred
phase_refs: []
主题: [RV32I 集成与 riscv-mini 落地, cache-stub 物理退役, dual-backend 全量覆盖, 平台矩阵扩展]
---

## phase-5 概览

Phase 5 是**计划中**的阶段：把 Phase 4 建立的 dual-backend 基础设施与 cache 子系统推进到端到端可用，并完成长期技术债退役。

| Theme | 覆盖 | 状态 |
|-------|------|------|
| RV32I 集成 | `examples/riscv-mini/`, `include/cpu/pipeline/rv32i_pipeline.h` | ⏳ 计划中 |
| cache-stub 退役 | `include/cpu/cache/{i,d}_cache_stub.h` 物理删除 | ⏳ 计划中（deadline 2027-04-17） |
| dual-backend 全量覆盖 | Phase 3-5 of `verilator-parity-all-tests` | ⏳ 计划中 |
| 平台矩阵 | Windows-2022 CI 启用 | ⏳ 计划中 |

## 计划能力

- **i_cache_prefetch.h wrapper SIGSEGV 修复** — 当前 instantiate 触发 SIGSEGV（root AGENTS.md §KNOWN LIMITATIONS），是 Phase 4 的 `dual-backend-rv32i-integration` 隐藏阻塞项，需先单独立项修复
- **RV32I 完整 e2e** — interpreter + verilator + JIT 三路 trace parity 通过（objective-rv32i-e2e）
- **dual-backend Phase 3-5** — chlib (~50 test) / RV32I integration / full coverage
- **cache-stub 物理删除**（ADR-037）— 6-month calendar deadline 2027-04-17；触发前提：`dual-backend-cache-tests`（Phase 2）落地
- **逻辑算子 `[[deprecated]]` 移除** — ADR-036 §8 Removal Gate（6-month deadline），前提：全量 migration 已完成
- **Windows-2022 CI** — 当前仅 macOS-14 启用；Windows 延后（MSVC C++20 module 边界）

## 依赖关系

```
Phase 4 (dual-backend Phase 1) 
    → Phase 5 dual-backend Phase 2 (cache)
        → Phase 5 dual-backend Phase 3 (chlib)
            → Phase 5 dual-backend Phase 4 (RV32I)  ← 阻塞: i_cache_prefetch SIGSEGV
                → Phase 5 dual-backend Phase 5 (full coverage)
                    → cache-stub 物理删除 (ADR-037)
```

## 架构文档锚点

| 文档 | 与本 phase 关联 |
|------|----------------|
| `docs/adr/ADR-037-cache-stub-removal.md` | stub 物理删除策略 + deadline |
| `docs/adr/ADR-036-deprecate-hdl-logical-operators.md` | 逻辑算子 removal gate |
| `docs/adr/ADR-035-verilator-backend.md` | Verilator 后端（RV32I e2e 依赖） |
| `.rddf/improvements/cache-stub-physical-removal.md` | stub 退役 improvement |
| `.rddf/improvements/verilator-parity-all-tests.md` | dual-backend Phase 2-5 |
| root `AGENTS.md §KNOWN LIMITATIONS` | i_cache_prefetch SIGSEGV + Windows CI 延后 |

## 完成条件

- [ ] i_cache_prefetch.h wrapper SIGSEGV 修复
- [ ] RV32I e2e 三路 trace parity 通过
- [ ] dual-backend 全量覆盖（Phase 5 of 5）
- [ ] cache-stub 物理删除（ADR-037）
- [ ] 逻辑算子 `[[deprecated]]` 移除（ADR-036 §8）
- [ ] Windows-2022 CI 启用
