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
   实现它需要真实的串行时序逻辑 + 逐 bit 的串行时序测试（`set_input_value` 已能驱动
   `uart_rx`，缺测试而非验证能力）。

此外，followups 遗留的 **"AXI SoC example 读路径不通"** 也挂在本 change（D3）：
其原始诊断（"框架层子模块输入端口在 tick 内被覆盖"，见 archive 的 F11 与根
`AGENTS.md:280`）在本 change 的启动评审中**被实测证伪**（见 D3），真实原因在
example 测试台的完成判定。为避免错误结论继续传播，D3 一并在此收口。

## What Changes

### D1: `wstrb` 字节写语义

- 6 个 peripheral（`axi_{gpio,uart,timer,pwm,i2c,spi}.h`）的写路径消费 `wstrb`：
  `new = (old & ~mask) | (wdata & mask)`
- `mask` 由 `wstrb` 逐字节展开（bit0 → byte0 … bit3 → byte3）
- 需先定义并登记的语义：
  - 哪些寄存器是字节可访问的（32 位寄存器 vs 亚 32 位寄存器）
  - 亚 32 位寄存器的 `wstrb` 高位如何处理
  - 写 `DATA_IN`（只读寄存器）时的字节掩码处理
- 亚 32 位可写寄存器仅 3 处（其余可写寄存器均为 `ch_uint<32>`）：
  `axi_i2c.h` 的 `tx_data_reg<8>` / `prescale_reg<PRESCALE_WIDTH>`、
  `axi_spi.h` 的 `baud_reg<BAUD_WIDTH>`（8/16，已有一套 `wdata` 截位规则，
  见 `axi_spi.h:301-305`，字节写 MUST 与之统一）
- 只读寄存器（GPIO `DATA_IN`、UART `RX_DATA`/`STATUS`、Timer `COUNT`）当前已在
  `write_mapped` 之外走 `bresp=DECERR`，字节写 MUST NOT 把它们纳入写路径
- 回归影响：`wstrb=0xF`（整字写）→ 行为不变。**但"既有测试一律用 wstrb=0xF"的前提
  不成立** —— `tests/test_axi_{gpio,timer,uart}.cpp` 的 6 处**手写** AXI 写序列从未
  驱动 `wstrb`（读出 0），实施时 MUST 按 AXI4-Lite 要求补 `wstrb=0xF`
  （master 侧的义务），MUST NOT 让 peripheral 把 `wstrb=0` 当作整字写

### D2: UART 串行引擎（TX 移位 + RX 接收 + 波特率）

- **TX**：波特率分频器 + 8 位移位寄存器 + 起始位/停止位帧格式
  → `uart_tx` 输出真实串行波形；`tx_empty` 反映"移位寄存器空"
- **RX**：起始位检测 + 8 位采样 + 帧错误标志 → `rx_data_reg` / `rx_valid` / `irq` 有真实语义
- `status_reg` bit0 = TX_EMPTY、bit1 = RX_VALID 的语义随之确定
- **待决策：波特率配置通道** —— `AxiLiteUart` 的 `PRESCALE_WIDTH` 模板参数当前
  **完全未使用**，CTRL 也无分频位。实现前 MUST 三选一并登记到
  `include/axi4/AGENTS.md`（复用模板参数作固定分频 / 新增 `BAUD` 寄存器 /
  CTRL 高位分频），否则串行波形断言没有确定的 bit 周期
- **既有断言迁移**：`tests/test_axi_uart.cpp:121-136` 断言"写即排空 → 下一拍
  TX_EMPTY 回 1"；既有用例又普遍以 `uart_rx=0` 作空闲电平（RX 引擎会判成
  持续起始位）。两者 MUST 与引擎实现同批迁移
- 需要新增串行时序测试（master 侧用 `set_input_value` + 逐 bit `tick()` 模拟收发）——
  现有 `set_input_value` 已能驱动 `uart_rx`，缺的是测试而非验证能力；
  测试编写 + 既有 UART 断言的迁移是本 change 的主要工作量

### D3: AXI SoC example 读事务"未完成"（**原诊断已证伪**）

- **原诊断**（archive `axi4-peripheral-followups-2026-10` F11 + 根 `AGENTS.md:280`）：
  `sim.set_input_value(top.io().write, false)` 写入 `data_map[4] = 0`，一次 `tick()` 后
  变回 **1**，判定为"框架层子模块输入端口的值在 tick 内被覆盖"。
- **实测结论：不成立**。启动评审时用三条独立证据复核：
  1. **最小复现**（top input → `<<=` → child input）：中途把输入置 0 后跨 tick 保持 0，
     child 的组合输出同步跟随。顶层输入没有被回写。
  2. **SoC 探针**（在 `AxiSocTop` 增加 debug 输出）：读事务发起前 `write` 置 0，经过
     整轮 tick 仍为 0；master 状态正常 `0 → 2`，AR 握手发生（`gpio.rvalid` 置起）。
  3. **读事务实际完成**：把 `gpio_in` 设为 `0x5a` 后，在读完成的那一拍读回
     `rdata = 0x5a`。数据通路与矩阵读路由均正常。
- **真实根因**：`AxiLiteTestMaster` 的 `io().done = select(write_done, true, read_done)`
  （`examples/axi4/axi_soc_example.cpp:142`）是**组合单拍脉冲**；读完成那一拍
  `state` **同拍**由 2 回到 0（`:136-139`），而轮询循环在 `tick()` **之后**采样
  （`:324-333`）→ 必然错过。写路径只因完成拍 `state` 仍为 1 才"侥幸"可见。
- **交付**（只改 example 与文档，**不改框架层**）：
  - 完成判定改为可观测：`done` 打一拍保持，或按 `state` 回 idle + `rdata` 判定
  - 删除 `known limitation` 打印（`examples/axi4/axi_soc_example.cpp:335-340`）
  - 更正根 `AGENTS.md:280` 的错误根因描述（框架层输入端口被覆盖 → example 完成判定）
- **验收**：`./build/examples/axi4/axi_soc_example` → 输出 `Read complete! DATA_IN = 0x...`；
  `git diff --stat src/` 无改动。

## 非目标（Out of Scope）

- ❌ 改 `include/axi4/peripherals/axi_{gpio,uart,timer}.h` 的**通道形态**
  （已由 `fix-axi4-peripheral-rst-n-port-2026-10` 收敛，本 change 不动）
- ❌ 改 `include/axi4/peripherals/axi_{spi,pwm,i2c}.h` 的**响应保持**
  （已由 `axi4-peripheral-followups-2026-10` 收敛）
- ❌ 顶层 interconnect（`axi_interconnect_4x4.h`）的复位策略
- ❌ 改框架层 `src/simulator*.cpp` / `src/ast/instr_io.cpp` / JIT 输入路径 ——
  D3 的原"输入端口被 tick 覆盖"诊断已证伪，输入端口无缺陷，不在此 change 动框架

## Capabilities

### ADDED Requirements

见 `specs/dual-backend-test-coverage/spec.md`。
