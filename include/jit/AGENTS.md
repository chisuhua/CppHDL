# AGENTS.md - JIT Compiler Subsystem

> 子 `AGENTS.md`（根 `AGENTS.md`）。JIT 编译器负责将 HDL 节点图编译为 LLVM IR 并 JIT 执行。

## OVERVIEW

JIT 编译器有两个核心函数：
- **`generate_ir()`**: 将 context 的 `eval_list` 中的节点编译为 JIT IR 指令
- **`compile_to_llvm()`**: 将 JIT IR 降低为 LLVM IR 并通过 ORC JIT 编译为原生代码

## STRUCTURE

```
include/jit/
├── jit_compiler.h   # JitCompiler 类声明
└── jit_ir.h         # JitOp 枚举、JitInstr 结构、辅助函数

src/jit/
└── jit_compiler.cpp # IR 生成 + LLVM lowering 实现
```

## 关键规则（CRITICAL）

### 规则 1: 新增 HDL 操作必须同步 JIT

**触发条件**: 在 `include/core/lnodeimpl.h` 的 `ch_op` 枚举中添加新操作

**必须修改**:
1. `include/jit/jit_ir.h` — 在 `JitOp` 添加对应枚举值
2. `src/jit/jit_compiler.cpp`:
   a. `generate_ir()` 的 `switch(ch_op)` 中添加映射
   b. `compile_to_llvm()` 的 `switch(instr.op)` 中添加 LLVM lowering

**如果跳过 JIT 支持**: 操作默认成为 `CALL_EXTERNAL`，依赖链中的下游 JIT-native 操作将读到陈旧值，导致**静默错误**（非崩溃）。

### 规则 2: `type_input` 节点必须检查 driver

`inputimpl` 可通过 `set_driver()` 设置驱动源（Component IO 连线 `<<=` 时使用）。JIT 的 `generate_ir()` 必须处理此情况：

```cpp
// ✅ 正确：有 driver 时从 driver 加载
if (node->num_srcs() > 0 && node->src(0)) {
    load_from(node->src(0)->id());
} else {
    load_from(node_id); // 直接输入（set_input_value）
}
```

### 规则 3: `type_lit` 使用 `continue`，不是 `break`

`type_lit` 的 case 在 `for (auto* node : eval_list)` 循环内。使用 `break` 会跳出整个循环，导致所有后续节点被跳过。

### 规则 4: <64-bit 操作必须 mask

所有算术/位操作的结果必须以目标位宽 mask：
```cpp
if (instr.bitwidth < 64) {
    uint64_t mask = (1ULL << instr.bitwidth) - 1;
    res = builder.CreateAnd(res, builder.getInt64(mask), "mask");
}
```

## ANTI-PATTERNS

| 反模式 | 后果 | 正确做法 |
|--------|------|---------|
| 新增 `ch_op` 不加 `JitOp` | 操作 CALL_EXTERNAL，下游静默读陈旧值 | 同步添加三处 |
| `type_input` 始终自加载 | Component IO 连线不工作 | 检查 `num_srcs() > 0` |
| `type_lit` case 用 `break` | 跳过所有后续节点 | 用 `continue` |
| 算术结果不 mask | 窄位宽值溢出/带垃圾位 | 加 `if (bw<64) AND mask` |
| `load_node` 用 Trunc 不用 AND | 存 iN 读 i64 时上 56 位是垃圾 | 用 AND 代替 Trunc |

## WHERE TO LOOK

| 任务 | 位置 |
|------|------|
| 添加新 JIT 操作 | `include/jit/jit_ir.h` (JitOp), `src/jit/jit_compiler.cpp` (mapping + lowering) |
| 修改 eval 顺序 | `src/simulator.cpp` (`eval_combinational`, `eval_sequential`) |
| 解释器参考实现 | `include/ast/instr_op.h` (BitSel::eval, BitsExtract::eval 等) |
| JIT 编译入口 | `src/simulator.cpp:1098-1110` (`init_jit()`) |
| JIT 执行入口 | `src/simulator.cpp:768-800` (`eval_combinational`) |

## 调试

参见 `docs/developer_guide/patterns/JIT-DEBUGGING-GUIDE.md`。

快速诊断:
```cpp
sim.set_jit_enabled(false); // 禁用 JIT 确认是否为 JIT 问题
```

## 相关文件

- `docs/developer_guide/patterns/JIT-DEBUGGING-GUIDE.md` — 系统化 JIT 调试指南
- `docs/superpowers/plans/2026-05-02-jit-ir-bitwidth-fix.md` — JIT IR 位宽修复计划
- `include/core/lnodeimpl.h` — `ch_op` 枚举（所有操作定义）
- `include/ast/instr_op.h` — 解释器操作实现（JIT lowering 参考）

---

**维护**: AI Agent
**版本**: v1.0
**最后更新**: 2026-05-06
