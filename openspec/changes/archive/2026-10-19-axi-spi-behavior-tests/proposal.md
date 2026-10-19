# Change: axi-spi-behavior-tests — AxiLiteSpi behavior parity runtime test (interpreter + verilator)

## Why

**现状**：
- `tests/test_axi_spi.cpp` 7 个 TEST_CASE **全是**接口结构检查（`io().port.impl() != nullptr`、`io().sport.length > 0`），不是行为验证
- 真正"AXI4-Lite handshake 完成 → SPI transfer 触发 → TX/RX 数据正确"的行为测试 = **0 个**
- 即有 `tests/test_axi_lite_verilator_e2e.cpp`（`verilator-e2e-coverage` R 锁定为 macro reference impl）只覆盖 1 个 AxiLiteSlave fixture，无法 catch `AxiLiteSpi<8,16>` / `<32,16>` variant 的 codegen drift
- dual-backend-test-coverage Phase 3（bundle/axi4 peripherals ~50 个 test）阻塞因现有 test_axi_* 是 anti-pattern `impl()` access，不符合 macro R3 contract

**真实代价**：

| 历史痛点 | 暴露点 |
|---|---|
| AxiLiteSpi state machine bug 延迟到 nightly | 现有 test_axi_spi 只 verify port 存在，不 verify SPI 行为正确 |
| TX_DATA write → SCLK toggles → MOSI shifted out 协议未 e2e 验证 | riscv-mini SOC 集成时若 SPI protocol drift，全 SoC 仿真才暴露 |
| AxiLiteSpi<32, 16> variant codegen path | 32-bit 转移指令未在 dual backend 跑过 |

**预期收益**：

1. **AxiLiteSpi behavior parity**：interpreter 与 verilator 双后端同时验证 AXI4-Lite 写入 TX_DATA → SCLK toggle → SPI transfer 完成 → STATUS bit 置位
2. **dual-backend 价值真实化**：之前"结构检查"测的 dual 化无意义；本 change 让 dual 化有实际 codegen parity 收效
3. **为未来 AxiLiteI2c / AxiLitePwm / AxiLiteUart / AxiLiteTimer / AxiLiteGpio 提供 behavior parity pattern**（7 peripherals 中先动 SPI = 最复杂的时序行为）

**预期成本**：

- 一次性 CI 墙钟增量 ~30-90 sec/PR（warm cache < 1 sec / dual-test，cold 30+ sec）
- 改造工作量 ~1-2 人天
- 7 peripheral × behavior parity test pattern = 后续 6 个 change 按本 pattern 复制

**Out of Scope**：
- ❌ `tests/test_axi_spi.cpp` 不动（保留原接口结构测试 + reference pattern）
- ❌ 其他 peripheral（I2c / Pwm / Uart / Timer / Gpio / Dma）的 behavior test —— 后续 sub-change
- ❌ `tests/test_axi_lite_verilator_e2e.cpp` —— spec R 锁定为 macro reference impl

## What Changes

### 1. 新建 `tests/test_axi_spi_behavior.cpp`

**7 个新 TEST_CASE**，全部用 `DUAL_BACKEND_TEST_PAIR` macro：

| TEST_CASE | 验证 |
|---|---|
| `AxiLiteSpi - AXI write TX_DATA latches value` | cycle 0: write addr=0x00, wdata=0xAB → 触发 tx_data_reg latch；STATUS.busy = 1 (per spec §6.2) |
| `AxiLiteSpi - AXI write CTRL triggers transfer` | 写 CTRL register → 触发 transfer_active（per state machine） |
| `AxiLiteSpi - Transfer inactive keeps MOSI low` | idle state: spi_mosi=0, spi_cs=1 (active low) |
| `AxiLiteSpi - Read STATUS returns busy + rx_valid` | 读 STATUS register（不消耗 transfer_state）→ 验证 busy / rx_valid bits |
| `AxiLiteSpi - AXI write captures BRESP=OKAY` | 写完成后 bvalid=1, bresp=0 (OKAY) |
| `AxiLiteSpi - 32-bit variant transfer path` | AxiLiteSpi<32, 16> 的 32-bit transfer （transfer cycle 数翻倍） |
| `AxiLiteSpi - Reset clears busy + SPI idle` | rst_n=0 → busy=0, transfer_active=0, spi_cs=1 |

### 2. CMakeLists.txt 注册

```cmake
add_dual_backend_test(test_axi_spi_behavior
    SOURCES test_axi_spi_behavior.cpp
    TIMEOUT 300  # 32-bit transfer 可能跨多 cycle
)
```

放到 tests/CMakeLists.txt 中现有 `test_axi_*` 注册区段（与 test_axi_spi 并列）。

### 3. spec `dual-backend-test-coverage` 增量

新增 `## ADDED Requirements`：
```
### Requirement: AXI4 peripheral behavior test SHALL 走 DUAL_BACKEND_TEST_PAIR

tests/test_axi_*_behavior.cpp（MUST 用 _behavior suffix 区别原 test_axi_*）
新文件 runtime test MUST 走 DUAL_BACKEND_TEST_PAIR macro。
原 test_axi_*.cpp（接口结构测试）保持 interpreter-only，不强制 dual 化。
```

## Acceptance

#### Scenario: 7 个 TEST_CASE_PAIR 全 PASS

- **WHEN** `cd build && cmake --build . --target test_axi_spi_behavior && ctest -L verilator_e2e -R test_axi_spi_behavior`
- **THEN** test_axi_spi_behavior 通过；14 个 test cases（7 dual pairs）全 PASS

#### Scenario: 端口值 parity

- **WHEN** interpreter half 与 verilator half 都跑同一份 lambda body
- **THEN** 对 TX_DATA、STATUS、CTRL、spi_mosi 等端口的逐周期 REQUIRE 断言两端都成立（per macro R3 契约）

#### Scenario: 编译无错

- **WHEN** `cmake --build build -j$(nproc) 2>&1 | grep -E "error|Error"`
- **THEN** 0 行错误

#### Scenario: 资源锁注入

- **WHEN** `cd build && ctest -N -L verilator_e2e -R test_axi_spi_behavior`
- **THEN** RESOURCE_LOCK "verilator_backend"，LABELS "base;verilator_e2e"

## Capabilities

### ADDED Requirements

#### Requirement: AXI4 peripheral behavior test SHALL 走 DUAL_BACKEND_TEST_PAIR

`tests/test_axi_*_behavior.cpp`（_behavior suffix 区别） MUST 用 `DUAL_BACKEND_TEST_PAIR` macro。**不允许**用裸 `TEST_CASE` 单独跑 interpreter；**不允许**直接访问 `port.impl()`（违反 macro R3 契约）。

#### Scenario: file 命名约束

- **WHEN** 新增 AXI4 peripheral behavior runtime test
- **THEN** 文件名 MUST 用 `_behavior` suffix（如 `test_axi_spi_behavior.cpp`）区别原 `test_axi_*.cpp` 接口结构测试

#### Scenario: macro 接口约束

- **WHEN** 测试访问 `ch_out<ch_uint<32>>` 等端口值
- **THEN** MUST 用 `sim.set_input_value(io().port, value)` + `sim.get_port_value(io().port)`；MUST NOT 用 `port.impl()` 跳过 ch_out 接口