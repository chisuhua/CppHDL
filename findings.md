# Findings: RISC-V Mini Benchmark Implementation

**创建日期**: 2026-04-24

---

## 1. 当前测试状态

### test_riscv_tests.cpp (解释器)
- 45+ RV32UI 测试
- 完整覆盖 ISA
- 没有性能数据

### test_riscv_tests_pipeline.cpp (硬件)
- 仅 3 个测试 (simple, add, addi)
- 只验证 PASS/FAIL
- 没有性能指标

---

## 2. 性能计数器设计决策

### 决策: 使用 ch_reg 作为内部计数器
原因: 与现有 pipeline 模式一致，可以利用现有的流水线寄存器机制

### 决策: 48-bit cycle counter, 32-bit event counters
原因: 48-bit 支持长时间运行，32-bit 对事件计数足够

### 决策: 性能计数器作为 pipeline IO 输出
原因: 测试环境需要通过 sim.get_port_value() 读取

---

## 3. 关键文件

| 文件 | 用途 |
|------|------|
| `include/cpu/pipeline/rv32i_pipeline.h` | 需要修改 - 添加性能计数器 |
| `examples/riscv-mini/tests/test_riscv_tests_pipeline.cpp` | 需要修改 - 添加测试 |
| `examples/riscv-mini/tests/test_riscv_tests.cpp` | 参考 - 解释器 baseline |

---

## 4. 测试分组

| 组别 | 测试数量 | 目的 |
|------|---------|------|
| RV32UI Complete | 45 | 完整 ISA 覆盖 |
| Arithmetic Intensive | 8 | add, sub, etc. |
| Memory Intensive | 8 | load/store patterns |
| Control Intensive | 10 | branch/jump heavy |

---

## 5. 已知风险

1. **性能计数器影响时序**: 使用组合逻辑输出，不添加额外寄存器
2. **测试运行时间长**: 添加 `[benchmark:quick]` 标签
3. **解释器参考可能不准**: riscv-tests 官方 ISA golden reference

---

## 6. 依赖关系

```
B001 (性能计数器 IO) → B002 (计数器逻辑) → B003 (测试框架)
                                                            ↓
B004 (45+ 测试) ←←←←←←←←←←←←←←←←←←←←←←←←←←←←←←←←←←←←←←←←←←←←←
                       ↓
                 B005 (CPI/IPC 报告)
                       ↓
                 B006 (Reference 对比)
                       ↓
                 B007 (报告格式)
```