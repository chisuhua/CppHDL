# UART TX 模块详细设计

> **模块**: UART TX (串行发送器)  
> **Phase**: Phase 1  
> **状态**: ✅ 已完成  
> **源文件**: `examples/spinalhdl-ported/uart/uart_tx.cpp`

---

## 模块概述

UART 发送器模块，验证 CppHDL 的状态机 DSL 和时序逻辑模式。对应 SpinalHDL 的 UartTx 实现。

---

## 设计规格

### 功能描述

- 将并行数据转换为串行比特流
- 支持可配置波特率
- 包含起始位、8 数据位、停止位
- 使用状态机控制发送时序

### IO 端口

| 端口 | 方向 | 类型 | 说明 |
|------|------|------|------|
| `write` | Input | `ch_bool` | 写使能 |
| `data` | Input | `ch_uint<8>` | 发送数据 |
| `tx` | Output | `ch_bool` | 串行发送引脚 |
| `busy` | Output | `ch_bool` | 发送忙标志 |

### 状态机

```
IDLE ──▶ START ──▶ DATA[0..7] ──▶ STOP ──▶ IDLE
```

---

## Verilog 生成验证

- ✅ Verilog 生成成功

---

## 相关资源

- [SpinalHDL UartTx 原文](SpinalHDL UartTx 实现)
- CppHDL 示例：`examples/spinalhdl-ported/uart/uart_tx.cpp`
