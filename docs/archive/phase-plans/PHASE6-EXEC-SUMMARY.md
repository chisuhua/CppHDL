# Phase 6 执行计划摘要

**快速参考版本** | 创建日期：2026-04-10

---

## 📊 一页纸概要

| 项目 | Phase 6 详情 |
|------|------------|
| **阶段主题** | 高级特性开发 + 性能突破 |
| **时间周期** | 2026-04-10 ~ 2026-05-30 (4 周) |
| **IPC 目标** | 0.90+ (Phase 5) → **1.20+** (Phase 6) |
| **关键特性** | Tournament 预测器、自适应预取、L2 Cache |
| **代码量** | ~1620 行 (P0) + ~500 行 (P1 可选) |

---

## 🎯 核心目标

### 性能目标

```
Phase 5 基线：0.90 IPC
    │
    +0.03~0.05  [Tournament 预测器]
    │
    +0.03       [自适应预取器]
    │
    +0.02       [Write Buffer 增强]
    │
    +0.08~0.12  [L2 Cache 256KB]
    │
Phase 6 目标：1.20+ IPC (相对提升 33%)
```

### 功能目标

| 模块 | Phase 5 | Phase 6 | 提升 |
|------|---------|--------|------|
| 分支预测器 | 2-bit BHT | Tournament | 85% → 92%+ |
| 预取器 | 顺序预取 | 自适应预取 | 流式 + 步长 |
| Write Buffer | 4 条目 | 8 条目 + 合并 | +100% |
| Cache 层级 | L1 only | L1 + L2 | +256KB |

---

## 📋 P0 必做任务 (Week 1-2)

| 任务 ID | 任务名称 | 工时 | 交付物 | IPC 提升 |
|---------|---------|------|--------|---------|
| **T601** | Tournament 分支预测器 | 3d | `branch_predictor_tournament.h` | +0.03~0.05 |
| **T602** | 动态自适应预取器 | 2d | `i_cache_adaptive_prefetch.h` | +0.03 |
| **T603** | Write Buffer 增强 | 2d | `d_cache_write_buffer_enhanced.h` | +0.02 |
| **T604** | L2 Cache 设计 | 4d | `l2_cache.h` | +0.08~0.12 |

**P0 小计**: 11 工作日，目标 IPC **1.15~1.25**

---

## 📋 P1 选做任务 (Week 3, 可选)

| 任务 ID | 任务名称 | 工时 | 交付物 | IPC 提升 |
|---------|---------|------|--------|---------|
| **T611** | 双发射超标量 | 5d | `superscalar_core.h` | +0.30 |
| **T612** | 基础乱序执行 | 5d | `scoreboard.h`, `rob.h` | +0.20 |
| **T613** | BTB 增强 (64+RAS) | 2d | `btb_enhanced.h` | +0.02 |

**策略**: 选择 1-2 项，基于 P0 完成情况

**P1 小计**: 可选 2-10 工作日，目标 IPC **1.30~1.50+**

---

## 📅 里程碑时间线

```
2026-04          2026-05
    10  16  23  30  07  14  21  28  30
    │   │   │   │   │   │   │   │   │
    ├───┼───┼───┤   │   │   │   │   │
    │ W1│ W2│ W3│   │   │   │   │   │
    │   │   │   ├───┼───┼───┼───┼───┤
    │   │   │   │ W4│ W5│ W6│ W7│结项│
    │   │   │   │   │   │   │   │   │
    ▶   T601  │   │   │   │   │   │
    │   T602  │   │   │   │   │   │
    │   │ T603│   │   │   │   │   │
    │   │ T604│   │   │   │   │   │
    │   │   │v3.0   │   │   │   │
    │   │   │P1 决策│   │   │   │
    │   │   │   P1(可选)  │   │
    │   │   │       │v4.0│   │
    │   │   │       │   │文档│结项│
    
M11: 2026-05-14  P0 完成
M12: 2026-05-16  性能测试 v3.0 (IPC 1.15+)
M13: 2026-05-23  P1 完成 (可选)
M14: 2026-05-25  性能测试 v4.0 (IPC 1.50+, 可选)
M15: 2026-05-30  Phase 6 结项
```

---

## 🔧 关键技术规格

### Tournament 分支预测器

```cpp
// 1024 条目 PHT + 256 条目 Bimodal + 256 条目 Meta
class TournamentBranchPredictor {
    ch_reg<ch_uint<2>> pht[1024];      // Pattern History Table
    ch_reg<ch_uint<2>> bimodal[256];   // Bimodal 预测器
    ch_reg<ch_uint<2>> meta[256];      // 选择器
    ch_reg<ch_uint<10>> ghr;           // 全局历史寄存器
};
```

**准确率**: 92%+ (vs 85% in Phase 5)

---

### 自适应预取器

```cpp
// 4 路流式检测 + 可配置预取深度
class AdaptivePrefetcher {
    struct StreamDetector {
        ch_uint<32> last_addr;
        ch_uint<32> stride;
        ch_uint<4>  confidence;
    };
    StreamDetector detectors[4];
    ch_reg<ch_uint<2>> prefetch_depth;  // 1-4
};
```

**支持模式**: 顺序 + 流式 + 步长

---

### L2 Cache

```cpp
// 256KB, 8-way, Write-Back
class L2Cache {
    ch_reg<ch_uint<512>> data[4096];   // 4096 行 × 64B
    struct TagEntry {
        ch_uint<20> tag;
        ch_uint<3>  lru;               // 8-way LRU
        ch_bool     valid;
        ch_bool     dirty;
    } tags[4096][8];
};
```

**命中率**: >95%

---

## 📊 性能基准

### 测试负载

| 负载 | Phase 5 基线 | Phase 6 目标 |
|------|-------------|-----------|
| CoreMark | 0.90 IPC | **1.20+ IPC** |
| Dhrystone | 0.85 IPC | 1.10+ IPC |
| Matrix Mult | 0.70 IPC | 1.00+ IPC |
| Branch Heavy | 0.80 IPC | 1.10+ IPC |

### 性能报告版本

- **v3.0** (2026-05-16): P0 完成，IPC 1.15+
- **v4.0** (2026-05-25): P0+P1 完成，IPC 1.50+ (可选)

---

## ✅ 验收标准

### 功能验收

- [ ] Tournament 预测器功能测试 100% 通过
- [ ] 自适应预取器流式/步长检测正确
- [ ] Write Buffer 8 条目 + 写合并正常
- [ ] L2 Cache 一致性验证通过
- [ ] Phase 5 回归测试 100% 通过

### 性能验收

| 指标 | Phase 5 | Phase 6 目标 | 验收条件 |
|------|---------|-----------|---------|
| IPC | 0.90+ | **1.20+** | CoreMark |
| 分支预测准确率 | 85%+ | 92%+ | 性能计数器 |
| L2 Cache 命中率 | N/A | >95% | 性能计数器 |

### 代码质量验收

- [ ] LSP 诊断：0 error, 0 warning
- [ ] 测试覆盖率：>90%
- [ ] 文档完整性：每个模块配套文档

---

## 📁 交付文件清单

### 核心代码 (~1620 行)

```
include/riscv/branch_predictor_tournament.h       (~300 行)
include/chlib/i_cache_adaptive_prefetch.h         (~200 行)
include/chlib/d_cache_write_buffer_enhanced.h     (~220 行)
include/chlib/l2_cache.h                          (~400 行)
include/riscv/btb_enhanced.h                      (~150 行，P1)
include/riscv/superscalar_core.h                  (~500 行，P1 可选)
```

### 测试代码 (~610 行)

```
tests/riscv/test_branch_predictor_tournament.cpp  (~100 行)
tests/chlib/test_adaptive_prefetch.cpp            (~80 行)
tests/chlib/test_write_buffer_enhanced.cpp        (~80 行)
tests/chlib/test_l2_cache.cpp                     (~150 行)
tests/riscv/test_phase6_integration.cpp           (~200 行)
```

### 文档 (~58 页)

```
docs/architecture/PHASE6-ARCHITECTURE.md          (~15 页)
docs/plans/PHASE6-IMPLEMENTATION-PLAN.md          (~12 页)
docs/architecture/tournament-branch-predictor.md  (~5 页)
docs/architecture/adaptive-prefetcher.md          (~4 页)
docs/architecture/l2-cache-design.md              (~6 页)
docs/reports/PHASE6-PERFORMANCE-BENCHMARK-v3.0.md (~8 页)
docs/reports/PHASE6-PERFORMANCE-BENCHMARK-v4.0.md (~8 页，可选)
```

---

## ⚠️ 关键风险与缓解

| 风险 | 概率 | 影响 | 缓解措施 |
|------|------|------|----------|
| L2 Cache 时序难收敛 | 中 | 高 | 先实现 4-way, 128KB 简化版 |
| 性能不达标 | 低 | 中 | 迭代优化，增加预取深度 |
| P1 进度延期 | 高 | 低 | 降级为可选，优先保 P0 |

---

## 🔗 相关文档

- **架构设计**: `docs/architecture/PHASE6-ARCHITECTURE.md` (详细版)
- **实施计划**: `docs/plans/PHASE6-IMPLEMENTATION-PLAN.md` (详细版)
- **Phase 5 总结**: `docs/plans/PHASE5-FINAL-SUMMARY.md`
- **Phase 5 性能**: `docs/reports/PHASE4-PERFORMANCE-BENCHMARK-V2.2.md`

---

## 📝 快速检查清单

### Week 1 结束检查 (2026-04-16)

- [ ] T601 Tournament 预测器编译通过
- [ ] T602 自适应预取器编译通过
- [ ] 单元测试框架就绪

### Week 2 结束检查 (2026-04-23)

- [ ] T603 Write Buffer 增强编译通过
- [ ] T604 L2 Cache 编译通过
- [ ] 集成测试框架就绪

### P0 完成检查 (2026-05-14)

- [ ] 所有 P0 任务功能测试通过
- [ ] 回归测试 100% 通过
- [ ] 性能测试 v3.0 准备就绪

### Phase 6 结项检查 (2026-05-30)

- [ ] IPC 1.20+ 达成
- [ ] 所有文档完成
- [ ] Git 提交整洁 (8-12 commits)

---

**状态**: 🟡 待启动  
**创建人**: AI Agent  
**最后更新**: 2026-04-10  
**下次更新**: Week 1 结束 (2026-04-16)
