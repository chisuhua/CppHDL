# Verilog Codegen 完整化计划（2026-06-05 v2.0）

> **关联文档**：[ADR-019: Verilog 代码生成操作完整性](../adr/ADR-019-verilog-codegen-completeness.md) ·
> [历史计划：verilog-codegen-implementation-plan.md](../../plans/verilog-codegen-implementation-plan.md)（2026-05-08）·
> **预计工作量**：2-3 小时 · **优先级**：P2
> **前置依赖**：[A 阶段 commit `3396b23`](#)（启用 `-Wswitch-enum`）+ [C 阶段 commit `18fae71`](#)（删除 6 个死 JitOp）
> **v2.0 修订**：经 Oracle 审查修正 3 个事实错误和 2 个风险误判，详见 [第 11 节](#11-变更记录)

---

## 1. 背景

2026-05-08 ADR-019 提出将 Verilog codegen 的 `ch_op` 覆盖率从 15/33 提升到 33/33。截至 2026-06-05 实际状态：

- **当前覆盖**：19/33 `ch_op` 在 `get_op_str()` 中映射
- **本次目标**：补全剩余 **12 个** `ch_op` + 修 **1 个 lnodetype 缺口**（`type_bitsupdate`）
- **关联前置工作**：A+C 阶段（已 commit）让 6 个死 JitOp 从 enum 移除，因此实际目标从原计划的 18 个减少为 12 个

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
| `print_op()` | 472 | 二元 op 打印（**已有 unary 分支 at line 519 处理 not_/neg**） |
| `print_mux()` | 554 | type_mux 三元运算 |
| `print_output()` | 431 | type_output 端口声明（与 print_logic 的输出赋值不同） |

### 2.2 `get_op_str()` 缺失 12 个 ch_op + 1 个 lnodetype 缺口

| ch_op | arity | Verilog 模式 | 难度 | 备注 |
|-------|-------|------------|------|------|
| `bit_sel` | 2 | `signal[idx]` | 中 | **字面量** idx（`get_literal_str()`）和**变量** idx（`node_names_[rhs_node]`）两种形式 |
| `bits_extract` | 2 | `signal[msb:lsb]` | 中 | range 在 64-bit literal 中编码（msb<<32 \| lsb） |
| `sext` | 1 | `{{N{src[SW-1]}}, src}` | 简单 | **不需要** `signed` 关键字；replication 模式 |
| `zext` | 1 | `{{N{1'b0}}, src}` | 简单 | 或 `assign name = src;`（隐式 0 扩展） |
| `mux` | — | — | 已实现 | 通过 `print_mux()` 走 lnodetype，不在 ch_op 路径 |
| `and_reduce` | 1 | `&sig` | 简单 | 一元归约 |
| `or_reduce` | 1 | `\|sig` | 简单 | 一元归约 |
| `xor_reduce` | 1 | `^sig` | 简单 | 一元归约 |
| `rotate_l` | 2 | `{lhs[N-amt-1:0], lhs[N-1:N-amt]}` | 中 | **二元**：lhs=值, rhs=amt；amt=0 降级为 passthrough |
| `rotate_r` | 2 | `{lhs[amt-1:0], lhs[N-1:amt]}` | 中 | **二元**；同上 |
| `popcount` | 1 | `$countones(sig)` | 简单 | SystemVerilog 系统函数 |
| `assign` | 1 | `assign name = src;` | 简单 | 直连，与 proxy 相同 |

**不在 ch_op 路径上**（不要在 `print_op` 处理）：
- `ch_op::bits_update`：该 opimpl 路径**已死**（`operators.h:536` 走 `build_bits_update()` 创建 `bitsupdateimpl`，不创建 opimpl）。`bits_update<W>()` 实际走 `type_bitsupdate` lnodetype 路径。
- `ch_op::mux`：同上，mux 永远走 `type_mux` 路径。
- `ch_op::concat`：已有特判（line 483）。

**额外 lnodetype 缺口**（`type_bitsupdate`）：

| lnodetype | 位置 | 现状 |
|-----------|------|------|
| `type_bitsupdate` | `print_decl:287` + `print_logic:361` | **完全缺失**——`bitsupdateimpl` 节点走 `default: break;`，**永不声明、永不打印**。这是 pre-existing bug，会让任何用 `bits_update<W>()` 的设计生成的 Verilog 出现"wire 用但未声明"错误。 |

### 2.3 `print_op()` 现有结构

```cpp
void verilogwriter::print_op(std::ostream &out, ch::core::opimpl *node) {
    if (node->num_srcs() >= 2) { /* binary */ }
    else if (node->num_srcs() == 1) { /* unary: not_, neg */ }
}
```

**现状**：已有 unary 分支（line 519）处理 `not_` 和 `neg`，但**未覆盖**：
- `*_reduce`（3 个）：前缀归约
- `popcount`：`$countones()`
- `sext` / `zext`：replication

**二元分支也需特判**：
- `bit_sel`：index 可字面量/变量
- `bits_extract`：range 在 64-bit literal 中编码
- `rotate_l/r`：值在 lhs，amt 在 rhs

### 2.4 已有架构缺陷（Oracle 指出，**不修但需知道**）

| 缺陷 | 位置 | 严重度 |
|------|------|--------|
| `print_logic` `type_output` "io hack" 遍历 `node_names_` 找第一个 reg 赋给 `io` 输出 | line 393-401 | 中 | 多 reg/多输出设计会出错；现有测试通过仅因单 reg+单 output |
| `get_literal_str` 十六进制 stream state 隐患 | line 137-138 | 低 | 当前 ostringstream 独立使用，无问题；future 改动需注意 |
| `sanitize_name` 不处理 Verilog 关键字 | line 96 | 低 | 节点命名为 `reg`/`wire` 会生成无效 Verilog；out of scope |

---

## 3. 实施计划

### 3.1 修改 `get_op_str()` (line 146-195)

补充 **11 个 ch_op** 字符串映射（不含 `bits_update` 死路径）：

```cpp
case ch::core::ch_op::bit_sel:        return "<bit_sel>";    // print_op 中特判
case ch::core::ch_op::bits_extract:   return "<bits_ex>";   // print_op 中特判
case ch::core::ch_op::sext:           return "<sext>";      // print_unary_op 中特判
case ch::core::ch_op::zext:           return "<zext>";      // print_unary_op 中特判
case ch::core::ch_op::and_reduce:     return "&";           // 一元归约
case ch::core::ch_op::or_reduce:      return "|";
case ch::core::ch_op::xor_reduce:     return "^";
case ch::core::ch_op::rotate_l:       return "<rotl>";      // print_op binary 特判
case ch::core::ch_op::rotate_r:       return "<rotr>";      // print_op binary 特判
case ch::core::ch_op::popcount:       return "$countones";  // print_unary_op 中特判
case ch::core::ch_op::assign:         return "=";           // 一元（直连）
```

**不补**：`ch_op::bits_update`（opimpl 路径已死，由 `type_bitsupdate` lnodetype 处理）。

### 3.2 重构 `print_op()` (line 472+)

按操作类型分发到专用方法：

```cpp
void verilogwriter::print_op(std::ostream &out, opimpl *node) {
    switch (node->op()) {
    // Unary prefix (已有 line 519 扩展)
    case ch_op::not_: case ch_op::neg:
    case ch_op::and_reduce: case ch_op::or_reduce: case ch_op::xor_reduce:
    case ch_op::popcount: case ch_op::sext: case ch_op::zext:
    case ch_op::assign:  // 直连
        return print_unary_op(out, node);

    // Binary infix (现有逻辑)
    case ch_op::add: case ch_op::sub: case ch_op::mul: case ch_op::div: case ch_op::mod:
    case ch_op::and_: case ch_op::or_: case ch_op::xor_:
    case ch_op::eq: case ch_op::ne: case ch_op::lt: case ch_op::le:
    case ch_op::gt: case ch_op::ge:
    case ch_op::shl: case ch_op::shr: case ch_op::sshr:
        return print_binary_op(out, node);

    // Binary with special parameter encoding
    case ch_op::bit_sel:        return print_bit_select(out, node);     // idx: literal OR lnode
    case ch_op::bits_extract:   return print_bits_extract(out, node);    // range: 64-bit literal
    case ch_op::rotate_l:       return print_rotate_l(out, node);       // amt: literal OR lnode
    case ch_op::rotate_r:       return print_rotate_r(out, node);

    // Special: existing inline handling
    case ch_op::concat:         return print_concat(out, node);
    }
    // 不需要 default: ch_op 是 enum，加 -Wswitch-enum 后漏 case 会编译错误
}
```

### 3.3 新增专用打印方法

| 方法 | 处理 op | Verilog 模式 | 关键实现 |
|------|--------|------------|----------|
| `print_unary_op()` (扩展现有) | not_, neg, *_reduce, popcount, sext, zext, assign | `&sig`, `\|sig`, `^sig`, `$countones(sig)`, `~sig`, `-sig`, `assign name=src;` | 已有 line 519 分支扩展；按 `op` switch 特判 |
| `print_binary_op()` (提取) | add, sub, mul, div, mod, and_, or_, xor_, eq/ne/lt/le/gt/ge, shl/shr/sshr | `lhs op rhs` | 现有 line 474 逻辑抽离 |
| `print_bit_select()` | bit_sel | `signal[idx]` | `if (rhs is litimpl) idx = get_literal_str(); else idx = node_names_[rhs]` |
| `print_bits_extract()` | bits_extract | `signal[msb:lsb]` | `range = static_cast<uint64_t>(lit->value()); msb = range>>32; lsb = range & 0xFFFFFFFF` |
| `print_rotate_l()` | rotate_l | `{lhs[N-amt-1:0], lhs[N-1:N-amt]}` | `N = lhs->size()`；**amt=0 降级**为 `assign name = lhs;`；变量 amt 降级为 passthrough + 注释 |
| `print_rotate_r()` | rotate_r | `{lhs[amt-1:0], lhs[N-1:amt]}` | 同上 |
| `print_concat()` (提取) | concat | `{lhs, rhs}` | 现有特判（line 483）抽离 |
| `print_sext()` 或内联 | sext | `{{ext{src[sw-1]}}, src}` | `ext = node->size() - src->size()`；ext=0 降级为 passthrough；**不需要 `signed` 关键字** |
| `print_zext()` 或内联 | zext | `{{ext{1'b0}}, src}` | 或简单 `assign name = src;`（隐式 0 扩展） |
| `print_popcount()` 或内联 | popcount | `$countones(sig)` | SystemVerilog；可文档化为依赖 |
| `print_assign_wire()` 或内联 | assign | `assign name = src;` | 直连；opimpl::op() == ch_op::assign 时触发 |

### 3.4 添加 `print_bitsupdate()` lnodetype 方法（**唯一**的 bits_update 路径）

```cpp
// include/codegen_verilog.h
void print_bitsupdate(std::ostream &out, ch::core::bitsupdateimpl *node);

// src/codegen_verilog.cpp
void verilogwriter::print_bitsupdate(std::ostream &out, bitsupdateimpl *node) {
    // 3 operands: target(N-bit), source(W-bit), range(64-bit literal: msb<<32 | lsb)
    // Verilog: {target[N-1:msb+1], source, target[lsb-1:0]}
    auto* tgt = node->target();
    auto* src = node->source();
    auto* rng = node->range();
    auto range_val = static_cast<uint64_t>(
        static_cast<litimpl*>(rng)->value().bv_.data()[0]);
    uint32_t msb = range_val >> 32;
    uint32_t lsb = range_val & 0xFFFFFFFFULL;
    uint32_t N = tgt->size();
    out << "    assign " << node_names_[node] << " = {";
    if (msb < N - 1)
        out << node_names_[tgt] << "[" << (N-1) << ":" << (msb+1) << "], ";
    out << node_names_[src];
    if (lsb > 0)
        out << ", " << node_names_[tgt] << "[" << (lsb-1) << ":0]";
    out << "};\n";
}
```

**边界处理**：
- `msb == N-1`：省略 `target[N-1:msb+1]`（零宽切片）
- `lsb == 0`：省略 `target[lsb-1:0]`（负下标）

### 3.5 更新 `print_decl()` 和 `print_logic()` switch

两个 switch 各加 1 行处理 `type_bitsupdate`：

```cpp
// print_decl (line 287 area)
case ch::core::lnodetype::type_bitsupdate:
    wires.push_back(node);
    declared_nodes_.insert(node);
    break;

// print_logic (line 361 area)
case ch::core::lnodetype::type_bitsupdate:
    print_bitsupdate(out, static_cast<ch::core::bitsupdateimpl *>(node));
    printed_logic_nodes_.insert(node);
    break;
```

---

## 4. 测试计划

### 4.1 在 `tests/test_verilog_gen.cpp` 中新增 12 个测试

每个新增的 ch_op 加一个 `TEST_CASE`：

| # | 测试名 | 关键 Verilog 模式 | 验证方式 |
|---|--------|----------------|----------|
| 1 | `AndReduce` | `assign result = &signal;` | `find("& signal")` |
| 2 | `OrReduce` | `assign result = \|signal;` | `find("\| signal")` |
| 3 | `XorReduce` | `assign result = ^signal;` | `find("^ signal")` |
| 4 | `Popcount` | `assign result = $countones(signal);` | `find("$countones")` |
| 5 | `RotateLeft` | `{src[N-1:X], src[N-X-1:0]}` | `find("{") && find("}")` |
| 6 | `RotateRight` | `{src[X-1:0], src[N-1:X]}` | 同上 |
| 7 | `BitSelect` (字面量) | `signal[idx_lit]` | `find("signal[")` |
| 8 | `BitSelect` (变量) | `signal[idx_var]` | 同上 |
| 9 | `BitsExtract` | `signal[hi:lo]` | `find("[hi:lo]")` 或模式匹配 |
| 10 | `Zext` | `{{ext{1'b0}}, src}` 或 `assign name = src;` | width 检查 |
| 11 | `Sext` | `{{ext{src[sw-1]}}, src}` | 复制模式 + `find("src[")` |
| 12 | `Assign` | `assign dst = src;` | `find("assign dst = src")` |
| 13 | `BitsUpdateLnodetype` | 通过 `bits_update<W>()` API 触发 | 验证 `type_bitsupdate` 路径（含 3 段拼接 + 边界处理） |

**注意**：
- `bit_sel` 测**两种形式**（字面量 + 变量索引）——Oracle 特别强调
- `rotate_l/r` 只测**字面量** amt（变量 amt 降级为 passthrough）
- `sext` 不要测 `"signed"` 字符串（不存在），测复制模式
- **不测 `ch_op::bits_update`**（死路径）

每个测试模式：
```cpp
TEST_CASE("VerilogGen - <OpName>", "[verilog][<tag>]") {
    auto ctx = std::make_unique<ch::core::context>("<op>_test");
    ch::core::ctx_swap ctx_guard(ctx.get());
    ch_uint<8> a(0xFF_d);
    ch_out<ch_uint<8>> out_port("io");
    out_port = <op-expr>(a);
    std::string verilog = generateVerilogToString(ctx.get());
    REQUIRE(verilog.find("EXPECTED_PATTERN") != std::string::npos);
}
```

### 4.2 端到端验证

| 测试套件 | 预期 |
|---------|------|
| `ctest -R test_verilog_gen` | 13+ cases 全过（含 12 新 + 1 lnodetype） |
| 现有 4 个测试 | 无回归 |
| `test_module` / `test_module_advanced` / `test_onehot` | 无回归 |
| 完整 `ctest -L base -E test_multithread` | 97+/97+ 通过 |

---

## 5. 风险评估

| 风险 | 严重度 | 概率 | 缓解 |
|------|--------|------|------|
| `print_logic` `type_output` "io hack" 暴露多 reg bug | 中 | 低（commit 1 修复） | 暂不修——本期聚焦新 op 覆盖；out of scope |
| `type_bitsupdate` pre-existing bug | **高** | **100%** | commit 1 修复此 bug；测试会立即验证 |
| `rotate_l/r` 变量 amt 降级为 passthrough | 低 | 中 | 明确文档化；Verilog synthesize 工具通常识别 concat 旋转 |
| SystemVerilog 依赖（`$countones`, `>>>`） | 中 | 中 | 文档化在 ADR-019 已有；可选用 Verilog-2001 函数模拟 |
| `<OP_NOT_IMPLEMENTED>` 在历史测试中作为预期 | 低 | 低 | 现有 4 个测试不触发新 op |
| `sext` 无 `signed` 关键字 | — | — | v2.0 移除此风险（Oracle 验证不需要） |
| `bits_update` 双路径混淆 | — | — | v2.0 移除此风险（opimpl 路径已死，仅 lnodetype） |

---

## 6. Commit 策略

按 git-master 规则：**3 commits**（Oracle 建议，从原 2 commit 升级）：

### Commit 1: `fix(codegen-verilog): declare and emit type_bitsupdate nodes`

**内容**：
- `include/codegen_verilog.h`：声明 `print_bitsupdate()` 方法
- `src/codegen_verilog.cpp`：
  - `print_decl` switch 添加 `type_bitsupdate` case
  - `print_logic` switch 添加 `type_bitsupdate` case
  - 实现 `print_bitsupdate()` 方法（3 段拼接 + 边界处理）
- `tests/test_verilog_gen.cpp`：新增 1 个测试 `BitsUpdateLnodetype`

**目的**：**优先修 pre-existing bug**（`type_bitsupdate` 节点完全不可见），并建立 lnodetype-specific printer 的模式。

**为什么先**：commit 2 会扩展 `print_op` switch，但 `type_bitsupdate` 是独立分支。先做它能确保后续 commit 的测试不会被这个旧 bug 污染。

### Commit 2: `feat(codegen-verilog): add 12 missing ch_op mappings`

**内容**：
- `include/codegen_verilog.h`：声明 6 个新打印方法（`print_bit_select`, `print_bits_extract`, `print_rotate_l`, `print_rotate_r`；`print_unary_op` 和 `print_binary_op` 是提取/扩展）
- `src/codegen_verilog.cpp`：
  - `get_op_str()` 补 11 case（不含 `bits_update`）
  - `print_op()` 重构为按 `op` switch 分发
  - 实现 6 个新打印方法
  - 提取/扩展 `print_unary_op` 和 `print_binary_op`

**目的**：纯 codegen 扩展，0 测试改动。容易 review。

### Commit 3: `test(codegen-verilog): cover new Verilog generation paths`

**内容**：
- `tests/test_verilog_gen.cpp`：新增 12 个新 TEST_CASE（每个新 op 一个 + `bit_sel` 双形式）

**目的**：测试验证 commit 2 的 12 个 op 映射。如果测试失败，立即定位到对应 op。

### Commit 顺序理由

| 顺序 | 理由 |
|------|------|
| 1 → 2 | commit 1 修 bug 独立，与 commit 2 的功能扩展不耦合 |
| 2 → 3 | 经典实现→测试配对；测试失败可立即定位到 commit 2 的新 op |
| 3 commits vs 2 | Oracle 建议：每 commit <150 行，便于 review；bug fix 单独可回滚 |

---

## 7. 不在本期范围

| 项 | 理由 | 下期建议 |
|----|------|---------|
| `print_logic` `type_output` "io hack" 修复 | 独立 hygiene 改进；改动可能影响多 reg 测试 | 单独 PR |
| `type_clock` / `type_reset` lnodetype 声明 | 需要 SystemVerilog `always` 特化、clocking block 复杂 | 单独 PR |
| DAG codegen（`src/codegen_dag.cpp`） | 与 Verilog 同步但属于不同代码路径 | 单独 PR |
| 移除空 `catch` 块（历史计划 1.1） | 独立 hygiene 改进 | 单独 PR |
| iverilog 集成测试（历史计划 2.2.1） | 需要 CI 基础设施 | 单独 PR |
| `simulator.cpp:338, 616` 不完整 switch | runtime 路径，不是 codegen | 单独 PR |
| `sanitize_name` 处理 Verilog 关键字 | 边角问题，out of scope | 单独 PR |

---

## 8. 验证门禁

| 门禁 | 命令 | 预期 |
|------|------|------|
| 编译 | `cmake --build build -j$(nproc)` | 0 error, 0 warning (来自本次改动) |
| 新测试 | `ctest -R test_verilog_gen --output-on-failure` | 12+ new cases 全过 |
| 全套件 | `ctest -L base -E test_multithread` | 97+/97+ |
| Verilog 语法 | `iverilog -g2012 -o /dev/null <generated>.v`（手动） | 语法正确（可选） |

---

## 9. 时间估计

| 阶段 | 估计 |
|------|------|
| Commit 1: `print_bitsupdate` + lnodetype 处理 | 30-45 分钟 |
| Commit 2: `get_op_str` + `print_op` 重构 + 6 个新方法 | 1-1.5 小时 |
| Commit 3: 12 个新测试 | 30-45 分钟 |
| Commit + 验证 | 15 分钟 |
| **总计** | **2.5-3.5 小时** |

---

## 10. 不在本期范围（续）

无。

---

## 11. 变更记录

| 日期 | 版本 | 变更 | 作者 |
|------|------|------|------|
| 2026-06-05 | v1.0 | 初始版本（post A+C 状态） | Sisyphus |
| 2026-06-05 | v2.0 | **Oracle 审查修订**：（1）移除 `ch_op::bits_update` 路径（opimpl 死路径，仅 lnodetype）；（2）`rotate_l/r` 修正为二元 op（lhs=值, rhs=amt）；（3）`bit_sel` 补充双形式说明（字面量+变量索引）；（4）移除 `sext` "signed 声明" 风险（用 replication 模式，不需要）；（5）`print_op` 已有 unary 分支（line 519）说明；（6）`get_op_str` 11 个 case（去除 bits_update）；（7）commit 数量 2→3；（8）测试 13→12 + 1 lnodetype 测；（9）补充 `type_output` "io hack" pre-existing 缺陷记录。 | Sisyphus |
