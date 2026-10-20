# Change: fix-axi4-peripheral-rst-n-port-2026-10 — 4 个 AXI4 peripheral 加 rst_n + busy self-clear

## Why

**现状**（oracle 2026-10-20 audit, dual-backend-axi4-peripherals pilot 触发）：

`dual-backend-axi4-peripherals` change（archive `2026-10-20`）pilot `tests/test_axi_uart_behavior.cpp` 验证发现 4 个 AXI4-Lite peripheral **缺 `rst_n` port** + 缺 busy self-clear → 首次 write 后 busy 永久锁死 → dual 行为 parity 测试不可行：

| Peripheral | rst_n port | busy self-clear | dual 化路径 |
|---|---|---|---|
| `AxiLitePwm` | ✅ | ✅ `select(!rst_n, false, ...)` | ✅ 已 archive `dual-backend-axi4-peripherals` |
| `AxiLiteI2c` | ✅ | ✅ | ✅ 已 archive |
| `AxiLiteUart` | ❌ | ❌ busy 永久锁 | ❌ 需修 design |
| `AxiLiteTimer` | ❌ | ❌ busy 永久锁 + IO port `&&` 反模式（axi4/AGENTS.md ANTI-PATTERN） | ❌ 需修 design |
| `AxiLiteGpio` | ❌ | ❌ busy 永久锁 | ❌ 需修 design |
| `AxiLiteDma` | ❌ | ❌ ch_reg<bool> init 不应用 | ❌ 需修 design |

**详细 design 问题**（per oracle pilot）：

### `axi_uart.h`
- `ch_reg<ch_bool> busy(ch_bool(false));` (line 66) + `ch_reg<ch_bool> tx_empty(ch_bool(true));` (line 75) + `ch_reg<ch_bool> rx_valid(ch_bool(false));` (line 76) —— **全缺 rst_n reset**
- `busy->next` (line 142) 永久锁：一旦 write 触发 `any_handshake=true`，busy 永远=true 直到下个 clock tick `any_ready=true` 才清——**但 axi_uart 是 combinational，没有 clock gating busy**
- **结果**: write 1 次后 busy 永久锁，bvalid 永远=0

### `axi_timer.h`
- `ch_reg<ch_bool> busy(ch_bool(false));` —— 缺 rst_n
- `busy->next` 用 `aw_handshake || ar_handshake` —— 但 **`io().awvalid && io().awready` (line 107, 122) 用 deprecated `operator&&(port, port)`** —— axi4/AGENTS.md ANTI-PATTERN 明确禁止
- `io().awvalid && io().awready` (line 77) 同样反模式 — line 77 / 79 / 94 / 122 共 4 处
- **结果**: busy 永久锁 + deprecated `&&` overload 警告

### `axi_gpio.h`
- `ch_reg<ch_bool> busy(ch_bool(false));` —— 缺 rst_n
- `busy->next` 与 uart 同样 `any_handshake=true→busy=true` 永久锁
- **结果**: write 1 次后 busy 永久锁

### `axi_dma.h`
- `ch_reg<ch_bool> data_vld(ch_bool(false)), err_flag(ch_bool(false)), irq_en(ch_bool(false));` —— 多 ch_reg<bool> 但**无 rst_n reset**
- 数据通路使用 `ch_reg<ch_uint<4>> beat_cnt(0_d);` 等 —— 也无 rst_n reset
- **结果**: ch_reg<bool> init value 在 VerilatorBackend 不应用（commit `7d69f56` 之前 bug，现已修）——但无 rst_n 用户无法 reset state

**真实代价**：

| 历史痛点 | 暴露点 |
|---|---|
| 4 个 peripheral 缺 rst_n → dual 行为 parity 测试不可行 | `dual-backend-axi4-peripherals` change scope 从 6 → 2 |
| axi_timer 4 处 IO port `&&` overload | `-Wdeprecated-declarations` 警告 (per `include/core/io_logical.h:73`) |
| axi_uart busy 永久锁 → 1 次 write 后 fixture 永久卡死 | riscv-mini SOC 集成 UART peripheral 路径延迟发现 bug |
| axi_dma 缺 rst_n → AXI burst 状态不清零 | riscv-mini SOC 集成 DMA peripheral 路径延迟发现 bug |

**预期收益**：

1. **4 peripheral dual 行为 parity 可测** — unblock `dual-backend-axi4-peripherals-extended-2026-10`（下一个 change）
2. **axi4 peripheral design 自洽** — 所有 AXI4-Lite peripheral 有 rst_n + busy self-clear 模式（与 `axi_spi` / `axi_pwm` / `axi_i2c` 一致）
3. **axi_timer 反模式修复** — 4 处 IO port `&&` → `select()`，消除 `-Wdeprecated-declarations`
4. **riscv-mini SOC integration 加速** — 4 peripheral 状态可重置，e2e test 可写多笔 transaction

**预期成本**：

- 修改 4 文件（4 peripheral headers）
- 改造工作量 ~2 人天（oracle 推荐 0.5 / peripheral × 4）
- ~1 个 atomic commit（per 修改建议）

**Out of Scope**（明确划线）：

- ❌ 改 4 个 peripheral 头文件 **之外的** AXI4 代码
- ❌ 改既有 `tests/test_axi_{uart,timer,gpio,dma}.cpp` 结构测（保留 interpreter-only baseline per `dual-backend-test-spec` R7）
- ❌ 改 `tests/test_axi_{pwm,i2c}_behavior.cpp`（已 archive dual-backend-axi4-peripherals）
- ❌ 改 `tests/test_verilator_complex_e2e.cpp`（macro reference impl）
- ❌ 新增 dual 行为 parity tests（本 change 只修 design；dual  follow-up 在 `dual-backend-axi4-peripherals-extended-2026-10`）

## What Changes

### 1. `include/axi4/peripherals/axi_uart.h` 加 rst_n

**改 1**: `__io` 加 `ch_in<ch_bool> rst_n;`

```cpp
__io(
    // ... existing ports ...
    
    // 异步复位 (低电平有效)
    ch_in<ch_bool> rst_n;
)
```

**改 2**: 所有 `ch_reg` 声明加 rst_n reset：

```cpp
// line 66: busy
ch_reg<ch_bool> busy(ch_bool(false));

// line 75: tx_empty  
ch_reg<ch_bool> tx_empty(ch_bool(true));

// line 76: rx_valid
ch_reg<ch_bool> rx_valid(ch_bool(false));

// line 140-150: 各状态机 registers 加 select(!rst_n, false, ...)
busy->next = select(!io().rst_n, ch_bool(false),
                    select(any_handshake, ch_bool(true),
                           select(any_ready, ch_bool(false), busy)));
// 类似 tx_empty / rx_valid / status_reg 等
```

### 2. `include/axi4/peripherals/axi_timer.h` 加 rst_n + 修 IO port `&&`

**改 1**: `__io` 加 `ch_in<ch_bool> rst_n;`

**改 2**: 修 4 处 IO port `&&` 反模式 → `select()`：

```cpp
// line 77: io().wready = io().awvalid && io().awready;
io().wready = select(io().awvalid, io().awready, ch_bool(false));

// line 79: auto w_handshake = io().wvalid && io().wready;
auto w_handshake = select(io().wvalid, io().wready, ch_bool(false));

// line 94: auto ar_handshake = io().arvalid && io().arready;
auto ar_handshake = select(io().arvalid, io().arready, ch_bool(false));

// line 122: busy->next select(io().bready || io().rready, ch_bool(false), busy)
//         改为:
select(io().bready, ch_bool(false), select(io().rready, ch_bool(false), busy))
```

**改 3**: `ch_reg<ch_bool> busy` + 状态 registers 加 `select(!rst_n, ...)` reset

### 3. `include/axi4/peripherals/axi_gpio.h` 加 rst_n

**改 1**: `__io` 加 `ch_in<ch_bool> rst_n;`

**改 2**: `busy` + 所有 ch_reg<bool> 加 `select(!rst_n, false, ...)` reset

### 4. `include/axi4/peripherals/axi_dma.h` 加 rst_n

**改 1**: `__io` 加 `ch_in<ch_bool> rst_n;`

**改 2**: 所有 ch_reg<bool> + ch_reg<uint> 加 `select(!rst_n, init_val, ...)` reset pattern

## Acceptance

#### Scenario: 4 个 peripheral 编译无错

- **WHEN** `cmake --build build -j$(nproc) 2>&1 | grep -E "error|Error"`
- **THEN** 0 行错误

#### Scenario: ctest 全量不退化

- **WHEN** `ctest -LE perf --output-on-failure`
- **THEN** 158/158 PASS（基线不变，**本 change 不新增 ctest target**）

- **WHEN** `ctest -L verilator_e2e -LE perf`
- **THEN** 23/23 PASS（同基线）

#### Scenario: axi_timer 无 deprecation warning

- **WHEN** `cmake --build build -j$(nproc) 2>&1 | grep -Wdeprecated-declarations | grep axi_timer`
- **THEN** 0 行（4 处 `&&`/`||` 已改写）

#### Scenario: 不破反约束

- **WHEN** reviewer 检查本 change 的 diff
- **THEN** MUST NOT 改 `include/axi4/peripherals/axi_{spi,pwm,i2c}.h`（已 design 自洽）
- **AND** MUST NOT 改 `tests/test_axi_{pwm,i2c}_behavior.cpp`（已 archive `dual-backend-axi4-peripherals`）
- **AND** MUST NOT 改既有 `tests/test_axi_{uart,timer,gpio,dma}.cpp` 结构测（spec R7 "原文件不动"）
- **AND** MUST NOT 改 `tests/test_verilator_complex_e2e.cpp`（macro reference impl）
- **AND** MUST NOT 改任何 spec 文件（no-spec-changes change）

#### Scenario: openspec validate 通过

- **WHEN** `openspec validate fix-axi4-peripheral-rst-n-port-2026-10 --strict`
- **THEN** valid

## Capabilities

### ADDED Requirements

#### Requirement: 4 个 AXI4 peripheral 加 rst_n + busy self-clear 修复 design 自洽

`include/axi4/peripherals/axi_{uart,timer,gpio,dma}.h` MUST 加 `ch_in<ch_bool> rst_n` port + 所有 `ch_reg` MUST 用 `select(!rst_n, init_val, ...)` reset pattern + `busy` MUST self-clear 在 `any_ready` 时。

#### Scenario: 4 个 peripheral 包含 rst_n port

- **WHEN** reviewer 阅读 `axi_uart.h` / `axi_timer.h` / `axi_gpio.h` / `axi_dma.h` `__io` section
- **THEN** MUST 含 `ch_in<ch_bool> rst_n;`

#### Scenario: 4 个 peripheral 状态 registers rst_n reset

- **WHEN** reviewer 阅读 `busy->next / aw_done->next / r_valid->next` 等 state register update lines
- **THEN** MUST 包含 `select(!io().rst_n, false/init_val, ...)` reset pattern

#### Scenario: axi_timer IO port 反模式修复

- **WHEN** reviewer 阅读 `axi_timer.h::describe()`
- **THEN** MUST NOT 含 `io().awvalid && io().awready` / `io().wvalid && io().wready` / `io().arvalid && io().arready` / `io().bready || io().rready` 等 IO port `&&`/`||` overload
- **AND** MUST 用 `select()` 替代

#### Scenario: dual backend parity unblock

- **WHEN** 本 change 落地后，`dual-backend-axi4-peripherals-extended-2026-10` change 可启动
- **THEN** 4 个 peripheral (`Uart` / `Timer` / `Gpio` / `Dma`) dual 行为 parity 可在 `DUAL_BACKEND_TEST_PAIR` macro 中跑过
- **AND** MUST NOT 出现 busy 永久锁导致 fixture 卡死