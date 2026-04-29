# JIT 架构债务分析

**创建日期**: 2026-04-29  
**Session**: Sisyphus JIT debugging session  
**状态**: 活跃 — 部分修复已应用，核心问题仍待解决

---

## 概述

CppHDL JIT 编译器在 Phase 2（简单电路 JIT）阶段存在两个层次的架构债务。本文档记录每个债务的根因分析、已尝试的修复方案、以及推荐的解决路径。

---

## 债务 1：JIT two-block 线性执行违反两阶段语义

**严重程度**: 🔴 高  
**影响**: JIT 启用时所有算术/比较测试返回错误值或 SIGSEGV

### 根因

解释器的 `tick()` 是两阶段模型：

```
eval_combinational()      # Phase 1: 组合逻辑求值 + proxy 读当前 reg 值
  ↓
clock toggle (0→1)
  ↓
eval_sequential()         # Phase 2: 寄存器捕获 next 值
  ↓
clock toggle (1→0)
  ↓
eval_combinational()      # Phase 3: proxy 读更新后的 reg 值
```

JIT 的 `generate_ir()` 将 `comb_block`（组合逻辑）和 `seq_block`（寄存器更新）编译进同一个 LLVM 函数，线性执行。但 **comb_block 中的 proxy 指令执行时，seq_block 尚未运行**，proxy 读到的是寄存器的旧值（初始值 0），而非更新后的值。

```
LLVM Function:
  comb_block instructions      ← proxy 在这里读 reg（读到 0）
  seq_block instructions       ← reg 在这里更新为 15
  return
```

### 已尝试的修复

| 尝试 | 结果 |
|------|------|
| 调整 `generate_ir()` 拓扑排序 | SIGSEGV（regimpl::create_instruction 中 bitwidth 不匹配）|
| 让 `compile_to_llvm()` 同时编译 blocks[0] 和 blocks[1] | 语义问题依旧，proxy 先于 reg 更新执行 |
| 单独编译两个 LLVM 函数 | 未实现 |

### 当前修复 (P1)

**JIT 不再编译 seq_block。register 更新留给解释器处理。**

改动：`generate_ir()` 中 `type_reg` / `type_mem` / `type_mem_read_port` / `type_mem_write_port` 节点被 `continue` 跳过，不生成任何 IR。`compile_to_llvm()` 中 blocks[1] 处理代码已删除（dead code）。

状态文件: `src/jit/jit_compiler.cpp` (已修改, 未提交)

**结果**: JIT 只处理组合逻辑运算，寄存器更新走解释器 `eval_sequential()`。语义正确，但 JIT 加速效果有限（寄存器更新仍是解释器开销）。

### 推荐方案

两个可选方向：

**方案 A**: 保持现状（JIT 只处理组合逻辑）— 最简单，改动最少

**方案 B**: 编译两个独立的 LLVM 函数

```cpp
// 两个入口函数，分别对应用一个 phase
void tick_comb(uint64_t* data_buffer);  // comb_block
void tick_seq(uint64_t* data_buffer);   // seq_block

// eval_combinational() 中调用 tick_comb
// eval_sequential() 中调用 tick_seq
```

需要改动：
- `compile_to_llvm()` 拆分为两个函数
- `JitCompiler` 存储两个 `compiled_func_` 指针
- `finalize_compilation()` 两次 `lookup()` 两个 symbol
- `eval_sequential()` 新增 JIT hook

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

### P0 修复尝试（已回退）

**尝试**: 给 `ch_reg<T>` 添加自己的 `operator=`，路由到 `next_assignment_proxy::operator=`：

```cpp
// include/core/reg.h
template <typename U> ch_reg &operator=(const U &value);

// include/lnode/reg.tpp
template <typename T> template <typename U>
ch_reg<T> &ch_reg<T>::operator=(const U &value) {
    if (__next__) {
        __next__->next = value;  // → regimpl::set_next()
    }
    return *this;
}
```

**效果**: `reg = expr` 正确设置 regimpl 的 next 值，而非替换 node_impl_。proxy 链路保持完整。

**回退原因**: 导致 **19 个测试失败**。因为 `operator=` 改变了所有 `ch_reg` 赋值的全局语义——原有代码中 `reg = some_signal` 期望的是 wire 连接（`logic_buffer::operator=` 语义），而非寄存器 next 值设置。

```
失败测试: test_converter, test_logic, test_pipeline_lib, test_sequential,
          test_stream, test_switch, test_stream_mux_demux, test_literal_left_shift,
          test_reg_timing, test_operator_connection, test_ch_stream,
          test_stream_pipeline, test_fragment, test_bits_update,
          test_bool_connection, test_uint_compare, test_module,
          test_operation_results, test_jit_compiler
```

**教训**: `ch_reg::operator=` 是一个全局语义变更，影响面太大。需要更精细的方案，例如：
- 只对 JIT 测试路径使用 `reg->next = expr` API
- 或者在框架层面统一 `ch_reg` 和 `logic_buffer` 的赋值语义
- 或者让 Simulator 支持动态 `reinitialize()`（重建 eval_list 和指令）

### 推荐方案

两个可选方向：

**方案 A** (低风险): 不修改 `ch_reg::operator=`，让 JIT 的 `generate_ir()` 处理被替换的 `node_impl_` 链路

- JIT 需要能处理 expression → bits_extract → add_op 的多层链条
- 对 CALL_EXTERNAL 节点也有 fallback（当前 compile_to_llvm 遇到 CALL_EXTERNAL 直接返回 UNSUPPORTED_OP）

**方案 B** (正确但工程量大): 在框架层面统一语义

- `ch_reg::operator=` 必须调用 `set_next()`
- 所有 `reg = signal` 的用法改为 `reg <<= signal`（非阻塞赋值）或通过信号连接机制
- 需要修改所有现有代码和测试

---

## 债务 3：JIT 非确定性 SIGSEGV

**严重程度**: 🟡 中  
**影响**: JIT 启用时部分测试随机崩溃

### 现象

- 简单 context 测试：稳定通过
- 算术测试（add/sub/mul/and/or/xor）：返回 0（而非崩溃）
- 比较测试（LT/GT/EQ）：SIGSEGV
- 链式运算（(a+b)*c）：非确定性 SIGSEGV
- 不同运行之间崩溃点不同

### 疑似根因

1. **bitwidth 不匹配**: `generate_ir()` 中 type_op 的 `bitwidth` 使用操作结果宽度，而非源操作数宽度。对于比较操作（ch_bool 结果，1 bit），源操作数被 1-bit 宽度加载，导致加载错误数据

```cpp
// jit_compiler.cpp generate_ir() for type_op:
auto bitwidth = static_cast<BitWidth>(node->size());  // 操作结果的 bitwidth
// 对于 LT (a < b): bitwidth = 1 (ch_bool 结果)
block.instrs.push_back(make_load(src0_vreg, node->src(0)->id(), bitwidth));
// 这里用 bitwidth=1 加载 8-bit 源操作数 a！
```

2. **CALL_EXTERNAL 节点**: `bits_extract` 等未实现的 op 生成 CALL_EXTERNAL 指令，compile_to_llvm 检测到后返回 UNSUPPORTED_OP，但 timing 不确定

3. **LLVM JIT session 跨测试残留**: 每个 Simulator 创建新的 JitCompiler → 新的 LLJIT，但 LLVM 全局状态可能不干净

### 推荐修复

1. **修复 bitwidth**: 在 `generate_ir()` 中，LOAD 源操作数时使用 `node->src(i)->size()` 而非 `node->size()`
2. **增加 CALL_EXTERNAL 的 compile_to_llvm 处理**: 读取 data_buffer 中的值并存储（fallback to passthrough），而非返回 UNSUPPORTED_OP
3. **添加 JIT 调试日志**: 记录每个指令的 node_id、bitwidth、操作类型

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
**状态**: 代码中已保留 false（安全默认）

### 原因

`Simulator` 构造时调用 `try_jit_compile()` 设置 `jit_compiled_ = true`，但 `jit_enabled_` 保持 `false`（默认值）。`eval_combinational()` 中的 guard：

```cpp
if (jit_enabled_ && jit_compiled_ && ...)  // jit_enabled_ = false → 永远走解释器
```

### 当前处理

`jit_enabled_` 保持 `false` 作为安全默认。用户通过 `sim.set_jit_enabled(true)` 显式启用。当前 JIT 在启用时仍有 bug（债务 1、3），所以不自动启用是正确的。

---

## 当前文件修改状态

| 文件 | 改动 | 提交状态 |
|------|------|---------|
| `src/jit/jit_compiler.cpp` | USE-AFTER-FREE 修复、P1 简化（删除 seq_block）、ir_instr_count 修复 | 未提交 |
| `src/simulator.cpp` | 无实质改动（尝试过 auto-enable，已回退） | 未提交 |
| `include/core/reg.h` | 无改动（P0 operator= 已回退） | 干净 |
| `include/lnode/reg.tpp` | 无改动（P0 operator= 已回退） | 干净 |
| `tests/test_jit_compiler.cpp` | 无改动（尝试过 reg test 重排，已回退） | 干净 |

---

## 推荐执行顺序

| 优先级 | 债务 | 改动量 | 风险 |
|--------|------|--------|------|
| P0 | 债务 3: 修复 JIT bitwidth 不匹配 | 5-10 行 | 低 |
| P1 | 债务 1: JIT 两阶段（方案 A 保持现状 或 方案 B 双函数）| 20-50 行 | 中 |
| P2 | 债务 2: ch_reg::operator= 语义（方案 A 或 B）| 50-200 行 | 高 |

建议先执行 P0（bitwidth fix），这能解决比较测试的 SIGSEGV。然后评估是否执行 P1 和 P2。

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
