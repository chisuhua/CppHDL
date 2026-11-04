# 设计：emitter 拆分（`hierarchical-rtl-emitter-split-2026-11`）

> 决策在 ADR-058 §2.3（新入口 · 既有路径逐字节不变）· ADR-059 D5（不改 `toVerilog()` 语义）。

## 1. 为什么不是「纯搬移」

`print_reg` / `emit_always_ff` **硬编码** `default_clock` / `default_reset`
（`src/codegen_verilog.cpp:278-283, 755-760`）⇒ ⇒ 层次化子模块的 `always_ff`
会引用**本模块内不存在**的信号名 ⇒ ⇒ ⇒ 时钟名 **MUST 参数化**。

⚠️ **参数化 ≠ 改扁平产物**：扁平路径**仍传** `default_clock` / `default_reset`
⇒ ⇒ 生成文本逐字节相同 ⇒ ⇒ 「扁平产物逐字节不变」这条零回归判据**仍然成立**。

## 2. 拆分边界

| 留在 `verilogwriter` | 拆到 `ModuleEmitter` |
|---|---|
| ctx 级遍历 / eval list | **节点子集**（由分析层给出） |
| `node_names_` 的**建立** | **名字表**（ctx 级只读快照 · ADR-061 §5） |
| 顶层 header / footer | **端口表**（方向 / 宽度 / 顺序） |
| 扁平路径的默认时钟名 | **参数化的**时钟 / 复位名 |

## 3. 风险（自 umbrella §7 分派）

| # | 风险 | 对策 |
|---|---|---|
| 2 | `default_clock` / `default_reset` **硬编码** ⇒ 子模块 `always_ff` 引用不存在的信号 | 本 change **MUST 参数化**时钟名（⇒ 不是「纯搬移」）⇒ **mutation**：去掉参数传递 ⇒ lint 必报错 |

---

## 4. 实施记录（**文档 MUST 与代码一致**）

落点：`include/codegen_verilog.h`（`ModuleView` / `ModuleEmitter` / `make_flat_module_view`）
+ `src/codegen_verilog.cpp`（原 `verilogwriter` 的实现体整体搬进 `ModuleEmitter`；
`verilogwriter` 变成**薄适配器**）。

### 4.1 落地形态（与 §2 的边界表一致）

```cpp
struct ModuleView {
    std::string module_name = "top";
    std::string clock_name  = "default_clock";   // ⇐ 参数化
    std::string reset_name  = "default_reset";
    std::vector<lnodeimpl*> nodes;               // 遍历顺序（拓扑序）
    std::vector<lnodeimpl*> ports;               // 已排序 / 已过滤的端口
    std::unordered_map<const lnodeimpl*, std::string> names;  // 全局唯一命名表
};
ModuleView make_flat_module_view(context*);      // 扁平 = 整颗 ctx 一个 module top
class ModuleEmitter { ModuleEmitter(context*, ModuleView); void print(std::ostream&); };
class verilogwriter { /* 旧 API 不变 */ std::unique_ptr<ModuleEmitter> emitter_; };
```

⚠️ **端口选择不再是发射器的事**：`print_header` 原来自扫 `ctx_->get_nodes()` 挑
input/output/clock/reset 再排序 / 过滤 —— 这段**逐字搬进** `make_flat_module_view`
（层次化路径由分析层给端口表）⇒ ⇒ 发射器只认 `ModuleView`。

### 4.2 「逐字节不变」怎么判的（**实测**，不是推断）

单测只能证明「两条路径互相一致」，不能证明「与拆分前一致」⇒ ⇒ 另做**外部基线**比对：
用一个独立的小程序对 3 个设计调 `toVerilog()`，拆分**前**存一份、拆分**后**再存一份：

| 设计 | 覆盖的 codegen 面 | `cmp` 结果 |
|---|---|---|
| `RegChain`（reg + 加减 + 比较 + `select`） | `print_reg` / `emit_always_ff` / 算子 / mux | ✅ 逐字节相同 |
| `MemTop`（`ch_mem` 读 + 写） | `print_mem_decl` / `print_mem_init` / 读写端口 | ✅ 逐字节相同 |
| `riscv::Rv32iPipeline`（145 KB） | 全量（7 个子模块 / 大量 reg / 算子 / 端口） | ✅ 逐字节相同 |

### 4.3 时钟 / 复位名参数化（4 处，缺一即错）

| 位置 | 原（硬编码） | 现 |
|---|---|---|
| `emit_always_ff` | `posedge default_clock or posedge default_reset` | `view_.clock_name` / `view_.reset_name` |
| `print_reg`（无 next 的保持分支） | 同上 | 同上 |
| `print_mem_read_port`（`sread` 的 `always_ff`） | 同上 | 同上 |
| `print_mem_write_port` | `posedge default_clock` | `view_.clock_name` |

### 4.4 判据 + 变异（**实测**）

`tests/test_verilog_module_emitter.cpp`（2 个 TEST_CASE / 11 断言）：
① `verilogwriter` 的输出 == `ModuleEmitter(make_flat_module_view(ctx))` 的输出（**字符串相等**）；
② 把 `ModuleView` 的 clock/reset 改名成 `clk_i` / `rst_ni` ⇒ 文本里 MUST 出现新名、
**MUST NOT** 再出现 `default_clock` / `default_reset`，且**每一处** `always_ff` 都换了
（`posedge clk_i` 出现 ≥ 2 次 —— 覆盖 reg 与 mem 两条路径）。

**M1（去掉参数传递 ⇒ 让子模块继续用硬编码时钟）**：
- 单测 **RED**（`REQUIRE(text.find("default_clock") == npos)` 失败）；
- 把该变异下的产物交给 `verilator --lint-only` ⇒ **exit 1**，
  `%Error: Can't find definition of variable: 'default_clock'` / `'default_reset'`
  ⇒ ⇒ **正是** proposal §Acceptance 预测的失败形态；
- 对照组（未变异的改名产物）`verilator --lint-only` ⇒ **exit 0**（无告警/错误）。

### 4.5 顺带清掉的死代码（Zero-Debt）

- `verilogwriter::node_uses_` —— 构造时**建了**（每个 src 边插一次），**从不读**
  ⇒ 整个成员 + 建表循环删除；
- `print_reg` 里的 `std::string clock_name = "default_clock"; (void)clock_name;`
  （注释写着 "kept for future reset-support; unused at present"）⇒ 删除。
