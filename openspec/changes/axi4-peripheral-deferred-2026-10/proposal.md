# Change: axi4-peripheral-deferred-2026-10 — AXI4 peripheral 的新增功能项

## Why

`axi4-peripheral-followups-2026-10` 收口 `include/axi4/peripherals/` 全体 peripheral 的
**协议缺陷**（响应保持、地址锁存、`bresp` 悬空、未映射地址响应、status 位语义）。

在该 change 的范围评审中，识别出 2 项**属新增功能而非修缺陷**的项 —— 它们会显著
扩大 diff 与风险，需要独立的 change 承接：

1. **`wstrb` 字节写语义**：`wstrb` 只出现在端口声明，**从未被使用** → 写恒为整字覆盖，
   违 AXI4-Lite 的字节写语义。实现它需要改 6 个 peripheral 的**所有** `ch_reg` 写路径，
   并先定义"字节道"语义（32 位寄存器的部分字节写如何与 `wstrb` 对应）。
2. **UART 串行引擎**：`axi_uart.h` 无 RX 引擎（`uart_rx` 输入未使用、`rx_data_reg`
   无 next 逻辑、`rx_valid` 恒 false）、无 TX 移位寄存器与波特率（`uart_tx` 恒为空闲高电平）。
   实现它需要真实的串行时序逻辑 + 对应的验证手段（当前无）。

## What Changes

### D1: `wstrb` 字节写语义

- 6 个 peripheral（`axi_{gpio,uart,timer,pwm,i2c,spi}.h`）的写路径消费 `wstrb`：
  `new = (old & ~mask) | (wdata & mask)`
- `mask` 由 `wstrb` 逐字节展开（bit0 → byte0 … bit3 → byte3）
- 需先定义并登记的语义：
  - 哪些寄存器是字节可访问的（32 位寄存器 vs 亚 32 位寄存器）
  - 亚 32 位寄存器的 `wstrb` 高位如何处理
  - 写 `DATA_IN`（只读寄存器）时的字节掩码处理
- 回归影响：既有测试一律用 `wstrb=0xF`（整字写）→ 行为不变，但 MUST 全量回归确认

### D2: UART 串行引擎（TX 移位 + RX 接收 + 波特率）

- **TX**：波特率分频器 + 8 位移位寄存器 + 起始位/停止位帧格式
  → `uart_tx` 输出真实串行波形；`tx_empty` 反映"移位寄存器空"
- **RX**：起始位检测 + 8 位采样 + 帧错误标志 → `rx_data_reg` / `rx_valid` / `irq` 有真实语义
- `status_reg` bit0 = TX_EMPTY、bit1 = RX_VALID 的语义随之确定
- 需要新增串行时序测试（master 侧模拟串行发送/接收）—— 当前**无此验证手段**，
  属本 change 的主要工作量与风险来源

### D3: 子模块输入端口的值在 tick 内被覆盖（框架层）

- **实测现象**：`sim.set_input_value(top.io().write, false)` 写入 `data_map[4] = 0`，
  但一次 `sim.tick()` 后 `data_map[4]` 变回 **1**（与同一拍被置位的另一个输入 `start` 相同）
- 该输入节点 `num_srcs() == 0`（无 `<<=` 驱动），按 `instr_input::eval()` 的语义
  （`*dst_ = *src_`，无 src 时不创建该指令）**不应**被写 → 需定位真正的写入者
- **影响**：测试台无法在仿真中途改变**子模块**的输入 → AXI SoC example 的读事务无法发起
  （写事务因首次赋值恰好为期望值而"侥幸"可用）
- 排查方向：`src/ast/instr_io.cpp`（`instr_input::eval`）、`src/simulator.cpp`
  （`eval_combinational` / `eval_sequential`）、`src/jit/jit_ir_gen_lnode_io.cpp`
  （JIT 的 `type_input` + driver 处理，per `include/jit/AGENTS.md`）
- **验收**：写一个最小复现 —— 子模块输入端口被 `set_input_value` 置 0 后，
  一次 `tick()` 后其值应仍为 0

## 非目标（Out of Scope）

- ❌ 改 `include/axi4/peripherals/axi_{gpio,uart,timer}.h` 的**通道形态**
  （已由 `fix-axi4-peripheral-rst-n-port-2026-10` 收敛，本 change 不动）
- ❌ 改 `include/axi4/peripherals/axi_{spi,pwm,i2c}.h` 的**响应保持**
  （已由 `axi4-peripheral-followups-2026-10` 收敛）
- ❌ 顶层 interconnect（`axi_interconnect_4x4.h`）的复位策略

## Capabilities

### ADDED Requirements

见 `specs/dual-backend-test-coverage/spec.md`。
