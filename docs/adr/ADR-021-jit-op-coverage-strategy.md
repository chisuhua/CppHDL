# ADR-021: JIT 操作覆盖策略

**状态**: ✅ 已采纳
**日期**: 2026-05-08
**决策人**: Sisyphus + 用户（参考 Explore + Oracle 分析）

---

## 1. 背景

JIT 编译器（`src/jit/jit_compiler.cpp`）通过 `generate_ir()` 将 `ch_op` 枚举映射为 `JitOp`，再经 `compile_to_llvm()` 降低为 LLVM IR。当前映射覆盖存在以下问题：

### 1.1 `generate_ir()` 映射覆盖（`jit_compiler.cpp:291-311`）

| 状态 | ch_op 数量 | 行为 |
|------|-----------|------|
| ✅ 已映射到 JitOp | 18 个 | add, sub, mul, mod, and_, or_, xor_, not_, eq, ne, lt, le, gt, ge, shl, shr, bit_sel, bits_extract, concat |
| ❌ default → CALL_EXTERNAL | 14 个 | div, sshr, neg, sext, zext, bits_update, and_reduce, or_reduce, xor_reduce, rotate_l, rotate_r, popcount, assign, mux |
| ⚠️ 有 JitOp 但 compile_to_llvm 拒绝 | 1 个 | **concat**（映射到 CONCAT 但 compile_to_llvm() 返回 UNSUPPORTED_OP） |

### 1.2 核心缺陷

| 缺陷 | 位置 | 影响 |
|------|------|------|
| **CALL_EXTERNAL 静默降级** | `generate_ir()` 314-317 行 | JIT 跳过外部节点，由解释器处理，但下游 JIT-native 节点读到的是前一周期值（拓扑序限制） |
| **CONCAT 映射死锁** | `generate_ir()` 310 行映射 CONCAT，但 `compile_to_llvm()` 408-415 行拒绝 | 整个 JIT 编译失败，退回到纯解释器 |
| **AND/OR/XOR 缺 mask** | `compile_to_llvm()` 541-562 行 | 虽实际安全（输入已 mask），但违反 AGENTS.md <64-bit mask 规则 |
| **CALL_EXTERNAL LLVM 处理为死代码** | `compile_to_llvm()` 703-708 行 | 从不被执行（generate_ir 已跳过） |

---

## 2. 讨论过程

### 2.1 Explore 代码审计

| 发现 | 详情 |
|------|------|
| 混合模式稳定性 | `simulator.cpp:776-784` 中解释器在 JIT 前运行，外部节点值已是最新，但 **JIT→EXTERNAL→JIT** 路径因拓扑序限制产生静默损坏 |
| CONCAT | `generate_ir()` 映射到 `JitOp::CONCAT` 但 `compile_to_llvm()` 立即拒绝 → 整个 JIT 编译失败 |
| Mux | 正确通过 `type_mux` 专用路径 → `JitOp::SELECT` → 正确 LLVM lowering |
| Mask 覆盖 | 10 处有 mask，AND/OR/XOR/SELECT 缺但安全（load_node 已 mask） |
| 测试覆盖 | **零测试**涉及混合模式或 CALL_EXTERNAL 触发操作 |

### 2.2 Oracle 架构分析

| 问题 | 推荐方案 | 理由 |
|------|---------|------|
| Q1: FAIL FAST vs 混合模式 | **FAIL FAST** | 混合模式下 `JIT→EXTERNAL→JIT` 路径必定产生静默数据损坏。快速出错的仿真器好于慢速出错的仿真器 |
| Q2: CONCAT 不一致 | **先移除映射**，后实现 LLVM lowering | 当前状态不一致（IR 认为支持，LLVM 拒绝）。先一致化到 CALL_EXTERNAL→FAIL FAST |
| Q3: 与 Verilog 对齐 | **批量但仅为语义一致性** | JIT (LLVM IR) 和 Verilog (字符串) 无共享实现路径 |
| Q4: AND/OR/XOR mask | **补上** | 极低风险，消除边界情况 |

---

## 3. 决议

### 3.1 Q1: FAIL FAST — 拒绝 CALL_EXTERNAL 混合模式

`generate_ir()` 中遇到需要 CALL_EXTERNAL 的操作时，直接返回编译错误，迫使仿真器完全退回到解释器模式。

**理由**：
1. 混合模式下 `JIT→EXTERNAL→JIT` 依赖链必定产生静默数据损坏
2. 正确但较慢（解释器）优于快速但错误（混合模式）
3. 架构清晰：JIT 要么全部加速，要么不使用

**改动**：
- `jit_compiler.cpp:314-317`：将 `continue` 改为返回 `JitResult::IR_GENERATION_FAILED`
- `include/jit/AGENTS.md`：更新规则 1，明确 CALL_EXTERNAL 不可接受

### 3.2 Q2: CONCAT — 先移除映射，后实现 LLVM lowering

**阶段一（立即）**：从 `generate_ir()` 映射中移除 `ch_op::concat → JitOp::CONCAT`，使其统一走 CALL_EXTERNAL → FAIL FAST。

**阶段二（短期）**：实现 CONCAT 的 LLVM lowering（`shl` + `or`），使用 `JitInstr::imm` 存储 src1 宽度。

**改动**：
- 阶段一：`jit_compiler.cpp:310` 移除 concat case
- 阶段二：`jit_ir.h` 更新 + `compile_to_llvm()` 添加 CONCAT lowering

### 3.3 Q3: 与 Verilog codegen 对齐策略

采用**批量但松散对齐**策略：
- 同一批次（1-2 天内）实现 JIT 和 Verilog 都缺失的操作
- 共享语义参考：`include/ast/instr_op.h` 中解释器的 eval() 实现定义了正确语义
- 共享测试用例：同一组测试验证解释器、JIT、Verilog 输出一致性
- **不共享代码**：JIT（LLVM IR）和 Verilog（字符串生成）无重叠实现路径

### 3.4 Q4: AND/OR/XOR 补 mask

在 `compile_to_llvm()` 的 AND、OR、XOR case 中添加 `<64` bitwidth masking。

**改动**：`jit_compiler.cpp:541-562`：

```cpp
// AND — 当前无 mask
auto* res = builder.CreateAnd(a, b, "and");
// 改为：
auto* res = builder.CreateAnd(a, b, "and");
if (instr.bitwidth < 64) {
    uint64_t mask = (1ULL << instr.bitwidth) - 1;
    res = builder.CreateAnd(res, builder.getInt64(mask), "mask_and");
}
```

（OR、XOR 同理）

---

## 4. 执行计划

### 阶段一：立即修复（< 1 小时）

| 步骤 | 内容 | 文件 | 风险 |
|------|------|------|------|
| 1 | AND/OR/XOR 加 `<64` mask | `jit_compiler.cpp:541-562` | 极低 |
| 2 | CONCAT 从 `generate_ir()` 映射移除 | `jit_compiler.cpp:310` | 低（当前已失败） |
| 3 | CALL_EXTERNAL 改为 FAIL FAST | `jit_compiler.cpp:314-317` | 中（消除静默损坏） |
| 4 | 更新 `include/jit/AGENTS.md` 规则 1 | AGENTS.md | 无 |

### 阶段二：短期批量（1-2 天）

| 优先级 | 操作 | JitOp 添加 | LLVM lowering | 工作量 |
|--------|------|-----------|---------------|--------|
| 🔴 高 | div | 新增 DIV | `udiv` | 2h |
| 🔴 高 | sext | 新增 SEXT | 位复制 + concat | 2h |
| 🔴 高 | zext | 新增 ZEXT | 零扩展 | 1h |
| 🟡 中 | neg | 新增 NEG | `sub(0, val)` 或 `neg` | 1h |
| 🟡 中 | concat | 已有 CONCAT | `shl + or` + mask | 2h |
| 🟢 低 | sshr | 新增 ASHR | `ashr` | 1h |
| 🟢 低 | bits_update | 新增 BITS_UPDATE | `shl + or + and` 组合 | 3h |
| 🟢 低 | and_reduce/or/xor | 新增 REDUCE_AND/OR/XOR | 迭代 `and`/`or`/`xor` | 2h |
| ⬜ 延期 | rotate, popcount, assign | — | — | 后续 |

### 阶段三：测试

| 测试类型 | 内容 | 验证方式 |
|---------|------|---------|
| **混合模式安全** | 验证含有 CALL_EXTERNAL 的设计的 JIT 编译失败 | `REQUIRE_FALSE(sim.is_jit_compiled())` |
| **操作等价性** | 每个新支持的操作：JIT on ≡ JIT off | 逐周期对比输出 |
| **回归** | 已有 14 个测试继续通过 | `ctest -R jit` |

---

## 5. 变更日志

| 日期 | 版本 | 变更 | 作者 |
|------|------|------|------|
| 2026-05-08 | v1.0 | 初始版本，记录 JIT 操作覆盖分析和 FAIL FAST 决议 | Sisyphus + 用户 |

---

**相关链接**:
- `src/jit/jit_compiler.cpp:290-311` — `generate_ir()` 操作映射
- `src/jit/jit_compiler.cpp:311-318` — CALL_EXTERNAL 当前处理
- `src/jit/jit_compiler.cpp:408-415` — compile_to_llvm() UNSUPPORTED_OP 门禁
- `src/jit/jit_compiler.cpp:464-714` — compile_to_llvm() LLVM lowering
- `src/jit/jit_compiler.cpp:541-562` — AND/OR/XOR（待加 mask）
- `include/jit/jit_ir.h:11-26` — JitOp 枚举
- `include/jit/jit_compiler.h` — JitCompiler 类
- `include/core/lnodeimpl.h:42-76` — ch_op 枚举
- `src/simulator.cpp:776-784` — JIT/解释器混合执行路径
- `include/jit/AGENTS.md` — JIT 规则
- `include/ast/instr_op.h` — 解释器操作实现（LLVM lowering 参考）
- `docs/adr/ADR-019-verilog-codegen-completeness.md` — Verilog codegen 关联 ADR
- `docs/adr/ADR-DISCUSSION-PLAN.md` — 议题 #15
