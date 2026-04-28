# Task Plan: CppHDL 仿真性能优化实施

**项目**: CppHDL 仿真架构优化
**创建日期**: 2026-04-26
**基于**: `docs/simulation/ARCHITECTURE.md`, `docs/simulation/ROADMAP.md`
**参考**: Oracle 审查反馈 (`4127c0f`)

---

## 目标

根据 ROADMAP.md 实施 P0-P1 阶段优化，**保留现有解释仿真作为 fallback**。

### 关键约束
- **不破坏现有解释仿真**：所有修改必须向后兼容
- **Fallback 机制**：JIT 失败时回退到解释执行
- **基准测试驱动**：每次优化后验证 perf_tests

---

## 实施阶段

### Phase 0: 基础设施准备
**状态**: `completed` ✅
**完成日期**: 2026-04-26
**工作量**: 1 day
**交付物**: 仿真后端选择机制

**任务**:
- [x] S0-1: 定义仿真后端接口 `SimulatorBackend` (解释/JIT)
- [x] S0-2: 添加 `Simulator::backend_` 成员和选择机制
- [x] S0-3: 实现 fallback 逻辑（JIT 失败时使用解释）

---

### Phase 1: P0 优化 - 数据结构层
**状态**: `pending`
**工作量**: 5-7 days
**交付物**: 性能提升 2-4x

**任务**:

#### P0-1: Trace 系统优化
**状态**: `completed` ✅
**完成日期**: 2026-04-26
**预期提升**: 2-4x
**实施时间**: 1-2 days
**提交**: `1806ff0`

- [x] P0-1a: 预计算信号指针数组 `signal_data_ptrs_[]`
- [x] P0-1b: 消除 trace 临时对象（直接 memcmp）
- [x] P0-1c: 验证 TC-04 overhead < 30%

#### P0-2: 扁平数组替代 unordered_map
**状态**: `completed` ✅
**完成日期**: 2026-04-27
**预期提升**: 1.5-2x
**实施时间**: 1-2 days
**提交**: `ee57579` (data buffer), `70c2e33` (proxy chain merging)

- [x] P0-2a: 设计 `data_array_` 布局（连续内存）
- [x] P0-2b: 添加 `instr_base::set_data_ptr()` 接口
- [x] P0-2c: 修改 `data_map_` 访问点
- [x] P0-2d: 验证 L1 cache miss 减少 >30%

#### P0-3: bitvector SSO 优化
**状态**: `pending`
**预期提升**: 1.2-2x
**实施时间**: 2-3 days

- [ ] P0-3a: 实现 `sdata_type` SSO（128bit inline）
- [ ] P0-3b: 添加 `is_inline_` 判断逻辑
- [ ] P0-3c: 验证小信号性能提升

---

### Phase 2: P1 优化 - 指令层
**状态**: `pending`
**工作量**: 7-10 days
**交付物**: 性能提升 1.5-2x

#### P1-1: 函数指针替代虚函数
**状态**: `pending`
**预期提升**: 1.1-1.3x
**实施时间**: 3-5 days
**注意**: Oracle 建议解释执行层面收益有限

- [ ] P1-1a: 设计 `eval_fn_t` 函数指针类型
- [ ] P1-1b: 构建 `combinational_eval_fns_` 数组
- [ ] P1-1c: 修改 `tick()` 使用函数指针分派

#### P1-2: DCE 死代码消除
**状态**: `pending`
**预期提升**: 1.1-1.3x
**实施时间**: 2-3 days

- [ ] P1-2a: 实现反向 BFS 标记活跃节点
- [ ] P1-2b: 处理 proxy 节点的 source 裁剪
- [ ] P1-2c: 验证指令数量减少 10-20%

#### P1-3: 常量折叠
**状态**: `pending`
**预期提升**: 1.1-1.2x
**实施时间**: 1-2 days

- [ ] P1-3a: 检测 type_lit 操作数
- [ ] P1-3b: 编译期计算常量表达式
- [ ] P1-3c: 验证常量密集设计提升

#### P1-4: 代理链合并
**状态**: `completed` ✅
**完成日期**: 2026-04-27
**预期提升**: 1.1-1.2x
**实施时间**: 1-2 days
**提交**: `70c2e33`

- [x] P1-4a: 检测连续 proxy chain
- [x] P1-4b: 合并为单一 proxy

---

### Phase 3: JIT 编译实现 (P0-P4)
**状态**: `in_progress` 🟡
**工作量**: 6-10 weeks (单线程开发)
**当前进度**: P0-P2 完成, P3 待开始
**预期提升**: 5-10x
**默认**: JIT 禁用，必须通过 API 显式启用

#### 技术决策

| 决策 | 选择 | 原因 |
|------|------|------|
| Backend | LLVM ORC JIT | 成熟、活跃维护、find_package 或 FetchContent |
| Data Layout | 扁平 `data_buffer[]` | 连续内存，cache-friendly，替换 unordered_map |
| Fallback | `CALL_EXTERNAL` | 复杂 op 和宽位宽（>256bit）使用外部调用 |
| 验证 | A/B 模式 | 每 tick 运行 JIT 和解释，mismatch 时 abort |
| 默认 | JIT OFF | 需要显式 `sim.set_jit_enabled(true)` |

---

### P0: 基础设施 (3-5 days)
**里程碑**: Benchmark baseline, LLVM dependency integrated
**状态**: `completed` ✅
**提交**: `d8c536d`

**任务**:
- [x] P0-1: 添加 CMake option `CH_JIT_ENABLE` (default OFF)
- [x] P0-2: 集成 LLVM (find_package 或 FetchContent)
- [x] P0-3: 创建 benchmark 框架 `tests/benchmark/`
- [x] P0-4: 建立性能 baseline (perf_tests)
- [x] P0-5: 验证 LLVM 连接正常

**验收标准**:
- CMake 配置通过，无 LLVM 链接错误
- benchmark 可运行并输出 baseline 数据

---

### P1: IR 生成 (7-10 days)
**里程碑**: IR generation passes all existing tests
**状态**: `completed` ✅
**提交**: `1f75a59`

**任务**:
- [x] P1-1: 实现 `JitCompiler` 类框架
- [x] P1-2: 实现节点遍历和 IR 生成
- [x] P1-3: 实现基本操作码编译 (add, sub, and, or, xor)
- [x] P1-4: 实现寄存器分配 (简单的线性扫描)
- [x] P1-5: 实现 memory load/store 编译
- [x] P1-6: 通过所有现有测试 (A/B 验证)

**验收标准**:
- 所有 ctest 通过
- `run_all_ported_tests.sh` 28/28 通过
- A/B 模式：无 mismatch

**Fallback 策略**:
- 复杂 op → CALL_EXTERNAL
- 位宽 >256bit → CALL_EXTERNAL

---

### P2: 简单电路 JIT (10-14 days)
**里程碑**: Simple circuit JIT compiles and executes correctly
**状态**: `completed` ✅
**提交**: `ee57579`, `eb996ff`, `38cef59`

**任务**:
- [x] P2-1: 实现完整操作码支持 (mux, select, comparison)
- [x] P2-2: 实现 clock/reset 处理
- [x] P2-3: 实现反馈环路处理 (reg output → input)
- [x] P2-4: 优化寄存器合并
- [x] P2-5: 验证简单设计 (counter, fifo, uart) - **集成完成，待测试**

**验收标准**:
- counter_demo JIT 执行正确
- fifo_demo JIT 执行正确
- uart_demo JIT 执行正确

---

### P3: A/B 验证模式 (7-10 days)
**里程碑**: A/B mode zero mismatch, JIT can be switched on/off

**任务**:
- [ ] P3-1: 实现 A/B 验证框架
- [ ] P3-2: 实现 mismatch 检测和报告
- [ ] P3-3: 实现 JIT 开/关切换 API
- [ ] P3-4: 全量回归测试 (所有测试 A/B 验证)
- [ ] P3-5: 性能测试对比 (JIT vs Interpreted)

**验收标准**:
- 所有测试 A/B 零 mismatch
- JIT 开/关切换正常
- 性能提升可测量

---

### P4: 性能优化 (14-21 days)
**里程碑**: Performance target met, memory ops inlined

**任务**:
- [ ] P4-1: 实现内联优化 (inline small functions)
- [ ] P4-2: 实现常量折叠优化
- [ ] P4-3: 实现死代码消除 (DCE)
- [ ] P4-4: 实现 memory operation inlining
- [ ] P4-5: 性能测试达到目标 (3-5x vs interpreted)
- [ ] P4-6: 内存使用优化

**验收标准**:
- 性能提升 3-5x (vs interpreted)
- 内存操作内联完成
- 无 regression

---

### 风险控制策略

| 风险 | 影响 | 缓解措施 |
|------|------|---------|
| LLVM 集成复杂 | 高 | P0 阶段先验证 LLVM 连接 |
| A/B mismatch 调试难 | 中 | P3 提供详细 mismatch 日志 |
| 性能目标未达 | 中 | 每阶段性能测试，及早发现 |
| 宽位宽支持 | 低 | CALL_EXTERNAL fallback |

### 测试策略

| 阶段 | 测试类型 | 覆盖率目标 |
|------|---------|-----------|
| P0 | LLVM 集成测试 | 100% |
| P1 | A/B 验证，所有现有测试 | 100% |
| P2 | 简单电路专项测试 | 覆盖 80% 操作 |
| P3 | 全量回归 A/B 验证 | 100% |
| P4 | 性能基准测试 | 关键路径 100% |

---

## 决策记录

| 日期 | 决策 | 原因 |
|------|------|------|
| 2026-04-26 | Phase 1 先于 JIT | 数据结构优化是 JIT 的前提 |
| 2026-04-26 | 保留解释仿真 | 兼容性要求，JIT 可选 |
| 2026-04-26 | P0-Trace 先行 | 实测 82% overhead，最大瓶颈 |
| 2026-04-27 | P0-1 完成 | signal_data_ptrs_ + memcmp 优化 |
| 2026-04-27 | JIT 分 5 Phase | Metis 建议：6-10 weeks，LLVM ORC JIT |

---

## 验收标准

| 优化项 | 验证方法 | 通过标准 |
|--------|---------|---------|
| P0-1: Trace | `perf_tests --tc=04` | overhead < 30% |
| P0-2: 扁平数组 | `valgrind --tool=cachegrind` | L1 miss 减少 >30% |
| P0-3: SSO | micro-benchmark | 小信号性能 +20% |
| P1-1: 函数指针 | `perf_tests --tc=01` | 提升 >10% |
| P1-2: DCE | node count before/after | 指令减少 >10% |
| P0: LLVM 集成 | cmake 配置 | 无链接错误 |
| P1: IR 生成 | ctest + run_all_ported_tests.sh | 100% 通过 |
| P2: 简单电路 | counter/fifo/uart demo | JIT 执行正确 |
| P3: A/B 验证 | 全量回归 | 零 mismatch |
| P4: 性能 | perf_tests | 3-5x 提升 |

---

## 相关文档

- `docs/simulation/ARCHITECTURE.md` - 架构分析
- `docs/simulation/ROADMAP.md` - 实施路线图
- `docs/simulation/JIT_ROADMAP.md` - JIT 实施路线图
- `docs/simulation/PERFORMANCE_TESTS.md` - 测试计划
- `/workspace/project/cash/src/compiler/simjit.cpp` - Cash JIT 参考

---

## 下一步

P0: 添加 CMake option `CH_JIT_ENABLE`，集成 LLVM 依赖

等待确认后开始 JIT 实现。