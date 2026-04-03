# Phase 4 规划 - 性能优化与功能完善

**创建日期**: 2026-04-01  
**阶段目标**: 提升 RV32I 性能和功能完整性  
**预计工期**: 2 周 (2026-04-02 ~ 2026-04-15)  
**当前状态**: 🟡 规划中

---

## 📊 Phase 3 回顾

### 完成情况

| 阶段 | 任务 | 状态 |
|------|------|------|
| Phase 3A | AXI4 总线系统 | ✅ 100% |
| Phase 3B | RISC-V 处理器 | ✅ 100% |
| Phase 3C | SoC 集成 | ✅ 100% |
| Phase 3D | 高级功能 | ⏳ 0% (可选) |

### 核心成就

- ✅ RV32I 核心 (40 条指令)
- ✅ AXI4 4x4 Interconnect
- ✅ 完整 SoC 集成
- ✅ Hello World 示例

### 待改进

- ⚠️ 单周期处理器 (性能低)
- ⚠️ 无缓存支持
- ⚠️ 无分支预测
- ⚠️ Plugin 架构缺失

---

## 🎯 Phase 4 目标

### 性能目标

| 指标 | Phase 3 | Phase 4 目标 | 提升 |
|------|---------|-----------|------|
| IPC | ~0.5 | ~0.8 | +60% |
| 频率 | 50MHz | 100MHz | +100% |
| 缓存命中率 | 0% | >90% | - |
| 分支预测准确率 | 50% | >85% | +70% |

### 功能目标

- [ ] 5 级流水线 (IF/ID/EX/MEM/WB)
- [ ] I-Cache (4KB, 2 路组相联)
- [ ] D-Cache (4KB, 2 路组相联)
- [ ] 静态分支预测
- [ ] Plugin 架构
- [ ] 硬件除法器
- [ ] CSR 指令支持

---

## 📋 任务分解

### T401: 5 级流水线实现 (3 天)

**目标**: 将单周期处理器改造为 5 级流水线

| 子任务 | 工时 | 说明 |
|--------|------|------|
| T401.1: IF 级 (取指) | 4h | 指令缓存接口 |
| T401.2: ID 级 (译码) | 4h | 指令译码 + 寄存器读 |
| T401.3: EX 级 (执行) | 8h | ALU + 分支计算 |
| T401.4: MEM 级 (访存) | 4h | 数据缓存接口 |
| T401.5: WB 级 (写回) | 4h | 寄存器写回 |
| T401.6: 冒险检测 | 8h | 数据/控制冒险处理 |

**交付物**:
- `include/riscv/rv32i_pipeline.h`
- `include/riscv/rv32i_hazard.h`
- `examples/riscv/rv32i_pipeline_test.cpp`

**验收标准**:
- 5 级流水线完整实现
- 数据冒险检测正确
- 控制冒险处理正确
- 性能提升 >50%

---

### T402: I-Cache 实现 (2 天)

**目标**: 实现 4KB 指令缓存

| 子任务 | 工时 | 说明 |
|--------|------|------|
| T402.1: Cache 控制器 | 4h | 状态机 + 标签比较 |
| T402.2: 2 路组相联 | 4h | LRU 替换策略 |
| T402.3: AXI4 接口 | 4h | 总线协议适配 |
| T402.4: 性能测试 | 4h | 命中率统计 |

**规格**:
- 容量：4KB
- 组相联：2 路
- 行大小：16 字节
- 替换策略：LRU

**交付物**:
- `include/riscv/icache.h`
- `tests/test_icache.cpp`

---

### T403: D-Cache 实现 (2 天)

**目标**: 实现 4KB 数据缓存

| 子任务 | 工时 | 说明 |
|--------|------|------|
| T403.1: Cache 控制器 | 4h | 状态机 + 标签比较 |
| T403.2: Write-Back 策略 | 4h | 写回 + 脏位管理 |
| T403.3: 一致性维护 | 4h | 缓存一致性 |
| T403.4: 性能测试 | 4h | 命中率统计 |

**规格**:
- 容量：4KB
- 组相联：2 路
- 行大小：16 字节
- 写策略：Write-Back

**交付物**:
- `include/riscv/dcache.h`
- `tests/test_dcache.cpp`

---

### T404: 分支预测 (2 天)

**目标**: 实现静态分支预测

| 子任务 | 工时 | 说明 |
|--------|------|------|
| T404.1: 静态预测 | 4h | BTFN (Backward Taken, Forward Not) |
| T404.2: 动态预测 (可选) | 8h | 2-bit 饱和计数器 |
| T404.3: BTB (可选) | 8h | 分支目标缓冲 |

**规格**:
- 静态预测：BTFN 策略
- 动态预测 (可选): 2-bit 计数器
- BTB (可选): 16 条目

**交付物**:
- `include/riscv/branch_predictor.h`
- `tests/test_branch_predictor.cpp`

---

### T405: Plugin 架构 (3 天)

**目标**: 实现模块化 Plugin 系统

| 子任务 | 工时 | 说明 |
|--------|------|------|
| T405.1: Plugin 接口 | 4h | 定义 Plugin 基类 |
| T405.2: CPU 集成点 | 8h | 添加 Plugin 注入点 |
| T405.3: 示例 Plugin | 8h | 乘法器/除法器 Plugin |
| T405.4: 文档 | 4h | Plugin 开发指南 |

**Plugin 类型**:
- 协处理器 Plugin (乘法器/除法器)
- 调试 Plugin (断点/单步)
- 性能监控 Plugin (计数器)
- 自定义指令 Plugin

**交付物**:
- `include/riscv/cpu_plugin.h`
- `plugins/mul_div_plugin.h`
- `docs/plugin_development_guide.md`

---

### T406: 硬件除法器 (1 天)

**目标**: 实现硬件除法/取模

| 子任务 | 工时 | 说明 |
|--------|------|------|
| T406.1: 除法算法 | 4h | 恢复余数法 |
| T406.2: 集成到 ALU | 4h | ALU 扩展 |

**规格**:
- 支持：DIV, DIVU, REM, REMU
- 延迟：32 周期
- 吞吐：1 指令/周期 (流水线)

**交付物**:
- `include/riscv/divider.h`

---

### T407: CSR 指令支持 (1 天)

**目标**: 实现 CSR 读写指令

| 子任务 | 工时 | 说明 |
|--------|------|------|
| T407.1: CSR 寄存器文件 | 4h | mstatus, mie, mtvec 等 |
| T407.2: CSR 指令译码 | 4h | CSRRW, CSRRS, CSRRC |

**规格**:
- 支持 CSR：mstatus, mie, mtvec, mepc, mcause
- 指令：CSRRW, CSRRS, CSRRC, CSRRWI, CSRRSI, CSRRCI

**交付物**:
- `include/riscv/csr.h`

---

## 📅 执行计划

### Week 1 (2026-04-02 ~ 2026-04-08)

| 日期 | 任务 | 交付物 |
|------|------|--------|
| Thu 04-02 | T401: 5 级流水线 (IF/ID) | `rv32i_pipeline.h` (部分) |
| Fri 04-03 | T401: 5 级流水线 (EX/MEM/WB) | `rv32i_pipeline.h` (完整) |
| Sat 04-04 | T401: 冒险检测 | `rv32i_hazard.h` |
| Sun 04-05 | T402: I-Cache | `icache.h` |
| Mon 04-06 | T403: D-Cache | `dcache.h` |
| Tue 04-07 | T404: 分支预测 | `branch_predictor.h` |
| Wed 04-08 | 中期评审 | Phase 4 中期报告 |

### Week 2 (2026-04-09 ~ 2026-04-15)

| 日期 | 任务 | 交付物 |
|------|------|--------|
| Thu 04-09 | T405: Plugin 架构 | `cpu_plugin.h` |
| Fri 04-10 | T405: 示例 Plugin | `mul_div_plugin.h` |
| Sat 04-11 | T406: 硬件除法器 | `divider.h` |
| Sun 04-12 | T407: CSR 指令 | `csr.h` |
| Mon 04-13 | 集成测试 | 完整系统验证 |
| Tue 04-14 | 性能测试 | 性能报告 |
| Wed 04-15 | Phase 4 评审 | Phase 4 完成报告 |

---

## 📊 验收标准

### 性能指标

| 指标 | 目标 | 测量方法 |
|------|------|---------|
| IPC | >0.8 | CoreMark 基准测试 |
| 频率 | >100MHz | FPGA 综合报告 |
| 缓存命中率 | >90% | 性能计数器 |
| 分支预测准确率 | >85% | 性能计数器 |

### 功能指标

| 功能 | 验收标准 |
|------|---------|
| 5 级流水线 | 无冒险错误 |
| I-Cache | 命中率 >90% |
| D-Cache | Write-Back 正确 |
| 分支预测 | 静态预测正确 |
| Plugin 架构 | 可加载/卸载 Plugin |
| 硬件除法 | DIV/REM 指令正确 |
| CSR 指令 | 中断正确处理 |

---

## 🔧 技术要点

### 5 级流水线设计

```
┌─────┬─────┬─────┬─────┬─────┐
│ IF  │ ID  │ EX  │ MEM │ WB  │
├─────┼─────┼─────┼─────┼─────┤
│取指 │译码 │执行 │访存 │写回 │
│PC+4 │读寄存器│ALU│Cache│写寄存器│
└─────┴─────┴─────┴─────┴─────┘
```

### 冒险处理

**数据冒险**:
- Forwarding (旁路)
- Stall (插入气泡)

**控制冒险**:
- 分支预测
- Flush (清空流水线)

### 缓存设计

```
┌─────────────────────────────────┐
│           Cache                 │
├─────────┬─────────┬─────────────┤
│  Tag    │  Index  │   Offset    │
│ 20-bit  │  8-bit  │   4-bit     │
└─────────┴─────────┴─────────────┘
```

---

## 📈 风险评估

| 风险 | 概率 | 影响 | 缓解措施 |
|------|------|------|---------|
| 流水线冒险复杂 | 中 | 高 | 先实现简单 Forwarding |
| 缓存一致性难 | 高 | 中 | 使用 Write-Through 简化 |
| 进度延期 | 中 | 中 | 优先完成核心功能 |
| 性能不达标 | 低 | 中 | 调整优化策略 |

---

## 📝 参考文档

- [RISC-V Privileged Specification](https://riscv.org/specifications/privileged-isa/)
- [ARM Cortex-M3 Technical Reference Manual](https://developer.arm.com/documentation/ddi0337/latest/)
- [Stanford CS152: Computer Architecture](https://cs152.org/)

---

**创建人**: DevMate  
**创建日期**: 2026-04-01  
**下次更新**: 2026-04-02 (Phase 4 启动)
