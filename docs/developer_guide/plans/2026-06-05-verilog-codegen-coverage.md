# Verilog Codegen 完整化计划（2026-06-05 增量）

> **关联文档**：[ADR-019: Verilog 代码生成操作完整性](../adr/ADR-019-verilog-codegen-completeness.md) ·
> [历史计划：verilog-codegen-implementation-plan.md](../../plans/verilog-codegen-implementation-plan.md)（2026-05-08）·
> **预计工作量**：2-3 小时 · **优先级**：P2
> **前置依赖**：[A 阶段 commit `3396b23`](#)（启用 `-Wswitch-enum`）+ [C 阶段 commit `18fae71`](#)（删除 6 个死 JitOp）

---

## 1. 背景

2026-05-08 ADR-019 提出将 Verilog codegen 的 `ch_op` 覆盖率从 15/33 提升到 33/33。截至 2026-06-05 实际状态：

- **当前覆盖**：19/33 `ch_op` 在 `get_op_str()` 中映射
- **本次目标**：补全剩余 **14 个** `ch_op`（含 1 个 lnodetype 缺口）
- **关联前置工作**：A+C 阶段（已 commit）让 6 个死 JitOp 从 enum 移除，因此实际目标从原计划的 18 个减少为 14 个

本次计划与历史计划（2026-05-08）**互补不冲突**——历史计划涵盖更广（含 catch 块重构、iverilog 集成测试），本次计划聚焦当前缺口、依据 post-A+C 状态调整。

---

## 2. 当前状态审计

**入口文件**：`src/codegen_verilog.cpp`（576 行）+ `include/codegen_verilog.h`（88 行）

### 2.1 现有函数结构

| 函数 | 行号 | 职责 |
|------|------|------|
| `get_op_str()` | 146-195 | 19 ch_op → Verilog 字符串映射 |
| `print_header()` | 197 | module 端口声明 |
| `print_decl()` | 274 | wire/reg 声明 |
| `print_logic()` | 350 | assign/always 块分发 |
| `print_op()` | 472 | 二元 op 打印（**无 unary 分支**） |
| `print_mux()` | 554 | type_mux 三元运算 |

### 2.2 `get_op_str()` 缺失 13 个 ch_op

| ch_op | Verilog 算子 | 难度 | 备注 |
|-------|------------|------|------|
| `bit_sel` | `signal[idx]` | 中 | 需 index 处理 |
| `bits_extract` | `signal[hi:lo]` | 中 | range 在 64-bit literal 中编码 |
| `bits_update` | — | 简单 | 已在 `print_op` line 483 特判 |
| `sext` | 符号扩展 | 中 | 需源位宽 |
| `zext` | 零扩展 | 简单 | 隐式 assign 给宽位 |
| `mux` | — | 已实现 | 通过 `print_mux()` 走 lnodetype |
| `and_reduce` | `&signal` | 简单 | 归约 |
| `or_reduce` | `\|signal` | 简单 | 归约 |
| `xor_reduce` | `^signal` | 简单 | 归约 |
| `rotate_l` | 拼接移位 | 中 | `{src[N-X-1:0], src[N-1:N-X]}` |
| `rotate_r` | 拼接移位 | 中 | `{src[X-1:0], src[N-1:X]}` |
| `popcount` | `$countones()` | 简单 | 系统函数 |
| `assign` | `=` | 简单 | 1:1 wire |

### 2.3 额外 lnodetype 缺口（与 ch_op 不直接对应）

| lnodetype | 位置 | 现状 |
|-----------|------|------|
| `type_bitsupdate` | `print_decl` line 287 + `print_logic` line 361 | **完全缺失**——`bitsupdateimpl` 节点不出现在 Verilog |
| `type_clock` / `type_reset` | 同上 | 缺失（系统信号，需 `always` 特化，**下期**） |

### 2.4 `print_op()` 结构性缺陷

当前仅处理 `num_srcs() >= 2` 二元 op：

```cpp
if (node->num_srcs() >= 2) { /* binary */ }
// 没有 unary 分支
```

**缺失分支**：
- 一元 op：`not_`, `neg`, `sext`, `zext`, `*_reduce`, `popcount`
- 位操作：`bit_sel`（带 index 参数）、`bits_extract`（带范围参数）、`bits_update`（带 lsb/width 参数）
- 直连：`assign`

---

## 3. 实施计划

### 3.1 修改 `get_op_str()` (line 146-195)

补充 11 个 ch_op 字符串映射（除已特判的 `concat` 和 `mux`）：

```cpp
case ch::core::ch_op::bit_sel:        return "<bit_sel>";    // print_op 中特判
case ch::core::ch_op::bits_extract:   return "<bits_ex>";   // print_op 中特判
case ch::core::ch_op::bits_update:    return "<bits_up>";   // print_op 中特判
case ch::core::ch_op::sext:           return "<sext>";      // print_op 中特判
case ch::core::ch_op::zext:           return "<zext>";      // print_op 中特判
case ch::core::ch_op::and_reduce:     return "&";           // 归约
case ch::core::ch_op::or_reduce:      return "|";
case ch::core::ch_op::xor_reduce:     return "^";
case ch::core::ch_op::rotate_l:       return "<rotl>";      // print_op 中特判
case ch::core::ch_op::rotate_r:       return "<rotr>";
case ch::core::ch_op::popcount:       return "$countones";
case ch::core::ch_op::assign:         return "=";           // 1:1 wire
```

占位符 `<...>` 用于标识"在 `print_op` 中需要特判"，方便后续审计。

### 3.2 重构 `print_op()` (line 472+)

按操作类型分发到专用方法：

```cpp
void verilogwriter::print_op(std::ostream &out, opimpl *node) {
    switch (node->op()) {
    // Unary prefix
    case ch_op::not_: case ch_op::neg:
    case ch_op::and_reduce: case ch_op::or_reduce: case ch_op::xor_reduce:
    case ch_op::popcount: case ch_op::sext: case ch_op::zext:
        return print_unary_op(out, node);

    // Binary infix
    case ch_op::add: case ch_op::sub: case ch_op::mul: case ch_op::div: case ch_op::mod:
    case ch_op::and_: case ch_op::or_: case ch_op::xor_:
    case ch_op::eq: case ch_op::ne: case ch_op::lt: case ch_op::le:
    case ch_op::gt: case ch_op::ge:
    case ch_op::shl: case ch_op::shr: case ch_op::sshr:
        return print_binary_op(out, node);

    // Bit / range / concat / wire (special)
    case ch_op::bit_sel:        return print_bit_select(out, node);
    case ch_op::bits_extract:   return print_bits_extract(out, node);
    case ch_op::bits_update:    return print_bits_update(out, node);
    case ch_op::concat:         return print_concat(out, node);
    case ch_op::rotate_l:       return print_rotate_l(out, node);
    case ch_op::rotate_r:       return print_rotate_r(out, node);
    case ch_op::assign:         return print_assign_wire(out, node);
    }
}
```

### 3.3 新增专用打印方法

| 方法 | 处理 op | Verilog 模式 | 实现要点 |
|------|--------|------------|----------|
| `print_unary_op()` | not_, neg, *_reduce, popcount, sext, zext | `&sig`, `\|sig`, `~sig`, `$countones(sig)` 等 | 需 width 字段 |
| `print_binary_op()` | 所有二元算术/逻辑/比较/移位 | `lhs op rhs` | 现有逻辑抽离 |
| `print_bit_select()` | bit_sel | `signal[idx]` | 解码 index literal |
| `print_bits_extract()` | bits_extract | `signal[hi:lo]` | 解码 64-bit range literal（高 32 位 = msb，低 32 位 = lsb） |
| `print_bits_update()` | bits_update | `{dst[hi+width:hi+1], src, dst[hi-1:0]}` | 三段拼接 |
| `print_concat()` | concat | `{lhs, rhs}` | 现有特判重构 |
| `print_rotate_l()` | rotate_l | `{src[N-1:X], src[N-X-1:0]}` | 拼接 |
| `print_rotate_r()` | rotate_r | `{src[X-1:0], src[N-1:X]}` | 拼接 |
| `print_assign_wire()` | assign | `assign dst = src;` | 直连 |

### 3.4 添加 `print_bitsupdate()` lnodetype 方法

处理 `bitsupdateimpl` 节点（与 ch_op::bits_update 路径并行）：

```cpp
// include/codegen_verilog.h
void print_bitsupdate(std::ostream &out, ch::core::bitsupdateimpl *node);

// src/codegen_verilog.cpp
void verilogwriter::print_bitsupdate(std::ostream &out, bitsupdateimpl *node) {
    // target/src/range 节点都在 node_names_ 中
    // Verilog: {target[MSB:hi+1], source, target[lo-1:0]}
    out << "    assign " << node_names_[node] << " = "
        << "{" << target_name << "[MSB:hi+1], "
        << source_name << ", "
        << target_name << "[lo-1:0]};\n";
}
```

### 3.5 更新 `print_decl()` 和 `print_logic()` switch

两个 switch 各加 1 行处理 `type_bitsupdate`：

```cpp
case ch::core::lnodetype::type_bitsupdate:
    // bitsupdateimpl 节点作为 wire + 在 print_logic 中调 print_bitsupdate
    wires.push_back(node);  // 或专属 type
    declared_nodes_.insert(node);
    break;
```

---

## 4. 测试计划

### 4.1 在 `tests/test_verilog_gen.cpp` 中新增测试

每个新增的 ch_op 加一个 `TEST_CASE`：

| # | 测试名 | 关键 Verilog 模式 | 验证函数 |
|---|--------|----------------|----------|
| 1 | `AndReduce` | `assign result = &signal;` | `find("& signal")` |
| 2 | `OrReduce` | `assign result = \|signal;` | `find("\| signal")` |
| 3 | `XorReduce` | `assign result = ^signal;` | `find("^ signal")` |
| 4 | `Popcount` | `assign result = $countones(signal);` | `find("$countones")` |
| 5 | `RotateLeft` | `{src[N-1:X], src[N-X-1:0]}` | `find("{") && find("}")` |
| 6 | `RotateRight` | `{src[X-1:0], src[N-1:X]}` | 同上 |
| 7 | `BitSelect` | `signal[idx]` | `find("signal[")` |
| 8 | `BitsExtract` | `signal[hi:lo]` | `find("signal[hi:lo]")` |
| 9 | `Zext` | `assign wide = src;` | 简单 width 检查 |
| 10 | `Sext` | sign-extended assignment | `find("signed")` 或拼接 |
| 11 | `BitsUpdate` | `{..., source, ...}` 拼接 | `find("{")` 且包含 source 名 |
| 12 | `Assign` | `assign dst = src;` | `find("assign dst = src")` |
| 13 | `BitsUpdateLnodetype` | 通过 `bits_update<W>()` API 触发 | 验证 type_bitsupdate 路径 |

每个测试模式：
```cpp
TEST_CASE("VerilogGen - <OpName>", "[verilog][<tag>]") {
    auto ctx = std::make_unique<ch::core::context>("<op>_test");
    ch::core::ctx_swap ctx_guard(ctx.get());
    // 构建 op
    ch_uint<8> a(0xFF_d);
    ch_out<ch_uint<8>> out_port("io");
    out_port = <op-expr>(a);
    // 验证
    std::string verilog = generateVerilogToString(ctx.get());
    REQUIRE(verilog.find("EXPECTED_PATTERN") != std::string::npos);
}
```

### 4.2 端到端验证

| 测试套件 | 预期 |
|---------|------|
| `ctest -R test_verilog_gen` | 13+ cases 全过 |
| 现有 `test_verilog_gen` 4 个测试 | 无回归 |
| `test_module` / `test_module_advanced` / `test_onehot` | 无回归 |
| 完整 `ctest -L base -E test_multithread` | 97+/97+ 通过 |

---

## 5. 风险评估

| 风险 | 严重度 | 概率 | 缓解 |
|------|--------|------|------|
| `sext` Verilog 实现复杂（需 `signed` 声明） | 中 | 中 | 简化版：直接 assign + 注释 `// sign-extended`，或 `$signed` 包装 |
| `rotate_l/r` 需要 opimpl 暴露 `bitwidth` 字段 | 中 | 中 | 需先探查 opimpl 接口，必要时从 result 类型推断 |
| `bits_update` 双路径（ch_op 路径 + lnodetype 路径） | 中 | 中 | 优先 lnodetype 路径；ch_op 路径已走 CALL_EXTERNAL 不常见 |
| 现有 `<OP_NOT_IMPLEMENTED>` 在某些历史测试中作为预期输出 | 低 | 低 | `test_verilog_gen.cpp` 现有 4 个测试不会触发新覆盖的 op |
| 14 个 op 的 Verilog 语法兼容 Verilog-2001（无 `$countones`、无 `>>>`） | 中 | 中 | 文档化 SystemVerilog 依赖；可选用 `{N{val}}` 模拟 `&val` 等 |

---

## 6. Commit 策略

按 git-master 规则（2 文件 → min 1 commit）：

**建议 2 commits**（实现 + 测试分离）：

1. **`feat(codegen-verilog): emit Verilog for reduction, rotate, popcount, extension, bit ops`**
   - `src/codegen_verilog.cpp`：`get_op_str` 补 case + `print_op` 重构 + `print_bitsupdate` 新增
   - `include/codegen_verilog.h`：声明新增方法
   - `print_decl` / `print_logic` 添加 `type_bitsupdate` case

2. **`test(codegen-verilog): cover new Verilog generation paths`**
   - `tests/test_verilog_gen.cpp`：13 个新 TEST_CASE

**理由**：实现 + 测试是同一原子单元（function + its test），但 2 commits 让 reviewer 单独审阅实现 vs 测试覆盖，可读性更好。

---

## 7. 不在本期范围

| 项 | 理由 | 下期建议 |
|----|------|---------|
| `type_clock` / `type_reset` lnodetype 声明 | 需要 SystemVerilog `always` 特化、clocking block 复杂 | 单独 PR |
| DAG codegen（`src/codegen_dag.cpp`） | 与 Verilog 同步但属于不同代码路径 | 单独 PR |
| 移除空 `catch` 块（历史计划 1.1） | 独立 hygiene 改进 | 单独 PR |
| iverilog 集成测试（历史计划 2.2.1） | 需要 CI 基础设施 | 单独 PR |
| `simulator.cpp:338, 616` 不完整 switch | runtime 路径，不是 codegen | 单独 PR |

---

## 8. 验证门禁

| 门禁 | 命令 | 预期 |
|------|------|------|
| 编译 | `cmake --build build -j$(nproc)` | 0 error, 0 warning (来自本次改动) |
| 新测试 | `ctest -R test_verilog_gen --output-on-failure` | 13+ new cases 全过 |
| 全套件 | `ctest -L base -E test_multithread` | 97+/97+ |
| Verilog 语法 | `iverilog -g2012 -o /dev/null <generated>.v`（手动） | 语法正确（可选） |

---

## 9. 时间估计

| 阶段 | 估计 |
|------|------|
| 3.1-3.3 print_op 重构 + 9 个新方法 | 1-1.5 小时（含 rotate 拼接表达式调试） |
| 3.4-3.5 type_bitsupdate 处理 | 20-30 分钟 |
| 4.1 13 个新测试 | 30-45 分钟 |
| 6 commit + 验证 | 15 分钟 |
| **总计** | **2-3 小时** |

---

## 10. 变更记录

| 日期 | 版本 | 变更 | 作者 |
|------|------|------|------|
| 2026-06-05 | v1.0 | 初始版本（post A+C 状态） | Sisyphus |
