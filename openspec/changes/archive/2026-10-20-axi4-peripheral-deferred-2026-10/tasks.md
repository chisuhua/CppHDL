## Tasks

> 前置依赖：`axi4-peripheral-followups-2026-10` 必须先 ship
> （本 change 假定全体 peripheral 已收敛到规范形态，且 `bresp` / `rresp` 已完整驱动）。
>
> 建议执行顺序：**Task 5 → Task 1 → Task 2 → Task 3 → Task 4**。
> Task 5 成本最低且能先清掉错误文档结论；Task 1 的语义表是 Task 2 的输入；
> Task 3/4 与 D1 无耦合，可分两个交付批次。

- [x] ### Task 1 (D1-a): 定义并登记"字节道"语义
  - **action**:
    - 决定哪些寄存器字节可访问（32 位 / 亚 32 位）
    - 决定亚 32 位寄存器的 `wstrb` 高位处理方式
    - 决定只读寄存器（`DATA_IN` / `RX_DATA` / `COUNT`）被写时的处理
    - 在 `include/axi4/AGENTS.md` CONVENTIONS 登记该语义
    - **受影响的亚 32 位可写寄存器（已核清，务必全部覆盖）**：
      - `axi_i2c.h`：`tx_data_reg<8>`、`prescale_reg<PRESCALE_WIDTH>`
      - `axi_spi.h`：`baud_reg<BAUD_WIDTH>`（8 或 16）—— 与现有 `wdata` 截位
        （`axi_spi.h:301-305`）保持同一语义，不得出现两套掩码规则
      - 其余可写寄存器（gpio/uart/timer/pwm）均为 `ch_uint<32>`
    - **只读寄存器现状（写侧已排除，字节写不得绕过）**：
      GPIO `DATA_IN`、UART `RX_DATA` / `STATUS`、Timer `COUNT` 均在 `write_mapped`
      之外 → 走 `bresp=DECERR`。Task 2 MUST NOT 把它们纳入字节写路径
  - **verify**:
    - reviewer 复核：语义表覆盖 6 个 peripheral 的全部可写寄存器

- [x] ### Task 2 (D1-b): 6 个 peripheral 的写路径消费 `wstrb`
  - **action**:
    - 写路径改为 `new = (old & ~mask) | (wdata & mask)`
    - `mask` 由 `wstrb` 逐字节展开
  - **实施记录**:
    - 新增 `include/axi4/byte_lane.h`（`byte_lane_mask` / `byte_lane_valid_mask<W>` /
      `byte_lane_apply<W>`），6 个 peripheral 各自展开一次掩码后逐寄存器套用
    - 新增 `tests/test_axi_byte_lane.cpp`（7 个 TEST_CASE，覆盖 6 个 peripheral）
    - `tests/axi_lite_slave_fixture.h` 的 `write_io` / `write` 增加 `wstrb` 参数（默认 0xF）
    - **修正既有测试**：`tests/test_axi_{gpio,timer,uart}.cpp` 的 6 处**手写** AXI 写序列
      此前未驱动 `wstrb`（读出 0）→ 按 AXI4-Lite 要求补 `wstrb=0xF`。
      ⚠️ 提案中"既有测试一律用 wstrb=0xF"的前提**不成立**（仅 fixture 与 `_behavior`
      测试驱动了 wstrb）—— 详见 proposal D1 的更正
  - **verify**:
    - 对 6 个 peripheral 统计 `io().wstrb` 的**使用**次数（排除端口声明）→ MUST ≥ 1
    - 新增用例：部分字节掩码写 → 未选中字节保持原值
    - `cd build && ctest -R "test_axi_(pwm|i2c|spi|gpio|uart|timer)" --output-on-failure` → 全 PASS

- [x] ### Task 3 (D2-a): UART TX 串行引擎（波特率 + 移位 + 帧）
  - **action**:
    - **先定波特率配置通道（三选一，需在 `include/axi4/AGENTS.md` 登记）**：
      复用当前**完全未使用**的 `PRESCALE_WIDTH` 模板参数作固定分频 /
      新增 `BAUD` 寄存器 / 用 `CTRL` 高位作分频值。
      未定此项则 Task 3 的波形断言无确定 bit 周期，MUST NOT 开工
    - 波特率分频器 + 8 位移位寄存器 + 起始位/停止位
    - `uart_tx` 输出真实串行波形；`tx_empty` 反映移位寄存器空
    - **迁移既有断言**：`tests/test_axi_uart.cpp:121-136` 现断言"写即排空 →
      下一拍 TX_EMPTY 回 1"，真实移位引擎下 MUST 改为"发送期间 TX_EMPTY=0，
      帧结束后回 1"
  - **实施记录**:
    - **波特率通道定案**：复用原 `PRESCALE_WIDTH` 模板参数 → 更名
      `UART_BIT_PERIOD` 并真正生效（bit 周期 = N 个时钟周期，默认 16），
      已登记到 `include/axi4/AGENTS.md`（CONVENTIONS → UART 串行语义）
    - `tx_empty` 由 `ch_reg` 改为**组合视图**（`!tx_busy`）—— 与移位寄存器
      状态严格一致（发送期间恒 0，帧结束回 1）
    - `CTRL` 明确为**保留**（不参与 TX/RX 门控），头部注释同步
  - **verify**:
    - 新增串行波形测试（采样 `uart_tx` 还原字节）
    - `cd build && ctest -R test_axi_uart --output-on-failure` → PASS

- [x] ### Task 4 (D2-b): UART RX 接收引擎（起始位 + 8 位采样）
  - **action**:
    - 起始位检测 + 8 位采样 + 帧错误标志
    - `rx_data_reg` / `rx_valid` / `irq` 获得真实语义
    - **统一 `uart_rx` idle 电平为高电平**：现有用例普遍写
      `sim.set_input_value(io.uart_rx, 0)`（如 `tests/test_axi_uart.cpp:84`），
      在 RX 引擎下会被判成持续起始位 → 全部 MUST 迁移为 idle=1；
      example 已是 idle=1（`examples/axi4/axi_soc_example.cpp:284`），以 example 为准
    - 明确 `irq` 与 `CTRL.INTERRUPT_EN` 的关系（当前 spec 只要求
      `irq` 由 `rx_valid` 驱动）：若做门控则 MUST 同步改 spec，否则写明"不门控"
  - **实施记录**:
    - RX 状态机：idle → 起始位（等 1.5 bit 同步）→ 数据位（每 bit 采一次）
      → 停止位（校验）；采样值右移插入最高位 → LSB 先收
    - `STATUS` bit2 = `RX_FRAME_ERROR`（停止位为低 → 置位，仅 `rst_n` 清除）
    - `rx_valid` 在 **R 握手**（读事务完成）且锁存地址为 `RX_DATA` 时清零 ——
      ⚠️ 不能用 AR 握手：地址在 AR 握手当拍才被锁存，握手当拍解码仍是旧地址
    - **框架层根因更正**：组合逻辑的"结束/完成"判定 MUST 用**下一个值**
      （`next_phase == 10` / `next_bits == 8`）而非当前值。本框架组合求值是
      **单遍、无拓扑序**（`src/simulator.cpp::eval_combinational`），寄存器比较的
      求值时机受节点创建顺序影响 —— 用当前值判定会让 RX 状态机早一拍离开 DATA 态，
      实测只采到 7 位（收到 `0xFE` 而非 `0xFF`）。已登记为
      `include/axi4/AGENTS.md` ANTI-PATTERN
    - 既有测试的 `uart_rx` 空闲电平全部由 0 迁移为 1（10 处 `test_axi_uart.cpp`
      + 1 处 `test_axi_uart_behavior.cpp`）
  - **verify**:
    - 新增 RX 测试（模拟串行发送 → 读 `RX_DATA` → `rx_valid` 清除）
    - `cd build && ctest -R test_axi_uart --output-on-failure` → PASS

- [x] ### Task 5 (D3): AXI SoC example 读完成判定（**原框架层诊断已证伪**）
  - **action**:
    - 修 `examples/axi4/axi_soc_example.cpp` 的完成判定：把 `done` 打一拍保持，
      或按 `state` 回到 idle + `rdata` 判定；读完成那一拍 `state` 与 `read_done`
      同拍归位（`:136-139`），组合单拍脉冲 `done`（`:142`）在 `tick()` 后采样必然丢失
    - 删除 `known limitation` 打印（`:335-340`）
    - 更正根 `AGENTS.md:280` 的错误结论："框架层子模块输入端口被 tick 覆盖"
      → "example 完成判定在 tick 后采样错过单拍脉冲"
    - **不改**框架层：`src/simulator*.cpp` / `src/ast/instr_io.cpp` / JIT 输入路径。
      输入端口无缺陷（最小复现与 SoC 探针均已验证跨 tick 保值）
  - **verify**:
    - proposal D3 的三条复核证据可重跑（最小复现 / SoC 探针 / `rdata` 回读）
    - `./build/examples/axi4/axi_soc_example` → 输出 `Read complete! DATA_IN = 0x...`
      （不再打印 known limitation）
    - `grep -n 'known limitation' examples/axi4/axi_soc_example.cpp` → 0 命中
    - `git diff --stat src/` → 无改动（D3 不碰框架层）
    - `cd build && ctest -LE perf` → 全 PASS

- [x] ### Task 6: 全量回归 + 计数同步
  - **实施记录**:
    - `ctest -LE perf` → **165/165 PASS**（含 26 个 verilator_e2e）
    - `./run_all_ported_tests.sh` → **28/28 PASS**
    - `bash scripts/verify_test_counts.sh` → exit 0
    - 计数同步：CTEST_TOTAL 175→176、non-perf 164→165、
      `tests/test_*.cpp` 105→106（新增 `test_axi_byte_lane.cpp`），
      根 `AGENTS.md` NOTES §Test counts 已更新
  - **verify**:
    - `cd build && ctest -LE perf --output-on-failure` → 全 PASS
    - `cd build && ctest -L verilator_e2e -LE perf --output-on-failure` → 全 PASS
    - `bash scripts/verify_test_counts.sh` → exit 0

- [x] ### Task 7: openspec archive（**禁用 `--skip-specs`**）
  - **action**:
    - `openspec archive axi4-peripheral-deferred-2026-10 --yes`
  - **实施记录**:
    - archive → `openspec/changes/archive/2026-10-20-axi4-peripheral-deferred-2026-10/`
    - 主 spec `openspec/specs/dual-backend-test-coverage/spec.md` 合并 **+2** Requirement
      （`AXI4 peripheral MUST 支持字节写` / `UART peripheral MUST 提供串行收发引擎`）
    - `openspec validate --specs --strict` → 14 passed, 0 failed
  - **verify**:
    - `ls openspec/changes/archive/ | grep deferred` → 命中
