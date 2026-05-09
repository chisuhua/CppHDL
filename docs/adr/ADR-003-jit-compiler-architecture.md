# ADR-003: JIT 编译器架构决策

**状态**: 草稿（待评审）  
**日期**: 2026-05-06  
**决策人**: QoderWork Architecture Review  
**优先级**: P0  
**相关文档**: `docs/architecture-review-report.md`, `include/jit/AGENTS.md`, `docs/developer_guide/patterns/JIT-DEBUGGING-GUIDE.md`

---

## 1. 背景

CppHDL 的 JIT 编译器是项目的核心差异化功能，目标是将 HDL 设计编译为原生机器码，实现比解释器 2-4 倍的性能提升。JIT 编译器基于 LLVM ORC JIT 框架，将 context 中的 AST 节点图编译为 LLVM IR 并执行。

当前 JIT 编译器处于**实验性阶段**，存在多个架构问题和已知债务，需要明确设计决策以便后续修复和改进。

---

## 2. 架构决策

### D1: 使用 LLVM ORC JIT 而非 MCJIT 或解释器

**决策**: 采用 LLVM ORC JIT API 进行运行时编译。

**理由**:
- **ORC JIT** 是 LLVM 推荐的现代 JIT API，支持惰性编译、模块化编译和更好的生命周期管理
- **MCJIT** 已标记为废弃，缺乏维护
- **LLVM 解释器** 性能比 ORC JIT 低 10-100 倍
- CppHDL 的 AST 节点图可以自然映射为 LLVM IR Module

**权衡**:
- ✅ 高性能：编译为原生代码后执行速度远超解释器
- ✅ 模块化：支持分块编译（combinational/sequential 分离）
- ❌ 编译延迟：首次编译需要 50-200ms（可通过优化 Pass 减少）
- ❌ 依赖体积：LLVM 库增加 ~50MB 二进制体积（可通过动态链接缓解）
- ❌ 构建复杂性：需要 LLVM 头文件和库，跨平台编译配置复杂

**已知问题**:
- LLVM 版本锁定在 17-19，版本兼容性需要手动测试
- JIT 编译失败时回退到解释器的机制不完善（仅通过 `is_external_node()` 标记）

---

### D2: 自定义 IR 中间表示层

**决策**: 在 AST 和 LLVM IR 之间设计自定义 JIT IR（`JitInstr`/`JitBlock`/`JitFunction`）。

**理由**:
- **解耦**: AST 节点（`lnodeimpl`）包含丰富的元数据，JIT IR 只提取执行必需的信息
- **优化**: 自定义 IR 可以在 lowering 到 LLVM IR 前进行优化（如常量折叠、死代码消除）
- **调试**: IR 可序列化为文本格式（`/tmp/jit_ir.ll`），便于诊断 JIT 问题
- **位宽跟踪**: IR 指令携带 `bitwidth` 字段，确保窄位宽操作正确 mask

**IR 设计**:
```
JitFunction (tick_comb / tick_seq)
├── JitBlock (combinational / sequential)
│   ├── JitInstr (LOAD_DATA, ADD, SUB, STORE_DATA, etc.)
│   │   ├── dst: VRegId (虚拟寄存器目标)
│   │   ├── src0/src1/src2: VRegId (虚拟寄存器源)
│   │   ├── node_id: NodeId (AST 节点 ID，用于数据缓冲区访问)
│   │   └── bitwidth: BitWidth (操作位宽)
│   └── ...
└── vreg_count: 虚拟寄存器总数
```

**权衡**:
- ✅ 灵活性：IR 可以独立于 AST 演进
- ✅ 可测试性：IR 生成和 LLVM lowering 可以分别测试
- ❌ 额外复杂度：需要维护两套 lowering 逻辑（AST→IR 和 IR→LLVM IR）
- ❌ 性能开销：IR 生成增加编译时间（但相对于 LLVM 编译可忽略）

**已知问题**:
- IR 不支持 `CONCAT`、`SLICE`、`MEM_READ`、`MEM_WRITE`、`JUMP`、`BRANCH`、`LABEL` 等操作（`compile_to_llvm()` 直接返回 `UNSUPPORTED_OP`）
- 虚拟寄存器分配是线性的，没有寄存器分配优化（所有 vreg 都 alloca 到栈上）

---

### D3: 组合逻辑和时序逻辑分离编译

**决策**: 生成两个独立的 JIT 函数 `tick_comb()` 和 `tick_seq()`，分别处理组合逻辑和时序逻辑。

**理由**:
- **语义清晰**: 组合逻辑（纯函数）和时序逻辑（状态更新）的求值时机不同
- **仿真正确性**: 时序逻辑的寄存器更新必须在时钟边沿后执行，不能与组合逻辑混合
- **优化机会**: 组合逻辑函数可以内联、缓存或并行执行

**执行流程**:
```
Simulator::tick()
├── eval_combinational()
│   ├── Interpreter: 执行 comb_instructions_
│   └── JIT:         执行 tick_comb(data_buffer_)
├── update_clocks() (时钟边沿检测)
└── eval_sequential()
    ├── Interpreter: 执行 seq_instructions_
    └── JIT:         执行 tick_seq(data_buffer_)
```

**权衡**:
- ✅ 语义正确：避免了组合逻辑和时序逻辑的竞争条件
- ✅ 可测试性：可以单独测试组合逻辑函数
- ❌ 数据同步：两个函数共享 `data_buffer_`，需要确保同步正确

---

### D4: 数据缓冲区采用统一 `uint64_t` 数组

**决策**: JIT 编译后的函数通过单一 `uint64_t* data_buffer_` 参数访问所有节点值。

**理由**:
- **简单性**: 所有节点值统一用 64 位存储，不需要复杂的类型系统
- **性能**: 连续内存布局对 CPU 缓存友好
- **ABI 稳定**: 函数签名 `void (*)(uint64_t*)` 简单稳定

**布局**:
```
data_buffer_[node_id] = 节点值（按节点 ID 索引）
```

**已知问题（P0 级）**:
1. **位宽浪费**: <64 位的信号也占用 64 位，内存利用率低（PRD 显示 1000 信号占用 5-10MB）— 未分配任务号，与 PRD T15（统一内存布局）相关但不完全相同
2. **掩码 UB**: 64 位掩码 `(1ULL << 64) - 1` 是未定义行为（PRD T4 计划修复）
3. **有符号算术错误**: 所有比较操作（LT/LE/GT/GE）使用无符号比较（`ICmpULT` 等），有符号信号产生错误结果（PRD T6 计划修复）
4. **缺少 LLVM 优化 Pass**: 生成的 IR 没有运行 `mem2reg`、`instcombine` 等优化 Pass，导致性能低下（PRD T8 计划修复）

---

### D5: 外部节点回退机制

**决策**: JIT 不支持的操作标记为 `CALL_EXTERNAL`，由解释器回退处理。

**实现**:
```cpp
// generate_ir() 中
if (jit_op == JitOp::CALL_EXTERNAL) {
    external_node_ids_.insert(node_id);
    continue;  // 跳过 JIT 编译，由解释器处理
}
```

**理由**:
- **渐进式支持**: 不需要一次性实现所有操作的 JIT 支持
- **安全性**: 避免 JIT 编译未知操作导致崩溃

**⚠️ 当前状态 vs 目标状态**:

| 维度 | 当前状态 | 目标状态 |
|------|---------|---------|
| 执行顺序 | JIT 函数执行完后才运行 CALL_EXTERNAL 解释器节点（错误顺序） | 先执行 CALL_EXTERNAL 解释器节点，再同步到 data_buffer_，最后执行 JIT 函数（正确顺序，见 ADR-005 D2） |
| 下游节点值 | JIT-native 节点读到**陈旧值**（CALL_EXTERNAL 的输出未更新到 data_buffer_） | CALL_EXTERNAL 的输出在执行 JIT 前已同步到 data_buffer_，下游节点值正确 |
| 用户通知 | 静默回退，无警告 | 输出警告日志，说明哪些操作回退到解释器 |

**当前已知问题（P0 级）**:
- **静默数据错误**: `CALL_EXTERNAL` 节点的输出由解释器更新，但当前 JIT 函数在解释器之前执行，下游 JIT-native 节点会读到**陈旧值**。这是**执行顺序错误**（正确顺序见 ADR-005 D2），导致 JIT 混合模式的结果不可信
- **缺少警告**: 用户不知道 JIT 编译不完整，可能误以为性能已优化
- **新操作必须同步添加 JIT 支持**: 在 `ch_op` 枚举添加新操作时，必须同步在 `JitOp`、`generate_ir()`、`compile_to_llvm()` 三处添加支持（详见 `include/jit/AGENTS.md` 规则 1）

**修复依赖**: ADR-005 D2 描述了正确的执行顺序。当前问题源于 `eval_combinational()` 和 `eval_sequential()` 中 JIT 函数与解释器节点的执行顺序颠倒。修复后 JIT 混合模式才能产生可信结果。

---

### D6: 位宽掩码策略

**决策**: 所有 <64 位的算术/位操作结果必须用 `(1ULL << bitwidth) - 1` 掩码截断。

**实施位置**:
- `generate_ir()`: IR 指令携带 `bitwidth` 字段
- `compile_to_llvm()`: 每个算术/位操作的 LLVM lowering 中检查 `if (bw < 64)` 并添加 `AND mask`

**示例**:
```cpp
case JitOp::ADD: {
    auto* res = builder.CreateAdd(a, b, "add");
    if (instr.bitwidth < 64) {
        uint64_t mask = (1ULL << instr.bitwidth) - 1;
        res = builder.CreateAnd(res, builder.getInt64(mask), "mask_add");
    }
    builder.CreateStore(res, vregs[instr.dst], "store_add");
    break;
}
```

**权衡**:
- ✅ 正确性：确保窄位宽运算结果不含垃圾位
- ❌ 性能开销：每个操作额外增加一条 `AND` 指令（可通过 LLVM 优化 Pass 消除冗余）

**已知问题**:
- 64 位掩码计算 `(1ULL << 64) - 1` 是 UB，应该特殊处理 `bitwidth == 64` 时不掩码
- `load_node()` 使用 `AND` 而非 `Trunc`，因为 `Trunc` 会丢弃高位，但高位可能是垃圾（详见 `include/jit/AGENTS.md` 反模式表）

---

### D7: `type_lit` 节点跳过策略

**决策**: `generate_ir()` 中遇到 `type_lit` 节点时，生成 `LOAD_DATA` 指令并从 `data_buffer_` 加载值，然后 `continue`（不是 `break`）。

**理由**:
- `type_lit` 节点的值由运行时 `set_value()` 设置，不是编译时常量
- 如果生成 `LOAD_CONST` 指令，会覆盖运行时设置的值
- 使用 `continue` 跳过当前节点，继续处理后续节点

**已知问题**:
- 历史上曾用 `break` 跳出整个循环，导致所有后续节点被跳过（详见 `include/jit/AGENTS.md` 规则 3）

---

### D8: `type_input` 节点驱动检查

**决策**: `type_input` 节点必须检查是否有 driver（通过 `num_srcs() > 0`），有 driver 时从 `src(0)->id()` 加载值，否则从自身 `node_id` 加载。

**理由**:
- Component IO 连线（`<<=`）通过 `set_driver()` 设置驱动源
- 如果始终从自身加载，会读到零值（连线不工作）

**实现**:
```cpp
case ch::core::lnodetype::type_input: {
    if (node->num_srcs() > 0 && node->src(0)) {
        // 有 driver，从 driver 加载
        block_comb.instrs.push_back(make_load(vreg, node->src(0)->id(), src_bitwidth));
    } else {
        // 直接输入（set_input_value）
        block_comb.instrs.push_back(make_load(vreg, node_id, bitwidth));
    }
    block_comb.instrs.push_back(make_store(node_id, vreg, bitwidth));
    break;
}
```

---

## 3. 当前 JIT 支持的操作列表

### 已支持的操作（原生 JIT）

| 操作 | JitOp | 位宽掩码 | 备注 |
|------|-------|---------|------|
| 加法 | ADD | ✅ | |
| 减法 | SUB | ✅ | |
| 乘法 | MUL | ✅ | |
| 取模 | MOD | ✅ | 除零保护（返回被除数） |
| 按位与 | AND | ❌ | 不需要（操作数已经是掩码后的） |
| 按位或 | OR | ❌ | |
| 按位异或 | XOR | ❌ | |
| 按位非 | NOT | ✅ | XOR with all-1s |
| 左移 | SHIFT_LEFT | ✅ | |
| 右移 | SHIFT_RIGHT | ✅ | 逻辑右移（LShr） |
| 等于 | EQ | ❌ | 返回 1-bit 布尔值 |
| 不等于 | NE | ❌ | |
| 小于 | LT | ❌ | 无符号比较（ICmpULT） |
| 小于等于 | LE | ❌ | 无符号比较 |
| 大于 | GT | ❌ | 无符号比较 |
| 大于等于 | GE | ❌ | 无符号比较 |
| 选择器 | SELECT | ❌ | `cond ? src0 : src1` |
| 位选择 | BIT_SELECT | ❌ | `data[idx]` |
| 位提取 | BITS_EXTRACT | ✅ | `(data >> lsb) & ((1 << width) - 1)` |
| 寄存器下一状态 | REG_NEXT | ✅ | 时序逻辑 |
| 加载数据 | LOAD_DATA | ✅ | 从 `data_buffer_` 加载 |
| 存储数据 | STORE_DATA | ✅ | 存储到 `data_buffer_` |

### 不支持的操作（回退到解释器）

| 操作 | 原因 | 影响 |
|------|------|------|
| DIV（除法） | 未在 `ch_op` 映射中添加 | JIT 编译不完整时回退 |
| SEXT（符号扩展） | 未在 `ch_op` 映射中添加 | 有符号运算不正确 |
| ZEXT（零扩展） | 未在 `ch_op` 映射中添加 | |
| CONCAT（拼接） | LLVM lowering 未实现 | 返回 `UNSUPPORTED_OP` |
| SLICE（切片） | LLVM lowering 未实现 | |
| MEM_READ/MEM_WRITE | 内存阵列状态不可访问 | 返回 `UNSUPPORTED_OP` |
| JUMP/BRANCH/LABEL | 控制流未实现 | |

---

## 4. 架构债务和修复计划

### P0 级债务（必须修复）

| 债务 | 影响 | 修复计划 | 预计工作量 |
|------|------|---------|-----------|
| 有符号算术被当作无符号 | JIT 产生错误结果 | T6: 添加 `SADD`/`SSUB`/`SLT` 等有符号操作 | 8h |
| 64 位掩码 UB | 潜在崩溃 | T4: 特殊处理 `bitwidth == 64` | 2h |
| 缺少 LLVM 优化 Pass | JIT 性能低下 | T8: 添加 `mem2reg`、`instcombine`、`simplifycfg` | 6h |
| 外部节点静默数据错误 | JIT 结果不可信 | 添加 `is_external_node()` 检查和警告 | 4h |

### P1 级债务（重要改进）

| 债务 | 影响 | 修复计划 |
|------|------|---------|
| 虚拟寄存器没有分配优化 | 栈空间浪费 | 实现线性扫描寄存器分配 |
| 数据缓冲区内存利用率低 | 大设计内存占用高 | 按位宽打包节点值 |
| JIT 编译延迟高 | 首次执行慢 | 缓存编译结果，增量编译 |
| `CONCAT`/`SLICE` 未实现 | 部分设计无法 JIT | 实现 LLVM lowering |
| `MEM_READ`/`MEM_WRITE` 未实现 | 带 `ch_mem` 的设计无法 JIT | 设计内存阵列 JIT 接口 |

---

## 5. 性能基准

### 当前性能（待更新）

| 指标 | 当前值 | 目标值 | 测试场景 |
|------|--------|--------|---------|
| JIT 编译延迟 | ~50-200ms | < 50ms | 100 节点设计 |
| JIT 加速比 | 未稳定（因 P0 债务） | 2-4x | 组合逻辑链深度 1000 |
| IR 指令数 | 与节点数线性相关 | - | - |
| 虚拟寄存器数 | 与节点数线性相关 | 优化后减少 50% | - |

### 性能分析

**瓶颈**:
1. **LLVM IR 生成**: 每条指令创建多个 LLVM Value，内存分配频繁
2. **缺少优化 Pass**: 生成的 IR 包含冗余 load/store，没有 `mem2reg` 优化
3. **数据缓冲区同步**: JIT 执行前后需要 `sync_to_buffer()` 和 `sync_from_buffer()`
4. **函数调用开销**: 每次 `tick()` 调用两个 JIT 函数，参数传递通过指针

**优化方向**:
- 启用 LLVM 优化 Pass（`FunctionPassManager`）
- 使用 `mem2reg` 消除栈上虚拟寄存器
- 缓存编译结果，避免重复编译
- 热点路径内联（如 `LOAD_DATA`/`STORE_DATA`）

---

## 6. 调试指南

### 常见问题诊断

| 症状 | 可能原因 | 诊断方法 |
|------|---------|---------|
| JIT 结果与解释器不同 | 有符号算术/位宽掩码/外部节点问题 | `sim.set_jit_enabled(false)` 对比（A/B 验证当前不可用，见 ADR-005 §5.1） |
| JIT 编译失败 | 不支持的操作/LLVM 初始化失败 | 检查 `JitCompileResult::result` |
| 寄存器值不正确 | `REG_NEXT` 逻辑错误/时钟边沿检测 | 检查 `tick_seq()` 执行 |
| 输入端口值始终为零 | `type_input` 未检查 driver | 检查 `num_srcs() > 0` |

### IR 检查

JIT 编译器将生成的 IR 输出到 `/tmp/jit_ir.ll`，可以用 `llc` 或 `opt` 工具分析：
```bash
llc -march=x86-64 /tmp/jit_ir.ll -o jit_ir.s  # 查看汇编
opt -O2 /tmp/jit_ir.ll -o jit_ir_opt.ll        # 优化 IR
```

---

## 7. 决策影响

### 对开发者的影响

1. **新增操作必须同步添加 JIT 支持**：修改 `ch_op` 枚举时，必须同步更新 `JitOp`、`generate_ir()`、`compile_to_llvm()` 三处
2. **测试要求**: 新功能必须同时测试解释器和 JIT 模式（`sim.set_jit_enabled(true/false)`）
3. **性能调优**: 关注 IR 指令数和虚拟寄存器数，避免不必要的中间值

### 对用户的影響

1. **JIT 默认启用**: 用户无需手动配置，但需要注意不支持的操作会静默回退
2. **性能预期**: 当前 JIT 性能未达到目标值（因 P0 债务），建议在性能敏感场景先测试
3. **调试建议**: 遇到不一致结果时，先禁用 JIT 确认是否为 JIT 问题

---

## 8. 变更日志

| 日期 | 版本 | 变更 | 作者 |
|------|------|------|------|
| 2026-05-06 | v1.0 | 初始版本，记录 JIT 编译器架构决策和已知债务 | QoderWork |
| 2026-05-06 | v1.1 | 标注 D5 CALL_EXTERNAL 的当前/目标状态，交叉引用 ADR-005 D2，补齐 PRD 任务引用 | QoderWork Review |

---

**相关链接**:
- [架构审查报告](../architecture-review-report.md)
- [JIT 调试指南](../developer_guide/patterns/JIT-DEBUGGING-GUIDE.md)
- [JIT IR 位宽修复计划](../superpowers/plans/2026-05-02-jit-ir-bitwidth-fix.md)
- [JIT 编译器路线图](../simulation/JIT_ROADMAP.md)
- [ADR-005: 双模式仿真设计](ADR-005-dual-mode-simulation.md) — D2 说明 CALL_EXTERNAL 的正确执行顺序
- [PRD - 已知架构债务](../adr/PRD.md) — P0 债务列表
- `include/jit/AGENTS.md` — JIT 子系统开发指南

---

**决策人**: AI Agent  
**审阅人**: ________________  
**状态**: 草稿（待评审）
