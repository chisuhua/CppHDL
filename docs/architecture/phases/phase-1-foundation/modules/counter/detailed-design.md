# Counter 模块详细设计

> **模块**: Counter (计数器)  
> **Phase**: Phase 1  
> **状态**: ✅ 已完成  
> **源文件**: `examples/spinalhdl-ported/counter/counter_simple.cpp`

---

## 模块概述

基础计数器模块，验证 CppHDL 的 `ch_reg` 和 `ch_uint` 基本操作模式。对应 SpinalHDL 基础 Counter 模式。

---

## 设计规格

### 功能描述

- 支持 n 位递增计数
- 支持使能信号 (enable)
- 支持同步复位
- 计数溢出时自动回零

### IO 端口

| 端口 | 方向 | 类型 | 说明 |
|------|------|------|------|
| `en` | Input | `ch_bool` | 使能信号 |
| `rst` | Input | `ch_bool` | 同步复位 |
| `count` | Output | `ch_uint<N>` | 计数值 |

---

## Verilog 生成验证

- ✅ Verilog 生成成功
- 输出文件：`counter.v`

---

## 相关资源

- [SpinalHDL Counter 原文](SpinalHDL Counter 模式)
- CppHDL 示例：`examples/spinalhdl-ported/counter/counter_simple.cpp`
