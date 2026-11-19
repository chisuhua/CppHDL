# Verilog 代码生成器实施计划

> **关联 ADR**: ADR-019 | **预计总工作量**: 2 天 | **优先级**: P2

---

## 概述

将 Verilog 代码生成器的操作覆盖率从 15/33 (45%) 提升到 33/33 (100%)，同时修复空 `catch` 块反模式，添加测试覆盖。

---

## 阶段一：可见性 + 关键操作（第 1 天）

### 任务 1.1：移除空 `catch` 块

**目标**：将 8 处空 `catch(...){}` 替换为异常传播或显式日志。

| 位置 | 当前 | 目标 |
|------|------|------|
| `toVerilog()`:38-46 | 顶层空 catch | **保留**，改为日志 `std::cerr << e.what()` |
| `verilogwriter()`:79-81 | 构造函数空 catch | **移除**，让异常传播 |
| `print()`:91-93 | 空 catch | **移除** |
| `get_width_str()`:119-121 | 空 catch | **移除** |
| `get_literal_str()`:140-143 | 空 catch | **移除** |
| `print_body()`:253-255 | 空 catch | **移除** |
| `print_decl()`:338-339 | 空 catch | **移除** |
| `print_logic()`:408-410 | 空 catch | **移除** |
| `print_reg()`:459-460 | 空 catch | **移除** |
| `print_op()`:506-507 | 空 catch | **移除** |

**改动范围**：`src/codegen_verilog.cpp`

**验证**：编译通过，不改变正常路径行为。

---

### 任务 1.2：重构 `print_op()` 架构

**目标**：从仅支持二元/一元的简单分支，改为按操作类型分发的 `switch` 模式。

**当前代码**（`codegen_verilog.cpp:463-508`）：
```cpp
void verilogwriter::print_op(std::ostream &out, opimpl *node) {
    if (node->num_srcs() >= 2) { /* binary */ }
    else if (node->num_srcs() == 1) { /* unary */ }
}
```

**目标代码**：
```cpp
void verilogwriter::print_op(std::ostream &out, opimpl *node) {
    switch (node->op()) {
    case ch_op::not_: case ch_op::neg:
    case ch_op::and_reduce: case ch_op::or_reduce: case ch_op::xor_reduce:
        return print_unary_op(out, node);
    case ch_op::add: case ch_op::sub: /* ... binary ... */
    case ch_op::shl: case ch_op::shr: case ch_op::sshr:
        return print_binary_op(out, node);
    case ch_op::bit_sel:
        return print_bit_select(out, node);
    case ch_op::mux:
        return print_mux(out, node);
    case ch_op::concat:
        return print_concat(out, node);
    // ... more cases ...
    }
}
```

新增专用打印方法：

| 方法 | 处理操作 | 说明 |
|------|---------|------|
| `print_unary_op()` | not_, neg, and_reduce, or_reduce, xor_reduce | 一元前缀 |
| `print_binary_op()` | 所有二元算术/逻辑/比较/移位 | 中缀 `lhs op rhs` |
| `print_bit_select()` | bit_sel | `signal[idx]` |
| `print_bits_extract()` | bits_extract | `signal[hi:lo]` |
| `print_mux()` | mux | `cond ? a : b` |
| `print_concat()` | concat | `{a, b, c}` |
| `print_sext()` | sext | `{{W{sign}}, val}` |
| `print_zext()` | zext | `{W{0}, val}` |
| `print_bits_update()` | bits_update | `{MSB, val, LSB}` |
| `print_popcount()` | popcount | `$countones(src)` |
| `print_rotate_reserved()` | rotate_l, rotate_r | 占位注释 |

**改动范围**：`include/codegen_verilog.h`（新增方法声明）+ `src/codegen_verilog.cpp`

**验证**：现有 15 个已实现操作行为不变。

---

### 任务 1.3：实现关键操作（高优先级）

按如下顺序实现 Verilog 映射：

#### 1.3.1 `mux` — 多路选择器（最高优先级）

```cpp
void verilogwriter::print_mux(std::ostream &out, opimpl *node) {
    // node->src(0) = condition (1-bit)
    // node->src(1) = true value
    // node->src(2) = false value
    out << "    assign " << node_name << " = " << cond_name << " ? "
        << true_name << " : " << false_name << ";\n";
}
```

**测试设计**：2-bit 多路选择器，`select(cond, a, b)`

#### 1.3.2 `concat` — 位拼接

```cpp
void verilogwriter::print_concat(std::ostream &out, opimpl *node) {
    out << "    assign " << node_name << " = {";
    for (uint32_t i = 0; i < node->num_srcs(); ++i) {
        if (i > 0) out << ", ";
        out << src_names[i];
    }
    out << "};\n";
}
```

**测试设计**：`cat(a, b)` 拼接两个 4-bit 值为 8-bit

#### 1.3.3 `bit_sel` — 位选取

```cpp
void verilogwriter::print_bit_select(std::ostream &out, opimpl *node) {
    // node->src(0) = signal
    // node->src(1) = index (literal)
    auto* idx_lit = static_cast<litimpl*>(node->src(1));
    out << "    assign " << node_name << " = " << sig_name
        << "[" << idx_lit->value() << "];\n";
}
```

**测试设计**：`signal[3]` 选取第 3 位

#### 1.3.4 `sext` / `zext` — 符号/零扩展

```cpp
void verilogwriter::print_sign_extend(std::ostream &out, opimpl *node) {
    // sext: {{(TW-SW){src[MSB]}}, src}
    auto* width_lit = static_cast<litimpl*>(node->src(1));
    uint32_t target_width = static_cast<uint64_t>(width_lit->value());
    uint32_t src_width = src_node->size();
    uint32_t replicate = target_width - src_width;
    if (replicate > 0) {
        out << "    assign " << node_name << " = {{" << replicate
            << "{" << src_name << "[" << (src_width-1) << "]}}, "
            << src_name << "};\n";
    } else {
        out << "    assign " << node_name << " = " << src_name << ";\n";
    }
}
```

**测试设计**：将 4-bit 信号 sext/zext 到 8-bit

#### 1.3.5 `neg` — 一元负号

`get_op_str()` 中 `case ch_op::neg: return "-";`，`print_op()` 中作为一元前缀处理即可。

#### 1.3.6 `shl`, `shr`, `sshr` — 移位操作

`get_op_str()` 添加：
```
case ch_op::shl:  return "<<";
case ch_op::shr:  return ">>";
case ch_op::sshr: return ">>>";
```

**注意**：`>>>` 在 Verilog 中是算术右移（SystemVerilog 特性）。如果目标设计需要严格 Verilog-2001 兼容，需要使用 `$signed()` 包装。

**测试设计**：`a << 2`, `b >> 1`, `$signed(c) >>> 1`

**改动范围**：`src/codegen_verilog.cpp`

**验证**：新增 9 个操作的 Verilog 输出通过 `iverilog -g2012` 语法检查。

---

## 阶段二：剩余操作 + 测试（第 2 天）

### 任务 2.1：实现剩余操作

#### 2.1.1 `and_reduce`, `or_reduce`, `xor_reduce` — 归约

作为一元前缀操作在 `print_unary_op()` 中处理：

```cpp
case ch_op::and_reduce:
    out << "    assign " << node_name << " = &" << src_name << ";\n";
    break;
case ch_op::or_reduce:
    out << "    assign " << node_name << " = |" << src_name << ";\n";
    break;
case ch_op::xor_reduce:
    out << "    assign " << node_name << " = ^" << src_name << ";\n";
    break;
```

**测试设计**：`(&signal)`, `(|signal)`, `(^signal)` 归约 4-bit 信号

#### 2.1.2 `bits_extract` — 位段提取

```cpp
void verilogwriter::print_bits_extract(std::ostream &out, opimpl *node) {
    // srcs = [signal, hi, lo]
    auto* hi_lit = static_cast<litimpl*>(node->src(1));
    auto* lo_lit = static_cast<litimpl*>(node->src(2));
    out << "    assign " << node_name << " = " << sig_name
        << "[" << hi_lit->value() << ":" << lo_lit->value() << "];\n";
}
```

#### 2.1.3 `bits_update` — 位段赋值

```cpp
void verilogwriter::print_bits_update(std::ostream &out, opimpl *node) {
    // srcs = [signal, offset, value]
    // Verilog: {signal[MSB:offset+width], value, signal[offset-1:0]}
    auto* offset_lit = static_cast<litimpl*>(node->src(1));
    uint32_t offset = offset_lit->value();
    // 构建位段赋值
}
```

#### 2.1.4 `popcount` — 种群计数

```cpp
void verilogwriter::print_popcount(std::ostream &out, opimpl *node) {
    // 使用 $countones (SystemVerilog)
    out << "    assign " << node_name << " = $countones(" << src_name << ");\n";
}
```

#### 2.1.5 `assign` — 直接赋值

可能已经通过 proxy 节点处理。如果 `ch_op::assign` 节点存在，直接输出：
```
assign target = src;
```

#### 2.1.6 `rotate_l`, `rotate_r` — 预留操作（跳过）

```cpp
// 在 print_op() 中处理：
case ch_op::rotate_l:
case ch_op::rotate_r:
    out << "    // " << node_name << ": rotate operation (reserved)\n";
    out << "    assign " << node_name << " = " << src_name << ";\n";
    break;
```

**验证**：所有 33 个 `ch_op` 都有对应的 Verilog 输出路径，不再输出 `<OP_NOT_IMPLEMENTED>`。

---

### 任务 2.2：添加测试

#### 2.2.1 编译测试

创建 `tests/verilog/test_verilog_codegen.cpp`：

```cpp
#include <catch2/catch_test_macros.hpp>
#include "ch.hpp"
#include "codegen_verilog.h"
#include <sstream>
#include <cstdlib>

// 测试辅助：生成 Verilog 并用 iverilog 编译
bool verify_verilog_compiles(ch::core::context* ctx) {
    std::stringstream ss;
    // 需要一种方式调用 verilogwriter 输出到 stringstream
    // 当前 toVerilog() 只接受文件名，可能需要扩展 API
    // 或临时文件 + iverilog 编译
    std::string tmp_file = "/tmp/test_output_" + std::to_string(ctx->id()) + ".v";
    ch::toVerilog(tmp_file, ctx);
    int ret = std::system(("iverilog -g2012 -o /dev/null " + tmp_file + " 2>/dev/null").c_str());
    std::remove(tmp_file.c_str());
    return ret == 0;
}

TEST_CASE("Verilog codegen: arithmetic operations", "[verilog][compile]") {
    // 构建包含 add, sub, mul 的设计
    // 验证 iverilog 编译通过
}

TEST_CASE("Verilog codegen: mux and concat", "[verilog][compile]") {
    // 构建包含 select, cat 的设计
    // 验证 iverilog 编译通过
}

TEST_CASE("Verilog codegen: bit select and shift", "[verilog][compile]") {
    // 构建包含 bit_sel, shl, shr 的设计
    // 验证 iverilog 编译通过
}

TEST_CASE("Verilog codegen: all ops coverage", "[verilog][coverage]") {
    // 验证所有 33 个 ch_op 的映射存在
}
```

#### 2.2.2 黄金文件测试

创建 `tests/verilog/test_verilog_golden.cpp` 和 `tests/verilog/golden/`：

```cpp
TEST_CASE("Verilog golden: 2-bit adder", "[verilog][golden]") {
    // 构建最简设计
    // 生成 Verilog → stringstream
    // 与 golden 文件对比
}
```

#### 2.2.3 覆盖追踪测试

```cpp
TEST_CASE("All ch_op values have Verilog mappings", "[verilog][coverage]") {
    std::vector<ch_op> missing;
    for (auto op : all_ch_op_values) {
        std::string str = get_op_str(op);  // 需要公开或测试友元
        if (str == "<OP_NOT_IMPLEMENTED>") {
            missing.push_back(op);
        }
    }
    REQUIRE(missing.empty());
}
```

#### 2.2.4 CMake 配置

在 `tests/CMakeLists.txt` 中添加：
```cmake
# Verilog 代码生成测试
if(IVERILOG_FOUND)
    add_test(NAME test_verilog_codegen
        COMMAND test_verilog_codegen
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR})
endif()
```

---

## 文件变更清单

| 文件 | 阶段 | 变更类型 | 说明 |
|------|------|---------|------|
| `src/codegen_verilog.cpp` | 1 | 修改 | 移除 8 处空 catch，重构 print_op，添加 18 个操作映射 |
| `include/codegen_verilog.h` | 1 | 修改 | 新增专用打印方法声明 |
| `tests/verilog/test_verilog_codegen.cpp` | 2 | 新建 | 编译测试 + 覆盖测试 |
| `tests/verilog/test_verilog_golden.cpp` | 2 | 新建 | 黄金文件测试 |
| `tests/verilog/golden/adder_2b.v` | 2 | 新建 | 黄金参考文件 |
| `tests/CMakeLists.txt` | 2 | 修改 | 添加 Verilog 测试目标 |

---

## 验证门禁

| 门禁 | 命令 | 预期结果 |
|------|------|---------|
| **编译** | `cmake --build build -j$(nproc)` | 0 errors, 0 warnings |
| **测试** | `ctest -R verilog --output-on-failure` | 全部通过 |
| **编译检查** | `iverilog -g2012 -o /dev/null test_output.v` | 语法正确 |
| **黄金文件** | `diff test_output.v golden/adder_2b.v` | 完全一致 |

---

## 风险与缓解

| 风险 | 概率 | 影响 | 缓解 |
|------|------|------|------|
| `verilogwriter` 构造函数中 `get_eval_list()` 抛出异常 | 低 | 代码生成失败 | 任务 1.1 移除 catch 使异常可见 |
| `mux` 节点源数不等于 3 | 中 | 生成错误 Verilog | `print_mux()` 中加断言 |
| `bits_extract` 的 hi/lo 源不是 `litimpl` | 低 | `static_cast` UB | 加 `CHREQUIRE` 类型检查 |
| iverilog 在 CI 中不可用 | 中 | 编译测试跳过 | 回退到黄金文件对比 |
| `sshr` 的 `>>>` 不被旧 Verilog 工具支持 | 低 | 编译失败 | 使用 `$signed()` + `>>>` 或文档化限制 |

---

## 依赖

- iverilog (Icarus Verilog) 用于编译测试
- 如有条件：Verilator 用于更严格的 lint 检查

---

## 变更记录

| 日期 | 版本 | 变更 | 作者 |
|------|------|------|------|
| 2026-05-08 | v1.0 | 初始版本 | Sisyphus |
