# Change: dual-backend-axi4-peripherals — AXI4 peripheral behavior parity dual 化 (Phase 3 partial 续: Pwm + I2c)

## Why

**现状**：

`dual-backend-test-coverage` Phase 3 partial 已 ship（`openspec/changes/archive/2026-10-19-axi-spi-behavior-tests/`），覆盖 `AxiLiteSpi<8,16>` + `<32,16>` 共 7 dual pairs（`tests/test_axi_spi_behavior.cpp:282`）。剩余 6 个 AXI4-Lite peripheral 中 pilot 验证后发现 design 自洽性参差：

| Peripheral | rst_n port | busy self-clear | dual 行为 parity 现状 | 本 change 可做？ |
|---|---|---|---|---|
| `AxiLitePwm` | ✅ | ✅ (`select(!rst_n, false, ...)`) | ❌ 0 dual tests | ✅ YES |
| `AxiLiteI2c` | ✅ | ✅ (`select(aw_handshake, true, ...)` + rst_n) | ❌ 0 dual tests | ✅ YES |
| `AxiLiteTimer` | ❌ | ❌ (busy 永久锁) | ❌ 0 | ❌ 独立 change 修设计 |
| `AxiLiteGpio` | ❌ | ❌ (busy 永久锁) | ❌ 0 | ❌ 独立 change 修设计 |
| `AxiLiteUart` | ❌ | ❌ (busy 永久锁) | ❌ 0 | ❌ 独立 change 修设计 |
| `AxiLiteDma` | ❌ | ❌ (ch_reg<bool> init 不应用) | ❌ 0 | ❌ 独立 change 修设计 |

**Scope 收缩理由**（pilot 2026-10-20 验证）：

- 仅有 rst_n + busy self-clear 的 peripheral design 才能支持 dual 行为 parity 测试（双 half 都要 reset + busy clear 才能跑第二笔 transaction）
- 4 个 peripheral（Timer/Uart/Gpio/Dma）无 rst_n port → busy 永久锁死或 CH_reg 初值不应用 → **dual 行为 parity 测试不可能**（除非修 design 加 rst_n）
- **本 change 收缩到 2 个 peripheral**（Pwm + I2c，design 自洽）；**4 个 peripheral 留独立 change 修 design**

**前置阻塞已解除**（commit `7d69f56` 2026-10-20）：
- `src/codegen_verilog.cpp::print_reg` else 分支修复：ch_reg<T>(init) 现在正确 emit hold-state always_ff
- axi_uart.h:75 ch_reg<ch_bool> tx_empty(ch_bool(true)) 等在 VerilatorBackend 应用 init value 正确
- 但 axi_uart 的 busy self-clear 仍需 rst_n 端口——属独立 change scope

**预期收益**（基于收窄 scope）：

1. **2 peripheral dual parity runtime 覆盖** — Pwm + I2c design 自洽，codegen drift 可即时暴露
2. **延续 `axi-spi-behavior-tests` `_behavior` suffix pattern** — spec 固化语义
3. **兑现 oracle 优先级 B 部分交付**（Pwm + I2c），其余 4 peripheral 独立 change 修 design
4. **axi4 peripherals 的 design 自洽性 audit 落地**（通过 dual test 路径暴露）

**预期成本**：

- 一次性 CI 墙钟增量 ~2-5 sec/PR（warm cache）
- 改造工作量 ~1 人天（oracle 推荐 0.5 / peripheral × 2）
- 2 个新 ctest target（CTEST_TOTAL 167 → 169）
- 2 × ~7 dual pairs = ~14 test case halves 增量

**Out of Scope**（明确划线）：

- ❌ 改 2 个 peripheral 头文件本身（保留 interpreter-only baseline）
- ❌ 改既有 `tests/test_axi_{i2c,pwm}.cpp` 等结构测（spec R7 "原文件不动"）
- ❌ `AxiLiteSpi`（已 phase-3 partial ship）
- ❌ **4 个 rst_n-missing peripheral (Timer/Uart/Gpio/Dma) dual 化** —— 需独立 change 先修 design 加 rst_n port + busy self-clear
- ❌ 顶层 SOC / interconnect 嵌套 dual 化（超 macro R2/R3 scope）
- ❌ 包络 AxiLiteSlave 模板（独立 fixture，已被 `test_axi_lite_verilator_e2e.cpp` 覆盖）

## What Changes

### 1. 新建 2 个 `_behavior` 测试文件

每个 peripheral ~7 dual pairs，覆盖：
- AXI write BRESP=OKAY（handshake 时序 probe）
- AXI read rdata 反 DRDATA（读响应时序）
- 主控寄存器写后状态 chg（核对 peripheral-specific 行为）
- 主控寄存器读返回正确值
- reset clears busy（rst_n=0）
- 32-bit variant（如支持）

| 文件 | peripheral | dual pairs | TIMEOUT |
|------|-----------|------------|---------|
| `tests/test_axi_pwm_behavior.cpp` | `AxiLitePwm` | 7 | 300 |
| `tests/test_axi_i2c_behavior.cpp` | `AxiLiteI2c` | 7 | 300 |

### 2. CMakeLists.txt 注册 2 个 `add_dual_backend_test`

```cmake
add_dual_backend_test(test_axi_pwm_behavior
    SOURCES test_axi_pwm_behavior.cpp
    TIMEOUT 300
)
target_include_directories(test_axi_pwm_behavior PRIVATE ${PROJECT_SOURCE_DIR}/include)

add_dual_backend_test(test_axi_i2c_behavior
    SOURCES test_axi_i2c_behavior.cpp
    TIMEOUT 300
)
target_include_directories(test_axi_i2c_behavior PRIVATE ${PROJECT_SOURCE_DIR}/include)
```

### 3. spec 增量

`openspec/specs/dual-backend-test-coverage/spec.md` +1 Requirement（仿照 `axi-spi-behavior-tests` archive spec delta）：

```
### Requirement: AXI4 peripheral behavior test SHALL 走 DUAL_BACKEND_TEST_PAIR

`tests/test_axi_<peripheral>_behavior.cpp`（_behavior suffix 区别原
`test_axi_<peripheral>.cpp` 结构测） MUST 用 DUAL_BACKEND_TEST_PAIR macro。
覆盖 peripherals: AxiLitePwm / AxiLiteI2c（design 自洽的 subset）。
原 test_axi_<peripheral>.cpp MUST NOT 修改。
```

## Acceptance

#### Scenario: 2 个新 ctest target 全 PASS

- **WHEN** `ctest -L verilator_e2e -LE perf -R "test_axi_(pwm|i2c)_behavior"`
- **THEN** 2 个 ctest target 全 PASS（含 14 dual halves）

#### Scenario: 编译无错

- **WHEN** `cmake --build build -j$(nproc) 2>&1 | grep -E "error|Error"`
- **THEN** 0 行错误

#### Scenario: 资源锁注入

- **WHEN** `cd build && ctest -N -L verilator_e2e -RE "test_axi_(pwm|i2c)_behavior"`
- **THEN** 每个 RESOURCE_LOCK "verilator_backend"，LABELS "base;verilator_e2e"，TIMEOUT 300

#### Scenario: 不破反约束

- **WHEN** reviewer 检查本 change 的 diff
- **THEN** MUST NOT 改 `tests/test_axi_{i2c,pwm}.cpp`（结构测，interpreter-only baseline）
- **AND** MUST NOT 改 `include/axi4/peripherals/*.h`（fixture source）
- **AND** MUST NOT 改 `tests/test_axi_spi_behavior.cpp`（已 archived）
- **AND** MUST NOT 改既有 9 个 `tests/test_verilator_*.cpp`（spec 锁定 macro reference impl）

#### Scenario: CTEST_TOTAL +2 sync

- **WHEN** `cd build && ctest -N | tail -1`
- **THEN** CTEST_TOTAL 167 → 169（+2）
- **AND** root `tests/test_*.cpp` 97 → 99

## Capabilities

### ADDED Requirements

#### Requirement: AXI4 peripheral behavior test SHALL 走 DUAL_BACKEND_TEST_PAIR

`tests/test_axi_<peripheral>_behavior.cpp`（_behavior suffix 区别原 `test_axi_<peripheral>.cpp` 结构测） MUST 用 `DUAL_BACKEND_TEST_PAIR` macro 覆盖 peripherals: `AxiLitePwm` / `AxiLiteI2c`（design 自洽的 subset）。**原** `tests/test_axi_<peripheral>.cpp` **MUST NOT 修改**（保留 interpreter-only baseline per spec R7）。

#### Scenario: file 命名 + space 互斥

- **WHEN** 新增 AXI4 peripheral behavior runtime test
- **THEN** 文件名 MUST 用 `_behavior` suffix 区别原 `test_axi_<peripheral>.cpp`
- **AND** 原 `test_axi_<peripheral>.cpp` MUST NOT 修改

#### Scenario: macro 接口约束

- **WHEN** 测试访问 `ch_out<ch_uint<32>>` 等端口值
- **THEN** MUST 用 `sim.set_input_value(io().port, value)` + `sim.get_port_value(io().port)`
- **AND** MUST NOT 用 `port.impl()` 跳过 ch_out 接口（违反 macro R3）

#### Scenario: 不破既有 spec 锁

- **WHEN** reviewer 检查本 change 的 diff
- **THEN** MUST NOT 改 `tests/test_verilator_*.cpp` 测试体（spec 锁定 macro reference impl）
- **AND** MUST NOT 改 `tests/test_axi_lite_verilator_e2e.cpp`（spec 锁定 reference impl）
- **AND** MUST NOT 改 `tests/test_axi_spi_behavior.cpp`（已 archived 2026-10-19）