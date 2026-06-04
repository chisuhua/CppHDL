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

**已知走 `CALL_EXTERNAL` 的 `ch_op`（2026-06-04 审计）**:

`generate_ir()` 的 `switch(ch_op)` 中**所有 33 个 ch_op 都有 case**（`-Wswitch-enum` 强制完整性），但其中 2 个映射到 `CALL_EXTERNAL`：

| ch_op | 走 CALL_EXTERNAL 的原因 | 替代 JIT 路径 |
|-------|------------------------|---------------|
| `ch_op::mux` | `src/jit/jit_compiler.cpp:378-380` 显式映射为 `CALL_EXTERNAL` | `lnodetype::type_mux` 节点 → `make_select` → `JitOp::SELECT` (line 481) |
| `ch_op::bits_update` | 同上，line 377-380 共享 case 块 | `lnodetype::type_bitsupdate` 节点 → `JitOp::BITS_UPDATE` (line 523) |

**双路径架构说明**:

CppHDL 的 `mux` 和 `bits_update` 有两种节点表示，通过不同创建入口使用：

1. **ch_op 路径**（type_op 节点 + ch_op 枚举）— 由 `include/lnode/operators_ext.h:197, 333` 的 `op_type = ch_op::mux/bits_update` 触发。`generate_ir()` 映射为 `CALL_EXTERNAL`，由 `include/ast/instr_op.h` 的解释器处理。
2. **lnodetype 路径**（专用节点类型）— 由 `include/core/node_builder.h:211, 376` 的 `create_node<muximpl>`/`create_node<bitsupdateimpl>` 触发。`generate_ir()` 直接 emit `JitOp::SELECT`/`JitOp::BITS_UPDATE`，由 `compile_to_llvm()` 原生 lowering。

**生产代码推荐路径**:

- ✅ `node_builder.h::ch_mux2()` / `bits_update()` → 创建 lnodetype 节点 → JIT 原生
- ❌ 直接使用 `ch_select(...)` 运算符重载（`operators_ext.h`）→ 创建 ch_op 节点 → 解释器（CALL_EXTERNAL），**下游 JIT-native 节点可能读到陈旧值**

**JitOp 中已定义但未降低的占位项**:

`compile_to_llvm()` switch 中**无对应 case**的 6 个 JitOp（`src/jit/jit_compiler.cpp:608-611` 显式 NOP）：

- `SLICE`, `MEM_READ`, `MEM_WRITE`, `JUMP`, `BRANCH`, `LABEL`

`generate_ir()` 从不 emit 这些 op（无任何 `jit_op = JitOp::SLICE` 等赋值），所以是**死代码 / 未来内存 JIT 与控制流 JIT 的预留**。**不构成当前功能缺口**。

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

### 规则 5: CALL_EXTERNAL 降低需要源位宽信息（CRITICAL — 学习教训）

**背景**: 
- `JitInstr` 只存储 `bitwidth`（目标位宽），不存储源操作数的原始位宽
- 需要源位宽的操作：`sext`, `zext`, `sshr`, `neg`, `mux`, `bits_update`, `and_reduce`, `or_reduce`, `xor_reduce`, `rotate_l`, `rotate_r`, `popcount`

**原因**:
```
// ❌ 错误：CreateSExt(i64, iN) — 非法 LLVM IR!
//     instr.bitwidth 是目标位宽，不是源位宽
auto* a = load i64 from vregs;
auto* dst_ty = builder.getIntNTy(instr.bitwidth);
auto* res = builder.CreateSExt(a, dst_ty);  // LLVM 后端崩溃！

// ✅ 正确：回退到 CALL_EXTERNAL，由解释器处理
jit_op = JitOp::CALL_EXTERNAL;
```

**新增 CALL_EXTERNAL 操作的标准流程**:
1. `src_bitwidth` 字段已在 `JitInstr` 中（`include/jit/jit_ir.h`），无需添加
2. 在 `generate_ir()` 的 `type_op` case 中填充 `instr.src_bitwidth`：在 `make_binary()` 调用后设置 `instr.src_bitwidth = src0_bitwidth`
3. 然后在 `compile_to_llvm()` 中添加 LLVM lowering:
   ```
   对值 a (源位宽 src_bw = instr.src_bitwidth, 目标位宽 dst_bw = instr.bitwidth):
     sext: trunc i64 a to src_bw → SExt to dst_bw → ZExt to i64 → store
     zext: trunc i64 a to src_bw → ZExt to dst_bw → ZExt to i64 → store
     sshr: trunc i64 a to src_bw → SExt to i64 → AShr → mask → store
     neg:  sub(0, a) → mask to dst_bw → store
   ```
4. 验证：运行 `ctest` 确认所有测试通过，`JIT compilation failed` 警告数量不增加

**警告**: 在 `JitInstr` 扩展前，任何将 `sext/zext/sshr/neg` 从 CALL_EXTERNAL 移出的尝试都会导致 LLVM IR 非法、LLVM 后端崩溃（`SelectionDAGISel` SIGSEGV）。

### 规则 4: <64-bit 操作必须 mask

所有算术/位操作的结果必须以目标位宽 mask：
```cpp
if (instr.bitwidth < 64) {
    uint64_t mask = (1ULL << instr.bitwidth) - 1;
    res = builder.CreateAnd(res, builder.getInt64(mask), "mask");
}
```

## 支持的 JIT 操作

`generate_ir()` 中**所有 33 个 ch_op 都有 case**（`-Wswitch-enum` 强制完整性）。其中 31 个映射为 JIT 原生 JitOp，2 个映射为 `CALL_EXTERNAL`（见上文"已知走 CALL_EXTERNAL 的 ch_op"表格）。下表仅列出需要特别说明语义细节的操作：

- **`concat`** (JitOp::CONCAT) — 语义 `result = (src0 << src1_width) | src1`，其中 `src0` 写入高位、`src1` 写入低位。降低路径：`SHL` + `OR` + `AND mask`（全部使用现有 JitOp）。语义对齐解释器 `Concat::eval`（`include/ast/instr_op.h:416-447`）。**跨任务不变式**：`generate_ir()` 中必须设置 `instr.src_bitwidth = src1_width`（LOW 操作数位宽，即移位量），否则 `compile_to_llvm()` 拿到错误的移位值。
- **`mux` / `bits_update`** — 通过 lnodetype 路径（`type_mux` / `type_bitsupdate` 节点）走 JIT 原生。**ch_op 路径走 CALL_EXTERNAL**。生产代码应使用 `node_builder.h` 的 API 避免 ch_op 路径。

## ANTI-PATTERNS

| 反模式 | 后果 | 正确做法 |
|--------|------|---------|
| 新增 `ch_op` 不加 `JitOp` | 操作 CALL_EXTERNAL，下游静默读陈旧值 | 同步添加三处 |
| `type_input` 始终自加载 | Component IO 连线不工作 | 检查 `num_srcs() > 0` |
| `type_lit` case 用 `break` | 跳过所有后续节点 | 用 `continue` |
| 算术结果不 mask | 窄位宽值溢出/带垃圾位 | 加 `if (bw<64) AND mask` |
| `load_node` 用 Trunc 不用 AND | 存 iN 读 i64 时上 56 位是垃圾 | 用 AND 代替 Trunc |
| 缺少源位宽就降低 `sext/zext/sshr/neg` | 非法 LLVM IR + `SelectionDAGISel` SIGSEGV | 先加 `src_bitwidth` 到 `JitInstr` 再降低 |

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
**版本**: v1.1
**最后更新**: 2026-06-04
