# RISC-V Mini 性能测试计划

**文档版本**: v1.0
**创建日期**: 2026-04-24
**状态**: 规划中
**负责人**: Sisyphus

---

## 1. 背景与目标

### 1.1 当前测试状态

| 测试文件 | 覆盖 | 问题 |
|---------|------|------|
| `test_riscv_tests.cpp` | 45+ RV32UI 测试 | ✅ 完整 (解释器) |
| `test_riscv_tests_pipeline.cpp` | **仅 3 个测试** | ❌ 不完整 |
| 其他 debug 测试 | 烟雾测试 | ❌ 无性能数据 |

**关键缺陷**:
1. Pipeline 硬件测试只验证了正确性 (PASS/FAIL)
2. 没有跟踪 cycle count、IPC、CPI 等性能指标
3. 没有 stall 计数、branch misprediction penalty 等分析数据
4. 没有与参考解释器的对比基准

### 1.2 性能测试目标

```
目标: 为 riscv-mini 建立完整的性能基准测试体系

指标:
├── CPI (Cycles Per Instruction)     # 核心指标
├── IPC (Instructions Per Cycle)    # CPI 的倒数
├── 分支预测准确率                   # 反映分支预测器效果
├── Load-Use stall 率               # 反映 hazard 检测效果
├── 流水线效率                       # actual CPI / ideal CPI (1.0)
└── Reference 对比                   # vs 解释器 baseline
```

---

## 2. 测试架构

### 2.1 性能计数器设计

```cpp
struct PipelineMetrics {
    uint64_t total_cycles;           // 总时钟周期
    uint64_t total_instructions;    // 总指令数 (commit count)

    // 分支统计
    uint64_t branch_count;          // 分支指令数
    uint64_t branch_mispredict;    // 分支预测错误数

    // Stall 统计
    uint64_t load_use_stalls;       // Load-Use hazard stalls
    uint64_t control_stalls;        // 控制冒险 stalls (branches/jumps)

    // 计算的指标
    double cpi() const { return (double)total_cycles / total_instructions; }
    double ipc() const { return (double)total_instructions / total_cycles; }
    double branch_accuracy() const { return 1.0 - (double)branch_mispredict / branch_count; }
};
```

### 2.2 流水线埋点方案

在 `rv32i_pipeline.h` 中添加性能计数器埋点:

```cpp
class Rv32iPipeline : public ch::Component {
    // ... existing IO ...

    // 性能计数器输出 (用于测试环境观测)
    __io(
        ch_out<ch_uint<48>> perf_cycles;        // 48-bit cycle counter
        ch_out<ch_uint<48>> perf_instructions;  // commit count
        ch_out<ch_uint<32>> perf_branch_count;
        ch_out<ch_uint<32>> perf_branch_mispredict;
        ch_out<ch_uint<32>> perf_load_use_stalls;
    )

    // 内部计数器
    ch_reg<ch_uint<48>> cycle_count(0_d, "cycle_count");
    ch_reg<ch_uint<48>> commit_count(0_d, "commit_count");
    ch_reg<ch_uint<32>> branch_count(0_d, "branch_count");
    ch_reg<ch_uint<32>> branch_mispredict(0_d, "branch_mispredict");
    ch_reg<ch_uint<32>> load_use_stalls(0_d, "load_use_stalls");
```

### 2.3 计数器更新逻辑

在流水线各阶段正确位置更新:

```cpp
// IF 阶段: cycle counter always increments
cycle_count->next = cycle_count + 1;

// ID 阶段: count branches when decoded
branch_count->next = branch_count + id_stage.io().is_branch;

// EX 阶段: branch misprediction detected
// (flush signal indicates misprediction)
branch_mispredict->next = branch_mispredict +
    (ex_stage.io().branch_taken & if_stage.io().valid & hazard.io().flush);

// MEM 阶段: load-use stall
load_use_stalls->next = load_use_stalls + hazard.io().stall;

// WB 阶段: commit one instruction
commit_count->next = commit_count + wb_stage.io().mem_valid_out;
```

---

## 3. 测试用例设计

### 3.1 测试分组

| 组别 | 测试数量 | 目的 |
|------|---------|------|
| **RV32UI Complete** | 45 | 完整 ISA 覆盖 |
| **Arithmetic Intensive** | 8 | add, sub, mul, div, etc. |
| **Memory Intensive** | 8 | load/store patterns |
| **Control Intensive** | 10 | branch/jump heavy |
| **Mix Benchmark** | 5 | representative workloads |

### 3.2 必需的基准测试

```
RV32UI 完整列表:
├── add, addi, sub, lui, auipc
├── and, andi, or, ori, xor, xori
├── sll, slli, srl, srli, sra, srai
├── slt, slti, sltu, sltiu
├── beq, bne, blt, bge, bltu, bgeu
├── jal, jalr
├── lb, lh, lw, lbu, lhu
├── sb, sh, sw
├── fence_i, ma_data, simple, st_ld
```

---

## 4. 实现任务

### 4.1 任务分解

| Task ID | 任务名称 | 工作量 | 依赖 |
|---------|---------|-------|------|
| **B001** | 添加 Pipeline 性能计数器 IO | 1d | - |
| **B002** | 实现计数器更新逻辑 | 2d | B001 |
| **B003** | 重构 test_riscv_tests_pipeline.cpp | 2d | B002 |
| **B004** | 添加所有 45+ RV32UI 测试用例 | 1d | B003 |
| **B005** | 实现 CPI/IPC 计算报告 | 1d | B004 |
| **B006** | 添加 Reference 对比 (解释器 baseline) | 1d | B005 |
| **B007** | 生成性能测试报告格式 | 1d | B006 |

**总计**: 9 工作日

### 4.2 详细实现计划

#### B001: 添加性能计数器 IO

**文件**: `include/cpu/pipeline/rv32i_pipeline.h`

```cpp
__io(
    // ... existing IO ...

    // Performance counter outputs
    ch_out<ch_uint<48>> perf_cycles;
    ch_out<ch_uint<48>> perf_instructions;
    ch_out<ch_uint<32>> perf_branch_count;
    ch_out<ch_uint<32>> perf_branch_mispredict;
    ch_out<ch_uint<32>> perf_load_use_stalls;
    ch_out<ch_uint<32>> perf_control_stalls;
)
```

#### B002: 实现计数器更新逻辑

在 `describe()` 中添加:

```cpp
// Cycle counter - always increments
cycle_count->next = cycle_count + 1;

// Instruction commit counter (WB stage)
auto commit = wb_stage.io().mem_valid_out;
commit_count->next = commit_count + commit;

// Branch counter (ID stage sees branch instruction)
branch_count->next = branch_count + id_stage.io().is_branch;

// Branch misprediction (EX stage detects, causes flush)
branch_mispredict->next = branch_mispredict +
    (ex_stage.io().branch_taken & if_stage.io().valid & hazard.io().flush);

// Load-use stall counter (HazardUnit stall signal)
load_use_stalls->next = load_use_stalls + hazard.io().stall;

// Connect to outputs
io().perf_cycles <<= cycle_count;
io().perf_instructions <<= commit_count;
// ... etc
```

#### B003: 重构测试框架

**新文件**: `examples/riscv-mini/tests/test_pipeline_benchmark.cpp`

```cpp
struct BenchmarkResult {
    std::string test_name;
    uint64_t cycles;
    uint64_t instructions;
    uint64_t branch_count;
    uint64_t branch_mispredict;
    uint64_t load_use_stalls;

    double cpi() const { return (double)cycles / instructions; }
    double ipc() const { return (double)instructions / cycles; }
    double branch_accuracy() const {
        return branch_count > 0 ? 1.0 - (double)branch_mispredict / branch_count : 1.0;
    }
};

bool run_benchmark(const std::string& test_name, const std::string& bin_path,
                    BenchmarkResult& result) {
    // ... 加载 binary 到 ITCM ...

    // 重置性能计数器
    sim.set_input_value(top.instance().io().rst, true);
    sim.tick();
    sim.set_input_value(top.instance().io().rst, false);

    // 运行直到完成
    while (!complete && tick < MAX_TICKS) {
        sim.tick();
        tick++;
    }

    // 读取性能计数器
    result.cycles = sim.get_port_value(top.instance().io().perf_cycles);
    result.instructions = sim.get_port_value(top.instance().io().perf_instructions);
    result.branch_count = sim.get_port_value(top.instance().io().perf_branch_count);
    result.branch_mispredict = sim.get_port_value(top.instance().io().perf_branch_mispredict);
    result.load_use_stalls = sim.get_port_value(top.instance().io().perf_load_use_stalls);
    result.test_name = test_name;

    return complete;
}
```

#### B004: 添加所有 RV32UI 测试

扩展 `TEST_CASE_FOR_each` 宏调用所有 45+ 测试。

#### B005: CPI/IPC 计算报告

```cpp
void print_benchmark_report(const std::vector<BenchmarkResult>& results) {
    std::cout << "\n=== RISC-V Mini Pipeline Benchmark Report ===\n\n";

    // 汇总统计
    uint64_t total_cycles = 0;
    uint64_t total_instructions = 0;
    for (const auto& r : results) {
        total_cycles += r.cycles;
        total_instructions += r.instructions;
    }

    double avg_cpi = (double)total_cycles / total_instructions;
    double avg_ipc = (double)total_instructions / total_cycles;

    std::cout << "Aggregate Results:\n";
    std::cout << "  Total Cycles:      " << total_cycles << "\n";
    std::cout << "  Total Instructions:" << total_instructions << "\n";
    std::cout << "  Average CPI:       " << avg_cpi << "\n";
    std::cout << "  Average IPC:       " << avg_ipc << "\n";
    std::cout << "  Ideal CPI:         1.0\n";
    std::cout << "  Pipeline Efficiency:" << (avg_cpi / 1.0 * 100) << "%\n\n";

    // Per-test breakdown
    std::cout << "Per-Test Results:\n";
    std::cout << std::setw(20) << "Test"
              << std::setw(12) << "Cycles"
              << std::setw(12) << "Instrs"
              << std::setw(10) << "CPI"
              << std::setw(10) << "IPC"
              << std::setw(12) << "Br Mispred %\n";
    std::cout << std::string(76, '-') << "\n";

    for (const auto& r : results) {
        std::cout << std::setw(20) << r.test_name
                  << std::setw(12) << r.cycles
                  << std::setw(12) << r.instructions
                  << std::setw(10) << std::fixed << std::setprecision(3) << r.cpi()
                  << std::setw(10) << std::fixed << std::setprecision(3) << r.ipc()
                  << std::setw(12) << std::fixed << std::setprecision(2)
                  << (r.branch_count > 0 ? (double)r.branch_mispredict / r.branch_count * 100 : 0)
                  << "\n";
    }
}
```

#### B006: Reference 对比 (解释器 Baseline)

```cpp
struct ReferenceResult {
    uint64_t cycles;  // 解释器 cycle count = instruction count (no stalls)
    uint64_t instructions;
};

ReferenceResult run_reference_interpreter(const std::string& bin_path) {
    Rv32iInterpreter interp;
    interp.load_binary(bin_path);

    uint64_t cycles = 0;
    while (!interp.tohost_written && cycles < MAX_TICKS) {
        interp.step();
        cycles++;
    }

    return {cycles, interp.get_instruction_count()};
}

void compare_with_reference(BenchmarkResult& hw, ReferenceResult& ref) {
    double overhead = (double)hw.cycles / ref.cycles;
    std::cout << "Reference cycles: " << ref.cycles
              << ", HW cycles: " << hw.cycles
              << ", Overhead: " << overhead << "x\n";
}
```

---

## 5. 测试输出格式

### 5.1 终端输出示例

```
=== RISC-V Mini Pipeline Benchmark Report ===

Aggregate Results:
  Total Cycles:      1254320
  Total Instructions: 987654
  Average CPI:        1.270
  Average IPC:        0.787
  Ideal CPI:         1.0
  Pipeline Efficiency: 78.7%

Per-Test Results:
               Test      Cycles      Instrs        CPI        IPC   Br Mispred %
----------------------------------------------------------------------------
               add        1234        1000      1.234      0.810          0.00%
              addi        1189         980      1.213      0.824          0.00%
               sub        1245        1020      1.220      0.819          0.00%
           branch_eq        1567        800      1.959      0.511         12.50%
              jalr        1890         720      2.625      0.381         15.28%
                lw        2134         890      2.397      0.417          0.00%
               sw        1987         920      2.160      0.463          0.00%
           ... (more tests)

Reference Comparison (Pipeline vs Interpreter):
  add:        HW=1234, Ref=1000, Overhead=1.23x
  addi:       HW=1189, Ref=980, Overhead=1.21x
  branch_eq:  HW=1567, Ref=800, Overhead=1.96x (branch penalty)
  jalr:       HW=1890, Ref=720, Overhead=2.63x (control hazard)
```

### 5.2 CTest 标签

```
[benchmark]      - 所有 benchmark 测试
[benchmark:isa]  - RV32UI 完整测试
[benchmark:mix]  - 混合负载测试
[benchmark:cpi]  - CPI 测试
[benchmark:ref]  - Reference 对比测试
```

---

## 6. 验收标准

### 6.1 功能验收

| 验收条件 | 验证方法 |
|---------|---------|
| 所有 45+ RV32UI 测试可运行 | `ctest -L benchmark:isa` 全部 PASS |
| 性能计数器正确更新 | 对比手动计算的 CPI vs 报告的 CPI |
| Reference 对比正常工作 | 解释器 baseline 与 pipeline 结果对比 |

### 6.2 性能指标目标

| 指标 | 当前值 | 目标值 | 说明 |
|------|-------|-------|------|
| Average CPI | N/A | < 1.5 | 理想 CPI=1.0 (无 stall) |
| Average IPC | N/A | > 0.66 | CPI 的倒数 |
| Branch Accuracy | N/A | > 85% | 分支预测准确率 |
| Load-Use Stall Rate | N/A | < 5% | Load 指令导致的 stall |

### 6.3 测试完整性检查

```bash
# 检查 benchmark 覆盖率
ctest -L benchmark -N  # 应显示 50+ 测试

# 检查所有测试通过
ctest -L benchmark --output-on-failure

# 检查性能报告生成
./build/examples/riscv-mini/test_pipeline_benchmark
```

---

## 7. 实施时间线

```
Week 1 (Day 1-5):
  Day 1-2: B001 + B002 (性能计数器 IO + 更新逻辑)
  Day 3-5: B003 (重构测试框架)

Week 2 (Day 6-10):
  Day 6-7: B004 (添加所有 45+ 测试)
  Day 8-9: B005 (CPI/IPC 报告)
  Day 10: B006 (Reference 对比)

Week 3 (Day 11-13):
  Day 11-12: B007 (报告格式美化)
  Day 13: 集成测试 + 验收
```

---

## 8. 风险与对策

| 风险 | 影响 | 对策 |
|------|-----|------|
| 性能计数器影响时序 | 可能改变 pipeline 行为 | 使用组合逻辑输出，不添加寄存器 |
| 45+ 测试运行时间长 | CI 超时 | 添加 `[benchmark:quick]` 标签选跑 5 个快速测试 |
| Reference 解释器不准确 | 对比结果无意义 | 使用 riscv-tests 官方 ISA golden reference |

---

## 9. 相关文档

| 文档 | 路径 |
|------|------|
| riscv-mini AGENTS.md | `examples/riscv-mini/AGENTS.md` |
| Pipeline 架构 | `include/cpu/pipeline/rv32i_pipeline.h` |
| 现有测试 | `examples/riscv-mini/tests/test_riscv_tests_pipeline.cpp` |
| Phase 6 计划 | `docs/plans/PHASE6-EXEC-SUMMARY.md` |
| 性能优化路线图 | `docs/plans/PHASE6-IMPLEMENTATION-PLAN.md` |

---

**文档状态**: 实施中
**更新日期**: 2026-04-24
**下一步**: B005/B006/B007 收尾 + 性能优化