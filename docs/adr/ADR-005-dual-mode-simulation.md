# ADR-005: 仿真引擎双模式（解释器/JIT）设计

**状态**: 草稿（待评审）  
**日期**: 2026-05-06  
**决策人**: QoderWork Architecture Review  
**优先级**: P0  
**相关文档**: `docs/adr/ADR-003-jit-compiler-architecture.md`, `include/simulator.h`, `src/simulator.cpp`

---

## 1. 背景

CppHDL 的仿真引擎支持两种执行模式：**解释器模式**和 **JIT 编译模式**。这种双模式设计提供了灵活性和性能优化空间，但也引入了数据同步、状态一致性、以及切换逻辑的复杂性。

本文档记录双模式仿真的设计决策、执行流程、切换策略，以及已知问题。

---

## 2. 架构决策

### D1: 解释器和 JIT 共享 data_map 数据结构

**决策**: 两种模式共享 `Simulator::data_map_`（`unordered_map<uint32_t, sdata_type>`）作为权威数据存储。JIT 模式在执行前后通过 `sync_to_buffer()` 和 `sync_from_buffer()` 与 `data_map_` 同步。

**理由**:
- **状态一致性**: `data_map_` 是仿真状态的唯一权威来源
- **切换灵活性**: 可以在解释器和 JIT 之间动态切换（通过 `set_jit_enabled()`）
- **A/B 验证**: 可以对比两种模式的结果（虽然当前 A/B 验证已禁用）

**同步流程**:
```
解释器模式:
  data_map_ → instr->eval() → data_map_ (直接更新)

JIT 模式:
  data_map_ → sync_to_buffer() → data_buffer_ → JIT 函数 → sync_from_buffer() → data_map_
```

**权衡**:
- ✅ 状态一致性：两种模式读写相同的数据结构
- ✅ 可切换性：运行时可以切换模式
- ❌ 同步开销：JIT 模式需要两次数据拷贝（`data_map_` ↔ `data_buffer_`）
- ❌ 性能瓶颈：`unordered_map` 查找比数组索引慢

**已知问题**:
- 同步开销可能抵消 JIT 的性能优势（尤其是小设计）
- PRD T15 任务计划统一 JIT 和解释器的内存布局

---

### D2: JIT 模式下 CALL_EXTERNAL 节点先于 JIT 函数执行 — **目标状态**

> ⚠️ **本文档描述的是目标状态（正确的执行顺序），但当前代码尚未完全实现。**
> 参见 ADR-003 D5 了解当前状态的 P0 债务。

**决策**: 在 `eval_combinational()` 和 `eval_sequential()` 中，先执行标记为 `CALL_EXTERNAL` 的解释器节点，然后再执行 JIT 编译的函数。

**理由**:
- **数据依赖**: `CALL_EXTERNAL` 节点的输出可能是 JIT 节点的输入
- **陈旧值问题**: 如果 JIT 函数先执行，它会读到旧的 `data_buffer_` 值（因为 `CALL_EXTERNAL` 节点的结果还没有更新到 `data_map_`）
- **历史 bug**: 当前代码在 JIT 之后执行解释器节点，导致 JIT 使用陈旧值（见 ADR-003 D5）

**实现（目标代码）**:
```cpp
// eval_combinational() 中的正确顺序
if (jit_enabled_ && jit_compiled_ && jit_compiler_ && jit_compiler_->has_comb_func()) {
    // 1. 先执行 CALL_EXTERNAL 解释器节点
    for (const auto &[node_id, instr] : combinational_instr_list_) {
        if (jit_compiler_->is_external_node(node_id)) {
            instr->eval();  // 更新 data_map_
        }
    }

    // 2. 同步到 JIT 缓冲区（包含解释器节点的更新）
    jit_compiler_->sync_to_buffer(data_map_);

    // 3. 执行 JIT 编译的函数（读到正确的值）
    jit_compiler_->execute_comb_tick();

    // 4. 同步回 data_map_
    jit_compiler_->sync_from_buffer(data_map_);
}
```

**CRITICAL 规则**:
- 解释器节点必须先于 JIT 函数执行（**当前代码顺序错误，需要修复**）
- `CALL_EXTERNAL` 节点的数量应该尽可能少（理想情况为零）

---

### D3: tick() 采用时钟驱动的组合/时序分离执行

**决策**: `tick()` 的执行顺序是：组合逻辑 → 时钟更新 → 根据时钟状态决定执行组合或时序逻辑。

**执行流程**:
```
Simulator::tick()
├── eval_combinational()          // 1. 始终执行组合逻辑
├── eval() (时钟更新)             // 2. 更新时钟边沿状态
│   ├── default_clock_instr_->eval()
│   ├── other_clock_instr_list_
│   └── 根据时钟状态:
│       ├── 时钟=0 → eval_combinational()
│       └── 时钟=1 → eval_sequential()
└── trace()                       // 3. 记录波形（如果启用）
```

**JIT 模式下的执行**:
```
Simulator::tick() (JIT enabled)
├── eval_combinational()
│   ├── 执行 CALL_EXTERNAL 解释器节点
│   ├── sync_to_buffer()
│   ├── JIT execute_comb_tick()
│   └── sync_from_buffer()
├── eval() (时钟更新 - 始终解释器执行)
│   └── default_clock_instr_->eval()
├── 根据时钟状态:
│   ├── 时钟=0 → eval_combinational() (JIT)
│   └── 时钟=1 → eval_sequential() (JIT)
│       ├── 执行 CALL_EXTERNAL 解释器节点
│       ├── sync_to_buffer()
│       ├── JIT execute_seq_tick()
│       └── sync_from_buffer()
└── trace()
```

**理由**:
- **语义正确**: 组合逻辑在每个时钟周期都执行，时序逻辑只在时钟边沿后执行
- **与硬件行为一致**: 组合逻辑的输出是时序逻辑的输入
- **JIT 优化**: 组合逻辑和时序逻辑分离编译，可以独立优化

**已知问题**:
- 时钟更新始终由解释器执行（`instr_clock::eval()`），没有 JIT 编译
- 时钟边沿检测存在 bug（PRD T1 任务）

---

### D4: JIT 编译在 Simulator 构造后延迟触发

**决策**: JIT 编译不在 `Simulator` 构造函数中执行，而是在第一次 `tick()` 时通过 `try_jit_compile()` 延迟触发。

**理由**:
- **编译开销**: JIT 编译需要 50-200ms，如果用户只执行几次 tick，编译开销可能超过收益
- **灵活性**: 用户可以在 tick 之前调用 `set_jit_enabled(false)` 禁用 JIT
- **错误处理**: 如果 JIT 编译失败，可以优雅回退到解释器

**触发时机**:
```cpp
// try_jit_compile() 在 tick() 中调用
void Simulator::try_jit_compile() {
    if (jit_enabled_ && !jit_compiled_ && ctx_) {
        auto result = jit_compiler_->compile(ctx_);
        if (result.result == JitResult::SUCCESS) {
            jit_compiled_ = true;
        } else {
            CHWARN("JIT compilation failed, falling back to interpreter");
            jit_enabled_ = false;
        }
    }
}
```

**权衡**:
- ✅ 避免不必要的编译（如果用户只执行少量 tick）
- ❌ 首次 tick 延迟较高（包含编译时间）
- ❌ 性能基准难以测量（首次 tick 包含编译时间）

---

### D5: 解释器指令缓存避免重复创建

**决策**: Simulator 使用 `instr_cache_`（`unordered_map<uint32_t, unique_ptr<instr_base>>`）缓存解释器指令，避免每次 `reinitialize()` 时重新创建。

**理由**:
- 指令创建开销不小（尤其是 AST 遍历和类型检查）
- 指令与 AST 节点的映射关系是稳定的，可以复用
- `update_instruction_pointers()` 在 context 变化时更新缓存中的指针

**实现**:
```cpp
void Simulator::update_instruction_pointers() {
    // 当 context 变化时，更新缓存指令中的节点指针
    for (const auto &[node_id, instr] : instr_cache_) {
        auto* node = ctx_->get_node_by_id(node_id);
        if (node) {
            instr->update_node_pointer(node);
        }
        // 同时更新 instr_map_ 用于快速查找
        instr_map_[node_id] = instr.get();
    }
}
```

---

### D6: JIT 启用状态可运行时切换

**决策**: 用户可以通过 `set_jit_enabled(bool)` 在运行时切换仿真模式。

**理由**:
- **调试**: 遇到不一致结果时，可以禁用 JIT 确认是否为 JIT 问题
- **渐进优化**: 可以先用解释器验证正确性，再启用 JIT 测试性能
- **兼容性**: 没有 LLVM 的环境中自动回退到解释器

**实现**:
```cpp
void Simulator::set_jit_enabled(bool enabled) {
    if (enabled && !jit_compiler_->is_available()) {
        CHWARN("JIT not available, falling back to interpreter");
        jit_enabled_ = false;
        return;
    }
    
    if (enabled != jit_enabled_) {
        jit_enabled_ = enabled;
        jit_compiled_ = false;  // 重置编译状态
        if (jit_compiler_) {
            jit_compiler_->clear();  // 清除已编译函数
        }
    }
}
```

**CRITICAL 规则**:
- 切换模式会清除已编译的 JIT 函数
- 下次 `tick()` 会重新触发 JIT 编译
- 不要在热点路径中频繁切换模式

---

### D7: 数据缓冲区双写同步（从 ADR-004 迁入）

**决策**: JIT 编译后的函数使用独立的 `data_buffer_`（`uint64_t*`），通过 `sync_to_buffer()` 和 `sync_from_buffer()` 与 Simulator 的 `data_map_` 同步。

**理由**:
- JIT 函数需要连续内存布局以获得最佳性能
- `data_map_` 是 `unordered_map<uint32_t, sdata_type>`，不支持直接指针访问
- 同步开销是 JIT 性能瓶颈之一（详见 ADR-003）

**同步流程**:
```
Simulator::tick() (JIT 模式)
├── sync_to_buffer(data_map_)     // TB 输入值 → JIT 缓冲区
├── jit_compiler_->execute_comb_tick()  // JIT 执行组合逻辑
├── jit_compiler_->execute_seq_tick()   // JIT 执行时序逻辑
└── sync_from_buffer(data_map_)   // JIT 缓冲区 → Simulator 数据
```

**已知问题**:
- 同步开销可能抵消 JIT 的性能优势（尤其是小设计）
- 未来应该统一 JIT 和解释器的内存布局（PRD T15 任务）

**相关决策**: D1（共享 data_map）定义了两者使用同一数据源的策略，D7 定义了 JIT 模式下数据的物理同步路径。

---

## 3. 模式对比

| 特性 | 解释器模式 | JIT 模式 |
|------|-----------|---------|
| **执行速度** | ~10K-100K ticks/s | 目标 2-4x 解释器（当前未稳定） |
| **首次执行延迟** | 低（直接执行） | 高（包含编译时间 50-200ms） |
| **内存占用** | 低（指令缓存 + data_map） | 中（额外 data_buffer_ + LLVM 会话） |
| **支持的操作** | 全部 | 部分（不支持的返回 CALL_EXTERNAL） |
| **调试友好** | 高（可以单步执行） | 低（编译后无法单步） |
| **适用场景** | 小设计、调试、首次验证 | 大设计、性能敏感、回归测试 |

---

## 4. 性能分析

### 4.1 解释器瓶颈

| 瓶颈 | 位置 | 影响 |
|------|------|------|
| `unordered_map` 查找 | `data_map_.find(node_id)` | 每次指令执行需要哈希查找 |
| 虚函数调用 | `instr->eval()` | 多态分发开销 |
| `sdata_type` 拷贝 | 值传递 | 位向量拷贝开销 |
| 类型检查 | `lnodeimpl::type()` | 虽然分类列表减少了检查，但仍有残留 |

### 4.2 JIT 瓶颈

| 瓶颈 | 位置 | 影响 |
|------|------|------|
| 数据同步 | `sync_to/from_buffer()` | 两次数据拷贝 |
| 编译延迟 | LLVM IR 生成 + 编译 | 首次 tick 延迟 |
| CALL_EXTERNAL 回退 | `external_node_ids_` | 混合执行模式开销 |
| 缺少优化 Pass | 未启用 `mem2reg` 等 | 生成的代码次优 |

### 4.3 优化方向

| 优化 | 预期收益 | 任务 |
|------|---------|------|
| `unordered_map` → `vector` | 2-3x 解释器加速 | PRD T10 |
| LLVM 优化 Pass | 1.5-2x JIT 加速 | PRD T8 |
| 统一内存布局 | 消除同步开销 | PRD T15 |
| 热点路径内联 | 减少函数调用 | 未来 |
| 缓存编译结果 | 避免重复编译 | 未来 |

---

## 5. 调试指南

### 5.1 如何确认问题是 JIT 还是解释器

```cpp
// 1. 禁用 JIT，使用解释器
sim.set_jit_enabled(false);
sim.tick();  // 如果结果正确，说明是 JIT 问题

// 2. 启用 A/B 验证（当前已禁用，以下为原理说明）
// sim.set_ab_verification(true);
// sim.tick();  // 对比两种模式的结果
```

> **关于 A/B 验证**: 该功能**当前已禁用**，原因如下：
> 1. CALL_EXTERNAL 混合执行模式产生不一致的 data_map 状态（同一轮 tick 被两种模式修改，见 ADR-003 D5）
> 2. 在 P0 债务（有符号算术、64 位掩码 UB、CALL_EXTERNAL 执行顺序）修复之前，A/B 验证的结果本身不可信
> 3. 每次 tick 运行两遍仿真使性能下降 2x，与 JIT 的优化目标冲突
>
> **恢复条件**: T1（时钟边沿检测）、T4（64 位掩码 UB）、T6（有符号算术）全部修复后，重新设计 A/B 验证机制。

### 5.2 常见症状和原因

| 症状 | 可能原因 | 解决方法 |
|------|---------|---------|
| JIT 结果与解释器不同 | 有符号算术错误/位宽掩码 UB | 禁用 JIT 确认，参考 ADR-003 |
| 首次 tick 很慢 | JIT 编译延迟 | 正常，编译只执行一次 |
| 部分设计无法 JIT | 不支持的操作 | 检查 `is_external_node()` 列表 |
| 切换模式后结果错误 | 数据同步问题 | 确保 `sync_from_buffer()` 执行 |

---

## 6. 技术债务

| 债务 | 影响 | 修复计划 | 参考 |
|------|------|---------|------|
| 时钟更新未 JIT 编译 | 时钟路径总是解释器执行 | 未来优化 | — |
| A/B 验证已禁用 | 无法自动检测 JIT 不一致 | P0 债务修复后重新设计 | ADR-003 D5 |
| 进度报告使用 `std::cout` | 影响性能测量 | 改用日志系统 | — |
| `static` 变量跟踪 tick 计数 | 多 Simulator 实例冲突 | 改为实例变量 | PRD T? |

---

## 7. 决策影响

### 对开发者的影响

1. **测试策略**: 新功能必须同时测试解释器和 JIT 模式
2. **性能基准**: 测量 JIT 性能时排除首次编译时间
3. **调试流程**: 遇到问题时先禁用 JIT 确认问题来源

### 对用户的影响

1. **默认行为**: JIT 默认启用（如果 LLVM 可用），用户无需手动配置
2. **性能预期**: 大设计才能获得显著加速，小设计可能更慢（因为编译开销）
3. **调试建议**: 遇到不一致结果时，先禁用 JIT

---

## 8. 变更日志

| 日期 | 版本 | 变更 | 作者 |
|------|------|------|------|
| 2026-05-06 | v1.0 | 初始版本，记录双模式仿真设计 | QoderWork |
| 2026-05-06 | v1.1 | 标注 CALL_EXTERNAL 执行顺序为"目标状态"，迁入 ADR-004 D6 作为 D7，补充 A/B 验证禁用原因和 PRD 任务引用 | QoderWork Review |

---

**相关链接**:
- [ADR-003: JIT 编译器架构](ADR-003-jit-compiler-architecture.md)
- [ADR-004: 数据所有权和生命周期](ADR-004-data-ownership-lifecycle.md)
- `include/simulator.h` — Simulator 定义
- `src/simulator.cpp` — 仿真执行实现
- `docs/simulation/JIT_ROADMAP.md` — JIT 路线图

---

**决策人**: AI Agent  
**审阅人**: ________________  
**状态**: 草稿（待评审）
