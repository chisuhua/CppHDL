# Phase 6 实施计划文档

**文档版本**: v1.0  
**创建日期**: 2026-04-10  
**基于架构**: `docs/architecture/PHASE6-ARCHITECTURE.md`  
**预计工期**: 4 周 (2026-04-10 ~ 2026-05-30)  

---

## 📋 目录

1. [实施路线图](#1-实施路线图)
2. [P0 任务详细计划](#2-p0-任务详细计划)
3. [P1 任务详细计划](#3-p1-任务详细计划)
4. [测试策略](#4-测试策略)
5. [性能基准测试计划](#5-性能基准测试计划)
6. [风险管理](#6-风险管理)
7. [交付清单](#7-交付清单)

---

## 1. 实施路线图

### 1.1 阶段划分

```
Phase 6 实施时间线 (4 周)
│
├─ Week 1 (2026-04-10 ~ 2026-04-16)
│   ├─ T601: Tournament 分支预测器 (3 天)
│   └─ T602: 动态自适应预取器 (2 天)
│   └─ 周评审 (2026-04-16)
│
├─ Week 2 (2026-04-17 ~ 2026-04-23)
│   ├─ T603: Write Buffer 增强 (2 天)
│   ├─ T604: L2 Cache 设计 (4 天)
│   └─ 周评审 (2026-04-23)
│
├─ Week 3 (2026-04-24 ~ 2026-04-30)
│   ├─ 性能测试 v3.0 (2 天)
│   ├─ P1 任务选择与实施 (3 天, 可选)
│   └─ 周评审 (2026-04-30)
│
└─ Week 4 (2026-05-01 ~ 2026-05-30)
    ├─ P2 远期储备研究 (2 天，可选)
    ├─ 性能测试 v4.0 (2 天)
    ├─ 文档整理与结项 (2 天)
    └─ Phase 6 结项评审 (2026-05-30)
```

### 1.2 关键路径

```
T601 (3d) ──┐
            ├─▶ 性能测试 v3.0 (2d) ──▶ P1 决策 ──▶ v4.0 (2d) ──▶ 结项 (2d)
T602 (2d) ──┘
T603 (2d) ──┤
T604 (4d) ──┘

关键路径: T604 → 性能测试 → 结项
总工期：11 工作日 (P0) + 可选 P1/P2
```

### 1.3 依赖关系

| 任务 | 前置依赖 | 后续依赖 |
|------|---------|---------|
| T601 | Phase 5 branch_predictor_v2.h | 性能测试 v3.0 |
| T602 | Phase 5 i_cache_prefetch.h | 性能测试 v3.0 |
| T603 | Phase 5 d_cache_write_buffer.h | 性能测试 v3.0 |
| T604 | T603 (Write Buffer) | 性能测试 v3.0 |
| P1 任务 | P0 全部完成 | 性能测试 v4.0 |

---

## 2. P0 任务详细计划

### 2.1 T601: Tournament 分支预测器

**目标**: 实现 Tournament 预测器，准确率从 85%+ 提升至 92%+

**文件**:
- 新增：`include/riscv/branch_predictor_tournament.h` (~300 行)
- 测试：`tests/riscv/test_branch_predictor_tournament.cpp` (~100 行)
- 文档：`docs/architecture/tournament-branch-predictor.md`

**实施步骤**:

```
Day 1: 设计与框架
├─ 上午：技术方案细化 (2h)
│   ├─ PHT/BHT/meta 大小确认
│   ├─ 索引算法设计
│   └─ 更新逻辑设计
├─ 下午：框架实现 (4h)
│   ├─ 类结构定义
│   ├─ IO 端口定义
│   └─ 状态寄存器定义
└─ 晚上：基础功能实现 (2h)
    ├─ 预测查询逻辑
    └─ 索引计算

Day 2: 核心逻辑
├─ 上午：预测算法 (3h)
│   ├─ 选择器逻辑
│   ├─ 预测输出
│   └─ BTB 集成
├─ 下午：更新逻辑 (4h)
│   ├─ PHT 更新
│   ├─ BHT 更新
│   ├─ Meta 更新
│   └─ GHR 更新
└─ 晚上：集成到流水线 (2h)
    ├─ 替换 Phase 5 预测器
    └─ 编译验证

Day 3: 测试与验证
├─ 上午：单元测试 (3h)
│   ├─ 2-bit 计数器测试
│   ├─ 选择器测试
│   └─ GHR 更新测试
├─ 下午：功能测试 (3h)
│   ├─ 典型分支模式测试
│   ├─ 准确率测量
│   └─ 边界条件测试
└─ 晚上：LSP 验证与文档 (2h)
    ├─ LSP 诊断通过
    └─ API 文档编写
```

**验收标准**:
- [ ] 功能测试 100% 通过
- [ ] LSP 诊断 0 error, 0 warning
- [ ] 模拟测试准确率 >90%
- [ ] 文档完整

**风险**:
- 索引冲突导致性能下降 → 增加 PHT 大小
- 更新逻辑时序复杂 → 简化为单周期更新

---

### 2.2 T602: 动态自适应预取器

**目标**: 实现动态预取，支持流式 + 步长模式

**文件**:
- 新增：`include/chlib/i_cache_adaptive_prefetch.h` (~200 行)
- 测试：`tests/chlib/test_adaptive_prefetch.cpp` (~80 行)
- 文档：`docs/architecture/adaptive-prefetcher.md`

**实施步骤**:

```
Day 1: 流式检测器
├─ 上午：流式检测算法 (3h)
│   ├─_stride 计算
│   ├─ 置信度 FSM
│   └─ 状态转移
├─ 下午：硬件实现 (4h)
│   ├─ StreamDetector 结构
│   ├─ 4 路检测器阵列
│   └─ 匹配逻辑
└─ 晚上：预取缓冲 (2h)
    ├─ 2 条目预取缓冲
    └─ 替换策略

Day 2: 集成与测试
├─ 上午：I-Cache 集成 (3h)
│   ├─ 替换 Phase 5 预取器
│   ├─ 预取请求生成
│   └─ AXI 握手
├─ 下午：单元测试 (3h)
│   ├─ 流式检测测试
│   ├─ 步长检测测试
│   └─ 预取缓冲测试
└─ 晚上：文档与验证 (2h)
    └─ LSP 诊断通过
```

**验收标准**:
- [ ] 流式检测准确率 >90%
- [ ] 步长检测准确率 >85%
- [ ] 预取缓冲命中率 >95%
- [ ] 编译通过，LSP 诊断 clean

---

### 2.3 T603: Write Buffer 增强

**目标**: 8 条目 Write Buffer + 写合并

**文件**:
- 新增：`include/chlib/d_cache_write_buffer_enhanced.h` (~220 行)
- 测试：`tests/chlib/test_write_buffer_enhanced.cpp` (~80 行)
- 废弃：Phase 5 `d_cache_write_buffer.h` (保留向后兼容)

**实施步骤**:

```
Day 1: 写缓冲核心
├─ 上午：8 条目缓冲设计 (2h)
│   ├─ WBEntry 结构
│   ├─ 字节掩码支持
│   └─ 有效位管理
├─ 下午：写合并逻辑 (4h)
│   ├─ 地址匹配
│   ├─ 掩码重叠检测
│   └─ 数据合并
└─ 晚上：AXI 仲裁 (2h)
    ├─ 优先级轮询
    └─ 仲裁 FSM

Day 2: 集成与测试
├─ 上午：D-Cache 集成 (3h)
│   ├─ 替换 Phase 5 Write Buffer
│   └─ 写请求队列
├─ 下午：单元测试 (3h)
│   ├─ 写合并测试
│   ├─ 8 条目压力测试
│   └─ AXI 仲裁测试
└─ 晚上：文档与验证 (2h)
```

**验收标准**:
- [ ] 写合并功能正常
- [ ] 8 条目压力下无死锁
- [ ] AXI 仲裁公平性验证
- [ ] LSP 诊断 clean

---

### 2.4 T604: L2 Cache 设计

**目标**: 256KB, 8-way 统一 Cache

**文件**:
- 新增：`include/chlib/l2_cache.h` (~400 行)
- 测试：`tests/chlib/test_l2_cache.cpp` (~150 行)
- 文档：`docs/architecture/l2-cache-design.md`

**实施步骤**:

```
Day 1: 数据阵列与 Tag
├─ 上午：数据阵列设计 (3h)
│   ├─ 256KB 存储 (64B/行)
│   ├─ 4096 行 × 8-way
│   └─ 读写端口
├─ 下午：Tag 阵列 (4h)
│   ├─ Tag + LRU + Dirty
│   ├─ 8-way LRU 实现
│   └─ 替换逻辑
└─ 晚上：地址映射 (2h)
    └─ Tag/Index/Set/Offset

Day 2: AXI 接口
├─ 上午：从接口 (连接 I/D-Cache) (3h)
│   ├─ 请求队列
│   ├─ 响应逻辑
│   └─ Hit/Miss 检测
├─ 下午：主接口 (连接内存) (4h)
│   ├─ AXI 读请求
│   ├─ AXI 写请求
│   └─ 填充逻辑
└─ 晚上：Hit/Miss FSM (2h)
    ├─ Hit 路径
    └─ Miss 填充路径

Day 3: 一致性与替换
├─ 上午：Write-Back 策略 (3h)
│   ├─ Dirty 位管理
│   ├─ Victim 选择
│   └─ 写回 AXI
├─ 下午：LRU 更新 (3h)
│   ├─ Hit 更新
│   ├─ Miss 更新
│   └─ 初始化
└─ 晚上：单元测试框架 (2h)

Day 4: 集成与测试
├─ 上午：系统级集成 (3h)
│   ├─ 连接 I-Cache
│   ├─ 连接 D-Cache
│   └─ AXI 时序验证
├─ 下午：完整测试 (4h)
│   ├─ Cache 一致性测试
│   ├─ LRU 正确性测试
│   ├─ Write-Back 测试
│   └─ 性能测试
└─ 晚上：文档与验证 (2h)
```

**验收标准**:
- [ ] Cache 一致性验证通过
- [ ] LRU 替换正确
- [ ] Write-Back 功能正常
- [ ] 256KB 地址映射正确
- [ ] AXI 握手时序正确

**风险**:
- LRU 实现复杂 → 简化为 PLRU (伪 LRU)
- 时序难收敛 → 先实现 4-way 简化版

---

## 3. P1 任务详细计划 (可选)

### 3.1 T611: 双发射超标量

**目标**: 每周期发射 2 条指令，IPC 1.50+

**文件**:
- 新增：`include/riscv/superscalar_core.h` (~500 行)
- 新增：`include/riscv/dual_issue.h` (~200 行)
- 测试：`tests/riscv/test_superscalar.cpp` (~150 行)

**工时**: 5 工作日

**前提条件**:
- P0 全部完成
- IPC 1.15+ 已达成
- 时间充裕

**关键挑战**:
- 指令间依赖检测复杂
- 发射宽度增加导致时序难收敛
- 与现有流水线兼容

---

### 3.2 T612: 基础乱序执行

**目标**: Scoreboard + 保留站，IPC 1.30~1.40

**文件**:
- 新增：`include/riscv/scoreboard.h` (~300 行)
- 新增：`include/riscv/reservation_station.h` (~250 行)
- 新增：`include/riscv/reorder_buffer.h` (~200 行)

**工时**: 5 工作日

**前提条件**: 同 T611

---

### 3.3 T613: BTB 增强

**目标**: 64 条目 BTB + RAS (Return Address Stack)

**文件**:
- 新增：`include/riscv/btb_enhanced.h` (~150 行)

**工时**: 2 工作日

**收益**: +0.02 IPC (分支预测)

---

## 4. 测试策略

### 4.1 单元测试

每个模块必须配备单元测试：

| 模块 | 测试文件 | 覆盖率要求 |
|------|---------|-----------|
| Tournament 预测器 | `test_branch_predictor_tournament.cpp` | >95% |
| 自适应预取器 | `test_adaptive_prefetch.cpp` | >90% |
| Write Buffer 增强 | `test_write_buffer_enhanced.cpp` | >90% |
| L2 Cache | `test_l2_cache.cpp` | >95% |

**测试框架**: Catch2 (Phase 5 继承)

### 4.2 集成测试

Phase 6 集成测试覆盖：

1. **流水线集成**: 5 级流水线与新增模块集成
2. **Cache 层次**: L1→L2→ 内存层次验证
3. **分支预测**: Tournament 与流水线集成
4. **预取集成**: I-Cache 预取与 L2 集成

**测试文件**: `tests/riscv/test_phase6_integration.cpp`

### 4.3 回归测试

Phase 5 所有测试必须 100% 通过：

```bash
ctest -R "phase5" --output-on-failure
# 预期：100% tests passed
```

---

## 5. 性能基准测试计划

### 5.1 基准测试框架

继承 Phase 5 框架：`tests/benchmark/test_phase4_coremark.cpp`

**新增计数器**:
- L2 Cache 命中率
- 预取命中率
- Write Buffer 利用率

### 5.2 测试负载

| 负载 | 说明 | Phase 5 基线 | Phase 6 目标 |
|------|------|-------------|-----------|
| CoreMark | EEMBC CoreMark | 0.90 IPC | **1.20+ IPC** |
| Dhrystone | 整数基准 | 0.85 IPC | 1.10+ IPC |
| Matrix Mult | 内存密集型 | 0.70 IPC | 1.00+ IPC |
| Branch Heavy | 分支密集型 | 0.80 IPC | 1.10+ IPC |

### 5.3 性能报告版本

| 版本 | 触发条件 | 目标 IPC |
|------|---------|---------|
| v3.0 | P0 完成 | 1.15+ |
| v3.5 | P0+P1 部分完成 | 1.30+ |
| v4.0 | P0+P1 完成 | 1.50+ |

---

## 6. 风险管理

### 6.1 技术风险

| 风险 | 概率 | 影响 | 缓解措施 | 负责人 |
|------|------|------|----------|--------|
| L2 Cache 时序难收敛 | 中 | 高 | 先实现 4-way 简化版 | AI Agent |
| Tournament 准确率低 | 低 | 中 | 增加 PHT 大小至 2048 | AI Agent |
| 预取器误检率高 | 中 | 中 | 增加置信度阈值 | AI Agent |
| Write Buffer 死锁 | 低 | 高 | 严格验证仲裁逻辑 | AI Agent |

### 6.2 进度风险

| 风险 | 概率 | 影响 | 缓解措施 |
|------|------|------|----------|
| P0 延期 | 中 | 中 | 周度评审，及时调整 |
| P1 无法完成 | 高 | 低 | 降级为可选，优先保 P0 |
| 性能不达标 | 低 | 中 | 迭代优化，增加 P0 特性 |

### 6.3 应对策略

**风险触发时的行动**:
1. L2 Cache 时序问题 →降级为 4-way, 128KB
2. 性能不达标 → 增加预取深度，优化 Write Buffer
3. 进度延期 → 削减 P1 任务，优先保 P0

---

## 7. 交付清单

### 7.1 核心代码

| 文件 | 行数 | 状态 |
|------|------|------|
| `include/riscv/branch_predictor_tournament.h` | ~300 | 新增 |
| `include/chlib/i_cache_adaptive_prefetch.h` | ~200 | 新增 |
| `include/chlib/d_cache_write_buffer_enhanced.h` | ~220 | 新增 |
| `include/chlib/l2_cache.h` | ~400 | 新增 |
| `include/riscv/superscalar_core.h` | ~500 | 可选 |
| `include/riscv/scoreboard.h` | ~300 | 可选 |

**总计**: ~1620 行 (P0) + ~500 行 (P1 可选)

### 7.2 测试代码

| 文件 | 行数 | 覆盖模块 |
|------|------|---------|
| `tests/riscv/test_branch_predictor_tournament.cpp` | ~100 | T601 |
| `tests/chlib/test_adaptive_prefetch.cpp` | ~80 | T602 |
| `tests/chlib/test_write_buffer_enhanced.cpp` | ~80 | T603 |
| `tests/chlib/test_l2_cache.cpp` | ~150 | T604 |
| `tests/riscv/test_phase6_integration.cpp` | ~200 | 集成测试 |

**总计**: ~610 行

### 7.3 文档

| 文件 | 页数 | 类型 |
|------|------|------|
| `docs/architecture/PHASE6-ARCHITECTURE.md` | ~15 | 架构设计 |
| `docs/plans/PHASE6-IMPLEMENTATION-PLAN.md` | ~12 | 实施计划 |
| `docs/architecture/tournament-branch-predictor.md` | ~5 | 技术文档 |
| `docs/architecture/adaptive-prefetcher.md` | ~4 | 技术文档 |
| `docs/architecture/l2-cache-design.md` | ~6 | 技术文档 |
| `docs/reports/PHASE6-PERFORMANCE-BENCHMARK-v3.0.md` | ~8 | 性能报告 |
| `docs/reports/PHASE6-PERFORMANCE-BENCHMARK-v4.0.md` | ~8 | 性能报告 |

**总计**: ~58 页

### 7.4 Git 提交

预计提交：
```
feat: Implement Tournament Branch Predictor (T601)
feat: Implement Adaptive Prefetcher (T602)
feat: Enhance Write Buffer to 8 entries (T603)
feat: Implement L2 Cache 256KB (T604)
test: Add Phase 6 integration tests
docs: Add Phase 6 architecture documentation
docs: Add Phase 6 performance benchmark v3.0
docs: Add Phase 6 performance benchmark v4.0 (optional)
```

**预计**: 8-12 commits

---

## 8. 工具与环境

### 8.1 构建要求

```bash
# CMake 配置
cmake -DCMAKE_BUILD_TYPE=Release ..

# 编译
cmake --build build/ -j$(nproc)

# 测试
ctest -R "phase6" --output-on-failure

# 性能测试
./build/benchmark_coremark
```

### 8.2 LSP 要求

- clangd 版本：>= 14.0
- compile_commands.json: 必须存在
- C++ 标准：C++17

### 8.3 代码风格

- 缩进：2 空格
- 命名：CamelCase (类), snake_case (变量)
- 注释：中文，Doxygen 格式

---

## 9. 审批与评审

### 9.1 周度评审

| 周次 | 日期 | 评审内容 | 验收标准 |
|------|------|---------|---------|
| Week 1 | 2026-04-16 | T601, T602 | 编译通过，单元测试通过 |
| Week 2 | 2026-04-23 | T603, T604 | 编译通过，集成测试通过 |
| Week 3 | 2026-04-30 | 性能测试 v3.0 | IPC 1.15+ |
| Week 4 | 2026-05-30 | Phase 6 结项 | IPC 1.20+, 文档完整 |

### 9.2 里程碑评审

| 里程碑 | 日期 | 决策项 |
|--------|------|--------|
| M11: P0 完成 | 2026-05-14 | 是否启动 P1 |
| M12: 性能测试 v3.0 | 2026-05-16 | 性能目标达成情况 |
| M13: P1 完成 | 2026-05-23 | (可选) P1 验收 |
| M15: Phase 6 结项 | 2026-05-30 | Phase 6 最终验收 |

---

**创建人**: AI Agent  
**创建日期**: 2026-04-10  
**状态**: 🟡 待启动  
**下次更新**: Week 1 结束 (2026-04-16)  
**版本历史**: v1.0 (初始版本)
