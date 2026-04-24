# Task Plan: RISC-V Mini 性能测试实现

**项目**: CppHDL riscv-mini benchmark
**创建日期**: 2026-04-24
**计划文件**: `/workspace/project/CppHDL/task_plan.md`

---

## 目标

为 riscv-mini 建立完整的性能基准测试体系，包括:
- CPI/IPC 指标
- 分支预测准确率
- Load-Use stall 率
- Reference 对比

---

## 阶段 (Phases)

### Phase 1: B001 + B002 - 性能计数器基础设施
**状态**: `completed`
**工作量**: 3 days
**交付物**:
- `include/cpu/pipeline/rv32i_pipeline.h` - 添加性能计数器 IO
- 计数器更新逻辑

**任务**:
- [x] B001: 添加 Pipeline 性能计数器 IO
- [x] B002: 实现计数器更新逻辑

---

### Phase 2: B003 - 测试框架重构
**状态**: `completed`
**工作量**: 2 days
**交付物**: `examples/riscv-mini/tests/test_riscv_tests_pipeline.cpp`

**任务**:
- [x] B003: 重构测试框架

---

### Phase 3: B004 - 完整测试覆盖
**状态**: `completed`
**工作量**: 1 day
**交付物**: 42 个 RV32UI 测试用例

**任务**:
- [x] B004: 添加所有 42 个 RV32UI 测试用例

---

### Phase 4: B005 + B006 + B007 - 报告与对比
**状态**: `completed`
**工作量**: 3 days
**交付物**: 性能报告 + Reference 对比

**任务**:
- [x] B005: 实现 CPI/IPC 计算报告
- [x] B006: 添加 Reference 对比 (ReferenceInterpreter)
- [x] B007: 生成性能测试报告格式

---

## 验收标准

| 指标 | 目标值 | 实际值 |
|------|-------|-------|
| Average CPI | < 1.5 | 待测试 |
| Average IPC | > 0.66 | 待测试 |
| Branch Accuracy | > 85% | 待测试 |
| Test Coverage | 45+ RV32UI tests | 42 tests |

---

## 决策记录

| 日期 | 决策 | 原因 |
|------|------|------|
| 2026-04-24 | 使用 ch_reg 作为内部计数器 | 与现有 pipeline 模式一致 |
| 2026-04-24 | 使用 static_cast<uint64_t> 转换 | sdata_type 类型需要转换 |

---

## 下一步

待用户验证测试执行情况