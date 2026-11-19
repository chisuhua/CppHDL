# FIFO 模块详细设计

> **模块**: Stream FIFO (流式先进先出队列)  
> **Phase**: Phase 1  
> **状态**: ✅ 已完成  
> **源文件**: `examples/spinalhdl-ported/fifo/stream_fifo_example.cpp`

---

## 模块概述

流式 FIFO 模块，验证 CppHDL 的 `ch_mem` 存储模式和 Stream 反压机制。对应 SpinalHDL 的 StreamFifo 实现。

---

## 设计规格

### 功能描述

- 支持任意数据宽度和深度
- 内置 backpressure (反压) 机制
- 空满标志正确管理
- 读写指针环绕处理

### IO 端口

| 端口 | 方向 | 类型 | 说明 |
|------|------|------|------|
| `io.push` | Input | Stream | 推入数据流 |
| `io.pop` | Output | Stream | 弹出数据流 |
| `occupancy` | Output | `ch_uint<N>` | 当前占用数 |

### 内部结构

- 使用 `ch_mem` 作为存储介质
- 读/写指针使用 `ch_reg` 寄存器
- 空满检测逻辑
- Stream valid/ready 握手协议

---

## Verilog 生成验证

- ✅ Verilog 生成成功

---

## 相关资源

- [SpinalHDL StreamFifo 原文](SpinalHDL StreamFifo 实现)
- CppHDL 示例：`examples/spinalhdl-ported/fifo/stream_fifo_example.cpp`
- 库头文件：`include/chlib/fifo.h`
