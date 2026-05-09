# Verilog 代码生成架构

> **版本**: v1.0 | **最后更新**: 2026-05-08 | **关联 ADR**: ADR-019

---

## 1. 概述

Verilog 代码生成器将 CppHDL 的内部 DAG（节点图）转换为可综合的 Verilog 模块。它位于前端（C++ HDL 描述）和后端（Verilog 输出）之间，是设计验证和综合的关键路径。

### 1.1 架构位置

```
C++ HDL 设计
    ↓ describe() 构建 DAG
内部节点图 (context::node_storage_)
    ↓ 拓扑排序 (context::get_eval_list())
sorted_nodes_
    ↓ Verilog 代码生成器 (verilogwriter)
Verilog 模块 (.v 文件)
    ↓ iverilog | Yosys | Vivado
仿真 / 综合
```

### 1.2 核心数据流

```
context (节点所有权)
  └── node_storage_ (vector<unique_ptr<lnodeimpl>>)
       └── sorted_nodes_ (拓扑序 vector<lnodeimpl*>)
            └── verilogwriter
                 ├── node_names_ (节点 → Verilog 名称映射)
                 ├── node_uses_ (节点 → 使用者映射)
                 ├── declared_nodes_ (已声明去重)
                 ├── printed_logic_nodes_ (已输出去重)
                 └── 输出流 (std::ostream → .v 文件)
```

---

## 2. verilogwriter 类架构

### 2.1 类层次

`verilogwriter`（`include/codegen_verilog.h:37`）是一个独立的生成器类，不从任何基类继承。它接收 `context*`，输出 Verilog 文本。

### 2.2 公共接口

```cpp
// 顶层入口函数
void toVerilog(const std::string &filename, ch::core::context *ctx);

// 生成器类
class verilogwriter {
public:
    explicit verilogwriter(ch::core::context *ctx);
    void print(std::ostream &out);
    // ...
};
```

### 2.3 内部方法分层

```
print()                           ← 公共入口
  ├── print_header(out)           ← module 声明 + 端口列表
  │    ├── print_input()          ← 单个输入端口声明
  │    └── print_output()         ← 单个输出端口声明
  ├── print_body(out)             ← 模块体
  │    ├── print_decl(out)        ← wire/reg 声明
  │    │    ├── print_input()     ← input 声明
  │    │    ├── print_output()    ← output 声明
  │    │    └── print_reg()       ← reg 声明
  │    └── print_logic(out)       ← 逻辑实现
  │         ├── print_op()        ← 操作节点 → assign 语句
  │         ├── print_proxy()     ← 代理节点 → assign 语句
  │         ├── print_reg()       ← 寄存器 → always @(posedge clk)
  │         └── print_output()    ← 输出驱动 → assign 语句
  └── print_footer(out)           ← endmodule
```

### 2.4 辅助方法

| 方法 | 用途 |
|------|------|
| `sanitize_name()` | 净化节点名为合法 Verilog 标识符 |
| `get_width_str()` | 生成 `[MSB:LSB]` 宽度字符串 |
| `get_literal_str()` | 将 `sdata_type` 转为 Verilog 字面量（如 `8'hFF`） |
| `get_op_str()` | 将 `ch_op` 映射为 Verilog 操作符字符串 |

---

## 3. 节点 → Verilog 映射

### 3.1 节点类型映射

| CppHDL 节点类型 | Verilog 构造 |
|----------------|-------------|
| `type_input` | `input [W-1:0] name;` |
| `type_output` | `output [W-1:0] name;` |
| `type_reg` | `reg [W-1:0] name;` + `always @(posedge clk) name <= next;` |
| `type_op` | `assign name = lhs OP rhs;` |
| `type_proxy` | `assign name = src;` |
| `type_lit` | 直接内联值或独立 wire 声明 |
| `type_cd` | 不输出（仿真内部类型） |

### 3.2 操作映射表

#### 算术操作（已实现）

| `ch_op` | Verilog | 优先级 |
|---------|---------|--------|
| `add` | `+` | 最低（已实现） |
| `sub` | `-` | 最低（已实现） |
| `mul` | `*` | 最低（已实现） |
| `div` | `/` | 最低（已实现） |
| `mod` | `%` | 最低（已实现） |

#### 按位逻辑操作（已实现）

| `ch_op` | Verilog | 说明 |
|---------|---------|------|
| `and_` | `&` | 二元按位与 |
| `or_` | `\|` | 二元按位或 |
| `xor_` | `^` | 二元按位异或 |
| `not_` | `~` | 一元按位取反 |

#### 比较操作（已实现）

| `ch_op` | Verilog |
|---------|---------|
| `eq` | `==` |
| `ne` | `!=` |
| `lt` | `<` |
| `le` | `<=` |
| `gt` | `>` |
| `ge` | `>=` |

#### 移位操作（待实现 — 高优先级）

| `ch_op` | Verilog | 源数 | 说明 |
|---------|---------|------|------|
| `shl` | `<<` | 2 | 逻辑左移 |
| `shr` | `>>` | 2 | 逻辑右移 |
| `sshr` | `$signed() >>>` | 2 | 算术右移（需 `$signed()` 包装） |

#### 位操作（待实现 — 高优先级）

| `ch_op` | Verilog | 源数 | 说明 |
|---------|---------|------|------|
| `bit_sel` | `sig[idx]` | 2 | 单比特选取 |
| `bits_extract` | `sig[hi:lo]` | 3+ | 位段提取（hi, lo 为常量） |
| `bits_update` | `{sig[MSB:hi+1], val, sig[lo-1:0]}` | 3+ | 位段赋值 |

**`bits_extract` 实现模式**：
```cpp
// ch_op::bits_extract — 节点有 srcs = [signal, hi_const, lo_const]
// Verilog: assign result = signal[hi:lo];
// 其中 hi 和 lo 是 litimpl 类型的常量节点
if (op == ch_op::bits_extract && num_srcs >= 3) {
    auto *hi_lit = static_cast<litimpl*>(node->src(1));
    auto *lo_lit = static_cast<litimpl*>(node->src(2));
    out << "assign " << name << " = " << src_name
        << "[" << hi_lit->value() << ":" << lo_lit->value() << "];\n";
}
```

#### 连接与扩展（待实现 — 高优先级）

| `ch_op` | Verilog | 源数 | 说明 |
|---------|---------|------|------|
| `concat` | `{a, b, c, ...}` | variadic | 位拼接 |
| `sext` | `{{W{signal[MSB]}}, signal}` | 2+ | 符号扩展 |
| `zext` | `{W{1'b0}, signal}` | 2+ | 零扩展 |

**`sext` 实现模式**：
```cpp
// ch_op::sext — 节点有 srcs = [signal, width_lit]
// Verilog: assign result = {{(W-w){signal[MSB]}}, signal};
auto *width_lit = static_cast<litimpl*>(node->src(1));
uint32_t target_width = width_lit->value();
uint32_t src_width = signal_node->size();
uint32_t replicate_bits = target_width - src_width;
out << "assign " << name << " = {{" << replicate_bits
    << "{" << src_name << "[" << (src_width-1) << "]}}, "
    << src_name << "};\n";
```

#### 多路选择器（待实现 — 最高优先级）

| `ch_op` | Verilog | 源数 | 说明 |
|---------|---------|------|------|
| `mux` | `cond ? a : b` | 3 | 2-to-1 多路选择器 |
| `mux` (链) | `case(sel) ... endcase` | 3+ | N-to-1 多路选择器 |

**`mux` 实现模式**：
```cpp
// ch_op::mux — 节点有 srcs = [cond, true_val, false_val]
// Verilog: assign result = cond ? true_val : false_val;
if (op == ch_op::mux && num_srcs == 3) {
    auto *cond = node->src(0);  // 条件
    auto *t_val = node->src(1); // 真值
    auto *f_val = node->src(2); // 假值
    out << "assign " << name << " = " << names[cond] << " ? "
        << names[t_val] << " : " << names[f_val] << ";\n";
}
```

#### 归约操作（待实现 — 中优先级）

| `ch_op` | Verilog | 源数 | 说明 |
|---------|---------|------|------|
| `and_reduce` | `&signal` | 1 | 与归约 |
| `or_reduce` | `\|signal` | 1 | 或归约 |
| `xor_reduce` | `^signal` | 1 | 异或归约 |

**注意**：Verilog 归约是一元前缀操作符，不是二元。需要在 `print_op()` 中作为一元操作单独处理。

#### 特殊操作（待实现 — 低优先级）

| `ch_op` | Verilog | 说明 |
|---------|---------|------|
| `neg` | `-signal` | 一元负号，通过 `get_op_str()` 返回 `-` |
| `popcount` | `$countones(signal)` 或循环 | 使用 SystemVerilog 内置函数 |
| `assign` | `assign target = src` | 直接连接（可能已通过 proxy 处理） |
| `rotate_l` | `// reserved` | 预留，输出占位注释 |
| `rotate_r` | `// reserved` | 预留，输出占位注释 |

---

## 4. 表达式打印架构

### 4.1 当前架构（待重构）

`print_op()` 当前仅处理二元/一元情况：

```cpp
void verilogwriter::print_op(std::ostream &out, opimpl *node) {
    if (node->num_srcs() >= 2) {      // 二元
        auto *lhs = node->lhs();
        auto *rhs = node->rhs();
        out << "assign " << name << " = " << lhs_name
            << " " << get_op_str(node->op()) << " " << rhs_name << ";\n";
    } else if (node->num_srcs() == 1) { // 一元
        out << "assign " << name << " = " << get_op_str(node->op())
            << src_name << ";\n";
    }
}
```

### 4.2 目标架构

需要扩展以支持：

```
print_op()
  ├── num_srcs == 1 → 一元操作
  │    ├── not_, neg        → 前缀操作符
  │    └── and_reduce, or_reduce, xor_reduce  → 一元归约前缀
  ├── num_srcs == 2 → 二元操作
  │    ├── add..ge          → 中缀操作符 (现有)
  │    ├── shl, shr, sshr   → 中缀操作符
  │    ├── bit_sel          → signal[idx] 索引
  │    └── sext, zext       → 扩展包装
  ├── num_srcs == 3 → 三目操作
  │    └── mux              → cond ? a : b
  └── num_srcs >= 3 → 变参操作
       ├── concat           → {a, b, c, ...}
       ├── bits_extract     → signal[hi:lo]
       └── bits_update      → {MSB, val, LSB}
```

### 4.3 推荐的重构模式

```cpp
void verilogwriter::print_op(std::ostream &out, opimpl *node) {
    auto op = node->op();
    auto num_srcs = node->num_srcs();

    switch (op) {
    // === 一元操作 ===
    case ch_op::not_:
    case ch_op::neg:
        return print_unary_prefix_op(out, node);  // op_str + src
    case ch_op::and_reduce:
    case ch_op::or_reduce:
    case ch_op::xor_reduce:
        return print_unary_prefix_reduce(out, node);  // &src, |src, ^src

    // === 二元操作 ===
    case ch_op::add:  case ch_op::sub:
    case ch_op::mul:  case ch_op::div:  case ch_op::mod:
    case ch_op::shl:  case ch_op::shr:  case ch_op::sshr:
        return print_binary_infix_op(out, node);  // lhs op_str rhs
    case ch_op::and_: case ch_op::or_: case ch_op::xor_:
        return print_binary_infix_op(out, node);
    case ch_op::eq:   case ch_op::ne:
    case ch_op::lt:   case ch_op::le:  case ch_op::gt:  case ch_op::ge:
        return print_binary_infix_op(out, node);

    // === 索引操作 ===
    case ch_op::bit_sel:
        return print_bit_select(out, node);      // signal[idx]
    case ch_op::bits_extract:
        return print_bits_extract(out, node);    // signal[hi:lo]

    // === 三目操作 ===
    case ch_op::mux:
        return print_mux(out, node);             // cond ? a : b

    // === 变参操作 ===
    case ch_op::concat:
        return print_concat(out, node);          // {a, b, c}
    case ch_op::bits_update:
        return print_bits_update(out, node);     // bit field assignment

    // === 扩展操作 ===
    case ch_op::sext:
        return print_sign_extend(out, node);     // {{W{sign}}, val}
    case ch_op::zext:
        return print_zero_extend(out, node);     // {W{0}, val}

    // === 特殊操作 ===
    case ch_op::popcount:
        return print_popcount(out, node);        // $countones()
    case ch_op::assign:
        return print_assign(out, node);          // direct assign

    // === 预留操作 ===
    case ch_op::rotate_l:
    case ch_op::rotate_r:
        return print_rotate_reserved(out, node); // comment placeholder
    }
}
```

---

## 5. 错误处理策略

### 5.1 当前（反模式）

```cpp
void verilogwriter::print_op(std::ostream &out, opimpl *node) {
    try {
        // ... 实际操作 ...
    } catch (...) {
        // 静默吞噬 ← 反模式
    }
}
```

### 5.2 目标

```cpp
void verilogwriter::print_op(std::ostream &out, opimpl *node) {
    // 不捕获异常 → 让调用者处理
    // ... 实际操作 ...
}

void toVerilog(const std::string &filename, context *ctx) {
    try {
        // ... 创建 writer 并生成 ...
    } catch (const std::exception &e) {
        std::cerr << "[toVerilog] Error: " << e.what() << std::endl;
    }
}
```

**原则**：
- 内部方法（`print_op`, `print_reg`, `print_decl` 等）**不捕获异常**
- 公共入口（`toVerilog`, `print`）**捕获并日志**，防止进程崩溃
- 调试构建中，缺失的操作通过 `default: static_assert(false)` 触发编译错误

---

## 6. 测试架构

### 6.1 编译测试

```cpp
// tests/chlib/test_verilog_codegen.cpp
TEST_CASE("Verilog codegen produces compilable output", "[verilog]") {
    // 1. 构建一个包含关键操作的 HDL 设计
    // 2. 生成 Verilog 输出到临时文件
    // 3. 调用 iverilog -g2012 检查语法

    // 测试覆盖的操作组合：
    // - 算术: add, sub, mul
    // - 逻辑: and_, or_, xor_, not_
    // - 比较: eq, ne, lt, le, gt, ge
    // - 移位: shl, shr, sshr
    // - 选择: mux
    // - 连接: concat
    // - 位操作: bit_sel, bits_extract
    // - 扩展: sext, zext
}
```

### 6.2 黄金文件测试

```cpp
TEST_CASE("Verilog output matches golden reference", "[verilog][golden]") {
    // 1. 构建 2-bit adder 设计
    // 2. 生成 Verilog 到 stringstream
    // 3. 与 tests/verilog_golden/adder_2b.v 对比
}
```

### 6.3 覆盖追踪

```cpp
TEST_CASE("All ch_op values have Verilog mappings", "[verilog][coverage]") {
    // 遍历所有 33 个 ch_op 值
    // 验证 get_op_str() 不返回 "<OP_NOT_IMPLEMENTED>"
    // 验证 print_op() 能处理对应源数
}
```

---

## 7. 已知限制

| 限制 | 说明 | 计划 |
|------|------|------|
| **单时钟域** | 所有寄存器使用 `default_clock` | ADR-008 已记录 |
| **无复位** | 生成的 Verilog 无复位逻辑 | 待规划 |
| **字段名丢失** | `__io` 宏不捕获端口名 | ADR-014 T002 |
| **无层级模块** | 所有逻辑平铺在单个 `top` 模块中 | 待规划 |
| **不支持 CDC** | 异步设计需手动处理 | ADR-008 |

---

## 8. 变更记录

| 日期 | 版本 | 变更 | 作者 |
|------|------|------|------|
| 2026-05-08 | v1.0 | 初始版本，架构描述和操作映射表 | Sisyphus |

---

**相关链接**:
- `src/codegen_verilog.cpp` — 实现文件
- `include/codegen_verilog.h` — 头文件
- `include/core/lnodeimpl.h:42-76` — `ch_op` 枚举
- `include/core/operators.h` — 操作符定义
- `docs/adr/ADR-019-verilog-codegen-completeness.md` — 决策记录
- `docs/plans/verilog-codegen-implementation-plan.md` — 实施计划
