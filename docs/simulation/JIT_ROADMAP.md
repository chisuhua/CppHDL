# JIT 编译实施路线图

**项目**: CppHDL JIT 编译实现
**创建日期**: 2026-04-27
**基于**: Metis JIT 实施计划
**总工期**: 6-10 weeks (单线程开发)

---

## 概述

JIT (Just-In-Time) 编译将 CppHDL 解释执行转为机器码执行，实现 5-10x 性能提升。

### 技术架构

```
┌─────────────────────────────────────────────────────────────┐
│                        Simulator                             │
├─────────────────────────────────────────────────────────────┤
│  ┌─────────────────┐    ┌─────────────────────────────────┐ │
│  │  Interpreted    │    │         JIT Backend             │ │
│  │  Executor       │    │  ┌─────────┐  ┌─────────────┐  │ │
│  │                 │ ←→ │  │ LLVM ORC │  │ A/B Verifier │  │ │
│  │  fallback path  │    │  │  JIT     │  │             │  │ │
│  │                 │    │  └─────────┘  └─────────────┘  │ │
│  └─────────────────┘    └─────────────────────────────────┘ │
│                              │                               │
│                    ┌─────────▼─────────┐                    │
│                    │   data_buffer[]    │                    │
│                    │   (扁平数组布局)    │                    │
│                    └───────────────────┘                    │
└─────────────────────────────────────────────────────────────┘
```

---

## Phase 0: 基础设施 (3-5 days)

### 目标
Benchmark baseline 建立，LLVM 依赖集成。

### 任务清单

| 任务 | 描述 | 交付物 |
|------|------|--------|
| P0-1 | 添加 CMake option `CH_JIT_ENABLE` (default OFF) | CMakeLists.txt 修改 |
| P0-2 | 集成 LLVM (find_package 或 FetchContent) | LLVM 可用 |
| P0-3 | 创建 benchmark 框架 `tests/benchmark/` | benchmark/ 目录 |
| P0-4 | 建立性能 baseline (perf_tests) | baseline 数据 |
| P0-5 | 验证 LLVM 连接正常 | 编译通过 |

### 验收标准

- CMake 配置通过，无 LLVM 链接错误
- benchmark 可运行并输出 baseline 数据

### CMake Option 设计

```cmake
# CH_JIT_ENABLE: 默认 OFF，JIT 必须显式启用
option(CH_JIT_ENABLE "Enable LLVM ORC JIT compilation" OFF)

if(CH_JIT_ENABLE)
    find_package(LLVM REQUIRED)
    add_compile_definitions(CH_JIT_ENABLED=1)
endif()
```

---

## Phase 1: IR 生成 (7-10 days)

### 目标
IR generation passes all existing tests。

### 任务清单

| 任务 | 描述 | 交付物 |
|------|------|--------|
| P1-1 | 实现 `JitCompiler` 类框架 | JitCompiler.h/cpp |
| P1-2 | 实现节点遍历和 IR 生成 | Node → LLVM IR |
| P1-3 | 实现基本操作码编译 | add, sub, and, or, xor |
| P1-4 | 实现寄存器分配 | 线性扫描分配器 |
| P1-5 | 实现 memory load/store 编译 | mem read/write |
| P1-6 | A/B 验证所有现有测试 | 零 mismatch |

### IR 生成流程

```
AST Node Graph
     │
     ▼
┌────────────────┐
│  Node Walker   │  遍历所有节点，建立执行顺序
└────────────────┘
     │
     ▼
┌────────────────┐
│  IR Generator  │  生成 LLVM IR 指令
└────────────────┘
     │
     ▼
┌────────────────┐
│ Optimization   │  可选：常量折叠、DCE
└────────────────┘
     │
     ▼
┌────────────────┐
│  JIT Compile   │  LLVM ORC 编译为机器码
└────────────────┘
     │
     ▼
   Executable
```

### 支持的操作码

| 类型 | 操作码 | 说明 |
|------|--------|------|
| 算术 | ADD, SUB, MUL | 整数运算 |
| 逻辑 | AND, OR, XOR, NOT | 位运算 |
| 比较 | EQ, NE, LT, LE, GT, GE | 条件比较 |
| 选择 | MUX, SELECT | 多元选择 |
| 内存 | LOAD, STORE | 内存访问 |
| 时钟 | CLOCK, RESET | 时序控制 |

### Fallback 策略

| 场景 | 处理方式 |
|------|---------|
| 复杂 op | `CALL_EXTERNAL` 调用解释版本 |
| 位宽 >256bit | `CALL_EXTERNAL` |
| 未实现 op | `CALL_EXTERNAL` |

---

## Phase 2: 简单电路 JIT (10-14 days)

### 目标
Simple circuit JIT compiles and executes correctly。

### 任务清单

| 任务 | 描述 | 交付物 |
|------|------|--------|
| P2-1 | 实现完整操作码支持 | mux, select, comparison |
| P2-2 | 实现 clock/reset 处理 | 时钟上升沿检测 |
| P2-3 | 实现反馈环路处理 | reg output → input |
| P2-4 | 优化寄存器合并 | 减少 memory access |
| P2-5 | 验证简单设计 | counter/fifo/uart demo |

### 反馈环路处理

```
Reg Output ──→ ┌────────────┐
               │   JIT      │
               │   Comb     │──→ Reg Input
               └────────────┘
```

JIT 编译时需要识别反馈环路，确保组合逻辑在时钟沿后正确求值。

### 验证设计

| 设计 | 复杂度 | 验证点 |
|------|--------|--------|
| counter_demo | 低 | 计数逻辑正确 |
| fifo_demo | 中 | 读写指针、full/empty |
| uart_demo | 中 | 波特率生成、帧格式 |

---

## Phase 3: A/B 验证模式 (7-10 days)

### 目标
A/B mode zero mismatch, JIT can be switched on/off。

### 任务清单

| 任务 | 描述 | 交付物 |
|------|------|--------|
| P3-1 | 实现 A/B 验证框架 | AbVerifier class |
| P3-2 | 实现 mismatch 检测和报告 | 详细日志 |
| P3-3 | 实现 JIT 开/关切换 API | `sim.set_jit_enabled(bool)` |
| P3-4 | 全量回归测试 | 所有测试 A/B 验证 |
| P3-5 | 性能测试对比 | JIT vs Interpreted |

### A/B 验证流程

```
tick()
  │
  ├─→ Interpreted Execution → result_A
  │
  └─→ JIT Execution (if enabled) → result_B
        │
        └─→ compare(result_A, result_B)
              │
              ├─→ match: continue
              │
              └─→ mismatch: abort with detail
```

### API 设计

```cpp
class Simulator {
public:
    // 启用/禁用 JIT
    void set_jit_enabled(bool enabled);

    // 获取 JIT 状态
    bool is_jit_enabled() const;

    // A/B 验证模式
    void set_ab_verification(bool enabled);
};
```

---

## Phase 4: 性能优化 (14-21 days)

### 目标
Performance target met, memory ops inlined。

### 任务清单

| 任务 | 描述 | 交付物 |
|------|------|--------|
| P4-1 | 实现内联优化 | 小函数内联 |
| P4-2 | 实现常量折叠优化 | 编译期求值 |
| P4-3 | 实现死代码消除 | DCE pass |
| P4-4 | 实现 memory operation inlining | 减少函数调用 |
| P4-5 | 性能测试达到目标 | 3-5x vs interpreted |
| P4-6 | 内存使用优化 | 减少分配 |

### 性能目标

| 指标 | 目标 | vs Interpreted |
|------|------|-----------------|
| 执行时间 | < 200ms | 3-5x 提升 |
| 内存使用 | < 50MB | 相近或更少 |
| 启动时间 | < 500ms | 可接受 |

---

## 风险控制

| 风险 | 影响 | 缓解措施 |
|------|------|---------|
| LLVM 集成复杂 | 高 | P0 阶段先验证 LLVM 连接 |
| A/B mismatch 调试难 | 中 | P3 提供详细 mismatch 日志 |
| 性能目标未达 | 中 | 每阶段性能测试，及早发现 |
| 宽位宽支持 | 低 | CALL_EXTERNAL fallback |
| JIT 编译慢 | 低 | 增量编译，缓存编译结果 |

---

## 测试策略

| 阶段 | 测试类型 | 覆盖率目标 |
|------|---------|-----------|
| P0 | LLVM 集成测试 | 100% |
| P1 | A/B 验证，所有现有测试 | 100% |
| P2 | 简单电路专项测试 | 覆盖 80% 操作 |
| P3 | 全量回归 A/B 验证 | 100% |
| P4 | 性能基准测试 | 关键路径 100% |

### 测试命令

```bash
# P1 验证
ctest --output-on-failure
./run_all_ported_tests.sh

# P3 全量回归
./run_all_ported_tests.sh --ab-verification
ctest -R "chlib" --output-on-failure

# P4 性能基准
./tests/benchmark/perf_tests --report
```

---

## 文件结构

```
CppHDL/
├── include/
│   └── jit/
│       ├── jit_compiler.h       # JIT 编译器主类
│       ├── jit_abi.h            # ABI 定义
│       ├── ir_gen.h             # IR 生成器
│       └── ab_verifier.h        # A/B 验证器
├── src/
│   └── jit/
│       ├── jit_compiler.cpp
│       ├── ir_gen.cpp
│       └── ab_verifier.cpp
├── tests/
│   └── benchmark/
│       ├── CMakeLists.txt
│       ├── perf_basic.cpp       # 基础性能测试
│       ├── perf_stream.cpp      # Stream 性能测试
│       └── perf_memory.cpp      # 内存操作测试
└── docs/
    └── simulation/
        └── JIT_ROADMAP.md       # 本文档
```

---

## 相关文档

- `task_plan.md` - 项目任务计划
- `docs/simulation/SIM_OPT_PLAN.md` - 详细实施计划
- `docs/simulation/ARCHITECTURE.md` - 仿真架构分析
- `/workspace/project/cash/src/compiler/simjit.cpp` - Cash JIT 参考实现

---

## 更新记录

| 日期 | 版本 | 变更 |
|------|------|------|
| 2026-04-27 | v1.0 | 初始版本，基于 Metis JIT 计划 |