# Phase 6 架构设计文档：高级特性与性能优化

**文档版本**: v1.0  
**创建日期**: 2026-04-10  
**阶段主题**: 高级特性开发 + 性能突破  
**前一阶段**: Phase 5 (IPC 0.90+, 5 级流水线+Cache+ 分支预测)  

---

## 📊 执行摘要

Phase 6 是 CppHDL RISC-V 处理器开发的高级阶段，目标是在 Phase 5 基础上实现性能突破和架构增强。

### Phase 6 核心目标

| 指标 | Phase 5 基线 | Phase 6 目标 | 提升幅度 |
|------|-------------|-----------|---------|
| **IPC** | 0.90+ | **1.20+** | **+33%** |
| **分支预测准确率** | 85%+ | **92%+** | +7% |
| **Cache 层级** | L1 (4KB) | **L1 + L2 (256KB)** | + 二级缓存 |
| **流水级数** | 5 级 | **5-8 级可配置** | 深度流水线 |
| **发射宽度** | 单发射 | **双发射 (P1 可选)** | 100% 提升 |

### Phase 6 关键特性

1. **Tournament 分支预测器** - 92%+ 准确率
2. **动态自适应预取器** - 流式 + 步长预取
3. **L2 Cache 统一缓存** - 256KB, 8-way
4. **Write Buffer 增强** - 8 条目 + 写合并
5. **超标量/乱序执行** (P1 可选) - 双发射 IPC 1.50+

---

## 🎯 Phase 6 设计原则

### 原则 1: 渐进式优化

- 基于 Phase 5 成熟架构，不做破坏性变更
- 每个特性独立模块化，可配置开关
- 性能提升可量化，可回退

### 原则 2: 性能导向

- 所有决策以 IPC 提升为核心指标
- 平衡硬件开销与性能收益
- 优先实现高 ROI 特性

### 原则 3: 可验证性

- 每个特性配套功能测试
- 性能基准测试框架完善
- 文档与代码同步更新

### 原则 4: 远期储备

- 为 Plugin 架构预留接口
- 为超标量/乱序执行预留扩展点
- 为多核一致性预留框架

---

## 🏗️ Phase 6 总体架构

### 3.1 系统框图

```
┌─────────────────────────────────────────────────────────────────┐
│                     CppHDL RISC-V Phase 6                       │
│                                                                 │
│  ┌──────────────┐     ┌──────────────┐     ┌──────────────┐   │
│  │   IF  stage  │────▶│   ID  stage  │────▶│   EX  stage  │   │
│  │  +BPU v3     │     │ +Decode v2   │     │ +ALU×2(P1)   │   │
│  │  +I-Cache    │     │ +Hazard v3   │     │ +Forward v3  │   │
│  └──────────────┘     └──────────────┘     └──────────────┘   │
│         │                    │                    │            │
│         ▼                    ▼                    ▼            │
│  ┌──────────────┐     ┌──────────────┐     ┌──────────────┐   │
│  │   MEM stage  │────▶│   WB  stage  │     │              │   │
│  │  +D-Cache    │     │ +Retire v2   │     │              │   │
│  │  +WBuff v2   │     │              │     │              │   │
│  └──────────────┘     └──────────────┘     │              │   │
│         │                                   │   L2 Cache   │   │
│         ▼                                   │   256KB      │   │
│  ┌──────────────┐                          │   8-way      │   │
│  │   AXI4       │─────────────────────────▶│              │   │
│  │   Interface  │                          │              │   │
│  └──────────────┘                          └──────────────┘   │
│                                                                 │
└─────────────────────────────────────────────────────────────────┘
```

### 3.2 模块层次结构

```
riscv/
├── core/
│   ├── rv32i_pipeline.h          # 5 级流水线核心 (Phase 5 继承)
│   ├── rv32i_hazard_v3.h         # Hazard Unit v3 (增强前推)
│   └── rv32i_retire.h            # 提交单元 (乱序执行预留)
│
├── branch_prediction/
│   ├── branch_predictor_v2.h     # 2-bit BHT (Phase 5, 保留)
│   ├── branch_predictor_tournament.h  # Tournament (NEW)
│   └── btb_enhanced.h            # 64 条目 BTB+RAS (NEW)
│
├── cache/
│   ├── i_cache_complete.h        # I-Cache (Phase 5, 保留)
│   ├── i_cache_prefetch.h        # 预取 (Phase 5, 保留)
│   ├── i_cache_adaptive_prefetch.h  # 自适应预取 (NEW)
│   ├── d_cache_complete.h        # D-Cache (Phase 5, 保留)
│   ├── d_cache_write_buffer.h    # Write Buffer (Phase 5, 保留)
│   └── l2_cache.h                # L2 Cache (NEW)
│
├── superscalar/                  # P1 可选
│   ├── dual_issue.h              # 双发射逻辑
│   ├── scoreboard.h              # 计分板
│   └── reservation_station.h     # 保留站
│
└── perf/
    ├── perf_counters.h           # 性能计数器 (增强)
    └── branch_stats.h            # 分支统计
```

---

## 🔧 Phase 6 关键技术规格

### 4.1 Tournament 分支预测器 (T601)

**目标**: 将分支预测准确率从 85%+ 提升至 92%+

**架构**:
```cpp
class TournamentBranchPredictor {
    // Pattern History Table (全局预测)
    ch_reg<ch_uint<2>> pht[1024];      // 1024 条目 × 2-bit
    
    // Bimodal 预测器 (局部预测)
    ch_reg<ch_uint<2>> bimodal[256];   // 256 条目 × 2-bit
    
    // 选择器 (决定使用哪个预测)
    ch_reg<ch_uint<2>> meta[256];      // 256 条目 × 2-bit
    
    // 全局历史寄存器
    ch_reg<ch_uint<10>> ghr;           // 10-bit GHR
};
```

**预测算法**:
```cpp
// 1. 计算索引
pht_index   = (pc ^ ghr) & 0x3FF;      // 10-bit
bimodal_idx = (pc >> 2) & 0xFF;        // 8-bit
meta_index  = (pc >> 2) & 0xFF;

// 2. 选择预测器
if (meta[meta_index] >= 2) {
    prediction = pht[pht_index];       // 选择全局预测
} else {
    prediction = bimodal[bimodal_idx]; // 选择局部预测
}

// 3. 更新选择器 (预测错误时)
if (prediction != actual) {
    if (actual == pht[pht_index]) {
        meta[meta_index]++;            // 更信任全局
    } else if (actual == bimodal[bimodal_idx]) {
        meta[meta_index]--;            // 更信任局部
    }
}
```

**硬件开销**:
- SRAM: (1024 + 256 + 256) × 2-bit = 3072 bits
- 逻辑：~500 LUTs
- 频率：>200MHz (关键路径：预测查询)

**性能预期**:
- 准确率：92%+ (vs 85% in Phase 5)
- IPC 提升：+0.03~0.05

---

### 4.2 动态自适应预取器 (T602)

**目标**: 根据访问模式动态调整预取策略

**架构**:
```cpp
class AdaptivePrefetcher {
    struct StreamDetector {
        ch_uint<32> last_addr;         // 上次访问地址
        ch_uint<32> stride;            // 步长
        ch_uint<4>  confidence;        // 置信度
        ch_uint<2>  state;             // FSM 状态
    };
    
    StreamDetector detectors[4];       // 4 路流式检测
    
    ch_reg<ch_bool>   enable;          // 预取使能
    ch_reg<ch_uint<2>> prefetch_depth; // 预取深度 (1-4)
};
```

**支持的模式**:
1. **顺序预取** (Phase 5 继承) - 下一行预取
2. **流式预取** - 检测连续地址序列
3. **步长预取** - 检测固定步长访问 (如数组遍历)
4. **混合模式** - 同时支持多种模式

**FSM 状态机**:
```
IDLE ──▶ ACCESS ──▶ PATTERN_MATCH ──▶ PREFETCH
  ▲         │              │              │
  │         ▼              ▼              ▼
  └────── RESET ◀──── MISMATCH ◀─── DONE
```

**性能预期**:
- Cache Miss 惩罚：-40% (vs -30% in Phase 5)
- IPC 提升：+0.03

---

### 4.3 Write Buffer 增强 (T603)

**目标**: 减少写 stall, 支持写合并

**规格**:
- 条目数：8 (vs 4 in Phase 5)
- 写策略：Write-Through with Buffer
- 写合并：支持字节掩码合并
- AXI 仲裁：优先级轮询

**架构**:
```cpp
class WriteBufferEnhanced {
    struct WBEntry {
        ch_uint<32> addr;              // 地址
        ch_uint<32> data;              // 数据
        ch_uint<4>  byte_mask;         // 字节掩码
        ch_bool     valid;             // 有效位
        ch_bool     pending;           // 等待 AXI 响应
    };
    
    WBEntry entries[8];                // 8 条目
    
    // AXI 写通道接口
    ch_bool     axi_aw_ready;
    ch_bool     axi_w_ready;
    ch_uint<2>  arbiter_ptr;           // 仲裁指针
};
```

**写合并逻辑**:
```cpp
// 检查是否可以合并 (地址相邻 + 字节掩码不重叠)
ch_bool can_merge = (
    (new_entry.addr == existing.addr) &&
    (~new_entry.byte_mask & existing.byte_mask) != 0
);

if (can_merge) {
    existing.data |= new_entry.data;
    existing.byte_mask |= new_entry.byte_mask;
} else {
    allocate_new_entry();
}
```

**性能预期**:
- Write Stall: -70% (vs -60% in Phase 5)
- IPC 提升：+0.02

---

### 4.4 L2 Cache 设计 (T604)

**目标**: 减少主存访问延迟，统一 I/D-Cache 后端

**规格**:
- 容量：256KB
- 组相联：8-way
- 行大小：64 字节
- 替换策略：LRU (或 PLRU 简化)
- 写策略：Write-Back

**架构**:
```cpp
class L2Cache {
    // 数据阵列 (256KB)
    ch_reg<ch_uint<512>> data[4096];   // 4096 行 × 512-bit (64B)
    
    // Tag 阵列 (8-way)
    struct TagEntry {
        ch_uint<20> tag;               // 20-bit Tag
        ch_uint<3>  lru;               // 3-bit LRU (8-way)
        ch_bool     valid;
        ch_bool     dirty;             // Write-Back 脏位
    };
    TagEntry tags[4096][8];            // 4096 组 × 8-way
    
    // AXI 主接口 (访问内存)
    AXIMasterPort axi_master;
    
    // AXI 从接口 (连接 I/D-Cache)
    AXISlavePort axi_slave;
};
```

**地址映射**:
```
32-bit Address:
┌─────────────┬─────────┬─────────┬──────────┐
│    Tag      │  Index  │  Set    │  Offset  │
│   [31:12]   │ [11:6]  │ [5:3]   │  [2:0]   │
│    20-bit   │  6-bit  │  3-bit  │  3-bit   │
└─────────────┴─────────┴─────────┴──────────┘
```

**性能预期**:
- L2 命中率：>95%
- 平均内存访问延迟：-50%
- IPC 提升：+0.08~0.12

---

### 4.5 超标量/乱序执行 (P1 可选 T611/T612)

**目标**: 双发射 IPC 1.50+

**架构**:
```cpp
class SuperscalarCore {
    // 取指单元 (每周期 2 条)
    FetchUnit fetch_unit;
    
    // 译码单元 (每周期 2 条)
    DecodeUnit decode_unit[2];
    
    // 乱序执行
    Scoreboard scoreboard;
    ReservationStation rs[16];
    
    // 执行单元
    ALU alu[2];                    // 2 个 ALU
    LoadStoreUnit lsu;
    BranchUnit bu;
    
    // 提交单元 (顺序提交)
    ReorderBuffer rob[32];
};
```

**数据流**:
```
IF (2 条/cycle) 
    │
ID (2 条/cycle)
    │
Dispatch (乱序)
    │
Issue (乱序)
    │
Execute (2 条/cycle)
    │
Writeback (2 条/cycle)
    │
Retire (顺序, 2 条/cycle)
```

**性能预期**:
- IPC: 1.50+ (vs 1.20+ in-order)
- 硬件开销：+200% LUTs
- 频率：-20% (关键路径延长)

---

## 📋 Phase 6 任务分解

### P0: 必做任务 (Week 1-2)

| 任务 ID | 任务 | 工时 | 交付物 | IPC 提升 |
|---------|------|------|--------|---------|
| **T601** | Tournament 预测器 | 3d | `branch_predictor_tournament.h` | +0.03~0.05 |
| **T602** | 动态自适应预取器 | 2d | `i_cache_adaptive_prefetch.h` | +0.03 |
| **T603** | Write Buffer 增强 | 2d | `d_cache_write_buffer_enhanced.h` | +0.02 |
| **T604** | L2 Cache 设计 | 4d | `l2_cache.h` + 测试 | +0.08~0.12 |

**P0 小计**: 11 工作日，目标 IPC 1.15~1.25

---

### P1: 选做任务 (Week 3)

| 任务 ID | 任务 | 工时 | 交付物 | IPC 提升 |
|---------|------|------|--------|---------|
| **T611** | 双发射超标量 | 5d | `superscalar_core.h` | +0.30 |
| **T612** | 基础乱序执行 | 5d | `scoreboard.h`, `rob.h` | +0.20 |
| **T613** | BTB 增强 (64+RAS) | 2d | `btb_enhanced.h` | +0.02 |
| **T614** | 向量扩展 RVV 基础 | 4d | `rvv_unit.h` | - |

**策略**: 选择 1-2 项，基于 P0 完成时间和性能目标达成情况

---

### P2: 远期储备 (Week 4)

| 任务 ID | 任务 | 工时 | 交付物 |
|---------|------|------|--------|
| **T621** | 多核一致性框架 | 5d | `mesi_protocol.h` |
| **T622** | 性能计数器增强 | 2d | `perf_counters_v2.h` |
| **T623** | 电源管理 | 3d | `power gating.h` |

**策略**: Phase 6 完成后再评估，可能延至 Phase 7

---

## 🎯 性能基准测试计划

### 测试负载

| 负载 | 说明 | 目标 IPC |
|------|------|---------|
| **CoreMark** | EEMBC CoreMark | 1.20+ |
| **Dhrystone** | 整数性能 | +30% vs Phase 5 |
| **Matrix Mult** | 内存密集型 | Cache 命中率 >95% |
| **Branch Heavy** | 分支密集型 | 预测准确率 >92% |

### 性能计数器

| 计数器 | 说明 |
|--------|------|
| `cycles` | 总周期数 |
| `instructions` | 退休指令数 |
| `l1_miss` | L1 Cache Miss 数 |
| `l2_miss` | L2 Cache Miss 数 |
| `branch_mispredict` | 分支预测错误数 |
| `stall_cycles` | Stall 周期数 |

---

## 📊 Phase 6 里程碑

| 里程碑 | 日期 | 交付物 |
|--------|------|--------|
| **M11: P0 完成** | 2026-05-14 | Tournament, 预取，WB, L2 Cache |
| **M12: 性能测试 v3.0** | 2026-05-16 | IPC 1.15+ |
| **M13: P1 完成 (可选)** | 2026-05-23 | 超标量/乱序执行 |
| **M14: 性能测试 v4.0** | 2026-05-25 | IPC 1.50+ (如完成 P1) |
| **M15: Phase 6 结项** | 2026-05-30 | 最终报告 |

---

## 🔧 技术风险评估

| 风险 | 概率 | 影响 | 缓解措施 |
|------|------|------|----------|
| L2 Cache 时序难收敛 | 中 | 高 | 先实现 4-way 简化版 |
| 超标量复杂度超预算 | 高 | 中 | 降级为双发射顺序 |
| 性能不达标 | 低 | 中 | 迭代优化，增加 P0 特性 |
| 与 Phase 5 代码冲突 | 低 | 低 | 分支开发，定期合并 |

---

## ⏭️ 与 Plugin 架构的关系

### 当前策略

根据用户要求，Plugin 架构以**新的 riscv 项目**开始，当前 RISC-V 保持现有架构继续 Phase 6 开发。

### Plugin 预留接口

```cpp
// 预留 Plugin 接口 (Phase 6 不实现，仅预留)
class IRiscVPlugin {
public:
    virtual void on_reset() = 0;
    virtual void on_tick() = 0;
    virtual void on_instr_retire(Instr instr) = 0;
};

// Phase 6 核心使用预留接口
class RiscVCore {
    std::vector<IRiscVPlugin*> plugins;  // 预留
    
    void register_plugin(IRiscVPlugin* p) {
        plugins.push_back(p);
    }
};
```

### Plugin 架构评估

详见文档：`docs/reports/PLUGIN-ARCHITECTURE-EVALUATION.md`

**结论**: Phase 6 完成后，基于成熟经验启动 Plugin 架构试点 (分支预测器 Plugin 化)。

---

## ✅ 验收标准

### 功能验收

| 功能 | 验收标准 |
|------|---------|
| Tournament 预测器 | 功能测试 100% 通过 |
| 自适应预取器 | 流式/步长模式检测正确 |
| Write Buffer | 8 条目 + 写合并功能正常 |
| L2 Cache | Cache 一致性验证通过 |

### 性能验收

| 指标 | Phase 5 基线 | Phase 6 目标 | 验收条件 |
|------|-------------|-----------|---------|
| IPC | 0.90+ | **1.20+** | CoreMark |
| 分支预测准确率 | 85%+ | 92%+ | 性能计数器 |
| L2 Cache 命中率 | N/A | >95% | 性能计数器 |
| 总 Cache Miss | 基线 | -50% | 性能计数器 |

### 代码质量验收

| 标准 | 要求 |
|------|------|
| 测试覆盖率 | >90% |
| 编译警告 | 0 |
| LSP 诊断 | 0 error, 0 warning |
| 文档完整性 | 每个模块配套文档 |

---

## 📚 参考资源

### 技术文档

- RISC-V Privileged Spec v1.12
- ARM Cortex-A Series Programmer's Guide
- Intel®64 and IA-32 Architectures Optimization Reference Manual

### 学术参考

- Stanford CS152: Computer Architecture
- MIT 6.823: Computer System Engineering
- Hennessy & Patterson: Computer Architecture: A Quantitative Approach

### Phase 5 继承文档

- `docs/reports/PHASE4-PERFORMANCE-BENCHMARK-V2.2.md`
- `docs/plans/PHASE5-FINAL-SUMMARY.md`
- `include/riscv/branch_predictor_v2.h`

---

**撰写人**: AI Agent (基于 Phase 5 成果)  
**审核状态**: 待审核  
**下次更新**: Phase 6 任务完成后  
**版本历史**: v1.0 (2026-04-10 创建)
