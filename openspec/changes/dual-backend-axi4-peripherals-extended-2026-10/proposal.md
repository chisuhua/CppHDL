# Change: dual-backend-axi4-peripherals-extended-2026-10 — 3 个 AXI4-Lite slave dual 化 (Timer/Uart/Gpio)

## Why

**前置依赖**（必须先 ship）：

1. `fix-axi4-peripheral-rst-n-port-2026-10` — 3 个 AXI4-Lite slave 收敛到规范形态
   （`rst_n` + `busy`/`aw_done`/`b_valid`/`r_valid` + AW→W 相位分离 + 响应保持）
2. `dual-backend-axi4-peripherals` (archived `2026-10-20`) — 已 dual 化 2 peripheral (Pwm + I2c)

**现状**：

| Peripheral | dual 行为 parity | 阻塞原因 | 本 change 状态 |
|---|---|---|---|
| `AxiLiteSpi` | ✅ 已 ship (archive `2026-10-19-axi-spi-behavior-tests`) | 无 | ✅ Done |
| `AxiLitePwm` | ✅ 已 ship (archive `2026-10-20-dual-backend-axi4-peripherals`) | 无 | ✅ Done |
| `AxiLiteI2c` | ✅ 已 ship (archive `2026-10-20-dual-backend-axi4-peripherals`) | 无 | ✅ Done |
| `AxiLiteTimer` | ❌ 0 | 需 `fix-axi4-peripheral-rst-n-port` | ✅ 本次可做 |
| `AxiLiteGpio` | ❌ 0 | 需 `fix-axi4-peripheral-rst-n-port` | ✅ 本次可做 |
| `AxiLiteUart` | ❌ 0 | 需 `fix-axi4-peripheral-rst-n-port` | ✅ 本次可做 |
| `AxiDma` | ❌ 0 | 是 AXI4 **master**（非 Lite slave），复位契约不同 | ➡️ 移出本 change，见 `axi4-peripheral-followups-2026-10` |

**预期收益**：

1. **dual-backend-test-coverage Phase 3 完成度 6/6 (spi + pwm + i2c + timer + gpio + uart)** — AXI4-Lite slave 全覆盖
2. **axi4 peripheral dual parity runtime 覆盖 6 个** — PR feedback ≤45 min 内 catch codegen drift
3. 兑现原计划 6 peripheral 的 AXI4-Lite slave 子集

**预期成本**：

- 3 个新测试文件 + CMake + AGENTS.md
- 3 × ~7 dual pairs = ~21 test case halves 增量
- CTEST_TOTAL：`fix-axi4-peripheral-rst-n-port-2026-10` 落地后 172 → 本 change 后 **175**

**Out of Scope**（明确划线）：

- ❌ 改 3 个 peripheral 头文件本身（design 收敛已在 `fix-axi4-peripheral-rst-n-port` 完成）
- ❌ **`AxiDma` 的 dual 化** → 移入 `axi4-peripheral-followups-2026-10`
  （`AxiDma` 是 AXI4 master，`rst_n` 复位契约与 slave 不同，且 `AxiLiteDma` 类从不存在）
- ❌ 改既有 `tests/test_axi_{uart,timer,gpio}.cpp` 结构测
  （由 `fix-axi4-peripheral-rst-n-port` 新建；spec R"原 test_axi_*.cpp 不动"约束其内容）
- ❌ 改 `tests/test_axi_{pwm,i2c,spi}_behavior.cpp`（已 archive）
- ❌ 改既有 9 个 `tests/test_verilator_*.cpp`（spec 锁定 macro reference impl）
- ❌ 顶层 SOC / interconnect 嵌套 dual 化（超 macro R2/R3 scope）

## What Changes

### 1. 新建 3 个 `_behavior` 测试文件

每个 peripheral ~7 dual pairs，覆盖：
- AXI write BRESP=OKAY（handshake 时序 probe）
- AXI read rdata 反 DRDATA（读响应时序）
- 主控寄存器写后状态 chg（核对 peripheral-specific 行为）
- 主控寄存器读返回正确值
- rst_n=0 reset clears busy（peripheral-specific 状态清零）
- **AW→W 相位分离**（AW 与 W 分两周期，验证 W 仍被接受）
- **响应保持**（BVALID 在 BREADY 拉高前必须已被 master 观察到）

| 文件 | peripheral | dual pairs | TIMEOUT |
|------|-----------|------------|---------|
| `tests/test_axi_timer_behavior.cpp` | `AxiLiteTimer` | 7 | 300 |
| `tests/test_axi_gpio_behavior.cpp` | `AxiLiteGpio` | 7 | 300 |
| `tests/test_axi_uart_behavior.cpp` | `AxiLiteUart` | 7 | 300 |

### 2. CMakeLists.txt 注册 3 个 `add_dual_backend_test`

`tests/CMakeLists.txt` 追加：
```cmake
add_dual_backend_test(test_axi_<peripheral>_behavior
    SOURCES test_axi_<peripheral>_behavior.cpp
    TIMEOUT 300
)
target_include_directories(test_axi_<peripheral>_behavior PRIVATE ${PROJECT_SOURCE_DIR}/include)
```

### 3. spec 增量

`openspec/specs/dual-backend-test-coverage/spec.md` **MODIFIED** "AXI4 peripheral behavior test
SHALL 走 DUAL_BACKEND_TEST_PAIR"：把 peripheral 列表补全为**实际有 `_behavior` 测试**的集合，
并修正错误类名 `AxiLiteDma` → `AxiDma`（该类从不存在；`AxiDma` 尚无 `_behavior` 测试，
故不进本列表，其 dual 化归 `axi4-peripheral-followups-2026-10`）。

> 现状主 spec 的该 Requirement **没有** peripheral 列表 —— 上一个已归档 change
> `2026-10-20-dual-backend-axi4-peripherals` 用了 `--skip-specs`，其 delta 被静默丢弃。
> 本 change 显式用 MODIFIED 修正。

## Acceptance

#### Scenario: 3 个新 ctest target 全 PASS

- **WHEN** `ctest -L verilator_e2e -LE perf -R "test_axi_(timer|gpio|uart)_behavior"`
- **THEN** 3 个 ctest target 全 PASS（含 21 dual halves）

#### Scenario: 编译无错

- **WHEN** `cmake --build build -j$(nproc) 2>&1 | grep -E "error|Error"`
- **THEN** 0 行错误

#### Scenario: 资源锁注入

- **WHEN** `cd build && ctest -N -L verilator_e2e -RE "test_axi_(timer|gpio|uart)_behavior"`
- **THEN** 每个 RESOURCE_LOCK "verilator_backend"，LABELS "base;verilator_e2e"，TIMEOUT 300

#### Scenario: 不破反约束

- **WHEN** reviewer 检查本 change 的 diff
- **THEN** MUST NOT 改 `tests/test_axi_{uart,timer,gpio}.cpp`（结构测，interpreter-only baseline）
- **AND** MUST NOT 改 `include/axi4/peripherals/*.h`（design 收敛已在 `fix-axi4-peripheral-rst-n-port` 完成）
- **AND** MUST NOT 改 `tests/test_axi_{spi,pwm,i2c}_behavior.cpp`（已 archive）
- **AND** MUST NOT 改既有 9 个 `tests/test_verilator_*.cpp`（spec 锁定 macro reference impl）
- **AND** MUST NOT 改 `include/axi4/peripherals/axi_dma.h`

#### Scenario: CTEST_TOTAL +3 sync

- **WHEN** `cd build && ctest -N | tail -1`
- **THEN** CTEST_TOTAL 172 → 175（+3）
- **AND** root `tests/test_*.cpp` 计数同步 +3

#### Scenario: openspec archive 不丢 Requirement

- **WHEN** archive 本 change
- **THEN** **不得**使用 `--skip-specs`（本 change 有 spec delta）
- **AND** archive 后 `grep -c "AxiLiteTimer" openspec/specs/dual-backend-test-coverage/spec.md` ≥ 1

## Capabilities

### MODIFIED Requirements

见 `specs/dual-backend-test-coverage/spec.md`。
