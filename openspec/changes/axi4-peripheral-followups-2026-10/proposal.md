# Change: axi4-peripheral-followups-2026-10 — AXI4 peripheral 收敛后的遗留项

## Why

`fix-axi4-peripheral-rst-n-port-2026-10` 把 `AxiLiteUart` / `AxiLiteTimer` / `AxiLiteGpio`
三个 AXI4-Lite slave 收敛到规范形态（`rst_n` + `busy`/`aw_done`/`b_valid`/`r_valid` +
AW→W 相位分离 + 响应保持）。

收敛过程中暴露出一批**不属于该 change 范围**的遗留项。它们分散在不同子系统中，
混进上一个 change 会让 diff 失焦、回归定位困难，因此单独开本 change 收口。

### 遗留项清单

| # | 项 | 位置 | 性质 |
|---|---|---|---|
| F1 | `AxiDma` 缺 `rst_n` | `include/axi4/peripherals/axi_dma.h` | AXI4 **master**（非 `AxiLite*` slave），复位语义与 slave 不同，13 个 `ch_reg` 的复位值需单独定义 |
| F2 | `bvalid` 双赋值（不保持） | `include/axi4/peripherals/axi_pwm.h:116,153`、`include/axi4/peripherals/axi_i2c.h:149,290` | 后赋值覆盖前赋值 → `bvalid` 退化为组合 1 周期脉冲，违反 AXI4-Lite 响应保持 |
| F3 | `bvalid = aw_done`（BVALID 早于 WDATA） | `include/axi4/peripherals/axi_spi.h:350` | BVALID 在写数据被接受之前就拉高 |
| F4 | UART status / irq 语义缺失 | `include/axi4/peripherals/axi_uart.h` | TX_EMPTY status 位是死代码；`tx_empty`/`rx_valid` 无 next 逻辑；`irq` 恒 0 |
| F5 | 孤儿头 `rv32i_soc.h` | `examples/riscv-mini/src/rv32i_soc.h` | 全仓库无任何 TU include（`grep -rn rv32i_soc` 除自身外零命中）→ 改动不会被编译；其中还实例化了 `AxiLiteUart`（缺 `rst_n` 接线） |
| F6 | `AxiSocTop` example 的 `GPIO_OUT` 打印 0x0 | `examples/axi4/axi_soc_example.cpp` | 写 0xAB 未落地（根因在 example 自身的 test master / matrix 路径，与 peripheral 无关）；无 assert → CI 察觉不到 |
| F7 | 主 spec 的 `AxiLiteDma` 类名不存在 | `openspec/specs/dual-backend-test-coverage/spec.md` | `AxiLiteDma` 类从未存在（真实类名是 `AxiDma`）；该错误来自已归档 change `2026-10-20-dual-backend-axi4-peripherals` 的 spec delta。**注**：该 Requirement 的 MODIFIED 由下游 change `dual-backend-axi4-peripherals-extended-2026-10` 统一负责（它重写整个 peripheral 列表），本 change 只做**验证**，不重复提交 MODIFIED delta（避免两个 change 同时 MODIFIED 同一 Requirement 造成 archive 冲突） |
| F8 | 顶层 SoC 未暴露 `rst_n` | `examples/axi4/axi_soc_example.cpp` 的 `AxiSocTop` | peripheral 有复位口但 SoC 顶层没有；复位逃生路径无法从 SoC 外部触达 |
| F9 | GPIO 写侧/读侧地址映射不一致 | `include/axi4/peripherals/axi_gpio.h` | 写侧：`0x00→DATA_OUT`、`0x04→DIRECTION`、`0x08→INTERRUPT`；读侧：`0x00→DATA_OUT`、`0x04→DATA_IN`、`0x08→DIRECTION`、`0x0C→INTERRUPT`。按头部文档的映射写 DIRECTION(0x08) 会落到 INTERRUPT 寄存器；写侧比读侧少了 DATA_IN 一档，整体错位一档 |

## 目标

把上述遗留项逐条收口，使 `include/axi4/peripherals/` 全体 peripheral 满足同一套规范形态，
并让主 spec / 示例 / 文档与实际代码一致。

## What Changes

### F1: `axi_dma.h` 加 `rst_n`

- `__io` 加 `ch_in<ch_bool> rst_n;`
- 13 个 `ch_reg` 逐个定义复位值（必须给出显式表，不允许"init_val"占位）：
  - `state` → 0（与 `is_idle` 耦合，`axi_dma.h:48`）
  - `src_addr` / `dst_addr` / `curr_raddr` / `curr_waddr` / `length_reg` / `bytes_rem` → 0
  - `beat_cnt` → 0、`rdata_buf` → 0
  - `data_vld` / `err_flag` → false、`irq_en` → false
  - `id_reg` → 0（ID 语义需明示：每次 `reg_start` 自增，故复位归 0）
- `state->next` 复位到 0（idle）
- **注意**：`AxiDma` 是 AXI4 master，`rst_n` 的复位对象是 master 自身的 FSM 状态，
  与 slave 的 "复位寄存器值" 语义不同 —— spec delta 需单列 Requirement，不复用
  `fix-axi4-peripheral-rst-n-port-2026-10` 的 slave Requirement

### F2 + F3: 参考实现回收到规范形态

- `axi_pwm.h`：删除 L153 `io().bvalid = w_handshake;`，保留 L116 的
  `io().bvalid = select(aw_done, true, false);`（或按新规范改为 `b_valid` 寄存器）
- `axi_i2c.h`：`build_register_bank()` 里的 L149 `io().bvalid = w_handshake;`
  与 `describe()` 里的 L290 冲突（helper 后调用 → 覆盖）→ 二者取一
- `axi_spi.h`：L350 `io().bvalid = select(s.aw_done, true, false);`
  → 改为 `b_valid` 寄存器（与新规范一致：BVALID 在 WDATA 之后拉高）
- 三个文件同步加 `b_valid` 寄存器 + `aw_done` 的 `b_valid && bready` 清零语义

### F4: UART status / irq 语义

- 明确 `tx_empty` / `rx_valid` 的状态机（何时置位 / 清除）
- `status_reg` bit0 = TX_EMPTY、bit1 = RX_VALID（与寄存器映射表 L9 对齐）
- `irq` 由 `rx_valid` 驱动（现状 L137 已是，但因 `rx_valid` 恒 false 而恒 0）
- 需要决定 TX 侧的"发送完成"事件来源（当前 UART TX 是简化实现：写 TX_DATA 即发送）

### F5: 孤儿头 `rv32i_soc.h` 处置（二选一，需决策）

- 选项 A：删除（该文件无任何 TU include，属死代码；删除符合 ZERO-DEBT "no orphan files"）
- 选项 B：纳入构建 + 补 `AxiLiteUart` 的 `rst_n` 接线 + 补对应测试
- **默认建议 A**（先确认 `git log` 无其它引用）

### F6: `AxiSocTop` example 的写路径

- 定位并修复 `GPIO_OUT` 打印 0x0 的根因（example 内的 `AxiLiteTestMaster` 与
  `AxiLiteMatrix2x2` 的握手/仲裁路径）
- 补 assert（`REQUIRE(gpio_out == 0xAB)`）使 CI 能捕获该类回归
- 同步确认 `AxiLiteTestMaster` 是否满足 AXI4-Lite "master 必须保持 WVALID 直到 WREADY"
  （现状 `io().wvalid = select(write_start, io().awready, false)` 是单周期脉冲，不满足）

### F7: 主 spec 类名修正（验证项，不提交 spec delta）

- `AxiLiteDma` → `AxiDma` 的 Requirement 级修正在下游 change
  `dual-backend-axi4-peripherals-extended-2026-10` 的 MODIFIED delta 中统一完成
  （它重写整个 peripheral 列表）
- 本 change 在 archive 前**验证**主 spec 中不再出现 `AxiLiteDma`
- **不得**在本 change 重复提交同一条 Requirement 的 MODIFIED delta
  （两个 change 同时 MODIFIED 同一 Requirement 会在 archive 时产生冲突）

### F8: `AxiSocTop` 暴露 `rst_n`

- `AxiSocTop` 加 `ch_in<ch_bool> rst_n`，透传给 gpio / uart
- `main()` 中 `sim.set_input_value(top.io().rst_n, 1u);`

### F9: GPIO 写侧 / 读侧地址映射对齐

- `axi_gpio.h` 写侧解码（`sel_data_out` / `sel_direction` / `sel_interrupt`）与读侧解码
  （`read_sel_data_out` / `read_sel_data_in` / `read_sel_direction` / `read_sel_interrupt`）
  MUST 指向同一组寄存器
- 目标映射（与头部文档一致）：
  `0x00→DATA_OUT`、`0x04→DATA_IN`、`0x08→DIRECTION`、`0x0C→INTERRUPT`
  （写侧 `0x04` 的 DATA_IN 为只读，写入被忽略）
- 同步更新 `tests/test_axi_gpio.cpp` 中依赖写侧旧映射的用例
  （当前用 `kRegDirectionWrite = 0x04` 锁定旧行为）

## 非目标（Out of Scope）

- ❌ 改 `include/axi4/peripherals/axi_{uart,timer,gpio}.h`
  （已由 `fix-axi4-peripheral-rst-n-port-2026-10` 收敛）
- ❌ 改 `tests/test_axi_{uart,timer,gpio}.cpp`（同上）
- ❌ 改 `tests/test_axi_{pwm,i2c,spi}_behavior.cpp`（已归档）
- ❌ 改 `tests/test_verilator_*.cpp`（spec 锁定的 macro reference impl）
- ❌ 顶层 interconnect（`axi_interconnect_4x4.h`）的复位策略

## Acceptance

### Scenario: 编译与回归不退化

- **WHEN** `cmake --build build -j$(nproc) 2>&1 | grep -cE "error:|Error"`
- **THEN** `0`
- **AND** `cd build && ctest -LE perf --output-on-failure` → 全 PASS（数字按实际 `ctest -N` 校准）

### Scenario: F1 `AxiDma` 复位语义有显式表

- **WHEN** reviewer 阅读 `axi_dma.h` 的复位实现
- **THEN** MUST 能找到 13 个 `ch_reg` 各自的显式复位值
- **AND** `state` 的复位值 MUST 为 0（与 `is_idle` 一致）
- **AND** `AxiDma` MUST 有独立 Requirement（不得复用 slave 的 Requirement）

### Scenario: F2/F3 参考实现与新规范一致

- **WHEN** 对 `axi_pwm.h` / `axi_i2c.h` / `axi_spi.h` 执行
  `grep -c "bvalid = " <file>`
- **THEN** 每个文件 MUST 只有 1 处 `bvalid` 赋值（消除双赋值）
- **AND** `bvalid` MUST 由寄存器驱动（保持语义）

### Scenario: F5 孤儿头处置完成

- **WHEN** `grep -rn "rv32i_soc" --include=*.cpp --include=*.h . | grep -v "src/rv32i_soc.h:"`
- **THEN** 命中 0 行（已删除）**或** ≥ 1 行（已纳入构建并有对应测试）
- **AND** 不得保持"存在但不被编译"的中间态

### Scenario: F6 example 有行为断言

- **WHEN** `./build/examples/axi4/axi_soc_example`
- **THEN** 输出 `GPIO_OUT = 0xab`
- **AND** exit 0
- **AND** example 源码含对 `gpio_out` 的断言（CI 可捕获回归）

### Scenario: F7 主 spec 类名一致（验证项）

- **WHEN** `grep -c "AxiLiteDma" openspec/specs/dual-backend-test-coverage/spec.md`
- **THEN** `0`
- **AND** `grep -c "AxiDma" openspec/specs/dual-backend-test-coverage/spec.md` ≥ 1
- **AND** 本 change **MUST NOT** 提交 "AXI4 peripheral behavior test SHALL 走
  DUAL_BACKEND_TEST_PAIR" 的 MODIFIED delta（由下游 change 统一负责）

### Scenario: openspec 合规

- **WHEN** `openspec validate axi4-peripheral-followups-2026-10 --strict`
- **THEN** valid
- **AND** archive 时**不得**使用 `--skip-specs`（本 change 有 spec delta）

### Scenario: F8/F9 文档与测试同步

- **WHEN** `grep -c "kRegDirectionWrite" tests/test_axi_gpio.cpp`
- **THEN** 修复后应为 `0`（写侧与读侧映射统一后不再需要写侧专用偏移常量）
- **AND** `grep -c "rst_n" examples/axi4/axi_soc_example.cpp` ≥ 3
  （顶层 `rst_n` 端口 + gpio + uart 的连线）

## Capabilities

### ADDED Requirements

见 `specs/dual-backend-test-coverage/spec.md`（2 个 ADDED Requirement：
AXI4 master 复位契约 + 参考实现与规范形态一致）。
