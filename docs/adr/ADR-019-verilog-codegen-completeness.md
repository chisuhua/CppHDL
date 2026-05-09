# ADR-019: Verilog 代码生成操作完整性

**状态**: ✅ 已采纳
**日期**: 2026-05-08
**决策人**: Sisyphus + 用户（参考 Oracle + Librarian 分析）

---

## 1. 背景

`src/codegen_verilog.cpp` 中的 `get_op_str()` 函数（第 146-187 行）负责将内部操作类型 `ch_op` 映射为 Verilog 操作符字符串。当前 `ch_op` 枚举（`include/core/lnodeimpl.h:42-76`）定义了 **33 种操作**，但 `get_op_str()` 仅实现其中 **15 种**（算术/逻辑/比较类）。未实现的 18 种操作输出字符串 `"<OP_NOT_IMPLEMENTED>"`，直接注入生成的 Verilog 代码中，产生语法无效的输出。

### 1.1 当前覆盖情况

| 类别 | 操作 | 状态 |
|------|------|------|
| **算术** (5) | add, sub, mul, div, mod | ✅ 已实现 |
| **按位逻辑** (4) | and_, or_, xor_, not_ | ✅ 已实现 |
| **比较** (6) | eq, ne, lt, le, gt, ge | ✅ 已实现 |
| **移位** (3) | shl, shr, sshr | ❌ 未实现 |
| **一元** (1) | neg | ❌ 未实现 |
| **位操作** (3) | bit_sel, bits_extract, bits_update | ❌ 未实现 |
| **连接/扩展** (3) | concat, sext, zext | ❌ 未实现 |
| **多路选择** (1) | mux | ❌ 未实现 |
| **归约** (3) | and_reduce, or_reduce, xor_reduce | ❌ 未实现 |
| **其他** (4) | rotate_l, rotate_r, popcount, assign | ❌ 未实现 |

### 1.2 关键代码路径

```cpp
// codegen_verilog.cpp:488-489 — 未实现的操作直接注入 Verilog
out << "    assign " << node_names_[node] << " = " << lhs_name
    << " " << op_str << " " << rhs_name << ";\n";
// 输出: assign wire_42 = lhs <OP_NOT_IMPLEMENTED> rhs;  ← 语法错误
```

此外，`print_op()`（第 463-508 行）仅处理二元（`num_srcs >= 2`）和一元（`num_srcs == 1`）情况，对于 mux（3 源）、concat（可变参数）、bit_sel（特殊索引）缺少专门处理分支。

### 1.3 其他代码问题

| 问题 | 位置 | 影响 |
|------|------|------|
| 空 `catch(...){}` | 8 处（第 183-186, 241-242, 254-255, 263-264, 338-339, 403-404, 459-460, 506-507 行） | 静默吞噬所有异常，使 bug 不可见 |
| 零测试覆盖 | 无 Verilog 输出测试 | 无法验证输出正确性 |

---

## 2. 讨论过程

### 2.1 初始分析（Sisyphus）

识别了 33 个 `ch_op` 中 18 个未实现的问题，以及空 `catch` 块和零测试覆盖的问题。

### 2.2 Librarian 调研

| 项目 | 操作覆盖率 | 处理方式 |
|------|-----------|---------|
| **SpinalHDL** | 100% | 所有操作在 emitter phase 中实现，无反代 |
| **Chisel/FIRRTL** | 100% | PrimOps 全部枚举，emitter 全部处理 |
| **hdl.cpp** | 完整操作表 | 不在表中的操作编译时不通过 |
| **CIRCT** | 结构化 LoweringOptions | 配置控制发射行为，无反代 |
| **Metron** | 完整 | 锁步仿真对比 Verilator 验证 |

**结论**：`<OP_NOT_IMPLEMENTED>` 回退是行业公认的反模式。主流框架要么 100% 实现，要么编译时不通过。

### 2.3 Oracle 架构分析

| # | 问题 | 推荐方案 |
|---|------|---------|
| Q1 | 增量 vs 批量？ | **批量 + 分阶段交付。** 代码生成器已对 ~80% 设计不可用（mux/concat 阻塞），增量无意义 |
| Q2 | 优先级排序？ | **关键路径优先**：mux → concat → bit_sel → sext/zext → shifts → neg → reduction |
| Q3 | 空 catch 块？ | **全部移除**，改为传播异常，仅在顶层 `toVerilog()` 保留日志 |
| Q4 | 测试策略？ | **iverilog 编译检查 + 黄金文件**。编译验证 90% 置信度/10% 工作量 |

---

## 3. 决议

### 3.1 批量 + 分阶段实施

**阶段一（可见性 + 关键操作）**：
1. 移除全部 8 处空 `catch(...){}` 块，仅在 `toVerilog()` 顶层保留异常捕获和日志
2. 扩展 `print_op()` 架构，添加对 mux（3 源）、concat（variadic）、bit_sel 的专门处理
3. 实现关键操作：mux, concat, bit_sel, sext, zext, neg, shl, shr, sshr

**阶段二（剩余操作 + 测试）**：
4. 实现剩余操作：and_reduce, or_reduce, xor_reduce, bits_extract, bits_update, popcount, assign
5. 跳过 rotate_l/rotate_r（标记为"预留"），输出占位注释
6. 添加 iverilog 编译测试 + 1 个黄金文件测试

### 3.2 Verilog 操作映射表（批准）

| 操作 | Verilog 模式 | 实现位置 |
|------|-------------|---------|
| mux (2→1) | `cond ? true_val : false_val` | `print_op()` 3-src 分支 |
| mux (N→1) | `case(sel) ... endcase` | `print_op()` 3-src 分支 |
| concat | `{a, b, c, ...}` | `print_op()` variadic 分支 |
| bit_sel | `signal[index]` | `print_op()` 专用分支 |
| bits_extract | `signal[hi:lo]` | `print_op()` 专用分支 |
| sext | `{{W{signal[MSB]}}, signal}` | `get_op_str()` + `print_op()` |
| zext | `{W{1'b0}, signal}` | `get_op_str()` + `print_op()` |
| neg | `-signal` 或 `0 - signal` | `get_op_str()` 返回 `-` |
| shl | `<<` | `get_op_str()` |
| shr | `>>` | `get_op_str()` |
| sshr | `$signed() >>>` | `get_op_str()` |
| and_reduce | `&signal` | `get_op_str()` |
| or_reduce | `\|signal` | `get_op_str()` |
| xor_reduce | `^signal` | `get_op_str()` |
| bits_update | `{orig[hi+1:lo+1], val, orig[lo-1:0]}` | `print_op()` 专用分支 |
| popcount | `$countones(signal)` (SV) 或循环求和 | `print_op()` 专用处理 |
| assign | `assign target = src` | `get_op_str()` 或通过 proxy 处理 |
| rotate_l/r | `// rotate: reserved` 占位注释 | `get_op_str()` 返回占位 |

### 3.3 错误处理策略（批准）

| 级别 | 处理方式 | 涉及方法 |
|------|---------|---------|
| 内部方法 | **传播异常**（不捕获） | `print_op()`, `print_reg()`, `print_decl()`, `print_input()`, `print_output()`, `print_proxy()` |
| 公共入口 | **`try-catch` + `std::cerr` 日志** | `toVerilog()`, `print()` |
| 调试断言 | 在 `default: static_assert` 保证编译时覆盖 | `get_op_str()` |

### 3.4 测试策略（批准）

| 测试类型 | 内容 | 验证方式 |
|---------|------|---------|
| **编译测试** | 3 个代表设计（算术、流 mux、状态机） | `iverilog -g2012` 编译检查 |
| **黄金文件** | 1 个最简设计（2-bit adder） | 字节对比 |
| **覆盖追踪** | 所有 33 个 `ch_op` 的测试覆盖率 | CI 检查 |

---

## 4. 变更日志

| 日期 | 版本 | 变更 | 作者 |
|------|------|------|------|
| 2026-05-08 | v1.0 | 初始版本，记录 Oracle/Librarian 分析和最终决议 | Sisyphus + 用户 |

---

**相关链接**:
- `include/core/lnodeimpl.h:42-76` — `ch_op` 枚举定义
- `src/codegen_verilog.cpp:146-187` — `get_op_str()` 当前实现
- `src/codegen_verilog.cpp:463-508` — `print_op()` 当前实现
- `include/codegen_verilog.h` — `verilogwriter` 类定义
- `include/core/operators.h` — 操作符定义
- `docs/developer_guide/architecture/verilog-codegen-architecture.md` — Verilog 代码生成架构文档
- `docs/plans/verilog-codegen-implementation-plan.md` — 实施计划
- `docs/adr/ADR-DISCUSSION-PLAN.md` — 议题 #13
