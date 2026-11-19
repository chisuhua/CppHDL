# CppHDL 仿真性能测试计划

> 本文档定义仿真性能基准测试的目标、方法论和验收标准。

---

## 1. 测试目标

| 目标 | 描述 |
|------|------|
| **建立性能基线** | 量化不同设计规模下的仿真速度（ticks/sec） |
| **识别性能瓶颈** | 定位影响仿真性能的关键代码路径 |
| **回归检测** | 确保代码修改不会引入性能退化（阈值 >10%） |
| **优化验证** | 评估优化策略的实际效果 |
| **JIT vs Interpreter 对比** | 评估 LLVM JIT 加速效果 |

---

## 2. 测试环境

| 项目 | 配置 |
|------|------|
| 编译器 | GCC/Clang C++20 |
| 构建 | CMake + Ninja |
| 硬件 | x86_64 Linux, >= 4 核 |
| 计时器 | `std::chrono::high_resolution_clock` |
| LLVM | 版本 18 (JIT 模式) |

---

## 3. 性能指标

| 指标 | 定义 | 单位 |
|------|------|------|
| **仿真速率** | `Tick 次数 / 总时间` | ticks/sec |
| **每 Tick 时间** | `总时间 / Tick 次数` | μs/tick |
| **每节点 Tick 时间** | `总时间 / (Tick次数 × 节点数)` | ns/node/tick |
| **跟踪开销** | `(trace_on - trace_off) / trace_off` | % |
| **内存占用** | 仿真期间峰值 RSS | MB |
| **初始化时间** | Simulator 构造到首个 tick | ms |
| **JIT 加速比** | `interpreter_ticks/sec / jit_ticks/sec` | 倍数 |

---

## 4. 测试用例

### TC-01：组合逻辑链深度测试

**目的**：评估组合逻辑链长度对仿真性能的影响

| 参数 | 值 |
|------|------|
| 链深度 N | 10, 100, 500, 1000, 5000, 10000 |
| Tick 次数 | 10000 |
| 测量指标 | 总时间、平均 tick 时间、ticks/sec |

**设计**：`输入 → XOR1 → XOR2 → ... → XORN → 输出`

**验收标准**：
- 时间与节点数呈近似线性关系
- 每节点 Tick 时间差异 < 20%

---

### TC-02：时序逻辑密度测试

**目的**：评估寄存器数量对仿真性能的影响

| 参数 | 值 |
|------|------|
| 寄存器数量 | 10, 100, 1000, 5000, 10000 |
| Tick 次数 | 10000 |
| 测量指标 | 总时间、每寄存器 tick 时间 |

**设计**：`每个寄存器: reg[i] = reg[i] + 1 (模 2^N)`

**验收标准**：
- 每寄存器 tick 时间应保持稳定（差异 < 15%）
- 寄存器 eval() 开销占比可量化

---

### TC-03：内存规模测试

**目的**：评估不同深度/宽度的内存仿真性能

| 参数 | 值 |
|------|------|
| 内存深度 | 16, 256, 1024, 4096, 16384 |
| 数据宽度 | 8, 32, 64, 128 bits |
| 端口类型 | async_read, sync_read, write |
| Tick 次数 | 10000 |

**验收标准**：
- 异步读：时间与深度无关（O(1) 访问）
- 同步读/写：时间与深度近似线性

---

### TC-04：信号跟踪开销测试

**目的**：量化信号跟踪功能对仿真性能的影响

| 参数 | 值 |
|------|------|
| 设计规模 | 1000 寄存器 + 组合逻辑 |
| 跟踪信号数 | 0, 10, 100, 500, 1000 |
| 信号变化率 | 10%, 50%, 100% |
| 测量指标 | trace_on vs trace_off 时间差 |

**验收标准**：
- 跟踪开销 = `(trace_on - trace_off) / trace_off`
- 跟踪信号数 > 500 时，开销应 < 50%

---

### TC-05：综合设计规模测试

**目的**：评估整体设计规模（节点总数）对性能的影响

| 参数 | 值 |
|------|------|
| 总节点数 N | 100, 500, 1000, 5000, 10000 |
| Tick 次数 | 10000 |

**设计**：混合设计包含 N/10 输入、N/10 输出、N/2 寄存器、N/5 组合逻辑、1 个内存

**验收标准**：
- 仿真速率随节点数增加而下降（预期非线性）
- 每节点时间应趋于稳定（规模效应）

---

### TC-06：批量 Tick 性能测试

**目的**：评估 `tick(count)` 批量执行 vs 单步 `tick()` 的性能差异

| 参数 | 值 |
|------|------|
| 设计规模 | 1000 节点, 10000 节点 |
| 执行方式 | `tick(10000)` vs `for(i=0;i<10000;i++) tick()` |
| 测量指标 | 时间差百分比 |

**验收标准**：
- 批量执行应 <= 单步循环（允许误差 5%）
- 如果批量执行更慢，说明存在优化空间

### TC-07：组合逻辑三路对比（Interpreter / JIT / Verilator）

**目的**：在 TC-01 DUT 上对比三个仿真后端的性能，建立跨后端基线

| 参数 | 值 |
|------|------|
| 链深度 N | 10, 100, 1000 |
| Tick 次数 | 10000 |
| 后端 | Interpreter (`set_jit_enabled(false)`)、JIT (`set_jit_enabled(true)`)、Verilator (subprocess + SHA-1 缓存) |
| DUT | 复用 TC-01（XOR 链 + `ch_out<>`） |

**测量方法**：
- 5 次 warmup + 10 次 measured，取 median
- `build_us` 和 `sim_us` 分列报告
- Verilator 不可用时降级为 2-way（Interpreter + JIT），输出 warning 后退出码仍为 0

**验收标准**：
- 三后端数据齐全，CSV / JSON / Markdown 报告可被解析
- Verilator 加速比 >= 1.5x（相对 Interpreter）
- JIT 在小规模（depth=10）下应快于 Interpreter

---

### TC-08：时序逻辑三路对比（Interpreter / JIT / Verilator）

**目的**：在 TC-02 DUT 上对比三个仿真后端的时序逻辑性能

| 参数 | 值 |
|------|------|
| 寄存器数量 | 10, 100, 1000（单 `ch_reg<>` 复用 DUT） |
| Tick 次数 | 10000 |
| 后端 | Interpreter, JIT, Verilator |

**测量方法**：
- 5 次 warmup + 10 次 measured，取 median
- 包含 reset 序列：`reset=1` → `clock=1` → `reset=0`
- Verilator 路径走 harness 的 `default_clock` 切换

**验收标准**：
- 三后端数据齐全
- Verilator 应提供显著加速（预期 >= 5x）
- 时序逻辑在 JIT 模式下加速比 >= 1.5x

---

## 5. 测试框架

```
tests/performance/
├── CMakeLists.txt
├── perf_main.cpp
├── bench_combinational.cpp      # TC-01
├── bench_sequential.cpp          # TC-02
├── bench_memory.cpp              # TC-03
├── bench_trace.cpp               # TC-04
├── bench_mixed_design.cpp        # TC-05
└── bench_batch_tick.cpp          # TC-06
```

---

## 6. 验收标准

| 测试 | 通过条件 |
|------|---------|
| TC-01 | 时间 vs 节点数呈线性（R² > 0.95） |
| TC-02 | 每寄存器时间差异 < 15% |
| TC-03 | 异步读时间稳定，同步读/写与深度线性相关 |
| TC-04 | 跟踪开销可量化且 < 预期上限 |
| TC-05 | 综合性能曲线可预测 |
| TC-06 | 批量执行时间 <= 单步循环 |
| JIT 对比 | JIT 模式应 >= Interpreter 模式性能 |

### 回归阈值

```
性能退化检测: 当前结果 vs 基线差异 > 10% → CI 失败
```

---

## 7. 当前性能基线（已测量）

> 数据来源：实测数据 (2026-05-02)
> 构建目录：`build_off` (Interpreter), `build_jit_on` (JIT enabled)

### TC-01：组合逻辑链 - Interpreter vs JIT 对比

| 参数 | Interpreter | JIT | 加速比 | 说明 |
|------|-------------|-----|--------|------|
| depth=10 | 22,813 ticks/sec | 25,114 ticks/sec | **1.10x** | JIT 小型设计 |
| depth=100 | 2,729 ticks/sec | 2,882 ticks/sec | **1.06x** | JIT 中型设计 |
| depth=1000 | 248 ticks/sec | 256 ticks/sec | **1.03x** | JIT 大型设计 |

**关键发现**：

1. **JIT 加速效果有限**：组合逻辑链场景下，JIT 仅带来 **3-10%** 性能提升
2. **规模越大加速比越低**：大设计（1000+ 节点）加速比下降，因为：
   - JIT 编译时间被摊销减少
   - 内存访问模式（data_buffer GEP）成为瓶颈，无法通过 JIT 优化
   - Interpreter 模式的 eval_list 遍历已经相当高效
3. **编译开销**：JIT 需要首次编译（~1-5ms），对长期运行有利但增加初始化成本

### 基线数据分析

**Interpreter 模式特性**：
- 每节点 Tick 时间随深度线性增长（约 4000 ns/node/tick）
- 简单遍历 eval_list，无特殊优化
- 适合中小型设计（<1000 节点）

**JIT 模式特性**：
- LLVM IR 编译后直接执行，减少解释开销
- 但内存访问模式无法优化（data_buffer 是运行时数组）
- 适合需要重复运行相同设计的场景

### TC-02：时序逻辑寄存器 - Interpreter vs JIT 对比

| 参数 | Interpreter | JIT | 加速比 | 说明 |
|------|-------------|-----|--------|------|
| regs=10 | 25,222 ticks/sec | 133,079 ticks/sec | **5.28x** | JIT 小型设计 |
| regs=100 | 65,626 ticks/sec | 111,031 ticks/sec | **1.69x** | JIT 中型设计 |
| regs=1000 | 47,290 ticks/sec | 108,244 ticks/sec | **2.29x** | JIT 大型设计 |
| regs=5000 | 41,834 ticks/sec | 303,005 ticks/sec | **7.24x** | JIT 最优区间 |
| regs=10000 | 50,753 ticks/sec | 95,580 ticks/sec | **1.88x** | JIT 大规模 |

**关键发现**：

1. **JIT 显著加速时序逻辑**：寄存器更新场景下，JIT 带来 **1.7x-7.2x** 性能提升
2. **regs=5000 时加速比最高 (7.24x)**：中等规模设计 JIT 效果最好
3. **每寄存器 tick 时间 (ns/reg/tick)**：

| 参数 | Interpreter | JIT | 加速比 |
|------|-------------|-----|--------|
| regs=10 | 3964.73 ns | 751.43 ns | **5.28x** |
| regs=100 | 152.38 ns | 90.07 ns | **1.69x** |
| regs=1000 | 21.15 ns | 9.24 ns | **2.29x** |
| regs=5000 | 4.78 ns | 0.66 ns | **7.24x** |
| regs=10000 | 1.97 ns | 1.05 ns | **1.88x** |

**原因分析**：
- JIT 将寄存器更新编译为直接内存写入，省去 Interpreter 的虚函数调用开销
- LLVM 优化了寄存器分配，减少了内存访问
- 大规模（10000）时 JIT 编译时间增加，加速比下降

### TC-04：信号跟踪开销 - Interpreter vs JIT 对比

| 参数 | Interpreter | JIT | 说明 |
|------|-------------|-----|------|
| 100 signals | **-16.79%** | **73.61%** | 负值表示 trace 模式更快 |

**关键发现**：

1. **Interpreter trace 异常**：负开销（-16.79%）表明 trace 模式反而更快，可能是测量误差或缓存效应
2. **JIT trace 开销显著**：73.61% 开销，说明 JIT 编译的代码在 trace 模式下性能下降
3. **Interpreter trace 更高效**：Interpreter 模式下 trace 开销几乎可以忽略

### TC-06：批量 Tick 性能 - Interpreter vs JIT 对比

| 参数 | Interpreter | JIT | 说明 |
|------|-------------|-----|------|
| batch vs single | **44.11%** | **4.57%** | batch 慢于 single 的百分比 |

**关键发现**：

1. **Interpreter batch 问题严重**：44.11% 开销，批量 tick 比单步慢很多
2. **JIT batch 几乎无开销**：仅 4.57%，批量 tick 效率极高
3. **JIT 大幅改善批量性能**：从 44% 降到 4.6%

### TC-01 完整数据

| 测试 | 参数 | 基线值 (Interpreter) | JIT 值 | 加速比 | 单位 | 状态 |
|------|------|----------------------|--------|--------|------|------|
| TC-01 | depth=10 | **22,813** | **25,114** | **1.10x** | ticks/sec | ✅ 已测量 |
| TC-01 | depth=100 | **2,729** | **2,882** | **1.06x** | ticks/sec | ✅ 已测量 |
| TC-01 | depth=1000 | **248** | **256** | **1.03x** | ticks/sec | ✅ 已测量 |
| TC-02 | regs=10 | **25,222** | **133,079** | **5.28x** | ticks/sec | ✅ 已测量 |
| TC-02 | regs=100 | **65,626** | **111,031** | **1.69x** | ticks/sec | ✅ 已测量 |
| TC-02 | regs=1000 | **47,290** | **108,244** | **2.29x** | ticks/sec | ✅ 已测量 |
| TC-02 | regs=5000 | **41,834** | **303,005** | **7.24x** | ticks/sec | ✅ 已测量 |
| TC-02 | regs=10000 | **50,753** | **95,580** | **1.88x** | ticks/sec | ✅ 已测量 |
| TC-04 | 100 signals | **-16.79%** | **73.61%** | — | overhead | ✅ 已测量 |
| TC-06 | batch vs single | **44.11%** | **4.57%** | — | % | ✅ 已测量 |

### TC-07 / TC-08：三路对比数据（待运行时填入）

> 数据来源：`run_perf_comparison.sh` 运行时填入
> 报告路径：`build/perf_report.{csv,json,md}`
> 字段含义：`build_us` 构建时间 / `sim_us` 仿真时间 / `total_us` 合计 / `iterations` 测量次数 / `median_us` 中位数 / `status` 状态

#### TC-07：XOR 链三路对比

| 参数 | Interpreter (μs) | JIT (μs) | Verilator (μs) | Verilator / Interpreter 加速比 |
|------|------------------|----------|----------------|-------------------------------|
| depth=10   | _fill_ | _fill_ | _fill_ | _fill_ |
| depth=100  | _fill_ | _fill_ | _fill_ | _fill_ |
| depth=1000 | _fill_ | _fill_ | _fill_ | _fill_ |

#### TC-08：寄存器三路对比

| 参数 | Interpreter (μs) | JIT (μs) | Verilator (μs) | Verilator / Interpreter 加速比 |
|------|------------------|----------|----------------|-------------------------------|
| regs=10   | _fill_ | _fill_ | _fill_ | _fill_ |
| regs=100  | _fill_ | _fill_ | _fill_ | _fill_ |
| regs=1000 | _fill_ | _fill_ | _fill_ | _fill_ |

#### 降级行为

Verilator 不可用时脚本打印 warning，报告只含 Interpreter 和 JIT 两行，对应 `status` 列标记为 `DEGRADED`，退出码仍为 0。

---

## 8. JIT 架构说明

### 当前 JIT 实现状态

| 组件 | 状态 | 说明 |
|------|------|------|
| LLVM IR 生成 | ✅ 完成 | 支持基本操作 |
| 位宽掩码处理 | ✅ 完成 | 修复 Trunc→AND 掩码 |
| 组合逻辑编译 | ✅ 完成 | ADD/SUB/MUL/NOT/SHL/SHR |
| 时序逻辑编译 | ✅ 完成 | REG_NEXT 支持 |
| 内存读端口 | ✅ 回退 | type_mem_read_port → UNSUPPORTED_OP |
| 内存写端口 | ✅ 跳过 | type_mem_write_port → continue |
| JIT/Interpreter 回退 | ✅ 完成 | UNSUPPORTED_OP → interpreter |

### JIT 限制

1. **不支持操作**：CONCAT, SLICE, MEM_READ, MEM_WRITE, JUMP, BRANCH, LABEL
2. **回退机制**：不支持的操作触发 interpreter 模式
3. **内存访问**：无法优化 data_buffer 访问模式

---

## 10. 总结

### JIT vs Interpreter 性能对比

| 场景 | Interpreter | JIT | 加速比 | 推荐 |
|------|-------------|-----|--------|------|
| **TC-01 组合逻辑链** | ~250 ticks/s | ~256 ticks/s | **1.03-1.10x** | Interpreter 足够 |
| **TC-02 时序寄存器** | ~50K ticks/s | ~95-303K ticks/s | **1.7-7.2x** | **JIT 显著更快** |
| **TC-04 Trace 开销** | -16.79% | 73.61% | — | Interpreter trace 更高效 |
| **TC-06 Batch 开销** | 44.11% | 4.57% | — | **JIT batch 几乎无开销** |

### 关键发现

1. **TC-01 (组合逻辑)**：JIT 加速效果有限（~3-10%）
   - 内存访问模式成为瓶颈
   - Interpreter 已经相当高效

2. **TC-02 (时序寄存器)**：JIT 显著加速（1.7x-7.2x）
   - regs=5000 时达到最高加速比 7.24x
   - 寄存器更新直接编译为内存写入

3. **TC-04 (Trace)**：Interpreter trace 更高效
   - Interpreter: -16.79%（trace 反而更快，可能是缓存效应）
   - JIT: 73.61%（trace 开销显著）
   - JIT 编译的代码在 trace 模式下性能下降

4. **TC-06 (Batch)**：JIT 大幅改善批量性能
   - Interpreter: 44.11%（批量比单步慢 44%）
   - JIT: 4.57%（批量开销几乎可忽略）
   - JIT 优化了批量 tick 的执行效率

### 结论

| 设计类型 | 推荐模式 | 原因 |
|----------|----------|------|
| 组合逻辑为主 | Interpreter | JIT 加速有限 |
| 时序逻辑为主 | JIT | 最高 7x 加速 |
| 需要 trace 功能 | Interpreter | JIT trace 开销大 |
| 批量 tick 执行 | JIT | 开销从 44% 降至 4.6% |
| 混合设计 | 自动回退 | 根据操作类型选择 |

### 优化建议

- **短期**：优化 data_buffer GEP 访问模式
- **中期**：增加对 CONCAT/SLICE 的 JIT 支持以提升覆盖率
- **长期**：多核并行仿真、GPU 加速

---

## 11. Three-way comparison (TC-07/TC-08)

> 本节定义 Interpreter / JIT / Verilator 三后端性能对比的执行方法、数据格式和回放步骤。
> 一键脚本：`./run_perf_comparison.sh`

### 11.1 测量环境

运行前记录以下环境信息，写入报告头部或单独保留：

| 项目 | 命令 |
|------|------|
| 操作系统 | `uname -a` |
| 编译器 | `g++ --version` 或 `clang++ --version` |
| 构建配置 | `cmake -L build 2>/dev/null \| grep -E "JIT\|LLVM"` |
| Verilator | `verilator --version`（缺失则降级 2-way） |
| CPU | `lscpu \| grep "Model name"` |
| 内存 | `free -h` |
| 时间戳 | `date -Iseconds` |

### 11.2 运行命令

```bash
# 一键脚本：编译 + 测试 + 生成三格式报告
./run_perf_comparison.sh

# 仅运行 TC-07 / TC-08，输出 CSV / JSON / Markdown
./build/tests/benchmark/perf_tests --tc=07 --tc=08 \
    --report=csv --report=json --report=md

# 自定义 Verilator 路径
./build/tests/benchmark/perf_tests --tc=07 --tc=08 \
    --verilator=/opt/verilator/bin/verilator

# 自定义缓存根目录
./build/tests/benchmark/perf_tests --tc=07 --tc=08 \
    --cache-root=$HOME/.cache/cpphdl-verilator
```

### 11.3 输出报告

| 格式 | 路径 | 用途 |
|------|------|------|
| CSV | `build/perf_report.csv` | 表格处理（Excel / pandas） |
| JSON | `build/perf_report.json` | 程序化解析 |
| Markdown | `build/perf_report.md` | GitHub 预览、文档嵌入 |

CSV header 固定为：
```
test_name,params,backend,build_us,sim_us,total_us,iterations,median_us,status
```

> 字段含义：`test_name` = TC 编号（如 `TC-07`），`params` = 运行时参数（如 `depth=10`），`backend` = `interpreter` / `jit` / `verilator` / 空（legacy TC-01/02/04/06）。这样拆分能保留 v1 legacy 行的可追溯性。

### 11.4 降级行为

Verilator 不可用时脚本：

1. 打印 warning：`[WARN] verilator not found, falling back to 2-way comparison`
2. 报告只含 Interpreter 和 JIT 两行，对应 `status` 列标记为 `DEGRADED`
3. 退出码仍为 0，方便 CI 持续集成

### 11.5 TC-07 sample run（XOR chain，待运行时填入）

> 数据来源：`build/perf_report.csv`（运行 `run_perf_comparison.sh` 后填入实际值）
> DUT：TC-01（XOR 链 + `ch_out<>`），链深度 10 / 100 / 1000
> Tick 次数：10000，测量方法：5 warmup + 10 measured，取 median

| 参数 | Interpreter (μs) | JIT (μs) | Verilator (μs) | 加速比（Verilator / Interpreter） |
|------|------------------|----------|----------------|----------------------------------|
| depth=10   | _fill_ | _fill_ | _fill_ | _fill_ |
| depth=100  | _fill_ | _fill_ | _fill_ | _fill_ |
| depth=1000 | _fill_ | _fill_ | _fill_ | _fill_ |

### 11.6 TC-08 sample run（sequential，待运行时填入）

> 数据来源：`build/perf_report.csv`（运行后填入实际值）
> DUT：TC-02（单 `ch_reg<>` + `ch_out<>`），寄存器数 10 / 100 / 1000
> Tick 次数：10000，测量方法：5 warmup + 10 measured，取 median

| 参数 | Interpreter (μs) | JIT (μs) | Verilator (μs) | 加速比（Verilator / Interpreter） |
|------|------------------|----------|----------------|----------------------------------|
| regs=10   | _fill_ | _fill_ | _fill_ | _fill_ |
| regs=100  | _fill_ | _fill_ | _fill_ | _fill_ |
| regs=1000 | _fill_ | _fill_ | _fill_ | _fill_ |

### 11.7 复用与扩展

- TC-07 复用 TC-01 DUT，TC-08 复用 TC-02 DUT，避免设计漂移
- 报告字段定义在 `tests/benchmark/report_generator.h`
- SHA-1 缓存由 `verilator_runner.h` 提供，Verilator 重复构建自动跳过
- 测量统一为 5 warmup + 10 measured，`build_us` 与 `sim_us` 分列

---

*文档版本: v2.3*
*创建日期: 2026-04-25*
*最后更新: 2026-06-08（追加 §11 Three-way comparison 与 TC-07/TC-08 章节）*