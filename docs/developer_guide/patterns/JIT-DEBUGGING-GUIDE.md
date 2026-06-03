# JIT Debugging Guide

> 如何诊断和修复 CppHDL JIT 编译器引起的测试失败

## 概述

CppHDL 使用 LLVM ORC JIT 将 HDL 节点图编译为原生代码以加速仿真。当 JIT 产生的结果与解释器不一致时，需要系统化分析。

## 快速诊断流程

### Step 1: 确认是否为 JIT 问题

```cpp
// 临时禁用 JIT 验证
sim.set_jit_enabled(false);
```

如果禁用 JIT 后测试通过 → JIT 问题。如果仍失败 → 解释器 bug（非 JIT）。

### Step 2: 检查 JIT 编译状态

```
[INFO] JIT compilation successful: 194 IR instructions, 168 vregs, 0 ns   ← 正常
[WARN] JIT compilation failed: (ir_instr_count=0)                           ← 异常
```

`ir_instr_count=0` 表示组合逻辑函数未生成任何 IR 指令。常见原因：
- 所有操作均为 `CALL_EXTERNAL`，JIT 无法处理
- `type_lit` 节点使用了 `break` 而非 `continue`（会跳出整个循环）

### Step 3: 追踪数据流

从失败断言反向追踪到根节点：

```
测试输出: grant=0b0001, 期望=0b0100
    ↓
MUX(select) 选择了错误输入
    ↓
BIT_SELECT 返回了错误的 bit 值
    ↓
pos 索引值为 0（应为 2），因为 MOD 结果是陈旧值
    ↓
MOD 是 CALL_EXTERNAL，JIT 未计算
```

## 常见 JIT Bug 模式

### Bug 1: 操作未在 JitOp 枚举中注册

**症状**: 特定操作使用解释器回退，导致依赖链中的后续节点读到陈旧值。

**根因链路**:
```
JIT-native 操作 → CALL_EXTERNAL 操作 → JIT-native 操作
     ✅               ❌ (陈旧值)          ❌ (读陈旧值)
```

**修复**: 将该操作添加为 JIT-native。

需要修改的文件:
1. `include/jit/jit_ir.h` — 在 `JitOp` 枚举中添加新操作
2. `src/jit/jit_compiler.cpp` — 两处修改:
   a. `generate_ir()`: 在 `switch(ch_op)` 中添加 `case ch::core::ch_op::<op>: jit_op = JitOp::<OP>;`
   b. `compile_to_llvm()`: 在 `switch(instr.op)` 中添加 LLVM IR lowering

**LLVM Lowering 模板**:
```cpp
case JitOp::NEW_OP: {
    auto* a = builder.CreateLoad(builder.getInt64Ty(), vregs[instr.src0], "load_a");
    auto* b = builder.CreateLoad(builder.getInt64Ty(), vregs[instr.src1], "load_b");
    auto* res = builder.Create<LLVM_OP>(a, b, "op");
    if (instr.bitwidth < 64) {
        uint64_t mask = (1ULL << instr.bitwidth) - 1;
        res = builder.CreateAnd(res, builder.getInt64(mask), "mask");
    }
    builder.CreateStore(res, vregs[instr.dst], "store");
    break;
}
```

### Bug 2: Input 节点未追踪 driver

**症状**: Component 层级中 `ch_module` 的 IO 端口通过 `<<=` 连接后，子模块输入端口始终为 0。

**根因**: `inputimpl::set_driver()` 设置了 `src(0)` 指向 driver 节点，但 JIT 的 `generate_ir()` 对 `type_input` 始终从自身 `node_id` 加载，忽略了 driver。

**正确实现**:
```cpp
case ch::core::lnodetype::type_input: {
    auto vreg = next_comb_vreg++;
    if (node->num_srcs() > 0 && node->src(0)) {
        // 有 driver → 从 driver 节点加载
        auto src_bw = static_cast<BitWidth>(node->src(0)->size());
        block_comb.instrs.push_back(make_load(vreg, node->src(0)->id(), src_bw));
    } else {
        // 无 driver → 自加载（值由 set_input_value 设置）
        block_comb.instrs.push_back(make_load(vreg, node_id, bitwidth));
    }
    block_comb.instrs.push_back(make_store(node_id, vreg, bitwidth));
    break;
}
```

### Bug 3: 解释器/CALL_EXTERNAL 与 JIT 执行顺序

**症状**: JIT 编译成功但输出值不正确。

**根因**: `CALL_EXTERNAL` 节点由解释器在 JIT 之后执行，导致 JIT 计算时读到陈旧值。

**修复**: 在 `eval_combinational()` 和 `eval_sequential()` 中，将解释器回退移到 JIT 之前:

```cpp
// ✅ 正确顺序
// 1. 先执行 CALL_EXTERNAL 解释器节点
for (const auto &[node_id, instr] : instr_list) {
    if (jit_compiler_->is_external_node(node_id)) {
        instr->eval();
    }
}
// 2. 同步到 JIT buffer
jit_compiler_->sync_to_buffer(data_map_);
// 3. JIT 执行（使用已更新的值）
jit_compiler_->execute_comb_tick();
// 4. 同步回 data_map_
jit_compiler_->sync_from_buffer(data_map_);
```

### Bug 4: Concat on Output Ports (RESOLVED, 2026-06-02)

**症状**:
- 输出端口读到的值比预期晚 1 个时钟周期，使用 `concat()` 时尤为明显
- 警告：`[instr_op_concat] Warning: Destination width mismatch! dest width=8, expected: 13, src0 width=8 src1_width=5`
- 警告：`Value not found for port node ID: <output-id>`
- JIT 编译失败，提示 `ir_instr_count=10`（或其他非零值，但 `JIT compilation successful` 未出现）

**根因**: 三个独立的架构缺陷叠加造成：

1. **节点 ID 别名冲突** — `ch::ch_device` 会创建两个 context：`top_ctx` 用于 IO 端口，组件自带的子 context 用于 ops。两者各自维护 `next_node_id_` 计数器，因此都会为各自的第一个非 clock/reset 节点发出 `id=2`。`eval_list` 沿 `src_` 边遍历会同时进入两个 context，JIT 因此看到两个 `id==2` 的节点，CONCAT 结果每 tick 都会覆盖 input `a`。JIT `data_buffer_` 按 `node->id()` 索引，两条写入会互相破坏。
2. **`ch_device` 拆分 context 导致 output port 丢失** — Output port 位于 `top_ctx`，但 Simulator 的 `data_map_` 由 `top` 的 `eval_list` 构造。拓扑排序不会回溯到 `top_ctx`，因此 output port 永远拿不到自己的值，`get_port_value(result)` 返回 0。
3. **`tick_seq` 符号缺失** — `compile_to_llvm` 在函数体为空（例如纯组合逻辑设计）时短路，不会发出 `Function` 声明，导致后续 `JIT->lookup("tick_seq")` 失败。错误信息也是空的，无法定位。

**修复**（提交在 plan `.omo/plans/jit-concat-lowering.md`）:
- `src/core/context.cpp`：将 `next_node_id()` 改为进程级 `std::atomic<uint32_t>`，保证跨 context 唯一
- `include/device.h`：`top_->build(ctx_.get())` — 把 device ctx 传给顶层组件，让 IO 和 ops 共享同一个 context（顺带消除缺陷 #2）
- `src/jit/jit_compiler.cpp`：无论函数体是否为空，都发出 `Function` 符号
- `src/jit/jit_compiler.cpp`：将 `LLVMError` 捕获到 `last_error_msg_`，失败原因可诊断
- `src/simulator.cpp`：编译失败时打印 `last_error_msg()`

**验证**:
- `ctest -R "jit_concat"`：5/6 通过（1 个已知的移位寄存器时序初始化缺陷，见下）
- `ctest -L base`：122/123（与改动前持平，无回归）
- `./run_all_ported_tests.sh`：28/28 通过
- JIT 编译日志：`JIT compilation successful: 16 IR instructions, 13 vregs`
- `src0 width=8` 与 `Value not found` 警告已消失

**已知遗留问题**：`test_jit_concat` 中的移位寄存器测试暴露出一个独立的 JIT 序贯初始化缺陷 —— `REG_NEXT` 在 tick 0 上会覆盖 init 值（0）为 next 值（1），发生在 Simulator 最终 `eval_combinational()` 重新读取寄存器的 output port 之前。这是一处独立缺陷，不在本次修复范围内；解释器路径仍是序贯逻辑的真相来源。

详细 debug 笔记：`.omo/evidence/task-6-debug-notes.md`

## LLVM Lowering 参考

### BIT_SELECT (动态位选择)
```cpp
case JitOp::BIT_SELECT: {
    auto* data = builder.CreateLoad(builder.getInt64Ty(), vregs[instr.src0], "load_data");
    auto* idx = builder.CreateLoad(builder.getInt64Ty(), vregs[instr.src1], "load_idx");
    auto* shifted = builder.CreateLShr(data, idx, "shift_idx");
    auto* masked = builder.CreateAnd(shifted, builder.getInt64(1), "mask_bit");
    builder.CreateStore(masked, vregs[instr.dst], "store_bit_sel");
    break;
}
```

### MOD (取模，带零除保护)
```cpp
case JitOp::MOD: {
    auto* a = builder.CreateLoad(builder.getInt64Ty(), vregs[instr.src0], "load_a");
    auto* b = builder.CreateLoad(builder.getInt64Ty(), vregs[instr.src1], "load_b");
    auto* is_zero = builder.CreateICmpEQ(b, builder.getInt64(0), "is_zero");
    auto* rem = builder.CreateURem(a, b, "mod");
    auto* res = builder.CreateSelect(is_zero, a, rem, "mod_safe");
    if (instr.bitwidth < 64) {
        uint64_t mask = (1ULL << instr.bitwidth) - 1;
        res = builder.CreateAnd(res, builder.getInt64(mask), "mask_mod");
    }
    builder.CreateStore(res, vregs[instr.dst], "store_mod");
    break;
}
```

### BITS_EXTRACT (位范围提取 [msb:lsb])
```cpp
case JitOp::BITS_EXTRACT: {
    auto* val = builder.CreateLoad(builder.getInt64Ty(), vregs[instr.src0], "load_val");
    auto* range = builder.CreateLoad(builder.getInt64Ty(), vregs[instr.src1], "load_range");
    auto* lsb = builder.CreateAnd(range, builder.getInt64(0xFFFFFFFF), "lsb");
    auto* msb = builder.CreateLShr(range, builder.getInt64(32), "msb_shift");
    msb = builder.CreateAnd(msb, builder.getInt64(0xFFFFFFFF), "msb");
    auto* shifted = builder.CreateLShr(val, lsb, "shift_val");
    auto* width = builder.CreateSub(msb, lsb, "width_sub");
    width = builder.CreateAdd(width, builder.getInt64(1), "width_add1");
    auto* mask = builder.CreateShl(builder.getInt64(1), width, "mask_shl");
    mask = builder.CreateSub(mask, builder.getInt64(1), "mask_sub1");
    auto* res = builder.CreateAnd(shifted, mask, "extract");
    if (instr.bitwidth < 64) {
        uint64_t bw_mask = (1ULL << instr.bitwidth) - 1;
        res = builder.CreateAnd(res, builder.getInt64(bw_mask), "mask_bw");
    }
    builder.CreateStore(res, vregs[instr.dst], "store_bits_extract");
    break;
}
```

## 验证清单

修复 JIT 问题后，验证三步：

```bash
# 1. 编译
cmake --build build -j$(nproc)

# 2. 运行失败测试
ctest -R "<test_name>" --output-on-failure

# 3. 确认 JIT 编译成功（无 ir_instr_count=0 警告）
./build/tests/<test_binary> 2>&1 | grep -E "JIT compilation|WARN"
```

## 相关文件

| 文件 | 用途 |
|------|------|
| `src/jit/jit_compiler.cpp` | JIT IR 生成 + LLVM lowering |
| `include/jit/jit_ir.h` | JitOp 枚举 + IR 指令结构 |
| `include/jit/jit_compiler.h` | JitCompiler 类接口 |
| `src/simulator.cpp` | `eval_combinational()` / `eval_sequential()` - JIT 执行入口 |
| `include/simulator.h` | `set_jit_enabled()` / `is_jit_enabled()` |
| `include/core/lnodeimpl.h` | `ch_op` 枚举（所有 HDL 操作） |
| `include/ast/instr_op.h` | 解释器指令 eval() 实现（JIT lowering 的参考） |

---

**维护**: AI Agent  
**版本**: v1.0  
**最后更新**: 2026-05-06
