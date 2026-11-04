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
