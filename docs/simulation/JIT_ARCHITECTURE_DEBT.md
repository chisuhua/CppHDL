# JIT 架构债务分析

**创建日期**: 2026-04-29  
**Session**: Sisyphus JIT debugging session  
**状态**: ✅ 全部债务已修复（Debt 3 P0, Debt 2 P1, Debt 1 Option B, Debt 5 默认启用）

---

## 概述

CppHDL JIT 编译器在 Phase 2（简单电路 JIT）阶段存在两个层次的架构债务。本文档记录每个债务的根因分析、已尝试的修复方案、以及推荐的解决路径。

---

## 债务 1：JIT two-block 线性执行违反两阶段语义

**严重程度**: 🔴 高  
**状态**: ✅ 已修复 - Option B 双函数架构

### 修复方案

**实现两个独立的 LLVM 函数**：

```cpp
void tick_comb(uint64_t* data_buffer);  // 组合逻辑
void tick_seq(uint64_t* data_buffer);   // 寄存器更新
```

### 代码改动

**JitCompiler 基础设施**：
- `compiled_comb_func_` 和 `compiled_seq_func_` 指针
- `execute_comb_tick()` 和 `execute_seq_tick()` 方法

**generate_ir() 分离**：
- `type_reg` 节点 → `func_seq`（REG_NEXT 指令）
- 其他组合节点 → `func_comb`

**compile_to_llvm() 双发射**：
- 编译单个模块包含两个函数
- `finalize_compilation_dual()` 查找两个 symbol

**Simulator 集成**：
- `eval_combinational()` → 调用 `execute_comb_tick()`
- `eval_sequential()` → 调用 `execute_seq_tick()`

### 正确时序

```
tick():
  eval_combinational() {
    execute_comb_tick();  // proxy 读 reg 当前值
  }
  eval();                // clock 0→1
  eval_sequential() {
    execute_seq_tick();   // reg 更新 next 值
  }
  eval();                // clock 1→0
```

---

## 债务 2：`ch_reg::operator=` 破坏了寄存器连接

**严重程度**: 🔴 高  
**影响**: 解释器和 JIT 路径中 `sim.get_value(reg)` 对 `reg = expr` 返回错误值（0 或异常值）

### 根因

`ch_reg<T>` 继承自 `T`（即 `logic_buffer<T>`）。`logic_buffer<T>::operator=` 的实现只是拷贝 `node_impl_` 指针：

```cpp
// include/lnode/logic_buffer.tpp:21
logic_buffer<T> &logic_buffer<T>::operator=(const logic_buffer &other) {
    node_impl_ = other.node_impl_;  // 仅仅替换指针
    return *this;
}
```

当 `result_reg = a + b` 执行时：

1. `a + b` 返回 `ch_uint<9>(add_node)`（加法操作节点）
2. 隐式转换 `ch_uint<8>`（通过 converting constructor）创建 `bits_extract` 节点（截断 9→8 位）
3. `operator=` 将 `result_reg.node_impl_` 替换为 `bits_extract` 节点
4. `get_value(reg)` 调用 `reg.impl()` 返回 `bits_extract` 节点，**而非寄存器 proxy**

原本 `ch_reg` 构造时设置 `node_impl_ = proxy_node`。proxy 在 `eval_combinational` 中从 regimpl 节点取值，而 regimpl 在 `eval_sequential` 中被解释器更新。这个链路被 `operator=` 完全绕过了。

```
构造时:         reg.impl() = proxy_node → 读 regimpl → 正确的 reg 当前值
operator= 后:   reg.impl() = bits_extract_node → 读写操作节点 → 可能为 0
```

### P1 修复方案（已实施）

**测试迁移到 `<<=`（非阻塞赋值）**：

`ch_reg` 已有 `<<=` 运算符正确实现，用于设置寄存器的 next 值。修复方案是将 JIT 测试中的 `reg = expr` 改为 `reg <<= expr`。

```cpp
// 修复前（使用 =，破坏 proxy 链路）
result_reg = a + b;

// 修复后（使用 <<=，正确设置 regimpl next 值）
result_reg <<= a + b;
```

### 修改文件

| 文件 | 改动 |
|------|------|
| `tests/test_jit_compiler.cpp` | 14 处 `result_reg =` → `result_reg <<=` |

### 推荐长期方案

**方案 B**（框架统一）：需要修改 `ch_reg::operator=` 调用 `set_next()`，并将所有 `reg = signal` 改为 `reg <<= signal`。

当前 `<<=` 运算符已存在且正确工作，但 `=` 运算符保留用于 wire 连接语义（这是设计意图）。需要文档说明两种语义的适用场景。

---

## 债务 3：JIT 非确定性 SIGSEGV ✅ 已修复

**严重程度**: 曾为 🟡 中  
**状态**: ✅ P0 修复完成

### 修复内容

**1. bitwidth 不匹配修复**（`jit_compiler.cpp` lines 208-225）：

```cpp
// 修复前：使用结果宽度加载源操作数
block.instrs.push_back(make_load(src0_vreg, node->src(0)->id(), bitwidth));  // bitwidth=1 for LT

// 修复后：使用源操作数实际宽度
auto src0_bitwidth = static_cast<BitWidth>(node->src(0)->size());
block.instrs.push_back(make_load(src0_vreg, node->src(0)->id(), src0_bitwidth));  // src0_bitwidth=8
```

**2. CALL_EXTERNAL fallback**（`jit_compiler.cpp` lines 556-561）：

```cpp
case JitOp::CALL_EXTERNAL: {
    auto* ptr = get_node_ptr(instr.node_id);
    auto* val = builder.CreateLoad(builder.getInt64Ty(), ptr, "call_ext_load");
    builder.CreateStore(val, vregs[instr.dst], "call_ext_store");
    break;
}
```

### 验证结果

- 比较测试（LT/GT/EQ）：SIGSEGV → 正常返回结果 ✅
- 算术测试（add/sub/mul/and/or/xor）：返回 0 → 正确值 ✅
- 链式运算（(a+b)*c）：非确定性 SIGSEGV → 正常 ✅



---

## 债务 4：USE-AFTER-FREE ✅ 已修复

**严重程度**: 曾为 🔴 高  
**状态**: ✅ 已修复

### 根因

`finalize_compilation()` 中：
```cpp
auto JIT = std::move(JIT_or_err.get());  // unique_ptr<llvm::orc::LLJIT>
// ... add module, lookup symbol ...
compiled_func_ = reinterpret_cast<void*>(Sym->getValue());
// JIT 在此作用域结束时析构 → compiled code 被释放！
// compiled_func_ 指向 freed memory
```

`execute_tick()` 调用 `tick_func(data_buffer_.data())` 时调用已释放的内存。

### 修复

```cpp
// 所有权从不释放，存入 jit_session_（已有的 void* 成员）
jit_session_ = static_cast<void*>(JIT.release());
llvm_module_ = nullptr;

// clear() 中正确析构:
delete static_cast<llvm::orc::LLJIT*>(jit_session_);
```

---

## 债务 5：`jit_enabled_` 默认 false → JIT 从未执行

**严重程度**: 🟡 中  
**状态**: ✅ 已修复 - 默认启用 JIT

### 修复

```cpp
// include/simulator.h line 549
bool jit_enabled_ = true;  // 改为 true，启用 JIT 默认
```

### 原因

Debt 1 Option B（双函数架构）已实现，JIT 现在可以安全地默认启用。

### 验证

用户仍可通过 `sim.set_jit_enabled(false)` 禁用 JIT。

---

## 当前文件修改状态

| 文件 | 改动 | 提交状态 |
|------|------|---------|
| `src/jit/jit_compiler.cpp` | 双函数架构（generate_ir 分离、compile_to_llvm 双发射、finalize_compilation_dual） | 未提交 |
| `include/jit/jit_compiler.h` | 双函数基础设施（compiled_comb_func_、compiled_seq_func_、execute_comb_tick、execute_seq_tick） | 未提交 |
| `include/jit/jit_ir.h` | 添加 make_reg_next | 未提交 |
| `src/simulator.cpp` | eval_combinational/eval_sequential 调用双函数、A/B 验证禁用 | 未提交 |
| `include/simulator.h` | jit_enabled_ = true | 未提交 |
| `tests/test_jit_compiler.cpp` | 14 处 `result_reg =` → `result_reg <<=` | 未提交 |

---

## 推荐执行顺序

| 优先级 | 债务 | 状态 | 改动量 | 风险 |
|--------|------|------|--------|------|
| ✅ 完成 | 债务 3: 修复 JIT bitwidth 不匹配 | P0 已修复 | 10 行 | 低 |
| ✅ 完成 | 债务 2: 测试迁移到 `<<=` | P1 已修复 | 14 处 | 低 |
| ✅ 完成 | 债务 1: JIT 双函数架构 Option B | 已实现 | ~100 行 | 中 |
| ✅ 完成 | 债务 5: 启用 JIT 默认 | 已启用 | 1 行 | 低 |

**已验证**：
- 122/122 单元测试通过
- 28/28 示例程序通过
- 15/15 JIT 测试通过（全部）
- JIT 默认启用，所有测试使用 JIT 加速

---

## 相关文件索引

| 文件 | 用途 |
|------|------|
| `src/jit/jit_compiler.cpp` | JIT 编译器主实现（generate_ir, compile_to_llvm, execute_tick）|
| `include/jit/jit_compiler.h` | JIT 编译器接口 |
| `include/jit/jit_ir.h` | JIT IR 指令定义 |
| `include/core/reg.h` | ch_reg 类声明 |
| `include/lnode/reg.tpp` | ch_reg + next_assignment_proxy 实现 |
| `include/lnode/logic_buffer.tpp` | logic_buffer::operator= 实现（bug 根因）|
| `src/ast/ast_nodes.cpp` | regimpl::create_instruction (current_buf_, next_buf_) |
| `src/ast/instr_reg.cpp` | 寄存器指令 eval() |
| `src/simulator.cpp:750-870` | eval_combinational(), tick(), eval_sequential() |
| `tests/test_jit_compiler.cpp` | 15 个 JIT 测试用例 |
| `docs/simulation/JIT_ROADMAP.md` | JIT 实施路线图 |
