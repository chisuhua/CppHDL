# Change: dual-backend-axi4-peripherals-extended-2026-10 — 4 个 AXI4 peripheral dual 化 (Timer/Uart/Gpio/Dma)

## Why

**前置依赖**（必须先 ship）：

1. `fix-axi4-peripheral-rst-n-port-2026-10` (proposed) — 4 个 peripheral design 加 `rst_n` port + busy self-clear
2. `dual-backend-axi4-peripherals` (archived `2026-10-20`) — 已 dual 化 2 peripheral (Pwm + I2c)

**现状**：

| Peripheral | dual 行为 parity | 阻塞此 comment | 本 change 状态 |
|---|---|---|---|
| `AxiLiteSpi` | ✅ 已 ship (archive `2026-10-19-axi-spi-behavior-tests`) | 无 | ✅ Done |
| `AxiLitePwm` | ✅ 已 ship (archive `2026-10-20-dual-backend-axi4-peripherals`) | 无 | ✅ Done |
| `AxiLiteI2c` | ✅ 已 ship (archive `2026-10-20-dual-backend-axi4-peripherals`) | 无 | ✅ Done |
| `AxiLiteTimer` | ❌ 0 | 需 `fix-axi4-peripheral-rst-n-port` | ✅ 本次可做 |
| `AxiLiteGpio` | ❌ 0 | 需 `fix-axi4-peripheral-rst-n-port` | ✅ 本次可做 |
| `AxiLiteDma` | ❌ 0 | 需 `fix-axi4-peripheral-rst-n-port` | ✅ 本次可做 |
| `AxiLiteUart` | ❌ 0 | 需 `fix-axi4-peripheral-rst-n-port` | ✅ 本次可做 |

**预期收益**：

1. **dual-backend-test-coverage Phase 3 完成度 7/7 (spi + pwm + i2c + timer + gpio + dma + uart)** — Phase 3 全 coverage
2. **axi4 peripheral dual parity runtime 覆盖 7 个** — PR feedback ≤45 min 内 catch codegen drift
3. **兑现 oracle 优先级 B 完整交付**（原计划 6 peripheral，实际 7 因为也包含 spi）

**预期成本**：

- 4 个文件 + workspace + CMake + AGENTS.md
- 改造工作量 ~2 人天（oracle 推荐 0.5 / peripheral × 4）
- 4 个新 ctest target（CTEST_TOTAL 169 → 173）
- 4 × ~7 dual pairs = ~28 test case halves 增量

**Out of Scope**（明确划线）：

- ❌ 改 4 个 peripheral 头文件本身（保留 interpreter-only baseline，`fix-axi4-peripheral-rst-n-port` 唯一例外）
- ❌ 改既有 `tests/test_axi_{timer,gpio,dma,uart}.cpp` 等结构测（spec R7 "原文件不动"）
- ❌ 改 `tests/test_axi_{pwm,i2c,spi}_behavior.cpp`（已 archive）
- ❌ 改既有 9 个 `tests/test_verilator_*.cpp`（spec 锁定 macro reference impl）
- ❌ 顶层 SOC / interconnect 嵌套 dual 化（超 macro R2/R3 scope）

## What Changes

### 1. 新建 4 个 `_behavior` 测试文件

每个 peripheral ~7 dual pairs，覆盖：
- AXI write BRESP=OKAY（handshake 时序 probe）
- AXI read rdata 反 DRDATA（读响应时序）
- 主控寄存器写后状态 chg（核对 peripheral-specific 行为）
- 主控寄存器读返回正确值
- rst_n=0 reset clears busy（peripheral-specific 状态清零）
- 32-bit variant（如支持）

| 文件 | peripheral | dual pairs | TIMEOUT |
|------|-----------|------------|---------|
| `tests/test_axi_timer_behavior.cpp` | `AxiLiteTimer` | 7 | 300 |
| `tests/test_axi_gpio_behavior.cpp` | `AxiLiteGpio` | 7 | 300 |
| `tests/test_axi_dma_behavior.cpp` | `AxiLiteDma` | 7 | 300 |
| `tests/test_axi_uart_behavior.cpp` | `AxiLiteUart` | 7 | 300 |

### 2. CMakeLists.txt 注册 4 个 `add_dual_backend_test`

每个 `tests/CMakeLists.txt` 追加：
```cmake
add_dual_backend_test(test_axi_<peripheral>_behavior
    SOURCES test_axi_<peripheral>_behavior.cpp
    TIMEOUT 300
)
target_include_directories(test_axi_<peripheral>_behavior PRIVATE ${PROJECT_SOURCE_DIR}/include)
```

### 3. spec 增量

`openspec/specs/dual-backend-test-coverage/spec.md` +1 Requirement 锁定 4 peripheral 设计收敛性（依赖 `fix-axi4-peripheral-rst-n-port` + 本 change）：

```
### Requirement: AXI4 peripheral design MUST 自洽 for dual 化

任何 include/axi4/peripherals/*.h 的 AxiLite<Peripheral> MUST 含
ch_in<ch_bool> rst_n port + 所有 ch_reg 用 select(!rst_n, init_val, ...)
reset pattern + busy MUST self-clear 在 any_ready 时。详见
fix-axi4-peripheral-rst-n-port-2026-10 spec delta。
```

## Acceptance

#### Scenario: 4 个新 ctest target 全 PASS

- **WHEN** `ctest -L verilator_e2e -LE perf -R "test_axi_(timer|gpio|dma|uart)_behavior"`
- **THEN** 4 个 ctest target 全 PASS（含 28 dual halves）

#### Scenario: 编译无错

- **WHEN** `cmake --build build -j$(nproc) 2>&1 | grep -E "error|Error"`
- **THEN** 0 行错误

#### Scenario: 资源锁注入

- **WHEN** `cd build && ctest -N -L verilator_e2e -RE "test_axi_(timer|gpio|dma|uart)_behavior"`
- **THEN** 每个 RESOURCE_LOCK "verilator_backend"，LABELS "base;verilator_e2e"，TIMEOUT 300

#### Scenario: 不破反约束

- **WHEN** reviewer 检查本 change 的 diff
- **THEN** MUST NOT 改 `tests/test_axi_{timer,gpio,dma,uart}.cpp`（结构测，interpreter-only baseline）
- **AND** MUST NOT 改 `include/axi4/peripherals/*.h`（design 自洽已在 `fix-axi4-peripheral-rst-n-port` 完成）
- **AND` MUST NOT 改 `tests/test_axi_{spi,pwm,i2c}_behavior.cpp`（已 archive）
- **AND** MUST NOT 改既有 9 个 `tests/test_verilator_*.cpp`（spec 锁定 macro reference impl）

#### Scenario: CTEST_TOTAL +4 sync

- **WHEN** `cd build && ctest -N | tail -1`
- **THEN** CTEST_TOTAL 169 → 173（+4）
- **AND` root `tests/test_*.cpp` 99 → 103

## Capabilities

### ADDED Requirements

#### Requirement: AXI4 peripheral behavior test SHALL 走 DUAL_BACKEND_TEST_PAIR（4 peripheral 扩展）

`tests/test_axi_<peripheral>_behavior.cpp`（_behavior suffix 区别原 `test_axi_<peripheral>.cpp` 结构测） MUST 用 `DUAL_BACKEND_TEST_PAIR` macro 覆盖 peripherals: `AxiLiteTimer` / `AxiLiteGpio` / `AxiLiteDma` / `AxiLiteUart`（设计自洽的扩展集）。**原** `tests/test_axi_<peripheral>.cpp` **MUST NOT 修改**（保留 interpreter-only baseline per spec R7）。

#### Scenario: file 命名 + space 互斥

- **WHEN** 新增 AXI4 peripheral behavior runtime test
- **THEN** 文件名 MUST 用 `_behavior` suffix 区别原 `test_axi_<peripheral>.cpp`
- **AND` 原 `test_axi_<peripheral>.cpp` MUST NOT 修改

#### Scenario: macro 接口约束

- **WHEN** 测试访问 `ch_out<ch_uint<32>>` 等端口值
- **THEN` MUST 用 `sim.set_input_value(io().port, value)` + `sim.get_port_value(io().port)`
- **AND` MUST NOT 用 `port.impl()` 跳过 ch_out 接口（违反 macro R3）

#### Scenario: 不破既有 spec 锁

- **WHEN` reviewer 检查本 change 的 diff
- **THEN** MUST NOT 改 `tests/test_verilator_*.cpp` 测试体（spec 锁定 macro reference impl）
- **AND** MUST NOT 改 `tests/test_axi_lite_verilator_e2e.cpp`（spec 锁定 reference impl）
- **AND** MUST NOT 改 `tests/test_axi_{spi,pwm,i2c}_behavior.cpp`（已 archived）