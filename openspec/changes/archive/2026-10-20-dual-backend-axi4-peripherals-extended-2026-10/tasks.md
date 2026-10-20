## Tasks

> 前置依赖：`fix-axi4-peripheral-rst-n-port-2026-10` 必须先 ship
> （本 change 假定 3 个 AXI4-Lite slave 已收敛到规范形态）。

- [x] ### Task 0 (前置): `fix-axi4-peripheral-rst-n-port-2026-10` ship
  - **action**: 等待该 change archive 完成
  - **note**: 本 change 的启动条件是 3 个 peripheral 满足
    `rst_n` + AW→W 相位分离 + 响应保持
  - **verify**: `ls openspec/changes/archive/ | grep fix-axi4-peripheral-rst-n-port` → 命中
  - **verify**: `grep -c "AW→W 相位分离" openspec/specs/dual-backend-test-coverage/spec.md` ≥ 1

- [x] ### Task 1: `AxiLiteTimer` behavior parity
  - **target**: `tests/test_axi_timer_behavior.cpp`（timer load/count/ctrl/status 寄存器）
  - **action**: 7 dual pairs
  - **verify**: `ctest -L verilator_e2e -R test_axi_timer_behavior --output-on-failure` → PASS

- [x] ### Task 2: `AxiLiteGpio` behavior parity
  - **target**: `tests/test_axi_gpio_behavior.cpp`（GPIO data / dir / interrupt 寄存器）
  - **action**: 7 dual pairs
  - **verify**: `ctest -L verilator_e2e -R test_axi_gpio_behavior --output-on-failure` → PASS

- [x] ### Task 3: `AxiLiteUart` behavior parity
  - **target**: `tests/test_axi_uart_behavior.cpp`（AXI-Lite UART tx/rx/status/ctrl 寄存器）
  - **action**: 7 dual pairs
  - **note**: 无 TX 移位 / RX 接收引擎（deferred D2）→ 只断言寄存器级语义
    （`tx_empty` / `status_reg` bit0），MUST NOT 断言 `uart_tx` 波形或 `rx_valid`
  - **verify**: `ctest -L verilator_e2e -R test_axi_uart_behavior --output-on-failure` → PASS

- [x] ### Task 4: CMakeLists.txt 注册 3 个 test_target
  - **action**: 3 个 `add_dual_backend_test`（`add_catch_test` 已注入 include 目录，
    无需额外 `target_include_directories`）
  - **verify**:
    - `ctest -N -L verilator_e2e -R "test_axi_(timer|gpio|uart)_behavior"` 输出 3 个
    - 每个 RESOURCE_LOCK "verilator_backend"，LABELS "base;verilator_e2e"，TIMEOUT 300
      （用 `ctest --show-only=json-v1` 查看属性；`-RE` 是排除正则，不能用）

- [x] ### Task 5: 全量回归
  - **verify**: `ctest -LE perf --output-on-failure` 仍全 PASS（+3 = 175 total）

- [x] ### Task 6: AGENTS.md + roadmap feature sync
  - **action**: CTEST_TOTAL 172→175 + root `tests/test_*.cpp` 计数 102→105
  - **action**: 同步 `.rddf/roadmap/features/feat-dual-backend-coverage.md`
    （Phase 3 子项表 + `ctest -L verilator_e2e` 实测数 23→26 + CTEST_TOTAL）
  - **verify**: `bash scripts/verify_test_counts.sh` exit 0

- [x] ### Task 7: archive 前置 gate
  - **verify**:
    - `openspec validate dual-backend-axi4-peripherals-extended-2026-10 --strict` → valid
    - `cmake --build build -j$(nproc) 2>&1 | grep -cE "error:|Error"` → 0
    - `bash scripts/verify_test_counts.sh` → exit 0

- [x] ### Task 8: archive（**禁用 `--skip-specs`**）
  - **action**: `openspec archive dual-backend-axi4-peripherals-extended-2026-10 --yes`
  - **verify**:
    - `ls openspec/changes/archive/ | grep peripherals-extended` → 命中
    - `grep -c "AxiLiteTimer" openspec/specs/dual-backend-test-coverage/spec.md` ≥ 1
      （证明 MODIFIED delta 已合并，未被 `--skip-specs` 静默丢弃）
    - `grep -c "AxiLiteDma" openspec/specs/dual-backend-test-coverage/spec.md` → 0
