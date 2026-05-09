# CPP-JIT-DBUGGING - C++ JIT 编译器系统性调试技能

## 触发场景 (Trigger)

满足以下任一条件时激活本技能：
- JIT 编译失败或行为异常
- SIGABRT/SIGSEGV 在 JIT 生成的代码中
- "JIT compilation successful" 但测试仍然失败
- 涉及 `jit_compiler.cpp`、`jit_ir.h`、`simulator.cpp`

## 调试流程

### 第一阶段：确认 JIT 是否真正编译成功

```
检查日志中是否同时满足：
✓ "[INFO] JIT compilation successful"
✗ "[WARN] JIT compilation failed"

如果成功，检查：
- ir_instr_count > 0
- vreg_count > 0
```

### 第二阶段：检查 IR 生成 (generate_ir)

检查 `src/jit/jit_compiler.cpp` 中 `generate_ir()` 函数：

```cpp
// 1. 检查 node 类型是否被正确处理
for (auto* node : eval_list) {
    auto node_type = node->type();
    
    // 漏掉的类型会导致 crash
    case ch::core::lnodetype::type_lit:  // 必须在 switch 内
    case ch::core::lnodetype::type_input:
    case ch::core::lnodetype::type_op:
    case ch::core::lnodetype::type_mux:
    case ch::core::lnodetype::type_proxy:
    // ...
}

// 2. type_lit 必须用 make_const（嵌入字面值）
case ch::core::lnodetype::type_lit: {
    auto* lit = static_cast<ch::core::litimpl*>(node);
    auto value = static_cast<uint64_t>(lit->value());
    block_comb.instrs.push_back(make_const(vreg, value, bitwidth));  // ✓ 正确
    // 错误: make_load() - 这会读取旧值而非字面值
    break;
}

// 3. 使用 break 而不是 continue（除非确实要跳过）
// break = 跳出 switch，处理下一个 node
// continue = 跳到 for 循环下一个（漏掉 node_count++）
```

### 第三阶段：检查 LLVM 代码生成 (compile_to_llvm)

```cpp
// 1. load_node 必须使用 CreateTrunc 进行安全截断
auto load_node = [&](uint32_t node_id, BitWidth bw) -> llvm::Value* {
    auto* ptr = get_node_ptr(node_id);
    auto* val = builder.CreateLoad(builder.getInt64Ty(), ptr, "load_node");
    if (bw < 64) {
        return builder.CreateTrunc(val, builder.getIntNTy(bw), "trunc");  // ✓ 正确
        // 错误: uint64_t mask = (1ULL << bw) - 1;  // UB 当 bw >= 64
    }
    return val;
};

// 2. store_node 必须使用 CreateZExt 进行零扩展
auto store_node = [&](uint32_t node_id, llvm::Value* val, BitWidth bw) {
    auto* ptr = get_node_ptr(node_id);
    llvm::Value* store_val = val;
    if (bw < 64) {
        store_val = builder.CreateZExt(store_val, builder.getInt64Ty(), "zext");  // ✓ 正确
    }
    builder.CreateStore(store_val, ptr);
};

// 3. 检查所有操作是否有位宽限制的掩码
// 操作后如果有掩码，必须用 LLVM 的 CreateAnd，而不是手动计算掩码
```

### 第四阶段：检查 Simulator 接口

```cpp
// src/simulator.cpp 中的 try_jit_compile()
auto result = jit_compiler_->compile(ctx_);
if (result.result == ch::jit::JitResult::SUCCESS && result.ir_instr_count > 0) {
    jit_compiled_ = true;  // 设置这个标志
}
// 失败时会输出:
CHWARN("JIT compilation failed: %s (ir_instr_count=%u)", ...);
```

## 常见 Bug 模式

| 错误模式 | 正确模式 | 后果 |
|---------|---------|------|
| `(1ULL << bw) - 1` 当 bw >= 64 | `CreateTrunc` | UB |
| `make_load(vreg, node_id, bitwidth)` 用于 type_lit | `make_const(vreg, value, bitwidth)` | 读到旧值 |
| `break` 在 type_lit 处理后 | `break`（正确） | 漏掉后续 nodes |
| `continue` 在 type_lit 处理后 | 取决于需求 | 跳过 node_count++ |
| `bw == 64` 时也做掩码 | bw < 64 才掩码 | 64 位值被截断 |

## 验证清单

```bash
# 检查是否有不安全的掩码计算
grep -n "1ULL << " src/jit/jit_compiler.cpp  # 应该无结果

# 检查 type_lit 是否在 switch 内
grep -A5 "type_lit" src/jit/jit_compiler.cpp | grep -E "case|make_const|make_load"

# 检查 load_node 实现
grep -A10 "auto load_node" src/jit/jit_compiler.cpp
```

## 退出条件

修复完成后：
1. `[INFO] JIT compilation successful` 日志出现
2. `sim.is_jit_compiled() == true` 测试通过
3. 无 SIGABRT/SIGSEGV