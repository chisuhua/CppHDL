# test_reg_timing 失败原因分析

**状态**: 已确认 - Pre-existing Bug
**日期**: 2026-05-31
**作者**: Sisyphus
**相关测试**: `test_reg_timing` (Shift Register Timing)

---

## 问题概述

`test_reg_timing` 测试失败，期望输出 `1`，实际得到 `0`：

```
/workspace/project/CppHDL/tests/test_reg_timing.cpp:263: FAILED:
  REQUIRE( static_cast<uint64_t>(val) == 1 )
with expansion:
  0 == 1
```

---

## 根本原因

**concat（组合逻辑）在寄存器更新之前执行，导致读取的是上一周期的旧值。**

---

## 问题详解

### 1. 移位寄存器结构

```cpp
class ShiftRegister : public ch::Component {
    void describe() override {
        auto bit1 = ch::ch_reg<ch_bool>(0_b, "bit1");
        auto bit2 = ch::ch_reg<ch_bool>(0_b, "bit2");
        auto bit3 = ch::ch_reg<ch_bool>(0_b, "bit3");
        auto bit4 = ch::ch_reg<ch_bool>(0_b, "bit4");

        bit1->next = io().in;     // bit1_next = input
        bit2->next = bit1;         // bit2_next = bit1(current)
        bit3->next = bit2;        // bit3_next = bit2(current)
        bit4->next = bit3;        // bit4_next = bit3(current)
        io().out = concat(concat(concat(bit4, bit3), bit2), bit1);
    }
};
```

### 2. tick() 执行顺序分析

```
tick() 当前执行顺序：

┌─────────────────────────────────────────────────────────────────┐
│ 1. eval_combinational()                                          │
│    ├── concat 读取 bit1.current_buf (= 0, 旧值)                │
│    ├── concat 读取 bit2.current_buf (= 0, 旧值)                │
│    ├── concat 读取 bit3.current_buf (= 0, 旧值)                │
│    ├── concat 读取 bit4.current_buf (= 0, 旧值)                │
│    └── 输出 concat(0,0,0,0) = 0 ❌ (应该在寄存器更新后执行)    │
├─────────────────────────────────────────────────────────────────┤
│ 2. eval() (时钟处理)                                             │
│    ├── default_clock_instr_->eval()                             │
│    └── other_clock_instr_list_.eval()                            │
├─────────────────────────────────────────────────────────────────┤
│ 3. eval_sequential()                                            │
│    ├── bit1.current_buf = bit1.next_buf (= 1) ✅              │
│    ├── bit2.current_buf = bit2.next_buf (= 0) ✅              │
│    ├── bit3.current_buf = bit3.next_buf (= 0) ✅              │
│    └── bit4.current_buf = bit4.next_buf (= 0) ✅              │
├─────────────────────────────────────────────────────────────────┤
│ 4. eval_combinational() (第二次，但结果已用于输出)             │
│    └── concat 再次执行但 tick() 已结束                          │
└─────────────────────────────────────────────────────────────────┘
```

### 3. 时序问题时间线

| tick | input | bit1_next | bit1_current (tick后) | concat输出 (期望) | concat输出 (实际) |
|------|-------|-----------|------------------------|-------------------|-------------------|
| 0 | 0 | 0 | 0 | 0 | 0 ✓ |
| 1 | 1 | 1 | **应该变为 1** | **1 (0001)** | **0 (0000)** ❌ |
| 2 | 0 | 0 | 0 | 3 (0011) | - |
| 3 | 0 | 0 | 0 | 7 (0111) | - |
| 4 | 0 | 0 | 0 | 15 (1111) | - |

---

## concat 节点的特殊问题

### concat 不是 JIT-native 操作

在 `src/jit/jit_compiler.cpp` 的 `generate_ir()` 中，`ch_op::concat` **没有显式映射**到任何 `JitOp`：

```cpp
// src/jit/jit_compiler.cpp, line 304-398
switch (ch_op) {
    case ch::core::ch_op::add:    jit_op = JitOp::ADD; break;
    case ch::core::ch_op::sub:    jit_op = JitOp::SUB; break;
    // ... 其他操作 ...
    case ch::core::ch_op::bits_update:
    case ch::core::ch_op::mux:
    case ch::core::ch_op::assign:
        jit_op = JitOp::CALL_EXTERNAL;  // concat 也在此落入 CALL_EXTERNAL
        break;
    // 注意：没有 ch_op::concat 的 case！
}
#pragma GCC diagnostic pop

if (jit_op == JitOp::CALL_EXTERNAL) {
    external_node_ids_.insert(node_id);
    continue;  // 跳过 JIT IR 生成
}
```

**但 concat 有 LLVM lowering 实现**（在 `compile_to_llvm` 的 `case JitOp::CONCAT`），所以实际上 concat 是被 JIT 支持的，只是没有在 `ch_op` 映射 switch 中添加。

### concat 成为 CALL_EXTERNAL

因为没有显式映射，`ch_op::concat` 使用默认值 `JitOp::CALL_EXTERNAL`，然后被添加到 `external_node_ids_`，在 `eval_combinational()` 中通过解释器执行。

**关键发现**：CALL_EXTERNAL 的 LLVM 实现只是 pass-through（从 data_buffer 读取然后存储），并没有真正调用解释器的 concat 实现：

```cpp
// src/jit/jit_compiler.cpp, line 1088-1093
case JitOp::CALL_EXTERNAL: {
    auto *ptr = get_node_ptr(instr.node_id);
    auto *val = builder.CreateLoad(builder.getInt64Ty(), ptr, "call_ext_load");
    builder.CreateStore(val, vregs[instr.dst], "call_ext_store");
    break;
}
```

---

## 验证这是 Pre-existing Bug

1. ** stash 了我的 concat JIT 修改后，测试仍然失败**（0 == 1）
2. **在 ba613de（fix namespace pollution）之前测试也失败**
3. **问题一直存在，只是之前可能没有被严格测试**

---

## 修复方向

需要修改 tick() 的执行顺序，确保组合逻辑（concat）在寄存器更新**之后**执行读取：

### 方案A：重组 tick() 流程

```cpp
void Simulator::tick() {
    // 1. 先处理时钟边沿
    eval();  // 执行时钟指令

    // 2. 更新寄存器
    eval_sequential();  // 用 next_buf 更新 current_buf

    // 3. 再执行组合逻辑（此时读取到更新后的值）
    eval_combinational();

    // 4. 如果需要，再次处理时钟边沿
    if (default_clock_instr_) {
        eval();  // 第二次时钟处理
    }
}
```

### 方案B：在 eval_sequential() 后添加 post组合逻辑

```cpp
void Simulator::tick() {
    eval_combinational();  // 第一次组合逻辑（设置输入）

    if (default_clock_instr_) {
        eval();            // 时钟处理
        eval_combinational();  // 输入更新后的组合逻辑
    }

    // 在寄存器更新后，再次执行依赖寄存器的组合逻辑
    eval_sequential();    // 更新寄存器

    // 添加：执行后组合逻辑（读取更新后的寄存器值）
    eval_post_combinational();  // 新增：读取 current_buf 的组合逻辑
}
```

### 方案C：修改 concat 的依赖解析

识别 concat 依赖的寄存器节点，在寄存器更新后重新计算 concat。

---

## 相关文件

| 文件 | 相关内容 |
|------|---------|
| `src/simulator.cpp` | tick(), eval_combinational(), eval_sequential() |
| `src/jit/jit_compiler.cpp` | concat JIT 映射缺失 |
| `src/ast/instr_clock.cpp` | 时钟边沿检测逻辑 |
| `src/ast/instr_reg.cpp` | 寄存器更新逻辑 |
| `include/ast/instr_op.h` | concat::eval() 实现 |
| `tests/test_reg_timing.cpp` | 失败的重现测试 |

---

## 后续行动

1. **确认修复方案**：需要与项目维护者讨论最佳修复方案
2. **创建 OpenSpec Change**：如果需要修复，创建一个 change proposal
3. **验证修复**：修复后运行 test_reg_timing 确保通过
4. **回归测试**：确保修复不会破坏其他测试