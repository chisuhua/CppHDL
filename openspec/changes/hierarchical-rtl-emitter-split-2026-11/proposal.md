# Proposal: 层次化 RTL 输出 · 第 5 段 —— emitter 拆分（`hierarchical-rtl-emitter-split-2026-11`）

## Why

层次发射需要「把**某一个** module 的节点子集 + 端口表 + 名字表**发射成一段 Verilog**」
的能力，而现状 `verilogwriter` 是**单体**：它假定「整个 ctx = 一个 module top」。

⚠️ **本 change NOT 是「纯搬移」**：`print_reg` / `emit_always_ff` **硬编码**
`default_clock` / `default_reset`（`src/codegen_verilog.cpp:278-283, 755-760`）
⇒ ⇒ 子模块的 `always_ff` 会引用**本模块内不存在**的信号名
⇒ ⇒ ⇒ **时钟名 MUST 参数化** ⇒ ⇒ 否则层次发射时必炸。

⚠️ **时钟名参数化会改扁平产物吗**？**不改** —— 扁平路径仍传 `default_clock` /
`default_reset` ⇒ ⇒ 「扁平产物相对 T3-①b 之后的基线**逐字节不变**」仍是零回归判据。

## What Changes

1. `verilogwriter` 拆出 `ModuleEmitter`：接收**节点子集 + 端口表 + 名字表**。
2. **时钟 / 复位名参数化**（`default_clock` / `default_reset` 改为入参，
   扁平路径传原值 ⇒ ⇒ **扁平产物逐字节不变**）。
3. 既有 `toVerilog()` 的**语义**一字不改（端口筛选 / 算子映射 / `always_ff` / mem）。

## 不做（Non-Goals）

- **不**发射层次结构（module 主体 / 实例化语句）⇒ ⇒ 那是 T3-④。
- **不**做等价性验证 ⇒ ⇒ 那是 T3-⑤。
- **不**改扁平产物的任何字节 ⇒ ⇒ 本 change 的**唯一**验收面就是「逐字节不变」。

## Acceptance

- [ ] `toVerilog()` 的扁平产物相对 **T3-①b 之后**的基线**逐字节不变**
      （`cmp` 退出码 **0** ⇒ ⇒ **不是**「差异逐条列明」）
- [ ] 时钟/复位名已参数化 ⇒ **mutation**：**去掉参数传递**（让子模块仍用硬编码
      `default_clock`）⇒ 生成物引用**未声明信号** ⇒ `verilator --lint-only` **必报错**
- [ ] `ctest -LE perf` 全绿 · 计数锚点同步

## Capabilities

**无 delta** —— 本 change 是**纯重构**（对外行为逐字节不变）
⇒ ⇒ `.openspec.yaml` 显式 `skip_specs: true`（**唯一**允许该标志的情形：本 change
显式声明「no spec changes」）。
