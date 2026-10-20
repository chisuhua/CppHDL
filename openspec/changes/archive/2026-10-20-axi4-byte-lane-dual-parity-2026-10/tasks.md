## Tasks

- [x] ### Task 1: `tests/test_axi_byte_lane.cpp` 改为双后端
  - **action**:
    - 7 个 `TEST_CASE` 改为 `DUAL_BACKEND_TEST_PAIR`（共 14 个：7 interpreter + 7 verilator）
    - 用 typedef 规避宏参数的逗号切分（`Gpio8` / `Timer32` / `Uart16` / `Pwm16x4` /
      `I2c16` / `Spi8x16`）
    - `tests/CMakeLists.txt`：`add_catch_test` → `add_dual_backend_test`（TIMEOUT 300）
  - **verify**:
    - `./build/tests/test_axi_byte_lane --list-tests` → 7 个 `[verilator]` 用例
    - `ctest -R '^test_axi_byte_lane$'` → PASS（14 case / 152 断言）

- [x] ### Task 2: spec delta（双后端一致性 + 窄寄存器截断）
  - **action**:
    - `specs/dual-backend-test-coverage/spec.md` 的 MODIFIED Requirement 补 2 个 Scenario
  - **verify**:
    - `openspec validate axi4-byte-lane-dual-parity-2026-10 --strict` → valid
    - `openspec archive axi4-byte-lane-dual-parity-2026-10 --yes`（**禁用 `--skip-specs`**）
    - `openspec validate --specs --strict` → 全 PASS

- [x] ### Task 3: 全量回归 + 计数同步
  - **verify**:
    - `cd build && ctest -LE perf --output-on-failure` → 全 PASS
    - `bash scripts/verify_test_counts.sh` → exit 0
    - 计数**不变**（沿用 `test_axi_byte_lane` 这个已注册的名字，只换注册方式：
      `base` 标签仍在，`verilator_e2e` 标签新增）
